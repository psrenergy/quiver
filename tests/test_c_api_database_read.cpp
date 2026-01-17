#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Read scalar tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegers) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_integer(e1, "integer_attribute", 42);
    psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_integer(e2, "integer_attribute", 100);
    psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 42);
    EXPECT_EQ(values[1], 100);

    psr_free_int_array(values);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadScalarDoubles) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_double(e1, "float_attribute", 3.14);
    psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_double(e2, "float_attribute", 2.71);
    psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    double* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_doubles(db, "Configuration", "float_attribute", &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(values[0], 3.14);
    EXPECT_DOUBLE_EQ(values[1], 2.71);

    psr_free_double_array(values);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStrings) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_string(e1, "string_attribute", "hello");
    psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_string(e2, "string_attribute", "world");
    psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(values[0], "hello");
    EXPECT_STREQ(values[1], "world");

    psr_free_string_array(values, count);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadScalarEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    int64_t* int_values = nullptr;
    size_t int_count = 0;
    auto err = psr_database_read_scalar_integers(db, "Collection", "some_integer", &int_values, &int_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(int_count, 0);
    EXPECT_EQ(int_values, nullptr);

    double* double_values = nullptr;
    size_t double_count = 0;
    err = psr_database_read_scalar_doubles(db, "Collection", "some_float", &double_values, &double_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(double_count, 0);
    EXPECT_EQ(double_values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Read vector tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegers) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e1, "value_int", values1, 3);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    psr_element_set_array_int(e2, "value_int", values2, 2);
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[1][0], 10);
    EXPECT_EQ(vectors[1][1], 20);

    psr_free_int_vectors(vectors, sizes, count);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadVectorDoubles) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(e1, "value_float", values1, 3);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    double values2[] = {10.5, 20.5};
    psr_element_set_array_double(e2, "value_float", values2, 2);
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    double** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_doubles(db, "Collection", "value_float", &vectors, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_DOUBLE_EQ(vectors[0][0], 1.5);
    EXPECT_DOUBLE_EQ(vectors[0][1], 2.5);
    EXPECT_DOUBLE_EQ(vectors[0][2], 3.5);
    EXPECT_DOUBLE_EQ(vectors[1][0], 10.5);
    EXPECT_DOUBLE_EQ(vectors[1][1], 20.5);

    psr_free_double_vectors(vectors, sizes, count);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadVectorEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    int64_t** int_vectors = nullptr;
    size_t* int_sizes = nullptr;
    size_t int_count = 0;
    auto err = psr_database_read_vector_integers(db, "Collection", "value_int", &int_vectors, &int_sizes, &int_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(int_count, 0);
    EXPECT_EQ(int_vectors, nullptr);
    EXPECT_EQ(int_sizes, nullptr);

    double** double_vectors = nullptr;
    size_t* double_sizes = nullptr;
    size_t double_count = 0;
    err = psr_database_read_vector_doubles(
        db, "Collection", "value_float", &double_vectors, &double_sizes, &double_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(double_count, 0);
    EXPECT_EQ(double_vectors, nullptr);
    EXPECT_EQ(double_sizes, nullptr);

    psr_database_close(db);
}

TEST(DatabaseCApi, ReadVectorOnlyReturnsElementsWithData) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // Element with vector data
    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e1, "value_int", values1, 3);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    // Element without vector data
    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    // Another element with vector data
    auto e3 = psr_element_create();
    psr_element_set_string(e3, "label", "Item 3");
    int64_t values3[] = {4, 5};
    psr_element_set_array_int(e3, "value_int", values3, 2);
    psr_database_create_element(db, "Collection", e3);
    psr_element_destroy(e3);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);

    // Only elements with vector data are returned
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[1][0], 4);
    EXPECT_EQ(vectors[1][1], 5);

    psr_free_int_vectors(vectors, sizes, count);
    psr_database_close(db);
}

// ============================================================================
// Read set tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStrings) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important", "urgent"};
    psr_element_set_array_string(e1, "tag", tags1, 2);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    psr_element_set_array_string(e2, "tag", tags2, 1);
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 2);
    EXPECT_EQ(sizes[1], 1);

    // Sets are unordered, so just check values exist
    std::vector<std::string> set1_values;
    for (size_t i = 0; i < sizes[0]; i++) {
        set1_values.push_back(sets[0][i]);
    }
    std::sort(set1_values.begin(), set1_values.end());
    EXPECT_EQ(set1_values[0], "important");
    EXPECT_EQ(set1_values[1], "urgent");

    EXPECT_STREQ(sets[1][0], "review");

    psr_free_string_vectors(sets, sizes, count);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadSetEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(sets, nullptr);
    EXPECT_EQ(sizes, nullptr);

    psr_database_close(db);
}

TEST(DatabaseCApi, ReadSetOnlyReturnsElementsWithData) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // Element with set data
    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important"};
    psr_element_set_array_string(e1, "tag", tags1, 1);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    // Element without set data
    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    // Another element with set data
    auto e3 = psr_element_create();
    psr_element_set_string(e3, "label", "Item 3");
    const char* tags3[] = {"urgent", "review"};
    psr_element_set_array_string(e3, "tag", tags3, 2);
    psr_database_create_element(db, "Collection", e3);
    psr_element_destroy(e3);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    // Only elements with set data are returned
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 1);
    EXPECT_EQ(sizes[1], 2);

    psr_free_string_vectors(sets, sizes, count);
    psr_database_close(db);
}

