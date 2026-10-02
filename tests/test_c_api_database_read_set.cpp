#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <optional>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Read set tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStrings) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
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
    const char* tags1[] = {"important", "urgent"};
    quiver_element_set_array_string(e1, "tag", tags1, 2, nullptr);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    quiver_element_set_array_string(e2, "tag", tags2, 1, nullptr);
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

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

    quiver_database_free_string_vectors(sets, sizes, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetEmpty) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

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

TEST(DatabaseCApi, ReadSetIncludesElementsWithNoRows) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Element with set data
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important"};
    quiver_element_set_array_string(e1, "tag", tags1, 1, nullptr);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Element without set data
    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t tmp_id3 = 0;
    quiver_database_create_element(db, "Collection", e2, &tmp_id3);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    // Another element with set data
    quiver_element_t* e3 = nullptr;
    ASSERT_EQ(quiver_element_create(&e3), QUIVER_OK);
    quiver_element_set_string(e3, "label", "Item 3");
    const char* tags3[] = {"urgent", "review"};
    quiver_element_set_array_string(e3, "tag", tags3, 2, nullptr);
    int64_t tmp_id4 = 0;
    quiver_database_create_element(db, "Collection", e3, &tmp_id4);
    EXPECT_EQ(quiver_element_destroy(e3), QUIVER_OK);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    // One entry per element: the element with no rows has size 0, not a gap
    ASSERT_EQ(err, QUIVER_OK);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(sizes[0], 1);
    ASSERT_EQ(sizes[1], 0);
    ASSERT_EQ(sizes[2], 2);
    EXPECT_STREQ(sets[0][0], "important");
    EXPECT_STREQ(sets[2][0], "urgent");
    EXPECT_STREQ(sets[2][1], "review");

    quiver_database_free_string_vectors(sets, sizes, count);
    quiver_database_close(db);
}

// ============================================================================
// Read set by Id tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStringById) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
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
    const char* tags1[] = {"important", "urgent"};
    quiver_element_set_array_string(e1, "tag", tags1, 2, nullptr);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    quiver_element_set_array_string(e2, "tag", tags2, 1, nullptr);
    int64_t id2 = 0;
    quiver_database_create_element(db, "Collection", e2, &id2);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

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
    quiver_database_free_string_array(values, count);

    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id2, &values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "review");
    quiver_database_free_string_array(values, count);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetByIdEmpty) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
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

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    quiver_database_close(db);
}

// The set counterpart of ReadVectorGroupByIdPreservesNullCells, over a group with two value
// columns of different types and a NULL in the second one. A set group's row order is consistent
// but unspecified, so each row is located by its code, never by position. An element with no rows
// must come back as all-NULL outputs with zero counts.
TEST(DatabaseCApi, ReadSetGroupByIdPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_column_groups.sql").c_str(), &options, &db),
        QUIVER_OK
    );

    const auto make = [&](const char* collection, const char* label) {
        quiver_element_t* e = nullptr;
        EXPECT_EQ(quiver_element_create(&e), QUIVER_OK);
        quiver_element_set_string(e, "label", label);
        int64_t id = 0;
        EXPECT_EQ(quiver_database_create_element(db, collection, e, &id), QUIVER_OK);
        EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
        return id;
    };
    make("Configuration", "Config");
    const int64_t item = make("Items", "Item 1");
    const int64_t empty_item = make("Items", "Item 2");

    const char* names[] = {"code", "weight"};
    const int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* codes[] = {"alpha", "beta"};
    const double weights[] = {1.5, 0.0};  // slot 1 is masked out and never read
    const uint8_t code_mask[] = {1, 1};
    const uint8_t weight_mask[] = {1, 0};
    const void* data[] = {codes, weights};
    const uint8_t* masks[] = {code_mask, weight_mask};
    ASSERT_EQ(quiver_database_update_set_group(db, "Items", "codes", item, names, types, data, masks, 2, 2), QUIVER_OK);

    char** column_names = nullptr;
    int* column_types = nullptr;
    void** column_data = nullptr;
    uint8_t** column_has_value = nullptr;
    size_t column_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(
        quiver_database_read_set_group_by_id(
            db,
            "Items",
            "codes",
            item,
            &column_names,
            &column_types,
            &column_data,
            &column_has_value,
            &column_count,
            &row_count
        ),
        QUIVER_OK
    );

    ASSERT_EQ(column_count, 2u);
    ASSERT_EQ(row_count, 2u);
    EXPECT_STREQ(column_names[0], "code");
    EXPECT_STREQ(column_names[1], "weight");
    EXPECT_EQ(column_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(column_types[1], QUIVER_DATA_TYPE_FLOAT);

    auto** read_codes = static_cast<char**>(column_data[0]);
    auto* read_weights = static_cast<double*>(column_data[1]);
    ASSERT_NE(read_codes[0], nullptr);
    const size_t alpha = std::string(read_codes[0]) == "alpha" ? 0 : 1;
    const size_t beta = 1 - alpha;
    EXPECT_STREQ(read_codes[alpha], "alpha");
    EXPECT_STREQ(read_codes[beta], "beta");
    EXPECT_EQ(column_has_value[0][alpha], 1);
    EXPECT_EQ(column_has_value[0][beta], 1);
    EXPECT_EQ(column_has_value[1][alpha], 1);
    EXPECT_DOUBLE_EQ(read_weights[alpha], 1.5);
    EXPECT_EQ(column_has_value[1][beta], 0);  // the NULL weight; its data slot is a placeholder

    quiver_database_free_time_series_data(
        column_names, column_types, column_data, column_has_value, column_count, row_count
    );

    // No rows: every out-array is NULL and both counts are 0. The counts are seeded non-zero so
    // the assertion proves the call wrote them.
    column_names = nullptr;
    column_types = nullptr;
    column_data = nullptr;
    column_has_value = nullptr;
    column_count = 99;
    row_count = 99;
    ASSERT_EQ(
        quiver_database_read_set_group_by_id(
            db,
            "Items",
            "codes",
            empty_item,
            &column_names,
            &column_types,
            &column_data,
            &column_has_value,
            &column_count,
            &row_count
        ),
        QUIVER_OK
    );
    EXPECT_EQ(column_count, 0u);
    EXPECT_EQ(row_count, 0u);
    EXPECT_EQ(column_names, nullptr);
    EXPECT_EQ(column_types, nullptr);
    EXPECT_EQ(column_data, nullptr);
    EXPECT_EQ(column_has_value, nullptr);

    EXPECT_EQ(quiver_database_close(db), QUIVER_OK);
}

