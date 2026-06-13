#include "test_utils.h"

#include <algorithm>
#include <cmath>
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

    quiver_group_metadata_t metadata;
    auto err = quiver_database_get_time_series_metadata(db, "Collection", "data", &metadata);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "data");
    EXPECT_STREQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_column_count, 1);
    EXPECT_STREQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, QUIVER_DATA_TYPE_FLOAT);

    quiver_database_free_group_metadata(&metadata);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroups) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* metadata = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Collection", &metadata, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(metadata[0].group_name, "data");
    EXPECT_STREQ(metadata[0].dimension_column, "date_time");
    EXPECT_EQ(metadata[0].value_column_count, 1);

    quiver_database_free_group_metadata_array(metadata, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroupsEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* metadata = nullptr;
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
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Insert time series data (multi-column: date_time + value)
    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-01T11:00:00", "2024-01-01T12:00:00"};
    double values[] = {1.5, 2.5, 3.5};
    const void* col_data[] = {date_times, values};
    auto err =
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names, col_types, col_data, nullptr, 2, 3);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 3);
    ASSERT_EQ(col_count, 2);  // date_time + value

    // Verify column names in schema order
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "value");

    // Verify column types
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_FLOAT);

    // Column 0: date_time (STRING)
    auto** out_date_times = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_date_times[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_date_times[1], "2024-01-01T11:00:00");
    EXPECT_STREQ(out_date_times[2], "2024-01-01T12:00:00");

    // Column 1: value (FLOAT)
    auto* out_values = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_values[0], 1.5);
    EXPECT_DOUBLE_EQ(out_values[1], 2.5);
    EXPECT_DOUBLE_EQ(out_values[2], 3.5);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
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
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Read without inserting data
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);
    EXPECT_EQ(col_count, 0);
    EXPECT_EQ(out_col_names, nullptr);
    EXPECT_EQ(out_col_types, nullptr);
    EXPECT_EQ(out_col_data, nullptr);
    EXPECT_EQ(out_col_has_value, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesGroupNullString) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("nullable_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Insert a row whose nullable value columns stay NULL (only the dimension column is provided)
    const char* col_names[] = {"date_time"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING};
    const char* date_times[] = {"2024-01-01T10:00:00"};
    const void* col_data[] = {date_times};
    ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, col_names, col_types, col_data, 1),
              QUIVER_OK);

    // NULL cells no longer fail the read: every column comes back with a per-cell
    // presence mask. The placeholder data for a masked-out cell must be ignored.
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1);
    ASSERT_EQ(col_count, 4);  // date_time + temperature + counter + status

    // Schema order: date_time (STRING), temperature (FLOAT), counter (INTEGER), status (STRING)
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "temperature");
    EXPECT_STREQ(out_col_names[2], "counter");
    EXPECT_STREQ(out_col_names[3], "status");

    // Dimension column is always present
    EXPECT_EQ(out_col_has_value[0][0], 1);
    auto** dt = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(dt[0], "2024-01-01T10:00:00");

    // Every value column is NULL: mask 0, with placeholder data that must be ignored
    EXPECT_EQ(out_col_has_value[1][0], 0);  // temperature
    EXPECT_EQ(out_col_has_value[2][0], 0);  // counter
    EXPECT_EQ(out_col_has_value[3][0], 0);  // status
    auto** status = static_cast<char**>(out_col_data[3]);
    EXPECT_EQ(status[0], nullptr);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
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
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Insert initial data (multi-column)
    const char* col_names1[] = {"date_time", "value"};
    int col_types1[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* date_times1[] = {"2024-01-01T10:00:00"};
    double values1[] = {1.0};
    const void* col_data1[] = {date_times1, values1};
    auto err =
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names1, col_types1, col_data1, nullptr, 2, 1);
    EXPECT_EQ(err, QUIVER_OK);

    // Replace with new data
    const char* date_times2[] = {"2024-02-01T10:00:00", "2024-02-01T11:00:00"};
    double values2[] = {10.0, 20.0};
    const void* col_data2[] = {date_times2, values2};
    err =
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names1, col_types1, col_data2, nullptr, 2, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    ASSERT_EQ(col_count, 2);

    // Verify column names and types
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "value");
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_FLOAT);

    // Verify replacement data (old data gone, new data present)
    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-02-01T10:00:00");
    EXPECT_STREQ(out_dts[1], "2024-02-01T11:00:00");
    auto* out_vals = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_vals[0], 10.0);
    EXPECT_DOUBLE_EQ(out_vals[1], 20.0);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
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
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create element
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Insert data (multi-column)
    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* date_times[] = {"2024-01-01T10:00:00"};
    double values[] = {1.0};
    const void* col_data[] = {date_times, values};
    auto err =
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names, col_types, col_data, nullptr, 2, 1);
    EXPECT_EQ(err, QUIVER_OK);

    // Clear by updating with empty (column_count == 0, row_count == 0)
    err = quiver_database_update_time_series_group(
        db, "Collection", "data", id, nullptr, nullptr, nullptr, nullptr, 0, 0);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify empty
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);
    EXPECT_EQ(col_count, 0);

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

    // Metadata null args
    quiver_group_metadata_t metadata;
    EXPECT_EQ(quiver_database_get_time_series_metadata(nullptr, "Collection", "data", &metadata), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, nullptr, "data", &metadata), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, "Collection", nullptr, &metadata), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, "Collection", "data", nullptr), QUIVER_ERROR);

    // List null args
    quiver_group_metadata_t* groups = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_database_list_time_series_groups(nullptr, "Collection", &groups, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_groups(db, nullptr, &groups, &count), QUIVER_ERROR);

    // Read null args: null db, null collection, null group
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    EXPECT_EQ(
        quiver_database_read_time_series_group(
            nullptr, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
        QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, nullptr, "data", 1, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", nullptr, 1, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
              QUIVER_ERROR);

    // Read null out-parameters
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, nullptr, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, nullptr, &out_col_data, &out_col_has_value, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, nullptr, &out_col_has_value, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, nullptr, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, nullptr),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, nullptr, &col_count, &row_count),
              QUIVER_ERROR);

    // Update null args: null db, null collection, null group
    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01T10:00:00"};
    double vals[] = {1.0};
    const void* col_data[] = {dts, vals};
    EXPECT_EQ(quiver_database_update_time_series_group(
                  nullptr, "Collection", "data", 1, col_names, col_types, col_data, nullptr, 2, 1),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_group(db, nullptr, "data", 1, col_names, col_types, col_data, nullptr, 2, 1),
              QUIVER_ERROR);
    EXPECT_EQ(
        quiver_database_update_time_series_group(db, "Collection", nullptr, 1, col_names, col_types, col_data, nullptr, 2, 1),
        QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Multi-column time series tests (mixed_time_series.sql)
// ============================================================================

TEST(DatabaseCApi, ReadTimeSeriesGroupMultiColumn) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create sensor element
    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Insert 4-column time series: date_time, temperature (REAL), humidity (INTEGER), status (TEXT)
    const char* col_names[] = {"date_time", "temperature", "humidity", "status"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01T10:00:00", "2024-01-01T11:00:00"};
    double temps[] = {20.5, 21.0};
    int64_t humids[] = {65, 70};
    const char* stats[] = {"ok", "warn"};
    const void* col_data[] = {dts, temps, humids, stats};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 4, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(col_count, 4);
    ASSERT_EQ(row_count, 2);

    // Verify columns: dimension first, then value columns in schema declaration order
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "temperature");
    EXPECT_STREQ(out_col_names[2], "humidity");
    EXPECT_STREQ(out_col_names[3], "status");

    // Verify types match column order
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);   // date_time
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_FLOAT);    // temperature
    EXPECT_EQ(out_col_types[2], QUIVER_DATA_TYPE_INTEGER);  // humidity
    EXPECT_EQ(out_col_types[3], QUIVER_DATA_TYPE_STRING);   // status

    // Verify data: column 0 (date_time - STRING)
    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_dts[1], "2024-01-01T11:00:00");

    // Column 1 (temperature - FLOAT)
    auto* out_temps = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_temps[0], 20.5);
    EXPECT_DOUBLE_EQ(out_temps[1], 21.0);

    // Column 2 (humidity - INTEGER)
    auto* out_humids = static_cast<int64_t*>(out_col_data[2]);
    EXPECT_EQ(out_humids[0], 65);
    EXPECT_EQ(out_humids[1], 70);

    // Column 3 (status - STRING)
    auto** out_stats = static_cast<char**>(out_col_data[3]);
    EXPECT_STREQ(out_stats[0], "ok");
    EXPECT_STREQ(out_stats[1], "warn");

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupPartialColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Update with only dimension + temperature (omitting humidity and status)
    // The C API accepts partial columns (only dimension is mandatory), but the schema
    // has NOT NULL constraints on omitted columns, so SQLite rejects the insert.
    // This validates that validation passes but schema constraints are enforced.
    const char* col_names[] = {"date_time", "temperature"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01T10:00:00"};
    double temps[] = {22.5};
    const void* col_data[] = {dts, temps};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 2, 1);
    // Fails due to NOT NULL constraint on omitted columns (humidity, status)
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupColumnOrderIndependent) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Pass columns in reversed order: status, humidity, temperature, date_time
    const char* col_names[] = {"status", "humidity", "temperature", "date_time"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_STRING};
    const char* stats[] = {"ok", "warn"};
    int64_t humids[] = {65, 70};
    double temps[] = {20.5, 21.0};
    const char* dts[] = {"2024-01-01T10:00:00", "2024-01-01T11:00:00"};
    const void* col_data[] = {stats, humids, temps, dts};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 4, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back - should return columns in schema order regardless of update order
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(col_count, 4);
    ASSERT_EQ(row_count, 2);

    // Metadata order: dimension first, then value columns in schema declaration order
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "temperature");
    EXPECT_STREQ(out_col_names[2], "humidity");
    EXPECT_STREQ(out_col_names[3], "status");

    // Verify data is correctly mapped despite out-of-order update
    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_dts[1], "2024-01-01T11:00:00");

    auto* out_temps = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_temps[0], 20.5);
    EXPECT_DOUBLE_EQ(out_temps[1], 21.0);

    auto* out_humids = static_cast<int64_t*>(out_col_data[2]);
    EXPECT_EQ(out_humids[0], 65);
    EXPECT_EQ(out_humids[1], 70);

    auto** out_stats = static_cast<char**>(out_col_data[3]);
    EXPECT_STREQ(out_stats[0], "ok");
    EXPECT_STREQ(out_stats[1], "warn");

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

