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
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names, col_types, col_data, 2, 3);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

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

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, col_count, row_count);
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
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);
    EXPECT_EQ(col_count, 0);
    EXPECT_EQ(out_col_names, nullptr);
    EXPECT_EQ(out_col_types, nullptr);
    EXPECT_EQ(out_col_data, nullptr);

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
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names1, col_types1, col_data1, 2, 1);
    EXPECT_EQ(err, QUIVER_OK);

    // Replace with new data
    const char* date_times2[] = {"2024-02-01T10:00:00", "2024-02-01T11:00:00"};
    double values2[] = {10.0, 20.0};
    const void* col_data2[] = {date_times2, values2};
    err =
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names1, col_types1, col_data2, 2, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

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

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, col_count, row_count);
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
        quiver_database_update_time_series_group(db, "Collection", "data", id, col_names, col_types, col_data, 2, 1);
    EXPECT_EQ(err, QUIVER_OK);

    // Clear by updating with empty (column_count == 0, row_count == 0)
    err = quiver_database_update_time_series_group(db, "Collection", "data", id, nullptr, nullptr, nullptr, 0, 0);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify empty
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Collection", "data", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

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
    size_t col_count = 0;
    size_t row_count = 0;
    EXPECT_EQ(
        quiver_database_read_time_series_group(
            nullptr, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count),
        QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, nullptr, "data", 1, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", nullptr, 1, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count),
              QUIVER_ERROR);

    // Read null out-params
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, nullptr, &out_col_types, &out_col_data, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, nullptr, &out_col_data, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, nullptr, &col_count, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, nullptr, &row_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_group(
                  db, "Collection", "data", 1, &out_col_names, &out_col_types, &out_col_data, &col_count, nullptr),
              QUIVER_ERROR);

    // Update null args: null db, null collection, null group
    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01T10:00:00"};
    double vals[] = {1.0};
    const void* col_data[] = {dts, vals};
    EXPECT_EQ(quiver_database_update_time_series_group(
                  nullptr, "Collection", "data", 1, col_names, col_types, col_data, 2, 1),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_group(db, nullptr, "data", 1, col_names, col_types, col_data, 2, 1),
              QUIVER_ERROR);
    EXPECT_EQ(
        quiver_database_update_time_series_group(db, "Collection", nullptr, 1, col_names, col_types, col_data, 2, 1),
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
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 4, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(col_count, 4);
    ASSERT_EQ(row_count, 2);

    // Verify columns: dimension first, then value columns in metadata order (alphabetical)
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "humidity");
    EXPECT_STREQ(out_col_names[2], "status");
    EXPECT_STREQ(out_col_names[3], "temperature");

    // Verify types match column order
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);   // date_time
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_INTEGER);  // humidity
    EXPECT_EQ(out_col_types[2], QUIVER_DATA_TYPE_STRING);   // status
    EXPECT_EQ(out_col_types[3], QUIVER_DATA_TYPE_FLOAT);    // temperature

    // Verify data: column 0 (date_time - STRING)
    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_dts[1], "2024-01-01T11:00:00");

    // Column 1 (humidity - INTEGER)
    auto* out_humids = static_cast<int64_t*>(out_col_data[1]);
    EXPECT_EQ(out_humids[0], 65);
    EXPECT_EQ(out_humids[1], 70);

    // Column 2 (status - STRING)
    auto** out_stats = static_cast<char**>(out_col_data[2]);
    EXPECT_STREQ(out_stats[0], "ok");
    EXPECT_STREQ(out_stats[1], "warn");

    // Column 3 (temperature - FLOAT)
    auto* out_temps = static_cast<double*>(out_col_data[3]);
    EXPECT_DOUBLE_EQ(out_temps[0], 20.5);
    EXPECT_DOUBLE_EQ(out_temps[1], 21.0);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, col_count, row_count);
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
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 2, 1);
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
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 4, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back - should return columns in schema order regardless of update order
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(col_count, 4);
    ASSERT_EQ(row_count, 2);

    // Metadata order: dimension first, then value columns alphabetically
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "humidity");
    EXPECT_STREQ(out_col_names[2], "status");
    EXPECT_STREQ(out_col_names[3], "temperature");

    // Verify data is correctly mapped despite out-of-order update
    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_dts[1], "2024-01-01T11:00:00");

    auto* out_humids = static_cast<int64_t*>(out_col_data[1]);
    EXPECT_EQ(out_humids[0], 65);
    EXPECT_EQ(out_humids[1], 70);

    auto** out_stats = static_cast<char**>(out_col_data[2]);
    EXPECT_STREQ(out_stats[0], "ok");
    EXPECT_STREQ(out_stats[1], "warn");

    auto* out_temps = static_cast<double*>(out_col_data[3]);
    EXPECT_DOUBLE_EQ(out_temps[0], 20.5);
    EXPECT_DOUBLE_EQ(out_temps[1], 21.0);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, col_count, row_count);
    quiver_database_close(db);
}

// ============================================================================
// Multi-column validation error tests
// ============================================================================

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
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 2, 1);
    EXPECT_EQ(err, QUIVER_ERROR);

    std::string error_msg = quiver_get_last_error();
    EXPECT_NE(error_msg.find("Cannot update_time_series_group: dimension column 'date_time' missing from column_names"),
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
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 2, 1);
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

    // Pass INTEGER type for temperature (should be FLOAT)
    const char* col_names[] = {"date_time", "temperature"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER};  // wrong type
    const char* dts[] = {"2024-01-01T10:00:00"};
    int64_t temps[] = {20};
    const void* col_data[] = {dts, temps};
    auto err =
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 2, 1);
    EXPECT_EQ(err, QUIVER_ERROR);

    std::string error_msg = quiver_get_last_error();
    EXPECT_NE(error_msg.find("Cannot update_time_series_group: column"), std::string::npos)
        << "Error was: " << error_msg;
    EXPECT_NE(error_msg.find("has type"), std::string::npos) << "Error was: " << error_msg;
    EXPECT_NE(error_msg.find("but received"), std::string::npos) << "Error was: " << error_msg;

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
        quiver_database_update_time_series_group(db, "Sensor", "readings", id, col_names, col_types, col_data, 4, 1);
    EXPECT_EQ(err, QUIVER_OK) << "DATE_TIME and STRING should be interchangeable for dimension column";

    // Read back to confirm
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1);
    ASSERT_EQ(col_count, 4);

    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, col_count, row_count);
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
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(
        db, "Sensor", "readings", id, &out_col_names, &out_col_types, &out_col_data, &col_count, &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(row_count, 0);
    EXPECT_EQ(col_count, 0);
    EXPECT_EQ(out_col_names, nullptr);
    EXPECT_EQ(out_col_types, nullptr);
    EXPECT_EQ(out_col_data, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, FreeTimeSeriesDataNull) {
    // Free with all NULLs and 0 counts - should succeed without crashing
    auto err = quiver_database_free_time_series_data(nullptr, nullptr, nullptr, 0, 0);
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
