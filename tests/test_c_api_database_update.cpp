#include "database_fixture.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

// ============================================================================
// Update scalar tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateScalarInteger) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_integer(e, "integer_attribute", 42);
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    auto err = psr_database_update_scalar_integer(db, "Configuration", "integer_attribute", id, 100);
    EXPECT_EQ(err, PSR_OK);

    int64_t value;
    int has_value;
    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateScalarDouble) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_double(e, "float_attribute", 3.14);
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    auto err = psr_database_update_scalar_double(db, "Configuration", "float_attribute", id, 2.71);
    EXPECT_EQ(err, PSR_OK);

    double value;
    int has_value;
    err = psr_database_read_scalar_doubles_by_id(db, "Configuration", "float_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 2.71);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateScalarString) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_string(e, "string_attribute", "hello");
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    auto err = psr_database_update_scalar_string(db, "Configuration", "string_attribute", id, "world");
    EXPECT_EQ(err, PSR_OK);

    char* value = nullptr;
    int has_value;
    err = psr_database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "world");

    delete[] value;
    psr_database_close(db);
}

// ============================================================================
// Update vector tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateVectorIntegers) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e, "value_int", values1, 3);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    int64_t new_values[] = {10, 20, 30, 40};
    auto err = psr_database_update_vector_integers(db, "Collection", "value_int", id, new_values, 4);
    EXPECT_EQ(err, PSR_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 4);
    EXPECT_EQ(read_values[0], 10);
    EXPECT_EQ(read_values[1], 20);
    EXPECT_EQ(read_values[2], 30);
    EXPECT_EQ(read_values[3], 40);

    psr_free_int_array(read_values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateVectorDoubles) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(e, "value_float", values1, 3);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    double new_values[] = {10.5, 20.5};
    auto err = psr_database_update_vector_doubles(db, "Collection", "value_float", id, new_values, 2);
    EXPECT_EQ(err, PSR_OK);

    double* read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_vector_doubles_by_id(db, "Collection", "value_float", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(read_values[0], 10.5);
    EXPECT_DOUBLE_EQ(read_values[1], 20.5);

    psr_free_double_array(read_values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateVectorToEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e, "value_int", values1, 3);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    auto err = psr_database_update_vector_integers(db, "Collection", "value_int", id, nullptr, 0);
    EXPECT_EQ(err, PSR_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Update set tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateSetStrings) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    psr_element_set_array_string(e, "tag", tags, 2);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    const char* new_tags[] = {"new_tag1", "new_tag2", "new_tag3"};
    auto err = psr_database_update_set_strings(db, "Collection", "tag", id, new_tags, 3);
    EXPECT_EQ(err, PSR_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
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
    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateSetToEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    psr_element_set_array_string(e, "tag", tags, 2);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    auto err = psr_database_update_set_strings(db, "Collection", "tag", id, nullptr, 0);
    EXPECT_EQ(err, PSR_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// update_element tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateElementSingleScalar) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_integer(e, "integer_attribute", 42);
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    // Update single scalar attribute
    auto update = psr_element_create();
    psr_element_set_integer(update, "integer_attribute", 100);
    auto err = psr_database_update_element(db, "Configuration", id, update);
    psr_element_destroy(update);
    EXPECT_EQ(err, PSR_OK);

    int64_t value;
    int has_value;
    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    // Verify label unchanged
    char* label = nullptr;
    err = psr_database_read_scalar_strings_by_id(db, "Configuration", "label", id, &label, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(label, "Config 1");
    delete[] label;

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateElementMultipleScalars) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_integer(e, "integer_attribute", 42);
    psr_element_set_double(e, "float_attribute", 3.14);
    psr_element_set_string(e, "string_attribute", "hello");
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    // Update multiple scalar attributes at once
    auto update = psr_element_create();
    psr_element_set_integer(update, "integer_attribute", 100);
    psr_element_set_double(update, "float_attribute", 2.71);
    psr_element_set_string(update, "string_attribute", "world");
    auto err = psr_database_update_element(db, "Configuration", id, update);
    psr_element_destroy(update);
    EXPECT_EQ(err, PSR_OK);

    int64_t int_value;
    int has_value;
    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &int_value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(int_value, 100);

    double double_value;
    err = psr_database_read_scalar_doubles_by_id(db, "Configuration", "float_attribute", id, &double_value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(double_value, 2.71);

    char* str_value = nullptr;
    err = psr_database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id, &str_value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(str_value, "world");
    delete[] str_value;

    // Verify label unchanged
    char* label = nullptr;
    err = psr_database_read_scalar_strings_by_id(db, "Configuration", "label", id, &label, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(label, "Config 1");
    delete[] label;

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateElementOtherElementsUnchanged) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    // Update only first element
    auto update = psr_element_create();
    psr_element_set_integer(update, "integer_attribute", 999);
    auto err = psr_database_update_element(db, "Configuration", id1, update);
    psr_element_destroy(update);
    EXPECT_EQ(err, PSR_OK);

    int64_t value;
    int has_value;

    // Verify first element changed
    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id1, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 999);

    // Verify second element unchanged
    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id2, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateElementNullArguments) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    psr_element_set_integer(element, "integer_attribute", 42);

    // Null db
    auto err = psr_database_update_element(nullptr, "Configuration", 1, element);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    // Null collection
    err = psr_database_update_element(db, nullptr, 1, element);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    // Null element
    err = psr_database_update_element(db, "Configuration", 1, nullptr);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_element_destroy(element);
    psr_database_close(db);
}
