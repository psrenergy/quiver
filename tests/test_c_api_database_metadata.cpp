#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <string>

// ============================================================================
// Get metadata tests
// ============================================================================

TEST(DatabaseCApiMetadata, GetVectorMetadata) {
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
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
    auto options = quiver::test::quiet_options();
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

// ============================================================================
// describe / describe_collection / summarize_collection (text reports)
// ============================================================================

namespace {
quiver_database_t* open_collections() {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    EXPECT_NE(db, nullptr);
    return db;
}
}  // namespace

TEST(DatabaseCApiMetadata, DescribeReturnsText) {
    quiver_database_t* db = open_collections();

    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe(db, &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    std::string text = report;
    EXPECT_NE(text.find("Collection: Configuration"), std::string::npos);
    EXPECT_NE(text.find("Collection: Collection"), std::string::npos);
    quiver_database_free_string(report);

    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, DescribeCollectionReturnsText) {
    quiver_database_t* db = open_collections();

    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe_collection(db, "Collection", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    std::string text = report;
    EXPECT_NE(text.find("Collection: Collection"), std::string::npos);
    EXPECT_NE(text.find("[date_time]"), std::string::npos);
    quiver_database_free_string(report);

    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, SummarizeCollectionReturnsText) {
    quiver_database_t* db = open_collections();

    char* report = nullptr;
    ASSERT_EQ(quiver_database_summarize_collection(db, "Collection", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_NE(std::string(report).find("non-null"), std::string::npos);
    quiver_database_free_string(report);

    quiver_database_close(db);
}

TEST(DatabaseCApiMetadata, SummarizeCollectionNotFound) {
    quiver_database_t* db = open_collections();

    char* report = nullptr;
    EXPECT_EQ(quiver_database_summarize_collection(db, "Nope", &report), QUIVER_ERROR);

    quiver_database_close(db);
}
