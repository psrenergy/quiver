#include "test_utils.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Time series metadata tests
// ============================================================================

TEST(DatabaseCApi, GetTimeSeriesMetadata) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t metadata;
    auto err = quiver_database_get_time_series_metadata(db, "Collection", "data", &metadata);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "data");
    EXPECT_STREQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_column_count, 1);
    EXPECT_STREQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, QUIVER_DATA_TYPE_FLOAT);

    quiver_database_free_group_metadata(&metadata);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroups) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* metadata = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Collection", &metadata, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(metadata[0].group_name, "data");
    EXPECT_STREQ(metadata[0].dimension_column, "date_time");
    EXPECT_EQ(metadata[0].value_column_count, 1);

    quiver_database_free_group_metadata_array(metadata, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesGroupsEmpty) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* metadata = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Configuration", &metadata, &count);

    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(metadata, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, GetTimeSeriesMetadataDateValueColumnIsNotTheDimension) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series_date_columns.sql").c_str(), &options, &db),
        QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t metadata;
    ASSERT_EQ(quiver_database_get_time_series_metadata(db, "Plant", "events", &metadata), QUIVER_OK);
    EXPECT_STREQ(metadata.dimension_column, "date_time");
    ASSERT_EQ(metadata.value_column_count, 2);
    EXPECT_STREQ(metadata.value_columns[0].name, "date_approved");
    EXPECT_EQ(metadata.value_columns[0].data_type, QUIVER_DATA_TYPE_DATE_TIME);
    EXPECT_EQ(metadata.value_columns[0].primary_key, 0);
    EXPECT_STREQ(metadata.value_columns[1].name, "value");
    quiver_database_free_group_metadata(&metadata);

    // A group with no date in its key reports the core's message through the one error channel.
    quiver_group_metadata_t meter{};
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, "Meter", "blocks", &meter), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Dimension column not found: time series table 'Meter_time_series_blocks'");

    quiver_database_close(db);
}
