#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/element.h>
#include <string>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

TEST_F(DatabaseFixture, OpenFileOnDisk) {
    psr::Database db(path);
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), path);
}

TEST_F(DatabaseFixture, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), ":memory:");
}

TEST_F(DatabaseFixture, DestructorClosesDatabase) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    // Database should be closed after destructor
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(DatabaseFixture, MoveConstructor) {
    psr::Database db1(path);
    EXPECT_TRUE(db1.is_healthy());

    psr::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(DatabaseFixture, MoveAssignment) {
    psr::Database db1(path);
    psr::Database db2(":memory:");

    db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(DatabaseFixture, LogLevelDebug) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::debug});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(DatabaseFixture, LogLevelOff) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(DatabaseFixture, CreatesFileOnDisk) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(DatabaseFixture, CreateElementWithScalars) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    // Create element
    psr::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42}).set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    // Verify
    auto result = db.execute("SELECT label, integer_attribute, float_attribute FROM Configuration WHERE id = ?", {id});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(0).value(), "Config 1");
    EXPECT_EQ(result[0].get_int(1).value(), 42);
    EXPECT_EQ(result[0].get_double(2).value(), 3.14);
}

TEST_F(DatabaseFixture, CreateElementWithVector) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    // Configuration required first
    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with vector array
    psr::Element element;
    element.set("label", std::string("Item 1"))
        .set("value_int", std::vector<int64_t>{1, 2, 3})
        .set("value_float", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify main table
    auto result = db.execute("SELECT label FROM Collection WHERE id = ?", {id});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(0).value(), "Item 1");

    // Verify vector table
    auto vec_result = db.execute(
        "SELECT vector_index, value_int, value_float FROM Collection_vector_values WHERE id = ? ORDER BY vector_index",
        {id});
    EXPECT_EQ(vec_result.row_count(), 3);
    EXPECT_EQ(vec_result[0].get_int(0).value(), 1);
    EXPECT_EQ(vec_result[0].get_int(1).value(), 1);
    EXPECT_EQ(vec_result[0].get_double(2).value(), 1.5);
}

TEST_F(DatabaseFixture, CreateElementWithVectorGroup) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    // Configuration required first
    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with vector group containing multiple attributes per row
    psr::Element element;
    element.set("label", std::string("Item 1"))
        .set("value_int", std::vector<int64_t>{10, 20, 30})
        .set("value_float", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify vector table with multiple attributes
    auto vec_result = db.execute(
        "SELECT vector_index, value_int, value_float FROM Collection_vector_values WHERE id = ? ORDER BY vector_index",
        {id});
    EXPECT_EQ(vec_result.row_count(), 3);

    EXPECT_EQ(vec_result[0].get_int(0).value(), 1);
    EXPECT_EQ(vec_result[0].get_int(1).value(), 10);
    EXPECT_EQ(vec_result[0].get_double(2).value(), 1.5);

    EXPECT_EQ(vec_result[1].get_int(0).value(), 2);
    EXPECT_EQ(vec_result[1].get_int(1).value(), 20);
    EXPECT_EQ(vec_result[1].get_double(2).value(), 2.5);

    EXPECT_EQ(vec_result[2].get_int(0).value(), 3);
    EXPECT_EQ(vec_result[2].get_int(1).value(), 30);
    EXPECT_EQ(vec_result[2].get_double(2).value(), 3.5);
}

TEST_F(DatabaseFixture, CreateElementWithSetGroup) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    // Configuration required first
    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with set attribute
    psr::Element element;
    element.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent", "review"});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify set table
    auto set_result = db.execute("SELECT tag FROM Collection_set_tags WHERE id = ? ORDER BY tag", {id});
    EXPECT_EQ(set_result.row_count(), 3);

    EXPECT_EQ(set_result[0].get_string(0).value(), "important");
    EXPECT_EQ(set_result[1].get_string(0).value(), "review");
    EXPECT_EQ(set_result[2].get_string(0).value(), "urgent");
}

