#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/migration.h>
#include <psr/migrations.h>
#include <string>

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

TEST_F(TempFileFixture, OpenFileOnDisk) {
    psr::Database db(path);
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), path);
}

TEST_F(TempFileFixture, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), ":memory:");
}

TEST_F(TempFileFixture, DestructorClosesDatabase) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    // Database should be closed after destructor
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(TempFileFixture, MoveConstructor) {
    psr::Database db1(path);
    EXPECT_TRUE(db1.is_healthy());

    psr::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(TempFileFixture, MoveAssignment) {
    psr::Database db1(path);
    psr::Database db2(":memory:");

    db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(TempFileFixture, LogLevelDebug) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::debug});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(TempFileFixture, LogLevelOff) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(TempFileFixture, CreatesFileOnDisk) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(TempFileFixture, CurrentVersion) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_EQ(db.current_version(), 0);
}

// ============================================================================
// Schema error tests
// ============================================================================

TEST_F(TempFileFixture, FromSchemaFileNotFound) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", "nonexistent/path/schema.sql", {.console_level = psr::LogLevel::off}),
        std::runtime_error);
}

TEST_F(TempFileFixture, FromSchemaInvalidPath) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", "", {.console_level = psr::LogLevel::off}), std::runtime_error);
}

TEST_F(TempFileFixture, FromMigrationsInvalidPath) {
    // Invalid migrations path results in database with version 0 (no migrations applied)
    auto db =
        psr::Database::from_migrations(":memory:", "nonexistent/migrations/", {.console_level = psr::LogLevel::off});
    EXPECT_EQ(db.current_version(), 0);
}

// ============================================================================
// Migration tests
// ============================================================================

class MigrationFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "psr_test.db").string();
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
    psr::Migration migration(1, migrations_path + "/1");
    EXPECT_EQ(migration.version(), 1);
    EXPECT_FALSE(migration.path().empty());
}

TEST_F(MigrationFixture, MigrationUpSqlRead) {
    psr::Migration migration(1, migrations_path + "/1");
    std::string sql = migration.up_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("CREATE TABLE Test1"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationDownSqlRead) {
    psr::Migration migration(1, migrations_path + "/1");
    std::string sql = migration.down_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("DROP TABLE"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationComparison) {
    psr::Migration m1(1, migrations_path + "/1");
    psr::Migration m2(2, migrations_path + "/2");
    psr::Migration m3(3, migrations_path + "/3");

    EXPECT_TRUE(m1 < m2);
    EXPECT_TRUE(m2 < m3);
    EXPECT_TRUE(m1 < m3);
    EXPECT_FALSE(m2 < m1);

    EXPECT_TRUE(m1 == m1);
    EXPECT_FALSE(m1 == m2);
    EXPECT_TRUE(m1 != m2);
}

TEST_F(MigrationFixture, MigrationCopy) {
    psr::Migration original(2, migrations_path + "/2");
    psr::Migration copy = original;

    EXPECT_EQ(copy.version(), original.version());
    EXPECT_EQ(copy.path(), original.path());
}

// ============================================================================
// Migrations class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationsLoad) {
    psr::Migrations migrations(migrations_path);

    EXPECT_FALSE(migrations.empty());
    EXPECT_EQ(migrations.count(), 3u);
    EXPECT_EQ(migrations.latest_version(), 3);
}

TEST_F(MigrationFixture, MigrationsOrder) {
    psr::Migrations migrations(migrations_path);
    auto all = migrations.all();

    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].version(), 1);
    EXPECT_EQ(all[1].version(), 2);
    EXPECT_EQ(all[2].version(), 3);
}

