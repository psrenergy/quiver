#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Read scalar tests
// ============================================================================

TEST(Database, ReadScalarIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("float_attribute", 2.71);
    db.create_element("Configuration", e2);

    auto values = db.read_scalar_floats("Configuration", "float_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_DOUBLE_EQ(*values[0], 3.14);
    EXPECT_DOUBLE_EQ(*values[1], 2.71);
}

TEST(Database, ReadScalarStrings) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
// NULL handling in bulk scalar reads (regression: NULLs must not be dropped)
// ============================================================================

TEST(Database, ReadScalarFloatsPreservesNull) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // float_attribute has no default, so an unset value is SQL NULL.
    db.create_element("Configuration",
                      quiver::Element().set("label", std::string("Config 1")).set("float_attribute", 10.0));
    db.create_element("Configuration", quiver::Element().set("label", std::string("Config 2")));  // NULL float
    db.create_element("Configuration",
                      quiver::Element().set("label", std::string("Config 3")).set("float_attribute", 30.0));
    db.create_element("Configuration",
                      quiver::Element().set("label", std::string("Config 4")).set("float_attribute", 40.0));

    auto values = db.read_scalar_floats("Configuration", "float_attribute");
    // One entry per element; the NULL occupies its slot positionally.
    ASSERT_EQ(values.size(), 4u);
    ASSERT_TRUE(values[0].has_value());
    EXPECT_DOUBLE_EQ(*values[0], 10.0);
    EXPECT_FALSE(values[1].has_value());
    ASSERT_TRUE(values[2].has_value());
    EXPECT_DOUBLE_EQ(*values[2], 30.0);
    ASSERT_TRUE(values[3].has_value());
    EXPECT_DOUBLE_EQ(*values[3], 40.0);
}

TEST(Database, ReadScalarIntegersPreservesNull) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // AllTypes.some_integer has no default, so an unset value is SQL NULL.
    db.create_element("AllTypes", quiver::Element().set("label", std::string("a")).set("some_integer", int64_t{10}));
    db.create_element("AllTypes", quiver::Element().set("label", std::string("b")));  // NULL integer
    db.create_element("AllTypes", quiver::Element().set("label", std::string("c")).set("some_integer", int64_t{30}));

    auto values = db.read_scalar_integers("AllTypes", "some_integer");
    ASSERT_EQ(values.size(), 3u);
    ASSERT_TRUE(values[0].has_value());
    EXPECT_EQ(*values[0], 10);
    EXPECT_FALSE(values[1].has_value());
    ASSERT_TRUE(values[2].has_value());
    EXPECT_EQ(*values[2], 30);
}

TEST(Database, ReadScalarStringsPreservesNull) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.create_element(
        "Configuration",
        quiver::Element().set("label", std::string("Config 1")).set("string_attribute", std::string("hello")));
    db.create_element("Configuration", quiver::Element().set("label", std::string("Config 2")));  // NULL string
    db.create_element(
        "Configuration",
        quiver::Element().set("label", std::string("Config 3")).set("string_attribute", std::string("world")));

    auto values = db.read_scalar_strings("Configuration", "string_attribute");
    ASSERT_EQ(values.size(), 3u);
    ASSERT_TRUE(values[0].has_value());
    EXPECT_EQ(*values[0], "hello");
    EXPECT_FALSE(values[1].has_value());
    ASSERT_TRUE(values[2].has_value());
    EXPECT_EQ(*values[2], "world");
}

TEST(Database, ReadScalarFloatsAllNull) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.create_element("Configuration", quiver::Element().set("label", std::string("Config 1")));
    db.create_element("Configuration", quiver::Element().set("label", std::string("Config 2")));

    auto values = db.read_scalar_floats("Configuration", "float_attribute");
    // All-NULL column: full-length result, every slot empty (not an empty vector).
    ASSERT_EQ(values.size(), 2u);
    EXPECT_FALSE(values[0].has_value());
    EXPECT_FALSE(values[1].has_value());
}

// ============================================================================
// Read scalar by ID tests
// ============================================================================

TEST(Database, ReadScalarIntegerById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e);

    // Non-existent ID
    auto val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", 999);
    EXPECT_FALSE(val.has_value());
}

// ============================================================================
// Read element Ids tests
// ============================================================================

TEST(Database, ReadElementIds) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_integers("NonexistentCollection", "integer_attribute"), std::runtime_error);
}

TEST(Database, ReadScalarIntegersInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_integers("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadScalarFloatsInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_floats("NonexistentCollection", "float_attribute"), std::runtime_error);
}

TEST(Database, ReadScalarStringsInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_strings("NonexistentCollection", "string_attribute"), std::runtime_error);
}

TEST(Database, ReadElementIdsInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_element_ids("NonexistentCollection"), std::runtime_error);
}

TEST(Database, ReadScalarIntegerByIdInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_integer_by_id("NonexistentCollection", "integer_attribute", 1), std::runtime_error);
}

// ============================================================================
// Ordering tests
// ============================================================================

TEST(Database, ReadScalarOrderFollowsInsertionOrder) {
    // The btree in SQLite tends to add elements in a specific order in memory.
    // If ORDER BY is not specified in the read queries, we may get results in insertion order, but this is not
    // guaranteed. There should always be an ORDER BY clause in the read queries to ensure consistent ordering.
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Insert 10 elements with labels in reverse alphabetical order (z -> a)
    // Expected: reads return values in insertion (ID) order
    std::vector<std::string> labels = {
        "zebra", "yak", "wolf", "viper", "urchin", "tiger", "snake", "rabbit", "quail", "penguin"};
    std::vector<int64_t> expected_integers;
    std::vector<double> expected_floats;

    for (int i = 0; i < 10; ++i) {
        quiver::Element e;
        int64_t int_val = static_cast<int64_t>((i + 1) * 10);
        double float_val = (i + 1) * 1.5;
        e.set("label", labels[i]).set("integer_attribute", int_val).set("float_attribute", float_val);
        db.create_element("Configuration", e);
        expected_integers.push_back(int_val);
        expected_floats.push_back(float_val);
    }

    auto some_labels = db.read_scalar_strings("Configuration", "label");
    auto ids_from_scalar_integers = db.read_scalar_integers("Configuration", "id");
    auto ids_from_read_element_ids = db.read_element_ids("Configuration");
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    auto floats = db.read_scalar_floats("Configuration", "float_attribute");

    ASSERT_EQ(integers.size(), 10);
    ASSERT_EQ(floats.size(), 10);

    // zebra
    EXPECT_EQ(some_labels[0], "zebra");
    EXPECT_EQ(ids_from_scalar_integers[0], 1);
    EXPECT_EQ(ids_from_read_element_ids[0], 1);
    EXPECT_EQ(integers[0], expected_integers[0]);
    EXPECT_DOUBLE_EQ(*floats[0], expected_floats[0]);

    // tiger
    EXPECT_EQ(some_labels[5], "tiger");
    EXPECT_EQ(ids_from_scalar_integers[5], 6);
    EXPECT_EQ(ids_from_read_element_ids[5], 6);
    EXPECT_EQ(integers[5], expected_integers[5]);
    EXPECT_DOUBLE_EQ(*floats[5], expected_floats[5]);

    // penguin
    EXPECT_EQ(some_labels[9], "penguin");
    EXPECT_EQ(ids_from_scalar_integers[9], 10);
    EXPECT_EQ(ids_from_read_element_ids[9], 10);
    EXPECT_EQ(integers[9], expected_integers[9]);
    EXPECT_DOUBLE_EQ(*floats[9], expected_floats[9]);
}

// ============================================================================
// DateTime metadata tests
// ============================================================================

TEST(Database, DateTimeAttributeMetadata) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_scalar_metadata("Configuration", "date_attribute");
    EXPECT_EQ(metadata.name, "date_attribute");
    EXPECT_EQ(metadata.data_type, quiver::DataType::DateTime);
    EXPECT_FALSE(metadata.not_null);
}

TEST(Database, DateTimeReadScalarString) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("date_attribute", std::string("2024-03-15T14:30:45"));
    db.create_element("Configuration", element);

    auto dates = db.read_scalar_strings("Configuration", "date_attribute");
    EXPECT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], "2024-03-15T14:30:45");
}

TEST(Database, DateTimeReadScalarStringById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("date_attribute", std::string("2024-03-15T14:30:45"));
    int64_t id = db.create_element("Configuration", element);

    auto date = db.read_scalar_string_by_id("Configuration", "date_attribute", id);
    EXPECT_TRUE(date.has_value());
    EXPECT_EQ(date.value(), "2024-03-15T14:30:45");
}

TEST(Database, DateTimeNullable) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Config 1"));
    int64_t id = db.create_element("Configuration", element);

    auto date = db.read_scalar_string_by_id("Configuration", "date_attribute", id);
    EXPECT_FALSE(date.has_value());
}

TEST(Database, DatetimeMetadataType) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_scalar_metadata("Configuration", "date_attribute");
    EXPECT_EQ(metadata.data_type, quiver::DataType::DateTime);
}

// ============================================================================
// Identifier validation tests
// ============================================================================

TEST(Database, ReadScalarIntegersInvalidColumnThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(
        {
            try {
                db.read_scalar_integers("Configuration", "nonexistent_column");
            } catch (const std::runtime_error& e) {
                EXPECT_THAT(std::string(e.what()), testing::HasSubstr("not found"));
                throw;
            }
        },
        std::runtime_error);
}
