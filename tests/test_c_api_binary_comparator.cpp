#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/c/binary/binary_file.h>
#include <quiver/c/binary/binary_comparator.h>
#include <quiver/c/binary/binary_metadata.h>
#include <quiver/c/common.h>
#include <quiver/c/element.h>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BinaryCApiCompareFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path1 = (fs::temp_directory_path() / "quiver_c_compare_test_1").string();
        path2 = (fs::temp_directory_path() / "quiver_c_compare_test_2").string();
    }

    void TearDown() override {
        for (auto* p : {&path1, &path2}) {
            for (auto ext : {".qvr", ".toml"}) {
                auto full = *p + ext;
                if (fs::exists(full))
                    fs::remove(full);
            }
        }
    }

    std::string path1;
    std::string path2;

    quiver_binary_metadata_t* make_metadata() {
        quiver_element_t* el = nullptr;
        quiver_element_create(&el);
        quiver_element_set_string(el, "version", "1");
        quiver_element_set_string(el, "initial_datetime", "2025-01-01T00:00:00");
        quiver_element_set_string(el, "unit", "MW");

        const char* dims[] = {"scenario", "block"};
        quiver_element_set_array_string(el, "dimensions", dims, 2);
        int64_t sizes[] = {2, 3};
        quiver_element_set_array_integer(el, "dimension_sizes", sizes, 2);
        const char* labels[] = {"plant_1", "plant_2"};
        quiver_element_set_array_string(el, "labels", labels, 2);

        quiver_binary_metadata_t* md = nullptr;
        quiver_binary_metadata_from_element(el, &md);
        quiver_element_destroy(el);
        return md;
    }

    void write_file(const std::string& path, quiver_binary_metadata_t* md, double val1, double val2) {
        quiver_binary_file_t* binary = nullptr;
        quiver_binary_file_open_write(path.c_str(), md, &binary);

        const char* dim_names[] = {"scenario", "block"};
        int64_t dim_values[] = {1, 1};
        double data[] = {val1, val2};
        quiver_binary_file_write(binary, dim_names, dim_values, 2, data, 2);
        quiver_binary_file_close(binary);
    }
};

// ============================================================================
// Tests
// ============================================================================

TEST_F(BinaryCApiCompareFixture, FileMatch) {
    auto* md = make_metadata();
    write_file(path1, md, 10.0, 20.0);
    write_file(path2, md, 10.0, 20.0);
    quiver_binary_metadata_free(md);

    auto options = quiver_compare_options_default();
    quiver_compare_status_t status;
    char* report = nullptr;
    ASSERT_EQ(quiver_binary_compare_files(path1.c_str(), path2.c_str(), &options, &status, &report), QUIVER_OK);
    EXPECT_EQ(status, QUIVER_COMPARE_FILE_MATCH);
    EXPECT_EQ(report, nullptr);
}

TEST_F(BinaryCApiCompareFixture, DataMismatch) {
    auto* md = make_metadata();
    write_file(path1, md, 10.0, 20.0);
    write_file(path2, md, 99.0, 20.0);
    quiver_binary_metadata_free(md);

    auto options = quiver_compare_options_default();
    quiver_compare_status_t status;
    char* report = nullptr;
    ASSERT_EQ(quiver_binary_compare_files(path1.c_str(), path2.c_str(), &options, &status, &report), QUIVER_OK);
    EXPECT_EQ(status, QUIVER_COMPARE_DATA_MISMATCH);
    EXPECT_EQ(report, nullptr);
}

TEST_F(BinaryCApiCompareFixture, MetadataMismatch) {
    auto* md1 = make_metadata();
    write_file(path1, md1, 10.0, 20.0);
    quiver_binary_metadata_free(md1);

    // Create second file with different unit
    quiver_element_t* el = nullptr;
    quiver_element_create(&el);
    quiver_element_set_string(el, "version", "1");
    quiver_element_set_string(el, "initial_datetime", "2025-01-01T00:00:00");
    quiver_element_set_string(el, "unit", "GWh");
    const char* dims[] = {"scenario", "block"};
    quiver_element_set_array_string(el, "dimensions", dims, 2);
    int64_t sizes[] = {2, 3};
    quiver_element_set_array_integer(el, "dimension_sizes", sizes, 2);
    const char* labels[] = {"plant_1", "plant_2"};
    quiver_element_set_array_string(el, "labels", labels, 2);

    quiver_binary_metadata_t* md2 = nullptr;
    quiver_binary_metadata_from_element(el, &md2);
    quiver_element_destroy(el);

    write_file(path2, md2, 10.0, 20.0);
    quiver_binary_metadata_free(md2);

    auto options = quiver_compare_options_default();
    quiver_compare_status_t status;
    char* report = nullptr;
    ASSERT_EQ(quiver_binary_compare_files(path1.c_str(), path2.c_str(), &options, &status, &report), QUIVER_OK);
    EXPECT_EQ(status, QUIVER_COMPARE_METADATA_MISMATCH);
    EXPECT_EQ(report, nullptr);
}

TEST_F(BinaryCApiCompareFixture, DetailedReportOnMatch) {
    auto* md = make_metadata();
    write_file(path1, md, 10.0, 20.0);
    write_file(path2, md, 10.0, 20.0);
    quiver_binary_metadata_free(md);

    auto options = quiver_compare_options_default();
    options.detailed_report = 1;
    quiver_compare_status_t status;
    char* report = nullptr;
    ASSERT_EQ(quiver_binary_compare_files(path1.c_str(), path2.c_str(), &options, &status, &report), QUIVER_OK);
    EXPECT_EQ(status, QUIVER_COMPARE_FILE_MATCH);
    ASSERT_NE(report, nullptr);
    quiver_binary_comparator_free_string(report);
}

TEST_F(BinaryCApiCompareFixture, DetailedReportOnDataMismatch) {
    auto* md = make_metadata();
    write_file(path1, md, 10.0, 20.0);
    write_file(path2, md, 99.0, 20.0);
    quiver_binary_metadata_free(md);

    auto options = quiver_compare_options_default();
    options.detailed_report = 1;
    quiver_compare_status_t status;
    char* report = nullptr;
    ASSERT_EQ(quiver_binary_compare_files(path1.c_str(), path2.c_str(), &options, &status, &report), QUIVER_OK);
    EXPECT_EQ(status, QUIVER_COMPARE_DATA_MISMATCH);
    ASSERT_NE(report, nullptr);

    std::string report_str(report);
    EXPECT_NE(report_str.find("plant_1"), std::string::npos);
    quiver_binary_comparator_free_string(report);
}

TEST_F(BinaryCApiCompareFixture, NullArgs) {
    auto options = quiver_compare_options_default();
    quiver_compare_status_t status;
    char* report = nullptr;
    EXPECT_EQ(quiver_binary_compare_files(nullptr, nullptr, &options, &status, &report), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path1");
}

TEST_F(BinaryCApiCompareFixture, NonExistentFile) {
    auto* md = make_metadata();
    write_file(path1, md, 10.0, 20.0);
    quiver_binary_metadata_free(md);

    auto options = quiver_compare_options_default();
    quiver_compare_status_t status;
    char* report = nullptr;
    EXPECT_EQ(quiver_binary_compare_files(path1.c_str(), "nonexistent", &options, &status, &report), QUIVER_ERROR);
}
