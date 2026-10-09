#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

#include <algorithm>
#include <string>
#include <vector>

// ============================================================================
// Read vector tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegers) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
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
    quiver_element_set_array_integer(e1, "value_int", values1, 3, nullptr);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    quiver_element_set_array_integer(e2, "value_int", values2, 2, nullptr);
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    int64_t** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &masks, &sizes, &count);

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
    quiver_database_free_masks(masks, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloats) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
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
    quiver_element_set_array_float(e1, "value_float", values1, 3, nullptr);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    double values2[] = {10.5, 20.5};
    quiver_element_set_array_float(e2, "value_float", values2, 2, nullptr);
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    double** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, &masks, &sizes, &count);

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
    quiver_database_free_masks(masks, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorEmpty) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    int64_t** integer_vectors = nullptr;
    uint8_t** integer_masks = nullptr;
    size_t* integer_sizes = nullptr;
    size_t integer_count = 0;
    auto err = quiver_database_read_vector_integers(
        db,
        "Collection",
        "value_int",
        &integer_vectors,
        &integer_masks,
        &integer_sizes,
        &integer_count
    );
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(integer_count, 0);
    EXPECT_EQ(integer_vectors, nullptr);
    EXPECT_EQ(integer_masks, nullptr);
    EXPECT_EQ(integer_sizes, nullptr);

    double** float_vectors = nullptr;
    uint8_t** float_masks = nullptr;
    size_t* float_sizes = nullptr;
    size_t float_count = 0;
    err = quiver_database_read_vector_floats(
        db,
        "Collection",
        "value_float",
        &float_vectors,
        &float_masks,
        &float_sizes,
        &float_count
    );
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(float_count, 0);
    EXPECT_EQ(float_vectors, nullptr);
    EXPECT_EQ(float_masks, nullptr);
    EXPECT_EQ(float_sizes, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIncludesElementsWithNoRows) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
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
    quiver_element_set_array_integer(e1, "value_int", values1, 3, nullptr);
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
    quiver_element_set_array_integer(e3, "value_int", values3, 2, nullptr);
    int64_t tmp_id4 = 0;
    quiver_database_create_element(db, "Collection", e3, &tmp_id4);
    EXPECT_EQ(quiver_element_destroy(e3), QUIVER_OK);

    int64_t** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &masks, &sizes, &count);

    // One entry per element: the element with no rows has size 0, not a gap
    ASSERT_EQ(err, QUIVER_OK);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(sizes[0], 3);
    ASSERT_EQ(sizes[1], 0);
    ASSERT_EQ(sizes[2], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[2][0], 4);
    EXPECT_EQ(vectors[2][1], 5);

    quiver_database_free_integer_vectors(vectors, sizes, count);
    quiver_database_free_masks(masks, count);
    quiver_database_close(db);
}

