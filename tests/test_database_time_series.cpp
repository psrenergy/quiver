#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time series metadata tests
// ============================================================================

TEST(Database, GetTimeSeriesMetadata) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    auto metadata = db.get_time_series_metadata("Collection", "data");
    EXPECT_EQ(metadata.group_name, "data");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::Real);
}

TEST(Database, ListTimeSeriesGroups) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    auto groups = db.list_time_series_groups("Collection");
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].group_name, "data");
    EXPECT_EQ(groups[0].dimension_column, "date_time");
    EXPECT_EQ(groups[0].value_columns.size(), 1);
    EXPECT_EQ(groups[0].value_columns[0].name, "value");
}

TEST(Database, ListTimeSeriesGroupsEmpty) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // Configuration has no time series tables
    auto groups = db.list_time_series_groups("Configuration");
    EXPECT_TRUE(groups.empty());
}

// ============================================================================
// Time series read tests
// ============================================================================

TEST(Database, ReadTimeSeriesGroupById) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert time series data
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.5}},
        {{"date_time", std::string("2024-01-01T11:00:00")}, {"value", 2.5}},
        {{"date_time", std::string("2024-01-01T12:00:00")}, {"value", 3.5}}};
    db.update_time_series_group("Collection", "data", id, rows);

    // Read back
    auto result = db.read_time_series_group_by_id("Collection", "data", id);
    ASSERT_EQ(result.size(), 3);

    // Check ordering by date_time
    EXPECT_EQ(std::get<std::string>(result[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result[0]["value"]), 1.5);

    EXPECT_EQ(std::get<std::string>(result[1]["date_time"]), "2024-01-01T11:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result[1]["value"]), 2.5);

    EXPECT_EQ(std::get<std::string>(result[2]["date_time"]), "2024-01-01T12:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result[2]["value"]), 3.5);
}

TEST(Database, ReadTimeSeriesGroupByIdEmpty) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // No time series data inserted
    auto result = db.read_time_series_group_by_id("Collection", "data", id);
    EXPECT_TRUE(result.empty());
}

TEST(Database, ReadTimeSeriesGroupByIdNonexistent) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Read from nonexistent element
    auto result = db.read_time_series_group_by_id("Collection", "data", 999);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// Time series update tests
// ============================================================================

TEST(Database, UpdateTimeSeriesGroup) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert initial data
    std::vector<std::map<std::string, quiver::Value>> rows1 = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}}};
    db.update_time_series_group("Collection", "data", id, rows1);

    auto result1 = db.read_time_series_group_by_id("Collection", "data", id);
    ASSERT_EQ(result1.size(), 1);

    // Replace with new data
    std::vector<std::map<std::string, quiver::Value>> rows2 = {
        {{"date_time", std::string("2024-02-01T10:00:00")}, {"value", 10.0}},
        {{"date_time", std::string("2024-02-01T11:00:00")}, {"value", 20.0}}};
    db.update_time_series_group("Collection", "data", id, rows2);

    auto result2 = db.read_time_series_group_by_id("Collection", "data", id);
    ASSERT_EQ(result2.size(), 2);
    EXPECT_EQ(std::get<std::string>(result2[0]["date_time"]), "2024-02-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result2[0]["value"]), 10.0);
}

TEST(Database, UpdateTimeSeriesGroupEmpty) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert some data first
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}}};
    db.update_time_series_group("Collection", "data", id, rows);

    // Clear by updating with empty
    std::vector<std::map<std::string, quiver::Value>> empty_rows;
    db.update_time_series_group("Collection", "data", id, empty_rows);

    auto result = db.read_time_series_group_by_id("Collection", "data", id);
    EXPECT_TRUE(result.empty());
}

