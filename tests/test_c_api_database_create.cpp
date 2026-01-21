#include "test_utils.h"

#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>

TEST(DatabaseCApi, CreateElementWithScalars) {
    // Test: Use C API to create element with schema
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = margaux_element_create();
    ASSERT_NE(element, nullptr);
    margaux_element_set_string(element, "label", "Config 1");
    margaux_element_set_integer(element, "integer_attribute", 42);
    margaux_element_set_float(element, "float_attribute", 3.14);

    int64_t id = margaux_create_element(db, "Configuration", element);
    EXPECT_EQ(id, 1);

    margaux_element_destroy(element);
    margaux_close(db);
}

TEST(DatabaseCApi, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    auto config = margaux_element_create();
    ASSERT_NE(config, nullptr);
    margaux_element_set_string(config, "label", "Test Config");
    margaux_create_element(db, "Configuration", config);
    margaux_element_destroy(config);

    // Create Collection with vector
    auto element = margaux_element_create();
    ASSERT_NE(element, nullptr);
    margaux_element_set_string(element, "label", "Item 1");

    int64_t values[] = {1, 2, 3};
    margaux_element_set_array_integer(element, "value_int", values, 3);

    int64_t id = margaux_create_element(db, "Collection", element);
    EXPECT_EQ(id, 1);

    margaux_element_destroy(element);
    margaux_close(db);
}

TEST(DatabaseCApi, CreateElementNullDb) {
    auto element = margaux_element_create();
    ASSERT_NE(element, nullptr);
    margaux_element_set_string(element, "label", "Test");

    int64_t id = margaux_create_element(nullptr, "Plant", element);
    EXPECT_EQ(id, -1);

    margaux_element_destroy(element);
}

TEST(DatabaseCApi, CreateElementNullCollection) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    auto element = margaux_element_create();
    ASSERT_NE(element, nullptr);
    margaux_element_set_string(element, "label", "Test");

    int64_t id = margaux_create_element(db, nullptr, element);
    EXPECT_EQ(id, -1);

    margaux_element_destroy(element);
    margaux_close(db);
}

TEST(DatabaseCApi, CreateElementNullElement) {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = margaux_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    int64_t id = margaux_create_element(db, "Plant", nullptr);
    EXPECT_EQ(id, -1);

    margaux_close(db);
}
