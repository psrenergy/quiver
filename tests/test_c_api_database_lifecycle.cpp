#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <margaux/c/database.h>
#include <margaux/c/element.h>

namespace fs = std::filesystem;

class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "margaux_test.db").string(); }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenAndClose) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(database_is_healthy(db), 1);

    database_close(db);
}

TEST_F(TempFileFixture, OpenInMemory) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(database_is_healthy(db), 1);

    database_close(db);
}

TEST_F(TempFileFixture, OpenNullPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(nullptr, &options);

    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, DatabasePath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(database_path(db), path.c_str());

    database_close(db);
}

TEST_F(TempFileFixture, DatabasePathInMemory) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(database_path(db), ":memory:");

    database_close(db);
}

TEST_F(TempFileFixture, DatabasePathNullDb) {
    EXPECT_EQ(database_path(nullptr), nullptr);
}

TEST_F(TempFileFixture, IsOpenNullDb) {
    EXPECT_EQ(database_is_healthy(nullptr), 0);
}

TEST_F(TempFileFixture, CloseNullDb) {
    database_close(nullptr);
}

TEST_F(TempFileFixture, ErrorStrings) {
    EXPECT_STREQ(margaux_error_string(MARGAUX_OK), "Success");
    EXPECT_STREQ(margaux_error_string(MARGAUX_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(margaux_error_string(MARGAUX_ERROR_DATABASE), "Database error");
    EXPECT_STREQ(margaux_error_string(MARGAUX_ERROR_MIGRATION), "Migration error");
    EXPECT_STREQ(margaux_error_string(MARGAUX_ERROR_SCHEMA), "Schema validation error");
    EXPECT_STREQ(margaux_error_string(MARGAUX_ERROR_CREATE_ELEMENT), "Failed to create element");
    EXPECT_STREQ(margaux_error_string(MARGAUX_ERROR_NOT_FOUND), "Not found");
    EXPECT_STREQ(margaux_error_string(static_cast<margaux_error_t>(-999)), "Unknown error");
}

TEST_F(TempFileFixture, Version) {
    auto version = margaux_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(TempFileFixture, LogLevelDebug) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_DEBUG;
    auto db = database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    database_close(db);
}

TEST_F(TempFileFixture, LogLevelInfo) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_INFO;
    auto db = database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    database_close(db);
}

TEST_F(TempFileFixture, LogLevelWarn) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_WARN;
    auto db = database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    database_close(db);
}

TEST_F(TempFileFixture, LogLevelError) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_ERROR;
    auto db = database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    database_close(db);
}

TEST_F(TempFileFixture, CreatesFileOnDisk) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(fs::exists(path));

    database_close(db);
}

TEST_F(TempFileFixture, DefaultOptions) {
    auto options = database_options_default();

    EXPECT_EQ(options.read_only, 0);
    EXPECT_EQ(options.console_level, MARGAUX_LOG_INFO);
}

TEST_F(TempFileFixture, OpenWithNullOptions) {
    auto db = database_open(":memory:", nullptr);

    ASSERT_NE(db, nullptr);

    database_close(db);
}

TEST_F(TempFileFixture, OpenReadOnly) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);
    database_close(db);

    options.read_only = 1;
    db = database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);

    database_close(db);
}

// ============================================================================
// Current version tests
// ============================================================================

TEST_F(TempFileFixture, CurrentVersionNullDb) {
    auto version = database_current_version(nullptr);
    EXPECT_EQ(version, -1);
}

TEST_F(TempFileFixture, CurrentVersionValid) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    auto version = database_current_version(db);
    EXPECT_EQ(version, 0);

    database_close(db);
}

// ============================================================================
// From schema error tests
// ============================================================================

TEST_F(TempFileFixture, FromSchemaNullDbPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(nullptr, "schema.sql", &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromSchemaNullSchemaPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", nullptr, &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", "nonexistent/path/schema.sql", &options);
    EXPECT_EQ(db, nullptr);
}

// ============================================================================
// From migrations tests
// ============================================================================

