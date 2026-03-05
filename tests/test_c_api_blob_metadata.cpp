#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/c/blob/blob_metadata.h>
#include <quiver/c/common.h>
#include <quiver/c/element.h>
#include <string>

// ============================================================================
// Lifecycle
// ============================================================================

TEST(BlobCApiMetadata, CreateAndDestroy) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    ASSERT_NE(md, nullptr);
    EXPECT_EQ(quiver_blob_metadata_free(md), QUIVER_OK);
}

TEST(BlobCApiMetadata, DestroyNull) {
    EXPECT_EQ(quiver_blob_metadata_free(nullptr), QUIVER_OK);
}

// ============================================================================
// Builders and Getters
// ============================================================================

TEST(BlobCApiMetadata, SetAndGetUnit) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_unit(md, "MW"), QUIVER_OK);

    char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");
    quiver_blob_metadata_free_string(unit);

    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, SetAndGetVersion) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_version(md, "1"), QUIVER_OK);

    char* version = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_version(md, &version), QUIVER_OK);
    EXPECT_STREQ(version, "1");
    quiver_blob_metadata_free_string(version);

    quiver_blob_metadata_free(md);
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

    quiver_blob_metadata_free(md);
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

    quiver_blob_metadata_free(md);
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

    quiver_blob_metadata_free(md);
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

    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, GetDimensionOutOfRange) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim), QUIVER_ERROR);

    quiver_blob_metadata_free(md);
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

    quiver_blob_metadata_free(md);
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
    char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");
    quiver_blob_metadata_free_string(unit);

    char* version = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_version(md, &version), QUIVER_OK);
    EXPECT_STREQ(version, "1");
    quiver_blob_metadata_free_string(version);

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

    char* unit2 = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md2, &unit2), QUIVER_OK);
    EXPECT_STREQ(unit2, "MW");
    quiver_blob_metadata_free_string(unit2);

    quiver_blob_metadata_free_string(toml_output);
    quiver_blob_metadata_free(md2);
    quiver_blob_metadata_free(md);
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

    char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");
    quiver_blob_metadata_free_string(unit);

    size_t dim_count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, &dim_count), QUIVER_OK);
    EXPECT_EQ(dim_count, 2u);

    quiver_blob_metadata_free(md);
    quiver_element_destroy(el);
}

// ============================================================================
// Error cases -- NULL arguments
// ============================================================================

