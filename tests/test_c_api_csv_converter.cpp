#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/c/binary/binary.h>
#include <quiver/c/binary/csv_converter.h>
#include <quiver/c/binary/binary_metadata.h>
#include <quiver/c/common.h>
#include <quiver/c/element.h>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BinaryCApiCSVFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_c_csv_converter_test").string(); }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full))
                fs::remove(full);
        }
    }

    std::string path;

    quiver_binary_metadata_t* make_simple_metadata() {
        quiver_element_t* el = nullptr;
        quiver_element_create(&el);
        quiver_element_set_string(el, "version", "1");
        quiver_element_set_string(el, "initial_datetime", "2025-01-01T00:00:00");
        quiver_element_set_string(el, "unit", "MW");

        const char* dims[] = {"row", "col"};
        quiver_element_set_array_string(el, "dimensions", dims, 2);
        int64_t sizes[] = {3, 2};
        quiver_element_set_array_integer(el, "dimension_sizes", sizes, 2);
        const char* labels[] = {"val1", "val2"};
        quiver_element_set_array_string(el, "labels", labels, 2);

        quiver_binary_metadata_t* md = nullptr;
        quiver_binary_metadata_from_element(el, &md);
        quiver_element_destroy(el);
        return md;
    }
};

// ============================================================================
// bin_to_csv
// ============================================================================

TEST_F(BinaryCApiCSVFixture, BinToCsvCreatesFile) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    EXPECT_EQ(quiver_csv_converter_bin_to_csv(path.c_str(), 1), QUIVER_OK);
    EXPECT_TRUE(fs::exists(path + ".csv"));
}

// ============================================================================
// csv_to_bin
// ============================================================================

TEST_F(BinaryCApiCSVFixture, CsvToBinCreatesFile) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);

        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double data[] = {static_cast<double>(r), static_cast<double>(c)};
                quiver_binary_write(binary, dim_names, dim_values, 2, data, 2);
            }
        }
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    quiver_csv_converter_bin_to_csv(path.c_str(), 0);
    fs::remove(path + ".qvr");

    EXPECT_EQ(quiver_csv_converter_csv_to_bin(path.c_str()), QUIVER_OK);
    EXPECT_TRUE(fs::exists(path + ".qvr"));
}

// ============================================================================
// Round-trip
// ============================================================================

TEST_F(BinaryCApiCSVFixture, RoundTrip) {
    auto* md = make_simple_metadata();

    // Write binary
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {42.5, 99.5};
        quiver_binary_write(binary, dim_names, dim_values, 2, data, 2);

        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    // bin -> csv
    ASSERT_EQ(quiver_csv_converter_bin_to_csv(path.c_str(), 0), QUIVER_OK);
    fs::remove(path + ".qvr");

    // csv -> bin
    ASSERT_EQ(quiver_csv_converter_csv_to_bin(path.c_str()), QUIVER_OK);

    // Read back and verify
    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_DOUBLE_EQ(out_data[0], 42.5);
        EXPECT_DOUBLE_EQ(out_data[1], 99.5);

        quiver_binary_free_float_array(out_data);
        quiver_binary_close(binary);
    }
}

// ============================================================================
// Aggregate parameter
// ============================================================================

TEST_F(BinaryCApiCSVFixture, BinToCsvNoAggregate) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);

        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double data[] = {static_cast<double>(r * 10 + c), static_cast<double>(r * 10 + c + 100)};
                quiver_binary_write(binary, dim_names, dim_values, 2, data, 2);
            }
        }
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    EXPECT_EQ(quiver_csv_converter_bin_to_csv(path.c_str(), 0), QUIVER_OK);
    EXPECT_TRUE(fs::exists(path + ".csv"));
}

TEST_F(BinaryCApiCSVFixture, BinToCsvAggregate) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);

        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double data[] = {static_cast<double>(r * 10 + c), static_cast<double>(r * 10 + c + 100)};
                quiver_binary_write(binary, dim_names, dim_values, 2, data, 2);
            }
        }
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    EXPECT_EQ(quiver_csv_converter_bin_to_csv(path.c_str(), 1), QUIVER_OK);
    EXPECT_TRUE(fs::exists(path + ".csv"));
}

// ============================================================================
// CSV round-trip with all positions written
// ============================================================================

TEST_F(BinaryCApiCSVFixture, RoundTripAllPositions) {
    auto* md = make_simple_metadata();

    // Write all 3x2 positions
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);

        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double data[] = {r * 100.0 + c, r * 100.0 + c + 0.5};
                quiver_binary_write(binary, dim_names, dim_values, 2, data, 2);
            }
        }
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    ASSERT_EQ(quiver_csv_converter_bin_to_csv(path.c_str(), 0), QUIVER_OK);
    fs::remove(path + ".qvr");
    ASSERT_EQ(quiver_csv_converter_csv_to_bin(path.c_str()), QUIVER_OK);

    // Verify all positions
    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double* out_data = nullptr;
                size_t out_count = 0;
                EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
                ASSERT_EQ(out_count, 2u);
                EXPECT_DOUBLE_EQ(out_data[0], r * 100.0 + c);
                EXPECT_DOUBLE_EQ(out_data[1], r * 100.0 + c + 0.5);
                quiver_binary_free_float_array(out_data);
            }
        }
        quiver_binary_close(binary);
    }
}

// ============================================================================
// Error cases
// ============================================================================

TEST_F(BinaryCApiCSVFixture, NullPath) {
    EXPECT_EQ(quiver_csv_converter_bin_to_csv(nullptr, 1), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path");

    EXPECT_EQ(quiver_csv_converter_csv_to_bin(nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path");
}

TEST_F(BinaryCApiCSVFixture, BinToCsvNonExistentFile) {
    EXPECT_EQ(quiver_csv_converter_bin_to_csv("nonexistent_path", 0), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_FALSE(err.empty());
}

TEST_F(BinaryCApiCSVFixture, CsvToBinNonExistentFile) {
    EXPECT_EQ(quiver_csv_converter_csv_to_bin("nonexistent_path"), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_FALSE(err.empty());
}

TEST_F(BinaryCApiCSVFixture, CsvToBinMissingToml) {
    // Create a CSV file but no TOML sidecar
    {
        std::ofstream csv(path + ".csv");
        csv << "row,col,val1,val2\n1,1,1.0,2.0\n";
    }

    EXPECT_EQ(quiver_csv_converter_csv_to_bin(path.c_str()), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_FALSE(err.empty());

    fs::remove(path + ".csv");
}