// ============================================================================
// Read set null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadSetIntegersNullDb) {
    int64_t** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(nullptr, "Collection", "tag", &sets, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetIntegersNullCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    int64_t** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, nullptr, "tag", &sets, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetIntegersNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    int64_t** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, "Collection", "tag", nullptr, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, &masks, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, &masks, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetFloatsNullDb) {
    double** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats(nullptr, "Collection", "tag", &sets, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetStringsNullDb) {
    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(nullptr, "Collection", "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetStringsNullCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, nullptr, "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetStringsNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    char*** sets = nullptr;
    err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Read set by ID null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadSetIntegersByIdNullDb) {
    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers_by_id(nullptr, "Collection", "tag", 1, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetFloatsByIdNullDb) {
    double* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats_by_id(nullptr, "Collection", "tag", 1, &values, &mask, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetStringsByIdNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetStringsByIdNullCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, nullptr, "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetStringsByIdNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", 1, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    char** values = nullptr;
    err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", 1, &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

// ============================================================================
// Gap-fill: Read set integers (using all_types.sql)
// ============================================================================

TEST(DatabaseCApi, ReadSetIntegersHappyPath) {
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
    int64_t int_values[] = {10, 20, 30};
    quiver_element_set_array_integer(update, "code", int_values, 3, nullptr);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    int64_t** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, "AllTypes", "code", &sets, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(sizes[0], 3);

    std::vector<int64_t> set_vals(sets[0], sets[0] + sizes[0]);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_EQ(set_vals[0], 10);
    EXPECT_EQ(set_vals[1], 20);
    EXPECT_EQ(set_vals[2], 30);

    quiver_database_free_integer_vectors(sets, sizes, count);
    quiver_database_free_masks(masks, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetIntegersByIdHappyPath) {
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
    int64_t int_values[] = {100, 200};
    quiver_element_set_array_integer(update, "code", int_values, 2, nullptr);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    int64_t* read_values = nullptr;
    uint8_t* read_mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers_by_id(db, "AllTypes", "code", id, &read_values, &read_mask, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    std::vector<int64_t> sorted(read_values, read_values + count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted[0], 100);
    EXPECT_EQ(sorted[1], 200);

    quiver_database_free_integer_array(read_values);
    quiver_database_free_mask(read_mask);
    quiver_database_close(db);
}

// ============================================================================
// Gap-fill: Read set floats (using all_types.sql)
// ============================================================================

TEST(DatabaseCApi, ReadSetFloatsHappyPath) {
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
    double float_values[] = {1.1, 2.2, 3.3};
    quiver_element_set_array_float(update, "weight", float_values, 3, nullptr);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    double** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats(db, "AllTypes", "weight", &sets, &masks, &sizes, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(sizes[0], 3);

    std::vector<double> set_vals(sets[0], sets[0] + sizes[0]);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_DOUBLE_EQ(set_vals[0], 1.1);
    EXPECT_DOUBLE_EQ(set_vals[1], 2.2);
    EXPECT_DOUBLE_EQ(set_vals[2], 3.3);

    quiver_database_free_float_vectors(sets, sizes, count);
    quiver_database_free_masks(masks, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetFloatsByIdHappyPath) {
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
    double float_values[] = {9.9, 8.8};
    quiver_element_set_array_float(update, "weight", float_values, 2, nullptr);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    double* read_values = nullptr;
    uint8_t* read_mask = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats_by_id(db, "AllTypes", "weight", id, &read_values, &read_mask, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    std::vector<double> sorted(read_values, read_values + count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_DOUBLE_EQ(sorted[0], 8.8);
    EXPECT_DOUBLE_EQ(sorted[1], 9.9);

    quiver_database_free_float_array(read_values);
    quiver_database_free_mask(read_mask);
    quiver_database_close(db);
}

// ============================================================================
// NULL handling in set reads
// ============================================================================

TEST(DatabaseCApi, ReadSetStringsPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
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
    const char* tags[] = {"a", nullptr, "c"};
    quiver_element_set_array_string(e1, "tag", tags, 3, nullptr);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e1, &id1), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e2, &id2), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e2), QUIVER_OK);

    // A NULL cell is a nullptr entry in the inner array - no mask for strings. Set order is
    // unspecified: pin the agreement between the two readers and the content, not the order.
    auto to_cells = [](char** values, size_t n) {
        std::vector<std::optional<std::string>> cells;
        for (size_t i = 0; i < n; ++i) {
            cells.push_back(values[i] ? std::optional<std::string>(values[i]) : std::nullopt);
        }
        return cells;
    };

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count), QUIVER_OK);
    ASSERT_EQ(count, 2);
    auto bulk = to_cells(sets[0], sizes[0]);
    EXPECT_EQ(sizes[1], 0);
    EXPECT_EQ(sets[1], nullptr);
    quiver_database_free_string_vectors(sets, sizes, count);

    char** by_id = nullptr;
    size_t by_id_count = 0;
    ASSERT_EQ(quiver_database_read_set_strings_by_id(db, "Collection", "tag", id1, &by_id, &by_id_count), QUIVER_OK);
    auto cells = to_cells(by_id, by_id_count);
    quiver_database_free_string_array(by_id, by_id_count);

    EXPECT_EQ(bulk, cells);
    std::sort(cells.begin(), cells.end());  // nullopt sorts first
    EXPECT_EQ(cells, (std::vector<std::optional<std::string>>{std::nullopt, "a", "c"}));

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetIntegersPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // score is a nullable INTEGER set column (Child_set_scores).
    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Child 1");
    int64_t scores[] = {7, 0};
    const uint8_t has_value[] = {1, 0};
    quiver_element_set_array_integer(e, "score", scores, 2, has_value);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", e, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    // Set order is unspecified: one cell is masked and the other is 7, whichever slot each is in.
    int64_t** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_set_integers(db, "Child", "score", &sets, &masks, &sizes, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(sizes[0], 2);
    EXPECT_EQ(masks[0][0] + masks[0][1], 1);
    EXPECT_EQ(masks[0][0] ? sets[0][0] : sets[0][1], 7);
    quiver_database_free_integer_vectors(sets, sizes, count);
    quiver_database_free_masks(masks, count);

    int64_t* by_id = nullptr;
    uint8_t* by_id_mask = nullptr;
    size_t by_id_count = 0;
    ASSERT_EQ(
        quiver_database_read_set_integers_by_id(db, "Child", "score", id, &by_id, &by_id_mask, &by_id_count), QUIVER_OK
    );
    ASSERT_EQ(by_id_count, 2);
    EXPECT_EQ(by_id_mask[0] + by_id_mask[1], 1);
    EXPECT_EQ(by_id_mask[0] ? by_id[0] : by_id[1], 7);
    quiver_database_free_integer_array(by_id);
    quiver_database_free_mask(by_id_mask);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetFloatsPreservesNullCells) {
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

    // weight is a nullable REAL set column (Items_set_codes); code stays NULL on both rows.
    quiver_element_t* e = nullptr;
    ASSERT_EQ(quiver_element_create(&e), QUIVER_OK);
    quiver_element_set_string(e, "label", "Item 1");
    double weights[] = {1.5, 0.0};
    const uint8_t has_value[] = {1, 0};
    quiver_element_set_array_float(e, "weight", weights, 2, has_value);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);

    double** sets = nullptr;
    uint8_t** masks = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_set_floats(db, "Items", "weight", &sets, &masks, &sizes, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(sizes[0], 2);
    EXPECT_EQ(masks[0][0] + masks[0][1], 1);
    EXPECT_EQ(masks[0][0] ? sets[0][0] : sets[0][1], 1.5);
    quiver_database_free_float_vectors(sets, sizes, count);
    quiver_database_free_masks(masks, count);

    double* by_id = nullptr;
    uint8_t* by_id_mask = nullptr;
    size_t by_id_count = 0;
    ASSERT_EQ(
        quiver_database_read_set_floats_by_id(db, "Items", "weight", id, &by_id, &by_id_mask, &by_id_count), QUIVER_OK
    );
    ASSERT_EQ(by_id_count, 2);
    EXPECT_EQ(by_id_mask[0] + by_id_mask[1], 1);
    EXPECT_EQ(by_id_mask[0] ? by_id[0] : by_id[1], 1.5);
    quiver_database_free_float_array(by_id);
    quiver_database_free_mask(by_id_mask);

    quiver_database_close(db);
}
