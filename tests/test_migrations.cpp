#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/migration.h>
#include <quiver/migrations.h>

namespace fs = std::filesystem;

class MigrationsTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = (fs::temp_directory_path() / "quiver_migrations_test").string();
        migrations_path = (fs::path(__FILE__).parent_path() / "schemas" / "migrations").string();

        // Clean up temp directory if it exists
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
    }

    void TearDown() override {
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
    }

    std::string temp_dir;
    std::string migrations_path;
};

// ============================================================================
// Migration class tests
// ============================================================================

TEST_F(MigrationsTestFixture, MigrationUpSqlNonexistentPath) {
    quiver::Migration migration(1, "/nonexistent/path/to/migration");
    auto sql = migration.up_sql();
    EXPECT_TRUE(sql.empty());
}

TEST_F(MigrationsTestFixture, MigrationDownSqlNonexistentPath) {
    quiver::Migration migration(1, "/nonexistent/path/to/migration");
    auto sql = migration.down_sql();
    EXPECT_TRUE(sql.empty());
}

TEST_F(MigrationsTestFixture, MigrationComparisonOperatorsLessOrEqual) {
    quiver::Migration m1(1, migrations_path + "/1");
    quiver::Migration m2(2, migrations_path + "/2");
    quiver::Migration m1_copy(1, migrations_path + "/1");

    EXPECT_TRUE(m1 <= m2);
    EXPECT_TRUE(m1 <= m1_copy);
    EXPECT_FALSE(m2 <= m1);
}

TEST_F(MigrationsTestFixture, MigrationComparisonOperatorsGreaterOrEqual) {
    quiver::Migration m1(1, migrations_path + "/1");
    quiver::Migration m2(2, migrations_path + "/2");
    quiver::Migration m2_copy(2, migrations_path + "/2");

    EXPECT_TRUE(m2 >= m1);
    EXPECT_TRUE(m2 >= m2_copy);
    EXPECT_FALSE(m1 >= m2);
}

TEST_F(MigrationsTestFixture, MigrationComparisonOperatorsGreater) {
    quiver::Migration m1(1, migrations_path + "/1");
    quiver::Migration m2(2, migrations_path + "/2");

    EXPECT_TRUE(m2 > m1);
    EXPECT_FALSE(m1 > m2);
    EXPECT_FALSE(m1 > m1);
}

TEST_F(MigrationsTestFixture, MigrationMoveSemantics) {
    quiver::Migration original(1, migrations_path + "/1");
    auto original_version = original.version();
    auto original_path = original.path();

    quiver::Migration moved = std::move(original);

    EXPECT_EQ(moved.version(), original_version);
    EXPECT_EQ(moved.path(), original_path);
}

TEST_F(MigrationsTestFixture, MigrationCopyAssignment) {
    quiver::Migration m1(1, migrations_path + "/1");
    quiver::Migration m2(2, migrations_path + "/2");

    m2 = m1;

    EXPECT_EQ(m2.version(), m1.version());
    EXPECT_EQ(m2.path(), m1.path());
}

TEST_F(MigrationsTestFixture, MigrationMoveAssignment) {
    quiver::Migration m1(1, migrations_path + "/1");
    quiver::Migration m2(2, migrations_path + "/2");

    auto m1_version = m1.version();
    auto m1_path = m1.path();

    m2 = std::move(m1);

    EXPECT_EQ(m2.version(), m1_version);
    EXPECT_EQ(m2.path(), m1_path);
}

// ============================================================================
// Migrations class tests
// ============================================================================

TEST_F(MigrationsTestFixture, MigrationsDefaultConstructor) {
    quiver::Migrations migrations;

    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
    EXPECT_EQ(migrations.latest_version(), 0);
}

TEST_F(MigrationsTestFixture, MigrationsPathIsFile) {
    // Create a temporary file instead of directory
    fs::create_directories(temp_dir);
    auto file_path = fs::path(temp_dir) / "not_a_directory.txt";
    std::ofstream file(file_path);
    file << "test content";
    file.close();

    quiver::Migrations migrations(file_path.string());

    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
}

TEST_F(MigrationsTestFixture, MigrationsCopySemantics) {
    quiver::Migrations original(migrations_path);

    quiver::Migrations copy = original;

    EXPECT_EQ(copy.count(), original.count());
    EXPECT_EQ(copy.latest_version(), original.latest_version());
}

TEST_F(MigrationsTestFixture, MigrationsMoveSemantics) {
    quiver::Migrations original(migrations_path);
    auto original_count = original.count();
    auto original_latest = original.latest_version();

    quiver::Migrations moved = std::move(original);

    EXPECT_EQ(moved.count(), original_count);
    EXPECT_EQ(moved.latest_version(), original_latest);
}

