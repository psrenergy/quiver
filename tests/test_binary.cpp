#include <chrono>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/binary/binary.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/element.h>

using namespace quiver;
namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BinaryTempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_binary_test").string(); }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full))
                fs::remove(full);
        }
    }

    std::string path;

    static BinaryMetadata make_simple_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row", "col"})
                                                .set("dimension_sizes", {3, 2})
                                                .set("labels", {"val1", "val2"}));
    }

    static BinaryMetadata make_time_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"stage", "block"})
                                                .set("dimension_sizes", {4, 31})
                                                .set("time_dimensions", {"stage", "block"})
                                                .set("frequencies", {"monthly", "daily"})
                                                .set("labels", {"plant_1", "plant_2"}));
    }
};

// ============================================================================
// BinaryOpenFile
// ============================================================================

TEST_F(BinaryTempFileFixture, WriteModeCreatesFiles) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_TRUE(fs::exists(path + ".qvr"));
    EXPECT_TRUE(fs::exists(path + ".toml"));
}

TEST_F(BinaryTempFileFixture, ReadModeAfterWrite) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
    }
    EXPECT_NO_THROW(Binary::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, ReadModeOnMissingFile) {
    EXPECT_THROW(Binary::open_file(path, 'r'), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, WriteModeWithoutMetadata) {
    EXPECT_THROW(Binary::open_file(path, 'w'), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, InvalidMode) {
    auto md = make_simple_metadata();
    EXPECT_THROW(Binary::open_file(path, 'x', md), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ReadModeReturnsCorrectMetadata) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
    }
    auto reader = Binary::open_file(path, 'r');
    EXPECT_EQ(reader.get_metadata().dimensions.size(), 2u);
    EXPECT_EQ(reader.get_metadata().labels.size(), 2u);
    EXPECT_EQ(reader.get_file_path(), path);
}

TEST_F(BinaryTempFileFixture, ReadModeReturnsCorrectFilePath) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
    }
    auto reader = Binary::open_file(path, 'r');
    EXPECT_EQ(reader.get_file_path(), path);
}

TEST_F(BinaryTempFileFixture, WriteModeInitializesWithNaN) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
    }

    auto reader = Binary::open_file(path, 'r');
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
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    ASSERT_EQ(values.size(), 2u);
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);
}

TEST_F(BinaryTempFileFixture, WriteReadMultiplePositions) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        binary.write({3.0, 4.0}, {{"row", 2}, {"col", 2}});
        binary.write({5.0, 6.0}, {{"row", 3}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"row", 1}, {"col", 1}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"row", 2}, {"col", 2}})[0], 3.0);
    EXPECT_DOUBLE_EQ(reader.read({{"row", 3}, {"col", 1}})[0], 5.0);
}

TEST_F(BinaryTempFileFixture, WriteReadAllPositions) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        int counter = 0;
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                binary.write({static_cast<double>(counter), static_cast<double>(counter + 1)},
                             {{"row", r}, {"col", c}});
                counter += 2;
            }
        }
    }
    auto reader = Binary::open_file(path, 'r');
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
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        binary.write({99.0, 100.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 99.0);
    EXPECT_DOUBLE_EQ(values[1], 100.0);
}

TEST_F(BinaryTempFileFixture, DataPersistsAfterReopen) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        binary.write({3.0, 4.0}, {{"row", 2}, {"col", 2}});
    }
    auto reader = Binary::open_file(path, 'r');
    auto v1 = reader.read({{"row", 1}, {"col", 1}});
    auto v2 = reader.read({{"row", 2}, {"col", 2}});
    EXPECT_DOUBLE_EQ(v1[0], 1.0);
    EXPECT_DOUBLE_EQ(v2[0], 3.0);
}

TEST_F(BinaryTempFileFixture, NegativeValues) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({-1.5, -999.99}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], -1.5);
    EXPECT_DOUBLE_EQ(values[1], -999.99);
}

TEST_F(BinaryTempFileFixture, ZeroValues) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({0.0, 0.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 0.0);
    EXPECT_DOUBLE_EQ(values[1], 0.0);
}

TEST_F(BinaryTempFileFixture, LargeDoubleValues) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        double big = 1e300;
        binary.write({big, -big}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
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
        auto binary = Binary::open_file(path, 'w', md);
    }
    auto reader = Binary::open_file(path, 'r');
    EXPECT_THROW(reader.read({{"row", 1}, {"col", 1}}, false), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, ReadUnwrittenPositionReturnsNaNWhenNullsAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
    }
    auto reader = Binary::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

TEST_F(BinaryTempFileFixture, ReadWrittenPositionSucceedsWithNullsNotAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    EXPECT_NO_THROW(reader.read({{"row", 1}, {"col", 1}}, false));
}

