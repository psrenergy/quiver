#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

TEST_F(DatabaseFixture, CreateElementWithScalars) {
    // Test: Use C API to create element with schema
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Config 1");
    psr_element_set_integer(element, "integer_attribute", 42);
    psr_element_set_double(element, "float_attribute", 3.14);

    int64_t id = psr_database_create_element(db, "Configuration", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    auto config = psr_element_create();
    ASSERT_NE(config, nullptr);
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // Create Collection with vector
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Item 1");

    int64_t values[] = {1, 2, 3};
    psr_element_set_array_int(element, "value_int", values, 3);

    int64_t id = psr_database_create_element(db, "Collection", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementNullDb) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Test");

    int64_t id = psr_database_create_element(nullptr, "Plant", element);
    EXPECT_EQ(id, -1);

    psr_element_destroy(element);
}

TEST_F(DatabaseFixture, CreateElementNullCollection) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Test");

    int64_t id = psr_database_create_element(db, nullptr, element);
    EXPECT_EQ(id, -1);

    psr_element_destroy(element);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementNullElement) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    int64_t id = psr_database_create_element(db, "Plant", nullptr);
    EXPECT_EQ(id, -1);

    psr_database_close(db);
}
