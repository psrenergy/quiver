#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/migration.h>
#include <quiver/migrations.h>
#include <sstream>
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

// ============================================================================
// Migration tests
// ============================================================================

class MigrationFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "quiver_test.db").string();
        migrations_path = (fs::path(__FILE__).parent_path() / "schemas" / "migrations").string();
        // Parent of migrations_path: its `migrations/` child is the shared 3-migration fixture, so
        // from_database(database_dir) exercises the migration engine (no `ui/` here -> empty UI).
        database_dir = (fs::path(__FILE__).parent_path() / "schemas").string();
    }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
    std::string migrations_path;
    std::string database_dir;
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

TEST_F(MigrationFixture, FromDatabaseAppliesAllMigrations) {
    auto db = quiver::Database::from_database(path, database_dir);

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

// ============================================================================
// from_database (migrations/ + ui/) tests
// ============================================================================

class DatabaseDirFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "quiver_from_database_test.db").string();
        dir = (fs::path(__FILE__).parent_path() / "schemas" / "from_database").string();
        if (fs::exists(path))
            fs::remove(path);
    }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
    std::string dir;
};

TEST_F(DatabaseDirFixture, FromDatabaseAppliesMigrations) {
    auto db = quiver::Database::from_database(path, dir);
    EXPECT_EQ(db.current_version(), 1);
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(DatabaseDirFixture, FromDatabaseLoadsUiMetadata) {
    auto db = quiver::Database::from_database(path, dir);
    EXPECT_EQ(db.get_model_name(), "demo_model");
    EXPECT_EQ(db.get_attribute_unit("Material", "demand"), "unit/year");
    EXPECT_EQ(db.get_attribute_unit("Material", "label"), "");
    EXPECT_EQ(db.get_attribute_unit("Nonexistent", "x"), "");
}

TEST_F(DatabaseDirFixture, FromDatabaseRejectsReadOnly) {
    quiver::DatabaseOptions options;
    options.read_only = true;
    EXPECT_THROW(quiver::Database::from_database(path, dir, options), std::runtime_error);
}

TEST_F(DatabaseDirFixture, FromDatabaseInvalidPath) {
    EXPECT_THROW(quiver::Database::from_database(path, "nonexistent/database/"), std::runtime_error);
}

TEST_F(DatabaseDirFixture, FromDatabaseMissingMigrationsThrows) {
    // A directory that exists but has no migrations/ subfolder must fail fast ("Migrations path not
    // found"), not silently yield a schema-less version-0 database.
    const auto dir_without_migrations = (fs::path(__FILE__).parent_path() / "schemas" / "valid").string();
    EXPECT_THROW(quiver::Database::from_database(path, dir_without_migrations), std::runtime_error);
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(DatabaseDirFixture, FromDatabaseMalformedUiLeavesNoDatabaseFile) {
    // UI is parsed before the db is created/migrated, so a malformed ui/ throws without leaving a
    // partially-initialized database on disk.
    const auto tmp = fs::temp_directory_path() / "quiver_from_database_malformed_ui";
    fs::remove_all(tmp);
    fs::create_directories(tmp / "migrations" / "1");
    {
        std::ofstream up((tmp / "migrations" / "1" / "up.sql").string());
        up << "PRAGMA user_version = 1;\nCREATE TABLE Configuration (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT "
              "NULL) STRICT;\n";
    }
    fs::create_directories(tmp / "ui");
    {
        std::ofstream bad((tmp / "ui" / "material.toml").string());
        bad << "id = \"Material\"\nthis is not valid toml = =\n";
    }

    EXPECT_THROW(quiver::Database::from_database(path, tmp.string()), std::runtime_error);
    EXPECT_FALSE(fs::exists(path));

    fs::remove_all(tmp);
}

TEST_F(TempFileFixture, UiMetadataEmptyWithoutFromDatabase) {
    auto db = quiver::Database::from_schema(path, VALID_SCHEMA("basic.sql"));
    EXPECT_EQ(db.get_model_name(), "");
    EXPECT_EQ(db.get_attribute_unit("Material", "demand"), "");
}

TEST_F(MigrationFixture, MigrationVersionZero) {
    quiver::Migrations migrations(migrations_path);

    // Version 0 should return all migrations as pending
    auto pending = migrations.pending(0);
    EXPECT_EQ(pending.size(), migrations.count());
}

TEST_F(MigrationFixture, MigrationsWithPartialApplication) {
    // Apply all migrations via from_database, then reopen and confirm the version persists.
    {
        auto db = quiver::Database::from_database(path, database_dir);
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

TEST_F(MigrationFixture, FromDatabaseInMemory) {
    auto db = quiver::Database::from_database(":memory:", database_dir);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

// ============================================================================
// Schema loading after migrations tests
// ============================================================================

TEST_F(MigrationFixture, FromDatabaseLoadsSchemaMetadata) {
    auto db = quiver::Database::from_database(":memory:", database_dir);

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

TEST_F(MigrationFixture, FromDatabaseAllowsCreateElement) {
    auto db = quiver::Database::from_database(":memory:", database_dir);

    // create_element requires schema and type_validator to be loaded
    auto id = db.create_element("Test1", quiver::Element().set("label", "item1").set("name", "Test Item"));
    EXPECT_GT(id, 0);

    // Verify element was created
    auto names = db.read_scalar_strings("Test1", "name");
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "Test Item");
}

TEST_F(MigrationFixture, FromDatabaseAllowsCreateElementInLaterMigration) {
    auto db = quiver::Database::from_database(":memory:", database_dir);

    // Test3 is created in migration 3
    auto id = db.create_element("Test3", quiver::Element().set("label", "item1").set("capacity", int64_t{100}));
    EXPECT_GT(id, 0);

    auto capacities = db.read_scalar_integers("Test3", "capacity");
    ASSERT_EQ(capacities.size(), 1u);
    EXPECT_EQ(capacities[0], 100);
}

TEST_F(MigrationFixture, FromDatabaseLoadsSchemaWhenAlreadyUpToDate) {
    // First, apply all migrations
    {
        auto db = quiver::Database::from_database(path, database_dir);
        EXPECT_EQ(db.current_version(), 3);
    }

    // Reopen with from_database again (no pending migrations)
    auto db = quiver::Database::from_database(path, database_dir);

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

// Helper to capture describe() output
static std::string capture_describe(const quiver::Database& db) {
    return db.describe();
}

TEST_F(TempFileFixture, DescribeVectorsHeaderPrintedOnce) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto output = capture_describe(db);

    // "Vectors:" header should appear exactly once
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find("Vectors:", pos)) != std::string::npos) {
        ++count;
        pos += 8;
    }
    EXPECT_EQ(count, 1) << "Vectors: header should appear exactly once. Output:\n" << output;

    // Both vector groups should be listed
    EXPECT_NE(output.find("values"), std::string::npos) << "Missing vector group 'values'";
    EXPECT_NE(output.find("scores"), std::string::npos) << "Missing vector group 'scores'";
}

TEST_F(TempFileFixture, DescribeSetsHeaderPrintedOnce) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto output = capture_describe(db);

    // "Sets:" header should appear exactly once
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find("Sets:", pos)) != std::string::npos) {
        ++count;
        pos += 5;
    }
    EXPECT_EQ(count, 1) << "Sets: header should appear exactly once. Output:\n" << output;

    // Both set groups should be listed
    EXPECT_NE(output.find("tags"), std::string::npos) << "Missing set group 'tags'";
    EXPECT_NE(output.find("categories"), std::string::npos) << "Missing set group 'categories'";
}