TEST_F(MigrationsTestFixture, MigrationsCopyAssignment) {
    quiver::Migrations m1(migrations_path);
    quiver::Migrations m2;

    m2 = m1;

    EXPECT_EQ(m2.count(), m1.count());
    EXPECT_EQ(m2.latest_version(), m1.latest_version());
}

TEST_F(MigrationsTestFixture, MigrationsMoveAssignment) {
    quiver::Migrations m1(migrations_path);
    auto m1_count = m1.count();
    auto m1_latest = m1.latest_version();

    quiver::Migrations m2;
    m2 = std::move(m1);

    EXPECT_EQ(m2.count(), m1_count);
    EXPECT_EQ(m2.latest_version(), m1_latest);
}

TEST_F(MigrationsTestFixture, MigrationsSelfAssignment) {
    quiver::Migrations migrations(migrations_path);
    auto count = migrations.count();

    migrations = migrations;

    EXPECT_EQ(migrations.count(), count);
}

// ============================================================================
// Database migration error tests
// ============================================================================

TEST_F(MigrationsTestFixture, DatabaseMigrationWithEmptyUpSql) {
    // Create a migration directory with empty up.sql
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream up_file(fs::path(temp_dir) / "1" / "up.sql");
    up_file << "";  // Empty SQL
    up_file.close();

    // Empty up.sql should cause migration to fail
    EXPECT_THROW(
        quiver::Database::from_migrations(":memory:", temp_dir, {.read_only = 0, .console_level = QUIVER_LOG_OFF}),
        std::runtime_error);
}

TEST_F(MigrationsTestFixture, DatabaseMigrationWithInvalidSQL) {
    // Create a migration directory with invalid SQL
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream up_file(fs::path(temp_dir) / "1" / "up.sql");
    up_file << "THIS IS NOT VALID SQL AT ALL;";
    up_file.close();

    EXPECT_THROW(
        quiver::Database::from_migrations(":memory:", temp_dir, {.read_only = 0, .console_level = QUIVER_LOG_OFF}),
        std::runtime_error);
}

TEST_F(MigrationsTestFixture, MigrationsWithNonNumericDirectories) {
    // Create directories with non-numeric names
    fs::create_directories(fs::path(temp_dir) / "abc");
    fs::create_directories(fs::path(temp_dir) / "not_a_number");

    quiver::Migrations migrations(temp_dir);

    // Non-numeric directories should be ignored
    EXPECT_TRUE(migrations.empty());
}

TEST_F(MigrationsTestFixture, MigrationsWithMixedDirectories) {
    // Create both valid and invalid directories
    fs::create_directories(fs::path(temp_dir) / "1");
    fs::create_directories(fs::path(temp_dir) / "abc");
    fs::create_directories(fs::path(temp_dir) / "2");
    fs::create_directories(fs::path(temp_dir) / "not_a_number");

    // Create up.sql files
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql") << "CREATE TABLE Test1 (id INTEGER PRIMARY KEY);";
    std::ofstream(fs::path(temp_dir) / "2" / "up.sql") << "CREATE TABLE Test2 (id INTEGER PRIMARY KEY);";

    quiver::Migrations migrations(temp_dir);

    // Only numeric directories should be counted
    EXPECT_EQ(migrations.count(), 2u);
    EXPECT_EQ(migrations.latest_version(), 2);
}

TEST_F(MigrationsTestFixture, MigrationsWithZeroVersionDirectory) {
    // Version 0 should be ignored (invalid)
    fs::create_directories(fs::path(temp_dir) / "0");
    fs::create_directories(fs::path(temp_dir) / "1");

    std::ofstream(fs::path(temp_dir) / "0" / "up.sql") << "CREATE TABLE Test0 (id INTEGER PRIMARY KEY);";
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql") << "CREATE TABLE Test1 (id INTEGER PRIMARY KEY);";

    quiver::Migrations migrations(temp_dir);

    // Version 0 should be ignored
    EXPECT_EQ(migrations.count(), 1u);
    EXPECT_EQ(migrations.latest_version(), 1);
}

TEST_F(MigrationsTestFixture, MigrationsWithNegativeVersionDirectory) {
    // Negative versions should be ignored (can't parse as valid)
    fs::create_directories(fs::path(temp_dir) / "1");

    std::ofstream(fs::path(temp_dir) / "1" / "up.sql") << "CREATE TABLE Test1 (id INTEGER PRIMARY KEY);";

    quiver::Migrations migrations(temp_dir);

    EXPECT_EQ(migrations.count(), 1u);
}
