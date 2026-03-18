#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/binary/binary.h>
#include <quiver/binary/binary_comparator.h>
#include <quiver/binary/binary_metadata.h>

using namespace quiver;
namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BinaryCompareFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path1 = (fs::temp_directory_path() / "quiver_compare_test1").string();
        path2 = (fs::temp_directory_path() / "quiver_compare_test2").string();
    }

    void TearDown() override {
        for (const auto& p : {path1, path2}) {
            for (auto ext : {".qvr", ".toml"}) {
                auto full = p + ext;
                if (fs::exists(full))
                    fs::remove(full);
            }
        }
    }

    std::string path1;
    std::string path2;

    static BinaryMetadata make_metadata() {
        BinaryMetadata md;
        md.version = "1";
        md.add_dimension("scenario", 2);
        md.add_dimension("block", 3);
        md.unit = "MW";
        md.labels = {"plant_1", "plant_2"};
        return md;
    }
};

// ============================================================================
// CompareFiles — Status
// ============================================================================

TEST_F(BinaryCompareFixture, IdenticalFilesReturnFileMatch) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
        bin.write({30.0, 40.0}, {{"scenario", 1}, {"block", 2}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
        bin.write({30.0, 40.0}, {{"scenario", 1}, {"block", 2}});
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::FileMatch);
}

TEST_F(BinaryCompareFixture, BothFilesUnwrittenReturnFileMatch) {
    auto md = make_metadata();
    {
        Binary::open_file(path1, 'w', md);
    }
    {
        Binary::open_file(path2, 'w', md);
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::FileMatch);
}

TEST_F(BinaryCompareFixture, DifferentDataReturnsDataMismatch) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({99.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
}

TEST_F(BinaryCompareFixture, WrittenVsUnwrittenReturnsDataMismatch) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        Binary::open_file(path2, 'w', md);
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
}

TEST_F(BinaryCompareFixture, DifferentMetadataReturnsMetadataMismatch) {
    auto md1 = make_metadata();
    auto md2 = make_metadata();
    md2.unit = "GWh";
    {
        auto bin = Binary::open_file(path1, 'w', md1);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md2);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::MetadataMismatch);
}

TEST_F(BinaryCompareFixture, DifferentDimensionSizesReturnsMetadataMismatch) {
    auto md1 = make_metadata();
    BinaryMetadata md2;
    md2.version = "1";
    md2.add_dimension("scenario", 4);
    md2.add_dimension("block", 3);
    md2.unit = "MW";
    md2.labels = {"plant_1", "plant_2"};
    {
        Binary::open_file(path1, 'w', md1);
    }
    {
        Binary::open_file(path2, 'w', md2);
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::MetadataMismatch);
}

TEST_F(BinaryCompareFixture, DifferentLabelsReturnsMetadataMismatch) {
    auto md1 = make_metadata();
    auto md2 = make_metadata();
    md2.labels = {"gen_1", "gen_2"};
    {
        Binary::open_file(path1, 'w', md1);
    }
    {
        Binary::open_file(path2, 'w', md2);
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::MetadataMismatch);
}

// ============================================================================
// CompareFiles — Report with detailed_report=true
// ============================================================================

TEST_F(BinaryCompareFixture, FileMatchReportIsNotEmpty) {
    auto md = make_metadata();
    {
        Binary::open_file(path1, 'w', md);
    }
    {
        Binary::open_file(path2, 'w', md);
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true});
    EXPECT_EQ(result.status, CompareStatus::FileMatch);
    EXPECT_FALSE(result.report.empty());
}

TEST_F(BinaryCompareFixture, DataMismatchReportContainsDimensionInfo) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 2}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({99.0, 20.0}, {{"scenario", 1}, {"block", 2}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
    EXPECT_NE(result.report.find("scenario=1"), std::string::npos);
    EXPECT_NE(result.report.find("block=2"), std::string::npos);
    EXPECT_NE(result.report.find("plant_1"), std::string::npos);
}

TEST_F(BinaryCompareFixture, DataMismatchReportContainsValues) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({99.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true});
    EXPECT_NE(result.report.find("10.0"), std::string::npos);
    EXPECT_NE(result.report.find("99.0"), std::string::npos);
}

TEST_F(BinaryCompareFixture, DataMismatchReportShowsNaN) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        // leave position unwritten (NaN)
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({5.0, 6.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
    EXPECT_NE(result.report.find("NaN"), std::string::npos);
}

TEST_F(BinaryCompareFixture, DataMismatchReportOnlyShowsDifferingPositions) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
        bin.write({30.0, 40.0}, {{"scenario", 1}, {"block", 2}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});  // same
        bin.write({99.0, 40.0}, {{"scenario", 1}, {"block", 2}});  // different
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
    // block=2 should appear (mismatch), block=1 should not (match)
    EXPECT_NE(result.report.find("block=2"), std::string::npos);
    EXPECT_EQ(result.report.find("block=1"), std::string::npos);
}

