#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/attribute_type.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Read scalar tests
// ============================================================================

TEST(Database, ReadScalarIntegers) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 42);
    EXPECT_EQ(values[1], 100);
}

TEST(Database, ReadScalarFloats) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("float_attribute", 2.71);
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_floats("Configuration", "float_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_DOUBLE_EQ(values[0], 3.14);
    EXPECT_DOUBLE_EQ(values[1], 2.71);
}

TEST(Database, ReadScalarStrings) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("string_attribute", std::string("world"));
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_strings("Configuration", "string_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], "hello");
    EXPECT_EQ(values[1], "world");
}

TEST(Database, ReadScalarEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto integers = db.read_scalar_integers("Collection", "some_integer");
    auto floats = db.read_scalar_floats("Collection", "some_float");
    auto strings = db.read_scalar_strings("Collection", "label");

    EXPECT_TRUE(integers.empty());
    EXPECT_TRUE(floats.empty());
    EXPECT_TRUE(strings.empty());
}

// ============================================================================
// Read vector tests
// ============================================================================

TEST(Database, ReadVectorIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vectors[1], (std::vector<int64_t>{10, 20}));
}

TEST(Database, ReadVectorFloats) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vectors[1], (std::vector<double>{10.5, 20.5}));
}

TEST(Database, ReadVectorEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto integer_vectors = db.read_vector_integers("Collection", "value_int");
    auto float_vectors = db.read_vector_floats("Collection", "value_float");

    EXPECT_TRUE(integer_vectors.empty());
    EXPECT_TRUE(float_vectors.empty());
}

TEST(Database, ReadVectorOnlyReturnsElementsWithData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with vector data
    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    // Element without vector data
    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with vector data
    quiver::Element e3;
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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    db.create_element("Collection", e1);

    quiver::Element e2;
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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(sets.empty());
}

TEST(Database, ReadSetOnlyReturnsElementsWithData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with set data
    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important"});
    db.create_element("Collection", e1);

    // Element without set data
    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with set data
    quiver::Element e3;
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
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    auto val1 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1);
    auto val2 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id2);

    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 42);
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 100);
}

TEST(Database, ReadScalarFloatById) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("float_attribute", 2.71);
    int64_t id2 = db.create_element("Configuration", e2);

    auto val1 = db.read_scalar_float_by_id("Configuration", "float_attribute", id1);
    auto val2 = db.read_scalar_float_by_id("Configuration", "float_attribute", id2);

    EXPECT_TRUE(val1.has_value());
    EXPECT_DOUBLE_EQ(*val1, 3.14);
    EXPECT_TRUE(val2.has_value());
    EXPECT_DOUBLE_EQ(*val2, 2.71);
}

TEST(Database, ReadScalarStringById) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("string_attribute", std::string("world"));
    int64_t id2 = db.create_element("Configuration", e2);

    auto val1 = db.read_scalar_string_by_id("Configuration", "string_attribute", id1);
    auto val2 = db.read_scalar_string_by_id("Configuration", "string_attribute", id2);

    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, "hello");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, "world");
}

TEST(Database, ReadScalarByIdNotFound) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e);

    // Non-existent ID
    auto val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", 999);
    EXPECT_FALSE(val.has_value());
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST(Database, ReadVectorIntegerById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_integers_by_id("Collection", "value_int", id1);
    auto vec2 = db.read_vector_integers_by_id("Collection", "value_int", id2);

    EXPECT_EQ(vec1, (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vec2, (std::vector<int64_t>{10, 20}));
}

TEST(Database, ReadVectorFloatById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_floats_by_id("Collection", "value_float", id1);
    auto vec2 = db.read_vector_floats_by_id("Collection", "value_float", id2);

    EXPECT_EQ(vec1, (std::vector<double>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vec2, (std::vector<double>{10.5, 20.5}));
}

TEST(Database, ReadVectorByIdEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));  // No vector data
    int64_t id = db.create_element("Collection", e);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

// ============================================================================
// Read set by ID tests
// ============================================================================

TEST(Database, ReadSetStringById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));  // No set data
    int64_t id = db.create_element("Collection", e);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set.empty());
}

