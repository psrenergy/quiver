#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <string>

// ============================================================================
// Get metadata tests
// ============================================================================

TEST(DatabaseCApiMetadata, GetVectorMetadata) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t metadata = {};
    auto err = quiver_database_get_vector_metadata(db, "Collection", "values", &metadata);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "values");
    EXPECT_EQ(metadata.dimension_column, nullptr);
    EXPECT_GT(metadata.value_column_count, 0);

    quiver_database_free_group_metadata(&metadata);
    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, GetSetMetadata) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t metadata = {};
    auto err = quiver_database_get_set_metadata(db, "Collection", "tags", &metadata);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "tags");
    EXPECT_EQ(metadata.dimension_column, nullptr);
    EXPECT_GT(metadata.value_column_count, 0);

    quiver_database_free_group_metadata(&metadata);
    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, GetTimeSeriesMetadata) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t metadata = {};
    auto err = quiver_database_get_time_series_metadata(db, "Collection", "data", &metadata);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "data");
    EXPECT_NE(metadata.dimension_column, nullptr);
    EXPECT_GT(metadata.value_column_count, 0);

    quiver_database_free_group_metadata(&metadata);
    quiver_database_close(db);
}

// ============================================================================
// List groups/attributes tests
// ============================================================================

TEST(DatabaseCApiMetadata, ListVectorGroups) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* groups = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_vector_groups(db, "Collection", &groups, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_GE(count, 1);

    quiver_database_free_group_metadata_array(groups, count);
    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, ListSetGroups) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* groups = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_set_groups(db, "Collection", &groups, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_GE(count, 1);

    quiver_database_free_group_metadata_array(groups, count);
    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, ListTimeSeriesGroups) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t* groups = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Collection", &groups, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_GE(count, 1);

    quiver_database_free_group_metadata_array(groups, count);
    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, ListScalarAttributes) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_scalar_metadata_t* attrs = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_scalar_attributes(db, "Collection", &attrs, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_GE(count, 1);

    quiver_database_free_scalar_metadata_array(attrs, count);
    quiver_database_close(db);
}
