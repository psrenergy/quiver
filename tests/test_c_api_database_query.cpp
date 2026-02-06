#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>

// ============================================================================
// Query string tests
// ============================================================================

TEST(DatabaseCApiQuery, QueryStringReturnsValue) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test Label");
    quiver_element_set_string(e, "string_attribute", "hello world");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    char* value = nullptr;
    int has_value = 0;
    auto err = quiver_database_query_string(
        db, "SELECT string_attribute FROM Configuration WHERE label = 'Test Label'", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "hello world");

    delete[] value;
    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryStringReturnsNoValueWhenEmpty) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* value = nullptr;
    int has_value = 1;  // Initialize to 1 to verify it gets set to 0
    auto err =
        quiver_database_query_string(db, "SELECT string_attribute FROM Configuration WHERE 1 = 0", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 0);
    EXPECT_EQ(value, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryStringNullDb) {
    char* value = nullptr;
    int has_value = 0;
    auto err = quiver_database_query_string(nullptr, "SELECT 1", &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApiQuery, QueryStringNullSql) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* value = nullptr;
    int has_value = 0;
    auto err = quiver_database_query_string(db, nullptr, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Query integer tests
// ============================================================================

TEST(DatabaseCApiQuery, QueryIntegerReturnsValue) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    quiver_element_set_integer(e, "integer_attribute", 42);
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer(
        db, "SELECT integer_attribute FROM Configuration WHERE label = 'Test'", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 42);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryIntegerReturnsNoValueWhenEmpty) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t value = 999;  // Initialize to non-zero to verify behavior
    int has_value = 1;
    auto err = quiver_database_query_integer(
        db, "SELECT integer_attribute FROM Configuration WHERE 1 = 0", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 0);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryIntegerCount) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "A");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "B");
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Configuration", e2, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer(db, "SELECT COUNT(*) FROM Configuration", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 2);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryIntegerNullDb) {
    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer(nullptr, "SELECT 1", &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

// ============================================================================
// Query float tests
// ============================================================================

TEST(DatabaseCApiQuery, QueryFloatReturnsValue) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    quiver_element_set_float(e, "float_attribute", 3.14159);
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    double value = 0.0;
    int has_value = 0;
    auto err = quiver_database_query_float(
        db, "SELECT float_attribute FROM Configuration WHERE label = 'Test'", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 3.14159);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryFloatReturnsNoValueWhenEmpty) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double value = 999.0;
    int has_value = 1;
    auto err =
        quiver_database_query_float(db, "SELECT float_attribute FROM Configuration WHERE 1 = 0", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 0);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryFloatAverage) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "A");
    quiver_element_set_float(e1, "float_attribute", 10.0);
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "B");
    quiver_element_set_float(e2, "float_attribute", 20.0);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Configuration", e2, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    double value = 0.0;
    int has_value = 0;
    auto err = quiver_database_query_float(db, "SELECT AVG(float_attribute) FROM Configuration", &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 15.0);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryFloatNullDb) {
    double value = 0.0;
    int has_value = 0;
    auto err = quiver_database_query_float(nullptr, "SELECT 1.0", &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

// ============================================================================
// Parameterized query tests
// ============================================================================

TEST(DatabaseCApiQuery, QueryStringWithParams) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test Label");
    quiver_element_set_string(e, "string_attribute", "hello world");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* label = "Test Label";
    int param_types[] = {QUIVER_DATA_TYPE_STRING};
    const void* param_values[] = {label};

    char* value = nullptr;
    int has_value = 0;
    auto err = quiver_database_query_string_params(db,
                                                   "SELECT string_attribute FROM Configuration WHERE label = ?",
                                                   param_types,
                                                   param_values,
                                                   1,
                                                   &value,
                                                   &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "hello world");

    delete[] value;
    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryIntegerWithParams) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    quiver_element_set_integer(e, "integer_attribute", 42);
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* label = "Test";
    int param_types[] = {QUIVER_DATA_TYPE_STRING};
    const void* param_values[] = {label};

    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer_params(db,
                                                    "SELECT integer_attribute FROM Configuration WHERE label = ?",
                                                    param_types,
                                                    param_values,
                                                    1,
                                                    &value,
                                                    &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 42);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryFloatWithParams) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    quiver_element_set_float(e, "float_attribute", 3.14159);
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* label = "Test";
    int param_types[] = {QUIVER_DATA_TYPE_STRING};
    const void* param_values[] = {label};

    double value = 0.0;
    int has_value = 0;
    auto err = quiver_database_query_float_params(db,
                                                  "SELECT float_attribute FROM Configuration WHERE label = ?",
                                                  param_types,
                                                  param_values,
                                                  1,
                                                  &value,
                                                  &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 3.14159);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryWithIntegerParam) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    quiver_element_set_integer(e, "integer_attribute", 42);
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int64_t min_val = 10;
    int param_types[] = {QUIVER_DATA_TYPE_INTEGER};
    const void* param_values[] = {&min_val};

    int64_t value = 0;
    int has_value = 0;
    auto err =
        quiver_database_query_integer_params(db,
                                             "SELECT integer_attribute FROM Configuration WHERE integer_attribute > ?",
                                             param_types,
                                             param_values,
                                             1,
                                             &value,
                                             &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 42);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryWithNullParam) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int param_types[] = {QUIVER_DATA_TYPE_NULL};
    const void* param_values[] = {nullptr};

    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer_params(
        db, "SELECT COUNT(*) FROM Configuration WHERE ? IS NULL", param_types, param_values, 1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 1);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryParamsNoMatch) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Test");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    const char* label = "NoMatch";
    int param_types[] = {QUIVER_DATA_TYPE_STRING};
    const void* param_values[] = {label};

    char* value = nullptr;
    int has_value = 1;
    auto err = quiver_database_query_string_params(
        db, "SELECT label FROM Configuration WHERE label = ?", param_types, param_values, 1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 0);
    EXPECT_EQ(value, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApiQuery, QueryParamsNullDb) {
    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer_params(nullptr, "SELECT 1", nullptr, nullptr, 0, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApiQuery, QueryParamsNullStringElement) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int param_types[] = {QUIVER_DATA_TYPE_STRING};
    const void* param_values[] = {nullptr};

    char* value = nullptr;
    int has_value = 0;
    auto err = quiver_database_query_string_params(
        db, "SELECT label FROM Configuration WHERE label = ?", param_types, param_values, 1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_ERROR_DATABASE);

    quiver_database_close(db);
}