TEST_F(TempFileFixture, DescribeTimeSeriesWithDimensionColumn) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto output = capture_describe(db);

    // "Time Series:" header should appear exactly once
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find("Time Series:", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 1) << "Time Series: header should appear exactly once. Output:\n" << output;

    // Dimension column should be in brackets
    EXPECT_NE(output.find("[date_time]"), std::string::npos)
        << "Expected dimension column [date_time] in brackets. Output:\n"
        << output;
    EXPECT_NE(output.find("[date_recorded]"), std::string::npos)
        << "Expected dimension column [date_recorded] in brackets. Output:\n"
        << output;
}

TEST_F(TempFileFixture, DescribeColumnOrderMatchesSchema) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto output = capture_describe(db);

    // In the Items collection scalars, the schema defines: id, label, priority, weight
    // The 'id' should appear before 'label', 'label' before 'priority', 'priority' before 'weight'
    auto id_pos = output.find("    - id ");
    auto label_pos = output.find("    - label ");
    auto priority_pos = output.find("    - priority ");
    auto weight_pos = output.find("    - weight ");

    ASSERT_NE(id_pos, std::string::npos) << "Missing 'id' scalar";
    ASSERT_NE(label_pos, std::string::npos) << "Missing 'label' scalar";
    ASSERT_NE(priority_pos, std::string::npos) << "Missing 'priority' scalar";
    ASSERT_NE(weight_pos, std::string::npos) << "Missing 'weight' scalar";

    EXPECT_LT(id_pos, label_pos) << "id should appear before label";
    EXPECT_LT(label_pos, priority_pos) << "label should appear before priority";
    EXPECT_LT(priority_pos, weight_pos) << "priority should appear before weight";
}

TEST_F(TempFileFixture, DescribeNoCategoryHeaderWhenEmpty) {
    // basic.sql has no vectors, sets, or time series
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto output = capture_describe(db);

    EXPECT_EQ(output.find("Vectors:"), std::string::npos)
        << "Vectors: header should not appear when no vectors exist. Output:\n"
        << output;
    EXPECT_EQ(output.find("Sets:"), std::string::npos) << "Sets: header should not appear when no sets exist. Output:\n"
                                                       << output;
    EXPECT_EQ(output.find("Time Series:"), std::string::npos)
        << "Time Series: header should not appear when no time series exist. Output:\n"
        << output;
}