// ============================================================================
// Read element IDs tests
// ============================================================================

TEST(Database, ReadElementIds) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    quiver::Element e3;
    e3.set("label", std::string("Config 3")).set("integer_attribute", int64_t{200});
    int64_t id3 = db.create_element("Configuration", e3);

    auto ids = db.read_element_ids("Configuration");
    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
    EXPECT_EQ(ids[2], id3);
}

TEST(Database, ReadElementIdsEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto ids = db.read_element_ids("Collection");
    EXPECT_TRUE(ids.empty());
}

// ============================================================================
// Invalid collection/attribute error tests
// ============================================================================

TEST(Database, ReadScalarIntegersInvalidCollection) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.read_scalar_integers("NonexistentCollection", "integer_attribute"), std::runtime_error);
}

TEST(Database, ReadScalarIntegersInvalidAttribute) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.read_scalar_integers("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadScalarFloatsInvalidCollection) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.read_scalar_floats("NonexistentCollection", "float_attribute"), std::runtime_error);
}

TEST(Database, ReadScalarStringsInvalidCollection) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.read_scalar_strings("NonexistentCollection", "string_attribute"), std::runtime_error);
}

TEST(Database, ReadVectorIntegersInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("NonexistentCollection", "value_int"), std::runtime_error);
}

TEST(Database, ReadVectorIntegersInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("Collection", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadSetStringsInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings("NonexistentCollection", "tag"), std::runtime_error);
}

TEST(Database, ReadSetStringsInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings("Collection", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadElementIdsInvalidCollection) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.read_element_ids("NonexistentCollection"), std::runtime_error);
}

TEST(Database, ReadScalarIntegerByIdInvalidCollection) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.read_scalar_integer_by_id("NonexistentCollection", "integer_attribute", 1), std::runtime_error);
}

TEST(Database, ReadVectorIntegerByIdInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers_by_id("NonexistentCollection", "value_int", 1), std::runtime_error);
}

TEST(Database, ReadSetStringsByIdInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings_by_id("NonexistentCollection", "tag", 1), std::runtime_error);
}

// ============================================================================
// DateTime metadata tests
// ============================================================================

TEST(Database, DateTimeAttributeMetadata) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    auto metadata = db.get_scalar_metadata("Configuration", "date_attribute");
    EXPECT_EQ(metadata.name, "date_attribute");
    EXPECT_EQ(metadata.data_type, quiver::DataType::DateTime);
    EXPECT_FALSE(metadata.not_null);
}

TEST(Database, DateTimeReadScalarString) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("date_attribute", std::string("2024-03-15T14:30:45"));
    db.create_element("Configuration", element);

    auto dates = db.read_scalar_strings("Configuration", "date_attribute");
    EXPECT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], "2024-03-15T14:30:45");
}

TEST(Database, DateTimeReadScalarStringById) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("date_attribute", std::string("2024-03-15T14:30:45"));
    int64_t id = db.create_element("Configuration", element);

    auto date = db.read_scalar_string_by_id("Configuration", "date_attribute", id);
    EXPECT_TRUE(date.has_value());
    EXPECT_EQ(date.value(), "2024-03-15T14:30:45");
}

TEST(Database, DateTimeNullable) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element element;
    element.set("label", std::string("Config 1"));
    int64_t id = db.create_element("Configuration", element);

    auto date = db.read_scalar_string_by_id("Configuration", "date_attribute", id);
    EXPECT_FALSE(date.has_value());
}

TEST(Database, DatetimeMetadataType) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    auto metadata = db.get_scalar_metadata("Configuration", "date_attribute");
    EXPECT_EQ(metadata.data_type, quiver::DataType::DateTime);
}
