#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
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
    const char* tags1[] = {"important", "urgent"};
    quiver_element_set_array_string(e1, "tag", tags1, 2);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", e1, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    quiver_element_set_array_string(e2, "tag", tags2, 1);
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
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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

TEST(DatabaseCApi, ReadSetOnlyReturnsElementsWithData) {
    auto options = quiver::test::quiet_options();
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

    // Element with set data
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important"};
    quiver_element_set_array_string(e1, "tag", tags1, 1);
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
    quiver_element_set_array_string(e3, "tag", tags3, 2);
    int64_t tmp_id4 = 0;
    quiver_database_create_element(db, "Collection", e3, &tmp_id4);
    EXPECT_EQ(quiver_element_destroy(e3), QUIVER_OK);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    // Only elements with set data are returned
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 1);
    EXPECT_EQ(sizes[1], 2);

    quiver_database_free_string_vectors(sets, sizes, count);
    quiver_database_close(db);
}

// ============================================================================
// Read set by Id tests
// ============================================================================

TEST(DatabaseCApi, ReadSetStringById) {
    auto options = quiver::test::quiet_options();
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
    const char* tags1[] = {"important", "urgent"};
    quiver_element_set_array_string(e1, "tag", tags1, 2);
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    quiver_element_set_array_string(e2, "tag", tags2, 1);
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

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_strings_by_id(db, "Collection", "tag", id, &values, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Read set null pointer tests
// ============================================================================

TEST(DatabaseCApi, ReadSetIntegersNullDb) {
    int64_t** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(nullptr, "Collection", "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetIntegersNullCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, nullptr, "tag", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetIntegersNullOutput) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, "Collection", "tag", nullptr, &sizes, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    int64_t** sets = nullptr;
    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_read_set_integers(db, "Collection", "tag", &sets, &sizes, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadSetFloatsNullDb) {
    double** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats(nullptr, "Collection", "tag", &sets, &sizes, &count);
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
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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
    size_t count = 0;
    auto err = quiver_database_read_set_integers_by_id(nullptr, "Collection", "tag", 1, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
}

TEST(DatabaseCApi, ReadSetFloatsByIdNullDb) {
    double* values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats_by_id(nullptr, "Collection", "tag", 1, &values, &count);
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
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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
    quiver_element_set_array_integer(update, "code", int_values, 3);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    int64_t** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers(db, "AllTypes", "code", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(sizes[0], 3);

    std::vector<int64_t> set_vals(sets[0], sets[0] + sizes[0]);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_EQ(set_vals[0], 10);
    EXPECT_EQ(set_vals[1], 20);
    EXPECT_EQ(set_vals[2], 30);

    quiver_database_free_integer_vectors(sets, sizes, count);
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
    quiver_element_set_array_integer(update, "code", int_values, 2);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_integers_by_id(db, "AllTypes", "code", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    std::vector<int64_t> sorted(read_values, read_values + count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted[0], 100);
    EXPECT_EQ(sorted[1], 200);

    quiver_database_free_integer_array(read_values);
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
    quiver_element_set_array_float(update, "weight", float_values, 3);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    double** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats(db, "AllTypes", "weight", &sets, &sizes, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(sizes[0], 3);

    std::vector<double> set_vals(sets[0], sets[0] + sizes[0]);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_DOUBLE_EQ(set_vals[0], 1.1);
    EXPECT_DOUBLE_EQ(set_vals[1], 2.2);
    EXPECT_DOUBLE_EQ(set_vals[2], 3.3);

    quiver_database_free_float_vectors(sets, sizes, count);
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
    quiver_element_set_array_float(update, "weight", float_values, 2);
    ASSERT_EQ(quiver_database_update_element(db, "AllTypes", id, update), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    double* read_values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_set_floats_by_id(db, "AllTypes", "weight", id, &read_values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    std::vector<double> sorted(read_values, read_values + count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_DOUBLE_EQ(sorted[0], 8.8);
    EXPECT_DOUBLE_EQ(sorted[1], 9.9);

    quiver_database_free_float_array(read_values);
    quiver_database_close(db);
}
