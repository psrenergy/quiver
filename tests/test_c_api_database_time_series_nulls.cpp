#include "test_utils.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>
#include <vector>

TEST(DatabaseCApi, ReadTimeSeriesGroupNullString) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("nullable_time_series.sql").c_str(), &options, &db),
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
    quiver_element_set_string(e1, "label", "Sensor 1");
    int64_t id = 0;
    quiver_database_create_element(db, "Sensor", e1, &id);
    EXPECT_EQ(quiver_element_destroy(e1), QUIVER_OK);

    // Insert a row whose nullable value columns stay NULL (only the dimension column is provided)
    const char* col_names[] = {"date_time"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING};
    const char* date_times[] = {"2024-01-01T10:00:00"};
    const void* col_data[] = {date_times};
    ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, col_names, col_types, col_data, 1),
              QUIVER_OK);

    // NULL cells no longer fail the read: every column comes back with a per-cell
    // presence mask. The placeholder data for a masked-out cell must be ignored.
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    auto err = quiver_database_read_time_series_group(db,
                                                      "Sensor",
                                                      "readings",
                                                      id,
                                                      &out_col_names,
                                                      &out_col_types,
                                                      &out_col_data,
                                                      &out_col_has_value,
                                                      &col_count,
                                                      &row_count);

    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(row_count, 1);
    ASSERT_EQ(col_count, 4);  // date_time + temperature + counter + status

    // Schema order: date_time (STRING), temperature (FLOAT), counter (INTEGER), status (STRING)
    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "temperature");
    EXPECT_STREQ(out_col_names[2], "counter");
    EXPECT_STREQ(out_col_names[3], "status");

    // Dimension column is always present
    EXPECT_EQ(out_col_has_value[0][0], 1);
    auto** dt = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(dt[0], "2024-01-01T10:00:00");

    // Every value column is NULL: mask 0, with placeholder data that must be ignored
    EXPECT_EQ(out_col_has_value[1][0], 0);  // temperature
    EXPECT_EQ(out_col_has_value[2][0], 0);  // counter
    EXPECT_EQ(out_col_has_value[3][0], 0);  // status
    auto** status = static_cast<char**>(out_col_data[3]);
    EXPECT_EQ(status[0], nullptr);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

// ============================================================================
// Time series group NULL-mask tests (nullable_time_series.sql:
// date_time TEXT NOT NULL, temperature REAL, counter INTEGER, status TEXT)
// ============================================================================

namespace {
// Opens an in-memory nullable_time_series.sql database with one Configuration
// and one Sensor element; returns the db and the sensor's id.
quiver_database_t* open_nullable_ts_db(int64_t* out_sensor_id) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("nullable_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    quiver_element_t* config = nullptr;
    quiver_element_create(&config);
    quiver_element_set_string(config, "label", "Config");
    int64_t tmp = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp);
    quiver_element_destroy(config);
    quiver_element_t* sensor = nullptr;
    quiver_element_create(&sensor);
    quiver_element_set_string(sensor, "label", "Sensor 1");
    quiver_database_create_element(db, "Sensor", sensor, out_sensor_id);
    quiver_element_destroy(sensor);
    return db;
}
}  // namespace

