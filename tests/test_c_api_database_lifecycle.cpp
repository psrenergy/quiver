#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

namespace fs = std::filesystem;

class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_test.db").string(); }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenAndClose) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
    EXPECT_EQ(healthy, 1);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenInMemory) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
    EXPECT_EQ(healthy, 1);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenNullPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_open(nullptr, &options, &db), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, DatabasePath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    const char* db_path = nullptr;
    EXPECT_EQ(quiver_database_path(db, &db_path), QUIVER_OK);
    EXPECT_STREQ(db_path, path.c_str());

    quiver_database_close(db);
}

TEST_F(TempFileFixture, DatabasePathInMemory) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    const char* db_path = nullptr;
    EXPECT_EQ(quiver_database_path(db, &db_path), QUIVER_OK);
    EXPECT_STREQ(db_path, ":memory:");

    quiver_database_close(db);
}

TEST_F(TempFileFixture, DatabasePathNullDb) {
    const char* db_path = nullptr;
    EXPECT_EQ(quiver_database_path(nullptr, &db_path), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, IsOpenNullDb) {
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(nullptr, &healthy), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, CloseNullDb) {
    EXPECT_EQ(quiver_database_close(nullptr), QUIVER_OK);
}

TEST_F(TempFileFixture, ErrorStrings) {
    EXPECT_STREQ(quiver_error_string(QUIVER_OK), "Success");
    EXPECT_STREQ(quiver_error_string(QUIVER_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(quiver_error_string(QUIVER_ERROR_DATABASE), "Database error");
    EXPECT_STREQ(quiver_error_string(QUIVER_ERROR_MIGRATION), "Migration error");
    EXPECT_STREQ(quiver_error_string(QUIVER_ERROR_SCHEMA), "Schema validation error");
    EXPECT_STREQ(quiver_error_string(QUIVER_ERROR_NOT_FOUND), "Not found");
    EXPECT_STREQ(quiver_error_string(static_cast<quiver_error_t>(-999)), "Unknown error");
}

TEST_F(TempFileFixture, LogLevelDebug) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_DEBUG;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, LogLevelInfo) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_INFO;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, LogLevelWarn) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_WARN;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, LogLevelError) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_ERROR;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, CreatesFileOnDisk) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(fs::exists(path));

    quiver_database_close(db);
}

TEST_F(TempFileFixture, DefaultOptions) {
    quiver_database_options_t options = {};
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);

    EXPECT_EQ(options.read_only, 0);
    EXPECT_EQ(options.console_level, QUIVER_LOG_INFO);
}

