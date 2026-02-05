#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Read scalar tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegers) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_integer(e1, "integer_attribute", 42);
    quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_integer(e2, "integer_attribute", 100);
    quiver_database_create_element(db, "Configuration", e2);
    quiver_element_destroy(e2);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 42);
    EXPECT_EQ(values[1], 100);

    quiver_free_integer_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloats) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_float(e1, "float_attribute", 3.14);
    quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_float(e2, "float_attribute", 2.71);
    quiver_database_create_element(db, "Configuration", e2);
    quiver_element_destroy(e2);

    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(values[0], 3.14);
    EXPECT_DOUBLE_EQ(values[1], 2.71);

    quiver_free_float_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStrings) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_string(e1, "string_attribute", "hello");
    quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_string(e2, "string_attribute", "world");
    quiver_database_create_element(db, "Configuration", e2);
    quiver_element_destroy(e2);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(values[0], "hello");
    EXPECT_STREQ(values[1], "world");

    quiver_free_string_array(values, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    int64_t* integer_values = nullptr;
    size_t integer_count = 0;
    auto err = quiver_database_read_scalar_integers(db, "Collection", "some_integer", &integer_values, &integer_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(integer_count, 0);
    EXPECT_EQ(integer_values, nullptr);

    double* float_values = nullptr;
    size_t float_count = 0;
    err = quiver_database_read_scalar_floats(db, "Collection", "some_float", &float_values, &float_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(float_count, 0);
    EXPECT_EQ(float_values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read vector tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegers) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e1, "value_int", values1, 3);
    quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    quiver_element_set_array_integer(e2, "value_int", values2, 2);
    quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[1][0], 10);
    EXPECT_EQ(vectors[1][1], 20);

    quiver_free_integer_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloats) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(e1, "value_float", values1, 3);
    quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    double values2[] = {10.5, 20.5};
    quiver_element_set_array_float(e2, "value_float", values2, 2);
    quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    double** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, &sizes, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_DOUBLE_EQ(vectors[0][0], 1.5);
    EXPECT_DOUBLE_EQ(vectors[0][1], 2.5);
    EXPECT_DOUBLE_EQ(vectors[0][2], 3.5);
    EXPECT_DOUBLE_EQ(vectors[1][0], 10.5);
    EXPECT_DOUBLE_EQ(vectors[1][1], 20.5);

    quiver_free_float_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    int64_t** integer_vectors = nullptr;
    size_t* integer_sizes = nullptr;
    size_t integer_count = 0;
    auto err = quiver_database_read_vector_integers(
        db, "Collection", "value_int", &integer_vectors, &integer_sizes, &integer_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(integer_count, 0);
    EXPECT_EQ(integer_vectors, nullptr);
    EXPECT_EQ(integer_sizes, nullptr);

    double** float_vectors = nullptr;
    size_t* float_sizes = nullptr;
    size_t float_count = 0;
    err =
        quiver_database_read_vector_floats(db, "Collection", "value_float", &float_vectors, &float_sizes, &float_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(float_count, 0);
    EXPECT_EQ(float_vectors, nullptr);
    EXPECT_EQ(float_sizes, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorOnlyReturnsElementsWithData) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Element with vector data
    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e1, "value_int", values1, 3);
    quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    // Element without vector data
    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    // Another element with vector data
    auto e3 = quiver_element_create();
    quiver_element_set_string(e3, "label", "Item 3");
    int64_t values3[] = {4, 5};
    quiver_element_set_array_integer(e3, "value_int", values3, 2);
    quiver_database_create_element(db, "Collection", e3);
    quiver_element_destroy(e3);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);

    // Only elements with vector data are returned
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[1][0], 4);
    EXPECT_EQ(vectors[1][1], 5);

    quiver_free_integer_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

// ============================================================================
// Read set tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStrings) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important", "urgent"};
    quiver_element_set_array_string(e1, "tag", tags1, 2);
    quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    quiver_element_set_array_string(e2, "tag", tags2, 1);
    quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    EXPECT_EQ(err, QUIVER_OK);
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

    quiver_free_string_vectors(sets, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(sets, nullptr);
    EXPECT_EQ(sizes, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetOnlyReturnsElementsWithData) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Element with set data
    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important"};
    quiver_element_set_array_string(e1, "tag", tags1, 1);
    quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    // Element without set data
    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    // Another element with set data
    auto e3 = quiver_element_create();
    quiver_element_set_string(e3, "label", "Item 3");
    const char* tags3[] = {"urgent", "review"};
    quiver_element_set_array_string(e3, "tag", tags3, 2);
    quiver_database_create_element(db, "Collection", e3);
    quiver_element_destroy(e3);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    // Only elements with set data are returned
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 1);
    EXPECT_EQ(sizes[1], 2);

    quiver_free_string_vectors(sets, sizes, count);
    quiver_database_close(db);
}

