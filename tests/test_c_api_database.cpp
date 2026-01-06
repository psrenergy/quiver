#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>
#include <psr/database.h>

namespace fs = std::filesystem;

TEST_F(DatabaseFixture, OpenAndClose) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_is_healthy(db), 1);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, OpenInMemory) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(psr_database_is_healthy(db), 1);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, OpenNullPath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(nullptr, &options);

    EXPECT_EQ(db, nullptr);
}

TEST_F(DatabaseFixture, DatabasePath) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), path.c_str());

    psr_database_close(db);
}

TEST_F(DatabaseFixture, DatabasePathInMemory) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);
    EXPECT_STREQ(psr_database_path(db), ":memory:");

    psr_database_close(db);
}

TEST_F(DatabaseFixture, DatabasePathNullDb) {
    EXPECT_EQ(psr_database_path(nullptr), nullptr);
}

TEST_F(DatabaseFixture, IsOpenNullDb) {
    EXPECT_EQ(psr_database_is_healthy(nullptr), 0);
}

TEST_F(DatabaseFixture, CloseNullDb) {
    psr_database_close(nullptr);
}

TEST_F(DatabaseFixture, ErrorStrings) {
    EXPECT_STREQ(psr_error_string(PSR_OK), "Success");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_DATABASE), "Database error");
}

TEST_F(DatabaseFixture, Version) {
    auto version = psr_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(DatabaseFixture, LogLevelDebug) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_DEBUG;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, LogLevelInfo) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_INFO;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, LogLevelWarn) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_WARN;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, LogLevelError) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_ERROR;
    auto db = psr_database_open(":memory:", &options);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreatesFileOnDisk) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(fs::exists(path));

    psr_database_close(db);
}

TEST_F(DatabaseFixture, DefaultOptions) {
    auto options = psr_database_options_default();

    EXPECT_EQ(options.read_only, 0);
    EXPECT_EQ(options.console_level, PSR_LOG_INFO);
}

TEST_F(DatabaseFixture, OpenWithNullOptions) {
    auto db = psr_database_open(":memory:", nullptr);

    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, OpenReadOnly) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);
    psr_database_close(db);

    options.read_only = 1;
    db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementWithScalars) {
    // Setup: Create database with schema using C++ API
    {
        psr::Database db(path, {.console_level = psr::LogLevel::off});
        db.execute("CREATE TABLE Plant (id INTEGER PRIMARY KEY, label TEXT, capacity REAL)");
    }

    // Test: Use C API to create element
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Plant 1");
    psr_element_set_double(element, "capacity", 50.0);

    int64_t id = psr_database_create_element(db, "Plant", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);

    // Verify: Check data was inserted correctly
    {
        psr::Database db(path, {.console_level = psr::LogLevel::off});
        auto result = db.execute("SELECT label, capacity FROM Plant WHERE id = ?", {id});
        EXPECT_EQ(result.row_count(), 1);
        EXPECT_EQ(result[0].get_string(0).value(), "Plant 1");
        EXPECT_EQ(result[0].get_double(1).value(), 50.0);
    }
}

TEST_F(DatabaseFixture, CreateElementWithVector) {
    // Setup: Create database with schema using C++ API
    {
        psr::Database db(path, {.console_level = psr::LogLevel::off});
        db.execute("CREATE TABLE Plant (id INTEGER PRIMARY KEY, label TEXT)");
        db.execute(
            "CREATE TABLE Plant_vector_costs (id INTEGER, vector_index INTEGER, costs REAL, PRIMARY KEY (id, "
            "vector_index))");
    }

    // Test: Use C API to create element with vector
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Plant 1");

    double costs[] = {1.5, 2.5, 3.5};
    psr_element_set_vector_double(element, "costs", costs, 3);

    int64_t id = psr_database_create_element(db, "Plant", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);

    // Verify
    {
        psr::Database db(path, {.console_level = psr::LogLevel::off});

        auto result = db.execute("SELECT label FROM Plant WHERE id = ?", {id});
        EXPECT_EQ(result.row_count(), 1);
        EXPECT_EQ(result[0].get_string(0).value(), "Plant 1");

        auto vec_result =
            db.execute("SELECT vector_index, costs FROM Plant_vector_costs WHERE id = ? ORDER BY vector_index", {id});
        EXPECT_EQ(vec_result.row_count(), 3);
        EXPECT_EQ(vec_result[0].get_int(0).value(), 1);
        EXPECT_EQ(vec_result[0].get_double(1).value(), 1.5);
        EXPECT_EQ(vec_result[1].get_int(0).value(), 2);
        EXPECT_EQ(vec_result[1].get_double(1).value(), 2.5);
        EXPECT_EQ(vec_result[2].get_int(0).value(), 3);
        EXPECT_EQ(vec_result[2].get_double(1).value(), 3.5);
    }
}

TEST_F(DatabaseFixture, CreateElementNullDb) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Test");

    int64_t id = psr_database_create_element(nullptr, "Plant", element);
    EXPECT_EQ(id, -1);

    psr_element_destroy(element);
}

TEST_F(DatabaseFixture, CreateElementNullCollection) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Test");

    int64_t id = psr_database_create_element(db, nullptr, element);
    EXPECT_EQ(id, -1);

    psr_element_destroy(element);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementNullElement) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(":memory:", &options);
    ASSERT_NE(db, nullptr);

    int64_t id = psr_database_create_element(db, "Plant", nullptr);
    EXPECT_EQ(id, -1);

    psr_database_close(db);
}