// ============================================================================
// Multi-column validation error tests
// ============================================================================

TEST(DatabaseCApi, UpdateTimeSeriesGroupNullColumnArraysWithCount) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_time_series_group(db,
                                                        "Sensor",
                                                        "readings",
                                                        1,
                                                        /*column_names=*/nullptr,
                                                        /*column_types=*/nullptr,
                                                        /*column_data=*/nullptr,
                                                        /*column_has_value=*/nullptr,
                                                        /*column_count=*/1,
                                                        /*row_count=*/0);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupUnknownColumnType) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    quiver_element_destroy(sensor);

    const char* col_names[] = {"date_time", "temperature"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, 999};  // bogus type
    const char* dts[] = {"2024-01-01T10:00:00"};
    double temps[] = {20.0};
    const void* col_data[] = {dts, temps};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 2, 1);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("unknown column type"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupMissingMultiDimColumn) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // Single row omitting the `block` PK dim — must be rejected before any DELETE.
    const char* col_names[] = {"date_time", "load"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01"};
    double load_buf[] = {1.0};
    const void* col_data[] = {dt_buf, load_buf};
    auto err =
        quiver_database_update_time_series_group(db, "Resource", "load", id, col_names, col_types, col_data, nullptr, 2, 1);
    EXPECT_EQ(err, QUIVER_ERROR);

    std::string error_msg = quiver_get_last_error();
    EXPECT_NE(error_msg.find("Cannot update_time_series_group: row missing required 'block' column"), std::string::npos)
        << "Error was: " << error_msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupMissingDimension) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Try update with only value columns (no date_time dimension)
    const char* col_names[] = {"temperature", "humidity"};
    int col_types[] = {QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};
    double temps[] = {20.5};
    int64_t humids[] = {65};
    const void* col_data[] = {temps, humids};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 2, 1);
    EXPECT_EQ(err, QUIVER_ERROR);

    std::string error_msg = quiver_get_last_error();
    EXPECT_NE(error_msg.find("Cannot update_time_series_group: row missing required 'date_time' column"),
              std::string::npos)
        << "Error was: " << error_msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupUnknownColumn) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Include unknown column "pressure"
    const char* col_names[] = {"date_time", "pressure"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01T10:00:00"};
    double pressures[] = {1013.25};
    const void* col_data[] = {dts, pressures};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 2, 1);
    EXPECT_EQ(err, QUIVER_ERROR);

    std::string error_msg = quiver_get_last_error();
    EXPECT_NE(error_msg.find("Cannot update_time_series_group: column 'pressure' not found in group"),
              std::string::npos)
        << "Error was: " << error_msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupTypeMismatch) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Pass FLOAT type for humidity (should be INTEGER)
    const char* col_names[] = {"date_time", "temperature", "humidity", "status"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING,
                       QUIVER_DATA_TYPE_FLOAT,
                       QUIVER_DATA_TYPE_FLOAT,
                       QUIVER_DATA_TYPE_STRING};  // humidity type is wrong
    const char* dts[] = {"2024-01-01T10:00:00"};
    double temps[] = {20.5};
    double humids[] = {65.5};
    const char* stats[] = {"ok"};
    const void* col_data[] = {dts, temps, humids, stats};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 4, 1);
    EXPECT_EQ(err, QUIVER_ERROR);

    std::string error_msg = quiver_get_last_error();
    EXPECT_NE(error_msg.find("Cannot update_time_series_group: column 'humidity' has type INTEGER but received REAL"),
              std::string::npos)
        << "Error was: " << error_msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupDateTimeStringInterchangeable) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Use DATE_TIME type for the dimension column (instead of STRING)
    const char* col_names[] = {"date_time", "temperature", "humidity", "status"};
    int col_types[] = {
        QUIVER_DATA_TYPE_DATE_TIME, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01T10:00:00"};
    double temps[] = {22.0};
    int64_t humids[] = {55};
    const char* stats[] = {"ok"};
    const void* col_data[] = {dts, temps, humids, stats};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, nullptr, 4, 1);
    EXPECT_EQ(err, QUIVER_OK) << "DATE_TIME and STRING should be interchangeable for dimension column";

    // Read back to confirm
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1);
    ASSERT_EQ(col_count, 4);

    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

