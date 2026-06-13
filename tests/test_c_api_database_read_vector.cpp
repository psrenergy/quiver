#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Read vector tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegers) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e1, "value_int", values1, 3);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    quiver_element_set_array_integer(e2, "value_int", values2, 2);
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

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

    quiver_database_free_integer_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloats) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(e1, "value_float", values1, 3);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    double values2[] = {10.5, 20.5};
    quiver_element_set_array_float(e2, "value_float", values2, 2);
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

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

    quiver_database_free_float_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorEmpty) {
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Element with vector data
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e1, "value_int", values1, 3);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Element without vector data
    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    // Another element with vector data
    quiver_element_t* e3 = nullptr;
    ASSERT_EQ(quiver_element_create(&e3), QUIVER_OK);
    quiver_element_set_string(e3, "label", "Item 3");
    int64_t values3[] = {4, 5};
    quiver_element_set_array_integer(e3, "value_int", values3, 2);
    int64_t tmp_id4 = 0;
    quiver_database_create_element(db, "Collection", e3, &tmp_id4);
    EXPECT_EQ(quiver_element_destroy(e3), QUIVER_OK);

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

    quiver_database_free_integer_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

// ============================================================================
// Read vector by Id tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegerById) {
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

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    quiver_element_set_array_integer(e1, "value_int", values1, 3);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    quiver_element_set_array_integer(e2, "value_int", values2, 2);
    int64_t id2 = 0;
    quiver_database_create_element(db, "Collection", e2, &id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id1, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    quiver_database_free_integer_array(values);

    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id2, &values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    quiver_database_free_integer_array(values);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatById) {
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

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(e1, "value_float", values1, 3);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", id1, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_DOUBLE_EQ(values[0], 1.5);
    EXPECT_DOUBLE_EQ(values[1], 2.5);
    EXPECT_DOUBLE_EQ(values[2], 3.5);

    quiver_database_free_float_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorByIdEmpty) {
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
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

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
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, nullptr, "value_int", &vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIntegersNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    int64_t** vectors = nullptr;
    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatsNullDb) {
    double** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(nullptr, "Collection", "value_float", &vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorFloatsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(db, "Collection", "value_float", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    double** vectors = nullptr;
    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorStringsNullDb) {
    char**** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_strings(nullptr, "Collection", "tag", vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

// ============================================================================
// Read vector by ID null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegersByIdNullDb) {
    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(nullptr, "Collection", "value_int", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorIntegersByIdNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, nullptr, "value_int", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIntegersByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    int64_t* values = nullptr;
    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatsByIdNullDb) {
    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(nullptr, "Collection", "value_float", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorFloatsByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    double* values = nullptr;
    err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorStringsByIdNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_strings_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

// ============================================================================
// Gap-fill: Read vector strings (using all_types.sql)
// ============================================================================

TEST(DatabaseCApi, ReadVectorStringsHappyPath) {
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

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* str_values[] = {"alpha", "beta", "gamma"};
    quiver_element_set_array_string(update, "label_value", str_values, 3);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    char*** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_strings(db, "AllTypes", "label_value", &vectors, &sizes, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_STREQ(vectors[0][0], "alpha");
    EXPECT_STREQ(vectors[0][1], "beta");
    EXPECT_STREQ(vectors[0][2], "gamma");

    quiver_database_free_string_vectors(vectors, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorStringsByIdHappyPath) {
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

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* str_values[] = {"hello", "world"};
    quiver_element_set_array_string(update, "label_value", str_values, 2);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    char** read_values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_strings_by_id(db, "AllTypes", "label_value", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(read_values[0], "hello");
    EXPECT_STREQ(read_values[1], "world");

    quiver_database_free_string_array(read_values, count);
    quiver_database_close(db);
}