TEST(Database, TimeSeriesOrdering) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert data out of order
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-03T10:00:00")}, {"value", 3.0}},
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}},
        {{"date_time", std::string("2024-01-02T10:00:00")}, {"value", 2.0}}};
    db.update_time_series_group("Collection", "data", id, rows);

    // Should be returned ordered by date_time
    auto result = db.read_time_series_group_by_id("Collection", "data", id);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(std::get<std::string>(result[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(result[1]["date_time"]), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(result[2]["date_time"]), "2024-01-03T10:00:00");
}

// ============================================================================
// Time series error handling tests
// ============================================================================

TEST(Database, TimeSeriesGroupNotFound) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    EXPECT_THROW(db.get_time_series_metadata("Collection", "nonexistent"), std::runtime_error);

    EXPECT_THROW(db.read_time_series_group_by_id("Collection", "nonexistent", 1), std::runtime_error);
}

TEST(Database, TimeSeriesCollectionNotFound) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // Nonexistent collection returns empty list (matches list_vector_groups behavior)
    auto groups = db.list_time_series_groups("NonexistentCollection");
    EXPECT_TRUE(groups.empty());
}

TEST(Database, TimeSeriesMissingDateTime) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Row missing date_time
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"value", 1.0}}  // Missing date_time
    };

    EXPECT_THROW(db.update_time_series_group("Collection", "data", id, rows), std::runtime_error);
}

// ============================================================================
// Time series files tests
// ============================================================================

TEST(Database, HasTimeSeriesFiles) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    EXPECT_TRUE(db.has_time_series_files("Collection"));
    EXPECT_FALSE(db.has_time_series_files("Configuration"));
}

TEST(Database, ListTimeSeriesFilesColumns) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    auto columns = db.list_time_series_files_columns("Collection");
    EXPECT_EQ(columns.size(), 2);
    EXPECT_TRUE(std::find(columns.begin(), columns.end(), "data_file") != columns.end());
    EXPECT_TRUE(std::find(columns.begin(), columns.end(), "metadata_file") != columns.end());
}

TEST(Database, ReadTimeSeriesFilesEmpty) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    auto paths = db.read_time_series_files("Collection");
    EXPECT_EQ(paths.size(), 2);
    EXPECT_FALSE(paths["data_file"].has_value());
    EXPECT_FALSE(paths["metadata_file"].has_value());
}

TEST(Database, UpdateAndReadTimeSeriesFiles) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    std::map<std::string, std::optional<std::string>> paths;
    paths["data_file"] = "/path/to/data.csv";
    paths["metadata_file"] = "/path/to/meta.json";

    db.update_time_series_files("Collection", paths);

    auto result = db.read_time_series_files("Collection");
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(result["data_file"].has_value());
    EXPECT_EQ(result["data_file"].value(), "/path/to/data.csv");
    EXPECT_TRUE(result["metadata_file"].has_value());
    EXPECT_EQ(result["metadata_file"].value(), "/path/to/meta.json");
}

TEST(Database, UpdateTimeSeriesFilesWithNulls) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    std::map<std::string, std::optional<std::string>> paths;
    paths["data_file"] = "/path/to/data.csv";
    paths["metadata_file"] = std::nullopt;

    db.update_time_series_files("Collection", paths);

    auto result = db.read_time_series_files("Collection");
    EXPECT_TRUE(result["data_file"].has_value());
    EXPECT_EQ(result["data_file"].value(), "/path/to/data.csv");
    EXPECT_FALSE(result["metadata_file"].has_value());
}

TEST(Database, UpdateTimeSeriesFilesReplace) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // First update
    std::map<std::string, std::optional<std::string>> paths1;
    paths1["data_file"] = "/old/data.csv";
    paths1["metadata_file"] = "/old/meta.json";
    db.update_time_series_files("Collection", paths1);

    // Second update replaces
    std::map<std::string, std::optional<std::string>> paths2;
    paths2["data_file"] = "/new/data.csv";
    paths2["metadata_file"] = "/new/meta.json";
    db.update_time_series_files("Collection", paths2);

    auto result = db.read_time_series_files("Collection");
    EXPECT_EQ(result["data_file"].value(), "/new/data.csv");
    EXPECT_EQ(result["metadata_file"].value(), "/new/meta.json");
}

TEST(Database, TimeSeriesFilesNotFound) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    EXPECT_THROW(db.read_time_series_files("Configuration"), std::runtime_error);
    EXPECT_THROW(db.list_time_series_files_columns("Configuration"), std::runtime_error);
}
