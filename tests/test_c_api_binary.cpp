#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/c/binary/binary.h>
#include <quiver/c/binary/binary_metadata.h>
#include <quiver/c/common.h>
#include <quiver/c/element.h>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class BinaryCApiFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_c_binary_test").string(); }

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
// Open/Close lifecycle
// ============================================================================

TEST_F(BinaryCApiFixture, OpenWriteAndClose) {
    auto* md = make_simple_metadata();
    ASSERT_NE(md, nullptr);

    quiver_binary_t* binary = nullptr;
    ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);
    ASSERT_NE(binary, nullptr);

    EXPECT_TRUE(fs::exists(path + ".qvr"));
    EXPECT_TRUE(fs::exists(path + ".toml"));

    EXPECT_EQ(quiver_binary_close(binary), QUIVER_OK);
    quiver_binary_metadata_free(md);
}

TEST_F(BinaryCApiFixture, OpenReadAfterWrite) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    quiver_binary_t* reader = nullptr;
    EXPECT_EQ(quiver_binary_open_read(path.c_str(), &reader), QUIVER_OK);
    ASSERT_NE(reader, nullptr);
    quiver_binary_close(reader);
}

TEST_F(BinaryCApiFixture, OpenReadNonExistent) {
    quiver_binary_t* binary = nullptr;
    EXPECT_EQ(quiver_binary_open_read("nonexistent_path", &binary), QUIVER_ERROR);
}

// ============================================================================
// Write + Read round-trip
// ============================================================================

TEST_F(BinaryCApiFixture, WriteReadRoundTrip) {
    auto* md = make_simple_metadata();

    // Write
    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {1.5, 2.5};
        EXPECT_EQ(quiver_binary_write(binary, dim_names, dim_values, 2, data, 2), QUIVER_OK);

        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    // Read
    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_DOUBLE_EQ(out_data[0], 1.5);
        EXPECT_DOUBLE_EQ(out_data[1], 2.5);

        quiver_binary_free_float_array(out_data);
        quiver_binary_close(binary);
    }
}

TEST_F(BinaryCApiFixture, WriteReadMultiplePositions) {
    auto* md = make_simple_metadata();

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};

        int64_t dims1[] = {1, 1};
        double data1[] = {1.0, 2.0};
        quiver_binary_write(binary, dim_names, dims1, 2, data1, 2);

        int64_t dims2[] = {2, 2};
        double data2[] = {3.0, 4.0};
        quiver_binary_write(binary, dim_names, dims2, 2, data2, 2);

        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        double* out_data = nullptr;
        size_t out_count = 0;

        int64_t dims1[] = {1, 1};
        quiver_binary_read(binary, dim_names, dims1, 2, 0, &out_data, &out_count);
        EXPECT_DOUBLE_EQ(out_data[0], 1.0);
        quiver_binary_free_float_array(out_data);

        int64_t dims2[] = {2, 2};
        quiver_binary_read(binary, dim_names, dims2, 2, 0, &out_data, &out_count);
        EXPECT_DOUBLE_EQ(out_data[0], 3.0);
        quiver_binary_free_float_array(out_data);

        quiver_binary_close(binary);
    }
}

// ============================================================================
// Getters
// ============================================================================

TEST_F(BinaryCApiFixture, GetMetadata) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    quiver_binary_t* binary = nullptr;
    ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

    quiver_binary_metadata_t* read_md = nullptr;
    EXPECT_EQ(quiver_binary_get_metadata(binary, &read_md), QUIVER_OK);
    ASSERT_NE(read_md, nullptr);

    char* unit = nullptr;
    quiver_binary_metadata_get_unit(read_md, &unit);
    EXPECT_STREQ(unit, "MW");
    quiver_binary_metadata_free_string(unit);

    size_t dim_count = 0;
    quiver_binary_metadata_get_dimension_count(read_md, &dim_count);
    EXPECT_EQ(dim_count, 2u);

    quiver_binary_metadata_free(read_md);
    quiver_binary_close(binary);
}