TEST_F(TempFileFixture, DefaultOptionsNullOut) {
    EXPECT_EQ(quiver_database_options_default(nullptr), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, OpenWithNullOptions) {
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", nullptr, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenReadOnly) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);
    quiver_database_close(db);

    options.read_only = 1;
    db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

// ============================================================================
// Current version tests
// ============================================================================

TEST_F(TempFileFixture, CurrentVersionNullDb) {
    int64_t version = 0;
    EXPECT_EQ(quiver_database_current_version(nullptr, &version), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, CurrentVersionValid) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t version = -1;
    EXPECT_EQ(quiver_database_current_version(db, &version), QUIVER_OK);
    EXPECT_EQ(version, 0);

    quiver_database_close(db);
}

// ============================================================================
// From schema error tests
// ============================================================================

TEST_F(TempFileFixture, FromSchemaNullDbPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(nullptr, "schema.sql", &options, &db), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, FromSchemaNullSchemaPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(":memory:", nullptr, &options, &db), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_NE(quiver_database_from_schema(":memory:", "nonexistent/path/schema.sql", &options, &db), QUIVER_OK);
}

// ============================================================================
// From migrations tests
// ============================================================================

TEST_F(TempFileFixture, FromMigrationsNullDbPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_migrations(nullptr, "migrations/", &options, &db), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, FromMigrationsNullMigrationsPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_migrations(":memory:", nullptr, &options, &db), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    // Invalid migrations path returns error
    EXPECT_NE(quiver_database_from_migrations(":memory:", "nonexistent/migrations/", &options, &db), QUIVER_OK);
}

// ============================================================================
// Relation operation tests
// ============================================================================

TEST_F(TempFileFixture, SetScalarRelationNullDb) {
    auto err = quiver_database_set_scalar_relation(nullptr, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, SetScalarRelationNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_set_scalar_relation(db, nullptr, "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullAttribute) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_set_scalar_relation(db, "Child", nullptr, "Child 1", "Parent 1");
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullFromLabel) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_set_scalar_relation(db, "Child", "parent_id", nullptr, "Parent 1");
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationNullToLabel) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_set_scalar_relation(db, "Child", "parent_id", "Child 1", nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, SetScalarRelationValid) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create parent
    quiver_element_t* parent = nullptr;
    ASSERT_EQ(quiver_element_create(&parent), QUIVER_OK);
    quiver_element_set_string(parent, "label", "Parent 1");
    int64_t parent_id = 0;
    quiver_database_create_element(db, "Parent", parent, &parent_id);
    EXPECT_EQ(quiver_element_destroy(parent), QUIVER_OK);

    // Create child
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    int64_t child_id = 0;
    quiver_database_create_element(db, "Child", child, &child_id);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Set relation
    auto err = quiver_database_set_scalar_relation(db, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, QUIVER_OK);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullDb) {
    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_relation(nullptr, "Child", "parent_id", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, ReadScalarRelationNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_relation(db, nullptr, "parent_id", &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullAttribute) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** values = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_scalar_relation(db, "Child", nullptr, &values, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationNullOutput) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_scalar_relation(db, "Child", "parent_id", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    char** values = nullptr;
    err = quiver_database_read_scalar_relation(db, "Child", "parent_id", &values, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, ReadScalarRelationValid) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create parent
    quiver_element_t* parent = nullptr;
    ASSERT_EQ(quiver_element_create(&parent), QUIVER_OK);
    quiver_element_set_string(parent, "label", "Parent 1");
    int64_t parent_id = 0;
    quiver_database_create_element(db, "Parent", parent, &parent_id);
    EXPECT_EQ(quiver_element_destroy(parent), QUIVER_OK);

    // Create child
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    int64_t child_id = 0;
    quiver_database_create_element(db, "Child", child, &child_id);
    EXPECT_EQ(quiver_element_destroy(child), QUIVER_OK);

    // Set relation
    auto err = quiver_database_set_scalar_relation(db, "Child", "parent_id", "Child 1", "Parent 1");
    EXPECT_EQ(err, QUIVER_OK);

    // Read relation
    char** values = nullptr;
    size_t count = 0;
    err = quiver_database_read_scalar_relation(db, "Child", "parent_id", &values, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "Parent 1");

    quiver_free_string_array(values, count);
    quiver_database_close(db);
}

// ============================================================================
// Additional error handling tests
// ============================================================================

TEST_F(TempFileFixture, CreateElementInNonExistentCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Try to create element in non-existent collection - should fail
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Test");
    int64_t id = 0;
    EXPECT_NE(quiver_database_create_element(db, "NonexistentCollection", element, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenReadOnlyNonExistentPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    options.read_only = 1;

    // Try to open non-existent file as read-only
    quiver_database_t* db = nullptr;
    auto err = quiver_database_open("nonexistent_path_12345.db", &options, &db);

    // Should fail because file doesn't exist and we can't create in read-only mode
    EXPECT_NE(err, QUIVER_OK);
}

TEST_F(TempFileFixture, FromSchemaValidPath) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
    EXPECT_EQ(healthy, 1);

    quiver_database_close(db);
}

// ============================================================================
// Element ID operations
// ============================================================================

TEST_F(TempFileFixture, ReadElementIdsNullDb) {
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(nullptr, "Collection", &ids, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, ReadElementIdsNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, nullptr, &ids, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, ReadElementIdsNullOutput) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Collection", nullptr, &count);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    int64_t* ids = nullptr;
    err = quiver_database_read_element_ids(db, "Collection", &ids, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, ReadElementIdsValid) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create some elements
    for (int i = 1; i <= 3; ++i) {
        quiver_element_t* element = nullptr;
        ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
        quiver_element_set_string(element, "label", ("Item " + std::to_string(i)).c_str());
        int64_t elem_id = 0;
        quiver_database_create_element(db, "Collection", element, &elem_id);
        EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    }

    // Read element IDs
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_element_ids(db, "Collection", &ids, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);

    if (ids != nullptr) {
        quiver_free_integer_array(ids);
    }

    quiver_database_close(db);
}

// ============================================================================
// Delete element tests
// ============================================================================

TEST_F(TempFileFixture, DeleteElementNullDb) {
    auto err = quiver_database_delete_element_by_id(nullptr, "Collection", 1);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(TempFileFixture, DeleteElementNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_delete_element_by_id(db, nullptr, 1);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, DeleteElementValid) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create element
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Item 1");
    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Collection", element, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);

    EXPECT_GT(id, 0);

    // Delete element
    auto err = quiver_database_delete_element_by_id(db, "Collection", id);
    EXPECT_EQ(err, QUIVER_OK);

    // Verify element is deleted
    int64_t* ids = nullptr;
    size_t count = 0;
    quiver_database_read_element_ids(db, "Collection", &ids, &count);
    EXPECT_EQ(count, 0);

    if (ids != nullptr) {
        quiver_free_integer_array(ids);
    }

    quiver_database_close(db);
}

// ============================================================================
// Update element tests
// ============================================================================

TEST_F(TempFileFixture, UpdateElementNullDb) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "New Label");

    auto err = quiver_database_update_element(nullptr, "Collection", 1, element);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST_F(TempFileFixture, UpdateElementNullCollection) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "New Label");

    auto err = quiver_database_update_element(db, nullptr, 1, element);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST_F(TempFileFixture, UpdateElementNullElement) {
    quiver_database_options_t options;
    ASSERT_EQ(quiver_database_options_default(&options), QUIVER_OK);
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_update_element(db, "Collection", 1, nullptr);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}
