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

TEST(DatabaseDescribe, VectorsHeaderAppearsOnce) {
    auto output = open(VALID_SCHEMA("describe_multi_group.sql")).describe();

    // "Vectors:" header should appear exactly once
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find("Vectors:", pos)) != std::string::npos) {
        ++count;
        pos += 8;
    }
    EXPECT_EQ(count, 1) << "Vectors: header should appear exactly once. Output:\n" << output;

    // Both vector groups should be listed
    EXPECT_NE(output.find("values"), std::string::npos) << "Missing vector group 'values'";
    EXPECT_NE(output.find("scores"), std::string::npos) << "Missing vector group 'scores'";
}

TEST(DatabaseDescribe, SetsHeaderAppearsOnce) {
    auto output = open(VALID_SCHEMA("describe_multi_group.sql")).describe();

    // "Sets:" header should appear exactly once
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find("Sets:", pos)) != std::string::npos) {
        ++count;
        pos += 5;
    }
    EXPECT_EQ(count, 1) << "Sets: header should appear exactly once. Output:\n" << output;

    // Both set groups should be listed
    EXPECT_NE(output.find("tags"), std::string::npos) << "Missing set group 'tags'";
    EXPECT_NE(output.find("categories"), std::string::npos) << "Missing set group 'categories'";
}

TEST(DatabaseDescribe, TimeSeriesHeaderAppearsOnceWithBracketedDimension) {
    auto output = open(VALID_SCHEMA("describe_multi_group.sql")).describe();

    // "Time Series:" header should appear exactly once
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find("Time Series:", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 1) << "Time Series: header should appear exactly once. Output:\n" << output;

    // Dimension column should be in brackets
    EXPECT_NE(output.find("[date_time]"), std::string::npos)
        << "Expected dimension column [date_time] in brackets. Output:\n"
        << output;
    EXPECT_NE(output.find("[date_recorded]"), std::string::npos)
        << "Expected dimension column [date_recorded] in brackets. Output:\n"
        << output;
}

TEST(DatabaseDescribe, ScalarOrderMatchesSchema) {
    auto output = open(VALID_SCHEMA("describe_multi_group.sql")).describe();

    // In the Items collection scalars, the schema defines: id, label, priority, weight
    // The 'id' should appear before 'label', 'label' before 'priority', 'priority' before 'weight'
    auto id_pos = output.find("    - id ");
    auto label_pos = output.find("    - label ");
    auto priority_pos = output.find("    - priority ");
    auto weight_pos = output.find("    - weight ");

    ASSERT_NE(id_pos, std::string::npos) << "Missing 'id' scalar";
    ASSERT_NE(label_pos, std::string::npos) << "Missing 'label' scalar";
    ASSERT_NE(priority_pos, std::string::npos) << "Missing 'priority' scalar";
    ASSERT_NE(weight_pos, std::string::npos) << "Missing 'weight' scalar";

    EXPECT_LT(id_pos, label_pos) << "id should appear before label";
    EXPECT_LT(label_pos, priority_pos) << "label should appear before priority";
    EXPECT_LT(priority_pos, weight_pos) << "priority should appear before weight";
}

TEST(DatabaseDescribe, NoCategoryHeaderWhenEmpty) {
    // basic.sql has no vectors, sets, or time series
    auto output = open(VALID_SCHEMA("basic.sql")).describe();

    EXPECT_EQ(output.find("Vectors:"), std::string::npos)
        << "Vectors: header should not appear when no vectors exist. Output:\n"
        << output;
    EXPECT_EQ(output.find("Sets:"), std::string::npos) << "Sets: header should not appear when no sets exist. Output:\n"
                                                       << output;
    EXPECT_EQ(output.find("Time Series:"), std::string::npos)
        << "Time Series: header should not appear when no time series exist. Output:\n"
        << output;
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

// A non-STRICT INTEGER column can hold TEXT and REAL cells; the value distribution counts only
// integers (they used to read back as codes 0 and 1).
TEST(DatabaseDescribe, SummarizeDistributionSkipsNonIntegerCells) {
    auto db = open(VALID_SCHEMA("non_strict_vector.sql"));
    db.query_string("INSERT INTO Items (label, code) VALUES ('a', 'not-a-number'), ('b', 1.5), ('c', 1)");
    EXPECT_TRUE(contains(db.summarize_collection("Items"), "code: 3 non-null, 0 null; values {1: 1}\n"));
}

TEST(DatabaseDescribe, SummarizeNotFound) {
    auto db = open(VALID_SCHEMA("describe_multi_group.sql"));
    EXPECT_THROW(db.summarize_collection("Nope"), std::runtime_error);
}

// Bracketed columns are exactly the time series' primary-key dimensions: a date_ value column is
// not bracketed, and a non-date key column (block) is.
TEST(DatabaseDescribe, TimeSeriesBracketsPrimaryKeyDimensionsOnly) {
    auto plant = open(VALID_SCHEMA("time_series_date_columns.sql")).describe_collection("Plant");
    EXPECT_TRUE(contains(plant, "- events: [date_time], date_approved(DATE_TIME), value(REAL)\n")) << plant;

    auto resource = open(VALID_SCHEMA("multi_dim_time_series.sql")).describe_collection("Resource");
    EXPECT_TRUE(contains(resource, "- load: [date_time], [block], load(REAL), flag(INTEGER)\n")) << resource;
}
