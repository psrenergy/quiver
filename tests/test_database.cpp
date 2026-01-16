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

    // Verify using public read APIs
    auto labels = db.read_scalar_strings("Configuration", "label");
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    auto floats = db.read_scalar_doubles("Configuration", "float_attribute");

    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Config 1");
    EXPECT_EQ(integers[0], 42);
    EXPECT_EQ(floats[0], 3.14);
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

    // Verify using public read APIs
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto int_vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(int_vectors.size(), 1);
    EXPECT_EQ(int_vectors[0], (std::vector<int64_t>{1, 2, 3}));

    auto float_vectors = db.read_vector_doubles("Collection", "value_float");
    EXPECT_EQ(float_vectors.size(), 1);
    EXPECT_EQ(float_vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
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

    // Verify using public read APIs
    auto int_vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(int_vectors.size(), 1);
    EXPECT_EQ(int_vectors[0], (std::vector<int64_t>{10, 20, 30}));

    auto float_vectors = db.read_vector_doubles("Collection", "value_float");
    EXPECT_EQ(float_vectors.size(), 1);
    EXPECT_EQ(float_vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
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

    // Verify using public read APIs
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 1);
    auto tags = sets[0];
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::string>{"important", "review", "urgent"}));
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

    // Verify using public read APIs
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 2);
}

TEST_F(DatabaseFixture, CurrentVersion) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_EQ(db.current_version(), 0);
}

TEST_F(DatabaseFixture, ReadScalarIntegers) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_integers("Configuration", "integer_attribute");
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
    auto integers = db.read_scalar_integers("Collection", "some_integer");
    auto doubles = db.read_scalar_doubles("Collection", "some_float");
    auto strings = db.read_scalar_strings("Collection", "label");

    EXPECT_TRUE(integers.empty());
    EXPECT_TRUE(doubles.empty());
    EXPECT_TRUE(strings.empty());
}

TEST_F(DatabaseFixture, ReadVectorIntegers) {
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

    auto vectors = db.read_vector_integers("Collection", "value_int");
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
    auto int_vectors = db.read_vector_integers("Collection", "value_int");
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
    auto vectors = db.read_vector_integers("Collection", "value_int");
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

// ============================================================================
// set_scalar_relation tests
// ============================================================================

TEST_F(DatabaseFixture, SetScalarRelation) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/relations.sql"), {.console_level = psr::LogLevel::off});

    // Create parent
    psr::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child without relation
    psr::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Set the relation
    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    // Verify the relation was set using public read API
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 1);
    EXPECT_EQ(relations[0], "Parent 1");
}

TEST_F(DatabaseFixture, SetScalarRelationSelfReference) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/relations.sql"), {.console_level = psr::LogLevel::off});

    // Create two children
    psr::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    psr::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Set self-referential relation (sibling)
    db.set_scalar_relation("Child", "sibling_id", "Child 1", "Child 2");

    // Verify the relation was set using public read API
    auto relations = db.read_scalar_relation("Child", "sibling_id");
    EXPECT_EQ(relations.size(), 2);
    // Child 1 has sibling_id pointing to Child 2, Child 2 has no sibling
    EXPECT_EQ(relations[0], "Child 2");
    EXPECT_EQ(relations[1], "");  // Child 2 has no sibling set
}

// ============================================================================
// Read scalar by ID tests
// ============================================================================

TEST_F(DatabaseFixture, ReadScalarIntegerById) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    auto val1 = db.read_scalar_integers_by_id("Configuration", "integer_attribute", id1);
    auto val2 = db.read_scalar_integers_by_id("Configuration", "integer_attribute", id2);

    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 42);
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 100);
}

TEST_F(DatabaseFixture, ReadScalarDoubleById) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("float_attribute", 2.71);
    int64_t id2 = db.create_element("Configuration", e2);

    auto val1 = db.read_scalar_doubles_by_id("Configuration", "float_attribute", id1);
    auto val2 = db.read_scalar_doubles_by_id("Configuration", "float_attribute", id2);

    EXPECT_TRUE(val1.has_value());
    EXPECT_DOUBLE_EQ(*val1, 3.14);
    EXPECT_TRUE(val2.has_value());
    EXPECT_DOUBLE_EQ(*val2, 2.71);
}

TEST_F(DatabaseFixture, ReadScalarStringById) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("string_attribute", std::string("world"));
    int64_t id2 = db.create_element("Configuration", e2);

    auto val1 = db.read_scalar_strings_by_id("Configuration", "string_attribute", id1);
    auto val2 = db.read_scalar_strings_by_id("Configuration", "string_attribute", id2);

    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, "hello");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, "world");
}

TEST_F(DatabaseFixture, ReadScalarByIdNotFound) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e);

    // Non-existent ID
    auto val = db.read_scalar_integers_by_id("Configuration", "integer_attribute", 999);
    EXPECT_FALSE(val.has_value());
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST_F(DatabaseFixture, ReadVectorIntegerById) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id1 = db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_integers_by_id("Collection", "value_int", id1);
    auto vec2 = db.read_vector_integers_by_id("Collection", "value_int", id2);

    EXPECT_EQ(vec1, (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vec2, (std::vector<int64_t>{10, 20}));
}

TEST_F(DatabaseFixture, ReadVectorDoubleById) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id1 = db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_doubles_by_id("Collection", "value_float", id1);
    auto vec2 = db.read_vector_doubles_by_id("Collection", "value_float", id2);

    EXPECT_EQ(vec1, (std::vector<double>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vec2, (std::vector<double>{10.5, 20.5}));
}

