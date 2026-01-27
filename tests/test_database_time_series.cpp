#include "test_utils.h"

#include <cmath>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time Series Add/Read Tests
// ============================================================================

TEST(TimeSeries, AddAndReadSingleRow) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    // Create a resource element
    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);
    EXPECT_EQ(id, 1);

    // Add a time series row
    db.add_time_series_row("Resource", "value1", id, 42.5, "2024-01-01T00:00:00");

    // Read back
    auto rows = db.read_time_series_table("Resource", "group1", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01T00:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value1"]), 42.5);
}

TEST(TimeSeries, AddMultipleRows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add multiple rows
    db.add_time_series_row("Resource", "value1", id, 10.0, "2024-01-01T00:00:00");
    db.add_time_series_row("Resource", "value1", id, 20.0, "2024-01-02T00:00:00");
    db.add_time_series_row("Resource", "value1", id, 30.0, "2024-01-03T00:00:00");

    auto rows = db.read_time_series_table("Resource", "group1", id);
    EXPECT_EQ(rows.size(), 3);

    // Should be ordered by date_time
    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[1]["date_time"]), "2024-01-02T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[2]["date_time"]), "2024-01-03T00:00:00");

    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value1"]), 10.0);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1]["value1"]), 20.0);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2]["value1"]), 30.0);
}

TEST(TimeSeries, AddWithNullValues) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add row with value1, leaving value2 as NULL
    db.add_time_series_row("Resource", "value1", id, 42.5, "2024-01-01T00:00:00");

    auto rows = db.read_time_series_table("Resource", "group1", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value1"]), 42.5);

    // value2 should be NaN (null)
    EXPECT_TRUE(std::isnan(std::get<double>(rows[0]["value2"])));
}

TEST(TimeSeries, MultiDimensionalWithBlock) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add rows with block dimension
    db.add_time_series_row("Resource", "value3", id, 100.0, "2024-01-01T00:00:00", {{"block", 1}});
    db.add_time_series_row("Resource", "value3", id, 200.0, "2024-01-01T00:00:00", {{"block", 2}});
    db.add_time_series_row("Resource", "value3", id, 150.0, "2024-01-02T00:00:00", {{"block", 1}});

    auto rows = db.read_time_series_table("Resource", "group2", id);
    EXPECT_EQ(rows.size(), 3);

    // Verify dimensions are correct
    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01T00:00:00");
    EXPECT_EQ(std::get<int64_t>(rows[0]["block"]), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value3"]), 100.0);

    EXPECT_EQ(std::get<std::string>(rows[1]["date_time"]), "2024-01-01T00:00:00");
    EXPECT_EQ(std::get<int64_t>(rows[1]["block"]), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1]["value3"]), 200.0);
}

TEST(TimeSeries, MultiDimensionalWithBlockAndScenario) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add rows with block and scenario dimensions
    db.add_time_series_row("Resource", "value4", id, 1.0, "2024-01-01T00:00:00", {{"block", 1}, {"scenario", 1}});
    db.add_time_series_row("Resource", "value4", id, 2.0, "2024-01-01T00:00:00", {{"block", 1}, {"scenario", 2}});
    db.add_time_series_row("Resource", "value4", id, 3.0, "2024-01-01T00:00:00", {{"block", 2}, {"scenario", 1}});

    auto rows = db.read_time_series_table("Resource", "group3", id);
    EXPECT_EQ(rows.size(), 3);
}

// ============================================================================
// Time Series Update Tests
// ============================================================================

TEST(TimeSeries, UpdateExistingRow) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add initial row
    db.add_time_series_row("Resource", "value1", id, 42.5, "2024-01-01T00:00:00");

    // Update it
    db.update_time_series_row("Resource", "value1", id, 99.9, "2024-01-01T00:00:00");

    auto rows = db.read_time_series_table("Resource", "group1", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value1"]), 99.9);
}

TEST(TimeSeries, UpdateMultiDimensionalRow) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add rows
    db.add_time_series_row("Resource", "value3", id, 100.0, "2024-01-01T00:00:00", {{"block", 1}});
    db.add_time_series_row("Resource", "value3", id, 200.0, "2024-01-01T00:00:00", {{"block", 2}});

    // Update only block 1
    db.update_time_series_row("Resource", "value3", id, 150.0, "2024-01-01T00:00:00", {{"block", 1}});

    auto rows = db.read_time_series_table("Resource", "group2", id);
    EXPECT_EQ(rows.size(), 2);

    // Block 1 should be updated
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value3"]), 150.0);
    // Block 2 should be unchanged
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1]["value3"]), 200.0);
}