TEST_F(MigrationFixture, MigrationsPending) {
    psr::Migrations migrations(migrations_path);

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
    psr::Migrations migrations(migrations_path);

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

TEST_F(MigrationFixture, DatabaseFromMigrations) {
    auto db = psr::Database::from_migrations(path, migrations_path);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(MigrationFixture, MigrationsInvalidDirectory) {
    psr::Migrations migrations("nonexistent/path/to/migrations");
    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
    EXPECT_EQ(migrations.latest_version(), 0);
}

TEST_F(MigrationFixture, MigrationsPendingFromHigherVersion) {
    psr::Migrations migrations(migrations_path);

    // If current version is higher than latest migration version
    auto pending = migrations.pending(100);
    EXPECT_TRUE(pending.empty());
}

TEST_F(MigrationFixture, DatabaseFromMigrationsInvalidPath) {
    // Invalid migrations path results in database with version 0 (no migrations applied)
    auto db = psr::Database::from_migrations(path, "nonexistent/migrations/");
    EXPECT_EQ(db.current_version(), 0);
}

TEST_F(MigrationFixture, MigrationVersionZero) {
    psr::Migrations migrations(migrations_path);

    // Version 0 should return all migrations as pending
    auto pending = migrations.pending(0);
    EXPECT_EQ(pending.size(), migrations.count());
}

TEST_F(MigrationFixture, MigrationsWithPartialApplication) {
    // First apply first migration using from_migrations and then check
    {
        auto db = psr::Database::from_migrations(path, migrations_path);
        EXPECT_EQ(db.current_version(), 3);
    }

    // Reopen the database and verify it still has version 3
    {
        psr::Database db(path);
        EXPECT_EQ(db.current_version(), 3);
    }
}

TEST_F(MigrationFixture, MigrationGetByVersion) {
    psr::Migrations migrations(migrations_path);

    auto all = migrations.all();
    ASSERT_EQ(all.size(), 3u);

    // Verify each migration has correct version
    for (const auto& m : all) {
        EXPECT_GE(m.version(), 1);
        EXPECT_LE(m.version(), 3);
    }
}

TEST_F(MigrationFixture, DatabaseFromMigrationsMemory) {
    auto db = psr::Database::from_migrations(":memory:", migrations_path);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

// ============================================================================
// Transaction tests
// ============================================================================

#include "test_utils.h"
#include <psr/element.h>

class TransactionFixture : public ::testing::Test {
protected:
    psr::DatabaseOptions opts{.console_level = psr::LogLevel::off};
};

TEST_F(TransactionFixture, BeginCommit) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    db.begin_transaction();

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    db.commit();

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Test Config");
}

TEST_F(TransactionFixture, BeginRollback) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Create an initial element
    psr::Element config1;
    config1.set("label", std::string("Initial Config"));
    db.create_element("Configuration", config1);

    // Start transaction
    db.begin_transaction();

    // Create another element within transaction
    psr::Element config2;
    config2.set("label", std::string("Transaction Config"));
    db.create_element("Configuration", config2);

    // Rollback
    db.rollback();

    // Only the initial element should remain
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Initial Config");
}

TEST_F(TransactionFixture, MultipleOperationsInTransaction) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    db.begin_transaction();

    psr::Element config1;
    config1.set("label", std::string("Config 1"));
    db.create_element("Configuration", config1);

    psr::Element config2;
    config2.set("label", std::string("Config 2"));
    db.create_element("Configuration", config2);

    psr::Element config3;
    config3.set("label", std::string("Config 3"));
    db.create_element("Configuration", config3);

    db.commit();

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 3);
}

TEST_F(TransactionFixture, ExecuteRawSqlSuccess) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    db.execute_raw("INSERT INTO Configuration (label) VALUES ('Raw Insert')");

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Raw Insert");
}

TEST_F(TransactionFixture, ExecuteRawSqlInvalidSQL) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    EXPECT_THROW(db.execute_raw("THIS IS NOT VALID SQL"), std::runtime_error);
}

TEST_F(TransactionFixture, ExecuteWithParams) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto result = db.execute("SELECT ?", {std::string("hello")});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(0).value(), "hello");
}

TEST_F(TransactionFixture, ExecuteWithNullParam) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto result = db.execute("SELECT ?", {nullptr});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_TRUE(result[0].is_null(0));
}

TEST_F(TransactionFixture, ExecuteWithIntParam) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto result = db.execute("SELECT ?", {int64_t{42}});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_int(0).value(), 42);
}

TEST_F(TransactionFixture, ExecuteWithDoubleParam) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto result = db.execute("SELECT ?", {3.14});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_DOUBLE_EQ(result[0].get_double(0).value(), 3.14);
}

TEST_F(TransactionFixture, ExecuteInvalidSQL) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    EXPECT_THROW(db.execute("INVALID SQL SYNTAX"), std::runtime_error);
}

TEST_F(TransactionFixture, ExecuteSelectNonExistentTable) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    EXPECT_THROW(db.execute("SELECT * FROM NonExistentTable"), std::runtime_error);
}
