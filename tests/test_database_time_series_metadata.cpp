#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time series metadata tests
// ============================================================================

TEST(Database, GetTimeSeriesMetadata) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off}
    );

    auto metadata = db.get_time_series_metadata("Collection", "data");
    EXPECT_EQ(metadata.group_name, "data");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::Real);
}

TEST(Database, GetTimeSeriesMetadataForeignKey) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off}
    );

    auto metadata = db.get_time_series_metadata("Child", "events");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    ASSERT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "sponsor_id");
    EXPECT_TRUE(metadata.value_columns[0].is_foreign_key);
    EXPECT_EQ(metadata.value_columns[0].references_collection, "Parent");
    EXPECT_EQ(metadata.value_columns[0].references_column, "id");
}

TEST(Database, ListTimeSeriesGroups) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off}
    );

    auto groups = db.list_time_series_groups("Collection");
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].group_name, "data");
    EXPECT_EQ(groups[0].dimension_column, "date_time");
    EXPECT_EQ(groups[0].value_columns.size(), 1);
    EXPECT_EQ(groups[0].value_columns[0].name, "value");
}

TEST(Database, ListTimeSeriesGroupsEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off}
    );

    // Configuration has no time series tables
    auto groups = db.list_time_series_groups("Configuration");
    EXPECT_TRUE(groups.empty());
}

// time_series_date_columns.sql: Plant_time_series_events has a nullable date_approved value column
// that sorts before its primary-key date column date_time. The dimension comes from the key.
TEST(Database, GetTimeSeriesMetadataDateValueColumnIsNotTheDimension) {
    auto db = quiver::Database::from_schema(
        ":memory:",
        VALID_SCHEMA("time_series_date_columns.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );

    auto metadata = db.get_time_series_metadata("Plant", "events");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    ASSERT_EQ(metadata.value_columns.size(), 2);
    EXPECT_EQ(metadata.value_columns[0].name, "date_approved");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::DateTime);
    EXPECT_FALSE(metadata.value_columns[0].primary_key);
    EXPECT_EQ(metadata.value_columns[1].name, "value");

    auto groups = db.list_time_series_groups("Plant");
    ASSERT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].dimension_column, "date_time");
}

// Meter_time_series_blocks keys on (id, block); its date_time is a value column, so the group has
// no date dimension. Metadata and the readers refuse it instead of ordering by a column the writers
// never key on.
TEST(Database, GetTimeSeriesMetadataDateColumnOutsidePrimaryKeyThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:",
        VALID_SCHEMA("time_series_date_columns.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
    auto id = db.create_element("Meter", quiver::Element().set("label", std::string("Meter 1")));

    try {
        db.get_time_series_metadata("Meter", "blocks");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Dimension column not found: time series table 'Meter_time_series_blocks'");
    }
    EXPECT_THROW(db.list_time_series_groups("Meter"), std::runtime_error);
    EXPECT_THROW(db.read_time_series_group("Meter", "blocks", id), std::runtime_error);
    EXPECT_THROW(db.read_time_series_row("Meter", "blocks", "value", "2024-01-01T00:00:00"), std::runtime_error);
}