// ============================================================================
// Read scalar by ID tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegerById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = quiver_database_create_element(db, "Configuration", e2);
    quiver_element_destroy(e2);

    int64_t value;
    int has_value;
    auto err =
        quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 42);

    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id2, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_float(e1, "float_attribute", 3.14);
    int64_t id1 = quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    double value;
    int has_value;
    auto err =
        quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 3.14);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_string(e1, "string_attribute", "hello");
    int64_t id1 = quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    char* value = nullptr;
    int has_value;
    auto err =
        quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "hello");

    delete[] value;
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarByIdNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = quiver_element_create();
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_integer(e, "integer_attribute", 42);
    quiver_database_create_element(db, "Configuration", e);
    quiver_element_destroy(e);

    int64_t value;
    int has_value;
    auto err =
        quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 999, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 0);

    quiver_database_close(db);
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegerById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e1, "value_int", values1, 3);
    int64_t id1 = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    quiver_element_set_array_integer(e2, "value_int", values2, 2);
    int64_t id2 = quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id1, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    quiver_free_integer_array(values);

    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id2, &values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    quiver_free_integer_array(values);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(e1, "value_float", values1, 3);
    int64_t id1 = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", id1, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_DOUBLE_EQ(values[0], 1.5);
    EXPECT_DOUBLE_EQ(values[1], 2.5);
    EXPECT_DOUBLE_EQ(values[2], 3.5);

    quiver_free_float_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorByIdEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e = quiver_element_create();
    quiver_element_set_string(e, "label", "Item 1");
    int64_t id = quiver_database_create_element(db, "Collection", e);
    quiver_element_destroy(e);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read set by ID tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStringById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important", "urgent"};
    quiver_element_set_array_string(e1, "tag", tags1, 2);
    int64_t id1 = quiver_database_create_element(db, "Collection", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    quiver_element_set_array_string(e2, "tag", tags2, 1);
    int64_t id2 = quiver_database_create_element(db, "Collection", e2);
    quiver_element_destroy(e2);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id1, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "important");
    EXPECT_EQ(set_values[1], "urgent");
    quiver_free_string_array(values, count);

    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id2, &values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "review");
    quiver_free_string_array(values, count);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetByIdEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto e = quiver_element_create();
    quiver_element_set_string(e, "label", "Item 1");
    int64_t id = quiver_database_create_element(db, "Collection", e);
    quiver_element_destroy(e);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read element IDs tests
// ============================================================================

