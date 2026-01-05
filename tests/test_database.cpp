#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <string>

namespace fs = std::filesystem;

TEST_F(DatabaseFixture, OpenAndClose) {
    psr::Database db(path);
    EXPECT_TRUE(db.is_open());
    EXPECT_EQ(db.path(), path);

    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseFixture, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_open());
    EXPECT_EQ(db.path(), ":memory:");
}

TEST_F(DatabaseFixture, DestructorClosesDatabase) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_open());
    }
    // Database should be closed after destructor
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(DatabaseFixture, MoveConstructor) {
    psr::Database db1(path);
    EXPECT_TRUE(db1.is_open());

    psr::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_open());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(DatabaseFixture, MoveAssignment) {
    psr::Database db1(path);
    psr::Database db2(":memory:");

    db2 = std::move(db1);
    EXPECT_TRUE(db2.is_open());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(DatabaseFixture, CloseIsIdempotent) {
    psr::Database db(path);
    db.close();
    EXPECT_FALSE(db.is_open());

    // Calling close again should not crash
    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseFixture, LogLevelDebug) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::debug});
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseFixture, LogLevelOff) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseFixture, CreatesFileOnDisk) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_open());
    }
    EXPECT_TRUE(fs::exists(path));
}
