#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>
#include <psr/database.h>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

TEST_F(DatabaseFixture, OpenAndClose) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_is_healthy(db), 1);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, OpenInMemory) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_is_healthy(db), 1);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, OpenNullPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(nullptr, &options);

    EXPECT_EQ(db, nullptr);
}

TEST_F(DatabaseFixture, DatabasePath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), path.c_str());

    psr_database_close(db);
}

TEST_F(DatabaseFixture, DatabasePathInMemory) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), ":memory:");

    psr_database_close(db);
}

TEST_F(DatabaseFixture, DatabasePathNullDb) {
    EXPECT_EQ(psr_database_path(nullptr), nullptr);
}

TEST_F(DatabaseFixture, IsOpenNullDb) {
    EXPECT_EQ(psr_database_is_healthy(nullptr), 0);
}

TEST_F(DatabaseFixture, CloseNullDb) {
    psr_database_close(nullptr);
}

TEST_F(DatabaseFixture, ErrorStrings) {
    EXPECT_STREQ(psr_error_string(PSR_OK), "Success");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_DATABASE), "Database error");
}

TEST_F(DatabaseFixture, Version) {
    auto version = psr_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(DatabaseFixture, LogLevelDebug) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_DEBUG;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, LogLevelInfo) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_INFO;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, LogLevelWarn) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_WARN;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, LogLevelError) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_ERROR;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreatesFileOnDisk) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(fs::exists(path));

    psr_database_close(db);
}

TEST_F(DatabaseFixture, DefaultOptions) {
    auto options = psr_database_options_default();

    EXPECT_EQ(options.read_only, 0);
    EXPECT_EQ(options.console_level, PSR_LOG_INFO);
}

TEST_F(DatabaseFixture, OpenWithNullOptions) {
    auto db = psr_database_open(":memory:", nullptr);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, OpenReadOnly) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);
    psr_database_close(db);

    options.read_only = 1;
    db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementWithScalars) {
    // Test: Use C API to create element with schema
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("test_database_schema.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Plant 1");
    psr_element_set_double(element, "capacity", 50.0);

    int64_t id = psr_database_create_element(db, "Plant", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("test_database_schema.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Plant 1");

    // Create array of costs
    double costs[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(element, "costs", costs, 3);

    int64_t id = psr_database_create_element(db, "Plant", element);
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
