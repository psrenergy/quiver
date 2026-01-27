#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

#include <cstring>
#include <string>

// Helper to create a resource element
static int64_t create_resource(quiver_database_t* db, const char* label) {
    auto element = quiver_element_create();
    quiver_element_set_string(element, "label", label);
    auto id = quiver_database_create_element(db, "Resource", element);
    quiver_element_destroy(element);
    return id;
}

// ============================================================================
// Time Series Add/Read Tests
// ============================================================================

TEST(TimeSeriesCApi, AddAndReadSingleRow) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");
    EXPECT_EQ(id, 1);

    // Add a time series row
    auto err = quiver_database_add_time_series_row(db, "Resource", "value1", id, 42.5, "2024-01-01T00:00:00", nullptr,
                                                   nullptr, 0);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char* json = nullptr;
    err = quiver_database_read_time_series_table(db, "Resource", "group1", id, &json);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_NE(json, nullptr);

    // Simple check that JSON contains expected values
    std::string json_str(json);
    EXPECT_NE(json_str.find("2024-01-01T00:00:00"), std::string::npos);
    EXPECT_NE(json_str.find("42.5"), std::string::npos);

    quiver_free_string(json);
    quiver_database_close(db);
}

TEST(TimeSeriesCApi, AddMultipleRows) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");

    // Add multiple rows
    quiver_database_add_time_series_row(db, "Resource", "value1", id, 10.0, "2024-01-01T00:00:00", nullptr, nullptr, 0);
    quiver_database_add_time_series_row(db, "Resource", "value1", id, 20.0, "2024-01-02T00:00:00", nullptr, nullptr, 0);
    quiver_database_add_time_series_row(db, "Resource", "value1", id, 30.0, "2024-01-03T00:00:00", nullptr, nullptr, 0);

    char* json = nullptr;
    auto err = quiver_database_read_time_series_table(db, "Resource", "group1", id, &json);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_NE(json, nullptr);

    // Check all dates are present
    std::string json_str(json);
    EXPECT_NE(json_str.find("2024-01-01"), std::string::npos);
    EXPECT_NE(json_str.find("2024-01-02"), std::string::npos);
    EXPECT_NE(json_str.find("2024-01-03"), std::string::npos);

    quiver_free_string(json);
    quiver_database_close(db);
}

TEST(TimeSeriesCApi, MultiDimensionalWithBlock) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");

    // Add rows with block dimension
    const char* dim_names[] = {"block"};
    int64_t dim_values1[] = {1};
    int64_t dim_values2[] = {2};

    auto err =
        quiver_database_add_time_series_row(db, "Resource", "value3", id, 100.0, "2024-01-01T00:00:00", dim_names,
                                            dim_values1, 1);
    EXPECT_EQ(err, QUIVER_OK);

    err = quiver_database_add_time_series_row(db, "Resource", "value3", id, 200.0, "2024-01-01T00:00:00", dim_names,
                                              dim_values2, 1);
    EXPECT_EQ(err, QUIVER_OK);

    char* json = nullptr;
    err = quiver_database_read_time_series_table(db, "Resource", "group2", id, &json);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_NE(json, nullptr);

    std::string json_str(json);
    EXPECT_NE(json_str.find("100"), std::string::npos);
    EXPECT_NE(json_str.find("200"), std::string::npos);

    quiver_free_string(json);
    quiver_database_close(db);
}

// ============================================================================
// Time Series Update Tests
// ============================================================================

TEST(TimeSeriesCApi, UpdateExistingRow) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");

    // Add initial row
    quiver_database_add_time_series_row(db, "Resource", "value1", id, 42.5, "2024-01-01T00:00:00", nullptr, nullptr, 0);

    // Update it
    auto err = quiver_database_update_time_series_row(db, "Resource", "value1", id, 99.9, "2024-01-01T00:00:00",
                                                      nullptr, nullptr, 0);
    EXPECT_EQ(err, QUIVER_OK);

    char* json = nullptr;
    err = quiver_database_read_time_series_table(db, "Resource", "group1", id, &json);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_NE(json, nullptr);

    std::string json_str(json);
    EXPECT_NE(json_str.find("99.9"), std::string::npos);

    quiver_free_string(json);
    quiver_database_close(db);
}

// ============================================================================
// Time Series Delete Tests
// ============================================================================

TEST(TimeSeriesCApi, DeleteAllForElement) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");

    // Add rows
    quiver_database_add_time_series_row(db, "Resource", "value1", id, 10.0, "2024-01-01T00:00:00", nullptr, nullptr, 0);
    quiver_database_add_time_series_row(db, "Resource", "value1", id, 20.0, "2024-01-02T00:00:00", nullptr, nullptr, 0);

    // Delete all
    auto err = quiver_database_delete_time_series(db, "Resource", "group1", id);
    EXPECT_EQ(err, QUIVER_OK);

    char* json = nullptr;
    err = quiver_database_read_time_series_table(db, "Resource", "group1", id, &json);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(json, "[]");

    quiver_free_string(json);
    quiver_database_close(db);
}

// ============================================================================
// Time Series Metadata Tests
// ============================================================================

TEST(TimeSeriesCApi, GetMetadata) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t meta;
    auto err = quiver_database_get_time_series_metadata(db, "Resource", "group1", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(meta.group_name, "group1");
    EXPECT_EQ(meta.dimension_count, 1);  // date_time
    EXPECT_EQ(meta.value_column_count, 2);  // value1, value2

    quiver_free_time_series_metadata(&meta);
    quiver_database_close(db);
}

TEST(TimeSeriesCApi, ListTimeSeriesGroups) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    quiver_time_series_metadata_t* groups = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_groups(db, "Resource", &groups, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 3);  // group1, group2, group3

    quiver_free_time_series_metadata_array(groups, count);
    quiver_database_close(db);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(TimeSeriesCApi, InvalidArguments) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Null db
    auto err =
        quiver_database_add_time_series_row(nullptr, "Resource", "value1", 1, 42.5, "2024-01-01T00:00:00", nullptr,
                                            nullptr, 0);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    // Null collection
    err = quiver_database_add_time_series_row(db, nullptr, "value1", 1, 42.5, "2024-01-01T00:00:00", nullptr, nullptr,
                                              0);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    // Null date_time
    err = quiver_database_add_time_series_row(db, "Resource", "value1", 1, 42.5, nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(err, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_database_close(db);
}

TEST(TimeSeriesCApi, MissingDimension) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");

    // group2 requires block dimension
    auto err =
        quiver_database_add_time_series_row(db, "Resource", "value3", id, 42.5, "2024-01-01T00:00:00", nullptr, nullptr,
                                            0);
    EXPECT_EQ(err, QUIVER_ERROR_DATABASE);

    quiver_database_close(db);
}

TEST(TimeSeriesCApi, InvalidGroup) {
    auto options = quiver::test::quiet_options();
    auto db = quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series.sql").c_str(), &options);
    ASSERT_NE(db, nullptr);

    int64_t id = create_resource(db, "Resource 1");

    char* json = nullptr;
    auto err = quiver_database_read_time_series_table(db, "Resource", "nonexistent", id, &json);
    EXPECT_EQ(err, QUIVER_ERROR_DATABASE);

    err = quiver_database_delete_time_series(db, "Resource", "nonexistent", id);
    EXPECT_EQ(err, QUIVER_ERROR_DATABASE);

    quiver_database_close(db);
}
