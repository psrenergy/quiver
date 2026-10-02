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
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
    EXPECT_EQ(healthy, 1);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenInMemory) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
    EXPECT_EQ(healthy, 1);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenNullPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_open(nullptr, &options, &db), QUIVER_ERROR);
}

TEST_F(TempFileFixture, DatabasePath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    const char* db_path = nullptr;
    EXPECT_EQ(quiver_database_path(db, &db_path), QUIVER_OK);
    EXPECT_STREQ(db_path, path.c_str());

    quiver_database_close(db);
}

TEST_F(TempFileFixture, DatabasePathInMemory) {
    auto options = quiver::test::quiet_options();
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
    EXPECT_EQ(quiver_database_path(nullptr, &db_path), QUIVER_ERROR);
}

TEST_F(TempFileFixture, IsOpenNullDb) {
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(nullptr, &healthy), QUIVER_ERROR);
}

TEST_F(TempFileFixture, CloseNullDb) {
    EXPECT_EQ(quiver_database_close(nullptr), QUIVER_OK);
}

TEST_F(TempFileFixture, LogLevelDebug) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_DEBUG;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, LogLevelInfo) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_INFO;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, LogLevelWarn) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_WARN;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, LogLevelError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_ERROR;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, CreatesFileOnDisk) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(fs::exists(path));

    quiver_database_close(db);
}

TEST_F(TempFileFixture, DefaultOptions) {
    auto options = quiver_database_options_default();

    EXPECT_EQ(options.read_only, 0);
    EXPECT_EQ(options.console_level, QUIVER_LOG_INFO);
}

TEST_F(TempFileFixture, OpenWithNullOptions) {
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", nullptr, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);

    quiver_database_close(db);
}

TEST_F(TempFileFixture, OpenReadOnly) {
    auto options = quiver::test::quiet_options();
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
    EXPECT_EQ(quiver_database_current_version(nullptr, &version), QUIVER_ERROR);
}

TEST_F(TempFileFixture, CurrentVersionValid) {
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(nullptr, "schema.sql", &options, &db), QUIVER_ERROR);
}

TEST_F(TempFileFixture, FromSchemaNullSchemaPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(":memory:", nullptr, &options, &db), QUIVER_ERROR);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_NE(quiver_database_from_schema(":memory:", "nonexistent/path/schema.sql", &options, &db), QUIVER_OK);
}

TEST_F(TempFileFixture, FromSchemaRejectsSetTableWithoutParentFk) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(
        quiver_database_from_schema(":memory:", INVALID_SCHEMA("set_no_parent_fk.sql").c_str(), &options, &db),
        QUIVER_ERROR
    );
    EXPECT_STREQ(
        quiver_get_last_error(),
        "Failed to validate schema: Set table 'Collection_set_tags' must have foreign key to parent "
        "collection 'Collection'"
    );
}

TEST_F(TempFileFixture, FromSchemaRejectsUnsupportedColumnType) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(
        quiver_database_from_schema(":memory:", INVALID_SCHEMA("unsupported_type.sql").c_str(), &options, &db),
        QUIVER_ERROR
    );
    EXPECT_STREQ(
        quiver_get_last_error(),
        "Failed to validate schema: column 'payload' in table 'Items' has unsupported type 'BLOB'"
    );
}

// ============================================================================
// From migrations tests
// ============================================================================

TEST_F(TempFileFixture, FromMigrationsNullDbPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_migrations(nullptr, "migrations/", &options, &db), QUIVER_ERROR);
}

TEST_F(TempFileFixture, FromMigrationsNullMigrationsPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_migrations(":memory:", nullptr, &options, &db), QUIVER_ERROR);
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    // Invalid migrations path returns error
    EXPECT_NE(quiver_database_from_migrations(":memory:", "nonexistent/migrations/", &options, &db), QUIVER_OK);
}

TEST_F(TempFileFixture, FromMigrationsSetsCurrentVersion) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_migrations(":memory:", SCHEMA_PATH("schemas/migrations").c_str(), &options, &db), QUIVER_OK
    ) << quiver_get_last_error();
    ASSERT_NE(db, nullptr);

    int64_t version = -1;
    EXPECT_EQ(quiver_database_current_version(db, &version), QUIVER_OK);
    EXPECT_EQ(version, 3);

    quiver_database_close(db);
}

// ============================================================================
// Migration round-trip tests
// ============================================================================

TEST_F(TempFileFixture, ValidateMigrationsSucceeds) {
    EXPECT_EQ(quiver_database_validate_migrations(SCHEMA_PATH("schemas/migrations").c_str()), QUIVER_OK);
}

TEST_F(TempFileFixture, ValidateMigrationsPropagatesFailure) {
    EXPECT_EQ(quiver_database_validate_migrations("nonexistent/migrations"), QUIVER_ERROR);
    EXPECT_STREQ(
        quiver_get_last_error(), "Cannot validate_migrations: migrations path not found: nonexistent/migrations"
    );
}

TEST_F(TempFileFixture, ValidateMigrationsRejectsNullPath) {
    EXPECT_EQ(quiver_database_validate_migrations(nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: migrations_path");
}

// ============================================================================
// Additional error handling tests
// ============================================================================

TEST_F(TempFileFixture, CreateElementInNonExistentCollection) {
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
    options.read_only = 1;

    // Try to open non-existent file as read-only
    quiver_database_t* db = nullptr;
    auto err = quiver_database_open("nonexistent_path_12345.db", &options, &db);

    // Should fail because file doesn't exist and we can't create in read-only mode
    EXPECT_NE(err, QUIVER_OK);
}

TEST_F(TempFileFixture, FromSchemaValidPath) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);

    ASSERT_NE(db, nullptr);
    int healthy = 0;
    EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
    EXPECT_EQ(healthy, 1);

    quiver_database_close(db);
}
