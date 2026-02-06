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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

    auto err = quiver_database_update_scalar_integer(db, "Configuration", "integer_attribute", id, 100);
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

    auto err = quiver_database_update_scalar_float(db, "Configuration", "float_attribute", id, 2.71);
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

    auto err = quiver_database_update_scalar_string(db, "Configuration", "string_attribute", id, "world");
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

    quiver_free_integer_array(read_values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorFloats) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

    quiver_free_float_array(read_values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorToEmpty) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

    quiver_free_string_array(read_values, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetToEmpty) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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

TEST(DatabaseCApi, UpdateElementNullArguments) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_integer(element, "integer_attribute", 42);

    // Null db
    auto err = quiver_database_update_element(nullptr, "Configuration", 1, element);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    // Null collection
    err = quiver_database_update_element(db, nullptr, 1, element);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    // Null element
    err = quiver_database_update_element(db, "Configuration", 1, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

// ============================================================================
// Update scalar null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateScalarIntegerNullDb) {
    auto err = quiver_database_update_scalar_integer(nullptr, "Configuration", "integer_attribute", 1, 42);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateScalarIntegerNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_scalar_integer(db, nullptr, "integer_attribute", 1, 42);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateScalarIntegerNullAttribute) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_scalar_integer(db, "Configuration", nullptr, 1, 42);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateScalarFloatNullDb) {
    auto err = quiver_database_update_scalar_float(nullptr, "Configuration", "float_attribute", 1, 3.14);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateScalarFloatNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_scalar_float(db, nullptr, "float_attribute", 1, 3.14);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateScalarStringNullDb) {
    auto err = quiver_database_update_scalar_string(nullptr, "Configuration", "string_attribute", 1, "test");
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateScalarStringNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_scalar_string(db, nullptr, "string_attribute", 1, "test");
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateScalarStringNullValue) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_scalar_string(db, "Configuration", "string_attribute", 1, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Update vector null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateVectorIntegersNullDb) {
    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_vector_integers(nullptr, "Collection", "value_int", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateVectorIntegersNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_vector_integers(db, nullptr, "value_int", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorIntegersNullAttribute) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_vector_integers(db, "Collection", nullptr, 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorFloatsNullDb) {
    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_vector_floats(nullptr, "Collection", "value_float", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateVectorFloatsNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_vector_floats(db, nullptr, "value_float", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateVectorStringsNullDb) {
    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_vector_strings(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateVectorStringsNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_vector_strings(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Update set null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateSetIntegersNullDb) {
    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_set_integers(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateSetIntegersNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = quiver_database_update_set_integers(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetFloatsNullDb) {
    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_set_floats(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateSetFloatsNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double values[] = {1.0, 2.0, 3.0};
    auto err = quiver_database_update_set_floats(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullDb) {
    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_set_strings(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateSetStringsNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_set_strings(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullAttribute) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = quiver_database_update_set_strings(db, "Collection", nullptr, 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// DateTime update tests
// ============================================================================

TEST(DatabaseCApi, UpdateDateTimeScalar) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
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
    auto err = quiver_database_update_scalar_string(db, "Configuration", "date_attribute", id, "2025-12-31T23:59:59");
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
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", nullptr, "c"};
    auto err = quiver_database_update_vector_strings(db, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullElement) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", nullptr, "c"};
    auto err = quiver_database_update_set_strings(db, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}
