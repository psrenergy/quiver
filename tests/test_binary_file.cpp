#include <gtest/gtest.h>
#include <quiver/binary/binary_file.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/binary/iteration.h>
#include <quiver/element.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace quiver;
namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BinaryTempFileFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "quiver_binary_test").string();
    }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full)) {
                fs::remove(full);
            }
        }
    }

    std::string path;

    static BinaryMetadata make_simple_metadata() {
        return BinaryMetadata::from_element(
            Element()
                .set("version", "1")
                .set("initial_datetime", "2025-01-01T00:00:00")
                .set("unit", "MW")
                .set("dimensions", {"row", "col"})
                .set("dimension_sizes", {3, 2})
                .set("labels", {"val1", "val2"})
        );
    }

    static BinaryMetadata make_time_metadata() {
        return BinaryMetadata::from_element(
            Element()
                .set("version", "1")
                .set("initial_datetime", "2025-01-01T00:00:00")
                .set("unit", "MW")
                .set("dimensions", {"stage", "block"})
                .set("dimension_sizes", {4, 31})
                .set("time_dimensions", {"stage", "block"})
                .set("frequencies", {"monthly", "daily"})
                .set("labels", {"plant_1", "plant_2"})
        );
    }

    // One-label metadata whose dimensions are all time dimensions, each named after its frequency.
    static BinaryMetadata make_time_layout(
        const std::vector<std::string>& frequencies,
        const std::vector<int64_t>& sizes,
        const std::string& initial_datetime
    ) {
        return BinaryMetadata::from_element(
            Element()
                .set("version", "1")
                .set("initial_datetime", initial_datetime)
                .set("unit", "MW")
                .set("dimensions", frequencies)
                .set("dimension_sizes", sizes)
                .set("time_dimensions", frequencies)
                .set("frequencies", frequencies)
                .set("labels", {"val"})
        );
    }

    // Writes a distinct value to every cell first_dimensions/next_dimensions visits, then reopens the file and
    // reads each one back. Returns the number of cells.
    size_t write_and_read_every_cell(const BinaryMetadata& md) {
        std::vector<std::vector<int64_t>> cells;
        for (std::optional<std::vector<int64_t>> cell = first_dimensions(md); cell; cell = next_dimensions(md, *cell)) {
            cells.push_back(*cell);
        }
        auto dims_of = [&md](const std::vector<int64_t>& cell) {
            std::unordered_map<std::string, int64_t> dims;
            for (size_t i = 0; i < cell.size(); ++i) {
                dims[md.dimensions[i].name] = cell[i];
            }
            return dims;
        };
        {
            auto writer = BinaryFile::open_file(path, 'w', md);
            for (size_t n = 0; n < cells.size(); ++n) {
                writer.write({static_cast<double>(n)}, dims_of(cells[n]));
            }
        }
        auto reader = BinaryFile::open_file(path, 'r');
        size_t mismatches = 0;
        for (size_t n = 0; n < cells.size(); ++n) {
            if (reader.read(dims_of(cells[n]))[0] != static_cast<double>(n)) {
                ++mismatches;
            }
        }
        EXPECT_EQ(mismatches, 0u);
        return cells.size();
    }
};

// ============================================================================
// BinaryOpenFile
// ============================================================================

TEST_F(BinaryTempFileFixture, WriteModeCreatesFiles) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_TRUE(fs::exists(path + ".qvr"));
    EXPECT_TRUE(fs::exists(path + ".toml"));
}

TEST_F(BinaryTempFileFixture, ReadModeAfterWrite) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, ReadModeOnMissingFile) {
    EXPECT_THROW(BinaryFile::open_file(path, 'r'), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, WriteModeWithoutMetadata) {
    EXPECT_THROW(BinaryFile::open_file(path, 'w'), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, TwoStepWriteWithOpenMember) {
    auto md = make_simple_metadata();
    BinaryFile binary_file(path);
    binary_file.open('w', md);
    EXPECT_TRUE(fs::exists(path + ".qvr"));
    EXPECT_TRUE(fs::exists(path + ".toml"));
}

TEST_F(BinaryTempFileFixture, TwoStepReadWithOpenMember) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
    }
    BinaryFile binary_file(path);
    EXPECT_NO_THROW(binary_file.open('r'));
}