TEST_F(BinaryTempFileFixture, ExplicitNaNWrite) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        double nan = std::numeric_limits<double>::quiet_NaN();
        binary.write({nan, nan}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Binary::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

// ============================================================================
// BinaryDimensionValidation
// ============================================================================

TEST_F(BinaryTempFileFixture, WrongDimensionCountTooFew) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.read({{"row", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, WrongDimensionCountTooMany) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.read({{"row", 1}, {"col", 1}, {"z", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, MissingDimensionName) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.read({{"row", 1}, {"bad", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ValueBelowOne) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.read({{"row", 0}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ValueAboveMax) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.read({{"row", 4}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, BoundaryMinValid) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary.read({{"row", 1}, {"col", 1}}, true));
}

TEST_F(BinaryTempFileFixture, BoundaryMaxValid) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary.read({{"row", 3}, {"col", 2}}, true));
}

// ============================================================================
// BinaryDataLengthValidation
// ============================================================================

TEST_F(BinaryTempFileFixture, DataTooShort) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.write({1.0}, {{"row", 1}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, DataTooLong) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_THROW(binary.write({1.0, 2.0, 3.0}, {{"row", 1}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, DataExactMatch) {
    auto md = make_simple_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}}));
}

// ============================================================================
// BinaryMoveSemantics
// ============================================================================

TEST_F(BinaryTempFileFixture, MoveConstruct) {
    auto md = make_simple_metadata();
    {
        auto binary1 = Binary::open_file(path, 'w', md);
        binary1.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader1 = Binary::open_file(path, 'r');
    Binary reader2 = std::move(reader1);
    auto values = reader2.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);
}

TEST_F(BinaryTempFileFixture, MoveAssign) {
    auto md = make_simple_metadata();
    {
        auto binary = Binary::open_file(path, 'w', md);
        binary.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto path2 = path + "_2";
    {
        auto binary2 = Binary::open_file(path2, 'w', md);
        binary2.write({5.0, 6.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader1 = Binary::open_file(path, 'r');
    auto reader2 = Binary::open_file(path2, 'r');
    reader2 = std::move(reader1);

    auto values = reader2.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);

    // Clean up path2 files
    for (auto ext : {".qvr", ".toml"}) {
        auto full = path2 + ext;
        if (fs::exists(full))
            fs::remove(full);
    }
}

TEST_F(BinaryTempFileFixture, MoveAssignWriterUnregistersOldPath) {
    auto md = make_simple_metadata();
    auto path2 = path + "_2";
    {
        auto writer1 = Binary::open_file(path, 'w', md);
        auto writer2 = Binary::open_file(path2, 'w', md);
        writer2 = std::move(writer1);
        // writer2's old path (path2) should be unregistered
        EXPECT_NO_THROW(Binary::open_file(path2, 'r'));
        // writer2 now owns path, still registered
        EXPECT_THROW(Binary::open_file(path, 'r'), std::runtime_error);
    }
    // After destruction, path is also unregistered
    EXPECT_NO_THROW(Binary::open_file(path, 'r'));

    for (auto ext : {".qvr", ".toml"}) {
        auto full = path2 + ext;
        if (fs::exists(full))
            fs::remove(full);
    }
}

// ============================================================================
// BinaryTimeDimensionValidation
// ============================================================================

TEST_F(BinaryTempFileFixture, ValidTimeDimensionCoordinates) {
    auto md = make_time_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    // stage=1 (Jan), block=1 → valid
    EXPECT_NO_THROW(binary.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}}));
}

TEST_F(BinaryTempFileFixture, InvalidTimeDimensionCoordinates) {
    auto md = make_time_metadata();
    auto binary = Binary::open_file(path, 'w', md);
    // stage=2 (Feb), block=30 → Feb doesn't have 30 days: datetime implies day 30 but month says day 2
    // The datetime accumulation would compute Feb + 29 days offset = March 2nd, datetime_to_int(March) != 2
    EXPECT_THROW(binary.write({1.0, 2.0}, {{"stage", 2}, {"block", 30}}), std::invalid_argument);
}

// ============================================================================
// WriteRegistry
// ============================================================================

TEST_F(BinaryTempFileFixture, WriterBlocksReader) {
    auto md = make_simple_metadata();
    auto writer = Binary::open_file(path, 'w', md);
    EXPECT_THROW(Binary::open_file(path, 'r'), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, WriterBlocksSecondWriter) {
    auto md = make_simple_metadata();
    auto writer = Binary::open_file(path, 'w', md);
    EXPECT_THROW(Binary::open_file(path, 'w', md), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, DestroyedWriterAllowsReader) {
    auto md = make_simple_metadata();
    {
        auto writer = Binary::open_file(path, 'w', md);
    }
    EXPECT_NO_THROW(Binary::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, MovedWriterClearsRegistryOnDestruction) {
    auto md = make_simple_metadata();
    {
        auto writer1 = Binary::open_file(path, 'w', md);
        auto writer2 = std::move(writer1);
    }
    EXPECT_NO_THROW(Binary::open_file(path, 'r'));
}

TEST_F(BinaryTempFileFixture, SingleTimeDimensionSkipsConsistencyCheck) {
    // With only one time dimension, there's no inner time dim to validate
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month", "scenario"})
                                               .set("dimension_sizes", {12, 3})
                                               .set("time_dimensions", {"month"})
                                               .set("frequencies", {"monthly"})
                                               .set("labels", {"val"}));
    auto binary = Binary::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary.write({1.0}, {{"month", 12}, {"scenario", 3}}));
}