// ============================================================================
// Time Series Delete Tests
// ============================================================================

TEST(TimeSeries, DeleteAllForElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add multiple rows
    db.add_time_series_row("Resource", "value1", id, 10.0, "2024-01-01T00:00:00");
    db.add_time_series_row("Resource", "value1", id, 20.0, "2024-01-02T00:00:00");

    // Delete all
    db.delete_time_series("Resource", "group1", id);

    auto rows = db.read_time_series_table("Resource", "group1", id);
    EXPECT_TRUE(rows.empty());
}

TEST(TimeSeries, DeleteDoesNotAffectOtherElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource1;
    resource1.set("label", std::string("Resource 1"));
    int64_t id1 = db.create_element("Resource", resource1);

    quiver::Element resource2;
    resource2.set("label", std::string("Resource 2"));
    int64_t id2 = db.create_element("Resource", resource2);

    // Add rows to both
    db.add_time_series_row("Resource", "value1", id1, 10.0, "2024-01-01T00:00:00");
    db.add_time_series_row("Resource", "value1", id2, 20.0, "2024-01-01T00:00:00");

    // Delete only resource 1
    db.delete_time_series("Resource", "group1", id1);

    auto rows1 = db.read_time_series_table("Resource", "group1", id1);
    auto rows2 = db.read_time_series_table("Resource", "group1", id2);

    EXPECT_TRUE(rows1.empty());
    EXPECT_EQ(rows2.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows2[0]["value1"]), 20.0);
}

// ============================================================================
// Time Series Metadata Tests
// ============================================================================

TEST(TimeSeries, GetMetadata) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    auto meta = db.get_time_series_metadata("Resource", "group1");
    EXPECT_EQ(meta.group_name, "group1");
    EXPECT_EQ(meta.dimension_columns.size(), 1);
    EXPECT_EQ(meta.dimension_columns[0], "date_time");

    EXPECT_EQ(meta.value_columns.size(), 2);  // value1 and value2
}

TEST(TimeSeries, GetMetadataMultiDimensional) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    auto meta = db.get_time_series_metadata("Resource", "group2");
    EXPECT_EQ(meta.group_name, "group2");
    EXPECT_EQ(meta.dimension_columns.size(), 2);  // date_time and block
}

TEST(TimeSeries, ListTimeSeriesGroups) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    auto groups = db.list_time_series_groups("Resource");
    EXPECT_EQ(groups.size(), 3);  // group1, group2, group3
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(TimeSeries, InvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    EXPECT_THROW(db.add_time_series_row("NonExistent", "value1", 1, 42.5, "2024-01-01T00:00:00"), std::runtime_error);
}

TEST(TimeSeries, InvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    EXPECT_THROW(db.add_time_series_row("Resource", "nonexistent", id, 42.5, "2024-01-01T00:00:00"), std::runtime_error);
}

TEST(TimeSeries, MissingDimension) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // group2 requires block dimension
    EXPECT_THROW(db.add_time_series_row("Resource", "value3", id, 42.5, "2024-01-01T00:00:00"), std::runtime_error);
}

TEST(TimeSeries, InvalidGroup) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    EXPECT_THROW(db.read_time_series_table("Resource", "nonexistent", id), std::runtime_error);
    EXPECT_THROW(db.delete_time_series("Resource", "nonexistent", id), std::runtime_error);
}

// ============================================================================
// Upsert Behavior Tests
// ============================================================================

TEST(TimeSeries, UpsertBehavior) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("time_series.sql"), {.console_level = quiver::LogLevel::off});

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    int64_t id = db.create_element("Resource", resource);

    // Add initial row
    db.add_time_series_row("Resource", "value1", id, 10.0, "2024-01-01T00:00:00");

    // Add again with same key - should replace
    db.add_time_series_row("Resource", "value1", id, 99.0, "2024-01-01T00:00:00");

    auto rows = db.read_time_series_table("Resource", "group1", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value1"]), 99.0);
}
