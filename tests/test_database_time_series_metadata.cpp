#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time series metadata tests
// ============================================================================

TEST(Database, GetTimeSeriesMetadata) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_time_series_metadata("Collection", "data");
    EXPECT_EQ(metadata.group_name, "data");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::Real);
}

TEST(Database, ListTimeSeriesGroups) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto groups = db.list_time_series_groups("Collection");
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].group_name, "data");
    EXPECT_EQ(groups[0].dimension_column, "date_time");
    EXPECT_EQ(groups[0].value_columns.size(), 1);
    EXPECT_EQ(groups[0].value_columns[0].name, "value");
}

TEST(Database, ListTimeSeriesGroupsEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Configuration has no time series tables
    auto groups = db.list_time_series_groups("Configuration");
    EXPECT_TRUE(groups.empty());
}