TEST_F(BinaryTempFileFixture, InvalidMode) {
    auto md = make_simple_metadata();
    EXPECT_THROW(BinaryFile::open_file(path, 'x', md), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ReadModeReturnsCorrectMetadata) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_EQ(reader.get_metadata().dimensions.size(), 2u);
    EXPECT_EQ(reader.get_metadata().labels.size(), 2u);
    EXPECT_EQ(reader.get_file_path(), path);
}

TEST_F(BinaryTempFileFixture, ReadModeReturnsCorrectFilePath) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_EQ(reader.get_file_path(), path);
}

TEST_F(BinaryTempFileFixture, WriteModeInitializesWithNaN) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }

    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

// ============================================================================
// BinaryWriteRead
// ============================================================================

TEST_F(BinaryTempFileFixture, WriteReadSinglePosition) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    ASSERT_EQ(values.size(), 2u);
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);
}

TEST_F(BinaryTempFileFixture, WriteReadMultiplePositions) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        binary_file.write({3.0, 4.0}, {{"row", 2}, {"col", 2}});
        binary_file.write({5.0, 6.0}, {{"row", 3}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"row", 1}, {"col", 1}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"row", 2}, {"col", 2}})[0], 3.0);
    EXPECT_DOUBLE_EQ(reader.read({{"row", 3}, {"col", 1}})[0], 5.0);
}

TEST_F(BinaryTempFileFixture, WriteReadAllPositions) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        int counter = 0;
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                binary_file.write(
                    {static_cast<double>(counter), static_cast<double>(counter + 1)},
                    {{"row", r}, {"col", c}}
                );
                counter += 2;
            }
        }
    }
    auto reader = BinaryFile::open_file(path, 'r');
    int counter = 0;
    for (int64_t r = 1; r <= 3; ++r) {
        for (int64_t c = 1; c <= 2; ++c) {
            auto values = reader.read({{"row", r}, {"col", c}});
            EXPECT_DOUBLE_EQ(values[0], static_cast<double>(counter));
            EXPECT_DOUBLE_EQ(values[1], static_cast<double>(counter + 1));
            counter += 2;
        }
    }
}

TEST_F(BinaryTempFileFixture, OverwriteExistingData) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        binary_file.write({99.0, 100.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 99.0);
    EXPECT_DOUBLE_EQ(values[1], 100.0);
}

TEST_F(BinaryTempFileFixture, DataPersistsAfterReopen) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        binary_file.write({3.0, 4.0}, {{"row", 2}, {"col", 2}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto v1 = reader.read({{"row", 1}, {"col", 1}});
    auto v2 = reader.read({{"row", 2}, {"col", 2}});
    EXPECT_DOUBLE_EQ(v1[0], 1.0);
    EXPECT_DOUBLE_EQ(v2[0], 3.0);
}

TEST_F(BinaryTempFileFixture, NegativeValues) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({-1.5, -999.99}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], -1.5);
    EXPECT_DOUBLE_EQ(values[1], -999.99);
}

TEST_F(BinaryTempFileFixture, ZeroValues) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({0.0, 0.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 0.0);
    EXPECT_DOUBLE_EQ(values[1], 0.0);
}

TEST_F(BinaryTempFileFixture, LargeDoubleValues) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        double big = 1e300;
        binary_file.write({big, -big}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    double big = 1e300;
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], big);
    EXPECT_DOUBLE_EQ(values[1], -big);
}

// ============================================================================
// BinaryNullHandling
// ============================================================================