TEST_F(BinaryCApiFixture, GetFilePath) {
    auto* md = make_simple_metadata();
    quiver_binary_t* binary = nullptr;
    ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

    char* file_path = nullptr;
    EXPECT_EQ(quiver_binary_get_file_path(binary, &file_path), QUIVER_OK);
    EXPECT_STREQ(file_path, path.c_str());
    quiver_binary_free_string(file_path);

    quiver_binary_close(binary);
    quiver_binary_metadata_free(md);
}

// ============================================================================
// Error cases
// ============================================================================

TEST_F(BinaryCApiFixture, NullArgs) {
    EXPECT_EQ(quiver_binary_open_read(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_binary_open_write(nullptr, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_binary_close(nullptr), QUIVER_OK);
    EXPECT_EQ(quiver_binary_get_file_path(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_binary_get_metadata(nullptr, nullptr), QUIVER_ERROR);
}

TEST_F(BinaryCApiFixture, NullArgsErrorMessages) {
    EXPECT_EQ(quiver_binary_open_read(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path");

    EXPECT_EQ(quiver_binary_open_write(nullptr, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: path");

    EXPECT_EQ(quiver_binary_get_file_path(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: binary");

    EXPECT_EQ(quiver_binary_get_metadata(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: binary");
}

TEST_F(BinaryCApiFixture, OpenReadNullOut) {
    EXPECT_EQ(quiver_binary_open_read("some_path", nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");
}

TEST_F(BinaryCApiFixture, OpenWriteNullMetadata) {
    quiver_binary_t* binary = nullptr;
    EXPECT_EQ(quiver_binary_open_write(path.c_str(), nullptr, &binary), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
}

TEST_F(BinaryCApiFixture, OpenWriteNullOut) {
    auto* md = make_simple_metadata();
    EXPECT_EQ(quiver_binary_open_write(path.c_str(), md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");
    quiver_binary_metadata_free(md);
}

TEST_F(BinaryCApiFixture, ReadNullBinary) {
    double* out_data = nullptr;
    size_t out_count = 0;
    EXPECT_EQ(quiver_binary_read(nullptr, nullptr, nullptr, 0, 0, &out_data, &out_count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: binary");
}

TEST_F(BinaryCApiFixture, ReadNullOutParams) {
    auto* md = make_simple_metadata();
    quiver_binary_t* binary = nullptr;
    ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);
    quiver_binary_close(binary);
    quiver_binary_metadata_free(md);

    ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};

    EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out_data");

    quiver_binary_close(binary);
}

TEST_F(BinaryCApiFixture, WriteNullBinary) {
    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};
    double data[] = {1.0};
    EXPECT_EQ(quiver_binary_write(nullptr, dim_names, dim_values, 2, data, 1), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: binary");
}

TEST_F(BinaryCApiFixture, WriteNullData) {
    auto* md = make_simple_metadata();
    quiver_binary_t* binary = nullptr;
    ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};
    EXPECT_EQ(quiver_binary_write(binary, dim_names, dim_values, 2, nullptr, 2), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: data");

    quiver_binary_close(binary);
    quiver_binary_metadata_free(md);
}

TEST_F(BinaryCApiFixture, OpenReadNonExistentErrorMessage) {
    quiver_binary_t* binary = nullptr;
    EXPECT_EQ(quiver_binary_open_read("nonexistent_path", &binary), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_FALSE(err.empty());
}

// ============================================================================
// Edge-case values
// ============================================================================

TEST_F(BinaryCApiFixture, WriteReadSpecialFloatValues) {
    auto* md = make_simple_metadata();

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
        EXPECT_EQ(quiver_binary_write(binary, dim_names, dim_values, 2, data, 2), QUIVER_OK);

        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_TRUE(std::isinf(out_data[0]) && out_data[0] > 0);
        EXPECT_TRUE(std::isinf(out_data[1]) && out_data[1] < 0);

        quiver_binary_free_float_array(out_data);
        quiver_binary_close(binary);
    }
}

TEST_F(BinaryCApiFixture, WriteReadLargeSmallValues) {
    auto* md = make_simple_metadata();

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {std::numeric_limits<double>::max(), std::numeric_limits<double>::min()};
        EXPECT_EQ(quiver_binary_write(binary, dim_names, dim_values, 2, data, 2), QUIVER_OK);

        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_OK);
        ASSERT_EQ(out_count, 2u);
        EXPECT_DOUBLE_EQ(out_data[0], std::numeric_limits<double>::max());
        EXPECT_DOUBLE_EQ(out_data[1], std::numeric_limits<double>::min());

        quiver_binary_free_float_array(out_data);
        quiver_binary_close(binary);
    }
}

// ============================================================================
// Free function validation
// ============================================================================

TEST_F(BinaryCApiFixture, FreeFloatArrayNull) {
    EXPECT_EQ(quiver_binary_free_float_array(nullptr), QUIVER_OK);
}

// ============================================================================
// Dimension mismatch errors
// ============================================================================

TEST_F(BinaryCApiFixture, ReadUnwrittenPositionFails) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &binary), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {1, 1};
        double data[] = {1.0, 2.0};
        quiver_binary_write(binary, dim_names, dim_values, 2, data, 2);
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    {
        quiver_binary_t* binary = nullptr;
        ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

        // Position (2,1) was never written
        const char* dim_names[] = {"row", "col"};
        int64_t dim_values[] = {2, 1};
        double* out_data = nullptr;
        size_t out_count = 0;
        EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_ERROR);
        std::string err = quiver_get_last_error();
        EXPECT_NE(err.find("null values"), std::string::npos);

        quiver_binary_close(binary);
    }
}

// ============================================================================
// Write registry
// ============================================================================

TEST_F(BinaryCApiFixture, WriterBlocksReader) {
    auto* md = make_simple_metadata();
    quiver_binary_t* writer = nullptr;
    ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &writer), QUIVER_OK);

    quiver_binary_t* reader = nullptr;
    EXPECT_EQ(quiver_binary_open_read(path.c_str(), &reader), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("already open for writing"), std::string::npos);

    quiver_binary_close(writer);
    quiver_binary_metadata_free(md);
}

TEST_F(BinaryCApiFixture, ClosedWriterAllowsReader) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* writer = nullptr;
        ASSERT_EQ(quiver_binary_open_write(path.c_str(), md, &writer), QUIVER_OK);
        quiver_binary_close(writer);
    }
    quiver_binary_metadata_free(md);

    quiver_binary_t* reader = nullptr;
    EXPECT_EQ(quiver_binary_open_read(path.c_str(), &reader), QUIVER_OK);
    quiver_binary_close(reader);
}

TEST_F(BinaryCApiFixture, ReadAllowNulls) {
    auto* md = make_simple_metadata();
    {
        quiver_binary_t* binary = nullptr;
        quiver_binary_open_write(path.c_str(), md, &binary);
        quiver_binary_close(binary);
    }
    quiver_binary_metadata_free(md);

    quiver_binary_t* binary = nullptr;
    ASSERT_EQ(quiver_binary_open_read(path.c_str(), &binary), QUIVER_OK);

    const char* dim_names[] = {"row", "col"};
    int64_t dim_values[] = {1, 1};
    double* out_data = nullptr;
    size_t out_count = 0;

    // Without allow_nulls should fail (unwritten position)
    EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 0, &out_data, &out_count), QUIVER_ERROR);

    // With allow_nulls should succeed
    EXPECT_EQ(quiver_binary_read(binary, dim_names, dim_values, 2, 1, &out_data, &out_count), QUIVER_OK);
    ASSERT_EQ(out_count, 2u);
    EXPECT_TRUE(std::isnan(out_data[0]));
    EXPECT_TRUE(std::isnan(out_data[1]));
    quiver_binary_free_float_array(out_data);

    quiver_binary_close(binary);
}
