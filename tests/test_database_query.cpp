#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Query string tests
// ============================================================================

TEST(DatabaseQuery, QueryStringReturnsValue) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test Label")).set("string_attribute", std::string("hello world"));
    db.create_element("Configuration", e);

    auto result = db.query_string("SELECT string_attribute FROM Configuration WHERE label = 'Test Label'");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello world");
}

TEST(DatabaseQuery, QueryStringReturnsNulloptWhenEmpty) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    auto result = db.query_string("SELECT string_attribute FROM Configuration WHERE 1 = 0");
    EXPECT_FALSE(result.has_value());
}

TEST(DatabaseQuery, QueryStringReturnsFirstRow) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("First")).set("string_attribute", std::string("first value"));
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Second")).set("string_attribute", std::string("second value"));
    db.create_element("Configuration", e2);

    auto result = db.query_string("SELECT string_attribute FROM Configuration ORDER BY label");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "first value");
}

// ============================================================================
// Query integer tests
// ============================================================================

TEST(DatabaseQuery, QueryIntegerReturnsValue) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e);

    auto result = db.query_integer("SELECT integer_attribute FROM Configuration WHERE label = 'Test'");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(DatabaseQuery, QueryIntegerReturnsNulloptWhenEmpty) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    auto result = db.query_integer("SELECT integer_attribute FROM Configuration WHERE 1 = 0");
    EXPECT_FALSE(result.has_value());
}

TEST(DatabaseQuery, QueryIntegerReturnsFirstRow) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("A")).set("integer_attribute", int64_t{100});
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("B")).set("integer_attribute", int64_t{200});
    db.create_element("Configuration", e2);

    auto result = db.query_integer("SELECT integer_attribute FROM Configuration ORDER BY label");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100);
}

TEST(DatabaseQuery, QueryIntegerCount) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("A"));
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("B"));
    db.create_element("Configuration", e2);

    auto result = db.query_integer("SELECT COUNT(*) FROM Configuration");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2);
}

// ============================================================================
// Query float tests
// ============================================================================

TEST(DatabaseQuery, QueryFloatReturnsValue) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test")).set("float_attribute", 3.14159);
    db.create_element("Configuration", e);

    auto result = db.query_float("SELECT float_attribute FROM Configuration WHERE label = 'Test'");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14159);
}

TEST(DatabaseQuery, QueryFloatReturnsNulloptWhenEmpty) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    auto result = db.query_float("SELECT float_attribute FROM Configuration WHERE 1 = 0");
    EXPECT_FALSE(result.has_value());
}

TEST(DatabaseQuery, QueryFloatReturnsFirstRow) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("A")).set("float_attribute", 1.5);
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("B")).set("float_attribute", 2.5);
    db.create_element("Configuration", e2);

    auto result = db.query_float("SELECT float_attribute FROM Configuration ORDER BY label");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1.5);
}

TEST(DatabaseQuery, QueryFloatAverage) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("A")).set("float_attribute", 10.0);
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("B")).set("float_attribute", 20.0);
    db.create_element("Configuration", e2);

    auto result = db.query_float("SELECT AVG(float_attribute) FROM Configuration");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 15.0);
}

// ============================================================================
// Parameterized query tests
// ============================================================================

TEST(DatabaseQuery, QueryStringWithParams) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test Label")).set("string_attribute", std::string("hello world"));
    db.create_element("Configuration", e);

    auto result =
        db.query_string("SELECT string_attribute FROM Configuration WHERE label = ?", {std::string("Test Label")});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello world");
}

TEST(DatabaseQuery, QueryStringWithParamsNoMatch) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test")).set("string_attribute", std::string("hello"));
    db.create_element("Configuration", e);

    auto result =
        db.query_string("SELECT string_attribute FROM Configuration WHERE label = ?", {std::string("NoMatch")});
    EXPECT_FALSE(result.has_value());
}

TEST(DatabaseQuery, QueryIntegerWithParams) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e);

    auto result = db.query_integer("SELECT integer_attribute FROM Configuration WHERE label = ?", {std::string("Test")});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(DatabaseQuery, QueryFloatWithParams) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test")).set("float_attribute", 3.14);
    db.create_element("Configuration", e);

    auto result = db.query_float("SELECT float_attribute FROM Configuration WHERE label = ?", {std::string("Test")});
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14);
}

TEST(DatabaseQuery, QueryIntegerWithMultipleParams) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e1;
    e1.set("label", std::string("A")).set("integer_attribute", int64_t{10});
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("B")).set("integer_attribute", int64_t{20});
    db.create_element("Configuration", e2);

    auto result = db.query_integer(
        "SELECT integer_attribute FROM Configuration WHERE label = ? AND integer_attribute > ?",
        {std::string("B"), int64_t{5}});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 20);
}

TEST(DatabaseQuery, QueryWithNullParam) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    auto result = db.query_integer("SELECT COUNT(*) FROM Configuration WHERE ? IS NULL", {nullptr});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
}