TEST_F(BinaryCompareFixture, CustomMaxReportLinesTruncatesReport) {
    // 2 scenarios x 3 blocks x 2 labels = 12 mismatch lines, truncated at 5
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        for (int64_t s = 1; s <= 2; ++s)
            for (int64_t b = 1; b <= 3; ++b)
                bin.write({1.0, 2.0}, {{"scenario", s}, {"block", b}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        for (int64_t s = 1; s <= 2; ++s)
            for (int64_t b = 1; b <= 3; ++b)
                bin.write({99.0, 98.0}, {{"scenario", s}, {"block", b}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true, .max_report_lines = 5});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
    EXPECT_NE(result.report.find("more ..."), std::string::npos);
}

TEST_F(BinaryCompareFixture, MetadataMismatchReportIsNotEmpty) {
    auto md1 = make_metadata();
    auto md2 = make_metadata();
    md2.unit = "GWh";
    {
        Binary::open_file(path1, 'w', md1);
    }
    {
        Binary::open_file(path2, 'w', md2);
    }

    auto result = BinaryComparator::compare(path1, path2, {.detailed_report = true});
    EXPECT_EQ(result.status, CompareStatus::MetadataMismatch);
    EXPECT_FALSE(result.report.empty());
}

// ============================================================================
// CompareFiles — Report with detailed_report=false (default)
// ============================================================================

TEST_F(BinaryCompareFixture, FileMatchNoReportIsEmpty) {
    auto md = make_metadata();
    {
        Binary::open_file(path1, 'w', md);
    }
    {
        Binary::open_file(path2, 'w', md);
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::FileMatch);
    EXPECT_TRUE(result.report.empty());
}

TEST_F(BinaryCompareFixture, DataMismatchNoReportIsEmpty) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({99.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
    EXPECT_TRUE(result.report.empty());
}

TEST_F(BinaryCompareFixture, MetadataMismatchNoReportIsEmpty) {
    auto md1 = make_metadata();
    auto md2 = make_metadata();
    md2.unit = "GWh";
    {
        Binary::open_file(path1, 'w', md1);
    }
    {
        Binary::open_file(path2, 'w', md2);
    }

    auto result = BinaryComparator::compare(path1, path2);
    EXPECT_EQ(result.status, CompareStatus::MetadataMismatch);
    EXPECT_TRUE(result.report.empty());
}

// ============================================================================
// CompareFiles — Tolerance
// ============================================================================

TEST_F(BinaryCompareFixture, AbsoluteToleranceMatchesWithinThreshold) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({10.05, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.absolute_tolerance = 0.1});
    EXPECT_EQ(result.status, CompareStatus::FileMatch);
}

TEST_F(BinaryCompareFixture, AbsoluteToleranceFailsOutsideThreshold) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({10.05, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.absolute_tolerance = 0.01});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
}

TEST_F(BinaryCompareFixture, RelativeToleranceMatch) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({100.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({100.05, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    // diff = 0.05, threshold = 0.0 + 0.001 * 100.05 = 0.10005
    auto result = BinaryComparator::compare(path1, path2, {.absolute_tolerance = 0.0, .relative_tolerance = 0.001});
    EXPECT_EQ(result.status, CompareStatus::FileMatch);
}

TEST_F(BinaryCompareFixture, RelativeToleranceFailsOutsideThreshold) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({100.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        bin.write({100.2, 20.0}, {{"scenario", 1}, {"block", 1}});
    }

    // diff = 0.2, threshold = 0.0 + 0.001 * 100.2 = 0.1002
    auto result = BinaryComparator::compare(path1, path2, {.absolute_tolerance = 0.0, .relative_tolerance = 0.001});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
}

TEST_F(BinaryCompareFixture, ToleranceReportOnlyShowsOutOfTolerance) {
    auto md = make_metadata();
    {
        auto bin = Binary::open_file(path1, 'w', md);
        bin.write({10.0, 20.0}, {{"scenario", 1}, {"block", 1}});
    }
    {
        auto bin = Binary::open_file(path2, 'w', md);
        // plant_1: diff = 0.01 (within atol=0.1), plant_2: diff = 5.0 (outside)
        bin.write({10.01, 25.0}, {{"scenario", 1}, {"block", 1}});
    }

    auto result = BinaryComparator::compare(path1, path2, {.absolute_tolerance = 0.1, .detailed_report = true});
    EXPECT_EQ(result.status, CompareStatus::DataMismatch);
    EXPECT_EQ(result.report.find("plant_1"), std::string::npos);
    EXPECT_NE(result.report.find("plant_2"), std::string::npos);
}
