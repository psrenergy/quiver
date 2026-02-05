#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/result.h>
#include <quiver/row.h>

// ============================================================================
// Row boundary tests
// ============================================================================

TEST(Row, EmptyRow) {
    quiver::Row row(std::vector<quiver::Value>{});

    EXPECT_TRUE(row.empty());
    EXPECT_EQ(row.size(), 0u);
    EXPECT_EQ(row.column_count(), 0u);
}

TEST(Row, AtOutOfBounds) {
    quiver::Row row(std::vector<quiver::Value>{int64_t{42}});

    EXPECT_THROW(row.at(1), std::out_of_range);
    EXPECT_THROW(row.at(100), std::out_of_range);
}

TEST(Row, OperatorBracketValidIndex) {
    quiver::Row row(std::vector<quiver::Value>{int64_t{42}, std::string("test"), 3.14});

    // Access valid indices
    EXPECT_TRUE(std::holds_alternative<int64_t>(row[0]));
    EXPECT_TRUE(std::holds_alternative<std::string>(row[1]));
    EXPECT_TRUE(std::holds_alternative<double>(row[2]));
}

TEST(Row, IsNullTrueForNullValue) {
    quiver::Row row(std::vector<quiver::Value>{nullptr});

    EXPECT_TRUE(row.is_null(0));
}

TEST(Row, IsNullFalseForNonNull) {
    quiver::Row row(std::vector<quiver::Value>{int64_t{42}});

    EXPECT_FALSE(row.is_null(0));
}

TEST(Row, GetIntWrongType) {
    quiver::Row row(std::vector<quiver::Value>{std::string("not an int")});

    auto result = row.get_integer(0);
    EXPECT_FALSE(result.has_value());
}

TEST(Row, GetFloatWrongType) {
    quiver::Row row(std::vector<quiver::Value>{std::string("not a float")});

    auto result = row.get_float(0);
    EXPECT_FALSE(result.has_value());
}

TEST(Row, GetStringWrongType) {
    quiver::Row row(std::vector<quiver::Value>{int64_t{42}});

    auto result = row.get_string(0);
    EXPECT_FALSE(result.has_value());
}

TEST(Row, GetIntFromNull) {
    quiver::Row row(std::vector<quiver::Value>{nullptr});

    auto result = row.get_integer(0);
    EXPECT_FALSE(result.has_value());
}

TEST(Row, GetFloatFromNull) {
    quiver::Row row(std::vector<quiver::Value>{nullptr});

    auto result = row.get_float(0);
    EXPECT_FALSE(result.has_value());
}

TEST(Row, GetStringFromNull) {
    quiver::Row row(std::vector<quiver::Value>{nullptr});

    auto result = row.get_string(0);
    EXPECT_FALSE(result.has_value());
}

TEST(Row, IteratorSupport) {
    std::vector<quiver::Value> values = {int64_t{1}, int64_t{2}, int64_t{3}};
    quiver::Row row(values);

    int count = 0;
    for (const auto& val : row) {
        EXPECT_TRUE(std::holds_alternative<int64_t>(val));
        ++count;
    }
    EXPECT_EQ(count, 3);
}

// ============================================================================
// Result tests
// ============================================================================

TEST(Result, DefaultConstructor) {
    quiver::Result result;

    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.row_count(), 0u);
    EXPECT_EQ(result.column_count(), 0u);
}

TEST(Result, ColumnsAccessor) {
    std::vector<std::string> columns = {"id", "name", "value"};
    std::vector<quiver::Row> rows;

    quiver::Result result(columns, std::move(rows));

    auto& cols = result.columns();
    EXPECT_EQ(cols.size(), 3u);
    EXPECT_EQ(cols[0], "id");
    EXPECT_EQ(cols[1], "name");
    EXPECT_EQ(cols[2], "value");
}

TEST(Result, AtOutOfBounds) {
    quiver::Result result;

    EXPECT_THROW(result.at(0), std::out_of_range);
    EXPECT_THROW(result.at(100), std::out_of_range);
}

TEST(Result, EmptyResult) {
    std::vector<std::string> columns = {"id", "name"};
    std::vector<quiver::Row> rows;

    quiver::Result result(columns, std::move(rows));

    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.row_count(), 0u);
    EXPECT_EQ(result.column_count(), 2u);  // Columns exist but no rows
}

TEST(Result, IteratorOnEmpty) {
    quiver::Result result;

    int count = 0;
    for (const auto& row : result) {
        (void)row;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(Result, IteratorOnNonEmpty) {
    std::vector<std::string> columns = {"value"};
    std::vector<quiver::Row> rows;
    rows.emplace_back(std::vector<quiver::Value>{int64_t{1}});
    rows.emplace_back(std::vector<quiver::Value>{int64_t{2}});
    rows.emplace_back(std::vector<quiver::Value>{int64_t{3}});

    quiver::Result result(columns, std::move(rows));

    int count = 0;
    for (const auto& row : result) {
        EXPECT_EQ(row.size(), 1u);
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST(Result, OperatorBracketValid) {
    std::vector<std::string> columns = {"value"};
    std::vector<quiver::Row> rows;
    rows.emplace_back(std::vector<quiver::Value>{int64_t{42}});

    quiver::Result result(columns, std::move(rows));

    const auto& row = result[0];
    EXPECT_EQ(row.get_integer(0).value(), 42);
}

TEST(Result, MixedValueTypes) {
    std::vector<std::string> columns = {"integer_col", "float_col", "string_col", "null_col"};
    std::vector<quiver::Row> rows;
    rows.emplace_back(std::vector<quiver::Value>{int64_t{42}, 3.14, std::string("hello"), nullptr});

    quiver::Result result(columns, std::move(rows));

    EXPECT_EQ(result.row_count(), 1u);
    EXPECT_EQ(result.column_count(), 4u);

    const auto& row = result[0];
    EXPECT_EQ(row.get_integer(0).value(), 42);
    EXPECT_DOUBLE_EQ(row.get_float(1).value(), 3.14);
    EXPECT_EQ(row.get_string(2).value(), "hello");
    EXPECT_TRUE(row.is_null(3));
}

// ============================================================================
// Integration tests with Database
// ============================================================================

TEST(RowResult, ReadScalarWithNullValues) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.console_level = quiver::LogLevel::off});

    // Create required Configuration
    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    // Create elements without optional scalar attributes
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("some_integer", int64_t{42});
    db.create_element("Collection", e2);

    // Read scalars - only non-null values are returned
    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers.size(), 1u);
    EXPECT_EQ(integers[0], 42);
}

TEST(RowResult, ReadScalarByIdWithNull) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    // Create element with minimal required fields
    quiver::Element e;
    e.set("label", std::string("Config"));
    int64_t id = db.create_element("Configuration", e);

    // Read optional float attribute (should be nullopt since we didn't set it)
    // Note: integer_attribute has DEFAULT 6, so we use float_attribute instead
    auto result = db.read_scalar_float_by_id("Configuration", "float_attribute", id);
    EXPECT_FALSE(result.has_value());
}

TEST(RowResult, EmptyResultFromQuery) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    // No elements created - should return empty vectors
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_TRUE(labels.empty());

    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_TRUE(integers.empty());
}
