#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/c/database.h>
#include <psr/c/element.h>
#include <psr/database.h>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

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
    // Test: Use C API to create element with schema
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Config 1");
    psr_element_set_integer(element, "integer_attribute", 42);
    psr_element_set_double(element, "float_attribute", 3.14);

    int64_t id = psr_database_create_element(db, "Configuration", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    auto config = psr_element_create();
    ASSERT_NE(config, nullptr);
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // Create Collection with vector
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_set_string(element, "label", "Item 1");

    int64_t values[] = {1, 2, 3};
    psr_element_set_array_int(element, "value_int", values, 3);

    int64_t id = psr_database_create_element(db, "Collection", element);
    EXPECT_EQ(id, 1);

    psr_element_destroy(element);
    psr_database_close(db);
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

TEST_F(DatabaseFixture, ReadScalarIntegers) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_integer(e1, "integer_attribute", 42);
    psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_integer(e2, "integer_attribute", 100);
    psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_integers(db, "Configuration", "integer_attribute", &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 42);
    EXPECT_EQ(values[1], 100);

    psr_free_int_array(values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadScalarDoubles) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_double(e1, "float_attribute", 3.14);
    psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_double(e2, "float_attribute", 2.71);
    psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    double* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_doubles(db, "Configuration", "float_attribute", &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(values[0], 3.14);
    EXPECT_DOUBLE_EQ(values[1], 2.71);

    psr_free_double_array(values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadScalarStrings) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_string(e1, "string_attribute", "hello");
    psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_string(e2, "string_attribute", "world");
    psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_scalar_strings(db, "Configuration", "string_attribute", &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(values[0], "hello");
    EXPECT_STREQ(values[1], "world");

    psr_free_string_array(values, count);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadScalarEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    int64_t* int_values = nullptr;
    size_t int_count = 0;
    auto err = psr_database_read_scalar_integers(db, "Collection", "some_integer", &int_values, &int_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(int_count, 0);
    EXPECT_EQ(int_values, nullptr);

    double* double_values = nullptr;
    size_t double_count = 0;
    err = psr_database_read_scalar_doubles(db, "Collection", "some_float", &double_values, &double_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(double_count, 0);
    EXPECT_EQ(double_values, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadVectorIntegers) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e1, "value_int", values1, 3);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    psr_element_set_array_int(e2, "value_int", values2, 2);
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[1][0], 10);
    EXPECT_EQ(vectors[1][1], 20);

    psr_free_int_vectors(vectors, sizes, count);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadVectorDoubles) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(e1, "value_float", values1, 3);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    double values2[] = {10.5, 20.5};
    psr_element_set_array_double(e2, "value_float", values2, 2);
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    double** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_doubles(db, "Collection", "value_float", &vectors, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_DOUBLE_EQ(vectors[0][0], 1.5);
    EXPECT_DOUBLE_EQ(vectors[0][1], 2.5);
    EXPECT_DOUBLE_EQ(vectors[0][2], 3.5);
    EXPECT_DOUBLE_EQ(vectors[1][0], 10.5);
    EXPECT_DOUBLE_EQ(vectors[1][1], 20.5);

    psr_free_double_vectors(vectors, sizes, count);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadVectorEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    int64_t** int_vectors = nullptr;
    size_t* int_sizes = nullptr;
    size_t int_count = 0;
    auto err = psr_database_read_vector_integers(db, "Collection", "value_int", &int_vectors, &int_sizes, &int_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(int_count, 0);
    EXPECT_EQ(int_vectors, nullptr);
    EXPECT_EQ(int_sizes, nullptr);

    double** double_vectors = nullptr;
    size_t* double_sizes = nullptr;
    size_t double_count = 0;
    err = psr_database_read_vector_doubles(
        db, "Collection", "value_float", &double_vectors, &double_sizes, &double_count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(double_count, 0);
    EXPECT_EQ(double_vectors, nullptr);
    EXPECT_EQ(double_sizes, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadVectorOnlyReturnsElementsWithData) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // Element with vector data
    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e1, "value_int", values1, 3);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    // Element without vector data
    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    // Another element with vector data
    auto e3 = psr_element_create();
    psr_element_set_string(e3, "label", "Item 3");
    int64_t values3[] = {4, 5};
    psr_element_set_array_int(e3, "value_int", values3, 2);
    psr_database_create_element(db, "Collection", e3);
    psr_element_destroy(e3);

    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);

    // Only elements with vector data are returned
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    EXPECT_EQ(vectors[1][0], 4);
    EXPECT_EQ(vectors[1][1], 5);

    psr_free_int_vectors(vectors, sizes, count);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadSetStrings) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important", "urgent"};
    psr_element_set_array_string(e1, "tag", tags1, 2);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    psr_element_set_array_string(e2, "tag", tags2, 1);
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 2);
    EXPECT_EQ(sizes[1], 1);

    // Sets are unordered, so just check values exist
    std::vector<std::string> set1_values;
    for (size_t i = 0; i < sizes[0]; i++) {
        set1_values.push_back(sets[0][i]);
    }
    std::sort(set1_values.begin(), set1_values.end());
    EXPECT_EQ(set1_values[0], "important");
    EXPECT_EQ(set1_values[1], "urgent");

    EXPECT_STREQ(sets[1][0], "review");

    psr_free_string_vectors(sets, sizes, count);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadSetEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(sets, nullptr);
    EXPECT_EQ(sizes, nullptr);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadSetOnlyReturnsElementsWithData) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // Element with set data
    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important"};
    psr_element_set_array_string(e1, "tag", tags1, 1);
    psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    // Element without set data
    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    // Another element with set data
    auto e3 = psr_element_create();
    psr_element_set_string(e3, "label", "Item 3");
    const char* tags3[] = {"urgent", "review"};
    psr_element_set_array_string(e3, "tag", tags3, 2);
    psr_database_create_element(db, "Collection", e3);
    psr_element_destroy(e3);

    char*** sets = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings(db, "Collection", "tag", &sets, &sizes, &count);

    // Only elements with set data are returned
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sizes[0], 1);
    EXPECT_EQ(sizes[1], 2);

    psr_free_string_vectors(sets, sizes, count);
    psr_database_close(db);
}

