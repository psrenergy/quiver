#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>
#include <string>

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
    // Should not crash
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
    // First create a database
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);
    psr_database_close(db);

    // Now open it in read-only mode
    options.read_only = 1;
    db = psr_database_open(path.c_str(), &options);
    ASSERT_NE(db, nullptr);

    psr_database_close(db);
}

// Element C API Tests

TEST(ElementCApi, CreateAndDestroy) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_destroy(element);
}

TEST(ElementCApi, DestroyNull) {
    psr_element_destroy(nullptr);  // Should not crash
}

TEST(ElementCApi, EmptyElement) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_has_scalars(element), 0);
    EXPECT_EQ(psr_element_has_vectors(element), 0);
    EXPECT_EQ(psr_element_scalar_count(element), 0);
    EXPECT_EQ(psr_element_vector_count(element), 0);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetInt) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_int(element, "count", 42), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);
    EXPECT_EQ(psr_element_scalar_count(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetDouble) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_double(element, "value", 3.14), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetString) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_string(element, "label", "Plant 1"), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetNull) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_null(element, "empty"), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetVectorInt) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    int64_t values[] = {1, 2, 3};
    EXPECT_EQ(psr_element_set_vector_int(element, "ids", values, 3), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);
    EXPECT_EQ(psr_element_vector_count(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetVectorDouble) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    double values[] = {1.5, 2.5, 3.5};
    EXPECT_EQ(psr_element_set_vector_double(element, "costs", values, 3), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetVectorString) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    const char* values[] = {"a", "b", "c"};
    EXPECT_EQ(psr_element_set_vector_string(element, "names", values, 3), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, Clear) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_int(element, "id", 1);
    double values[] = {1.0, 2.0};
    psr_element_set_vector_double(element, "data", values, 2);

    EXPECT_EQ(psr_element_has_scalars(element), 1);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_clear(element);

    EXPECT_EQ(psr_element_has_scalars(element), 0);
    EXPECT_EQ(psr_element_has_vectors(element), 0);

    psr_element_destroy(element);
}

TEST(ElementCApi, NullElementErrors) {
    EXPECT_EQ(psr_element_set_int(nullptr, "x", 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_double(nullptr, "x", 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_string(nullptr, "x", "y"), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_null(nullptr, "x"), PSR_ERROR_INVALID_ARGUMENT);

    int64_t ival[] = {1};
    EXPECT_EQ(psr_element_set_vector_int(nullptr, "x", ival, 1), PSR_ERROR_INVALID_ARGUMENT);

    double dval[] = {1.0};
    EXPECT_EQ(psr_element_set_vector_double(nullptr, "x", dval, 1), PSR_ERROR_INVALID_ARGUMENT);

    const char* sval[] = {"a"};
    EXPECT_EQ(psr_element_set_vector_string(nullptr, "x", sval, 1), PSR_ERROR_INVALID_ARGUMENT);
}

TEST(ElementCApi, NullNameErrors) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_int(element, nullptr, 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_double(element, nullptr, 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_string(element, nullptr, "y"), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_null(element, nullptr), PSR_ERROR_INVALID_ARGUMENT);

    psr_element_destroy(element);
}

TEST(ElementCApi, NullAccessors) {
    EXPECT_EQ(psr_element_has_scalars(nullptr), 0);
    EXPECT_EQ(psr_element_has_vectors(nullptr), 0);
    EXPECT_EQ(psr_element_scalar_count(nullptr), 0);
    EXPECT_EQ(psr_element_vector_count(nullptr), 0);
}

TEST(ElementCApi, MultipleScalars) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_string(element, "label", "Plant 1");
    psr_element_set_double(element, "capacity", 50.0);
    psr_element_set_int(element, "id", 1);

    EXPECT_EQ(psr_element_scalar_count(element), 3);

    psr_element_destroy(element);
}

TEST(ElementCApi, EmptyVector) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_vector_double(element, "empty", nullptr, 0), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_destroy(element);
}
