#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time series files tests
// ============================================================================

TEST(Database, HasTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_TRUE(db.has_time_series_files("Collection"));
    EXPECT_FALSE(db.has_time_series_files("Configuration"));
}

TEST(Database, ListTimeSeriesFilesColumns) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto columns = db.list_time_series_files_columns("Collection");
    EXPECT_EQ(columns.size(), 2);
    EXPECT_TRUE(std::find(columns.begin(), columns.end(), "data_file") != columns.end());
    EXPECT_TRUE(std::find(columns.begin(), columns.end(), "metadata_file") != columns.end());
}

TEST(Database, ReadTimeSeriesFilesEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto paths = db.read_time_series_files("Collection");
    EXPECT_EQ(paths.size(), 2);
    EXPECT_FALSE(paths["data_file"].has_value());
    EXPECT_FALSE(paths["metadata_file"].has_value());
}

TEST(Database, UpdateAndReadTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_time_series_files("Configuration"), std::runtime_error);
    EXPECT_THROW(db.list_time_series_files_columns("Configuration"), std::runtime_error);
}

// An empty map is a no-op only after the collection and its files table have been validated. The
// FFI bindings forward an empty map here instead of returning early, so this order is what makes
// the call fail the same way in every layer.
TEST(Database, UpdateTimeSeriesFilesEmptyMapValidatesCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    std::map<std::string, std::optional<std::string>> empty;

    try {
        db.update_time_series_files("NoSuchCollection", empty);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot update_time_series_files: collection not found: NoSuchCollection");
    }

    // Configuration exists but has no _time_series_files table
    EXPECT_THROW(db.update_time_series_files("Configuration", empty), std::runtime_error);

    // On a collection that has the table, an empty map changes nothing
    std::map<std::string, std::optional<std::string>> paths;
    paths["data_file"] = "/path/to/data.csv";
    db.update_time_series_files("Collection", paths);
    db.update_time_series_files("Collection", empty);

    auto result = db.read_time_series_files("Collection");
    EXPECT_EQ(result["data_file"].value(), "/path/to/data.csv");
    EXPECT_FALSE(result["metadata_file"].has_value());
}