TEST_F(DatabaseFixture, ReadVectorByIdEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1"));  // No vector data
    int64_t id = db.create_element("Collection", e);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

// ============================================================================
// Read set by ID tests
// ============================================================================

TEST_F(DatabaseFixture, ReadSetStringById) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id1 = db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("tag", std::vector<std::string>{"review"});
    int64_t id2 = db.create_element("Collection", e2);

    auto set1 = db.read_set_strings_by_id("Collection", "tag", id1);
    auto set2 = db.read_set_strings_by_id("Collection", "tag", id2);

    // Sets are unordered, so sort before comparison
    std::sort(set1.begin(), set1.end());
    EXPECT_EQ(set1, (std::vector<std::string>{"important", "urgent"}));
    EXPECT_EQ(set2, (std::vector<std::string>{"review"}));
}

TEST_F(DatabaseFixture, ReadSetByIdEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1"));  // No set data
    int64_t id = db.create_element("Collection", e);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set.empty());
}

// ============================================================================
// Read element IDs tests
// ============================================================================

TEST_F(DatabaseFixture, ReadElementIds) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    psr::Element e3;
    e3.set("label", std::string("Config 3")).set("integer_attribute", int64_t{200});
    int64_t id3 = db.create_element("Configuration", e3);

    auto ids = db.read_element_ids("Configuration");
    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
    EXPECT_EQ(ids[2], id3);
}

TEST_F(DatabaseFixture, ReadElementIdsEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto ids = db.read_element_ids("Collection");
    EXPECT_TRUE(ids.empty());
}

// ============================================================================
// Update scalar tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateScalarInteger) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_integer("Configuration", "integer_attribute", id, 100);

    auto val = db.read_scalar_integers_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 100);
}

TEST_F(DatabaseFixture, UpdateScalarDouble) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_double("Configuration", "float_attribute", id, 2.71);

    auto val = db.read_scalar_doubles_by_id("Configuration", "float_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 2.71);
}

TEST_F(DatabaseFixture, UpdateScalarString) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_string("Configuration", "string_attribute", id, "world");

    auto val = db.read_scalar_strings_by_id("Configuration", "string_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, "world");
}

TEST_F(DatabaseFixture, UpdateScalarMultipleElements) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    // Update only first element
    db.update_scalar_integer("Configuration", "integer_attribute", id1, 999);

    // Verify first element changed
    auto val1 = db.read_scalar_integers_by_id("Configuration", "integer_attribute", id1);
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 999);

    // Verify second element unchanged
    auto val2 = db.read_scalar_integers_by_id("Configuration", "integer_attribute", id2);
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 100);
}

// ============================================================================
// Update vector tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateVectorIntegers) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    db.update_vector_integers("Collection", "value_int", id, {10, 20, 30, 40});

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{10, 20, 30, 40}));
}

TEST_F(DatabaseFixture, UpdateVectorDoubles) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id = db.create_element("Collection", e);

    db.update_vector_doubles("Collection", "value_float", id, {10.5, 20.5});

    auto vec = db.read_vector_doubles_by_id("Collection", "value_float", id);
    EXPECT_EQ(vec, (std::vector<double>{10.5, 20.5}));
}

TEST_F(DatabaseFixture, UpdateVectorToEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    db.update_vector_integers("Collection", "value_int", id, {});

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

TEST_F(DatabaseFixture, UpdateVectorMultipleElements) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id1 = db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    int64_t id2 = db.create_element("Collection", e2);

    // Update only first element
    db.update_vector_integers("Collection", "value_int", id1, {100, 200});

    // Verify first element changed
    auto vec1 = db.read_vector_integers_by_id("Collection", "value_int", id1);
    EXPECT_EQ(vec1, (std::vector<int64_t>{100, 200}));

    // Verify second element unchanged
    auto vec2 = db.read_vector_integers_by_id("Collection", "value_int", id2);
    EXPECT_EQ(vec2, (std::vector<int64_t>{10, 20}));
}

// ============================================================================
// Update set tests
// ============================================================================

TEST_F(DatabaseFixture, UpdateSetStrings) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    db.update_set_strings("Collection", "tag", id, {"new_tag1", "new_tag2", "new_tag3"});

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"new_tag1", "new_tag2", "new_tag3"}));
}

TEST_F(DatabaseFixture, UpdateSetToEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    db.update_set_strings("Collection", "tag", id, {});

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set.empty());
}

TEST_F(DatabaseFixture, UpdateSetMultipleElements) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("schemas/valid/collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    psr::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important"});
    int64_t id1 = db.create_element("Collection", e1);

    psr::Element e2;
    e2.set("label", std::string("Item 2")).set("tag", std::vector<std::string>{"urgent", "review"});
    int64_t id2 = db.create_element("Collection", e2);

    // Update only first element
    db.update_set_strings("Collection", "tag", id1, {"updated"});

    // Verify first element changed
    auto set1 = db.read_set_strings_by_id("Collection", "tag", id1);
    EXPECT_EQ(set1, (std::vector<std::string>{"updated"}));

    // Verify second element unchanged
    auto set2 = db.read_set_strings_by_id("Collection", "tag", id2);
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set2, (std::vector<std::string>{"review", "urgent"}));
}
