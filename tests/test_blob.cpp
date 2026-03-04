#include <chrono>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/blob/blob.h>
#include <quiver/blob/blob_metadata.h>
#include <quiver/element.h>

using namespace quiver;
namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BlobTempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_blob_test").string(); }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full))
                fs::remove(full);
        }
    }

    std::string path;

    static BlobMetadata make_simple_metadata() {
        return BlobMetadata::from_element(Element()
                                              .set("version", "1")
                                              .set("initial_datetime", "2025-01-01T00:00:00")
                                              .set("unit", "MW")
                                              .set("dimensions", {"row", "col"})
                                              .set("dimension_sizes", {3, 2})
                                              .set("labels", {"val1", "val2"}));
    }

    static BlobMetadata make_time_metadata() {
        return BlobMetadata::from_element(Element()
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
// BlobOpenFile
// ============================================================================

TEST_F(BlobTempFileFixture, WriteModeCreatesFiles) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_TRUE(fs::exists(path + ".qvr"));
    EXPECT_TRUE(fs::exists(path + ".toml"));
}

TEST_F(BlobTempFileFixture, ReadModeAfterWrite) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
    }
    EXPECT_NO_THROW(Blob::open_file(path, 'r'));
}

TEST_F(BlobTempFileFixture, ReadModeOnMissingFile) {
    EXPECT_THROW(Blob::open_file(path, 'r'), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, WriteModeWithoutMetadata) {
    EXPECT_THROW(Blob::open_file(path, 'w'), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, InvalidMode) {
    auto md = make_simple_metadata();
    EXPECT_THROW(Blob::open_file(path, 'x', md), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, ReadModeReturnsCorrectMetadata) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
    }
    auto reader = Blob::open_file(path, 'r');
    EXPECT_EQ(reader.get_metadata().dimensions.size(), 2u);
    EXPECT_EQ(reader.get_metadata().labels.size(), 2u);
    EXPECT_EQ(reader.get_file_path(), path);
}

TEST_F(BlobTempFileFixture, ReadModeReturnsCorrectFilePath) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
    }
    auto reader = Blob::open_file(path, 'r');
    EXPECT_EQ(reader.get_file_path(), path);
}

TEST_F(BlobTempFileFixture, WriteModeInitializesWithNaN) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
    }

    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

// ============================================================================
// BlobWriteRead
// ============================================================================

TEST_F(BlobTempFileFixture, WriteReadSinglePosition) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    ASSERT_EQ(values.size(), 2u);
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);
}

TEST_F(BlobTempFileFixture, WriteReadMultiplePositions) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        blob.write({3.0, 4.0}, {{"row", 2}, {"col", 2}});
        blob.write({5.0, 6.0}, {{"row", 3}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"row", 1}, {"col", 1}})[0], 1.0);
    EXPECT_DOUBLE_EQ(reader.read({{"row", 2}, {"col", 2}})[0], 3.0);
    EXPECT_DOUBLE_EQ(reader.read({{"row", 3}, {"col", 1}})[0], 5.0);
}

TEST_F(BlobTempFileFixture, WriteReadAllPositions) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        int counter = 0;
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                blob.write({static_cast<double>(counter), static_cast<double>(counter + 1)}, {{"row", r}, {"col", c}});
                counter += 2;
            }
        }
    }
    auto reader = Blob::open_file(path, 'r');
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

TEST_F(BlobTempFileFixture, OverwriteExistingData) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        blob.write({99.0, 100.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 99.0);
    EXPECT_DOUBLE_EQ(values[1], 100.0);
}

TEST_F(BlobTempFileFixture, DataPersistsAfterReopen) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
        blob.write({3.0, 4.0}, {{"row", 2}, {"col", 2}});
    }
    auto reader = Blob::open_file(path, 'r');
    auto v1 = reader.read({{"row", 1}, {"col", 1}});
    auto v2 = reader.read({{"row", 2}, {"col", 2}});
    EXPECT_DOUBLE_EQ(v1[0], 1.0);
    EXPECT_DOUBLE_EQ(v2[0], 3.0);
}

TEST_F(BlobTempFileFixture, NegativeValues) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({-1.5, -999.99}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], -1.5);
    EXPECT_DOUBLE_EQ(values[1], -999.99);
}

TEST_F(BlobTempFileFixture, ZeroValues) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({0.0, 0.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 0.0);
    EXPECT_DOUBLE_EQ(values[1], 0.0);
}

TEST_F(BlobTempFileFixture, LargeDoubleValues) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        double big = 1e300;
        blob.write({big, -big}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    double big = 1e300;
    auto values = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], big);
    EXPECT_DOUBLE_EQ(values[1], -big);
}

