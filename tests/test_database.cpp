#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <string>

namespace fs = std::filesystem;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override { test_db_path_ = (fs::temp_directory_path() / "psr_test.db").string(); }

    void TearDown() override {
        if (fs::exists(test_db_path_)) {
            fs::remove(test_db_path_);
        }
        // Clean up log file
        auto log_path = fs::temp_directory_path() / "psr_database.log";
        if (fs::exists(log_path)) {
            fs::remove(log_path);
        }
    }

    std::string test_db_path_;
};

TEST_F(DatabaseTest, OpenAndClose) {
    psr::Database db(test_db_path_);
    EXPECT_TRUE(db.is_open());
    EXPECT_EQ(db.path(), test_db_path_);

    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseTest, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_open());
    EXPECT_EQ(db.path(), ":memory:");
}

TEST_F(DatabaseTest, DestructorClosesDatabase) {
    {
        psr::Database db(test_db_path_);
        EXPECT_TRUE(db.is_open());
    }
    // Database should be closed after destructor
    EXPECT_TRUE(fs::exists(test_db_path_));
}

TEST_F(DatabaseTest, MoveConstructor) {
    psr::Database db1(test_db_path_);
    EXPECT_TRUE(db1.is_open());

    psr::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_open());
    EXPECT_EQ(db2.path(), test_db_path_);
}

TEST_F(DatabaseTest, MoveAssignment) {
    psr::Database db1(test_db_path_);
    psr::Database db2(":memory:");

    db2 = std::move(db1);
    EXPECT_TRUE(db2.is_open());
    EXPECT_EQ(db2.path(), test_db_path_);
}

TEST_F(DatabaseTest, CloseIsIdempotent) {
    psr::Database db(test_db_path_);
    db.close();
    EXPECT_FALSE(db.is_open());

    // Calling close again should not crash
    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseTest, LogLevelDebug) {
    psr::Database db(":memory:", psr::LogLevel::debug);
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseTest, LogLevelOff) {
    psr::Database db(":memory:", psr::LogLevel::off);
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseTest, CreatesFileOnDisk) {
    {
        psr::Database db(test_db_path_);
        EXPECT_TRUE(db.is_open());
    }
    EXPECT_TRUE(fs::exists(test_db_path_));
}