// ============================================================================
// Read scalar by ID tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegerById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
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

    int64_t value;
    int has_value;
    auto err =
        psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 42);

    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id2, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    psr_database_close(db);
}

TEST(DatabaseCApi, ReadScalarDoubleById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_double(e1, "float_attribute", 3.14);
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    double value;
    int has_value;
    auto err = psr_database_read_scalar_doubles_by_id(db, "Configuration", "float_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 3.14);

    psr_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_string(e1, "string_attribute", "hello");
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    char* value = nullptr;
    int has_value;
    auto err = psr_database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "hello");

    delete[] value;
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadScalarByIdNotFound) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_integer(e, "integer_attribute", 42);
    psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    int64_t value;
    int has_value;
    auto err =
        psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 999, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 0);

    psr_database_close(db);
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegerById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e1, "value_int", values1, 3);
    int64_t id1 = psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    psr_element_set_array_int(e2, "value_int", values2, 2);
    int64_t id2 = psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id1, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    psr_free_int_array(values);

    err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id2, &values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    psr_free_int_array(values);

    psr_database_close(db);
}

TEST(DatabaseCApi, ReadVectorDoubleById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(e1, "value_float", values1, 3);
    int64_t id1 = psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    double* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_doubles_by_id(db, "Collection", "value_float", id1, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);
    EXPECT_DOUBLE_EQ(values[0], 1.5);
    EXPECT_DOUBLE_EQ(values[1], 2.5);
    EXPECT_DOUBLE_EQ(values[2], 3.5);

    psr_free_double_array(values);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadVectorByIdEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Read set by ID tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStringById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important", "urgent"};
    psr_element_set_array_string(e1, "tag", tags1, 2);
    int64_t id1 = psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    psr_element_set_array_string(e2, "tag", tags2, 1);
    int64_t id2 = psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id1, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "important");
    EXPECT_EQ(set_values[1], "urgent");
    psr_free_string_array(values, count);

    err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id2, &values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "review");
    psr_free_string_array(values, count);

    psr_database_close(db);
}

TEST(DatabaseCApi, ReadSetByIdEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Read element IDs tests
// ============================================================================

TEST(DatabaseCApi, ReadElementIds) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
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

    auto e3 = psr_element_create();
    psr_element_set_string(e3, "label", "Config 3");
    psr_element_set_integer(e3, "integer_attribute", 200);
    int64_t id3 = psr_database_create_element(db, "Configuration", e3);
    psr_element_destroy(e3);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = psr_database_read_element_ids(db, "Configuration", &ids, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
    EXPECT_EQ(ids[2], id3);

    psr_free_int_array(ids);
    psr_database_close(db);
}

TEST(DatabaseCApi, ReadElementIdsEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // No Collection elements created
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = psr_database_read_element_ids(db, "Collection", &ids, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(ids, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Get attribute type tests
// ============================================================================

TEST(DatabaseCApi, GetAttributeTypeScalarInteger) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Configuration", "integer_attribute", &structure, &data_type);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(structure, PSR_ATTRIBUTE_SCALAR);
    EXPECT_EQ(data_type, PSR_DATA_TYPE_INTEGER);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeScalarReal) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Configuration", "float_attribute", &structure, &data_type);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(structure, PSR_ATTRIBUTE_SCALAR);
    EXPECT_EQ(data_type, PSR_DATA_TYPE_REAL);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeScalarText) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Configuration", "string_attribute", &structure, &data_type);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(structure, PSR_ATTRIBUTE_SCALAR);
    EXPECT_EQ(data_type, PSR_DATA_TYPE_TEXT);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeVectorInteger) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Collection", "value_int", &structure, &data_type);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(structure, PSR_ATTRIBUTE_VECTOR);
    EXPECT_EQ(data_type, PSR_DATA_TYPE_INTEGER);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeVectorReal) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Collection", "value_float", &structure, &data_type);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(structure, PSR_ATTRIBUTE_VECTOR);
    EXPECT_EQ(data_type, PSR_DATA_TYPE_REAL);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeSetText) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Collection", "tag", &structure, &data_type);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(structure, PSR_ATTRIBUTE_SET);
    EXPECT_EQ(data_type, PSR_DATA_TYPE_TEXT);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeNotFound) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;
    auto err = psr_database_get_attribute_type(db, "Configuration", "nonexistent", &structure, &data_type);

    EXPECT_EQ(err, PSR_ERROR_DATABASE);

    psr_database_close(db);
}

TEST(DatabaseCApi, GetAttributeTypeInvalidArgument) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_attribute_structure_t structure;
    psr_data_type_t data_type;

    // Null db
    auto err = psr_database_get_attribute_type(nullptr, "Configuration", "label", &structure, &data_type);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    // Null collection
    err = psr_database_get_attribute_type(db, nullptr, "label", &structure, &data_type);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    // Null attribute
    err = psr_database_get_attribute_type(db, "Configuration", nullptr, &structure, &data_type);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    // Null out_structure
    err = psr_database_get_attribute_type(db, "Configuration", "label", nullptr, &data_type);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    // Null out_data_type
    err = psr_database_get_attribute_type(db, "Configuration", "label", &structure, nullptr);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}
