#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <string>

namespace fs = std::filesystem;

class CApiTest : public ::testing::Test {
protected:
    void SetUp() override { test_db_path_ = (fs::temp_directory_path() / "psr_c_test.db").string(); }

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

TEST_F(CApiTest, OpenAndClose) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(test_db_path_.c_str(), PSR_LOG_OFF, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);
    EXPECT_EQ(psr_database_is_open(db), 1);

    psr_database_close(db);
}

TEST_F(CApiTest, OpenInMemory) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", PSR_LOG_OFF, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);
    EXPECT_EQ(psr_database_is_open(db), 1);

    psr_database_close(db);
}

TEST_F(CApiTest, OpenNullPath) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(nullptr, PSR_LOG_OFF, &error);

    EXPECT_EQ(db, nullptr);
    EXPECT_EQ(error, PSR_ERROR_INVALID_ARGUMENT);
}

TEST_F(CApiTest, DatabasePath) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(test_db_path_.c_str(), PSR_LOG_OFF, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), test_db_path_.c_str());

    psr_database_close(db);
}

TEST_F(CApiTest, DatabasePathInMemory) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", PSR_LOG_OFF, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), ":memory:");

    psr_database_close(db);
}

TEST_F(CApiTest, DatabasePathNullDb) {
    EXPECT_EQ(psr_database_path(nullptr), nullptr);
}

TEST_F(CApiTest, IsOpenNullDb) {
    EXPECT_EQ(psr_database_is_open(nullptr), 0);
}

TEST_F(CApiTest, CloseNullDb) {
    // Should not crash
    psr_database_close(nullptr);
}

TEST_F(CApiTest, ErrorStrings) {
    EXPECT_STREQ(psr_error_string(PSR_OK), "Success");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_DATABASE), "Database error");
}

TEST_F(CApiTest, Version) {
    const char* version = psr_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(CApiTest, LogLevelDebug) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", PSR_LOG_DEBUG, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);

    psr_database_close(db);
}

TEST_F(CApiTest, LogLevelInfo) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", PSR_LOG_INFO, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);

    psr_database_close(db);
}

TEST_F(CApiTest, LogLevelWarn) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", PSR_LOG_WARN, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);

    psr_database_close(db);
}

TEST_F(CApiTest, LogLevelError) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", PSR_LOG_ERROR, &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);

    psr_database_close(db);
}

TEST_F(CApiTest, CreatesFileOnDisk) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(test_db_path_.c_str(), PSR_LOG_OFF, &error);

    ASSERT_NE(db, nullptr);
    psr_database_close(db);

    EXPECT_TRUE(fs::exists(test_db_path_));
}
