#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <margaux/c/database.h>
#include <margaux/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Update scalar tests
// ============================================================================

TEST(DatabaseCApi, UpdateScalarInteger) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = element_create();
    element_set_string(e, "label", "Config 1");
    element_set_integer(e, "integer_attribute", 42);
    int64_t id = database_create_element(db, "Configuration", e);
    element_destroy(e);

    auto err = database_update_scalar_integer(db, "Configuration", "integer_attribute", id, 100);
    EXPECT_EQ(err, MARGAUX_OK);

    int64_t value;
    int has_value;
    err = database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    database_close(db);
}

TEST(DatabaseCApi, UpdateScalarFloat) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = element_create();
    element_set_string(e, "label", "Config 1");
    element_set_float(e, "float_attribute", 3.14);
    int64_t id = database_create_element(db, "Configuration", e);
    element_destroy(e);

    auto err = database_update_scalar_float(db, "Configuration", "float_attribute", id, 2.71);
    EXPECT_EQ(err, MARGAUX_OK);

    double value;
    int has_value;
    err = database_read_scalar_floats_by_id(db, "Configuration", "float_attribute", id, &value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 2.71);

    database_close(db);
}

TEST(DatabaseCApi, UpdateScalarString) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = element_create();
    element_set_string(e, "label", "Config 1");
    element_set_string(e, "string_attribute", "hello");
    int64_t id = database_create_element(db, "Configuration", e);
    element_destroy(e);

    auto err = database_update_scalar_string(db, "Configuration", "string_attribute", id, "world");
    EXPECT_EQ(err, MARGAUX_OK);

    char* value = nullptr;
    int has_value;
    err = database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id, &value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "world");

    delete[] value;
    database_close(db);
}

// ============================================================================
// Update vector tests
// ============================================================================

TEST(DatabaseCApi, UpdateVectorIntegers) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = element_create();
    element_set_string(config, "label", "Test Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    auto e = element_create();
    element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    element_set_array_integer(e, "value_int", values1, 3);
    int64_t id = database_create_element(db, "Collection", e);
    element_destroy(e);

    int64_t new_values[] = {10, 20, 30, 40};
    auto err = database_update_vector_integers(db, "Collection", "value_int", id, new_values, 4);
    EXPECT_EQ(err, MARGAUX_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 4);
    EXPECT_EQ(read_values[0], 10);
    EXPECT_EQ(read_values[1], 20);
    EXPECT_EQ(read_values[2], 30);
    EXPECT_EQ(read_values[3], 40);

    psr_free_integer_array(read_values);
    database_close(db);
}

