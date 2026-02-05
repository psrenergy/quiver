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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t metadata;
    auto err = quiver_database_get_time_series_metadata(db, "Collection", "data", &metadata);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "data");
    EXPECT_STREQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_column_count, 1);
    EXPECT_STREQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, QUIVER_DATA_TYPE_FLOAT);

    quiver_free_time_series_metadata(&metadata);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroups) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t* metadata = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Collection", &metadata, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(metadata[0].group_name, "data");
    EXPECT_STREQ(metadata[0].dimension_column, "date_time");
    EXPECT_EQ(metadata[0].value_column_count, 1);

    quiver_free_time_series_metadata_array(metadata, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroupsEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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

// ============================================================================
// Time series files tests
// ============================================================================

TEST(DatabaseCApi, HasTimeSeriesFiles) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int result = 0;
    auto err = quiver_database_has_time_series_files(db, "Collection", &result);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(result, 1);

    err = quiver_database_has_time_series_files(db, "Configuration", &result);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(result, 0);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesFilesColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** columns = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_files_columns(db, "Collection", &columns, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    // Check both columns exist (order may vary)
    bool found_data_file = false;
    bool found_metadata_file = false;
    for (size_t i = 0; i < count; ++i) {
        if (std::string(columns[i]) == "data_file")
            found_data_file = true;
        if (std::string(columns[i]) == "metadata_file")
            found_metadata_file = true;
    }
    EXPECT_TRUE(found_data_file);
    EXPECT_TRUE(found_metadata_file);

    quiver_free_string_array(columns, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesFilesEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** columns = nullptr;
    char** paths = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_time_series_files(db, "Collection", &columns, &paths, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    // All paths should be null (no data yet)
    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(paths[i], nullptr);
    }

    quiver_free_time_series_files(columns, paths, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateAndReadTimeSeriesFiles) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* columns[] = {"data_file", "metadata_file"};
    const char* paths[] = {"/path/to/data.csv", "/path/to/meta.json"};
    auto err = quiver_database_update_time_series_files(db, "Collection", columns, paths, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_columns = nullptr;
    char** out_paths = nullptr;
    size_t out_count = 0;
    err = quiver_database_read_time_series_files(db, "Collection", &out_columns, &out_paths, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 2);

    // Find and verify each column
    for (size_t i = 0; i < out_count; ++i) {
        if (std::string(out_columns[i]) == "data_file") {
            EXPECT_STREQ(out_paths[i], "/path/to/data.csv");
        } else if (std::string(out_columns[i]) == "metadata_file") {
            EXPECT_STREQ(out_paths[i], "/path/to/meta.json");
        }
    }

    quiver_free_time_series_files(out_columns, out_paths, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesFilesWithNulls) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* columns[] = {"data_file", "metadata_file"};
    const char* paths[] = {"/path/to/data.csv", nullptr};
    auto err = quiver_database_update_time_series_files(db, "Collection", columns, paths, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_columns = nullptr;
    char** out_paths = nullptr;
    size_t out_count = 0;
    err = quiver_database_read_time_series_files(db, "Collection", &out_columns, &out_paths, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 2);

    // Find and verify each column
    for (size_t i = 0; i < out_count; ++i) {
        if (std::string(out_columns[i]) == "data_file") {
            EXPECT_STREQ(out_paths[i], "/path/to/data.csv");
        } else if (std::string(out_columns[i]) == "metadata_file") {
            EXPECT_EQ(out_paths[i], nullptr);
        }
    }

    quiver_free_time_series_files(out_columns, out_paths, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesFilesReplace) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // First update
    const char* columns1[] = {"data_file", "metadata_file"};
    const char* paths1[] = {"/old/data.csv", "/old/meta.json"};
    auto err = quiver_database_update_time_series_files(db, "Collection", columns1, paths1, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Second update replaces
    const char* columns2[] = {"data_file", "metadata_file"};
    const char* paths2[] = {"/new/data.csv", "/new/meta.json"};
    err = quiver_database_update_time_series_files(db, "Collection", columns2, paths2, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_columns = nullptr;
    char** out_paths = nullptr;
    size_t out_count = 0;
    err = quiver_database_read_time_series_files(db, "Collection", &out_columns, &out_paths, &out_count);
    EXPECT_EQ(err, QUIVER_OK);

    for (size_t i = 0; i < out_count; ++i) {
        if (std::string(out_columns[i]) == "data_file") {
            EXPECT_STREQ(out_paths[i], "/new/data.csv");
        } else if (std::string(out_columns[i]) == "metadata_file") {
            EXPECT_STREQ(out_paths[i], "/new/meta.json");
        }
    }

    quiver_free_time_series_files(out_columns, out_paths, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, TimeSeriesFilesNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** columns = nullptr;
    char** paths = nullptr;
    size_t count = 0;

    // Configuration has no time series files table
    auto err = quiver_database_read_time_series_files(db, "Configuration", &columns, &paths, &count);
    EXPECT_EQ(err, QUIVER_ERROR_DATABASE);

    err = quiver_database_list_time_series_files_columns(db, "Configuration", &columns, &count);
    EXPECT_EQ(err, QUIVER_ERROR_DATABASE);

    quiver_database_close(db);
}

TEST(DatabaseCApi, TimeSeriesFilesNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int result = 0;
    EXPECT_EQ(quiver_database_has_time_series_files(nullptr, "Collection", &result), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_has_time_series_files(db, nullptr, &result), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_has_time_series_files(db, "Collection", nullptr), QUIVER_ERROR_INVALID_ARGUMENT);

    char** columns = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_database_list_time_series_files_columns(nullptr, "Collection", &columns, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, nullptr, &columns, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, "Collection", nullptr, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, "Collection", &columns, nullptr),
              QUIVER_ERROR_INVALID_ARGUMENT);

    char** paths = nullptr;
    EXPECT_EQ(quiver_database_read_time_series_files(nullptr, "Collection", &columns, &paths, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_read_time_series_files(db, nullptr, &columns, &paths, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", nullptr, &paths, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", &columns, nullptr, &count),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", &columns, &paths, nullptr),
              QUIVER_ERROR_INVALID_ARGUMENT);

    const char* in_columns[] = {"data_file"};
    const char* in_paths[] = {"/path"};
    EXPECT_EQ(quiver_database_update_time_series_files(nullptr, "Collection", in_columns, in_paths, 1),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_update_time_series_files(db, nullptr, in_columns, in_paths, 1),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", nullptr, in_paths, 1),
              QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", in_columns, nullptr, 1),
              QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}