// ============================================================================
// BlobNullHandling
// ============================================================================

TEST_F(BlobTempFileFixture, ReadUnwrittenPositionThrowsWhenNullsNotAllowed) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
    }
    auto reader = Blob::open_file(path, 'r');
    EXPECT_THROW(reader.read({{"row", 1}, {"col", 1}}, false), std::runtime_error);
}

TEST_F(BlobTempFileFixture, ReadUnwrittenPositionReturnsNaNWhenNullsAllowed) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
    }
    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

TEST_F(BlobTempFileFixture, ReadWrittenPositionSucceedsWithNullsNotAllowed) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    EXPECT_NO_THROW(reader.read({{"row", 1}, {"col", 1}}, false));
}

TEST_F(BlobTempFileFixture, ExplicitNaNWrite) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        double nan = std::numeric_limits<double>::quiet_NaN();
        blob.write({nan, nan}, {{"row", 1}, {"col", 1}});
    }
    auto reader = Blob::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}

// ============================================================================
// BlobDimensionValidation
// ============================================================================

TEST_F(BlobTempFileFixture, WrongDimensionCountTooFew) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.read({{"row", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, WrongDimensionCountTooMany) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.read({{"row", 1}, {"col", 1}, {"z", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, MissingDimensionName) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.read({{"row", 1}, {"bad", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, ValueBelowOne) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.read({{"row", 0}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, ValueAboveMax) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.read({{"row", 4}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, BoundaryMinValid) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_NO_THROW(blob.read({{"row", 1}, {"col", 1}}, true));
}

TEST_F(BlobTempFileFixture, BoundaryMaxValid) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_NO_THROW(blob.read({{"row", 3}, {"col", 2}}, true));
}

// ============================================================================
// BlobDataLengthValidation
// ============================================================================

TEST_F(BlobTempFileFixture, DataTooShort) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.write({1.0}, {{"row", 1}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, DataTooLong) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_THROW(blob.write({1.0, 2.0, 3.0}, {{"row", 1}, {"col", 1}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, DataExactMatch) {
    auto md = make_simple_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_NO_THROW(blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}}));
}

// ============================================================================
// BlobMoveSemantics
// ============================================================================

TEST_F(BlobTempFileFixture, MoveConstruct) {
    auto md = make_simple_metadata();
    {
        auto blob1 = Blob::open_file(path, 'w', md);
        blob1.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader1 = Blob::open_file(path, 'r');
    Blob reader2 = std::move(reader1);
    auto values = reader2.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[1], 2.0);
}

TEST_F(BlobTempFileFixture, MoveAssign) {
    auto md = make_simple_metadata();
    {
        auto blob = Blob::open_file(path, 'w', md);
        blob.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    auto path2 = path + "_2";
    {
        auto blob2 = Blob::open_file(path2, 'w', md);
        blob2.write({5.0, 6.0}, {{"row", 1}, {"col", 1}});
    }
    auto reader1 = Blob::open_file(path, 'r');
    auto reader2 = Blob::open_file(path2, 'r');
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

// ============================================================================
// BlobTimeDimensionValidation
// ============================================================================

TEST_F(BlobTempFileFixture, ValidTimeDimensionCoordinates) {
    auto md = make_time_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    // stage=1 (Jan), block=1 → valid
    EXPECT_NO_THROW(blob.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}}));
}

TEST_F(BlobTempFileFixture, InvalidTimeDimensionCoordinates) {
    auto md = make_time_metadata();
    auto blob = Blob::open_file(path, 'w', md);
    // stage=2 (Feb), block=30 → Feb doesn't have 30 days: datetime implies day 30 but month says day 2
    // The datetime accumulation would compute Feb + 29 days offset = March 2nd, datetime_to_int(March) != 2
    EXPECT_THROW(blob.write({1.0, 2.0}, {{"stage", 2}, {"block", 30}}), std::invalid_argument);
}

TEST_F(BlobTempFileFixture, SingleTimeDimensionSkipsConsistencyCheck) {
    // With only one time dimension, there's no inner time dim to validate
    auto md = BlobMetadata::from_element(Element()
                                             .set("version", "1")
                                             .set("initial_datetime", "2025-01-01T00:00:00")
                                             .set("unit", "MW")
                                             .set("dimensions", {"month", "scenario"})
                                             .set("dimension_sizes", {12, 3})
                                             .set("time_dimensions", {"month"})
                                             .set("frequencies", {"monthly"})
                                             .set("labels", {"val"}));
    auto blob = Blob::open_file(path, 'w', md);
    EXPECT_NO_THROW(blob.write({1.0}, {{"month", 12}, {"scenario", 3}}));
}
