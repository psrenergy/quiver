#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "quiver_test.db").string();
    }
    void TearDown() override {
        if (fs::exists(path)) {
            fs::remove(path);
        }
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenFileOnDisk) {
    quiver::Database db(path);
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), path);
}

TEST_F(TempFileFixture, OpenInMemory) {
    quiver::Database db(":memory:");
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), ":memory:");
}

TEST_F(TempFileFixture, DestructorClosesDatabase) {
    {
        quiver::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    // Database should be closed after destructor
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(TempFileFixture, MoveConstructor) {
    quiver::Database db1(path);
    EXPECT_TRUE(db1.is_healthy());

    quiver::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(TempFileFixture, MoveAssignment) {
    quiver::Database db1(path);
    quiver::Database db2(":memory:");

    db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(TempFileFixture, LogLevelDebug) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Debug});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(TempFileFixture, LogLevelOff) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(TempFileFixture, CreatesFileOnDisk) {
    {
        quiver::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(TempFileFixture, CurrentVersion) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});
    EXPECT_EQ(db.current_version(), 0);
}

// ============================================================================
// Schema error tests
// ============================================================================

TEST_F(TempFileFixture, FromSchemaFileNotFound) {
    EXPECT_THROW(
        quiver::Database::from_schema(
            ":memory:",
            "nonexistent/path/schema.sql",
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        ),
        std::runtime_error
    );
}

TEST_F(TempFileFixture, FromSchemaEmptyFile) {
    const auto schema_path = (fs::temp_directory_path() / "quiver_empty_schema.sql").string();
    std::ofstream(schema_path).close();

    try {
        quiver::Database::from_schema(
            ":memory:",
            schema_path,
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        );
        ADD_FAILURE() << "Expected from_schema to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()), "Cannot from_schema: schema file is empty: " + schema_path);
    }
    fs::remove(schema_path);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    EXPECT_THROW(
        quiver::Database::from_schema(":memory:", "", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error
    );
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    EXPECT_THROW(
        quiver::Database::from_migrations(
            ":memory:",
            "nonexistent/migrations/",
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        ),
        std::runtime_error
    );
}

// ============================================================================
// Migration tests
// ============================================================================

class MigrationFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "quiver_test.db").string();
        migrations_path = (fs::path(__FILE__).parent_path() / "schemas" / "migrations").string();
    }
    void TearDown() override {
        if (fs::exists(path)) {
            fs::remove(path);
        }
    }
    std::string path;
    std::string migrations_path;
};

// ============================================================================
// Database migration tests
// ============================================================================

TEST_F(MigrationFixture, DatabaseCurrentVersion) {
    quiver::Database db(":memory:");
    EXPECT_EQ(db.current_version(), 0);
}