TEST_F(DatabaseFixture, CreateMultipleElements) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    // Create multiple Configuration elements
    psr::Element e1;
    e1.set("label", std::string("Config A")).set("integer_attribute", int64_t{100});
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config B")).set("integer_attribute", int64_t{200});
    int64_t id2 = db.create_element("Configuration", e2);

    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);

    auto result = db.execute("SELECT COUNT(*) FROM Configuration");
    EXPECT_EQ(result[0].get_int(0).value(), 2);
}

TEST_F(DatabaseFixture, CreateTable) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
    db.execute("INSERT INTO test (value) VALUES (?)", {std::string("hello")});

    auto result = db.execute("SELECT * FROM test");
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(1).value(), "hello");
}

TEST_F(DatabaseFixture, CurrentVersion) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_EQ(db.current_version(), 0);
}

TEST_F(DatabaseFixture, ReadScalarInts) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_ints("Configuration", "integer_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 42);
    EXPECT_EQ(values[1], 100);
}

TEST_F(DatabaseFixture, ReadScalarDoubles) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("float_attribute", 2.71);
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_doubles("Configuration", "float_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_DOUBLE_EQ(values[0], 3.14);
    EXPECT_DOUBLE_EQ(values[1], 2.71);
}

TEST_F(DatabaseFixture, ReadScalarStrings) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("string_attribute", std::string("world"));
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_strings("Configuration", "string_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], "hello");
    EXPECT_EQ(values[1], "world");
}

TEST_F(DatabaseFixture, ReadScalarEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto ints = db.read_scalar_ints("Collection", "some_integer");
    auto doubles = db.read_scalar_doubles("Collection", "some_float");
    auto strings = db.read_scalar_strings("Collection", "label");

    EXPECT_TRUE(ints.empty());
    EXPECT_TRUE(doubles.empty());
    EXPECT_TRUE(strings.empty());
}

TEST_F(DatabaseFixture, ReadVectorInts) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_ints("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vectors[1], (std::vector<int64_t>{10, 20}));
}

TEST_F(DatabaseFixture, ReadVectorDoubles) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_doubles("Collection", "value_float");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vectors[1], (std::vector<double>{10.5, 20.5}));
}

TEST_F(DatabaseFixture, ReadVectorEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto int_vectors = db.read_vector_ints("Collection", "value_int");
    auto double_vectors = db.read_vector_doubles("Collection", "value_float");

    EXPECT_TRUE(int_vectors.empty());
    EXPECT_TRUE(double_vectors.empty());
}

TEST_F(DatabaseFixture, ReadVectorOnlyReturnsElementsWithData) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with vector data
    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    // Element without vector data
    psr::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with vector data
    psr::Element e3;
    e3.set("label", std::string("Item 3")).set("value_int", std::vector<int64_t>{4, 5});
    db.create_element("Collection", e3);

    // Only elements with vector data are returned
    auto vectors = db.read_vector_ints("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vectors[1], (std::vector<int64_t>{4, 5}));
}

TEST_F(DatabaseFixture, ReadSetStrings) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("tag", std::vector<std::string>{"review"});
    db.create_element("Collection", e2);

    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 2);
    // Sets are unordered, so sort before comparison
    auto set1 = sets[0];
    auto set2 = sets[1];
    std::sort(set1.begin(), set1.end());
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set1, (std::vector<std::string>{"important", "urgent"}));
    EXPECT_EQ(set2, (std::vector<std::string>{"review"}));
}

TEST_F(DatabaseFixture, ReadSetEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(sets.empty());
}

TEST_F(DatabaseFixture, ReadSetOnlyReturnsElementsWithData) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with set data
    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important"});
    db.create_element("Collection", e1);

    // Element without set data
    psr::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with set data
    psr::Element e3;
    e3.set("label", std::string("Item 3")).set("tag", std::vector<std::string>{"urgent", "review"});
    db.create_element("Collection", e3);

    // Only elements with set data are returned
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 2);
}