TEST(BlobCApiMetadata, NullArgs) {
    EXPECT_EQ(quiver_blob_metadata_create(nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_metadata_from_toml(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_blob_metadata_set_unit(nullptr, "MW"), QUIVER_ERROR);

    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_set_unit(md, nullptr), QUIVER_ERROR);
    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, NullArgsErrorMessages) {
    EXPECT_EQ(quiver_blob_metadata_create(nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_from_toml(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: toml");

    EXPECT_EQ(quiver_blob_metadata_set_unit(nullptr, "MW"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_set_version(nullptr, "1"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_set_initial_datetime(nullptr, "2025-01-01T00:00:00"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_set_labels(nullptr, nullptr, 0), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_add_dimension(nullptr, "x", 10), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(nullptr, "x", 10, "monthly"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_to_toml(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_unit(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_version(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_initial_datetime(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_number_of_time_dimensions(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_labels(nullptr, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");

    EXPECT_EQ(quiver_blob_metadata_get_dimension(nullptr, 0, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
}

TEST(BlobCApiMetadata, NullArgsOnSetters) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_unit(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: unit");

    EXPECT_EQ(quiver_blob_metadata_set_version(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: version");

    EXPECT_EQ(quiver_blob_metadata_set_initial_datetime(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: iso8601");

    EXPECT_EQ(quiver_blob_metadata_set_labels(md, nullptr, 0), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: labels");

    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, nullptr, 10), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: name");

    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, nullptr, 10, "monthly"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: name");

    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, "x", 10, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: frequency");

    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, NullArgsOnGetterOutParams) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_get_unit(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_get_version(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_get_initial_datetime(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_get_number_of_time_dimensions(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_get_labels(md, nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");

    EXPECT_EQ(quiver_blob_metadata_to_toml(md, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out_toml");

    quiver_blob_metadata_free(md);
}

// ============================================================================
// Error cases -- Invalid inputs
// ============================================================================

TEST(BlobCApiMetadata, InvalidToml) {
    quiver_blob_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_blob_metadata_from_toml("not valid toml {{{", &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    std::string err = quiver_get_last_error();
    EXPECT_FALSE(err.empty());
}

TEST(BlobCApiMetadata, InvalidFrequency) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, "bad", 10, "invalid_freq"), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("invalid_freq"), std::string::npos);
    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, InvalidInitialDatetime) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_set_initial_datetime(md, "not-a-date"), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("not-a-date"), std::string::npos);
    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, GetDimensionOutOfRangeErrorMessage) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Dimension index out of range");

    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "x", 5), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 1, &dim), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Dimension index out of range");

    quiver_blob_metadata_free(md);
}

// ============================================================================
// Free function validation
// ============================================================================

TEST(BlobCApiMetadata, FreeStringNull) {
    EXPECT_EQ(quiver_blob_metadata_free_string(nullptr), QUIVER_OK);
}

TEST(BlobCApiMetadata, FreeStringArrayNull) {
    EXPECT_EQ(quiver_blob_metadata_free_string_array(nullptr, 0), QUIVER_OK);
}

TEST(BlobCApiMetadata, FreeStringArrayExplicit) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    const char* labels[] = {"a", "b", "c"};
    EXPECT_EQ(quiver_blob_metadata_set_labels(md, labels, 3), QUIVER_OK);

    char** out_labels = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_labels(md, &out_labels, &count), QUIVER_OK);
    ASSERT_EQ(count, 3u);
    EXPECT_STREQ(out_labels[0], "a");
    EXPECT_STREQ(out_labels[1], "b");
    EXPECT_STREQ(out_labels[2], "c");
    EXPECT_EQ(quiver_blob_metadata_free_string_array(out_labels, count), QUIVER_OK);

    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, FreeDimensionNull) {
    EXPECT_EQ(quiver_blob_metadata_free_dimension(nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: dim");
}

// ============================================================================
// Time dimension -- all frequencies
// ============================================================================

TEST(BlobCApiMetadata, AllTimeFrequencies) {
    struct FreqTest {
        const char* name;
        quiver_time_frequency_t expected;
    };
    FreqTest cases[] = {
        {"yearly", QUIVER_TIME_FREQUENCY_YEARLY},
        {"monthly", QUIVER_TIME_FREQUENCY_MONTHLY},
        {"weekly", QUIVER_TIME_FREQUENCY_WEEKLY},
        {"daily", QUIVER_TIME_FREQUENCY_DAILY},
        {"hourly", QUIVER_TIME_FREQUENCY_HOURLY},
    };

    for (auto& tc : cases) {
        quiver_blob_metadata_t* md = nullptr;
        ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);
        EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, tc.name, 10, tc.name), QUIVER_OK);

        quiver_dimension_t dim = {};
        EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim), QUIVER_OK);
        EXPECT_EQ(dim.is_time_dimension, 1);
        EXPECT_EQ(dim.time_properties.frequency, tc.expected);
        quiver_blob_metadata_free_dimension(&dim);
        quiver_blob_metadata_free(md);
    }
}

// ============================================================================
// Multiple dimensions
// ============================================================================

TEST(BlobCApiMetadata, MultipleDimensions) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "x", 10), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "y", 20), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "z", 30), QUIVER_OK);

    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, &count), QUIVER_OK);
    EXPECT_EQ(count, 3u);

    const char* expected_names[] = {"x", "y", "z"};
    int64_t expected_sizes[] = {10, 20, 30};

    for (size_t i = 0; i < 3; ++i) {
        quiver_dimension_t dim = {};
        EXPECT_EQ(quiver_blob_metadata_get_dimension(md, i, &dim), QUIVER_OK);
        EXPECT_STREQ(dim.name, expected_names[i]);
        EXPECT_EQ(dim.size, expected_sizes[i]);
        EXPECT_EQ(dim.is_time_dimension, 0);
        quiver_blob_metadata_free_dimension(&dim);
    }

    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, MixedDimensionsAndTimeDimensions) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, "stage", 4, "monthly"), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "scenario", 5), QUIVER_OK);

    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md, &count), QUIVER_OK);
    EXPECT_EQ(count, 2u);

    quiver_dimension_t dim0 = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 0, &dim0), QUIVER_OK);
    EXPECT_STREQ(dim0.name, "stage");
    EXPECT_EQ(dim0.is_time_dimension, 1);
    EXPECT_EQ(dim0.time_properties.frequency, QUIVER_TIME_FREQUENCY_MONTHLY);
    quiver_blob_metadata_free_dimension(&dim0);

    quiver_dimension_t dim1 = {};
    EXPECT_EQ(quiver_blob_metadata_get_dimension(md, 1, &dim1), QUIVER_OK);
    EXPECT_STREQ(dim1.name, "scenario");
    EXPECT_EQ(dim1.is_time_dimension, 0);
    quiver_blob_metadata_free_dimension(&dim1);

    quiver_blob_metadata_free(md);
}

// ============================================================================
// TOML round-trip with builders
// ============================================================================

