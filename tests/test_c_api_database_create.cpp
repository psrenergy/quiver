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

    // Verify via read_time_series_group
    char** out_date_times = nullptr;
    double* out_values = nullptr;
    size_t out_count = 0;
    ASSERT_EQ(
        quiver_database_read_time_series_group(db, "Collection", "data", id, &out_date_times, &out_values, &out_count),
        QUIVER_OK);
    EXPECT_EQ(out_count, 3);
    EXPECT_STREQ(out_date_times[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_date_times[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_date_times[2], "2024-01-03T10:00:00");
    EXPECT_DOUBLE_EQ(out_values[0], 1.5);
    EXPECT_DOUBLE_EQ(out_values[1], 2.5);
    EXPECT_DOUBLE_EQ(out_values[2], 3.5);

    quiver_database_free_string_array(out_date_times, out_count);
    delete[] out_values;
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
