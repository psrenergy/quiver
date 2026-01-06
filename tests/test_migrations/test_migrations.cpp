#include "../database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/migration.h>
#include <psr/migrations.h>
#include <string>

namespace fs = std::filesystem;

namespace database_utils {

// Get path to test migrations directory
std::string get_test_migrations_path() {
    // This file is at tests/test_migrations/test_migrations.cpp
    // Migrations are at tests/test_migrations/chaining_migrations/
    fs::path source_file(__FILE__);
    return (source_file.parent_path() / "chaining_migrations").string();
}

// Test fixture for migration tests
class MigrationFixture : public DatabaseFixture {
protected:
    std::string schema_path;

    void SetUp() override {
        DatabaseFixture::SetUp();
        schema_path = get_test_migrations_path();
    }
};

// ============================================================================
// Migration class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationCreation) {
    psr::Migration migration(1, schema_path + "/1");
    EXPECT_EQ(migration.version(), 1);
    EXPECT_FALSE(migration.path().empty());
}

TEST_F(MigrationFixture, MigrationUpSqlRead) {
    psr::Migration migration(1, schema_path + "/1");
    std::string sql = migration.up_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("CREATE TABLE Test1"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationDownSqlRead) {
    psr::Migration migration(1, schema_path + "/1");
    std::string sql = migration.down_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("DROP TABLE"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationComparison) {
    psr::Migration m1(1, schema_path + "/1");
    psr::Migration m2(2, schema_path + "/2");
    psr::Migration m3(3, schema_path + "/3");

    EXPECT_TRUE(m1 < m2);
    EXPECT_TRUE(m2 < m3);
    EXPECT_TRUE(m1 < m3);
    EXPECT_FALSE(m2 < m1);

    EXPECT_TRUE(m1 == m1);
    EXPECT_FALSE(m1 == m2);
    EXPECT_TRUE(m1 != m2);
}

TEST_F(MigrationFixture, MigrationCopy) {
    psr::Migration original(2, schema_path + "/2");
    psr::Migration copy = original;

    EXPECT_EQ(copy.version(), original.version());
    EXPECT_EQ(copy.path(), original.path());
}

// ============================================================================
// Migrations class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationsLoad) {
    psr::Migrations migrations(schema_path);

    EXPECT_FALSE(migrations.empty());
    EXPECT_EQ(migrations.count(), 3u);
    EXPECT_EQ(migrations.latest_version(), 3);
}

TEST_F(MigrationFixture, MigrationsOrder) {
    psr::Migrations migrations(schema_path);
    auto all = migrations.all();

    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].version(), 1);
    EXPECT_EQ(all[1].version(), 2);
    EXPECT_EQ(all[2].version(), 3);
}

TEST_F(MigrationFixture, MigrationsPending) {
    psr::Migrations migrations(schema_path);

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
    psr::Migrations migrations(schema_path);

    int64_t expected_version = 1;
    for (const auto& migration : migrations) {
        EXPECT_EQ(migration.version(), expected_version);
        ++expected_version;
    }
    EXPECT_EQ(expected_version, 4);
}

TEST_F(MigrationFixture, MigrationsEmptyPath) {
    psr::Migrations migrations("non_existent_path");
    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
    EXPECT_EQ(migrations.latest_version(), 0);
}

// ============================================================================
// Database migration tests
// ============================================================================

TEST_F(MigrationFixture, DatabaseCurrentVersion) {
    psr::Database db(":memory:");
    EXPECT_EQ(db.current_version(), 0);
}

TEST_F(MigrationFixture, DatabaseSetVersion) {
    psr::Database db(":memory:");
    db.set_version(42);
    EXPECT_EQ(db.current_version(), 42);
}

TEST_F(MigrationFixture, DatabaseMigrateUp) {
    psr::Database db(path);

    EXPECT_EQ(db.current_version(), 0);

    db.migrate_up(schema_path);

    EXPECT_EQ(db.current_version(), 3);

    // Verify tables exist
    auto result = db.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
    std::vector<std::string> tables;
    for (const auto& row : result) {
        auto name = row.get_string(0);
        if (name) {
            tables.push_back(*name);
        }
    }

    EXPECT_NE(std::find(tables.begin(), tables.end(), "Test1"), tables.end());
    EXPECT_NE(std::find(tables.begin(), tables.end(), "Test3"), tables.end());
    // Test2 should not exist (dropped in migration 2)
    EXPECT_EQ(std::find(tables.begin(), tables.end(), "Test2"), tables.end());
}

TEST_F(MigrationFixture, DatabaseMigrateUpIdempotent) {
    psr::Database db(path);

    db.migrate_up(schema_path);
    EXPECT_EQ(db.current_version(), 3);

    // Running again should be a no-op
    db.migrate_up(schema_path);
    EXPECT_EQ(db.current_version(), 3);
}

TEST_F(MigrationFixture, DatabaseFromSchema) {
    auto db = psr::Database::from_schema(path, schema_path);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(MigrationFixture, DatabasePartialMigration) {
    // Start with database at version 1 (matching migration 1's state)
    {
        psr::Database db(path);
        db.set_version(1);
        // Create tables that migration 1 would have created
        db.execute("CREATE TABLE Test1 (id INTEGER PRIMARY KEY, name TEXT)");
        db.execute("CREATE TABLE Test2 (id INTEGER PRIMARY KEY, capacity INTEGER, some_coefficient REAL)");
    }

    // Migrate from version 1 to 3
    {
        psr::Database db(path);
        EXPECT_EQ(db.current_version(), 1);

        db.migrate_up(schema_path);
        EXPECT_EQ(db.current_version(), 3);

        // Verify final state: Test1 and Test3 exist, Test2 was dropped
        auto result = db.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
        std::vector<std::string> tables;
        for (const auto& row : result) {
            auto name = row.get_string(0);
            if (name) {
                tables.push_back(*name);
            }
        }

        EXPECT_NE(std::find(tables.begin(), tables.end(), "Test1"), tables.end());
        EXPECT_NE(std::find(tables.begin(), tables.end(), "Test3"), tables.end());
        EXPECT_EQ(std::find(tables.begin(), tables.end(), "Test2"), tables.end());
    }
}

// ============================================================================
// Transaction tests
// ============================================================================

TEST_F(MigrationFixture, TransactionCommit) {
    psr::Database db(path);
    db.execute("CREATE TABLE trans_test (id INTEGER PRIMARY KEY, val TEXT)");

    db.begin_transaction();
    db.execute("INSERT INTO trans_test (val) VALUES (?)", {std::string("test")});
    db.commit();

    auto result = db.execute("SELECT val FROM trans_test");
    EXPECT_EQ(result.row_count(), 1u);
}

TEST_F(MigrationFixture, TransactionRollback) {
    psr::Database db(path);
    db.execute("CREATE TABLE trans_test (id INTEGER PRIMARY KEY, val TEXT)");

    db.begin_transaction();
    db.execute("INSERT INTO trans_test (val) VALUES (?)", {std::string("test")});
    db.rollback();

    auto result = db.execute("SELECT val FROM trans_test");
    EXPECT_EQ(result.row_count(), 0u);
}

}  // namespace database_utils