TEST_F(BinaryTempFileFixture, ReadUnwrittenPositionThrowsWhenNullsNotAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_THROW(reader.read({{"row", 1}, {"col", 1}}, false), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, ReadUnwrittenPositionReturnsNaNWhenNullsAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

TEST_F(BinaryTempFileFixture, ReadWrittenPositionSucceedsWithNullsNotAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_NO_THROW(reader.read({{"row", 1}, {"col", 1}}, false));
}

TEST_F(BinaryTempFileFixture, ExplicitNaNWrite) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        double nan = std::numeric_limits<double>::quiet_NaN();
        binary_file.write({nan, nan}, {{"row", 1}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

// ============================================================================
// BinaryDimensionValidation
// ============================================================================

TEST_F(BinaryTempFileFixture, WrongDimensionCountTooFew) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.read({{"row", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, WrongDimensionCountTooMany) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.read({{"row", 1}, {"col", 1}, {"z", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, MissingDimensionName) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.read({{"row", 1}, {"bad", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ValueBelowOne) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.read({{"row", 0}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ValueAboveMax) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.read({{"row", 4}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, BoundaryMinValid) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary_file.read({{"row", 1}, {"col", 1}}, true));
}

TEST_F(BinaryTempFileFixture, BoundaryMaxValid) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary_file.read({{"row", 3}, {"col", 2}}, true));
}

// ============================================================================
// BinaryDataLengthValidation
// ============================================================================

TEST_F(BinaryTempFileFixture, DataTooShort) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.write({1.0}, {{"row", 1}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, DataTooLong) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(binary_file.write({1.0, 2.0, 3.0}, {{"row", 1}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, DataExactMatch) {
    auto md = make_simple_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}}));
}

// ============================================================================
// BinaryMoveSemantics
// ============================================================================

TEST_F(BinaryTempFileFixture, MoveConstruct) {
    auto md = make_simple_metadata();
    {
        auto binary1 = BinaryFile::open_file(path, 'w', md);
        binary1.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader1 = BinaryFile::open_file(path, 'r');
    BinaryFile reader2 = std::move(reader1);
    auto values = reader2.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);
}

TEST_F(BinaryTempFileFixture, MoveAssign) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto path2 = path + "_2";
    {
        auto binary2 = BinaryFile::open_file(path2, 'w', md);
        binary2.write({5.0, 6.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader1 = BinaryFile::open_file(path, 'r');
    auto reader2 = BinaryFile::open_file(path2, 'r');
    reader2 = std::move(reader1);

    auto values = reader2.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);

    // Clean up path2 files
    for (auto ext : {".qvr", ".toml"}) {
        auto full = path2 + ext;
        if (fs::exists(full)) {
            fs::remove(full);
        }
    }
}

TEST_F(BinaryTempFileFixture, MoveAssignWriterUnregistersOldPath) {
    auto md = make_simple_metadata();
    auto path2 = path + "_2";
    {
        auto writer1 = BinaryFile::open_file(path, 'w', md);
        auto writer2 = BinaryFile::open_file(path2, 'w', md);
        writer2 = std::move(writer1);
        // writer2's old path (path2) should be unregistered
        EXPECT_NO_THROW(BinaryFile::open_file(path2, 'r'));
        // writer2 now owns path, still registered
        EXPECT_THROW(BinaryFile::open_file(path, 'r'), std::runtime_error);
    }
    // After destruction, path is also unregistered
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));

    for (auto ext : {".qvr", ".toml"}) {
        auto full = path2 + ext;
        if (fs::exists(full)) {
            fs::remove(full);
        }
    }
}

// ============================================================================
// BinaryTimeDimensionValidation
// ============================================================================

TEST_F(BinaryTempFileFixture, ValidTimeDimensionCoordinates) {
    auto md = make_time_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    // stage=1 (Jan), block=1 → valid
    EXPECT_NO_THROW(binary_file.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}}));
}

TEST_F(BinaryTempFileFixture, InvalidTimeDimensionCoordinates) {
    auto md = make_time_metadata();
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    // stage=2 (Feb), block=30: February 1 + 29 days is March 2, whose day of month (2) is not the 30 given
    EXPECT_THROW(binary_file.write({1.0, 2.0}, {{"stage", 2}, {"block", 30}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, InitialDatetimeYear1960) {
    // initial_datetime in 1960 is before the Unix epoch. The metadata's time-dimension
    // computation and the write/read round-trip must handle pre-1970 dates, asserting the exact
    // value survives serialization to the .toml sidecar and back.
    auto md = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "1960-01-01T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"stage", "block"})
            .set("dimension_sizes", {4, 31})
            .set("time_dimensions", {"stage", "block"})
            .set("frequencies", {"monthly", "daily"})
            .set("labels", {"plant_1", "plant_2"})
    );

    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        // stage=1 (Jan), block=1 and block=31 are valid (1960 is a leap year, Jan has 31 days).
        binary_file.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}});
        binary_file.write({3.0, 4.0}, {{"stage", 1}, {"block", 31}});
    }

    auto reader = BinaryFile::open_file(path, 'r');
    // The exact 1960 initial_datetime must round-trip — this is the assertion that catches the
    // pre-epoch corruption (the bug produced 1969-12-31T23:59:59 instead of 1960-01-01).
    auto expected = std::chrono::system_clock::time_point{
        std::chrono::sys_days{std::chrono::year{1960} / std::chrono::January / 1}
    };
    EXPECT_EQ(reader.get_metadata().initial_datetime, expected);
    // ...and the serialized sidecar must read back the same string via to_toml().
    EXPECT_NE(reader.get_metadata().to_toml().find("1960-01-01T00:00:00"), std::string::npos);

    auto v1 = reader.read({{"stage", 1}, {"block", 1}});
    EXPECT_DOUBLE_EQ(v1[0], 1.0);
    EXPECT_DOUBLE_EQ(v1[1], 2.0);

    auto v2 = reader.read({{"stage", 1}, {"block", 31}});
    EXPECT_DOUBLE_EQ(v2[0], 3.0);
    EXPECT_DOUBLE_EQ(v2[1], 4.0);
}

// ============================================================================
// BinaryTimeLayouts -- every cell of each of the 8 parent/child pairs validate() accepts, from Saturday
// 2025-03-15T06:00:00: not midnight, not the start of any day, week, month or year.
// ============================================================================

TEST_F(BinaryTempFileFixture, EveryCellMonthlyUnderYearly) {
    auto md = make_time_layout({"yearly", "monthly"}, {2, 12}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 3}));
    EXPECT_EQ(write_and_read_every_cell(md), 22u);  // March-December 2025, then all of 2026
}

