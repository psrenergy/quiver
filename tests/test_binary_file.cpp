#include <chrono>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/binary/binary_file.h>
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
                binary_file.write({static_cast<double>(counter), static_cast<double>(counter + 1)},
                                  {{"row", r}, {"col", c}});
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
        if (fs::exists(full))
            fs::remove(full);
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
        if (fs::exists(full))
            fs::remove(full);
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
    // stage=2 (Feb), block=30 → Feb doesn't have 30 days: datetime implies day 30 but month says day 2
    // The datetime accumulation would compute Feb + 29 days offset = March 2nd, datetime_to_int(March) != 2
    EXPECT_THROW(binary_file.write({1.0, 2.0}, {{"stage", 2}, {"block", 30}}), std::invalid_argument);
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
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month", "scenario"})
                                               .set("dimension_sizes", {12, 3})
                                               .set("time_dimensions", {"month"})
                                               .set("frequencies", {"monthly"})
                                               .set("labels", {"val"}));
    auto binary_file = BinaryFile::open_file(path, 'w', md);
    EXPECT_NO_THROW(binary_file.write({1.0}, {{"month", 12}, {"scenario", 3}}));
}

// ============================================================================
// Fast Overloads (D-13/D-14): vector<int64_t> dims, read_into trusted-caller path
// ============================================================================

TEST_F(BinaryTempFileFixture, FastReadOverloadRoundTrip) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        // Write all 6 cells of a 3x2 grid using the fast vector-keyed overload.
        writer.write({1.0, 2.0}, std::vector<int64_t>{1, 1});
        writer.write({3.0, 4.0}, std::vector<int64_t>{1, 2});
        writer.write({5.0, 6.0}, std::vector<int64_t>{2, 1});
        writer.write({7.0, 8.0}, std::vector<int64_t>{2, 2});
        writer.write({9.0, 10.0}, std::vector<int64_t>{3, 1});
        writer.write({11.0, 12.0}, std::vector<int64_t>{3, 2});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto v11 = reader.read(std::vector<int64_t>{1, 1}, false);
    auto v22 = reader.read(std::vector<int64_t>{2, 2}, false);
    auto v32 = reader.read(std::vector<int64_t>{3, 2}, false);
    ASSERT_EQ(v11.size(), 2u);
    EXPECT_DOUBLE_EQ(v11[0], 1.0);
    EXPECT_DOUBLE_EQ(v11[1], 2.0);
    EXPECT_DOUBLE_EQ(v22[0], 7.0);
    EXPECT_DOUBLE_EQ(v22[1], 8.0);
    EXPECT_DOUBLE_EQ(v32[0], 11.0);
    EXPECT_DOUBLE_EQ(v32[1], 12.0);
}

TEST_F(BinaryTempFileFixture, FastReadOverloadMatchesUnorderedMap) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({42.0, 99.0}, {{"row", 2}, {"col", 1}});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto slow = reader.read({{"row", 2}, {"col", 1}}, false);
    auto fast = reader.read(std::vector<int64_t>{2, 1}, false);
    ASSERT_EQ(slow.size(), fast.size());
    for (size_t i = 0; i < slow.size(); ++i) {
        EXPECT_DOUBLE_EQ(slow[i], fast[i]);
    }
}

TEST_F(BinaryTempFileFixture, FastReadOverloadValidatesBounds) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_THROW(reader.read(std::vector<int64_t>{0, 1}, true), std::invalid_argument);
    EXPECT_THROW(reader.read(std::vector<int64_t>{4, 1}, true), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, FastReadOverloadValidatesCount) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_THROW(reader.read(std::vector<int64_t>{1}, true), std::invalid_argument);
    EXPECT_THROW(reader.read(std::vector<int64_t>{1, 1, 1}, true), std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, ReadIntoSkipsValidation) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({100.0, 200.0}, std::vector<int64_t>{1, 1});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    std::vector<double> buf;
    reader.read_into(std::vector<int64_t>{1, 1}, buf, false);
    ASSERT_EQ(buf.size(), 2u);
    EXPECT_DOUBLE_EQ(buf[0], 100.0);
    EXPECT_DOUBLE_EQ(buf[1], 200.0);
}

TEST_F(BinaryTempFileFixture, ReadIntoReusesBuffer) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        writer.write({1.5, 2.5}, std::vector<int64_t>{1, 1});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    std::vector<double> buf(2, -1.0);
    auto data_ptr_before = buf.data();
    auto cap_before = buf.capacity();
    reader.read_into(std::vector<int64_t>{1, 1}, buf, false);
    EXPECT_EQ(buf.size(), 2u);
    EXPECT_DOUBLE_EQ(buf[0], 1.5);
    EXPECT_DOUBLE_EQ(buf[1], 2.5);
    // No reallocation expected when size already matches label_count.
    EXPECT_EQ(buf.capacity(), cap_before);
    EXPECT_EQ(buf.data(), data_ptr_before);
}

TEST_F(BinaryTempFileFixture, FastWriteOverloadValidatesDataLength) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write(std::vector<double>{1.0, 2.0, 3.0}, std::vector<int64_t>{1, 1}),
                 std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, FastWriteOverloadValidatesBounds) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path, 'w', md);
    EXPECT_THROW(writer.write(std::vector<double>{1.0, 2.0}, std::vector<int64_t>{0, 1}),
                 std::invalid_argument);
    EXPECT_THROW(writer.write(std::vector<double>{1.0, 2.0}, std::vector<int64_t>{4, 1}),
                 std::invalid_argument);
}

TEST_F(BinaryTempFileFixture, FastWriteOverloadAcceptsTimeMetadata) {
    auto md = make_time_metadata();
    {
        auto writer = BinaryFile::open_file(path, 'w', md);
        // Stage 2 = February 2025; block 5 is a valid day in February.
        writer.write({0.5, 1.5}, std::vector<int64_t>{2, 5});
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read(std::vector<int64_t>{2, 5}, false);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_DOUBLE_EQ(values[0], 0.5);
    EXPECT_DOUBLE_EQ(values[1], 1.5);
}
