#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Update scalar tests
// ============================================================================

TEST(DatabaseCApi, UpdateScalarInteger) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_integer(e, "integer_attribute", 42);
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "integer_attribute", 100);
    auto err = quiver_database_update_element(db, "Configuration", id, update);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t value;
    int has_value;
    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateScalarFloat) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_float(e, "float_attribute", 3.14);
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_float(update, "float_attribute", 2.71);
    auto err = quiver_database_update_element(db, "Configuration", id, update);    
    EXPECT_EQ(err, QUIVER_OK);

    double value;
    int has_value;
    err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 2.71);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateScalarString) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_string(e, "string_attribute", "hello");
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_string(update, "string_attribute", "world");
    auto err = quiver_database_update_element(db, "Configuration", id, update);    
    EXPECT_EQ(err, QUIVER_OK);    

    char* value = nullptr;
    int has_value;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "world");

    delete[] value;
    quiver_database_close(db);
}

// ============================================================================
// Update vector tests
// ============================================================================

TEST(DatabaseCApi, UpdateVectorIntegers) {
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

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e, "value_int", values1, 3);
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int64_t new_values[] = {10, 20, 30, 40};
    auto err = quiver_database_update_vector_integers(db, "Collection", "value_int", id, new_values, 4);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 4);
    EXPECT_EQ(read_values[0], 10);
    EXPECT_EQ(read_values[1], 20);
    EXPECT_EQ(read_values[2], 30);
    EXPECT_EQ(read_values[3], 40);

    quiver_database_free_integer_array(read_values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorFloats) {
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

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(e, "value_float", values1, 3);
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    double new_values[] = {10.5, 20.5};
    auto err = quiver_database_update_vector_floats(db, "Collection", "value_float", id, new_values, 2);
    EXPECT_EQ(err, QUIVER_OK);

    double* read_values = nullptr;
    size_t count = 0;
    err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(read_values[0], 10.5);
    EXPECT_DOUBLE_EQ(read_values[1], 20.5);

    quiver_database_free_float_array(read_values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorToEmpty) {
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

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e, "value_int", values1, 3);
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    auto err = quiver_database_update_vector_integers(db, "Collection", "value_int", id, nullptr, 0);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Update set tests
// ============================================================================

TEST(DatabaseCApi, UpdateSetStrings) {
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

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    quiver_element_set_array_string(e, "tag", tags, 2);
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* new_tags[] = {"new_tag1", "new_tag2", "new_tag3"};
    auto err = quiver_database_update_set_strings(db, "Collection", "tag", id, new_tags, 3);
    EXPECT_EQ(err, QUIVER_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);

    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(read_values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "new_tag1");
    EXPECT_EQ(set_values[1], "new_tag2");
    EXPECT_EQ(set_values[2], "new_tag3");

    quiver_database_free_string_array(read_values, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetToEmpty) {
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

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    quiver_element_set_array_string(e, "tag", tags, 2);
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    auto err = quiver_database_update_set_strings(db, "Collection", "tag", id, nullptr, 0);
    EXPECT_EQ(err, QUIVER_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// update_element tests
// ============================================================================

TEST(DatabaseCApi, UpdateElementSingleScalar) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_integer(e, "integer_attribute", 42);
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    // Update single scalar attribute
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "integer_attribute", 100);
    auto err = quiver_database_update_element(db, "Configuration", id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t value;
    int has_value;
    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    // Verify label unchanged
    char* label = nullptr;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "label", id, &label, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(label, "Config 1");
    delete[] label;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementMultipleScalars) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_integer(e, "integer_attribute", 42);
    quiver_element_set_float(e, "float_attribute", 3.14);
    quiver_element_set_string(e, "string_attribute", "hello");
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    // Update multiple scalar attributes at once
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "integer_attribute", 100);
    quiver_element_set_float(update, "float_attribute", 2.71);
    quiver_element_set_string(update, "string_attribute", "world");
    auto err = quiver_database_update_element(db, "Configuration", id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t integer_value;
    int has_value;
    err = quiver_database_read_scalar_integer_by_id(
        db, "Configuration", "integer_attribute", id, &integer_value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(integer_value, 100);

    double float_value;
    err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", id, &float_value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(float_value, 2.71);

    char* str_value = nullptr;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", id, &str_value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(str_value, "world");
    delete[] str_value;

    // Verify label unchanged
    char* label = nullptr;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "label", id, &label, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(label, "Config 1");
    delete[] label;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementOtherElementsUnchanged) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = 0;
    quiver_database_create_element(db, "Configuration", e2, &id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    // Update only first element
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "integer_attribute", 999);
    auto err = quiver_database_update_element(db, "Configuration", id1, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t value;
    int has_value;

    // Verify first element changed
    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 999);

    // Verify second element unchanged
    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id2, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementWithTimeSeries) {
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

    // Create element with initial time series
    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    const char* dates1[] = {"2024-01-01T10:00:00", "2024-01-02T10:00:00"};
    quiver_element_set_array_string(e, "date_time", dates1, 2);
    double vals1[] = {1.0, 2.0};
    quiver_element_set_array_float(e, "value", vals1, 2);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    // Update time series via update_element
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* dates2[] = {"2025-06-01T00:00:00", "2025-06-02T00:00:00", "2025-06-03T00:00:00"};
    quiver_element_set_array_string(update, "date_time", dates2, 3);
    double vals2[] = {10.0, 20.0, 30.0};
    quiver_element_set_array_float(update, "value", vals2, 3);
    auto err = quiver_database_update_element(db, "Collection", id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

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
    EXPECT_STREQ(out_date_times[0], "2025-06-01T00:00:00");
    EXPECT_STREQ(out_date_times[1], "2025-06-02T00:00:00");
    EXPECT_STREQ(out_date_times[2], "2025-06-03T00:00:00");
    auto* out_values = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_values[0], 10.0);
    EXPECT_DOUBLE_EQ(out_values[1], 20.0);
    EXPECT_DOUBLE_EQ(out_values[2], 30.0);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_integer(element, "integer_attribute", 42);

    // Null db
    auto err = quiver_database_update_element(nullptr, "Configuration", 1, element);
    EXPECT_EQ(err, QUIVER_ERROR);

    // Null collection
    err = quiver_database_update_element(db, nullptr, 1, element);
    EXPECT_EQ(err, QUIVER_ERROR);

    // Null element
    err = quiver_database_update_element(db, "Configuration", 1, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

// ============================================================================
// Update vector null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateVectorIntegersNullDb) {
    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_vector_integers(nullptr, "Collection", "value_int", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, UpdateVectorIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_vector_integers(db, nullptr, "value_int", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorIntegersNullAttribute) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_vector_integers(db, "Collection", nullptr, 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorFloatsNullDb) {
    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_vector_floats(nullptr, "Collection", "value_float", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, UpdateVectorFloatsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_vector_floats(db, nullptr, "value_float", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorStringsNullDb) {
    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_vector_strings(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, UpdateVectorStringsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_vector_strings(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Update set null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateSetIntegersNullDb) {
    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_set_integers(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, UpdateSetIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_set_integers(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetFloatsNullDb) {
    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_set_floats(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, UpdateSetFloatsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_set_floats(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullDb) {
    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_set_strings(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, UpdateSetStringsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_set_strings(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullAttribute) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_set_strings(db, "Collection", nullptr, 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Whitespace trimming tests
// ============================================================================

TEST(DatabaseCApi, UpdateScalarStringTrimsWhitespace) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_string(e, "string_attribute", "hello");
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_string(update, "string_attribute", "  world  ");
    auto err = quiver_database_update_element(db, "Configuration", id, update);    
    EXPECT_EQ(err, QUIVER_OK);    

    char* value = nullptr;
    int has_value;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "world");

    delete[] value;
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsTrimsWhitespace) {
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

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    quiver_element_set_array_string(e, "tag", tags, 2);
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* new_tags[] = {"  alpha  ", "\tbeta\n", " gamma "};
    auto err = quiver_database_update_set_strings(db, "Collection", "tag", id, new_tags, 3);
    EXPECT_EQ(err, QUIVER_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);

    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(read_values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "alpha");
    EXPECT_EQ(set_values[1], "beta");
    EXPECT_EQ(set_values[2], "gamma");

    quiver_database_free_string_array(read_values, count);
    quiver_database_close(db);
}

// ============================================================================
// DateTime update tests
// ============================================================================

TEST(DatabaseCApi, UpdateDateTimeScalar) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_string(e, "date_attribute", "2024-01-01T00:00:00");
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
    EXPECT_GT(id, 0);

    // Update the datetime value
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_string(update, "date_attribute", "2025-12-31T23:59:59");
    auto err = quiver_database_update_element(db, "Configuration", id, update);    
    EXPECT_EQ(err, QUIVER_OK);        

    // Verify the update
    char* value = nullptr;
    int has_value;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "date_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "2025-12-31T23:59:59");

    delete[] value;
    quiver_database_close(db);
}

// ============================================================================
// Null string element tests
// ============================================================================

TEST(DatabaseCApi, UpdateVectorStringsNullElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", nullptr, "c"};
    auto err = quiver_database_update_vector_strings(db, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", nullptr, "c"};
    auto err = quiver_database_update_set_strings(db, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Update element FK label resolution tests
// ============================================================================

TEST(DatabaseCApi, UpdateElementScalarFkLabel) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create two parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p1), QUIVER_OK);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p2), QUIVER_OK);

    // Create child with parent_id pointing to Parent 1 via string label
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_string(child, "parent_id", "Parent 1");
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Update child: change parent_id to Parent 2 via string label
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_string(update, "parent_id", "Parent 2");
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify: parent_id resolved to Parent 2's ID (2)
    int64_t* parent_ids = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_integers(db, "Child", "parent_id", &parent_ids, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(parent_ids[0], 2);

    quiver_database_free_integer_array(parent_ids);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementScalarFkInteger) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create two parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p1), QUIVER_OK);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p2), QUIVER_OK);

    // Create child with parent_id = 1 (integer)
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_integer(child, "parent_id", 1);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Update child: change parent_id to 2 using integer ID directly
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "parent_id", 2);
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify: parent_id updated to 2
    int64_t* parent_ids = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_integers(db, "Child", "parent_id", &parent_ids, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(parent_ids[0], 2);

    quiver_database_free_integer_array(parent_ids);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementVectorFkLabels) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create two parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p1), QUIVER_OK);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p2), QUIVER_OK);

    // Create child with vector FK pointing to Parent 1
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* init_refs[] = {"Parent 1"};
    quiver_element_set_array_string(child, "parent_ref", init_refs, 1);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Update child: change vector FK to {Parent 2, Parent 1}
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* new_refs[] = {"Parent 2", "Parent 1"};
    quiver_element_set_array_string(update, "parent_ref", new_refs, 2);
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify: vector resolved to {2, 1} (order preserved)
    int64_t* refs = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_vector_integers_by_id(db, "Child", "parent_ref", child_id, &refs, &count),
              QUIVER_OK);
    ASSERT_EQ(count, 2);
    EXPECT_EQ(refs[0], 2);
    EXPECT_EQ(refs[1], 1);

    quiver_database_free_integer_array(refs);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementSetFkLabels) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create two parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p1), QUIVER_OK);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p2), QUIVER_OK);

    // Create child with set FK pointing to Parent 1
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* init_mentors[] = {"Parent 1"};
    quiver_element_set_array_string(child, "mentor_id", init_mentors, 1);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Update child: change set FK to {Parent 2}
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* new_mentors[] = {"Parent 2"};
    quiver_element_set_array_string(update, "mentor_id", new_mentors, 1);
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify: set resolved to {2}
    int64_t* mentors = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_set_integers_by_id(db, "Child", "mentor_id", child_id, &mentors, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(mentors[0], 2);

    quiver_database_free_integer_array(mentors);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementTimeSeriesFkLabels) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create two parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p1), QUIVER_OK);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p2), QUIVER_OK);

    // Create child with time series FK pointing to Parent 1
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* init_dates[] = {"2024-01-01"};
    quiver_element_set_array_string(child, "date_time", init_dates, 1);
    const char* init_sponsors[] = {"Parent 1"};
    quiver_element_set_array_string(child, "sponsor_id", init_sponsors, 1);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Update child: change time series FK to {Parent 2, Parent 1}
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* new_dates[] = {"2024-06-01", "2024-06-02"};
    quiver_element_set_array_string(update, "date_time", new_dates, 2);
    const char* new_sponsors[] = {"Parent 2", "Parent 1"};
    quiver_element_set_array_string(update, "sponsor_id", new_sponsors, 2);
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify via read_time_series_group
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Child",
                                                     "events",
                                                     child_id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_count,
                                                     &out_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_row_count, 2);
    ASSERT_EQ(out_col_count, 2);  // date_time + sponsor_id

    // sponsor_id is col 1 (INTEGER type)
    auto* sponsor_ids = static_cast<int64_t*>(out_col_data[1]);
    EXPECT_EQ(sponsor_ids[0], 2);
    EXPECT_EQ(sponsor_ids[1], 1);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementAllFkTypesInOneCall) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create two parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p1), QUIVER_OK);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(p2), QUIVER_OK);

    // Create child with ALL FK types pointing to Parent 1
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_string(child, "parent_id", "Parent 1");
    const char* init_mentors[] = {"Parent 1"};
    quiver_element_set_array_string(child, "mentor_id", init_mentors, 1);
    const char* init_refs[] = {"Parent 1"};
    quiver_element_set_array_string(child, "parent_ref", init_refs, 1);
    const char* init_dates[] = {"2024-01-01"};
    quiver_element_set_array_string(child, "date_time", init_dates, 1);
    const char* init_sponsors[] = {"Parent 1"};
    quiver_element_set_array_string(child, "sponsor_id", init_sponsors, 1);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Update child: change ALL FK types to Parent 2
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_string(update, "parent_id", "Parent 2");
    const char* new_mentors[] = {"Parent 2"};
    quiver_element_set_array_string(update, "mentor_id", new_mentors, 1);
    const char* new_refs[] = {"Parent 2"};
    quiver_element_set_array_string(update, "parent_ref", new_refs, 1);
    const char* new_dates[] = {"2025-01-01"};
    quiver_element_set_array_string(update, "date_time", new_dates, 1);
    const char* new_sponsors[] = {"Parent 2"};
    quiver_element_set_array_string(update, "sponsor_id", new_sponsors, 1);
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify scalar FK: parent_id == 2
    int64_t* parent_ids = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_integers(db, "Child", "parent_id", &parent_ids, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(parent_ids[0], 2);
    quiver_database_free_integer_array(parent_ids);

    // Verify set FK: mentor_id == {2}
    int64_t* mentors = nullptr;
    size_t mentor_count = 0;
    ASSERT_EQ(quiver_database_read_set_integers_by_id(db, "Child", "mentor_id", child_id, &mentors, &mentor_count),
              QUIVER_OK);
    ASSERT_EQ(mentor_count, 1);
    EXPECT_EQ(mentors[0], 2);
    quiver_database_free_integer_array(mentors);

    // Verify vector FK: parent_ref == {2}
    int64_t* refs = nullptr;
    size_t ref_count = 0;
    ASSERT_EQ(quiver_database_read_vector_integers_by_id(db, "Child", "parent_ref", child_id, &refs, &ref_count),
              QUIVER_OK);
    ASSERT_EQ(ref_count, 1);
    EXPECT_EQ(refs[0], 2);
    quiver_database_free_integer_array(refs);

    // Verify time series FK: sponsor_id == {2}
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Child",
                                                     "events",
                                                     child_id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_count,
                                                     &out_row_count),
              QUIVER_OK);
    ASSERT_EQ(out_row_count, 1);
    ASSERT_EQ(out_col_count, 2);
    auto* sponsor_ids = static_cast<int64_t*>(out_col_data[1]);
    EXPECT_EQ(sponsor_ids[0], 2);
    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementNoFkColumnsUnchanged) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create element with all scalar types
    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_integer(e, "integer_attribute", 42);
    quiver_element_set_float(e, "float_attribute", 3.14);
    quiver_element_set_string(e, "string_attribute", "hello");
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", e, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    // Update via update_element (FK pre-resolve pass should be a no-op for non-FK schemas)
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "integer_attribute", 100);
    quiver_element_set_float(update, "float_attribute", 2.71);
    quiver_element_set_string(update, "string_attribute", "world");
    auto err = quiver_database_update_element(db, "Configuration", id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify integer updated
    int64_t int_val;
    int has_value;
    ASSERT_EQ(
        quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id, &int_val, &has_value),
        QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(int_val, 100);

    // Verify float updated
    double float_val;
    ASSERT_EQ(
        quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", id, &float_val, &has_value),
        QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(float_val, 2.71);

    // Verify string updated
    char* str_val = nullptr;
    ASSERT_EQ(
        quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", id, &str_val, &has_value),
        QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(str_val, "world");
    delete[] str_val;

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateElementFkResolutionFailurePreservesExisting) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create parent and child with parent_id pointing to Parent 1
    quiver_element_t* parent = nullptr;
    ASSERT_EQ(quiver_element_create(&parent), QUIVER_OK);
    quiver_element_set_string(parent, "label", "Parent 1");
    int64_t parent_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", parent, &parent_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(parent), QUIVER_OK);

    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_string(child, "parent_id", "Parent 1");
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Attempt update with nonexistent parent label
    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_string(update, "parent_id", "Nonexistent Parent");
    auto err = quiver_database_update_element(db, "Child", child_id, update);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);
    EXPECT_EQ(err, QUIVER_ERROR);

    // Verify: original value preserved (parent_id still points to Parent 1's ID)
    int64_t* parent_ids = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_integers(db, "Child", "parent_id", &parent_ids, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(parent_ids[0], 1);

    quiver_database_free_integer_array(parent_ids);
    quiver_database_close(db);
}

// ============================================================================
// Gap-fill: Update vector strings (using all_types.sql)
// ============================================================================

TEST(DatabaseCApi, UpdateVectorStringsHappyPath) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("all_types.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "AllTypes", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* values[] = {"alpha", "beta"};
    auto err = quiver_database_update_vector_strings(db, "AllTypes", "label_value", id, values, 2);
    EXPECT_EQ(err, QUIVER_OK);

    char** read_values = nullptr;
    size_t read_count = 0;
    err = quiver_database_read_vector_strings_by_id(db, "AllTypes", "label_value", id, &read_values, &read_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(read_count, 2);
    EXPECT_STREQ(read_values[0], "alpha");
    EXPECT_STREQ(read_values[1], "beta");

    quiver_database_free_string_array(read_values, read_count);
    quiver_database_close(db);
}

// ============================================================================
// Gap-fill: Update set integers (using all_types.sql)
// ============================================================================

TEST(DatabaseCApi, UpdateSetIntegersHappyPath) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("all_types.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "AllTypes", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int64_t int_values[] = {10, 20, 30};
    auto err = quiver_database_update_set_integers(db, "AllTypes", "code", id, int_values, 3);
    EXPECT_EQ(err, QUIVER_OK);

    int64_t* read_values = nullptr;
    size_t read_count = 0;
    err = quiver_database_read_set_integers_by_id(db, "AllTypes", "code", id, &read_values, &read_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(read_count, 3);

    std::vector<int64_t> sorted(read_values, read_values + read_count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted[0], 10);
    EXPECT_EQ(sorted[1], 20);
    EXPECT_EQ(sorted[2], 30);

    quiver_database_free_integer_array(read_values);
    quiver_database_close(db);
}

// ============================================================================
// Gap-fill: Update set floats (using all_types.sql)
// ============================================================================

TEST(DatabaseCApi, UpdateSetFloatsHappyPath) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("all_types.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "AllTypes", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    double float_values[] = {1.1, 2.2};
    auto err = quiver_database_update_set_floats(db, "AllTypes", "weight", id, float_values, 2);
    EXPECT_EQ(err, QUIVER_OK);

    double* read_values = nullptr;
    size_t read_count = 0;
    err = quiver_database_read_set_floats_by_id(db, "AllTypes", "weight", id, &read_values, &read_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(read_count, 2);

    std::vector<double> sorted(read_values, read_values + read_count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_DOUBLE_EQ(sorted[0], 1.1);
    EXPECT_DOUBLE_EQ(sorted[1], 2.2);

    quiver_database_free_float_array(read_values);
    quiver_database_close(db);
}