TEST(DatabaseCApi, UpdateVectorFloats) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = element_create();
    element_set_string(config, "label", "Test Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    auto e = element_create();
    element_set_string(e, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    element_set_array_float(e, "value_float", values1, 3);
    int64_t id = database_create_element(db, "Collection", e);
    element_destroy(e);

    double new_values[] = {10.5, 20.5};
    auto err = database_update_vector_floats(db, "Collection", "value_float", id, new_values, 2);
    EXPECT_EQ(err, MARGAUX_OK);

    double* read_values = nullptr;
    size_t count = 0;
    err = database_read_vector_floats_by_id(db, "Collection", "value_float", id, &read_values, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(read_values[0], 10.5);
    EXPECT_DOUBLE_EQ(read_values[1], 20.5);

    psr_free_float_array(read_values);
    database_close(db);
}

TEST(DatabaseCApi, UpdateVectorToEmpty) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = element_create();
    element_set_string(config, "label", "Test Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    auto e = element_create();
    element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    element_set_array_integer(e, "value_int", values1, 3);
    int64_t id = database_create_element(db, "Collection", e);
    element_destroy(e);

    auto err = database_update_vector_integers(db, "Collection", "value_int", id, nullptr, 0);
    EXPECT_EQ(err, MARGAUX_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    database_close(db);
}

// ============================================================================
// Update set tests
// ============================================================================

TEST(DatabaseCApi, UpdateSetStrings) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = element_create();
    element_set_string(config, "label", "Test Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    auto e = element_create();
    element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    element_set_array_string(e, "tag", tags, 2);
    int64_t id = database_create_element(db, "Collection", e);
    element_destroy(e);

    const char* new_tags[] = {"new_tag1", "new_tag2", "new_tag3"};
    auto err = database_update_set_strings(db, "Collection", "tag", id, new_tags, 3);
    EXPECT_EQ(err, MARGAUX_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 3);

    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(read_values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "new_tag1");
    EXPECT_EQ(set_values[1], "new_tag2");
    EXPECT_EQ(set_values[2], "new_tag3");

    psr_free_string_array(read_values, count);
    database_close(db);
}

TEST(DatabaseCApi, UpdateSetToEmpty) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = element_create();
    element_set_string(config, "label", "Test Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    auto e = element_create();
    element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    element_set_array_string(e, "tag", tags, 2);
    int64_t id = database_create_element(db, "Collection", e);
    element_destroy(e);

    auto err = database_update_set_strings(db, "Collection", "tag", id, nullptr, 0);
    EXPECT_EQ(err, MARGAUX_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    database_close(db);
}

// ============================================================================
// update_element tests
// ============================================================================

TEST(DatabaseCApi, UpdateElementSingleScalar) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = element_create();
    element_set_string(e, "label", "Config 1");
    element_set_integer(e, "integer_attribute", 42);
    int64_t id = database_create_element(db, "Configuration", e);
    element_destroy(e);

    // Update single scalar attribute
    auto update = element_create();
    element_set_integer(update, "integer_attribute", 100);
    auto err = database_update_element(db, "Configuration", id, update);
    element_destroy(update);
    EXPECT_EQ(err, MARGAUX_OK);

    int64_t value;
    int has_value;
    err = database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    // Verify label unchanged
    char* label = nullptr;
    err = database_read_scalar_strings_by_id(db, "Configuration", "label", id, &label, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(label, "Config 1");
    delete[] label;

    database_close(db);
}

TEST(DatabaseCApi, UpdateElementMultipleScalars) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = element_create();
    element_set_string(e, "label", "Config 1");
    element_set_integer(e, "integer_attribute", 42);
    element_set_float(e, "float_attribute", 3.14);
    element_set_string(e, "string_attribute", "hello");
    int64_t id = database_create_element(db, "Configuration", e);
    element_destroy(e);

    // Update multiple scalar attributes at once
    auto update = element_create();
    element_set_integer(update, "integer_attribute", 100);
    element_set_float(update, "float_attribute", 2.71);
    element_set_string(update, "string_attribute", "world");
    auto err = database_update_element(db, "Configuration", id, update);
    element_destroy(update);
    EXPECT_EQ(err, MARGAUX_OK);

    int64_t integer_value;
    int has_value;
    err = database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &integer_value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(integer_value, 100);

    double float_value;
    err = database_read_scalar_floats_by_id(db, "Configuration", "float_attribute", id, &float_value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(float_value, 2.71);

    char* str_value = nullptr;
    err = database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id, &str_value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(str_value, "world");
    delete[] str_value;

    // Verify label unchanged
    char* label = nullptr;
    err = database_read_scalar_strings_by_id(db, "Configuration", "label", id, &label, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(label, "Config 1");
    delete[] label;

    database_close(db);
}

TEST(DatabaseCApi, UpdateElementOtherElementsUnchanged) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = element_create();
    element_set_string(e1, "label", "Config 1");
    element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = database_create_element(db, "Configuration", e1);
    element_destroy(e1);

    auto e2 = element_create();
    element_set_string(e2, "label", "Config 2");
    element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = database_create_element(db, "Configuration", e2);
    element_destroy(e2);

    // Update only first element
    auto update = element_create();
    element_set_integer(update, "integer_attribute", 999);
    auto err = database_update_element(db, "Configuration", id1, update);
    element_destroy(update);
    EXPECT_EQ(err, MARGAUX_OK);

    int64_t value;
    int has_value;

    // Verify first element changed
    err = database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id1, &value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 999);

    // Verify second element unchanged
    err = database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id2, &value, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    database_close(db);
}

TEST(DatabaseCApi, UpdateElementNullArguments) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = element_create();
    element_set_integer(element, "integer_attribute", 42);

    // Null db
    auto err = database_update_element(nullptr, "Configuration", 1, element);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    // Null collection
    err = database_update_element(db, nullptr, 1, element);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    // Null element
    err = database_update_element(db, "Configuration", 1, nullptr);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    element_destroy(element);
    database_close(db);
}

// ============================================================================
// Update scalar null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateScalarIntegerNullDb) {
    auto err = database_update_scalar_integer(nullptr, "Configuration", "integer_attribute", 1, 42);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateScalarIntegerNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_update_scalar_integer(db, nullptr, "integer_attribute", 1, 42);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateScalarIntegerNullAttribute) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_update_scalar_integer(db, "Configuration", nullptr, 1, 42);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateScalarFloatNullDb) {
    auto err = database_update_scalar_float(nullptr, "Configuration", "float_attribute", 1, 3.14);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateScalarFloatNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_update_scalar_float(db, nullptr, "float_attribute", 1, 3.14);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateScalarStringNullDb) {
    auto err = database_update_scalar_string(nullptr, "Configuration", "string_attribute", 1, "test");
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateScalarStringNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_update_scalar_string(db, nullptr, "string_attribute", 1, "test");
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateScalarStringNullValue) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_update_scalar_string(db, "Configuration", "string_attribute", 1, nullptr);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

// ============================================================================
// Update vector null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateVectorIntegersNullDb) {
    int64_t values[] = {1, 2, 3};
    auto err = database_update_vector_integers(nullptr, "Collection", "value_int", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateVectorIntegersNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = database_update_vector_integers(db, nullptr, "value_int", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateVectorIntegersNullAttribute) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = database_update_vector_integers(db, "Collection", nullptr, 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateVectorFloatsNullDb) {
    double values[] = {1.0, 2.0, 3.0};
    auto err = database_update_vector_floats(nullptr, "Collection", "value_float", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateVectorFloatsNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    double values[] = {1.0, 2.0, 3.0};
    auto err = database_update_vector_floats(db, nullptr, "value_float", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateVectorStringsNullDb) {
    const char* values[] = {"a", "b", "c"};
    auto err = database_update_vector_strings(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateVectorStringsNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = database_update_vector_strings(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

// ============================================================================
// Update set null pointer tests
// ============================================================================

TEST(DatabaseCApi, UpdateSetIntegersNullDb) {
    int64_t values[] = {1, 2, 3};
    auto err = database_update_set_integers(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateSetIntegersNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t values[] = {1, 2, 3};
    auto err = database_update_set_integers(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateSetFloatsNullDb) {
    double values[] = {1.0, 2.0, 3.0};
    auto err = database_update_set_floats(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateSetFloatsNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    double values[] = {1.0, 2.0, 3.0};
    auto err = database_update_set_floats(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullDb) {
    const char* values[] = {"a", "b", "c"};
    auto err = database_update_set_strings(nullptr, "Collection", "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, UpdateSetStringsNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = database_update_set_strings(db, nullptr, "tag", 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST(DatabaseCApi, UpdateSetStringsNullAttribute) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    const char* values[] = {"a", "b", "c"};
    auto err = database_update_set_strings(db, "Collection", nullptr, 1, values, 3);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}
