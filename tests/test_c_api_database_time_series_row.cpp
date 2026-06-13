#include "test_utils.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

// ============================================================================
// add_time_series_row tests (CAPI-11..13)
// ============================================================================

TEST(DatabaseCApi, AddTimeSeriesRowInsert) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01T10:00:00"};
    double val_buf[] = {1.5};
    const void* col_data[] = {dt_buf, val_buf};
    auto err = quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2);
    EXPECT_EQ(err, QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(db,
                                                 "Collection",
                                                 "data",
                                                 id,
                                                 &out_col_names,
                                                 &out_col_types,
                                                 &out_col_data,
                                                 &out_col_has_value,
                                                 &col_count,
                                                 &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1u);
    ASSERT_EQ(col_count, 2u);

    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "value");
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_FLOAT);

    auto** out_dts = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_dts[0], "2024-01-01T10:00:00");
    auto* out_vals = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_vals[0], 1.5);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowUpsertSamePK) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01T10:00:00"};
    double val_first[] = {1.0};
    const void* col_data_first[] = {dt_buf, val_first};
    auto err =
        quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data_first, 2);
    EXPECT_EQ(err, QUIVER_OK);

    double val_second[] = {99.0};
    const void* col_data_second[] = {dt_buf, val_second};
    err = quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data_second, 2);
    EXPECT_EQ(err, QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    err = quiver_database_read_time_series_group(db,
                                                 "Collection",
                                                 "data",
                                                 id,
                                                 &out_col_names,
                                                 &out_col_types,
                                                 &out_col_data,
                                                 &out_col_has_value,
                                                 &col_count,
                                                 &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1u);  // upsert keyed on (id, date_time) — second call overwrites
    ASSERT_EQ(col_count, 2u);

    auto* out_vals = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_vals[0], 99.0);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowMultiDimInsert) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    const char* col_names[] = {"date_time", "block", "load", "flag"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};

    // Four rows differing in (date_time, block) — each a separate add call.
    struct Row {
        const char* dt;
        int64_t block;
        double load;
        int64_t flag;
    };
    const Row rows_to_insert[] = {
        {"2024-01-01", 1, 10.0, 1},
        {"2024-01-01", 2, 20.0, 1},
        {"2024-01-02", 1, 30.0, 0},
        {"2024-01-02", 2, 40.0, 0},
    };
    for (const auto& r : rows_to_insert) {
        const char* dt_buf[] = {r.dt};
        int64_t block_buf[] = {r.block};
        double load_buf[] = {r.load};
        int64_t flag_buf[] = {r.flag};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4);
        EXPECT_EQ(err, QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(db,
                                                      "Resource",
                                                      "load",
                                                      id,
                                                      &out_col_names,
                                                      &out_col_types,
                                                      &out_col_data,
                                                      &out_col_has_value,
                                                      &col_count,
                                                      &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 4u);

    // Locate each column index (read order is dim_col first, then value columns
    // in schema declaration order).
    int dt_idx = -1, block_idx = -1, load_idx = -1, flag_idx = -1;
    for (size_t c = 0; c < col_count; ++c) {
        std::string n = out_col_names[c];
        if (n == "date_time")
            dt_idx = static_cast<int>(c);
        else if (n == "block")
            block_idx = static_cast<int>(c);
        else if (n == "load")
            load_idx = static_cast<int>(c);
        else if (n == "flag")
            flag_idx = static_cast<int>(c);
    }
    ASSERT_NE(dt_idx, -1);
    ASSERT_NE(block_idx, -1);
    ASSERT_NE(load_idx, -1);
    ASSERT_NE(flag_idx, -1);

    auto** out_dts = static_cast<char**>(out_col_data[dt_idx]);
    auto* out_blocks = static_cast<int64_t*>(out_col_data[block_idx]);
    auto* out_loads = static_cast<double*>(out_col_data[load_idx]);
    auto* out_flags = static_cast<int64_t*>(out_col_data[flag_idx]);

    // Stable sort: read_time_series_group orders by date_time only; sort by
    // (date_time, block) to match the C++ test idiom.
    std::vector<size_t> idx{0, 1, 2, 3};
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        std::string da = out_dts[a];
        std::string db_ = out_dts[b];
        if (da != db_)
            return da < db_;
        return out_blocks[a] < out_blocks[b];
    });

    const Row expected[] = {
        {"2024-01-01", 1, 10.0, 1},
        {"2024-01-01", 2, 20.0, 1},
        {"2024-01-02", 1, 30.0, 0},
        {"2024-01-02", 2, 40.0, 0},
    };
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_STREQ(out_dts[idx[i]], expected[i].dt);
        EXPECT_EQ(out_blocks[idx[i]], expected[i].block);
        EXPECT_DOUBLE_EQ(out_loads[idx[i]], expected[i].load);
        EXPECT_EQ(out_flags[idx[i]], expected[i].flag);
    }

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowMultiDimUpsert) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    const char* col_names[] = {"date_time", "block", "load", "flag"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};

    // Initial insert at (2024-01-01, 1).
    {
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        double load_buf[] = {10.0};
        int64_t flag_buf[] = {1};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4),
                  QUIVER_OK);
    }
    // Upsert same (date_time, block) — must overwrite.
    {
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        double load_buf[] = {99.0};
        int64_t flag_buf[] = {0};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4),
                  QUIVER_OK);
    }
    // Different block at same date_time — must be a new row.
    {
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {2};
        double load_buf[] = {20.0};
        int64_t flag_buf[] = {1};
        const void* col_data[] = {dt_buf, block_buf, load_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4),
                  QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(db,
                                                      "Resource",
                                                      "load",
                                                      id,
                                                      &out_col_names,
                                                      &out_col_types,
                                                      &out_col_data,
                                                      &out_col_has_value,
                                                      &col_count,
                                                      &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 2u);  // upsert keyed on full (id, date_time, block)

    int block_idx = -1, load_idx = -1, flag_idx = -1;
    for (size_t c = 0; c < col_count; ++c) {
        std::string n = out_col_names[c];
        if (n == "block")
            block_idx = static_cast<int>(c);
        else if (n == "load")
            load_idx = static_cast<int>(c);
        else if (n == "flag")
            flag_idx = static_cast<int>(c);
    }
    ASSERT_NE(block_idx, -1);
    ASSERT_NE(load_idx, -1);
    ASSERT_NE(flag_idx, -1);

    auto* out_blocks = static_cast<int64_t*>(out_col_data[block_idx]);
    auto* out_loads = static_cast<double*>(out_col_data[load_idx]);
    auto* out_flags = static_cast<int64_t*>(out_col_data[flag_idx]);

    // Sort by block for stable assertions.
    std::vector<size_t> idx{0, 1};
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return out_blocks[a] < out_blocks[b]; });

    // (date_time=2024-01-01, block=1) — overwritten by the second insert.
    EXPECT_EQ(out_blocks[idx[0]], 1);
    EXPECT_DOUBLE_EQ(out_loads[idx[0]], 99.0);
    EXPECT_EQ(out_flags[idx[0]], 0);

    // (date_time=2024-01-01, block=2) — independent new row.
    EXPECT_EQ(out_blocks[idx[1]], 2);
    EXPECT_DOUBLE_EQ(out_loads[idx[1]], 20.0);
    EXPECT_EQ(out_flags[idx[1]], 1);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowPartialValueColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // Row 1: supply (date_time, block, load) — omit `flag`. (column_count = 3)
    {
        const char* col_names[] = {"date_time", "block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT};
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        double load_buf[] = {10.0};
        const void* col_data[] = {dt_buf, block_buf, load_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3),
                  QUIVER_OK);
    }

    // Row 2: supply (date_time, block, flag) — omit `load`. (column_count = 3)
    {
        const char* col_names[] = {"date_time", "block", "flag"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_INTEGER};
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {2};
        int64_t flag_buf[] = {5};
        const void* col_data[] = {dt_buf, block_buf, flag_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3),
                  QUIVER_OK);
    }

    // Verify both rows persisted with the supplied columns intact.
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(db,
                                                      "Resource",
                                                      "load",
                                                      id,
                                                      &out_col_names,
                                                      &out_col_types,
                                                      &out_col_data,
                                                      &out_col_has_value,
                                                      &col_count,
                                                      &row_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 2u);

    int block_idx = -1, load_idx = -1, flag_idx = -1;
    for (size_t c = 0; c < col_count; ++c) {
        std::string n = out_col_names[c];
        if (n == "block")
            block_idx = static_cast<int>(c);
        else if (n == "load")
            load_idx = static_cast<int>(c);
        else if (n == "flag")
            flag_idx = static_cast<int>(c);
    }
    ASSERT_NE(block_idx, -1);
    ASSERT_NE(load_idx, -1);
    ASSERT_NE(flag_idx, -1);

    auto* out_blocks = static_cast<int64_t*>(out_col_data[block_idx]);
    auto* out_loads = static_cast<double*>(out_col_data[load_idx]);
    auto* out_flags = static_cast<int64_t*>(out_col_data[flag_idx]);

    std::vector<size_t> idx{0, 1};
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return out_blocks[a] < out_blocks[b]; });

    // Row 1 (block=1): supplied load=10.0; omitted flag → NULL (0 via C API read).
    EXPECT_EQ(out_blocks[idx[0]], 1);
    EXPECT_DOUBLE_EQ(out_loads[idx[0]], 10.0);
    EXPECT_EQ(out_flags[idx[0]], 0);

    // Row 2 (block=2): omitted load → NULL (0.0 via C API read); supplied flag=5.
    EXPECT_EQ(out_blocks[idx[1]], 2);
    EXPECT_DOUBLE_EQ(out_loads[idx[1]], 0.0);
    EXPECT_EQ(out_flags[idx[1]], 5);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowMissingDimension) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // a. Omit `block` — only date_time + load supplied.
    {
        const char* col_names[] = {"date_time", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
        const char* dt_buf[] = {"2024-01-01"};
        double load_buf[] = {1.0};
        const void* col_data[] = {dt_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 2);
        EXPECT_EQ(err, QUIVER_ERROR);
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Cannot add_time_series_row: row missing required 'block' column"), std::string::npos)
            << "Actual: " << msg;
    }

    // b. Omit `date_time` — only block + load supplied.
    {
        const char* col_names[] = {"block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT};
        int64_t block_buf[] = {1};
        double load_buf[] = {1.0};
        const void* col_data[] = {block_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 2);
        EXPECT_EQ(err, QUIVER_ERROR);
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Cannot add_time_series_row: row missing required 'date_time' column"), std::string::npos)
            << "Actual: " << msg;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowUnknownColumn) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // Include an unknown column 'pressure' alongside the required dimensions.
    const char* col_names[] = {"date_time", "block", "load", "pressure"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01"};
    int64_t block_buf[] = {1};
    double load_buf[] = {1.0};
    double pressure_buf[] = {1013.25};
    const void* col_data[] = {dt_buf, block_buf, load_buf, pressure_buf};
    auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 4);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Cannot add_time_series_row: column 'pressure' not found in group 'load'"), std::string::npos)
        << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowTypeMismatch) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    // a. INTEGER passed for REAL column 'load' is accepted (converted on insert).
    {
        const char* col_names[] = {"date_time", "block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_INTEGER};
        const char* dt_buf[] = {"2024-01-01"};
        int64_t block_buf[] = {1};
        int64_t load_buf[] = {42};
        const void* col_data[] = {dt_buf, block_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3);
        EXPECT_EQ(err, QUIVER_OK);
    }

    // b. FLOAT passed for INTEGER column 'block'.
    {
        const char* col_names[] = {"date_time", "block", "load"};
        int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
        const char* dt_buf[] = {"2024-01-01"};
        double block_buf[] = {1.5};
        double load_buf[] = {1.0};
        const void* col_data[] = {dt_buf, block_buf, load_buf};
        auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3);
        EXPECT_EQ(err, QUIVER_ERROR);
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Cannot add_time_series_row: column"), std::string::npos) << "Actual: " << msg;
        EXPECT_NE(msg.find("has type"), std::string::npos) << "Actual: " << msg;
        EXPECT_NE(msg.find("but received"), std::string::npos) << "Actual: " << msg;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowTransactionMatrix) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Collection", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};

    // ----- Phase A: rollback discards work -----
    EXPECT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    int in_txn = 0;
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_NE(in_txn, 0);

    {
        const char* dt_buf[] = {"2024-01-01T10:00:00"};
        double val_buf[] = {1.0};
        const void* col_data[] = {dt_buf, val_buf};
        auto err = quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2);
        EXPECT_EQ(err, QUIVER_OK);
    }

    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_NE(in_txn, 0);  // nest-aware: still inside the outer txn

    EXPECT_EQ(quiver_database_rollback(db), QUIVER_OK);
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_EQ(in_txn, 0);

    {
        char** out_col_names = nullptr;
        int* out_col_types = nullptr;
        void** out_col_data = nullptr;
        uint8_t** out_col_has_value = nullptr;
        size_t col_count = 0;
        size_t row_count = 0;
        EXPECT_EQ(quiver_database_read_time_series_group(db,
                                                         "Collection",
                                                         "data",
                                                         id,
                                                         &out_col_names,
                                                         &out_col_types,
                                                         &out_col_data,
                                                         &out_col_has_value,
                                                         &col_count,
                                                         &row_count),
                  QUIVER_OK);
        EXPECT_EQ(row_count, 0u);  // rolled back — nothing persisted
        quiver_database_free_time_series_data(
            out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    }

    // ----- Phase B: explicit commit persists batched writes -----
    EXPECT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    {
        const char* dt_buf[] = {"2024-01-01T10:00:00"};
        double val_buf[] = {1.0};
        const void* col_data[] = {dt_buf, val_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2),
                  QUIVER_OK);
    }
    {
        const char* dt_buf[] = {"2024-01-02T10:00:00"};
        double val_buf[] = {2.0};
        const void* col_data[] = {dt_buf, val_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2),
                  QUIVER_OK);
    }
    EXPECT_EQ(quiver_database_commit(db), QUIVER_OK);

    {
        char** out_col_names = nullptr;
        int* out_col_types = nullptr;
        void** out_col_data = nullptr;
        uint8_t** out_col_has_value = nullptr;
        size_t col_count = 0;
        size_t row_count = 0;
        EXPECT_EQ(quiver_database_read_time_series_group(db,
                                                         "Collection",
                                                         "data",
                                                         id,
                                                         &out_col_names,
                                                         &out_col_types,
                                                         &out_col_data,
                                                         &out_col_has_value,
                                                         &col_count,
                                                         &row_count),
                  QUIVER_OK);
        EXPECT_EQ(row_count, 2u);  // both committed
        quiver_database_free_time_series_data(
            out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    }

    // ----- Phase C: standalone autocommit -----
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_EQ(in_txn, 0);
    {
        const char* dt_buf[] = {"2024-01-03T10:00:00"};
        double val_buf[] = {3.0};
        const void* col_data[] = {dt_buf, val_buf};
        EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", "data", id, col_names, col_types, col_data, 2),
                  QUIVER_OK);
    }
    EXPECT_EQ(quiver_database_in_transaction(db, &in_txn), QUIVER_OK);
    EXPECT_EQ(in_txn, 0);  // returned to autocommit

    {
        char** out_col_names = nullptr;
        int* out_col_types = nullptr;
        void** out_col_data = nullptr;
        uint8_t** out_col_has_value = nullptr;
        size_t col_count = 0;
        size_t row_count = 0;
        EXPECT_EQ(quiver_database_read_time_series_group(db,
                                                         "Collection",
                                                         "data",
                                                         id,
                                                         &out_col_names,
                                                         &out_col_types,
                                                         &out_col_data,
                                                         &out_col_has_value,
                                                         &col_count,
                                                         &row_count),
                  QUIVER_OK);
        EXPECT_EQ(row_count, 3u);  // Phase B's 2 + Phase C's 1
        quiver_database_free_time_series_data(
            out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dt_buf[] = {"2024-01-01T10:00:00"};
    double val_buf[] = {1.0};
    const void* col_data[] = {dt_buf, val_buf};

    // a. Null db.
    EXPECT_EQ(quiver_database_add_time_series_row(nullptr, "Collection", "data", 1, col_names, col_types, col_data, 2),
              QUIVER_ERROR);
    {
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;
    }

    // b. Null collection.
    EXPECT_EQ(quiver_database_add_time_series_row(db, nullptr, "data", 1, col_names, col_types, col_data, 2),
              QUIVER_ERROR);
    {
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;
    }

    // c. Null group.
    EXPECT_EQ(quiver_database_add_time_series_row(db, "Collection", nullptr, 1, col_names, col_types, col_data, 2),
              QUIVER_ERROR);
    {
        std::string msg = quiver_get_last_error();
        EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowUnknownColumnType) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* resource = nullptr;
    ASSERT_EQ(quiver_element_create(&resource), QUIVER_OK);
    quiver_element_set_string(resource, "label", "Resource 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Resource", resource, &id);
    EXPECT_EQ(quiver_element_destroy(resource), QUIVER_OK);

    const char* col_names[] = {"date_time", "block", "load"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER, 999};  // 999 is bogus
    const char* dt_buf[] = {"2024-01-01"};
    int64_t block_buf[] = {1};
    double load_buf[] = {10.0};
    const void* col_data[] = {dt_buf, block_buf, load_buf};
    auto err = quiver_database_add_time_series_row(db, "Resource", "load", id, col_names, col_types, col_data, 3);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Cannot add_time_series_row: unknown column type"), std::string::npos) << "Actual: " << msg;
    EXPECT_NE(msg.find("999"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, AddTimeSeriesRowNullColumnArraysWithCount) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto err = quiver_database_add_time_series_row(db,
                                                   "Resource",
                                                   "load",
                                                   1,
                                                   /*column_names=*/nullptr,
                                                   /*column_types=*/nullptr,
                                                   /*column_data=*/nullptr,
                                                   /*column_count=*/3);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Null argument"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

// ============================================================================
// Time series row read tests (read_time_series_row)
// ============================================================================

TEST(DatabaseCApi, ReadTimeSeriesRow) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create config
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // Create two elements
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t id2 = 0;
    quiver_database_create_element(db, "Collection", e2, &id2);
    quiver_element_destroy(e2);

    // Insert time series for both elements
    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};

    const char* dts1[] = {"2024-01-01", "2024-01-02", "2024-01-03"};
    double vals1[] = {1.0, 2.0, 3.0};
    const void* data1[] = {dts1, vals1};
    ASSERT_EQ(quiver_database_update_time_series_group(
                  db, "Collection", "data", id1, col_names, col_types, data1, nullptr, 2, 3),
              QUIVER_OK);

    const char* dts2[] = {"2024-01-01", "2024-01-02"};
    double vals2[] = {10.0, 20.0};
    const void* data2[] = {dts2, vals2};
    ASSERT_EQ(quiver_database_update_time_series_group(
                  db, "Collection", "data", id2, col_names, col_types, data2, nullptr, 2, 2),
              QUIVER_OK);

    // Read at 2024-01-02
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-02", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);
    ASSERT_EQ(out_count, 2);

    auto* floats = static_cast<double*>(out_values);
    EXPECT_DOUBLE_EQ(floats[0], 2.0);
    EXPECT_DOUBLE_EQ(floats[1], 20.0);

    quiver_database_free_float_array(floats);

    // Read at 2024-01-03: Item 1 -> 3.0, Item 2 -> 20.0 (last at or before)
    err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-03", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(out_count, 2);

    floats = static_cast<double*>(out_values);
    EXPECT_DOUBLE_EQ(floats[0], 3.0);
    EXPECT_DOUBLE_EQ(floats[1], 20.0);

    quiver_database_free_float_array(floats);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowBeforeAllData) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id1 = 0;
    quiver_database_create_element(db, "Collection", e1, &id1);
    quiver_element_destroy(e1);

    const char* col_names[] = {"date_time", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-02"};
    double vals[] = {1.0};
    const void* data[] = {dts, vals};
    ASSERT_EQ(quiver_database_update_time_series_group(
                  db, "Collection", "data", id1, col_names, col_types, data, nullptr, 2, 1),
              QUIVER_OK);

    // Query before any data: value should be NaN (null sentinel for float)
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(out_count, 1);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);

    auto* floats = static_cast<double*>(out_values);
    EXPECT_TRUE(std::isnan(floats[0]));

    quiver_database_free_float_array(floats);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowEmptyCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    // No elements in Collection
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 0);
    EXPECT_EQ(out_values, nullptr);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowMultiColumnInteger) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    quiver_element_t* sensor = nullptr;
    ASSERT_EQ(quiver_element_create(&sensor), QUIVER_OK);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", sensor, &id);
    quiver_element_destroy(sensor);

    const char* col_names[] = {"date_time", "temperature", "humidity", "status"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {20.5, 21.0};
    int64_t humids[] = {65, 70};
    const char* stats[] = {"ok", "warn"};
    const void* data[] = {dts, temps, humids, stats};
    ASSERT_EQ(quiver_database_update_time_series_group(
                  db, "Sensor", "readings", id, col_names, col_types, data, nullptr, 4, 2),
              QUIVER_OK);

    // Read humidity (INTEGER) at 2024-01-02
    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Sensor", "readings", "humidity", "2024-01-02", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_INTEGER);
    ASSERT_EQ(out_count, 1);

    auto* ints = static_cast<int64_t*>(out_values);
    EXPECT_EQ(ints[0], 70);

    quiver_database_free_integer_array(ints);

    // Read status (STRING) at 2024-01-01
    err = quiver_database_read_time_series_row(
        db, "Sensor", "readings", "status", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_STRING);
    ASSERT_EQ(out_count, 1);

    auto** strings = static_cast<char**>(out_values);
    EXPECT_STREQ(strings[0], "ok");

    quiver_database_free_string_array(strings, out_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowNullArguments) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;

    EXPECT_EQ(quiver_database_read_time_series_row(
                  nullptr, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, nullptr, "data", "value", "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", nullptr, "value", "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", nullptr, "2024-01-01", &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", nullptr, &out_type, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", "2024-01-01", nullptr, &out_values, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", "2024-01-01", &out_type, nullptr, &out_count),
              QUIVER_ERROR);
    EXPECT_EQ(quiver_database_read_time_series_row(
                  db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, nullptr),
              QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowAttributeNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "data", "nonexistent", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("Time series attribute not found"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesRowGroupNotFound) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Collection", "nonexistent", "value", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_ERROR);
    std::string msg = quiver_get_last_error();
    EXPECT_NE(msg.find("not found"), std::string::npos) << "Actual: " << msg;

    quiver_database_close(db);
}