TEST(BlobCApiMetadata, TomlRoundTripFromBuilders) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_set_version(md, "1"), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_set_unit(md, "GWh"), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_set_initial_datetime(md, "2025-06-15T12:00:00"), QUIVER_OK);

    const char* labels[] = {"plant_a", "plant_b", "plant_c"};
    EXPECT_EQ(quiver_blob_metadata_set_labels(md, labels, 3), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_add_time_dimension(md, "stage", 12, "monthly"), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "scenario", 5), QUIVER_OK);

    // Serialize
    char* toml = nullptr;
    EXPECT_EQ(quiver_blob_metadata_to_toml(md, &toml), QUIVER_OK);
    ASSERT_NE(toml, nullptr);

    // Parse back
    quiver_blob_metadata_t* md2 = nullptr;
    ASSERT_EQ(quiver_blob_metadata_from_toml(toml, &md2), QUIVER_OK);

    // Verify fields
    char* unit = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_unit(md2, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "GWh");
    quiver_blob_metadata_free_string(unit);

    char* version = nullptr;
    EXPECT_EQ(quiver_blob_metadata_get_version(md2, &version), QUIVER_OK);
    EXPECT_STREQ(version, "1");
    quiver_blob_metadata_free_string(version);

    size_t dim_count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_dimension_count(md2, &dim_count), QUIVER_OK);
    EXPECT_EQ(dim_count, 2u);

    char** out_labels = nullptr;
    size_t label_count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_labels(md2, &out_labels, &label_count), QUIVER_OK);
    ASSERT_EQ(label_count, 3u);
    EXPECT_STREQ(out_labels[0], "plant_a");
    EXPECT_STREQ(out_labels[1], "plant_b");
    EXPECT_STREQ(out_labels[2], "plant_c");
    quiver_blob_metadata_free_string_array(out_labels, label_count);

    int64_t num_time = 0;
    EXPECT_EQ(quiver_blob_metadata_get_number_of_time_dimensions(md2, &num_time), QUIVER_OK);
    EXPECT_EQ(num_time, 1);

    quiver_blob_metadata_free_string(toml);
    quiver_blob_metadata_free(md2);
    quiver_blob_metadata_free(md);
}

// ============================================================================
// From Element -- error cases
// ============================================================================

TEST(BlobCApiMetadata, FromElementNullArgs) {
    EXPECT_EQ(quiver_blob_metadata_from_element(nullptr, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: el");
}

TEST(BlobCApiMetadata, FromElementMissingRequiredField) {
    quiver_element_t* el = nullptr;
    ASSERT_EQ(quiver_element_create(&el), QUIVER_OK);

    // Only set version, missing dimensions, dimension_sizes, etc.
    quiver_element_set_string(el, "version", "1");

    quiver_blob_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_blob_metadata_from_element(el, &md), QUIVER_ERROR);
    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Cannot from_element"), std::string::npos);

    quiver_element_destroy(el);
}

// ============================================================================
// Labels -- edge cases
// ============================================================================

TEST(BlobCApiMetadata, EmptyLabels) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    const char* labels[] = {""};
    EXPECT_EQ(quiver_blob_metadata_set_labels(md, labels, 0), QUIVER_OK);

    char** out_labels = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_labels(md, &out_labels, &count), QUIVER_OK);
    EXPECT_EQ(count, 0u);

    quiver_blob_metadata_free_string_array(out_labels, count);
    quiver_blob_metadata_free(md);
}

TEST(BlobCApiMetadata, OverwriteLabels) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    const char* labels1[] = {"a", "b"};
    EXPECT_EQ(quiver_blob_metadata_set_labels(md, labels1, 2), QUIVER_OK);

    const char* labels2[] = {"x", "y", "z"};
    EXPECT_EQ(quiver_blob_metadata_set_labels(md, labels2, 3), QUIVER_OK);

    char** out_labels = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_blob_metadata_get_labels(md, &out_labels, &count), QUIVER_OK);
    ASSERT_EQ(count, 3u);
    EXPECT_STREQ(out_labels[0], "x");
    EXPECT_STREQ(out_labels[1], "y");
    EXPECT_STREQ(out_labels[2], "z");
    quiver_blob_metadata_free_string_array(out_labels, count);

    quiver_blob_metadata_free(md);
}

// ============================================================================
// Zero time dimensions
// ============================================================================

TEST(BlobCApiMetadata, ZeroTimeDimensions) {
    quiver_blob_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_blob_metadata_create(&md), QUIVER_OK);

    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "x", 10), QUIVER_OK);
    EXPECT_EQ(quiver_blob_metadata_add_dimension(md, "y", 20), QUIVER_OK);

    int64_t num_time = -1;
    EXPECT_EQ(quiver_blob_metadata_get_number_of_time_dimensions(md, &num_time), QUIVER_OK);
    EXPECT_EQ(num_time, 0);

    quiver_blob_metadata_free(md);
}
