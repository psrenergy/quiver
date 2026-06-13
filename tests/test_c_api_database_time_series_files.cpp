#include "test_utils.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// Time series files tests
// ============================================================================

TEST(DatabaseCApi, HasTimeSeriesFiles) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int result = 0;
    auto err = quiver_database_has_time_series_files(db, "Collection", &result);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(result, 1);

    err = quiver_database_has_time_series_files(db, "Configuration", &result);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(result, 0);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ListTimeSeriesFilesColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** columns = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_time_series_files_columns(db, "Collection", &columns, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    // Check both columns exist (order may vary)
    bool found_data_file = false;
    bool found_metadata_file = false;
    for (size_t i = 0; i < count; ++i) {
        if (std::string(columns[i]) == "data_file")
            found_data_file = true;
        if (std::string(columns[i]) == "metadata_file")
            found_metadata_file = true;
    }
    EXPECT_TRUE(found_data_file);
    EXPECT_TRUE(found_metadata_file);

    quiver_database_free_string_array(columns, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesFilesEmpty) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** columns = nullptr;
    char** paths = nullptr;
    size_t count = 0;
    auto err = quiver_database_read_time_series_files(db, "Collection", &columns, &paths, &count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(count, 2);

    // All paths should be null (no data yet)
    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(paths[i], nullptr);
    }

    quiver_database_free_time_series_files(columns, paths, count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateAndReadTimeSeriesFiles) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* columns[] = {"data_file", "metadata_file"};
    const char* paths[] = {"/path/to/data.csv", "/path/to/meta.json"};
    auto err = quiver_database_update_time_series_files(db, "Collection", columns, paths, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_columns = nullptr;
    char** out_paths = nullptr;
    size_t out_count = 0;
    err = quiver_database_read_time_series_files(db, "Collection", &out_columns, &out_paths, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 2);

    // Find and verify each column
    for (size_t i = 0; i < out_count; ++i) {
        if (std::string(out_columns[i]) == "data_file") {
            EXPECT_STREQ(out_paths[i], "/path/to/data.csv");
        } else if (std::string(out_columns[i]) == "metadata_file") {
            EXPECT_STREQ(out_paths[i], "/path/to/meta.json");
        }
    }

    quiver_database_free_time_series_files(out_columns, out_paths, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesFilesWithNulls) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* columns[] = {"data_file", "metadata_file"};
    const char* paths[] = {"/path/to/data.csv", nullptr};
    auto err = quiver_database_update_time_series_files(db, "Collection", columns, paths, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_columns = nullptr;
    char** out_paths = nullptr;
    size_t out_count = 0;
    err = quiver_database_read_time_series_files(db, "Collection", &out_columns, &out_paths, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 2);

    // Find and verify each column
    for (size_t i = 0; i < out_count; ++i) {
        if (std::string(out_columns[i]) == "data_file") {
            EXPECT_STREQ(out_paths[i], "/path/to/data.csv");
        } else if (std::string(out_columns[i]) == "metadata_file") {
            EXPECT_EQ(out_paths[i], nullptr);
        }
    }

    quiver_database_free_time_series_files(out_columns, out_paths, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesFilesReplace) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // First update
    const char* columns1[] = {"data_file", "metadata_file"};
    const char* paths1[] = {"/old/data.csv", "/old/meta.json"};
    auto err = quiver_database_update_time_series_files(db, "Collection", columns1, paths1, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Second update replaces
    const char* columns2[] = {"data_file", "metadata_file"};
    const char* paths2[] = {"/new/data.csv", "/new/meta.json"};
    err = quiver_database_update_time_series_files(db, "Collection", columns2, paths2, 2);
    EXPECT_EQ(err, QUIVER_OK);

    // Read back
    char** out_columns = nullptr;
    char** out_paths = nullptr;
    size_t out_count = 0;
    err = quiver_database_read_time_series_files(db, "Collection", &out_columns, &out_paths, &out_count);
    EXPECT_EQ(err, QUIVER_OK);

    for (size_t i = 0; i < out_count; ++i) {
        if (std::string(out_columns[i]) == "data_file") {
            EXPECT_STREQ(out_paths[i], "/new/data.csv");
        } else if (std::string(out_columns[i]) == "metadata_file") {
            EXPECT_STREQ(out_paths[i], "/new/meta.json");
        }
    }

    quiver_database_free_time_series_files(out_columns, out_paths, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, TimeSeriesFilesNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char** columns = nullptr;
    char** paths = nullptr;
    size_t count = 0;

    // Configuration has no time series files table
    auto err = quiver_database_read_time_series_files(db, "Configuration", &columns, &paths, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    err = quiver_database_list_time_series_files_columns(db, "Configuration", &columns, &count);
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, TimeSeriesFilesNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int result = 0;
    EXPECT_EQ(quiver_database_has_time_series_files(nullptr, "Collection", &result), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_has_time_series_files(db, nullptr, &result), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_has_time_series_files(db, "Collection", nullptr), QUIVER_ERROR);

    char** columns = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_database_list_time_series_files_columns(nullptr, "Collection", &columns, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, nullptr, &columns, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, "Collection", nullptr, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_list_time_series_files_columns(db, "Collection", &columns, nullptr), QUIVER_ERROR);

    char** paths = nullptr;
    EXPECT_EQ(quiver_database_read_time_series_files(nullptr, "Collection", &columns, &paths, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, nullptr, &columns, &paths, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", nullptr, &paths, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", &columns, nullptr, &count), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_files(db, "Collection", &columns, &paths, nullptr), QUIVER_ERROR);

    const char* in_columns[] = {"data_file"};
    const char* in_paths[] = {"/path"};
    EXPECT_EQ(quiver_database_update_time_series_files(nullptr, "Collection", in_columns, in_paths, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_files(db, nullptr, in_columns, in_paths, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", nullptr, in_paths, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", in_columns, nullptr, 1), QUIVER_ERROR);

    quiver_database_close(db);
}