TEST(DatabaseCApi, ReadTimeSeriesGroupNullNumerics) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    // Row 1 has temperature only; row 2 has counter only.
    {
        const char* names[] = {"date_time", "temperature"};
        int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
        const char* dts[] = {"2024-01-01"};
        double temps[] = {20.0};
        const void* data[] = {dts, temps};
        ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, names, types, data, 2), QUIVER_OK);
    }
    {
        const char* names[] = {"date_time", "counter"};
        int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_INTEGER};
        const char* dts[] = {"2024-01-02"};
        int64_t counters[] = {5};
        const void* data[] = {dts, counters};
        ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, names, types, data, 2), QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    ASSERT_EQ(col_count, 4);

    // date_time (col 0) is always present
    EXPECT_EQ(out_col_has_value[0][0], 1);
    EXPECT_EQ(out_col_has_value[0][1], 1);

    // temperature (col 1): present in row 1, NULL in row 2
    auto* temps = static_cast<double*>(out_col_data[1]);
    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_DOUBLE_EQ(temps[0], 20.0);
    EXPECT_EQ(out_col_has_value[1][1], 0);

    // counter (col 2): NULL in row 1, present in row 2
    auto* counters = static_cast<int64_t*>(out_col_data[2]);
    EXPECT_EQ(out_col_has_value[2][0], 0);
    EXPECT_EQ(out_col_has_value[2][1], 1);
    EXPECT_EQ(counters[1], 5);

    // status (col 3): NULL in both rows
    EXPECT_EQ(out_col_has_value[3][0], 0);
    EXPECT_EQ(out_col_has_value[3][1], 0);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ReadTimeSeriesGroupAllNullStringColumn) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    for (const char* dt : {"2024-01-01", "2024-01-02"}) {
        const char* names[] = {"date_time"};
        int types[] = {QUIVER_DATA_TYPE_STRING};
        const char* dts[] = {dt};
        const void* data[] = {dts};
        ASSERT_EQ(quiver_database_add_time_series_row(db, "Sensor", "readings", id, names, types, data, 1), QUIVER_OK);
    }

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    ASSERT_EQ(col_count, 4);

    // status (col 3) is entirely NULL: all masks 0, all data pointers NULL
    auto** status = static_cast<char**>(out_col_data[3]);
    EXPECT_EQ(out_col_has_value[3][0], 0);
    EXPECT_EQ(out_col_has_value[3][1], 0);
    EXPECT_EQ(status[0], nullptr);
    EXPECT_EQ(status[1], nullptr);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupNullCellsRoundTrip) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature", "counter", "status"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {10.0, 0.0};              // row 2 masked out
    int64_t counters[] = {0, 7};               // row 1 masked out
    const char* statuses[] = {"ok", nullptr};  // row 2 masked out (NULL placeholder)
    const void* data[] = {dts, temps, counters, statuses};
    uint8_t m_dt[] = {1, 1};
    uint8_t m_temp[] = {1, 0};
    uint8_t m_cnt[] = {0, 1};
    uint8_t m_status[] = {1, 0};
    const uint8_t* masks[] = {m_dt, m_temp, m_cnt, m_status};
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 4, 2),
              QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);

    auto* out_temps = static_cast<double*>(out_col_data[1]);
    auto* out_cnts = static_cast<int64_t*>(out_col_data[2]);
    auto** out_status = static_cast<char**>(out_col_data[3]);

    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_DOUBLE_EQ(out_temps[0], 10.0);
    EXPECT_EQ(out_col_has_value[1][1], 0);

    EXPECT_EQ(out_col_has_value[2][0], 0);
    EXPECT_EQ(out_col_has_value[2][1], 1);
    EXPECT_EQ(out_cnts[1], 7);

    EXPECT_EQ(out_col_has_value[3][0], 1);
    EXPECT_STREQ(out_status[0], "ok");
    EXPECT_EQ(out_col_has_value[3][1], 0);
    EXPECT_EQ(out_status[1], nullptr);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupNullMaskIsDense) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {1.5, 2.5};
    const void* data[] = {dts, temps};

    // An explicit all-ones mask must behave identically to passing nullptr (dense).
    uint8_t m_dt[] = {1, 1};
    uint8_t m_temp[] = {1, 1};
    const uint8_t* masks[] = {m_dt, m_temp};
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 2, 2),
              QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    auto* out_temps = static_cast<double*>(out_col_data[1]);
    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_EQ(out_col_has_value[1][1], 1);
    EXPECT_DOUBLE_EQ(out_temps[0], 1.5);
    EXPECT_DOUBLE_EQ(out_temps[1], 2.5);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupPerColumnNullMask) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature", "counter", "status"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {1.0, 0.0};  // row 2 masked
    int64_t counters[] = {3, 4};
    const char* statuses[] = {"a", "b"};
    const void* data[] = {dts, temps, counters, statuses};
    uint8_t m_temp[] = {1, 0};
    // Only temperature carries a mask; the other columns pass nullptr (dense).
    const uint8_t* masks[] = {nullptr, m_temp, nullptr, nullptr};
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 4, 2),
              QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 2);
    auto* out_cnts = static_cast<int64_t*>(out_col_data[2]);
    auto** out_status = static_cast<char**>(out_col_data[3]);

    EXPECT_EQ(out_col_has_value[1][0], 1);
    EXPECT_EQ(out_col_has_value[1][1], 0);  // masked
    EXPECT_EQ(out_col_has_value[2][0], 1);  // dense
    EXPECT_EQ(out_col_has_value[2][1], 1);
    EXPECT_EQ(out_cnts[0], 3);
    EXPECT_EQ(out_cnts[1], 4);
    EXPECT_EQ(out_col_has_value[3][0], 1);  // dense
    EXPECT_STREQ(out_status[0], "a");
    EXPECT_STREQ(out_status[1], "b");

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupMaskedDimensionFails) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    const char* names[] = {"date_time", "temperature"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01", "2024-01-02"};
    double temps[] = {1.0, 2.0};
    const void* data[] = {dts, temps};
    uint8_t m_dt[] = {1, 0};  // masking a dimension/PK cell -> SQL NULL into a NOT NULL PK column
    uint8_t m_temp[] = {1, 1};
    const uint8_t* masks[] = {m_dt, m_temp};
    EXPECT_EQ(quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 2, 2),
              QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, UpdateTimeSeriesGroupAllNullColumnFloatTag) {
    int64_t id = 0;
    quiver_database_t* db = open_nullable_ts_db(&id);

    // counter (INTEGER) and status (TEXT) are tagged FLOAT with all-zero masks: the type tag and
    // data are ignored for masked-out cells, so both columns insert SQL NULL regardless.
    const char* names[] = {"date_time", "counter", "status"};
    int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
    const char* dts[] = {"2024-01-01"};
    double dummy_counter[] = {0.0};
    double dummy_status[] = {0.0};
    const void* data[] = {dts, dummy_counter, dummy_status};
    uint8_t m_dt[] = {1};
    uint8_t m_cnt[] = {0};
    uint8_t m_status[] = {0};
    const uint8_t* masks[] = {m_dt, m_cnt, m_status};
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Sensor", "readings", id, names, types, data, masks, 3, 1),
              QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "readings",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(row_count, 1);
    ASSERT_EQ(col_count, 4);
    // counter (col 2, INTEGER) and status (col 3, TEXT) are both NULL
    EXPECT_EQ(out_col_has_value[2][0], 0);
    EXPECT_EQ(out_col_has_value[3][0], 0);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, FreeTimeSeriesDataNull) {
    // Free with all NULLs and 0 counts - should succeed without crashing
    auto err = quiver_database_free_time_series_data(nullptr, nullptr, nullptr, nullptr, 0, 0);
    EXPECT_EQ(err, QUIVER_OK);
}