TEST_F(MigrationFixture, DatabaseFromMigrations) {
    auto db = quiver::Database::from_migrations(path, migrations_path);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(MigrationFixture, MigrationsPendingFromHigherVersion) {
    // A database past the latest migration (3) has nothing pending, so nothing is applied
    quiver::test::exec_sql(path, "PRAGMA user_version = 100;");

    auto db = quiver::Database::from_migrations(path, migrations_path);
    EXPECT_EQ(db.current_version(), 100);
}

TEST_F(MigrationFixture, DatabaseFromMigrationsInvalidPath) {
    EXPECT_THROW(quiver::Database::from_migrations(path, "nonexistent/migrations/"), std::runtime_error);
}

TEST_F(MigrationFixture, MigrationsWithPartialApplication) {
    // A database at version 1 gets only migrations 2 and 3: re-running 1/up.sql would fail on CREATE TABLE
    const auto first_only = fs::temp_directory_path() / "quiver_first_migration_only";
    fs::remove_all(first_only);
    fs::create_directories(first_only / "1");
    fs::copy(fs::path(migrations_path) / "1", first_only / "1");
    {
        auto db = quiver::Database::from_migrations(path, first_only.string());
        EXPECT_EQ(db.current_version(), 1);
    }
    fs::remove_all(first_only);

    {
        auto db = quiver::Database::from_migrations(path, migrations_path);
        EXPECT_EQ(db.current_version(), 3);
    }

    // Reopen the database and verify it still has version 3
    {
        quiver::Database db(path);
        EXPECT_EQ(db.current_version(), 3);
    }
}

TEST_F(MigrationFixture, DatabaseFromMigrationsMemory) {
    auto db = quiver::Database::from_migrations(":memory:", migrations_path);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

// ============================================================================
// Schema loading after migrations tests
// ============================================================================

TEST_F(MigrationFixture, FromMigrationsLoadsSchemaMetadata) {
    auto db = quiver::Database::from_migrations(":memory:", migrations_path);

    // list_scalar_attributes requires schema to be loaded
    auto attributes = db.list_scalar_attributes("Test1");
    EXPECT_FALSE(attributes.empty());

    // Verify expected columns exist
    bool has_id = false, has_label = false, has_name = false;
    for (const auto& attribute : attributes) {
        if (attribute.name == "id") {
            has_id = true;
        }
        if (attribute.name == "label") {
            has_label = true;
        }
        if (attribute.name == "name") {
            has_name = true;
        }
    }
    EXPECT_TRUE(has_id);
    EXPECT_TRUE(has_label);
    EXPECT_TRUE(has_name);
}

TEST_F(MigrationFixture, FromMigrationsAllowsCreateElement) {
    auto db = quiver::Database::from_migrations(":memory:", migrations_path);

    // create_element requires the schema to be loaded
    auto id = db.create_element("Test1", quiver::Element().set("label", "item1").set("name", "Test Item"));
    EXPECT_GT(id, 0);

    // Verify element was created
    auto names = db.read_scalar_strings("Test1", "name");
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "Test Item");
}

TEST_F(MigrationFixture, FromMigrationsAllowsCreateElementInLaterMigration) {
    auto db = quiver::Database::from_migrations(":memory:", migrations_path);

    // Test3 is created in migration 3
    auto id = db.create_element("Test3", quiver::Element().set("label", "item1").set("capacity", int64_t{100}));
    EXPECT_GT(id, 0);

    auto capacities = db.read_scalar_integers("Test3", "capacity");
    ASSERT_EQ(capacities.size(), 1u);
    EXPECT_EQ(capacities[0], 100);
}

TEST_F(MigrationFixture, FromMigrationsLoadsSchemaWhenAlreadyUpToDate) {
    // First, apply all migrations
    {
        auto db = quiver::Database::from_migrations(path, migrations_path);
        EXPECT_EQ(db.current_version(), 3);
    }

    // Reopen with from_migrations again (no pending migrations)
    auto db = quiver::Database::from_migrations(path, migrations_path);

    // Schema should be loaded even though no migrations were applied
    auto attributes = db.list_scalar_attributes("Test3");
    EXPECT_FALSE(attributes.empty());

    // Verify we can create elements
    auto id = db.create_element("Test3", quiver::Element().set("label", "item1").set("capacity", int64_t{42}));
    EXPECT_GT(id, 0);
}

// The constructor - the only implementation behind quiver_database_open and every binding's
// open() - does not read the schema; require_schema loads it on first use. Without that, an
// opened database answered every metadata and CRUD call with "no schema loaded".
TEST_F(TempFileFixture, OpenExistingDatabaseLoadsSchemaOnFirstUse) {
    {
        auto created = quiver::Database::from_schema(
            path,
            VALID_SCHEMA("collections.sql"),
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        );
        created.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
        created.create_element(
            "Collection",
            quiver::Element().set("label", std::string("Item 1")).set("some_integer", int64_t{42})
        );
    }

    quiver::Database db(path, {.read_only = false, .console_level = quiver::LogLevel::Off});
    EXPECT_EQ(db.read_scalar_integers("Collection", "some_integer"), (std::vector<std::optional<int64_t>>{42}));
    EXPECT_FALSE(db.list_vector_groups("Collection").empty());
    EXPECT_NO_THROW(db.create_element("Collection", quiver::Element().set("label", std::string("Item 2"))));
}

// Read-only open must reach the same schema (Schema::from_database is read-only SQL), and fail
// the write for the right reason rather than for a missing schema.
TEST_F(TempFileFixture, OpenReadOnlyLoadsSchemaOnFirstUse) {
    {
        auto created = quiver::Database::from_schema(
            path,
            VALID_SCHEMA("collections.sql"),
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        );
        created.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
    }

    quiver::Database db(path, {.read_only = true, .console_level = quiver::LogLevel::Off});
    EXPECT_EQ(db.list_scalar_attributes("Configuration").size(), 2u);
    EXPECT_THROW(
        db.create_element("Configuration", quiver::Element().set("label", std::string("Nope"))),
        std::runtime_error
    );
}

// A directory with no versioned subdirectories is not an error: it holds no migrations, and
// from_migrations returns a usable handle. Loading (and validating) the schema eagerly here would
// turn an empty database into a "Schema must have a 'Configuration' table" throw at open.
TEST_F(TempFileFixture, FromMigrationsWithNoVersionsReturnsHandle) {
    auto empty_dir = fs::temp_directory_path() / "quiver_empty_migrations";
    fs::create_directories(empty_dir);

    auto db = quiver::Database::from_migrations(
        path,
        empty_dir.string(),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.current_version(), 0);

    fs::remove_all(empty_dir);
}
