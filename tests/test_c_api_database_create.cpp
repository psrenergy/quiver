#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

TEST(DatabaseCApi, CreateElementWithScalars) {
    // Test: Use C API to create element with schema
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Config 1");
    quiver_element_set_integer(element, "integer_attribute", 42);
    quiver_element_set_float(element, "float_attribute", 3.14);

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Configuration", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    ASSERT_NE(config, nullptr);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create Collection with vector
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Item 1");

    int64_t values[] = {1, 2, 3};
    quiver_element_set_array_integer(element, "value_int", values, 3);

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Collection", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNullDb) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Test");

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(nullptr, "Plant", element, &id), QUIVER_ERROR);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(DatabaseCApi, CreateElementNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Test");

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, nullptr, element, &id), QUIVER_ERROR);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNullElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Plant", nullptr, &id), QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithTimeSeries) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create Collection element with time series arrays
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Item 1");

    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"};
    quiver_element_set_array_string(element, "date_time", date_times, 3);

    double values[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(element, "value", values, 3);

    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    // Verify via read_time_series_group (multi-column)
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Collection",
                                                     "data",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_count,
                                                     &out_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_row_count, 3);
    ASSERT_EQ(out_col_count, 2);  // date_time + value

    auto** out_date_times = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_date_times[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_date_times[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_date_times[2], "2024-01-03T10:00:00");
    auto* out_values = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_values[0], 1.5);
    EXPECT_DOUBLE_EQ(out_values[1], 2.5);
    EXPECT_DOUBLE_EQ(out_values[2], 3.5);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithMultiTimeSeries) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create Sensor element with shared date_time routed to both time series tables
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Sensor 1");

    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"};
    quiver_element_set_array_string(element, "date_time", date_times, 3);

    double temps[] = {20.0, 21.5, 22.0};
    quiver_element_set_array_float(element, "temperature", temps, 3);

    double hums[] = {45.0, 50.0, 55.0};
    quiver_element_set_array_float(element, "humidity", hums, 3);

    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Sensor", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    // Verify temperature group (multi-column)
    char** out_temp_col_names = nullptr;
    int* out_temp_col_types = nullptr;
    void** out_temp_col_data = nullptr;
    size_t out_temp_col_count = 0;
    size_t out_temp_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "temperature",
                                                     id,
                                                     &out_temp_col_names,
                                                     &out_temp_col_types,
                                                     &out_temp_col_data,
                                                     &out_temp_col_count,
                                                     &out_temp_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_temp_row_count, 3);
    ASSERT_EQ(out_temp_col_count, 2);  // date_time + temperature
    auto** out_temp_dates = static_cast<char**>(out_temp_col_data[0]);
    EXPECT_STREQ(out_temp_dates[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_temp_dates[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_temp_dates[2], "2024-01-03T10:00:00");
    auto* out_temp_values = static_cast<double*>(out_temp_col_data[1]);
    EXPECT_DOUBLE_EQ(out_temp_values[0], 20.0);
    EXPECT_DOUBLE_EQ(out_temp_values[1], 21.5);
    EXPECT_DOUBLE_EQ(out_temp_values[2], 22.0);
    quiver_database_free_time_series_data(
        out_temp_col_names, out_temp_col_types, out_temp_col_data, out_temp_col_count, out_temp_row_count);

    // Verify humidity group (multi-column)
    char** out_hum_col_names = nullptr;
    int* out_hum_col_types = nullptr;
    void** out_hum_col_data = nullptr;
    size_t out_hum_col_count = 0;
    size_t out_hum_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "humidity",
                                                     id,
                                                     &out_hum_col_names,
                                                     &out_hum_col_types,
                                                     &out_hum_col_data,
                                                     &out_hum_col_count,
                                                     &out_hum_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_hum_row_count, 3);
    ASSERT_EQ(out_hum_col_count, 2);  // date_time + humidity
    auto** out_hum_dates = static_cast<char**>(out_hum_col_data[0]);
    EXPECT_STREQ(out_hum_dates[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_hum_dates[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_hum_dates[2], "2024-01-03T10:00:00");
    auto* out_hum_values = static_cast<double*>(out_hum_col_data[1]);
    EXPECT_DOUBLE_EQ(out_hum_values[0], 45.0);
    EXPECT_DOUBLE_EQ(out_hum_values[1], 50.0);
    EXPECT_DOUBLE_EQ(out_hum_values[2], 55.0);
    quiver_database_free_time_series_data(
        out_hum_col_names, out_hum_col_types, out_hum_col_data, out_hum_col_count, out_hum_row_count);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithDatetime) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Config 1");
    quiver_element_set_string(element, "date_attribute", "2024-03-15T14:30:45");

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Configuration", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    char** out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "date_attribute", &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 1);
    EXPECT_STREQ(out_values[0], "2024-03-15T14:30:45");

    quiver_database_free_string_array(out_values, out_count);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}
