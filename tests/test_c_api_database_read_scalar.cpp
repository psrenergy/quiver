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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_integer(e1, "integer_attribute", 42);
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_integer(e2, "integer_attribute", 100);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Configuration", e2, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 42);
    EXPECT_EQ(values[1], 100);
    EXPECT_EQ(mask[0], 1);
    EXPECT_EQ(mask[1], 1);

    quiver_database_free_integer_array(values);
    quiver_database_free_mask(mask);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloats) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_float(e1, "float_attribute", 3.14);
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_float(e2, "float_attribute", 2.71);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Configuration", e2, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(values[0], 3.14);
    EXPECT_DOUBLE_EQ(values[1], 2.71);
    EXPECT_EQ(mask[0], 1);
    EXPECT_EQ(mask[1], 1);

    quiver_database_free_float_array(values);
    quiver_database_free_mask(mask);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatsPreservesNull) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // 4 elements; element 2 leaves float_attribute (no default) unset -> SQL NULL.
    const char* labels[] = {"Config 1", "Config 2", "Config 3", "Config 4"};
    const double float_values[] = {10.0, 0.0, 30.0, 40.0};
    const bool has_float[] = {true, false, true, true};
    for (int i = 0; i < 4; ++i) {
        quiver_element_t* e = nullptr;
        ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
        quiver_element_set_string(e, "label", labels[i]);
        if (has_float[i]) {
            quiver_element_set_float(e, "float_attribute", float_values[i]);
        }
        int64_t tmp_id = 0;
        quiver_database_create_element(db, "Configuration", e, &tmp_id);
        EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
    }

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    // One entry per element; the NULL occupies its slot positionally (mask[i] == 0).
    ASSERT_EQ(count, 4u);
    EXPECT_EQ(mask[0], 1);
    EXPECT_DOUBLE_EQ(values[0], 10.0);
    EXPECT_EQ(mask[1], 0);
    EXPECT_EQ(mask[2], 1);
    EXPECT_DOUBLE_EQ(values[2], 30.0);
    EXPECT_EQ(mask[3], 1);
    EXPECT_DOUBLE_EQ(values[3], 40.0);

    quiver_database_free_float_array(values);
    quiver_database_free_mask(mask);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegersPreservesNull) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("all_types.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // 3 AllTypes elements; element 2 leaves some_integer (no default) unset -> SQL NULL.
    const char* labels[] = {"a", "b", "c"};
    const int64_t int_values[] = {10, 0, 30};
    const bool has_int[] = {true, false, true};
    for (int i = 0; i < 3; ++i) {
        quiver_element_t* e = nullptr;
        ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
        quiver_element_set_string(e, "label", labels[i]);
        if (has_int[i]) {
            quiver_element_set_integer(e, "some_integer", int_values[i]);
        }
        int64_t tmp_id = 0;
        quiver_database_create_element(db, "AllTypes", e, &tmp_id);
        EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
    }

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, "AllTypes", "some_integer", &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(count, 3u);
    EXPECT_EQ(mask[0], 1);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(mask[1], 0);
    EXPECT_EQ(mask[2], 1);
    EXPECT_EQ(values[2], 30);

    quiver_database_free_integer_array(values);
    quiver_database_free_mask(mask);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringsPreservesNull) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // 4 elements; element 2 leaves string_attribute unset -> SQL NULL (a nullptr entry, no mask).
    // Element 4 stores an empty string -> a present, non-NULL value distinct from element 2.
    const char* labels[] = {"Config 1", "Config 2", "Config 3", "Config 4"};
    const char* str_values[] = {"hello", nullptr, "world", ""};
    for (int i = 0; i < 4; ++i) {
        quiver_element_t* e = nullptr;
        ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
        quiver_element_set_string(e, "label", labels[i]);
        if (str_values[i] != nullptr) {
            quiver_element_set_string(e, "string_attribute", str_values[i]);
        }
        int64_t tmp_id = 0;
        quiver_database_create_element(db, "Configuration", e, &tmp_id);
        EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
    }

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(count, 4u);
    EXPECT_STREQ(values[0], "hello");
    EXPECT_EQ(values[1], nullptr);  // NULL string is a nullptr entry
    EXPECT_STREQ(values[2], "world");
    ASSERT_NE(values[3], nullptr);  // empty string is present, not NULL
    EXPECT_STREQ(values[3], "");

    quiver_database_free_string_array(values, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatsAllNull) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    for (int i = 0; i < 2; ++i) {
        quiver_element_t* e = nullptr;
        ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
        quiver_element_set_string(e, "label", i == 0 ? "Config 1" : "Config 2");
        int64_t tmp_id = 0;
        quiver_database_create_element(db, "Configuration", e, &tmp_id);
        EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
    }

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, &mask, &count);

    EXPECT_EQ(err, QUIVER_OK);
    // All-NULL column: full-length result with every mask entry 0 (not an empty array).
    ASSERT_EQ(count, 2u);
    EXPECT_EQ(mask[0], 0);
    EXPECT_EQ(mask[1], 0);

    quiver_database_free_float_array(values);
    quiver_database_free_mask(mask);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStrings) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_string(e1, "string_attribute", "hello");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Config 2");
    quiver_element_set_string(e2, "string_attribute", "world");
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Configuration", e2, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(values[0], "hello");
    EXPECT_STREQ(values[1], "world");

    quiver_database_free_string_array(values, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarEmpty) {
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

    int64_t* integer_values = nullptr;
    uint8_t* integer_mask = nullptr;
    size_t integer_count = 0;
    auto err = quiver_database_read_scalar_integers(
        db, "Collection", "some_integer", &integer_values, &integer_mask, &integer_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(integer_count, 0);
    EXPECT_EQ(integer_values, nullptr);
    EXPECT_EQ(integer_mask, nullptr);

    double* float_values = nullptr;
    uint8_t* float_mask = nullptr;
    size_t float_count = 0;
    err = quiver_database_read_scalar_floats(db, "Collection", "some_float", &float_values, &float_mask, &float_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(float_count, 0);
    EXPECT_EQ(float_values, nullptr);
    EXPECT_EQ(float_mask, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read scalar by Id tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegerById) {
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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_float(e1, "float_attribute", 3.14);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    double value;
    int has_value;
    auto err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 3.14);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringById) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Config 1");
    quiver_element_set_string(e1, "string_attribute", "hello");
    int64_t id1 = 0;
    quiver_database_create_element(db, "Configuration", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

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
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_integer(e, "integer_attribute", 42);
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", e, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    int64_t value;
    int has_value;
    auto err =
        quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 999, &value, &has_value);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(has_value, 0);

    quiver_database_close(db);
}

// ============================================================================
// Read element Ids tests
// ============================================================================

TEST(DatabaseCApi, ReadElementIds) {
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

    quiver_element_t* e3 = nullptr;
    ASSERT_EQ(quiver_element_create(&e3), QUIVER_OK);
    quiver_element_set_string(e3, "label", "Config 3");
    quiver_element_set_integer(e3, "integer_attribute", 200);
    int64_t id3 = 0;
    quiver_database_create_element(db, "Configuration", e3, &id3);
    EXPECT_EQ(quiver_element_destroy(e3), QUIVER_OK);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Configuration", &ids, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
    EXPECT_EQ(ids[2], id3);

    quiver_database_free_integer_array(ids);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadElementIdsEmpty) {
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
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err =
        quiver_database_read_scalar_integers(nullptr, "Configuration", "integer_attribute", &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadScalarIntegersNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, nullptr, "integer_attribute", &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegersNullAttribute) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_integers(db, "Configuration", nullptr, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegersNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;

    auto err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", nullptr, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, &mask, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatsNullDb) {
    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(nullptr, "Configuration", "float_attribute", &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadScalarFloatsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_floats(db, nullptr, "float_attribute", &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;

    auto err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", nullptr, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &values, &mask, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringsNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(nullptr, "Configuration", "string_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadScalarStringsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, nullptr, "string_attribute", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    char** values = nullptr;
    err = quiver_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Read scalar by ID null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadScalarIntegerByIdNullDb) {
    int64_t value;
    int has_value;
    auto err =
        quiver_database_read_scalar_integer_by_id(nullptr, "Configuration", "integer_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadScalarIntegerByIdNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t value;
    int has_value;
    auto err = quiver_database_read_scalar_integer_by_id(db, nullptr, "integer_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarIntegerByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int has_value;
    auto err =
        quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 1, nullptr, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);

    int64_t value;
    err = quiver_database_read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 1, &value, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarFloatByIdNullDb) {
    double value;
    int has_value;
    auto err =
        quiver_database_read_scalar_float_by_id(nullptr, "Configuration", "float_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadScalarFloatByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int has_value;
    auto err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", 1, nullptr, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);

    double value;
    err = quiver_database_read_scalar_float_by_id(db, "Configuration", "float_attribute", 1, &value, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadScalarStringByIdNullDb) {
    char* value = nullptr;
    int has_value;
    auto err =
        quiver_database_read_scalar_string_by_id(nullptr, "Configuration", "string_attribute", 1, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadScalarStringByIdNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int has_value;
    auto err =
        quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", 1, nullptr, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);

    char* value = nullptr;
    err = quiver_database_read_scalar_string_by_id(db, "Configuration", "string_attribute", 1, &value, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Read element Ids null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadElementIdsNullDb) {
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(nullptr, "Configuration", &ids, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadElementIdsNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, nullptr, &ids, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadElementIdsNullOutput) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Configuration", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    int64_t* ids = nullptr;
    err = quiver_database_read_element_ids(db, "Configuration", &ids, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// DateTime tests
// ============================================================================

TEST(DatabaseCApi, DateTimeAttributeMetadata) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t* attributes = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_scalar_attributes(db, "Configuration", &attributes, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_GT(count, 0);

    // Find date_attribute and verify it has DATE_TIME type
    bool found_date_attr = false;
    for (size_t i = 0; i < count; ++i) {
        if (std::string(attributes[i].name) == "date_attribute") {
            EXPECT_EQ(attributes[i].data_type, QUIVER_DATA_TYPE_DATE_TIME);
            found_date_attr = true;
            break;
        }
    }
    EXPECT_TRUE(found_date_attr);

    quiver_database_free_scalar_metadata_array(attributes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ScalarMetadataForeignKey) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t metadata{};
    auto err = quiver_database_get_scalar_metadata(db, "Child", "parent_id", &metadata);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(metadata.is_foreign_key, 1);
    EXPECT_NE(metadata.references_collection, nullptr);
    EXPECT_STREQ(metadata.references_collection, "Parent");
    EXPECT_NE(metadata.references_column, nullptr);
    EXPECT_STREQ(metadata.references_column, "id");
    quiver_database_free_scalar_metadata(&metadata);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ScalarMetadataNotForeignKey) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t metadata{};
    auto err = quiver_database_get_scalar_metadata(db, "Child", "label", &metadata);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(metadata.is_foreign_key, 0);
    EXPECT_EQ(metadata.references_collection, nullptr);
    EXPECT_EQ(metadata.references_column, nullptr);
    quiver_database_free_scalar_metadata(&metadata);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ListScalarAttributesForeignKeys) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t* attributes = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_scalar_attributes(db, "Child", &attributes, &count);
    EXPECT_EQ(err, QUIVER_OK);

    // Find parent_id - should be FK
    bool found_fk = false;
    bool found_non_fk = false;
    for (size_t i = 0; i < count; ++i) {
        if (std::string(attributes[i].name) == "parent_id") {
            EXPECT_EQ(attributes[i].is_foreign_key, 1);
            EXPECT_STREQ(attributes[i].references_collection, "Parent");
            EXPECT_STREQ(attributes[i].references_column, "id");
            found_fk = true;
        }
        if (std::string(attributes[i].name) == "label") {
            EXPECT_EQ(attributes[i].is_foreign_key, 0);
            EXPECT_EQ(attributes[i].references_collection, nullptr);
            found_non_fk = true;
        }
    }
    EXPECT_TRUE(found_fk);
    EXPECT_TRUE(found_non_fk);

    quiver_database_free_scalar_metadata_array(attributes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, DateTimeReadScalarString) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Config 1");
    quiver_element_set_string(e, "date_attribute", "2024-03-17T09:30:00");
    int64_t id = 0;
    quiver_database_create_element(db, "Configuration", e, &id);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
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