TEST_F(TempFileFixture, FromMigrationsNullDbPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_migrations(nullptr, "migrations/", &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromMigrationsNullMigrationsPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_migrations(":memory:", nullptr, &options);
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_migrations(":memory:", "nonexistent/migrations/", &options);
    // Invalid migrations path results in database with version 0 (no migrations applied)
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(database_current_version(db), 0);
    database_close(db);
}

// ============================================================================
// Relation operation tests
// ============================================================================

TEST_F(TempFileFixture, SetScalarRelationNullDb) {
    auto err = database_set_scalar_relation(nullptr, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, SetScalarRelationNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_set_scalar_relation(db, nullptr, "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullAttribute) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_set_scalar_relation(db, "Child", nullptr, "Child 1", "Parent 1");
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullFromLabel) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_set_scalar_relation(db, "Child", "parent_id", nullptr, "Parent 1");
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullToLabel) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_set_scalar_relation(db, "Child", "parent_id", "Child 1", nullptr);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationValid) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create parent
    auto parent = element_create();
    element_set_string(parent, "label", "Parent 1");
    database_create_element(db, "Parent", parent);
    element_destroy(parent);

    // Create child
    auto child = element_create();
    element_set_string(child, "label", "Child 1");
    database_create_element(db, "Child", child);
    element_destroy(child);

    // Set relation
    auto err = database_set_scalar_relation(db, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, MARGAUX_OK);

    database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = database_read_scalar_relation(nullptr, "Child", "parent_id", &values, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, ReadScalarRelationNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = database_read_scalar_relation(db, nullptr, "parent_id", &values, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullAttribute) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = database_read_scalar_relation(db, "Child", nullptr, &values, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullOutput) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = database_read_scalar_relation(db, "Child", "parent_id", nullptr, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    char** values = nullptr;
    err = database_read_scalar_relation(db, "Child", "parent_id", &values, nullptr);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationValid) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create parent
    auto parent = element_create();
    element_set_string(parent, "label", "Parent 1");
    database_create_element(db, "Parent", parent);
    element_destroy(parent);

    // Create child
    auto child = element_create();
    element_set_string(child, "label", "Child 1");
    database_create_element(db, "Child", child);
    element_destroy(child);

    // Set relation
    auto err = database_set_scalar_relation(db, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, MARGAUX_OK);

    // Read relation
    char** values = nullptr;
    size_t count = 0;
    err = database_read_scalar_relation(db, "Child", "parent_id", &values, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "Parent 1");

    margaux_free_string_array(values, count);
    database_close(db);
}

// ============================================================================
// Additional error handling tests
// ============================================================================

TEST_F(TempFileFixture, CreateElementInNonExistentCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Try to create element in non-existent collection - should fail
    auto element = element_create();
    element_set_string(element, "label", "Test");
    auto id = database_create_element(db, "NonexistentCollection", element);
    element_destroy(element);

    EXPECT_EQ(id, -1);

    database_close(db);
}

TEST_F(TempFileFixture, OpenReadOnlyNonExistentPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    options.read_only = 1;

    // Try to open non-existent file as read-only
    auto db = database_open("nonexistent_path_12345.db", &options);

    // Should fail because file doesn't exist and we can't create in read-only mode
    EXPECT_EQ(db, nullptr);
}

TEST_F(TempFileFixture, FromSchemaValidPath) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(database_is_healthy(db), 1);

    database_close(db);
}

// ============================================================================
// Element ID operations
// ============================================================================

TEST_F(TempFileFixture, ReadElementIdsNullDb) {
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = database_read_element_ids(nullptr, "Collection", &ids, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, ReadElementIdsNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = database_read_element_ids(db, nullptr, &ids, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, ReadElementIdsNullOutput) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = database_read_element_ids(db, "Collection", nullptr, &count);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    int64_t* ids = nullptr;
    err = database_read_element_ids(db, "Collection", &ids, nullptr);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, ReadElementIdsValid) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    auto config = element_create();
    element_set_string(config, "label", "Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    // Create some elements
    for (int i = 1; i <= 3; ++i) {
        auto element = element_create();
        element_set_string(element, "label", ("Item " + std::to_string(i)).c_str());
        database_create_element(db, "Collection", element);
        element_destroy(element);
    }

    // Read element IDs
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = database_read_element_ids(db, "Collection", &ids, &count);
    EXPECT_EQ(err, MARGAUX_OK);
    EXPECT_EQ(count, 3);

    if (ids != nullptr) {
        free(ids);
    }

    database_close(db);
}

// ============================================================================
// Delete element tests
// ============================================================================

TEST_F(TempFileFixture, DeleteElementNullDb) {
    auto err = database_delete_element_by_id(nullptr, "Collection", 1);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, DeleteElementNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_delete_element_by_id(db, nullptr, 1);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}

TEST_F(TempFileFixture, DeleteElementValid) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    auto config = element_create();
    element_set_string(config, "label", "Config");
    database_create_element(db, "Configuration", config);
    element_destroy(config);

    // Create element
    auto element = element_create();
    element_set_string(element, "label", "Item 1");
    int64_t id = database_create_element(db, "Collection", element);
    element_destroy(element);

    EXPECT_GT(id, 0);

    // Delete element
    auto err = database_delete_element_by_id(db, "Collection", id);
    EXPECT_EQ(err, MARGAUX_OK);

    // Verify element is deleted
    int64_t* ids = nullptr;
    size_t count = 0;
    database_read_element_ids(db, "Collection", &ids, &count);
    EXPECT_EQ(count, 0);

    if (ids != nullptr) {
        free(ids);
    }

    database_close(db);
}

// ============================================================================
// Update element tests
// ============================================================================

TEST_F(TempFileFixture, UpdateElementNullDb) {
    auto element = element_create();
    element_set_string(element, "label", "New Label");

    auto err = database_update_element(nullptr, "Collection", 1, element);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    element_destroy(element);
}

TEST_F(TempFileFixture, UpdateElementNullCollection) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = element_create();
    element_set_string(element, "label", "New Label");

    auto err = database_update_element(db, nullptr, 1, element);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    element_destroy(element);
    database_close(db);
}

TEST_F(TempFileFixture, UpdateElementNullElement) {
    auto options = database_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    auto db = database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto err = database_update_element(db, "Collection", 1, nullptr);
    EXPECT_EQ(err, MARGAUX_ERROR_INVALID_ARGUMENT);

    database_close(db);
}