TEST(DatabaseCApi, ReadElementIds) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = quiver_element_create();
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = quiver_database_create_element(db, "Configuration", e1);
    quiver_element_destroy(e1);

    auto e2 = quiver_element_create();
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = quiver_database_create_element(db, "Configuration", e2);
    quiver_element_destroy(e2);

    auto e3 = quiver_element_create();
    quiver_element_set_string(e3, "label", "Config 3");
    quiver_element_set_integer(e3, "integer_attribute", 200);
    int64_t id3 = quiver_database_create_element(db, "Configuration", e3);
    quiver_element_destroy(e3);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Configuration", &ids, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
    EXPECT_EQ(ids[2], id3);

    quiver_free_integer_array(ids);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadElementIdsEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // No Collection elements created
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Collection", &ids, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(ids, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read scalar null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegersNullDb) {
    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(nullptr, "Configuration", "integer_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadScalarIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, nullptr, "integer_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegersNullAttribute) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, "Configuration", nullptr, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegersNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t* values = nullptr;
    err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatsNullDb) {
    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(nullptr, "Configuration", "float_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadScalarFloatsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, nullptr, "float_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    double* values = nullptr;
    err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringsNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(nullptr, "Configuration", "string_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadScalarStringsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, nullptr, "string_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    char** values = nullptr;
    err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Read scalar by ID null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegerByIdNullDb) {
    int64_t value;
    int has_value;
    auto err = quiver_database_read_scalar_integer_by_id(
        nullptr, "Configuration", "integer_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadScalarIntegerByIdNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t value;
    int has_value;
    auto err = quiver_database_read_scalar_integer_by_id(db, nullptr, "integer_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegerByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int has_value;
    auto err =
        quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 1, nullptr, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t value;
    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 1, &value, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatByIdNullDb) {
    double value;
    int has_value;
    auto err =
        quiver_database_read_scalar_float_by_id(nullptr, "Configuration", "float_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadScalarFloatByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int has_value;
    auto err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", 1, nullptr, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    double value;
    err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", 1, &value, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringByIdNullDb) {
    char* value = nullptr;
    int has_value;
    auto err =
        quiver_database_read_scalar_string_by_id(nullptr, "Configuration", "string_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadScalarStringByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int has_value;
    auto err =
        quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", 1, nullptr, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    char* value = nullptr;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", 1, &value, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Read vector null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegersNullDb) {
    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(nullptr, "Collection", "value_int", &vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadVectorIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, nullptr, "value_int", &vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIntegersNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t** vectors = nullptr;
    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatsNullDb) {
    double** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(nullptr, "Collection", "value_float", &vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadVectorFloatsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(db, "Collection", "value_float", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    double** vectors = nullptr;
    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorStringsNullDb) {
    char**** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_strings(nullptr, "Collection", "tag", vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

// ============================================================================
// Read vector by ID null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegersByIdNullDb) {
    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(nullptr, "Collection", "value_int", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadVectorIntegersByIdNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, nullptr, "value_int", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIntegersByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t* values = nullptr;
    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatsByIdNullDb) {
    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(nullptr, "Collection", "value_float", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadVectorFloatsByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    double* values = nullptr;
    err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorStringsByIdNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_strings_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

// ============================================================================
// Read set null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadSetIntegersNullDb) {
    int64_t** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(nullptr, "Collection", "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadSetIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, nullptr, "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetIntegersNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, "Collection", "tag", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t** sets = nullptr;
    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetFloatsNullDb) {
    double** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats(nullptr, "Collection", "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadSetStringsNullDb) {
    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(nullptr, "Collection", "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadSetStringsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, nullptr, "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetStringsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    char*** sets = nullptr;
    err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Read set by ID null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadSetIntegersByIdNullDb) {
    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadSetFloatsByIdNullDb) {
    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadSetStringsByIdNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadSetStringsByIdNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, nullptr, "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetStringsByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", 1, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    char** values = nullptr;
    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", 1, &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// Read element IDs null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadElementIdsNullDb) {
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(nullptr, "Configuration", &ids, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(DatabaseCApi, ReadElementIdsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, nullptr, &ids, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadElementIdsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Configuration", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t* ids = nullptr;
    err = quiver_database_read_element_ids(db, "Configuration", &ids, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

// ============================================================================
// DateTime tests
// ============================================================================

TEST(DatabaseCApi, DateTimeAttributeMetadata) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t* attrs = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_scalar_attributes(db, "Configuration", &attrs, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_GT(count, 0);

    // Find date_attribute and verify it has DATE_TIME type
    bool found_date_attr = false;
    for (size_t i = 0; i < count; ++i) {
        if (std::string(attrs[i].name) == "date_attribute") {
            EXPECT_EQ(attrs[i].data_type, QUIVER_DATA_TYPE_DATE_TIME);
            found_date_attr = true;
            break;
        }
    }
    EXPECT_TRUE(found_date_attr);

    quiver_free_scalar_metadata_array(attrs, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ScalarMetadataForeignKey) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t meta{};
    auto err = quiver_database_get_scalar_metadata(db, "Child", "parent_id", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(meta.is_foreign_key, 1);
    EXPECT_NE(meta.references_collection, nullptr);
    EXPECT_STREQ(meta.references_collection, "Parent");
    EXPECT_NE(meta.references_column, nullptr);
    EXPECT_STREQ(meta.references_column, "id");
    quiver_free_scalar_metadata(&meta);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ScalarMetadataNotForeignKey) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t meta{};
    auto err = quiver_database_get_scalar_metadata(db, "Child", "label", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(meta.is_foreign_key, 0);
    EXPECT_EQ(meta.references_collection, nullptr);
    EXPECT_EQ(meta.references_column, nullptr);
    quiver_free_scalar_metadata(&meta);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ListScalarAttributesForeignKeys) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t* attrs = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_scalar_attributes(db, "Child", &attrs, &count);
    EXPECT_EQ(err, QUIVER_OK);

    // Find parent_id - should be FK
    bool found_fk = false;
    bool found_non_fk = false;
    for (size_t i = 0; i < count; ++i) {
        if (std::string(attrs[i].name) == "parent_id") {
            EXPECT_EQ(attrs[i].is_foreign_key, 1);
            EXPECT_STREQ(attrs[i].references_collection, "Parent");
            EXPECT_STREQ(attrs[i].references_column, "id");
            found_fk = true;
        }
        if (std::string(attrs[i].name) == "label") {
            EXPECT_EQ(attrs[i].is_foreign_key, 0);
            EXPECT_EQ(attrs[i].references_collection, nullptr);
            found_non_fk = true;
        }
    }
    EXPECT_TRUE(found_fk);
    EXPECT_TRUE(found_non_fk);

    quiver_free_scalar_metadata_array(attrs, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, DateTimeReadScalarString) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = quiver_element_create();
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_string(e, "date_attribute", "2024-03-17T09:30:00");
    int64_t id = quiver_database_create_element(db, "Configuration", e);
    quiver_element_destroy(e);
    EXPECT_GT(id, 0);

    // Read the datetime value
    char* value = nullptr;
    int has_value;
    auto err = quiver_database_read_scalar_string_by_id(db, "Configuration", "date_attribute", id, &value, &has_value);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "2024-03-17T09:30:00");

    delete[] value;
    quiver_database_close(db);
}
