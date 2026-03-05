#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/c/blob/blob_metadata.h>
#include <quiver/c/element.h>
#include <string>

// ============================================================================
// Lifecycle
// ============================================================================

TEST(BlobCApiMetadata, CreateAndDestroy) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    ASSERT_NE(md, nullptr);
    EXPECT_EQ(quiver_blob_metadata_destroy(md), QUIVER_OK);
}

TEST(BlobCApiMetadata, DestroyNull) {
    EXPECT_EQ(quiver_blob_metadata_destroy(nullptr), QUIVER_OK);
}

// ============================================================================
// Builders and Getters
// ============================================================================

TEST(BlobCApiMetadata, SetAndGetUnit) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_unit(md, "MW"), QUIVER_OK);

    const char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");

    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, SetAndGetVersion) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_version(md, "1"), QUIVER_OK);

    const char* version = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_version(md, &version), QUIVER_OK);
    EXPECT_STREQ(version, "1");

    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, SetAndGetInitialDatetime) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_initial_datetime(md, "2025-01-01T00:00:00"), QUIVER_OK);

    char* datetime = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_initial_datetime(md, &datetime), QUIVER_OK);
    ASSERT_NE(datetime, nullptr);
    // Verify it contains a valid ISO 8601 date (exact value may differ by timezone,
    // matching C++ from_toml/to_toml behavior which uses mktime + std::format)
    EXPECT_NE(std::string(datetime).find("2025-01-01"), std::string::npos);
    quiver_blob_metadata_free_string(datetime);

    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, SetAndGetLabels) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    const char* labels[] = {"plant_1", "plant_2"};
    EXPECT_EQ(quiver_blob_metadata_set_labels(md, labels, 2), QUIVER_OK);

    char** out_labels = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_labels(md, &out_labels, &count), QUIVER_OK);
    ASSERT_EQ(count, 2u);
    EXPECT_STREQ(out_labels[0], "plant_1");
    EXPECT_STREQ(out_labels[1], "plant_2");
    quiver_blob_metadata_free_string_array(out_labels, count);

    quiver_blob_metadata_destroy(md);
}

// ============================================================================
// Dimensions
// ============================================================================

TEST(BlobCApiMetadata, AddDimensionAndGet) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "row", 3), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "col", 2), QUIVER_OK);

    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, &count), QUIVER_OK);
    ASSERT_EQ(count, 2u);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim), QUIVER_OK);
    EXPECT_STREQ(dim.name, "row");
    EXPECT_EQ(dim.size, 3);
    EXPECT_EQ(dim.is_time_dimension, 0);
    quiver_blob_metadata_free_dimension(&dim);

    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 1, &dim), QUIVER_OK);
    EXPECT_STREQ(dim.name, "col");
    EXPECT_EQ(dim.size, 2);
    EXPECT_EQ(dim.is_time_dimension, 0);
    quiver_blob_metadata_free_dimension(&dim);

    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, AddTimeDimensionAndGet) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, "month", 12, "monthly"), QUIVER_OK);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim), QUIVER_OK);
    EXPECT_STREQ(dim.name, "month");
    EXPECT_EQ(dim.size, 12);
    EXPECT_EQ(dim.is_time_dimension, 1);
    EXPECT_EQ(dim.time_properties.frequency, QUIVER_TIME_FREQUENCY_MONTHLY);
    quiver_blob_metadata_free_dimension(&dim);

    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, GetDimensionOutOfRange) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim), QUIVER_ERROR);

    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, GetNumberOfTimeDimensions) {
    const char* toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["plant_1", "plant_2"]
)";

    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_from_toml(toml, &md), QUIVER_OK);

    int64_t num_time = 0;
    EXPECT_EQ(quiver_blob_metadata_get_number_of_time_dimensions(md, &num_time), QUIVER_OK);
    EXPECT_EQ(num_time, 2);

    quiver_blob_metadata_destroy(md);
}

// ============================================================================
// TOML round-trip
// ============================================================================

TEST(BlobCApiMetadata, TomlRoundTrip) {
    const char* toml_input = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["plant_1", "plant_2"]
)";

    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_from_toml(toml_input, &md), QUIVER_OK);
    ASSERT_NE(md, nullptr);

    // Verify fields
    const char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");

    const char* version = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_version(md, &version), QUIVER_OK);
    EXPECT_STREQ(version, "1");

    size_t dim_count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, &dim_count), QUIVER_OK);
    EXPECT_EQ(dim_count, 2u);

    // Serialize back to TOML
    char* toml_output = nullptr;
    EXPECT_EQ(quiver_blob_metadata_to_toml(md, &toml_output), QUIVER_OK);
    ASSERT_NE(toml_output, nullptr);

    // Parse again and verify
    quiver_blob_metadata_t* md2 = nullptr;
    ASSERT_EQ(quiver_blob_metadata_from_toml(toml_output, &md2), QUIVER_OK);

    const char* unit2 = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md2, &unit2), QUIVER_OK);
    EXPECT_STREQ(unit2, "MW");

    quiver_blob_metadata_free_string(toml_output);
    quiver_blob_metadata_destroy(md2);
    quiver_blob_metadata_destroy(md);
}

// ============================================================================
// From Element
// ============================================================================

TEST(BlobCApiMetadata, FromElement) {
    quiver_element_t* el = nullptr;
    ASSERT_EQ(quiver_element_create(&el), QUIVER_OK);

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
    ASSERT_EQ(quiver_blob_metadata_from_element(el, &md), QUIVER_OK);
    ASSERT_NE(md, nullptr);

    const char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");

    size_t dim_count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, &dim_count), QUIVER_OK);
    EXPECT_EQ(dim_count, 2u);

    quiver_blob_metadata_destroy(md);
    quiver_element_destroy(el);
}

// ============================================================================
// Error cases
// ============================================================================

TEST(BlobCApiMetadata, NullArgs) {
    EXPECT_EQ(quiver_blob_metadata_create(nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_metadata_from_toml(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_metadata_set_unit(nullptr, "MW"), QUIVER_ERROR);

    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_set_unit(md, nullptr), QUIVER_ERROR);
    quiver_blob_metadata_destroy(md);
}

TEST(BlobCApiMetadata, InvalidToml) {
    quiver_blob_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_blob_metadata_from_toml("not valid toml {{{", &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
}

TEST(BlobCApiMetadata, InvalidFrequency) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, "bad", 10, "invalid_freq"), QUIVER_ERROR);
    quiver_blob_metadata_destroy(md);
}
