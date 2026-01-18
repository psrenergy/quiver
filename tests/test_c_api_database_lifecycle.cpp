#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>

namespace fs = std::filesystem;

class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "psr_test.db").string(); }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenAndClose) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_is_healthy(db), 1);

    psr_database_close(db);
}

TEST_F(TempFileFixture, OpenInMemory) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_is_healthy(db), 1);

    psr_database_close(db);
}

TEST_F(TempFileFixture, OpenNullPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(nullptr, &options);

    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, DatabasePath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), path.c_str());

    psr_database_close(db);
}

TEST_F(TempFileFixture, DatabasePathInMemory) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), ":memory:");

    psr_database_close(db);
}

TEST_F(TempFileFixture, DatabasePathNullDb) {
    EXPECT_EQ(psr_database_path(nullptr), nullptr);
}

TEST_F(TempFileFixture, IsOpenNullDb) {
    EXPECT_EQ(psr_database_is_healthy(nullptr), 0);
}

TEST_F(TempFileFixture, CloseNullDb) {
    psr_database_close(nullptr);
}

TEST_F(TempFileFixture, ErrorStrings) {
    EXPECT_STREQ(psr_error_string(PSR_OK), "Success");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_DATABASE), "Database error");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_MIGRATION), "Migration error");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_SCHEMA), "Schema validation error");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_CREATE_ELEMENT), "Failed to create element");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_NOT_FOUND), "Not found");
    EXPECT_STREQ(psr_error_string(static_cast<psr_error_t>(-999)), "Unknown error");
}

TEST_F(TempFileFixture, Version) {
    auto version = psr_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(TempFileFixture, LogLevelDebug) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_DEBUG;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(TempFileFixture, LogLevelInfo) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_INFO;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(TempFileFixture, LogLevelWarn) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_WARN;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(TempFileFixture, LogLevelError) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_ERROR;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(TempFileFixture, CreatesFileOnDisk) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(fs::exists(path));

    psr_database_close(db);
}

TEST_F(TempFileFixture, DefaultOptions) {
    auto options = psr_database_options_default();

    EXPECT_EQ(options.read_only, 0);
    EXPECT_EQ(options.console_level, PSR_LOG_INFO);
}

TEST_F(TempFileFixture, OpenWithNullOptions) {
    auto db = psr_database_open(":memory:", nullptr);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(TempFileFixture, OpenReadOnly) {
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

// ============================================================================
// Current version tests
// ============================================================================

TEST_F(TempFileFixture, CurrentVersionNullDb) {
    auto version = psr_database_current_version(nullptr);
    EXPECT_EQ(version, -1);
}

TEST_F(TempFileFixture, CurrentVersionValid) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    auto version = psr_database_current_version(db);
    EXPECT_EQ(version, 0);

    psr_database_close(db);
}

// ============================================================================
// From schema error tests
// ============================================================================

TEST_F(TempFileFixture, FromSchemaNullDbPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(nullptr, "schema.sql", &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromSchemaNullSchemaPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", nullptr, &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", "nonexistent/path/schema.sql", &options);
    EXPECT_EQ(db, nullptr);
}

// ============================================================================
// From migrations tests
// ============================================================================

TEST_F(TempFileFixture, FromMigrationsNullDbPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_migrations(nullptr, "migrations/", &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromMigrationsNullMigrationsPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_migrations(":memory:", nullptr, &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_migrations(":memory:", "nonexistent/migrations/", &options);
    // Invalid migrations path results in database with version 0 (no migrations applied)
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_current_version(db), 0);
    psr_database_close(db);
}

// ============================================================================
// Relation operation tests
// ============================================================================

TEST_F(TempFileFixture, SetScalarRelationNullDb) {
    auto err = psr_database_set_scalar_relation(nullptr, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, SetScalarRelationNullCollection) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = psr_database_set_scalar_relation(db, nullptr, "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullAttribute) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = psr_database_set_scalar_relation(db, "Child", nullptr, "Child 1", "Parent 1");
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullFromLabel) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = psr_database_set_scalar_relation(db, "Child", "parent_id", nullptr, "Parent 1");
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullToLabel) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = psr_database_set_scalar_relation(db, "Child", "parent_id", "Child 1", nullptr);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationValid) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create parent
    auto parent = psr_element_create();
    psr_element_set_string(parent, "label", "Parent 1");
    psr_database_create_element(db, "Parent", parent);
    psr_element_destroy(parent);

    // Create child
    auto child = psr_element_create();
    psr_element_set_string(child, "label", "Child 1");
    psr_database_create_element(db, "Child", child);
    psr_element_destroy(child);

    // Set relation
    auto err = psr_database_set_scalar_relation(db, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, PSR_OK);

    psr_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_relation(nullptr, "Child", "parent_id", &values, &count);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, ReadScalarRelationNullCollection) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_relation(db, nullptr, "parent_id", &values, &count);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullAttribute) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_relation(db, "Child", nullptr, &values, &count);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullOutput) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = psr_database_read_scalar_relation(db, "Child", "parent_id", nullptr, &count);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    char** values = nullptr;
    err = psr_database_read_scalar_relation(db, "Child", "parent_id", &values, nullptr);
    EXPECT_EQ(err, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationValid) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create parent
    auto parent = psr_element_create();
    psr_element_set_string(parent, "label", "Parent 1");
    psr_database_create_element(db, "Parent", parent);
    psr_element_destroy(parent);

    // Create child
    auto child = psr_element_create();
    psr_element_set_string(child, "label", "Child 1");
    psr_database_create_element(db, "Child", child);
    psr_element_destroy(child);

    // Set relation
    auto err = psr_database_set_scalar_relation(db, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, PSR_OK);

    // Read relation
    char** values = nullptr;
    size_t count = 0;
    err = psr_database_read_scalar_relation(db, "Child", "parent_id", &values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "Parent 1");

    psr_free_string_array(values, count);
    psr_database_close(db);
}