// ============================================================================
// add_time_series_row tests (CAPI-11..13)
// ============================================================================

TEST(DatabaseCApi, AddTimeSeriesRowInsert) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01T10:00:00"};
    double val_buf[] = {1.5};
    const void* col_data[] = {dt_buf, val_buf};
    auto err = quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2);
    EXPECT_EQ(err, QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1u);
    ASSERT_EQ(col_count, 2u);

    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "value");
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_FLOAT);

    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");
    auto* out_vals = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_vals[0], 1.5);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowUpsertSamePK) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01T10:00:00"};
    double val_first[] = {1.0};
    const void* col_data_first[] = {dt_buf, val_first};
    auto err =
        quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data_first, 2);
    EXPECT_EQ(err, QUIVER_OK);

    double val_second[] = {99.0};
    const void* col_data_second[] = {dt_buf, val_second};
    err = quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data_second, 2);
    EXPECT_EQ(err, QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1u);  // upsert keyed on (id, date_time) — second call overwrites
    ASSERT_EQ(col_count, 2u);

    auto* out_vals = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_vals[0], 99.0);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowMultiDimInsert) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    const char* col_names[] = {"date_time", "block", "load", "flag"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};

    // Four rows differing in (date_time, block) — each a separate add call.
    struct Row {
        const char* dt;
        int64_t block;
        double load;
        int64_t flag;
    };
    const Row rows_to_insert[] = {
        {"2024-01-01", 1, 10.0, 1},
        {"2024-01-01", 2, 20.0, 1},
        {"2024-01-02", 1, 30.0, 0},
        {"2024-01-02", 2, 40.0, 0},
    };
    for (const auto& r : rows_to_insert) {
        const char* dt_buf[] = {r.dt};
        int64_t block_buf[] = {r.block};
        double load_buf[] = {r.load};
        int64_t flag_buf[] = {r.flag};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4);
        EXPECT_EQ(err, QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Resource", "load", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 4u);

    // Locate each column index (read order is dim_col first, then value columns
    // in schema declaration order).
    int dt_idx = -1, block_idx = -1, load_idx = -1, flag_idx = -1;
    for (size_t c = 0; c < col_count; ++c) {
        std::string n = out_col_names[c];
        if (n == "date_time")
            dt_idx = static_cast<int>(c);
        else if (n == "block")
            block_idx = static_cast<int>(c);
        else if (n == "load")
            load_idx = static_cast<int>(c);
        else if (n == "flag")
            flag_idx = static_cast<int>(c);
    }
    ASSERT_NE(dt_idx, -1);
    ASSERT_NE(block_idx, -1);
    ASSERT_NE(load_idx, -1);
    ASSERT_NE(flag_idx, -1);

    auto** out_dts = static_cast<char**>(out_col_data[dt_idx]);
    auto* out_blocks = static_cast<int64_t*>(out_col_data[block_idx]);
    auto* out_loads = static_cast<double*>(out_col_data[load_idx]);
    auto* out_flags = static_cast<int64_t*>(out_col_data[flag_idx]);

    // Stable sort: read_time_series_group orders by date_time only; sort by
    // (date_time, block) to match the C++ test idiom.
    std::vector<size_t> idx{0, 1, 2, 3};
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        std::string da = out_dts[a];
        std::string db_ = out_dts[b];
        if (da != db_)
            return da < db_;
        return out_blocks[a] < out_blocks[b];
    });

    const Row expected[] = {
        {"2024-01-01", 1, 10.0, 1},
        {"2024-01-01", 2, 20.0, 1},
        {"2024-01-02", 1, 30.0, 0},
        {"2024-01-02", 2, 40.0, 0},
    };
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_STREQ(out_dts[idx[i]], expected[i].dt);
        EXPECT_EQ(out_blocks[idx[i]], expected[i].block);
        EXPECT_DOUBLE_EQ(out_loads[idx[i]], expected[i].load);
        EXPECT_EQ(out_flags[idx[i]], expected[i].flag);
    }

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowMultiDimUpsert) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    const char* col_names[] = {"date_time", "block", "load", "flag"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};

    // Initial insert at (2024-01-01, 1).
    {
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        double load_buf[] = {10.0};
        int64_t flag_buf[] = {1};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4),
                  QUIVER_OK);
    }
    // Upsert same (date_time, block) — must overwrite.
    {
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        double load_buf[] = {99.0};
        int64_t flag_buf[] = {0};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4),
                  QUIVER_OK);
    }
    // Different block at same date_time — must be a new row.
    {
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {2};
        double load_buf[] = {20.0};
        int64_t flag_buf[] = {1};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4),
                  QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Resource", "load", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 2u);  // upsert keyed on full (id, date_time, block)

    int block_idx = -1, load_idx = -1, flag_idx = -1;
    for (size_t c = 0; c < col_count; ++c) {
        std::string n = out_col_names[c];
        if (n == "block")
            block_idx = static_cast<int>(c);
        else if (n == "load")
            load_idx = static_cast<int>(c);
        else if (n == "flag")
            flag_idx = static_cast<int>(c);
    }
    ASSERT_NE(block_idx, -1);
    ASSERT_NE(load_idx, -1);
    ASSERT_NE(flag_idx, -1);

    auto* out_blocks = static_cast<int64_t*>(out_col_data[block_idx]);
    auto* out_loads = static_cast<double*>(out_col_data[load_idx]);
    auto* out_flags = static_cast<int64_t*>(out_col_data[flag_idx]);

    // Sort by block for stable assertions.
    std::vector<size_t> idx{0, 1};
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return out_blocks[a] < out_blocks[b]; });

    // (date_time=2024-01-01, block=1) — overwritten by the second insert.
    EXPECT_EQ(out_blocks[idx[0]], 1);
    EXPECT_DOUBLE_EQ(out_loads[idx[0]], 99.0);
    EXPECT_EQ(out_flags[idx[0]], 0);

    // (date_time=2024-01-01, block=2) — independent new row.
    EXPECT_EQ(out_blocks[idx[1]], 2);
    EXPECT_DOUBLE_EQ(out_loads[idx[1]], 20.0);
    EXPECT_EQ(out_flags[idx[1]], 1);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowPartialValueColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // Row 1: supply (date_time, block, load) — omit `flag`. (column_count = 3)
    {
        const char* col_names[] = {"date_time", "block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT};
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        double load_buf[] = {10.0};
        const void* col_data[] = {dt_buf, block_buf, load_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3),
                  QUIVER_OK);
    }

    // Row 2: supply (date_time, block, flag) — omit `load`. (column_count = 3)
    {
        const char* col_names[] = {"date_time", "block", "flag"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_INTEGER};
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {2};
        int64_t flag_buf[] = {5};
        const void* col_data[] = {dt_buf, block_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3),
                  QUIVER_OK);
    }

    // Verify both rows persisted with the supplied columns intact.
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Resource", "load", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 2u);

    int block_idx = -1, load_idx = -1, flag_idx = -1;
    for (size_t c = 0; c < col_count; ++c) {
        std::string n = out_col_names[c];
        if (n == "block")
            block_idx = static_cast<int>(c);
        else if (n == "load")
            load_idx = static_cast<int>(c);
        else if (n == "flag")
            flag_idx = static_cast<int>(c);
    }
    ASSERT_NE(block_idx, -1);
    ASSERT_NE(load_idx, -1);
    ASSERT_NE(flag_idx, -1);

    auto* out_blocks = static_cast<int64_t*>(out_col_data[block_idx]);
    auto* out_loads = static_cast<double*>(out_col_data[load_idx]);
    auto* out_flags = static_cast<int64_t*>(out_col_data[flag_idx]);

    std::vector<size_t> idx{0, 1};
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return out_blocks[a] < out_blocks[b]; });

    // Row 1 (block=1): supplied load=10.0; omitted flag → NULL (0 via C API read).
    EXPECT_EQ(out_blocks[idx[0]], 1);
    EXPECT_DOUBLE_EQ(out_loads[idx[0]], 10.0);
    EXPECT_EQ(out_flags[idx[0]], 0);

    // Row 2 (block=2): omitted load → NULL (0.0 via C API read); supplied flag=5.
    EXPECT_EQ(out_blocks[idx[1]], 2);
    EXPECT_DOUBLE_EQ(out_loads[idx[1]], 0.0);
    EXPECT_EQ(out_flags[idx[1]], 5);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowMissingDimension) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // a. Omit `block` — only date_time + load supplied.
    {
        const char* col_names[] = {"date_time", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
        const char* dt_buf[] = {"2024-01-01"};
        double load_buf[] = {1.0};
        const void* col_data[] = {dt_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 2);
        EXPECT_EQ(err, QUIVER_ERROR);
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Cannot add_time_series_row: row missing required 'block' column"), std::string::npos)
            << "Actual: " << msg;
    }

    // b. Omit `date_time` — only block + load supplied.
    {
        const char* col_names[] = {"block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT};
        int64_t block_buf[] = {1};
        double load_buf[] = {1.0};
        const void* col_data[] = {block_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 2);
        EXPECT_EQ(err, QUIVER_ERROR);
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Cannot add_time_series_row: row missing required 'date_time' column"), std::string::npos)
            << "Actual: " << msg;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowUnknownColumn) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // Include an unknown column 'pressure' alongside the required dimensions.
    const char* col_names[] = {"date_time", "block", "load", "pressure"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01"};
    int64_t block_buf[] = {1};
    double load_buf[] = {1.0};
    double pressure_buf[] = {1013.25};
    const void* col_data[] = {dt_buf, block_buf, load_buf, pressure_buf};
    auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Cannot add_time_series_row: column 'pressure' not found in group 'load'"), std::string::npos)
        << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowTypeMismatch) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // a. INTEGER passed for REAL column 'load' is accepted (converted on insert).
    {
        const char* col_names[] = {"date_time", "block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_INTEGER};
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        int64_t load_buf[] = {42};
        const void* col_data[] = {dt_buf, block_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3);
        EXPECT_EQ(err, QUIVER_OK);
    }

    // b. FLOAT passed for INTEGER column 'block'.
    {
        const char* col_names[] = {"date_time", "block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
        const char* dt_buf[] = {"2024-01-01"};
        double block_buf[] = {1.5};
        double load_buf[] = {1.0};
        const void* col_data[] = {dt_buf, block_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3);
        EXPECT_EQ(err, QUIVER_ERROR);
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Cannot add_time_series_row: column"), std::string::npos) << "Actual: " << msg;
        EXPECT_NE(msg.find("has type"), std::string::npos) << "Actual: " << msg;
        EXPECT_NE(msg.find("but received"), std::string::npos) << "Actual: " << msg;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowTransactionMatrix) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};

    // ----- Phase A: rollback discards work -----
    EXPECT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    int in_txn = 0;
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_NE(in_txn, 0);

    {
        const char* dt_buf[] = {"2024-01-01T10:00:00"};
        double val_buf[] = {1.0};
        const void* col_data[] = {dt_buf, val_buf};
        auto err = quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2);
        EXPECT_EQ(err, QUIVER_OK);
    }

    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_NE(in_txn, 0);  // nest-aware: still inside the outer txn

    EXPECT_EQ(quiver_database_rollback(db), QUIVER_OK);
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_EQ(in_txn, 0);

    {
        char** out_col_names = nullptr;
        int* out_col_types = nullptr;
        void** out_col_data = nullptr;
        uint8_t** out_col_has_value = nullptr;
        size_t col_count = 0;
        size_t row_count = 0;
        EXPECT_EQ(
            quiver_database_read_time_series_group(
                db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
            QUIVER_OK);
        EXPECT_EQ(row_count, 0u);  // rolled back — nothing persisted
        quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    }

    // ----- Phase B: explicit commit persists batched writes -----
    EXPECT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    {
        const char* dt_buf[] = {"2024-01-01T10:00:00"};
        double val_buf[] = {1.0};
        const void* col_data[] = {dt_buf, val_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2),
                  QUIVER_OK);
    }
    {
        const char* dt_buf[] = {"2024-01-02T10:00:00"};
        double val_buf[] = {2.0};
        const void* col_data[] = {dt_buf, val_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2),
                  QUIVER_OK);
    }
    EXPECT_EQ(quiver_database_commit(db), QUIVER_OK);

    {
        char** out_col_names = nullptr;
        int* out_col_types = nullptr;
        void** out_col_data = nullptr;
        uint8_t** out_col_has_value = nullptr;
        size_t col_count = 0;
        size_t row_count = 0;
        EXPECT_EQ(
            quiver_database_read_time_series_group(
                db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
            QUIVER_OK);
        EXPECT_EQ(row_count, 2u);  // both committed
        quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    }

    // ----- Phase C: standalone autocommit -----
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_EQ(in_txn, 0);
    {
        const char* dt_buf[] = {"2024-01-03T10:00:00"};
        double val_buf[] = {3.0};
        const void* col_data[] = {dt_buf, val_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2),
                  QUIVER_OK);
    }
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_EQ(in_txn, 0);  // returned to autocommit

    {
        char** out_col_names = nullptr;
        int* out_col_types = nullptr;
        void** out_col_data = nullptr;
        uint8_t** out_col_has_value = nullptr;
        size_t col_count = 0;
        size_t row_count = 0;
        EXPECT_EQ(
            quiver_database_read_time_series_group(
                db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count),
            QUIVER_OK);
        EXPECT_EQ(row_count, 3u);  // Phase B's 2 + Phase C's 1
        quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01T10:00:00"};
    double val_buf[] = {1.0};
    const void* col_data[] = {dt_buf, val_buf};

    // a. Null db.
    EXPECT_EQ(quiver_database_add_time_series_row(nullptr, "Collection", "data", 1, col_names, col_types, col_data, 2),
              QUIVER_ERROR);
    {
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;
    }

    // b. Null collection.
    EXPECT_EQ(quiver_database_add_time_series_row(db, nullptr, "data", 1, col_names, col_types, col_data, 2),
              QUIVER_ERROR);
    {
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;
    }

    // c. Null group.
    EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", nullptr, 1, col_names, col_types, col_data, 2),
              QUIVER_ERROR);
    {
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowUnknownColumnType) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    const char* col_names[] = {"date_time", "block", "load"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, 999};  // 999 is bogus
    const char* dt_buf[] = {"2024-01-01"};
    int64_t block_buf[] = {1};
    double load_buf[] = {10.0};
    const void* col_data[] = {dt_buf, block_buf, load_buf};
    auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Cannot add_time_series_row: unknown column type"), std::string::npos) << "Actual: " << msg;
    EXPECT_NE(msg.find("999"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowNullColumnArraysWithCount) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_add_time_series_row(db,
                                                   "Resource",
                                                   "load",
                                                   1,
                                                   /*column_names=*/nullptr,
                                                   /*column_types=*/nullptr,
                                                   /*column_data=*/nullptr,
                                                   /*column_count=*/3);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

// ============================================================================
// Multi-column edge case tests
// ============================================================================

TEST(DatabaseCApi, ReadTimeSeriesGroupMultiColumnEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config + sensor
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    EXPECT_EQ(quiver_element_destroy(sensor), QUIVER_OK);

    // Read without inserting data - should return all NULL/0
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &out_col_has_value, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);
    EXPECT_EQ(col_count, 0);
    EXPECT_EQ(out_col_names, nullptr);
    EXPECT_EQ(out_col_types, nullptr);
    EXPECT_EQ(out_col_data, nullptr);
    EXPECT_EQ(out_col_has_value, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Time series row read tests (read_time_series_row)
// ============================================================================

TEST(DatabaseCApi, ReadTimeSeriesRow) {
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

    // Create two elements
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t id2 = 0;
    quiver_database_create_element(db, "Collection", e2, &id2);
    quiver_element_destroy(e2);

    // Insert time series for both elements
    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};

    const char* dts1[] = {"2024-01-01", "2024-01-02", "2024-01-03"};
    double vals1[] = {1.0, 2.0, 3.0};
    const void* data1[] = {dts1, vals1};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Collection", "data", id1, col_names, col_types, data1, nullptr, 2, 3),
        QUIVER_OK);

    const char* dts2[] = {"2024-01-01", "2024-01-02"};
    double vals2[] = {10.0, 20.0};
    const void* data2[] = {dts2, vals2};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Collection", "data", id2, col_names, col_types, data2, nullptr, 2, 2),
        QUIVER_OK);

    // Read at 2024-01-02
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-02", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);
    ASSERT_EQ(out_count, 2);

    auto* floats = static_cast<double*>(out_values);
    EXPECT_DOUBLE_EQ(floats[0], 2.0);
    EXPECT_DOUBLE_EQ(floats[1], 20.0);

    quiver_database_free_float_array(floats);

    // Read at 2024-01-03: Item 1 -> 3.0, Item 2 -> 20.0 (last at or before)
    err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-03", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(out_count, 2);

    floats = static_cast<double*>(out_values);
    EXPECT_DOUBLE_EQ(floats[0], 3.0);
    EXPECT_DOUBLE_EQ(floats[1], 20.0);

    quiver_database_free_float_array(floats);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowBeforeAllData) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    quiver_element_destroy(e1);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-02"};
    double vals[] = {1.0};
    const void* data[] = {dts, vals};
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Collection", "data", id1, col_names, col_types, data, nullptr, 2, 1),
              QUIVER_OK);

    // Query before any data: value should be NaN (null sentinel for float)
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(out_count, 1);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);

    auto* floats = static_cast<double*>(out_values);
    EXPECT_TRUE(std::isnan(floats[0]));

    quiver_database_free_float_array(floats);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowEmptyCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // No elements in Collection
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 0);
    EXPECT_EQ(out_values, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowMultiColumnInteger) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    quiver_element_destroy(sensor);

    const char* col_names[] = {"date_time", "temperature", "humidity", "status"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {20.5, 21.0};
    int64_t humids[] = {65, 70};
    const char* stats[] = {"ok", "warn"};
    const void* data[] = {dts, temps, humids, stats};
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, data, nullptr, 4, 2),
              QUIVER_OK);

    // Read humidity (INTEGER) at 2024-01-02
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Sensor", "readings", "humidity", "2024-01-02", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_INTEGER);
    ASSERT_EQ(out_count, 1);

    auto* ints = static_cast<int64_t*>(out_values);
    EXPECT_EQ(ints[0], 70);

    quiver_database_free_integer_array(ints);

    // Read status (STRING) at 2024-01-01
    err = quiver_database_read_time_series_row(
        db, "Sensor", "readings", "status", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_STRING);
    ASSERT_EQ(out_count, 1);

    auto** strings = static_cast<char**>(out_values);
    EXPECT_STREQ(strings[0], "ok");

    quiver_database_free_string_array(strings, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;

    EXPECT_EQ(quiver_database_read_time_series_row(
                  nullptr, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, nullptr, "data", "value", "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", nullptr, "value", "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", nullptr, "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", nullptr, &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", "2024-01-01", nullptr, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", "2024-01-01", &out_type, nullptr, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, nullptr),
              QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowAttributeNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "nonexistent", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Time series attribute not found"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowGroupNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "nonexistent", "value", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("not found"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

// ============================================================================
// Time series group NULL-mask tests (nullable_time_series.sql:
// date_time TEXT NOT NULL, temperature REAL, counter INTEGER, status TEXT)
// ============================================================================

namespace {
// Opens an in-memory nullable_time_series.sql database with one Configuration
// and one Sensor element; returns the db and the sensor's id.
quiver_database_t* open_nullable_ts_db(int64_t* out_sensor_id) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("nullable_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    quiver_element_t* config = nullptr;
    quiver_element_create(&config);
    quiver_element_set_string(config, "label", "Config");
    int64_t tmp = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp);
    quiver_element_destroy(config);
    quiver_element_t* sensor = nullptr;
    quiver_element_create(&sensor);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    quiver_database_create_element(db, "Sensor", sensor, out_sensor_id);
    quiver_element_destroy(sensor);
    return db;
}
}  // namespace

TEST(DatabaseCApi, ReadTimeSeriesGroupNullNumerics) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    // Row 1 has temperature only; row 2 has counter only.
    {
        const char* names[] = {"date_time", "temperature"};
        int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
        const char* dts[] = {"2024-01-01"};
        double temps[] = {20.0};
        const void* data[] = {dts, temps};
        ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, names, types, data, 2), QUIVER_OK);
    }
    {
        const char* names[] = {"date_time", "counter"};
        int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER};
        const char* dts[] = {"2024-01-02"};
        int64_t counters[] = {5};
        const void* data[] = {dts, counters};
        ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, names, types, data, 2), QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    ASSERT_EQ(col_count, 4);

    // date_time (col 0) is always present
    EXPECT_EQ(out_col_has_value[0][0], 1);
    EXPECT_EQ(out_col_has_value[0][1], 1);

    // temperature (col 1): present in row 1, NULL in row 2
    auto* temps = static_cast<double*>(out_col_data[1]);
    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_DOUBLE_EQ(temps[0], 20.0);
    EXPECT_EQ(out_col_has_value[1][1], 0);

    // counter (col 2): NULL in row 1, present in row 2
    auto* counters = static_cast<int64_t*>(out_col_data[2]);
    EXPECT_EQ(out_col_has_value[2][0], 0);
    EXPECT_EQ(out_col_has_value[2][1], 1);
    EXPECT_EQ(counters[1], 5);

    // status (col 3): NULL in both rows
    EXPECT_EQ(out_col_has_value[3][0], 0);
    EXPECT_EQ(out_col_has_value[3][1], 0);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesGroupAllNullStringColumn) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    for (const char* dt : {"2024-01-01", "2024-01-02"}) {
        const char* names[] = {"date_time"};
        int types[] = {QUIVER_DATA_TYPE_STRING};
        const char* dts[] = {dt};
        const void* data[] = {dts};
        ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, names, types, data, 1), QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    ASSERT_EQ(col_count, 4);

    // status (col 3) is entirely NULL: all masks 0, all data pointers NULL
    auto** status = static_cast<char**>(out_col_data[3]);
    EXPECT_EQ(out_col_has_value[3][0], 0);
    EXPECT_EQ(out_col_has_value[3][1], 0);
    EXPECT_EQ(status[0], nullptr);
    EXPECT_EQ(status[1], nullptr);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupNullCellsRoundTrip) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature", "counter", "status"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {10.0, 0.0};            // row 2 masked out
    int64_t counters[] = {0, 7};             // row 1 masked out
    const char* statuses[] = {"ok", nullptr};  // row 2 masked out (NULL placeholder)
    const void* data[] = {dts, temps, counters, statuses};
    uint8_t m_dt[] = {1, 1};
    uint8_t m_temp[] = {1, 0};
    uint8_t m_cnt[] = {0, 1};
    uint8_t m_status[] = {1, 0};
    const uint8_t* masks[] = {m_dt, m_temp, m_cnt, m_status};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 4, 2),
        QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);

    auto* out_temps = static_cast<double*>(out_col_data[1]);
    auto* out_cnts = static_cast<int64_t*>(out_col_data[2]);
    auto** out_status = static_cast<char**>(out_col_data[3]);

    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_DOUBLE_EQ(out_temps[0], 10.0);
    EXPECT_EQ(out_col_has_value[1][1], 0);

    EXPECT_EQ(out_col_has_value[2][0], 0);
    EXPECT_EQ(out_col_has_value[2][1], 1);
    EXPECT_EQ(out_cnts[1], 7);

    EXPECT_EQ(out_col_has_value[3][0], 1);
    EXPECT_STREQ(out_status[0], "ok");
    EXPECT_EQ(out_col_has_value[3][1], 0);
    EXPECT_EQ(out_status[1], nullptr);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupNullMaskIsDense) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {1.5, 2.5};
    const void* data[] = {dts, temps};

    // An explicit all-ones mask must behave identically to passing nullptr (dense).
    uint8_t m_dt[] = {1, 1};
    uint8_t m_temp[] = {1, 1};
    const uint8_t* masks[] = {m_dt, m_temp};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 2, 2),
        QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    auto* out_temps = static_cast<double*>(out_col_data[1]);
    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_EQ(out_col_has_value[1][1], 1);
    EXPECT_DOUBLE_EQ(out_temps[0], 1.5);
    EXPECT_DOUBLE_EQ(out_temps[1], 2.5);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupPerColumnNullMask) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature", "counter", "status"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {1.0, 0.0};  // row 2 masked
    int64_t counters[] = {3, 4};
    const char* statuses[] = {"a", "b"};
    const void* data[] = {dts, temps, counters, statuses};
    uint8_t m_temp[] = {1, 0};
    // Only temperature carries a mask; the other columns pass nullptr (dense).
    const uint8_t* masks[] = {nullptr, m_temp, nullptr, nullptr};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 4, 2),
        QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    auto* out_cnts = static_cast<int64_t*>(out_col_data[2]);
    auto** out_status = static_cast<char**>(out_col_data[3]);

    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_EQ(out_col_has_value[1][1], 0);  // masked
    EXPECT_EQ(out_col_has_value[2][0], 1);  // dense
    EXPECT_EQ(out_col_has_value[2][1], 1);
    EXPECT_EQ(out_cnts[0], 3);
    EXPECT_EQ(out_cnts[1], 4);
    EXPECT_EQ(out_col_has_value[3][0], 1);  // dense
    EXPECT_STREQ(out_status[0], "a");
    EXPECT_STREQ(out_status[1], "b");

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupMaskedDimensionFails) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {1.0, 2.0};
    const void* data[] = {dts, temps};
    uint8_t m_dt[] = {1, 0};  // masking a dimension/PK cell -> SQL NULL into a NOT NULL PK column
    uint8_t m_temp[] = {1, 1};
    const uint8_t* masks[] = {m_dt, m_temp};
    EXPECT_EQ(
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 2, 2),
        QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupAllNullColumnFloatTag) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    // counter (INTEGER) and status (TEXT) are tagged FLOAT with all-zero masks: the type tag and
    // data are ignored for masked-out cells, so both columns insert SQL NULL regardless.
    const char* names[] = {"date_time", "counter", "status"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01"};
    double dummy_counter[] = {0.0};
    double dummy_status[] = {0.0};
    const void* data[] = {dts, dummy_counter, dummy_status};
    uint8_t m_dt[] = {1};
    uint8_t m_cnt[] = {0};
    uint8_t m_status[] = {0};
    const uint8_t* masks[] = {m_dt, m_cnt, m_status};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 3, 1),
        QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 1);
    ASSERT_EQ(col_count, 4);
    // counter (col 2, INTEGER) and status (col 3, TEXT) are both NULL
    EXPECT_EQ(out_col_has_value[2][0], 0);
    EXPECT_EQ(out_col_has_value[3][0], 0);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, FreeTimeSeriesDataNull) {
    // Free with all NULLs and 0 counts - should succeed without crashing
    auto err = quiver_database_free_time_series_data(nullptr, nullptr, nullptr, nullptr, 0, 0);
    EXPECT_EQ(err, QUIVER_OK);
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

    quiver_database_free_string_array(columns, count);
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

    quiver_database_free_time_series_files(columns, paths, count);
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

    quiver_database_free_time_series_files(out_columns, out_paths, out_count);
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

    quiver_database_free_time_series_files(out_columns, out_paths, out_count);
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

    quiver_database_free_time_series_files(out_columns, out_paths, out_count);
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
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_list_time_series_files_columns(db, "Configuration", &columns, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

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
    EXPECT_EQ(quiver_database_has_time_series_files(nullptr, "Collection", &result), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_has_time_series_files(db, nullptr, &result), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_has_time_series_files(db, "Collection", nullptr), QUIVER_ERROR);

    char** columns = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_database_list_time_series_files_columns(nullptr, "Collection", &columns, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, nullptr, &columns, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, "Collection", nullptr, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, "Collection", &columns, nullptr), QUIVER_ERROR);

    char** paths = nullptr;
    EXPECT_EQ(quiver_database_read_time_series_files(nullptr, "Collection", &columns, &paths, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, nullptr, &columns, &paths, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", nullptr, &paths, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", &columns, nullptr, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", &columns, &paths, nullptr), QUIVER_ERROR);

    const char* in_columns[] = {"data_file"};
    const char* in_paths[] = {"/path"};
    EXPECT_EQ(quiver_database_update_time_series_files(nullptr, "Collection", in_columns, in_paths, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_files(db, nullptr, in_columns, in_paths, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", nullptr, in_paths, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", in_columns, nullptr, 1), QUIVER_ERROR);

    quiver_database_close(db);
}
