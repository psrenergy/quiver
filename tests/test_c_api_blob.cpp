#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/c/blob/blob.h>
#include <quiver/c/blob/blob_metadata.h>
#include <quiver/c/common.h>
#include <quiver/c/element.h>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BlobCApiFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_c_blob_test").string(); }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full))
                fs::remove(full);
        }
    }

    std::string path;

    quiver_blob_metadata_t* make_simple_metadata() {
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

        quiver_blob_metadata_t* md = nullptr;
        quiver_blob_metadata_from_element(el, &md);
        quiver_element_destroy(el);
        return md;
    }
};

// ============================================================================
// Open/Close lifecycle
// ============================================================================

TEST_F(BlobCApiFixture, OpenWriteAndClose) {
    auto* md = make_simple_metadata();
    ASSERT_NE(md, nullptr);

    quiver_blob_t* blob = nullptr;
    ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);
    ASSERT_NE(blob, nullptr);

    EXPECT_TRUE(fs::exists(path + ".qvr"));
    EXPECT_TRUE(fs::exists(path + ".toml"));

    EXPECT_EQ(quiver_blob_close(blob), QUIVER_OK);
    quiver_blob_metadata_free(md);
}

TEST_F(BlobCApiFixture, OpenReadAfterWrite) {
    auto* md = make_simple_metadata();
    {
        quiver_blob_t* blob = nullptr;
        quiver_blob_open_write(path.c_str(), md, &blob);
        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    quiver_blob_t* reader = nullptr;
    EXPECT_EQ(quiver_blob_open_read(path.c_str(), &reader), QUIVER_OK);
    ASSERT_NE(reader, nullptr);
    quiver_blob_close(reader);
}

TEST_F(BlobCApiFixture, OpenReadNonExistent) {
    quiver_blob_t* blob = nullptr;
    EXPECT_EQ(quiver_blob_open_read("nonexistent_path", &blob), QUIVER_ERROR);
}

// ============================================================================
// Write + Read round-trip
// ============================================================================

TEST_F(BlobCApiFixture, WriteReadRoundTrip) {
    auto* md = make_simple_metadata();

    // Write
    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {1.5, 2.5};
        EXPECT_EQ(quiver_blob_write(blob, dim_names, dim_values, 2, data, 2), QUIVER_OK);

        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    // Read
    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_DOUBLE_EQ(out_data[0], 1.5);
        EXPECT_DOUBLE_EQ(out_data[1], 2.5);

        quiver_blob_free_float_array(out_data);
        quiver_blob_close(blob);
    }
}

TEST_F(BlobCApiFixture, WriteReadMultiplePositions) {
    auto* md = make_simple_metadata();

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};

        int64_t dims1[] = {1, 1};
        double data1[] = {1.0, 2.0};
        quiver_blob_write(blob, dim_names, dims1, 2, data1, 2);

        int64_t dims2[] = {2, 2};
        double data2[] = {3.0, 4.0};
        quiver_blob_write(blob, dim_names, dims2, 2, data2, 2);

        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        double* out_data = nullptr;
        size_t out_count = 0;

        int64_t dims1[] = {1, 1};
        quiver_blob_read(blob, dim_names, dims1, 2, 0, &out_data, &out_count);
        EXPECT_DOUBLE_EQ(out_data[0], 1.0);
        quiver_blob_free_float_array(out_data);

        int64_t dims2[] = {2, 2};
        quiver_blob_read(blob, dim_names, dims2, 2, 0, &out_data, &out_count);
        EXPECT_DOUBLE_EQ(out_data[0], 3.0);
        quiver_blob_free_float_array(out_data);

        quiver_blob_close(blob);
    }
}

// ============================================================================
// Getters
// ============================================================================

