#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <string>

namespace {

quiver::Database open(const std::string& schema) {
    return quiver::Database::from_schema(
        ":memory:", schema, {.read_only = false, .console_level = quiver::LogLevel::Off});
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST(DatabaseDescribe, WholeDatabaseReport) {
    auto db = open(VALID_SCHEMA("describe_multi_group.sql"));
    db.create_element("Items", quiver::Element().set("label", std::string("a")));
    db.create_element("Items", quiver::Element().set("label", std::string("b")));

    auto report = db.describe();
    EXPECT_TRUE(contains(report, "Database: :memory:"));
    EXPECT_TRUE(contains(report, "Version: 0"));
    // Every collection appears, with element counts.
    EXPECT_TRUE(contains(report, "Collection: Configuration"));
    EXPECT_TRUE(contains(report, "Collection: Items (2 elements)"));
    // Structure of Items.
    EXPECT_TRUE(contains(report, "- priority (INTEGER)"));
    EXPECT_TRUE(contains(report, "Vectors:"));
    EXPECT_TRUE(contains(report, "Time Series:"));
}

TEST(DatabaseDescribe, ThrowsWithoutSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});
    EXPECT_THROW(db.describe(), std::runtime_error);
}

TEST(DatabaseDescribe, DescribeCollection) {
    auto db = open(VALID_SCHEMA("describe_multi_group.sql"));
    auto report = db.describe_collection("Items");

    EXPECT_TRUE(contains(report, "Collection: Items (0 elements)"));
    EXPECT_TRUE(contains(report, "- label (TEXT)"));
    EXPECT_TRUE(contains(report, "values"));
    EXPECT_TRUE(contains(report, "[date_time]"));  // time-series dimension column, bracketed
    // describe_collection reports only that collection.
    EXPECT_FALSE(contains(report, "Collection: Configuration"));
}

TEST(DatabaseDescribe, DescribeCollectionNotFound) {
    auto db = open(VALID_SCHEMA("describe_multi_group.sql"));
    EXPECT_THROW(db.describe_collection("Nope"), std::runtime_error);
}

TEST(DatabaseDescribe, SummarizeScalarsAndGroups) {
    auto db = open(VALID_SCHEMA("all_types.sql"));
    db.create_element("AllTypes",
                      quiver::Element()
                          .set("label", std::string("a"))
                          .set("some_integer", static_cast<int64_t>(1))
                          .set("some_float", 1.5)
                          .set("some_text", std::string("x"))
                          .set("code", std::vector<int64_t>{10, 20}));
    db.create_element("AllTypes",
                      quiver::Element()
                          .set("label", std::string("b"))
                          .set("some_integer", static_cast<int64_t>(1))
                          .set("some_float", 2.5));
    db.create_element("AllTypes",
                      quiver::Element().set("label", std::string("c")).set("some_integer", static_cast<int64_t>(5)));

    auto report = db.summarize_collection("AllTypes");
    EXPECT_TRUE(contains(report, "Collection: AllTypes (3 elements)"));
    // Integer scalar gets a value distribution; nulls are counted.
    EXPECT_TRUE(contains(report, "some_integer: 3 non-null, 0 null; values {1: 2, 5: 1}"));
    EXPECT_TRUE(contains(report, "some_float: 2 non-null, 1 null"));
    EXPECT_TRUE(contains(report, "some_text: 1 non-null, 2 null"));
    // Float scalars never get a distribution.
    EXPECT_FALSE(contains(report, "some_float: 2 non-null, 1 null; values"));
    // Group fill counts: only the first element has a 'codes' set.
    EXPECT_TRUE(contains(report, "codes: 1/3 non-empty"));
}

TEST(DatabaseDescribe, SummarizeDistributionCardinalityBoundary) {
    {
        auto db = open(VALID_SCHEMA("all_types.sql"));
        for (int64_t i = 1; i <= 64; ++i) {
            db.create_element("AllTypes",
                              quiver::Element().set("label", "L" + std::to_string(i)).set("some_integer", i));
        }
        EXPECT_TRUE(contains(db.summarize_collection("AllTypes"), "values {"));  // 64 distinct -> shown
    }
    {
        auto db = open(VALID_SCHEMA("all_types.sql"));
        for (int64_t i = 1; i <= 65; ++i) {
            db.create_element("AllTypes",
                              quiver::Element().set("label", "L" + std::to_string(i)).set("some_integer", i));
        }
        EXPECT_FALSE(contains(db.summarize_collection("AllTypes"), "values {"));  // 65 distinct -> omitted
    }
}

TEST(DatabaseDescribe, SummarizeNotFound) {
    auto db = open(VALID_SCHEMA("describe_multi_group.sql"));
    EXPECT_THROW(db.summarize_collection("Nope"), std::runtime_error);
}
