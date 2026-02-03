#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Time series metadata tests
// ============================================================================

TEST(DatabaseCApi, GetTimeSeriesMetadata) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t metadata;
    auto err = quiver_database_get_time_series_metadata(db, "Collection", "data", &metadata);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "data");
    EXPECT_EQ(metadata.value_column_count, 1);
    EXPECT_STREQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, QUIVER_DATA_TYPE_FLOAT);

    quiver_free_time_series_metadata(&metadata);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroups) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t* metadata = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Collection", &metadata, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(metadata[0].group_name, "data");
    EXPECT_EQ(metadata[0].value_column_count, 1);

    quiver_free_time_series_metadata_array(metadata, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroupsEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t* metadata = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Configuration", &metadata, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(metadata, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Time series read tests
// ============================================================================

TEST(DatabaseCApi, ReadTimeSeriesGroupById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create config
    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Create element
    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    auto id = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    // Insert time series data
    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-01T11:00:00", "2024-01-01T12:00:00"};
    double values[] = {1.5, 2.5, 3.5};
    auto err = quiver_database_update_time_series_group(db, "Collection", "data", id, date_times, values, 3);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_date_times = nullptr;
    double* out_values = nullptr;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group_by_id(
        db, "Collection", "data", id, &out_date_times, &out_values, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 3);
    EXPECT_STREQ(out_date_times[0], "2024-01-01T10:00:00");
    EXPECT_DOUBLE_EQ(out_values[0], 1.5);
    EXPECT_STREQ(out_date_times[1], "2024-01-01T11:00:00");
    EXPECT_DOUBLE_EQ(out_values[1], 2.5);
    EXPECT_STREQ(out_date_times[2], "2024-01-01T12:00:00");
    EXPECT_DOUBLE_EQ(out_values[2], 3.5);

    quiver_free_time_series_data(out_date_times, out_values, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesGroupByIdEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create config
    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Create element
    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    auto id = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    // Read without inserting data
    char** out_date_times = nullptr;
    double* out_values = nullptr;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group_by_id(
        db, "Collection", "data", id, &out_date_times, &out_values, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);
    EXPECT_EQ(out_date_times, nullptr);
    EXPECT_EQ(out_values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Time series update tests
// ============================================================================

TEST(DatabaseCApi, UpdateTimeSeriesGroup) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create config
    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Create element
    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    auto id = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    // Insert initial data
    const char* date_times1[] = {"2024-01-01T10:00:00"};
    double values1[] = {1.0};
    auto err = quiver_database_update_time_series_group(db, "Collection", "data", id, date_times1, values1, 1);
    EXPECT_EQ(err, QUIVER_OK);

    // Replace with new data
    const char* date_times2[] = {"2024-02-01T10:00:00", "2024-02-01T11:00:00"};
    double values2[] = {10.0, 20.0};
    err = quiver_database_update_time_series_group(db, "Collection", "data", id, date_times2, values2, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_date_times = nullptr;
    double* out_values = nullptr;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group_by_id(
        db, "Collection", "data", id, &out_date_times, &out_values, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    EXPECT_STREQ(out_date_times[0], "2024-02-01T10:00:00");
    EXPECT_DOUBLE_EQ(out_values[0], 10.0);

    quiver_free_time_series_data(out_date_times, out_values, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupClear) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create config
    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Create element
    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    auto id = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    // Insert data
    const char* date_times[] = {"2024-01-01T10:00:00"};
    double values[] = {1.0};
    auto err = quiver_database_update_time_series_group(db, "Collection", "data", id, date_times, values, 1);
    EXPECT_EQ(err, QUIVER_OK);

    // Clear by updating with empty
    err = quiver_database_update_time_series_group(db, "Collection", "data", id, nullptr, nullptr, 0);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify empty
    char** out_date_times = nullptr;
    double* out_values = nullptr;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group_by_id(
        db, "Collection", "data", id, &out_date_times, &out_values, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);

    quiver_database_close(db);
}

// ============================================================================
// Time series error handling tests
// ============================================================================

TEST(DatabaseCApi, TimeSeriesNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t metadata;
    EXPECT_EQ(quiver_database_get_time_series_metadata(nullptr, "Collection", "data", &metadata),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, nullptr, "data", &metadata), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, "Collection", nullptr, &metadata),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, "Collection", "data", nullptr),
              QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_time_series_metadata_t* groups = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_database_list_time_series_groups(nullptr, "Collection", &groups, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_list_time_series_groups(db, nullptr, &groups, &count), QUIVER_ERROR_INVALID_ARGUMENT);

    char** out_date_times = nullptr;
    double* out_values = nullptr;
    size_t row_count = 0;
    EXPECT_EQ(quiver_database_read_time_series_group_by_id(
                  nullptr, "Collection", "data", 1, &out_date_times, &out_values, &row_count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(
        quiver_database_read_time_series_group_by_id(db, nullptr, "data", 1, &out_date_times, &out_values, &row_count),
        QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}