TEST_F(BlobCApiFixture, GetMetadata) {
    auto* md = make_simple_metadata();
    {
        quiver_blob_t* blob = nullptr;
        quiver_blob_open_write(path.c_str(), md, &blob);
        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    quiver_blob_t* blob = nullptr;
    ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

    quiver_blob_metadata_t* read_md = nullptr;
    EXPECT_EQ(quiver_blob_get_metadata(blob, &read_md), QUIVER_OK);
    ASSERT_NE(read_md, nullptr);

    char* unit = nullptr;
    quiver_blob_metadata_get_unit(read_md, &unit);
    EXPECT_STREQ(unit, "MW");
    quiver_blob_metadata_free_string(unit);

    size_t dim_count = 0;
    quiver_blob_metadata_get_dimension_count(read_md, &dim_count);
    EXPECT_EQ(dim_count, 2u);

    quiver_blob_metadata_free(read_md);
    quiver_blob_close(blob);
}

TEST_F(BlobCApiFixture, GetFilePath) {
    auto* md = make_simple_metadata();
    quiver_blob_t* blob = nullptr;
    ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

    char* file_path = nullptr;
    EXPECT_EQ(quiver_blob_get_file_path(blob, &file_path), QUIVER_OK);
    EXPECT_STREQ(file_path, path.c_str());
    quiver_blob_free_string(file_path);

    quiver_blob_close(blob);
    quiver_blob_metadata_free(md);
}

// ============================================================================
// Error cases
// ============================================================================

TEST_F(BlobCApiFixture, NullArgs) {
    EXPECT_EQ(quiver_blob_open_read(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_open_write(nullptr, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_close(nullptr), QUIVER_OK);
    EXPECT_EQ(quiver_blob_get_file_path(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_get_metadata(nullptr, nullptr), QUIVER_ERROR);
}

TEST_F(BlobCApiFixture, NullArgsErrorMessages) {
    EXPECT_EQ(quiver_blob_open_read(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path");

    EXPECT_EQ(quiver_blob_open_write(nullptr, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path");

    EXPECT_EQ(quiver_blob_get_file_path(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: blob");

    EXPECT_EQ(quiver_blob_get_metadata(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: blob");
}

TEST_F(BlobCApiFixture, OpenReadNullOut) {
    EXPECT_EQ(quiver_blob_open_read("some_path", nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");
}

TEST_F(BlobCApiFixture, OpenWriteNullMetadata) {
    quiver_blob_t* blob = nullptr;
    EXPECT_EQ(quiver_blob_open_write(path.c_str(), nullptr, &blob), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
}

TEST_F(BlobCApiFixture, OpenWriteNullOut) {
    auto* md = make_simple_metadata();
    EXPECT_EQ(quiver_blob_open_write(path.c_str(), md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");
    quiver_blob_metadata_free(md);
}

TEST_F(BlobCApiFixture, ReadNullBlob) {
    double* out_data = nullptr;
    size_t out_count = 0;
    EXPECT_EQ(quiver_blob_read(nullptr, nullptr, nullptr, 0, 0, &out_data, &out_count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: blob");
}

TEST_F(BlobCApiFixture, ReadNullOutParams) {
    auto* md = make_simple_metadata();
    quiver_blob_t* blob = nullptr;
    ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);
    quiver_blob_close(blob);
    quiver_blob_metadata_free(md);

    ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};

    EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 0, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out_data");

    quiver_blob_close(blob);
}

TEST_F(BlobCApiFixture, WriteNullBlob) {
    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};
    double data[] = {1.0};
    EXPECT_EQ(quiver_blob_write(nullptr, dim_names, dim_values, 2, data, 1), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: blob");
}

TEST_F(BlobCApiFixture, WriteNullData) {
    auto* md = make_simple_metadata();
    quiver_blob_t* blob = nullptr;
    ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};
    EXPECT_EQ(quiver_blob_write(blob, dim_names, dim_values, 2, nullptr, 2), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: data");

    quiver_blob_close(blob);
    quiver_blob_metadata_free(md);
}

TEST_F(BlobCApiFixture, OpenReadNonExistentErrorMessage) {
    quiver_blob_t* blob = nullptr;
    EXPECT_EQ(quiver_blob_open_read("nonexistent_path", &blob), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_FALSE(err.empty());
}

// ============================================================================
// Edge-case values
// ============================================================================

TEST_F(BlobCApiFixture, WriteReadSpecialFloatValues) {
    auto* md = make_simple_metadata();

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
        EXPECT_EQ(quiver_blob_write(blob, dim_names, dim_values, 2, data, 2), QUIVER_OK);

        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_TRUE(std::isinf(out_data[0]) && out_data[0] > 0);
        EXPECT_TRUE(std::isinf(out_data[1]) && out_data[1] < 0);

        quiver_blob_free_float_array(out_data);
        quiver_blob_close(blob);
    }
}

TEST_F(BlobCApiFixture, WriteReadLargeSmallValues) {
    auto* md = make_simple_metadata();

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {std::numeric_limits<double>::max(), std::numeric_limits<double>::min()};
        EXPECT_EQ(quiver_blob_write(blob, dim_names, dim_values, 2, data, 2), QUIVER_OK);

        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_DOUBLE_EQ(out_data[0], std::numeric_limits<double>::max());
        EXPECT_DOUBLE_EQ(out_data[1], std::numeric_limits<double>::min());

        quiver_blob_free_float_array(out_data);
        quiver_blob_close(blob);
    }
}

// ============================================================================
// Free function validation
// ============================================================================

TEST_F(BlobCApiFixture, FreeFloatArrayNull) {
    EXPECT_EQ(quiver_blob_free_float_array(nullptr), QUIVER_OK);
}

// ============================================================================
// Dimension mismatch errors
// ============================================================================

TEST_F(BlobCApiFixture, ReadUnwrittenPositionFails) {
    auto* md = make_simple_metadata();
    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_write(path.c_str(), md, &blob), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {1.0, 2.0};
        quiver_blob_write(blob, dim_names, dim_values, 2, data, 2);
        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    {
        quiver_blob_t* blob = nullptr;
        ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

        // Position (2,1) was never written
        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {2, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_ERROR);
        std::string err = quiver_get_last_error();
        EXPECT_NE(err.find("null values"), std::string::npos);

        quiver_blob_close(blob);
    }
}

TEST_F(BlobCApiFixture, ReadAllowNulls) {
    auto* md = make_simple_metadata();
    {
        quiver_blob_t* blob = nullptr;
        quiver_blob_open_write(path.c_str(), md, &blob);
        quiver_blob_close(blob);
    }
    quiver_blob_metadata_free(md);

    quiver_blob_t* blob = nullptr;
    ASSERT_EQ(quiver_blob_open_read(path.c_str(), &blob), QUIVER_OK);

    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};
    double* out_data = nullptr;
    size_t out_count = 0;

    // Without allow_nulls should fail (unwritten position)
    EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_ERROR);

    // With allow_nulls should succeed
    EXPECT_EQ(quiver_blob_read(blob, dim_names, dim_values, 2, 1, &out_data, &out_count), QUIVER_OK);
    ASSERT_EQ(out_count, 2u);
    EXPECT_TRUE(std::isnan(out_data[0]));
    EXPECT_TRUE(std::isnan(out_data[1]));
    quiver_blob_free_float_array(out_data);

    quiver_blob_close(blob);
}
