#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>

#include <filesystem>
#include <fstream>

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
// Database migration error tests
// ============================================================================

TEST_F(MigrationsTestFixture, DatabaseMigrationWithEmptyUpSql) {
    // Create a migration directory with empty up.sql
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream up_file(fs::path(temp_dir) / "1" / "up.sql");
    up_file << "";  // Empty SQL
    up_file.close();

    // Empty up.sql should cause migration to fail
    try {
        quiver::Database::from_migrations(
            ":memory:",
            temp_dir,
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        );
        FAIL() << "Expected from_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Cannot from_migrations: migration 1 has no up.sql file");
    }
}

TEST_F(MigrationsTestFixture, DatabaseMigrationWithInvalidSQL) {
    // Create a migration directory with invalid SQL
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream up_file(fs::path(temp_dir) / "1" / "up.sql");
    up_file << "THIS IS NOT VALID SQL AT ALL;";
    up_file.close();

    try {
        quiver::Database::from_migrations(
            ":memory:",
            temp_dir,
            {.read_only = false, .console_level = quiver::LogLevel::Off}
        );
        FAIL() << "Expected from_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("Failed to from_migrations: up migration 1:"), std::string::npos);
    }
}

TEST_F(MigrationsTestFixture, DatabaseMigrationFailureLeavesNoPartialSchema) {
    // up.sql creates a table and then fails: the migration's transaction must take the table with it.
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql")
        << "CREATE TABLE Configuration (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;"
           "THIS IS NOT VALID SQL;";
    const auto db_path = (fs::path(temp_dir) / "study.db").string();
    const quiver::DatabaseOptions quiet{.read_only = false, .console_level = quiver::LogLevel::Off};

    EXPECT_THROW(quiver::Database::from_migrations(db_path, temp_dir, quiet), std::runtime_error);

    quiver::Database db(db_path, quiet);
    EXPECT_EQ(db.current_version(), 0);
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'"), 0);
}

// ============================================================================
// Migration round-trip tests
// ============================================================================

TEST_F(MigrationsTestFixture, ValidateMigrationsAppliesAndRevertsSharedFixture) {
    EXPECT_NO_THROW(quiver::Database::validate_migrations(migrations_path));
}

TEST_F(MigrationsTestFixture, ValidateMigrationsExecutesUpSql) {
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql") << "THIS IS NOT VALID SQL;";
    std::ofstream(fs::path(temp_dir) / "1" / "down.sql") << "DROP TABLE Test;";

    try {
        quiver::Database::validate_migrations(temp_dir);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Failed to validate_migrations: up migration 1:"), std::string::npos);
        EXPECT_NE(message.find("Failed to execute SQL:"), std::string::npos);
    }
}

TEST_F(MigrationsTestFixture, ValidateMigrationsExecutesDownSql) {
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql")
        << "CREATE TABLE Configuration (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;"
           "CREATE TABLE Test (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;";
    std::ofstream(fs::path(temp_dir) / "1" / "down.sql") << "THIS IS NOT VALID SQL;";

    try {
        quiver::Database::validate_migrations(temp_dir);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Failed to validate_migrations: down migration 1:"), std::string::npos);
        EXPECT_NE(message.find("Failed to execute SQL:"), std::string::npos);
    }
}

TEST_F(MigrationsTestFixture, ValidateMigrationsRejectsLeftoverTables) {
    // Test is AUTOINCREMENT on purpose: dropping it leaves sqlite_sequence behind for good, which
    // is not a leftover and must stay out of the message.
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql")
        << "CREATE TABLE Configuration (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;"
           "CREATE TABLE Test (id INTEGER PRIMARY KEY AUTOINCREMENT, label TEXT UNIQUE NOT NULL) STRICT;"
           "CREATE TABLE Extra (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;";
    std::ofstream(fs::path(temp_dir) / "1" / "down.sql") << "DROP TABLE Test;";

    try {
        quiver::Database::validate_migrations(temp_dir);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(
            error.what(),
            "Failed to validate_migrations: down migrations left tables behind: Configuration, Extra"
        );
    }
}

TEST_F(MigrationsTestFixture, ValidateMigrationsRequiresDownSql) {
    fs::create_directories(fs::path(temp_dir) / "1");
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql")
        << "CREATE TABLE Configuration (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;"
           "CREATE TABLE Test (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;";

    try {
        quiver::Database::validate_migrations(temp_dir);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Cannot validate_migrations: migration 1 has no down.sql file");
    }
}

TEST_F(MigrationsTestFixture, ValidateMigrationsValidatesPath) {
    const auto nonexistent_path = (fs::path(temp_dir) / "missing").string();
    try {
        quiver::Database::validate_migrations(nonexistent_path);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(
            std::string(error.what()),
            "Cannot validate_migrations: migrations path not found: " + nonexistent_path
        );
    }

    fs::create_directories(temp_dir);
    const auto file_path = fs::path(temp_dir) / "not_a_directory.sql";
    std::ofstream(file_path) << "SELECT 1;";
    try {
        quiver::Database::validate_migrations(file_path.string());
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(
            std::string(error.what()),
            "Cannot validate_migrations: path is not a directory: " + file_path.string()
        );
    }
}

TEST_F(MigrationsTestFixture, ValidateMigrationsRequiresMigrations) {
    // A real directory holding no numeric migration subdirectories is a path mistake, not a pass:
    // pointing at "<migrations>/1" instead of "<migrations>" would otherwise succeed vacuously.
    fs::create_directories(temp_dir);
    try {
        quiver::Database::validate_migrations(temp_dir);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()), "Cannot validate_migrations: no migrations found in " + temp_dir);
    }
}

// Discovery: only subdirectories whose whole name is a positive integer are migrations. Each skipped
// directory holds SQL that would fail, so applying it would throw.
TEST_F(MigrationsTestFixture, MigrationsWithNonNumericDirectories) {
    fs::create_directories(fs::path(temp_dir) / "abc");
    fs::create_directories(fs::path(temp_dir) / "not_a_number");

    try {
        quiver::Database::validate_migrations(temp_dir);
        FAIL() << "Expected validate_migrations to throw";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()), "Cannot validate_migrations: no migrations found in " + temp_dir);
    }
}

TEST_F(MigrationsTestFixture, MigrationsSkipNonPositiveAndPartlyNumericDirectories) {
    for (const auto* name : {"0", "-1", "abc", "2abc", "not_a_number"}) {
        fs::create_directories(fs::path(temp_dir) / name);
        std::ofstream(fs::path(temp_dir) / name / "up.sql") << "THIS IS NOT VALID SQL;";
    }
    fs::create_directories(fs::path(temp_dir) / "1");
    fs::create_directories(fs::path(temp_dir) / "2");
    std::ofstream(fs::path(temp_dir) / "1" / "up.sql")
        << "CREATE TABLE Configuration (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;";
    std::ofstream(fs::path(temp_dir) / "2" / "up.sql")
        << "CREATE TABLE Test (id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT;";

    auto db = quiver::Database::from_migrations(
        ":memory:",
        temp_dir,
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
    EXPECT_EQ(db.current_version(), 2);
}