TEST_F(BinaryTempFileFixture, EveryCellDailyUnderYearly) {
    auto md = make_time_layout({"yearly", "daily"}, {2, 366}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 74}));
    EXPECT_EQ(write_and_read_every_cell(md), 657u);  // 292 days left in 2025 + 365
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"yearly", 1}, {"daily", 366}}), std::invalid_argument);  // 2025 has 365
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderYearly) {
    auto md = make_time_layout({"yearly", "hourly"}, {2, 8784}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 1759}));  // 73 * 24 + 6 + 1
    EXPECT_EQ(write_and_read_every_cell(md), 15762u);                  // 7002 hours left in 2025 + 8760
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"yearly", 1}, {"hourly", 8761}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, EveryCellDailyUnderMonthly) {
    auto md = make_time_layout({"monthly", "daily"}, {12, 31}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 15}));
    EXPECT_EQ(write_and_read_every_cell(md), 351u);  // 2025-03-15 to 2026-02-28
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"monthly", 2}, {"daily", 31}}), std::invalid_argument);  // April 31
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderMonthly) {
    auto md = make_time_layout({"monthly", "hourly"}, {12, 744}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 343}));  // 14 * 24 + 6 + 1
    EXPECT_EQ(write_and_read_every_cell(md), 8418u);                  // 402 hours of March + 334 days * 24
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write({1.0}, {{"monthly", 2}, {"hourly", 721}}), std::invalid_argument);  // April: 720
}

TEST_F(BinaryTempFileFixture, EveryCellDailyUnderWeeklyAcrossYearEnd) {
    // A week is seven days from the day of initial_datetime, so week 42 day 6 is 2026-01-01 and the grid does
    // not restart on January 1
    auto md = make_time_layout({"weekly", "daily"}, {60, 7}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 1}));
    EXPECT_EQ(write_and_read_every_cell(md), 420u);
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderWeeklyAcrossYearEnd) {
    auto md = make_time_layout({"weekly", "hourly"}, {60, 168}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 7}));
    EXPECT_EQ(write_and_read_every_cell(md), 10074u);  // 162 hours of week 1 + 59 * 168
}

