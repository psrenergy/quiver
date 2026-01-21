#include "test_utils.h"

#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>

TEST(DatabaseCApi, DeleteElementById) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = margaux_element_create();
    margaux_element_set_string(e, "label", "Config 1");
    margaux_element_set_integer(e, "integer_attribute", 42);
    int64_t id = margaux_create_element(db, "Configuration", e);
    margaux_element_destroy(e);

    // Verify element exists
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = margaux_read_element_ids(db, "Configuration", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 1);
    margaux_free_integer_array(ids);

    // Delete element
    err = margaux_delete_element_by_id(db, "Configuration", id);
    EXPECT_EQ(err, MARGAUX_OK);

    // Verify element is gone
    err = margaux_read_element_ids(db, "Configuration", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(ids, nullptr);

    margaux_close(db);
}

TEST(DatabaseCApi, DeleteElementByIdWithVectorData) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = margaux_element_create();
    margaux_element_set_string(config, "label", "Test Config");
    margaux_create_element(db, "Configuration", config);
    margaux_element_destroy(config);

    auto e = margaux_element_create();
    margaux_element_set_string(e, "label", "Item 1");
    int64_t values[] = {1, 2, 3};
    margaux_element_set_array_integer(e, "value_int", values, 3);
    int64_t id = margaux_create_element(db, "Collection", e);
    margaux_element_destroy(e);

    // Verify vector data exists
    int64_t* vec_values = nullptr;
    size_t vec_count = 0;
    auto err = margaux_read_vector_integers_by_id(db, "Collection", "value_int", id, &vec_values, &vec_count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(vec_count, 3);
    margaux_free_integer_array(vec_values);

    // Delete element - CASCADE should delete vector rows too
    err = margaux_delete_element_by_id(db, "Collection", id);
    EXPECT_EQ(err, MARGAUX_OK);

    // Verify element is gone
    int64_t* ids = nullptr;
    size_t count = 0;
    err = margaux_read_element_ids(db, "Collection", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(ids, nullptr);

    // Verify vector data is also gone (via CASCADE DELETE)
    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    err = margaux_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(vectors, nullptr);
    EXPECT_EQ(sizes, nullptr);

    margaux_close(db);
}

TEST(DatabaseCApi, DeleteElementByIdWithSetData) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = margaux_element_create();
    margaux_element_set_string(config, "label", "Test Config");
    margaux_create_element(db, "Configuration", config);
    margaux_element_destroy(config);

    auto e = margaux_element_create();
    margaux_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    margaux_element_set_array_string(e, "tag", tags, 2);
    int64_t id = margaux_create_element(db, "Collection", e);
    margaux_element_destroy(e);

    // Verify set data exists
    char** set_values = nullptr;
    size_t set_count = 0;
    auto err = margaux_read_set_strings_by_id(db, "Collection", "tag", id, &set_values, &set_count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(set_count, 2);
    margaux_free_string_array(set_values, set_count);

    // Delete element - CASCADE should delete set rows too
    err = margaux_delete_element_by_id(db, "Collection", id);
    EXPECT_EQ(err, MARGAUX_OK);

    // Verify element is gone
    int64_t* ids = nullptr;
    size_t count = 0;
    err = margaux_read_element_ids(db, "Collection", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(ids, nullptr);

    // Verify set data is also gone (via CASCADE DELETE)
    char*** sets = nullptr;
    size_t* sizes = nullptr;
    err = margaux_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(sets, nullptr);
    EXPECT_EQ(sizes, nullptr);

    margaux_close(db);
}

TEST(DatabaseCApi, DeleteElementByIdNonExistent) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = margaux_element_create();
    margaux_element_set_string(e, "label", "Config 1");
    margaux_element_set_integer(e, "integer_attribute", 42);
    margaux_create_element(db, "Configuration", e);
    margaux_element_destroy(e);

    // Delete non-existent ID - should succeed silently (SQL DELETE is idempotent)
    auto err = margaux_delete_element_by_id(db, "Configuration", 999);
    EXPECT_EQ(err, MARGAUX_OK);

    // Verify original element still exists
    int64_t* ids = nullptr;
    size_t count = 0;
    err = margaux_read_element_ids(db, "Configuration", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 1);
    margaux_free_integer_array(ids);

    margaux_close(db);
}

TEST(DatabaseCApi, DeleteElementByIdOtherElementsUnchanged) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = margaux_element_create();
    margaux_element_set_string(e1, "label", "Config 1");
    margaux_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = margaux_create_element(db, "Configuration", e1);
    margaux_element_destroy(e1);

    auto e2 = margaux_element_create();
    margaux_element_set_string(e2, "label", "Config 2");
    margaux_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = margaux_create_element(db, "Configuration", e2);
    margaux_element_destroy(e2);

    auto e3 = margaux_element_create();
    margaux_element_set_string(e3, "label", "Config 3");
    margaux_element_set_integer(e3, "integer_attribute", 200);
    int64_t id3 = margaux_create_element(db, "Configuration", e3);
    margaux_element_destroy(e3);

    // Delete middle element
    auto err = margaux_delete_element_by_id(db, "Configuration", id2);
    EXPECT_EQ(err, MARGAUX_OK);

    // Verify only two elements remain
    int64_t* ids = nullptr;
    size_t count = 0;
    err = margaux_read_element_ids(db, "Configuration", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id3);
    margaux_free_integer_array(ids);

    // Verify first element unchanged
    int64_t val1;
    int has_value;
    err = margaux_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id1, &val1, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(val1, 42);

    // Verify third element unchanged
    int64_t val3;
    err = margaux_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id3, &val3, &has_value);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(val3, 200);

    margaux_close(db);
}

TEST(DatabaseCApi, DeleteElementByIdNullArguments) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Null db
    auto err = margaux_delete_element_by_id(nullptr, "Configuration", 1);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    // Null collection
    err = margaux_delete_element_by_id(db, nullptr, 1);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    margaux_close(db);
}
