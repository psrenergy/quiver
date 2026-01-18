#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>

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
    auto db = psr::Database::from_migrations(":memory:", "nonexistent/migrations/", {.console_level = psr::LogLevel::off});
    EXPECT_EQ(db.current_version(), 0);
}
