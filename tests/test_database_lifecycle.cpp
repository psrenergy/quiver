#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/migration.h>
#include <quiver/migrations.h>
#include <string>

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
            ":memory:", "nonexistent/path/schema.sql", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    EXPECT_THROW(
        quiver::Database::from_schema(":memory:", "", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error);
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    EXPECT_THROW(
        quiver::Database::from_migrations(
            ":memory:", "nonexistent/migrations/", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error);
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
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
    std::string migrations_path;
};

// ============================================================================
// Migration class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationCreation) {
    quiver::Migration migration(1, migrations_path + "/1");
    EXPECT_EQ(migration.version(), 1);
    EXPECT_FALSE(migration.path().empty());
}

TEST_F(MigrationFixture, MigrationUpSqlRead) {
    quiver::Migration migration(1, migrations_path + "/1");
    std::string sql = migration.up_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("CREATE TABLE Test1"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationDownSqlRead) {
    quiver::Migration migration(1, migrations_path + "/1");
    std::string sql = migration.down_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("DROP TABLE"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationComparison) {
    quiver::Migration m1(1, migrations_path + "/1");
    quiver::Migration m2(2, migrations_path + "/2");
    quiver::Migration m3(3, migrations_path + "/3");

    EXPECT_TRUE(m1 < m2);
    EXPECT_TRUE(m2 < m3);
    EXPECT_TRUE(m1 < m3);
    EXPECT_FALSE(m2 < m1);

    EXPECT_TRUE(m1 == m1);
    EXPECT_FALSE(m1 == m2);
    EXPECT_TRUE(m1 != m2);
}

TEST_F(MigrationFixture, MigrationCopy) {
    quiver::Migration original(2, migrations_path + "/2");
    quiver::Migration copy = original;

    EXPECT_EQ(copy.version(), original.version());
    EXPECT_EQ(copy.path(), original.path());
}

// ============================================================================
// Migrations class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationsLoad) {
    quiver::Migrations migrations(migrations_path);

    EXPECT_FALSE(migrations.empty());
    EXPECT_EQ(migrations.count(), 3u);
    EXPECT_EQ(migrations.latest_version(), 3);
}

TEST_F(MigrationFixture, MigrationsOrder) {
    quiver::Migrations migrations(migrations_path);
    auto all = migrations.all();

    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].version(), 1);
    EXPECT_EQ(all[1].version(), 2);
    EXPECT_EQ(all[2].version(), 3);
}

TEST_F(MigrationFixture, MigrationsPending) {
    quiver::Migrations migrations(migrations_path);

    auto pending_from_0 = migrations.pending(0);
    EXPECT_EQ(pending_from_0.size(), 3u);

    auto pending_from_1 = migrations.pending(1);
    EXPECT_EQ(pending_from_1.size(), 2u);
    EXPECT_EQ(pending_from_1[0].version(), 2);

    auto pending_from_2 = migrations.pending(2);
    EXPECT_EQ(pending_from_2.size(), 1u);
    EXPECT_EQ(pending_from_2[0].version(), 3);

    auto pending_from_3 = migrations.pending(3);
    EXPECT_TRUE(pending_from_3.empty());
}

TEST_F(MigrationFixture, MigrationsIteration) {
    quiver::Migrations migrations(migrations_path);

    int64_t expected_version = 1;
    for (const auto& migration : migrations) {
        EXPECT_EQ(migration.version(), expected_version);
        ++expected_version;
    }
    EXPECT_EQ(expected_version, 4);
}

TEST_F(MigrationFixture, MigrationsEmptyPath) {
    quiver::Migrations migrations("non_existent_path");
    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
    EXPECT_EQ(migrations.latest_version(), 0);
}

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

TEST_F(MigrationFixture, MigrationsInvalidDirectory) {
    quiver::Migrations migrations("nonexistent/path/to/migrations");
    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
    EXPECT_EQ(migrations.latest_version(), 0);
}

TEST_F(MigrationFixture, MigrationsPendingFromHigherVersion) {
    quiver::Migrations migrations(migrations_path);

    // If current version is higher than latest migration version
    auto pending = migrations.pending(100);
    EXPECT_TRUE(pending.empty());
}

TEST_F(MigrationFixture, DatabaseFromMigrationsInvalidPath) {
    EXPECT_THROW(quiver::Database::from_migrations(path, "nonexistent/migrations/"), std::runtime_error);
}

TEST_F(MigrationFixture, MigrationVersionZero) {
    quiver::Migrations migrations(migrations_path);

    // Version 0 should return all migrations as pending
    auto pending = migrations.pending(0);
    EXPECT_EQ(pending.size(), migrations.count());
}

TEST_F(MigrationFixture, MigrationsWithPartialApplication) {
    // First apply first migration using from_migrations and then check
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

TEST_F(MigrationFixture, MigrationGetByVersion) {
    quiver::Migrations migrations(migrations_path);

    auto all = migrations.all();
    ASSERT_EQ(all.size(), 3u);

    // Verify each migration has correct version
    for (const auto& m : all) {
        EXPECT_GE(m.version(), 1);
        EXPECT_LE(m.version(), 3);
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
        if (attribute.name == "id")
            has_id = true;
        if (attribute.name == "label")
            has_label = true;
        if (attribute.name == "name")
            has_name = true;
    }
    EXPECT_TRUE(has_id);
    EXPECT_TRUE(has_label);
    EXPECT_TRUE(has_name);
}

TEST_F(MigrationFixture, FromMigrationsAllowsCreateElement) {
    auto db = quiver::Database::from_migrations(":memory:", migrations_path);

    // create_element requires schema and type_validator to be loaded
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

// ============================================================================
// Describe tests
// ============================================================================

TEST_F(TempFileFixture, DescribeDoesNotThrow) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_NO_THROW(db.describe());
}