// ============================================================================
// Read vector by Id tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegerById) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
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
    quiver_element_set_array_integer(e1, "value_int", values1, 3, nullptr);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    quiver_element_set_array_integer(e2, "value_int", values2, 2, nullptr);
    int64_t id2 = 0;
    quiver_database_create_element(db, "Collection", e2, &id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id1, &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    quiver_database_free_integer_array(values);
    quiver_database_free_mask(mask);

    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id2, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    quiver_database_free_integer_array(values);
    quiver_database_free_mask(mask);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatById) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
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
    quiver_element_set_array_float(e1, "value_float", values1, 3, nullptr);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", id1, &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_DOUBLE_EQ(values[0], 1.5);
    EXPECT_DOUBLE_EQ(values[1], 2.5);
    EXPECT_DOUBLE_EQ(values[2], 3.5);

    quiver_database_free_float_array(values);
    quiver_database_free_mask(mask);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorByIdEmpty) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
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
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);
    EXPECT_EQ(mask, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read vector null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegersNullDb) {
    int64_t** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err =
        quiver_database_read_vector_integers(nullptr, "Collection", "value_int", &vectors, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorIntegersNullCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    int64_t** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, nullptr, "value_int", &vectors, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIntegersNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    int64_t** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers(db, "Collection", "value_int", nullptr, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &masks, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &masks, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatsNullDb) {
    double** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err =
        quiver_database_read_vector_floats(nullptr, "Collection", "value_float", &vectors, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorFloatsNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    double** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats(db, "Collection", "value_float", nullptr, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, &masks, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_floats(db, "Collection", "value_float", &vectors, &masks, &sizes, nullptr);
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
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err =
        quiver_database_read_vector_integers_by_id(nullptr, "Collection", "value_int", 1, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorIntegersByIdNullCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, nullptr, "value_int", 1, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorIntegersByIdNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, nullptr, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, &values, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_integers_by_id(db, "Collection", "value_int", 1, &values, &mask, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorFloatsByIdNullDb) {
    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err =
        quiver_database_read_vector_floats_by_id(nullptr, "Collection", "value_float", 1, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadVectorFloatsByIdNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, nullptr, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, &values, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_vector_floats_by_id(db, "Collection", "value_float", 1, &values, &mask, nullptr);
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
    auto options = quiver::test::quiet_options();
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
    quiver_element_set_array_string(update, "label_value", str_values, 3, nullptr);
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
    auto options = quiver::test::quiet_options();
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
    quiver_element_set_array_string(update, "label_value", str_values, 2, nullptr);
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

TEST(DatabaseCApi, ReadVectorGroupByIdPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* parent = nullptr;
    ASSERT_EQ(quiver_element_create(&parent), QUIVER_OK);
    quiver_element_set_string(parent, "label", "Parent 1");
    int64_t parent_id = 0;
    quiver_database_create_element(db, "Parent", parent, &parent_id);
    EXPECT_EQ(quiver_element_destroy(parent), QUIVER_OK);

    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    int64_t refs[] = {1, 0};
    uint8_t refs_mask[] = {1, 0};
    quiver_element_set_array_integer(child, "parent_ref", refs, 2, refs_mask);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    char** column_names = nullptr;
    int* column_types = nullptr;
    void** column_data = nullptr;
    uint8_t** column_has_value = nullptr;
    size_t column_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_group_by_id(
            db,
            "Child",
            "refs",
            child_id,
            &column_names,
            &column_types,
            &column_data,
            &column_has_value,
            &column_count,
            &row_count
        ),
        QUIVER_OK
    );

    ASSERT_EQ(column_count, 1);
    ASSERT_EQ(row_count, 2);
    EXPECT_STREQ(column_names[0], "parent_ref");
    EXPECT_EQ(column_types[0], QUIVER_DATA_TYPE_INTEGER);
    EXPECT_EQ(column_has_value[0][0], 1);
    EXPECT_EQ(static_cast<int64_t*>(column_data[0])[0], 1);
    EXPECT_EQ(column_has_value[0][1], 0);

    quiver_database_free_time_series_data(
        column_names,
        column_types,
        column_data,
        column_has_value,
        column_count,
        row_count
    );
    quiver_database_close(db);
}

// A non-STRICT table's INTEGER column keeps a non-integral value as REAL (INTEGER affinity only
// converts what it can convert losslessly). The group reader must report that cell absent, as
// Row::get_integer and the per-column reader do - not truncate 1.5 to 1 and call it present.
TEST(DatabaseCApi, ReadVectorGroupByIdMasksRealCellInIntegerColumn) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("non_strict_vector.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* item = nullptr;
    ASSERT_EQ(quiver_element_create(&item), QUIVER_OK);
    quiver_element_set_string(item, "label", "Item 1");
    int64_t item_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", item, &item_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(item), QUIVER_OK);

    // Every API write path rejects a double for an INTEGER column, so raw SQL is the only way in.
    int param_types[] = {QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_INTEGER};
    const void* param_values[] = {&item_id, &item_id};
    int64_t unused = 0;
    int has_value = 1;
    ASSERT_EQ(
        quiver_database_query_integer(
            db,
            "INSERT INTO Items_vector_counts (id, vector_index, quantity) "
            "VALUES (?, 1, 7), (?, 2, 1.5)",
            param_types,
            param_values,
            2,
            &unused,
            &has_value
        ),
        QUIVER_OK
    );
    EXPECT_EQ(has_value, 0);

    char** column_names = nullptr;
    int* column_types = nullptr;
    void** column_data = nullptr;
    uint8_t** column_has_value = nullptr;
    size_t column_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_group_by_id(
            db,
            "Items",
            "counts",
            item_id,
            &column_names,
            &column_types,
            &column_data,
            &column_has_value,
            &column_count,
            &row_count
        ),
        QUIVER_OK
    );

    ASSERT_EQ(column_count, 1);
    ASSERT_EQ(row_count, 2);
    EXPECT_STREQ(column_names[0], "quantity");
    EXPECT_EQ(column_types[0], QUIVER_DATA_TYPE_INTEGER);
    EXPECT_EQ(column_has_value[0][0], 1);
    EXPECT_EQ(static_cast<int64_t*>(column_data[0])[0], 7);
    EXPECT_EQ(column_has_value[0][1], 0);

    quiver_database_free_time_series_data(
        column_names,
        column_types,
        column_data,
        column_has_value,
        column_count,
        row_count
    );

    // The per-column reader reports the same cell absent, so the two readers agree.
    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_integers_by_id(db, "Items", "quantity", item_id, &values, &mask, &count),
        QUIVER_OK
    );
    ASSERT_EQ(count, 2);
    EXPECT_EQ(mask[0], 1);
    EXPECT_EQ(values[0], 7);
    EXPECT_EQ(mask[1], 0);
    quiver_database_free_integer_array(values);
    quiver_database_free_mask(mask);

    quiver_database_close(db);
}

// ============================================================================
// NULL handling in vector reads
// ============================================================================

TEST(DatabaseCApi, ReadVectorIntegersPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Item 1 has a NULL cell in the middle; Item 2 has no group rows at all.
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t values[] = {10, 0, 30};
    const uint8_t has_value[] = {1, 0, 1};
    quiver_element_set_array_integer(e1, "value_int", values, 3, has_value);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e1, &id1), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e2, &id2), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    // Item 3 has exactly one row, and its value is NULL: the pair only the presence column tells
    // apart from Item 2 (no rows at all).
    quiver_element_t* e3 = nullptr;
    ASSERT_EQ(quiver_element_create(&e3), QUIVER_OK);
    quiver_element_set_string(e3, "label", "Item 3");
    int64_t null_only[] = {0};
    const uint8_t null_only_mask[] = {0};
    quiver_element_set_array_integer(e3, "value_int", null_only, 1, null_only_mask);
    int64_t id3 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e3, &id3), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e3), QUIVER_OK);

    int64_t** vectors = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &masks, &sizes, &count),
        QUIVER_OK
    );
    ASSERT_EQ(count, 3);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(masks[0][0], 1);
    EXPECT_EQ(masks[0][1], 0);
    EXPECT_EQ(masks[0][2], 1);
    EXPECT_EQ(vectors[0][0], 10);
    EXPECT_EQ(vectors[0][1], 0);  // the documented placeholder in a masked slot
    EXPECT_EQ(vectors[0][2], 30);
    // The element with no rows is an empty entry, not a NULL cell.
    EXPECT_EQ(sizes[1], 0);
    EXPECT_EQ(vectors[1], nullptr);
    EXPECT_EQ(masks[1], nullptr);
    // The element whose only row is NULL is one masked cell, not an empty entry.
    EXPECT_EQ(sizes[2], 1);
    EXPECT_EQ(masks[2][0], 0);
    quiver_database_free_integer_vectors(vectors, sizes, count);
    quiver_database_free_masks(masks, count);

    int64_t* by_id = nullptr;
    uint8_t* by_id_mask = nullptr;
    size_t by_id_count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_integers_by_id(
            db,
            "Collection",
            "value_int",
            id1,
            &by_id,
            &by_id_mask,
            &by_id_count
        ),
        QUIVER_OK
    );
    ASSERT_EQ(by_id_count, 3);
    EXPECT_EQ(by_id_mask[0], 1);
    EXPECT_EQ(by_id_mask[1], 0);
    EXPECT_EQ(by_id_mask[2], 1);
    EXPECT_EQ(by_id[0], 10);
    EXPECT_EQ(by_id[2], 30);
    quiver_database_free_integer_array(by_id);
    quiver_database_free_mask(by_id_mask);

    // value_float shares the group and was never written, so every cell of Item 1 is NULL.
    double** float_vectors = nullptr;
    uint8_t** float_masks = nullptr;
    size_t* float_sizes = nullptr;
    size_t float_count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_floats(
            db,
            "Collection",
            "value_float",
            &float_vectors,
            &float_masks,
            &float_sizes,
            &float_count
        ),
        QUIVER_OK
    );
    ASSERT_EQ(float_count, 3);
    ASSERT_EQ(float_sizes[0], 3);
    for (size_t j = 0; j < 3; ++j) {
        EXPECT_EQ(float_masks[0][j], 0);
        EXPECT_EQ(float_vectors[0][j], 0.0);
    }
    EXPECT_EQ(float_sizes[1], 0);
    EXPECT_EQ(float_sizes[2], 1);
    EXPECT_EQ(float_masks[2][0], 0);
    quiver_database_free_float_vectors(float_vectors, float_sizes, float_count);
    quiver_database_free_masks(float_masks, float_count);

    double* float_by_id = nullptr;
    uint8_t* float_by_id_mask = nullptr;
    size_t float_by_id_count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_floats_by_id(
            db,
            "Collection",
            "value_float",
            id1,
            &float_by_id,
            &float_by_id_mask,
            &float_by_id_count
        ),
        QUIVER_OK
    );
    ASSERT_EQ(float_by_id_count, 3);
    for (size_t j = 0; j < 3; ++j) {
        EXPECT_EQ(float_by_id_mask[j], 0);
    }
    quiver_database_free_float_array(float_by_id);
    quiver_database_free_mask(float_by_id_mask);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadVectorStringsPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_column_groups.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // note lives only in Items_vector_events, so the array routes there; a nullptr entry is NULL.
    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    const char* notes[] = {"x", nullptr, "z"};
    quiver_element_set_array_string(e, "note", notes, 3, nullptr);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    char*** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_vector_strings(db, "Items", "note", &vectors, &sizes, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(sizes[0], 3);
    EXPECT_STREQ(vectors[0][0], "x");
    EXPECT_EQ(vectors[0][1], nullptr);
    EXPECT_STREQ(vectors[0][2], "z");
    quiver_database_free_string_vectors(vectors, sizes, count);

    char** by_id = nullptr;
    size_t by_id_count = 0;
    ASSERT_EQ(quiver_database_read_vector_strings_by_id(db, "Items", "note", id, &by_id, &by_id_count), QUIVER_OK);
    ASSERT_EQ(by_id_count, 3);
    EXPECT_STREQ(by_id[0], "x");
    EXPECT_EQ(by_id[1], nullptr);
    EXPECT_STREQ(by_id[2], "z");
    quiver_database_free_string_array(by_id, by_id_count);

    quiver_database_close(db);
}