TEST_F(BinaryTempFileFixture, EveryCellHourlyUnderDaily) {
    auto md = make_time_layout({"daily", "hourly"}, {3, 24}, "2025-03-15T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 7}));
    EXPECT_EQ(write_and_read_every_cell(md), 66u);  // 18 + 24 + 24
}

TEST_F(BinaryTempFileFixture, MonthlyUnderYearlyFromTheThirtyFirst) {
    // January 31 + one month used to be March 3, so month 2 was rejected
    auto md = make_time_layout({"yearly", "monthly"}, {1, 12}, "2025-01-31T00:00:00");
    EXPECT_EQ(write_and_read_every_cell(md), 12u);
}

TEST_F(BinaryTempFileFixture, LeapDayStartUnderYearlyMonthlyDaily) {
    // From 2024-02-29, (2, 3, 1) is 2025-03-01: the missing 2025-02-29 must not shift it
    auto md = make_time_layout({"yearly", "monthly", "daily"}, {2, 12, 31}, "2024-02-29T00:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 2, 29}));
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({1.0}, {{"yearly", 1}, {"monthly", 2}, {"daily", 29}});
        writer.write({2.0}, {{"yearly", 2}, {"monthly", 2}, {"daily", 28}});
        writer.write({3.0}, {{"yearly", 2}, {"monthly", 3}, {"daily", 1}});
        EXPECT_THROW(writer.write({4.0}, {{"yearly", 2}, {"monthly", 2}, {"daily", 29}}), std::invalid_argument);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"yearly", 1}, {"monthly", 2}, {"daily", 29}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"yearly", 2}, {"monthly", 2}, {"daily", 28}})[0], 2.0);
    EXPECT_DOUBLE_EQ(reader.read({{"yearly", 2}, {"monthly", 3}, {"daily", 1}})[0], 3.0);
}

TEST_F(BinaryTempFileFixture, NonMidnightStartUnderMonthlyDailyHourly) {
    // The first cell is 06:00 on January 1; the monthly step used to drop the time of day and reject it
    auto md = make_time_layout({"monthly", "daily", "hourly"}, {12, 31, 24}, "2025-01-01T06:00:00");
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 1, 7}));
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({1.0}, {{"monthly", 1}, {"daily", 1}, {"hourly", 7}});
        writer.write({2.0}, {{"monthly", 2}, {"daily", 28}, {"hourly", 24}});
        EXPECT_THROW(writer.write({3.0}, {{"monthly", 2}, {"daily", 29}, {"hourly", 1}}), std::invalid_argument);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"monthly", 1}, {"daily", 1}, {"hourly", 7}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"monthly", 2}, {"daily", 28}, {"hourly", 24}})[0], 2.0);
}

// ============================================================================
// WriteRegistry
// ============================================================================

TEST_F(BinaryTempFileFixture, WriterBlocksReader) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(BinaryFile::open_file(path, 'r'), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, WriterBlocksSecondWriter) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(BinaryFile::open_file(path, 'w', md), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, DestroyedWriterAllowsReader) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
    }
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, MovedWriterClearsRegistryOnDestruction) {
    auto md = make_simple_metadata();
    {
        auto writer1 = BinaryFile::open_file(path, 'w', md);
        auto writer2 = std::move(writer1);
    }
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, OpenFileWriteFailureDoesNotLeakRegistry) {
    // Force to_toml() -> validate() to throw by setting an invalid version.
    // validate() at src/binary/binary_metadata.cpp rejects version != "1".
    auto bad_md = make_simple_metadata();
    bad_md.version = "999";

    // First call must throw because to_toml() rejects the bad version. The fix-under-test
    // is whether the registry is left clean after this throw.
    EXPECT_THROW(BinaryFile::open_file(path, 'w', bad_md), std::runtime_error);

    // Second call on the SAME canonical path with VALID metadata must succeed
    // -- only possible if the registry was correctly cleaned up by the late-insert fix.
    auto good_md = make_simple_metadata();
    EXPECT_NO_THROW({
        auto writer = BinaryFile::open_file(path, 'w', good_md);
        writer.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    });

    // Third call: reopen for read confirms the writer was real.
    EXPECT_NO_THROW(BinaryFile::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, SingleTimeDimensionSkipsConsistencyCheck) {
    // With only one time dimension, there's no inner time dim to validate
    auto md = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2025-01-01T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"month", "scenario"})
            .set("dimension_sizes", {12, 3})
            .set("time_dimensions", {"month"})
            .set("frequencies", {"monthly"})
            .set("labels", {"val"})
    );
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary_file.write({1.0}, {{"month", 12}, {"scenario", 3}}));
}
