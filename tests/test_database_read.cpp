#include "test_utils.h"

#include <gtest/gtest.h>
#include <psr/attribute_type.h>
#include <psr/database.h>
#include <psr/element.h>

// ============================================================================
// Read scalar tests
// ============================================================================

TEST(Database, ReadScalarIntegers) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadScalarDoubles) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadScalarStrings) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadScalarEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

// ============================================================================
// Read vector tests
// ============================================================================

TEST(Database, ReadVectorIntegers) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadVectorDoubles) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadVectorEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto int_vectors = db.read_vector_integers("Collection", "value_int");
    auto double_vectors = db.read_vector_doubles("Collection", "value_float");

    EXPECT_TRUE(int_vectors.empty());
    EXPECT_TRUE(double_vectors.empty());
}

TEST(Database, ReadVectorOnlyReturnsElementsWithData) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

// ============================================================================
// Read set tests
// ============================================================================

TEST(Database, ReadSetStrings) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadSetEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(sets.empty());
}

TEST(Database, ReadSetOnlyReturnsElementsWithData) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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
// Read scalar by ID tests
// ============================================================================

TEST(Database, ReadScalarIntegerById) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadScalarDoubleById) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadScalarStringById) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadScalarByIdNotFound) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadVectorIntegerById) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadVectorDoubleById) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadVectorByIdEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadSetStringById) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadSetByIdEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadElementIds) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

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

TEST(Database, ReadElementIdsEmpty) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto ids = db.read_element_ids("Collection");
    EXPECT_TRUE(ids.empty());
}

// ============================================================================
// Get attribute type tests
// ============================================================================

TEST(Database, GetAttributeTypeScalarInteger) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    auto attr_type = db.get_attribute_type("Configuration", "integer_attribute");
    EXPECT_EQ(attr_type.structure, psr::AttributeStructure::Scalar);
    EXPECT_EQ(attr_type.data_type, psr::AttributeDataType::Integer);
}

TEST(Database, GetAttributeTypeScalarReal) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    auto attr_type = db.get_attribute_type("Configuration", "float_attribute");
    EXPECT_EQ(attr_type.structure, psr::AttributeStructure::Scalar);
    EXPECT_EQ(attr_type.data_type, psr::AttributeDataType::Real);
}

TEST(Database, GetAttributeTypeScalarText) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    auto attr_type = db.get_attribute_type("Configuration", "string_attribute");
    EXPECT_EQ(attr_type.structure, psr::AttributeStructure::Scalar);
    EXPECT_EQ(attr_type.data_type, psr::AttributeDataType::Text);
}

TEST(Database, GetAttributeTypeVectorInteger) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    auto attr_type = db.get_attribute_type("Collection", "value_int");
    EXPECT_EQ(attr_type.structure, psr::AttributeStructure::Vector);
    EXPECT_EQ(attr_type.data_type, psr::AttributeDataType::Integer);
}

TEST(Database, GetAttributeTypeVectorReal) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    auto attr_type = db.get_attribute_type("Collection", "value_float");
    EXPECT_EQ(attr_type.structure, psr::AttributeStructure::Vector);
    EXPECT_EQ(attr_type.data_type, psr::AttributeDataType::Real);
}

TEST(Database, GetAttributeTypeSetText) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    auto attr_type = db.get_attribute_type("Collection", "tag");
    EXPECT_EQ(attr_type.structure, psr::AttributeStructure::Set);
    EXPECT_EQ(attr_type.data_type, psr::AttributeDataType::Text);
}

TEST(Database, GetAttributeTypeNotFound) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.get_attribute_type("Configuration", "nonexistent"), std::runtime_error);
}

TEST(Database, GetAttributeTypeCollectionNotFound) {
    auto db = psr::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.get_attribute_type("NonexistentCollection", "label"), std::runtime_error);
}