// ============================================================================
// Read scalar by ID tests
// ============================================================================

TEST_F(DatabaseFixture, ReadScalarIntegerById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    int64_t value;
    int has_value;
    auto err =
        psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 42);

    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id2, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadScalarDoubleById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_double(e1, "float_attribute", 3.14);
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    double value;
    int has_value;
    auto err = psr_database_read_scalar_doubles_by_id(db, "Configuration", "float_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 3.14);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadScalarStringById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_string(e1, "string_attribute", "hello");
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    char* value = nullptr;
    int has_value;
    auto err = psr_database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id1, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "hello");

    delete[] value;
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadScalarByIdNotFound) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_integer(e, "integer_attribute", 42);
    psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    int64_t value;
    int has_value;
    auto err =
        psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 999, &value, &has_value);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 0);

    psr_database_close(db);
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST_F(DatabaseFixture, ReadVectorIntegerById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e1, "value_int", values1, 3);
    int64_t id1 = psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    int64_t values2[] = {10, 20};
    psr_element_set_array_int(e2, "value_int", values2, 2);
    int64_t id2 = psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id1, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    psr_free_int_array(values);

    err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id2, &values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    psr_free_int_array(values);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadVectorDoubleById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(e1, "value_float", values1, 3);
    int64_t id1 = psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    double* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_doubles_by_id(db, "Collection", "value_float", id1, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);
    EXPECT_DOUBLE_EQ(values[0], 1.5);
    EXPECT_DOUBLE_EQ(values[1], 2.5);
    EXPECT_DOUBLE_EQ(values[2], 3.5);

    psr_free_double_array(values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadVectorByIdEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    int64_t* values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Read set by ID tests
// ============================================================================

TEST_F(DatabaseFixture, ReadSetStringById) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Item 1");
    const char* tags1[] = {"important", "urgent"};
    psr_element_set_array_string(e1, "tag", tags1, 2);
    int64_t id1 = psr_database_create_element(db, "Collection", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Item 2");
    const char* tags2[] = {"review"};
    psr_element_set_array_string(e2, "tag", tags2, 1);
    int64_t id2 = psr_database_create_element(db, "Collection", e2);
    psr_element_destroy(e2);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id1, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "important");
    EXPECT_EQ(set_values[1], "urgent");
    psr_free_string_array(values, count);

    err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id2, &values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(values[0], "review");
    psr_free_string_array(values, count);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadSetByIdEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    char** values = nullptr;
    size_t count = 0;
    auto err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id, &values, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Read element IDs tests
// ============================================================================

TEST_F(DatabaseFixture, ReadElementIds) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e1 = psr_element_create();
    psr_element_set_string(e1, "label", "Config 1");
    psr_element_set_integer(e1, "integer_attribute", 42);
    int64_t id1 = psr_database_create_element(db, "Configuration", e1);
    psr_element_destroy(e1);

    auto e2 = psr_element_create();
    psr_element_set_string(e2, "label", "Config 2");
    psr_element_set_integer(e2, "integer_attribute", 100);
    int64_t id2 = psr_database_create_element(db, "Configuration", e2);
    psr_element_destroy(e2);

    auto e3 = psr_element_create();
    psr_element_set_string(e3, "label", "Config 3");
    psr_element_set_integer(e3, "integer_attribute", 200);
    int64_t id3 = psr_database_create_element(db, "Configuration", e3);
    psr_element_destroy(e3);

    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = psr_database_read_element_ids(db, "Configuration", &ids, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
    EXPECT_EQ(ids[2], id3);

    psr_free_int_array(ids);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, ReadElementIdsEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    // No Collection elements created
    int64_t* ids = nullptr;
    size_t count = 0;
    auto err = psr_database_read_element_ids(db, "Collection", &ids, &count);

    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(ids, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Update scalar tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateScalarInteger) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_integer(e, "integer_attribute", 42);
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    auto err = psr_database_update_scalar_integer(db, "Configuration", "integer_attribute", id, 100);
    EXPECT_EQ(err, PSR_OK);

    int64_t value;
    int has_value;
    err = psr_database_read_scalar_integers_by_id(db, "Configuration", "integer_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 100);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateScalarDouble) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_double(e, "float_attribute", 3.14);
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    auto err = psr_database_update_scalar_double(db, "Configuration", "float_attribute", id, 2.71);
    EXPECT_EQ(err, PSR_OK);

    double value;
    int has_value;
    err = psr_database_read_scalar_doubles_by_id(db, "Configuration", "float_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_DOUBLE_EQ(value, 2.71);

    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateScalarString) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/basic.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Config 1");
    psr_element_set_string(e, "string_attribute", "hello");
    int64_t id = psr_database_create_element(db, "Configuration", e);
    psr_element_destroy(e);

    auto err = psr_database_update_scalar_string(db, "Configuration", "string_attribute", id, "world");
    EXPECT_EQ(err, PSR_OK);

    char* value = nullptr;
    int has_value;
    err = psr_database_read_scalar_strings_by_id(db, "Configuration", "string_attribute", id, &value, &has_value);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_STREQ(value, "world");

    delete[] value;
    psr_database_close(db);
}

// ============================================================================
// Update vector tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateVectorIntegers) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e, "value_int", values1, 3);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    int64_t new_values[] = {10, 20, 30, 40};
    auto err = psr_database_update_vector_integers(db, "Collection", "value_int", id, new_values, 4);
    EXPECT_EQ(err, PSR_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 4);
    EXPECT_EQ(read_values[0], 10);
    EXPECT_EQ(read_values[1], 20);
    EXPECT_EQ(read_values[2], 30);
    EXPECT_EQ(read_values[3], 40);

    psr_free_int_array(read_values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateVectorDoubles) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    double values1[] = {1.5, 2.5, 3.5};
    psr_element_set_array_double(e, "value_float", values1, 3);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    double new_values[] = {10.5, 20.5};
    auto err = psr_database_update_vector_doubles(db, "Collection", "value_float", id, new_values, 2);
    EXPECT_EQ(err, PSR_OK);

    double* read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_vector_doubles_by_id(db, "Collection", "value_float", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(read_values[0], 10.5);
    EXPECT_DOUBLE_EQ(read_values[1], 20.5);

    psr_free_double_array(read_values);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateVectorToEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    int64_t values1[] = {1, 2, 3};
    psr_element_set_array_int(e, "value_int", values1, 3);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    auto err = psr_database_update_vector_integers(db, "Collection", "value_int", id, nullptr, 0);
    EXPECT_EQ(err, PSR_OK);

    int64_t* read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_vector_integers_by_id(db, "Collection", "value_int", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    psr_database_close(db);
}

// ============================================================================
// Update set tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateSetStrings) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    psr_element_set_array_string(e, "tag", tags, 2);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    const char* new_tags[] = {"new_tag1", "new_tag2", "new_tag3"};
    auto err = psr_database_update_set_strings(db, "Collection", "tag", id, new_tags, 3);
    EXPECT_EQ(err, PSR_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 3);

    std::vector<std::string> set_values;
    for (size_t i = 0; i < count; i++) {
        set_values.push_back(read_values[i]);
    }
    std::sort(set_values.begin(), set_values.end());
    EXPECT_EQ(set_values[0], "new_tag1");
    EXPECT_EQ(set_values[1], "new_tag2");
    EXPECT_EQ(set_values[2], "new_tag3");

    psr_free_string_array(read_values, count);
    psr_database_close(db);
}

TEST_F(DatabaseFixture, UpdateSetToEmpty) {
    auto options = psr_database_options_default();
    options.console_level = PSR_LOG_OFF;
    auto db = psr_database_from_schema(":memory:", schema_path("schemas/valid/collections.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto config = psr_element_create();
    psr_element_set_string(config, "label", "Test Config");
    psr_database_create_element(db, "Configuration", config);
    psr_element_destroy(config);

    auto e = psr_element_create();
    psr_element_set_string(e, "label", "Item 1");
    const char* tags[] = {"important", "urgent"};
    psr_element_set_array_string(e, "tag", tags, 2);
    int64_t id = psr_database_create_element(db, "Collection", e);
    psr_element_destroy(e);

    auto err = psr_database_update_set_strings(db, "Collection", "tag", id, nullptr, 0);
    EXPECT_EQ(err, PSR_OK);

    char** read_values = nullptr;
    size_t count = 0;
    err = psr_database_read_set_strings_by_id(db, "Collection", "tag", id, &read_values, &count);
    EXPECT_EQ(err, PSR_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(read_values, nullptr);

    psr_database_close(db);
}
