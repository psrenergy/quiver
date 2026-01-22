#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

TEST(DatabaseCApi, CreateElementWithScalars) {
    // Test: Use C API to create element with schema
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Config 1");
    quiver_element_set_integer(element, "integer_attribute", 42);
    quiver_element_set_float(element, "float_attribute", 3.14);

    int64_t id = quiver_database_create_element(db, "Configuration", element);
    EXPECT_EQ(id, 1);

    quiver_element_destroy(element);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    auto config = quiver_element_create();
    ASSERT_NE(config, nullptr);
    quiver_element_set_string(config, "label", "Test Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    // Create Collection with vector
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Item 1");

    int64_t values[] = {1, 2, 3};
    quiver_element_set_array_integer(element, "value_int", values, 3);

    int64_t id = quiver_database_create_element(db, "Collection", element);
    EXPECT_EQ(id, 1);

    quiver_element_destroy(element);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNullDb) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Test");

    int64_t id = quiver_database_create_element(nullptr, "Plant", element);
    EXPECT_EQ(id, -1);

    quiver_element_destroy(element);
}

TEST(DatabaseCApi, CreateElementNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Test");

    int64_t id = quiver_database_create_element(db, nullptr, element);
    EXPECT_EQ(id, -1);

    quiver_element_destroy(element);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNullElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    int64_t id = quiver_database_create_element(db, "Plant", nullptr);
    EXPECT_EQ(id, -1);

    quiver_database_close(db);
}
