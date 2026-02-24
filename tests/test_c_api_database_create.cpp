#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

TEST(DatabaseCApi, CreateElementWithScalars) {
    // Test: Use C API to create element with schema
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Config 1");
    quiver_element_set_integer(element, "integer_attribute", 42);
    quiver_element_set_float(element, "float_attribute", 3.14);

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Configuration", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithVector) {
    // Test: Use C API to create element with array using schema
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    ASSERT_NE(config, nullptr);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    quiver_database_create_element(db, "Configuration", config, &config_id);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create Collection with vector
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Item 1");

    int64_t values[] = {1, 2, 3};
    quiver_element_set_array_integer(element, "value_int", values, 3);

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Collection", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNullDb) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Test");

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(nullptr, "Plant", element, &id), QUIVER_ERROR);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(DatabaseCApi, CreateElementNullCollection) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Test");

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, nullptr, element, &id), QUIVER_ERROR);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNullElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Plant", nullptr, &id), QUIVER_ERROR);

    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithTimeSeries) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create Collection element with time series arrays
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Item 1");

    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"};
    quiver_element_set_array_string(element, "date_time", date_times, 3);

    double values[] = {1.5, 2.5, 3.5};
    quiver_element_set_array_float(element, "value", values, 3);

    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    // Verify via read_time_series_group (multi-column)
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Collection",
                                                     "data",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_count,
                                                     &out_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_row_count, 3);
    ASSERT_EQ(out_col_count, 2);  // date_time + value

    auto** out_date_times = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(out_date_times[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_date_times[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_date_times[2], "2024-01-03T10:00:00");
    auto* out_values = static_cast<double*>(out_col_data[1]);
    EXPECT_DOUBLE_EQ(out_values[0], 1.5);
    EXPECT_DOUBLE_EQ(out_values[1], 2.5);
    EXPECT_DOUBLE_EQ(out_values[2], 3.5);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithMultiTimeSeries) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    // Create Sensor element with shared date_time routed to both time series tables
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Sensor 1");

    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"};
    quiver_element_set_array_string(element, "date_time", date_times, 3);

    double temps[] = {20.0, 21.5, 22.0};
    quiver_element_set_array_float(element, "temperature", temps, 3);

    double hums[] = {45.0, 50.0, 55.0};
    quiver_element_set_array_float(element, "humidity", hums, 3);

    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Sensor", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    // Verify temperature group (multi-column)
    char** out_temp_col_names = nullptr;
    int* out_temp_col_types = nullptr;
    void** out_temp_col_data = nullptr;
    size_t out_temp_col_count = 0;
    size_t out_temp_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "temperature",
                                                     id,
                                                     &out_temp_col_names,
                                                     &out_temp_col_types,
                                                     &out_temp_col_data,
                                                     &out_temp_col_count,
                                                     &out_temp_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_temp_row_count, 3);
    ASSERT_EQ(out_temp_col_count, 2);  // date_time + temperature
    auto** out_temp_dates = static_cast<char**>(out_temp_col_data[0]);
    EXPECT_STREQ(out_temp_dates[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_temp_dates[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_temp_dates[2], "2024-01-03T10:00:00");
    auto* out_temp_values = static_cast<double*>(out_temp_col_data[1]);
    EXPECT_DOUBLE_EQ(out_temp_values[0], 20.0);
    EXPECT_DOUBLE_EQ(out_temp_values[1], 21.5);
    EXPECT_DOUBLE_EQ(out_temp_values[2], 22.0);
    quiver_database_free_time_series_data(
        out_temp_col_names, out_temp_col_types, out_temp_col_data, out_temp_col_count, out_temp_row_count);

    // Verify humidity group (multi-column)
    char** out_hum_col_names = nullptr;
    int* out_hum_col_types = nullptr;
    void** out_hum_col_data = nullptr;
    size_t out_hum_col_count = 0;
    size_t out_hum_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Sensor",
                                                     "humidity",
                                                     id,
                                                     &out_hum_col_names,
                                                     &out_hum_col_types,
                                                     &out_hum_col_data,
                                                     &out_hum_col_count,
                                                     &out_hum_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_hum_row_count, 3);
    ASSERT_EQ(out_hum_col_count, 2);  // date_time + humidity
    auto** out_hum_dates = static_cast<char**>(out_hum_col_data[0]);
    EXPECT_STREQ(out_hum_dates[0], "2024-01-01T10:00:00");
    EXPECT_STREQ(out_hum_dates[1], "2024-01-02T10:00:00");
    EXPECT_STREQ(out_hum_dates[2], "2024-01-03T10:00:00");
    auto* out_hum_values = static_cast<double*>(out_hum_col_data[1]);
    EXPECT_DOUBLE_EQ(out_hum_values[0], 45.0);
    EXPECT_DOUBLE_EQ(out_hum_values[1], 50.0);
    EXPECT_DOUBLE_EQ(out_hum_values[2], 55.0);
    quiver_database_free_time_series_data(
        out_hum_col_names, out_hum_col_types, out_hum_col_data, out_hum_col_count, out_hum_row_count);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementWithDatetime) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    quiver_element_set_string(element, "label", "Config 1");
    quiver_element_set_string(element, "date_attribute", "2024-03-15T14:30:45");

    int64_t id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Configuration", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);

    char** out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_scalar_strings(db, "Configuration", "date_attribute", &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(out_count, 1);
    EXPECT_STREQ(out_values[0], "2024-03-15T14:30:45");

    quiver_database_free_string_array(out_values, out_count);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
    quiver_database_close(db);
}

// ============================================================================
// FK label resolution in create_element
// ============================================================================

TEST(DatabaseCApi, ResolveFkLabelInSetCreate) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create 2 parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    quiver_element_destroy(p1);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    quiver_element_destroy(p2);

    // Create child with set FK using string labels
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* mentor_labels[] = {"Parent 1", "Parent 2"};
    quiver_element_set_array_string(child, "mentor_id", mentor_labels, 2);

    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    quiver_element_destroy(child);

    // Verify via read_set_integers_by_id
    int64_t* values = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_set_integers_by_id(db, "Child", "mentor_id", child_id, &values, &count), QUIVER_OK);
    ASSERT_EQ(count, 2);

    std::vector<int64_t> sorted(values, values + count);
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted[0], 1);
    EXPECT_EQ(sorted[1], 2);

    quiver_database_free_integer_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, ResolveFkLabelMissingTarget) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create child with set FK referencing nonexistent parent
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* mentor_labels[] = {"Nonexistent Parent"};
    quiver_element_set_array_string(child, "mentor_id", mentor_labels, 1);

    int64_t child_id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_ERROR);

    quiver_element_destroy(child);
    quiver_database_close(db);
}

TEST(DatabaseCApi, RejectStringForNonFkIntegerColumn) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create child with string in non-FK INTEGER set column (score)
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* score_labels[] = {"not_a_label"};
    quiver_element_set_array_string(child, "score", score_labels, 1);

    int64_t child_id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_ERROR);

    quiver_element_destroy(child);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementScalarFkLabel) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create parent
    quiver_element_t* parent = nullptr;
    ASSERT_EQ(quiver_element_create(&parent), QUIVER_OK);
    quiver_element_set_string(parent, "label", "Parent 1");
    int64_t parent_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", parent, &parent_id), QUIVER_OK);
    quiver_element_destroy(parent);

    // Create child with scalar FK using string label
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_string(child, "parent_id", "Parent 1");

    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    quiver_element_destroy(child);

    // Verify via read_scalar_integers
    int64_t* values = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_integers(db, "Child", "parent_id", &values, &count), QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(values[0], 1);

    quiver_database_free_integer_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementVectorFkLabels) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    quiver_element_destroy(p1);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    quiver_element_destroy(p2);

    // Create child with vector FK using string labels
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* ref_labels[] = {"Parent 1", "Parent 2"};
    quiver_element_set_array_string(child, "parent_ref", ref_labels, 2);

    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    quiver_element_destroy(child);

    // Verify via read_vector_integers_by_id
    int64_t* values = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_vector_integers_by_id(db, "Child", "parent_ref", child_id, &values, &count),
              QUIVER_OK);
    ASSERT_EQ(count, 2);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);

    quiver_database_free_integer_array(values);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementTimeSeriesFkLabels) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    quiver_element_destroy(p1);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    quiver_element_destroy(p2);

    // Create child with time series FK using string labels
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    const char* date_times[] = {"2024-01-01", "2024-01-02"};
    quiver_element_set_array_string(child, "date_time", date_times, 2);
    const char* sponsor_labels[] = {"Parent 1", "Parent 2"};
    quiver_element_set_array_string(child, "sponsor_id", sponsor_labels, 2);

    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    quiver_element_destroy(child);

    // Verify via read_time_series_group
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db, "Child", "events", child_id, &out_col_names, &out_col_types,
                                                     &out_col_data, &out_col_count, &out_row_count),
              QUIVER_OK);
    ASSERT_EQ(out_col_count, 2);  // date_time + sponsor_id
    ASSERT_EQ(out_row_count, 2);

    // sponsor_id is col 1 (INTEGER type)
    auto* sponsor_ids = static_cast<int64_t*>(out_col_data[1]);
    EXPECT_EQ(sponsor_ids[0], 1);
    EXPECT_EQ(sponsor_ids[1], 2);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);
    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementAllFkTypesInOneCall) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create parents
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent 1");
    int64_t p1_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &p1_id), QUIVER_OK);
    quiver_element_destroy(p1);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent 2");
    int64_t p2_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &p2_id), QUIVER_OK);
    quiver_element_destroy(p2);

    // Create child with ALL FK types in one call
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_string(child, "parent_id", "Parent 1");  // scalar FK

    const char* mentor_labels[] = {"Parent 2"};
    quiver_element_set_array_string(child, "mentor_id", mentor_labels, 1);  // set FK

    const char* ref_labels[] = {"Parent 1"};
    quiver_element_set_array_string(child, "parent_ref", ref_labels, 1);  // vector+set FK

    const char* date_times[] = {"2024-01-01"};
    quiver_element_set_array_string(child, "date_time", date_times, 1);  // time series dimension

    const char* sponsor_labels[] = {"Parent 2"};
    quiver_element_set_array_string(child, "sponsor_id", sponsor_labels, 1);  // time series FK

    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    quiver_element_destroy(child);

    // Verify scalar FK
    int64_t* scalar_values = nullptr;
    size_t scalar_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_integers(db, "Child", "parent_id", &scalar_values, &scalar_count), QUIVER_OK);
    ASSERT_EQ(scalar_count, 1);
    EXPECT_EQ(scalar_values[0], 1);
    quiver_database_free_integer_array(scalar_values);

    // Verify set FK (mentor_id)
    int64_t* mentor_values = nullptr;
    size_t mentor_count = 0;
    ASSERT_EQ(
        quiver_database_read_set_integers_by_id(db, "Child", "mentor_id", child_id, &mentor_values, &mentor_count),
        QUIVER_OK);
    ASSERT_EQ(mentor_count, 1);
    EXPECT_EQ(mentor_values[0], 2);
    quiver_database_free_integer_array(mentor_values);

    // Verify vector FK (parent_ref)
    int64_t* vector_values = nullptr;
    size_t vector_count = 0;
    ASSERT_EQ(
        quiver_database_read_vector_integers_by_id(db, "Child", "parent_ref", child_id, &vector_values, &vector_count),
        QUIVER_OK);
    ASSERT_EQ(vector_count, 1);
    EXPECT_EQ(vector_values[0], 1);
    quiver_database_free_integer_array(vector_values);

    // Verify time series FK (sponsor_id)
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db, "Child", "events", child_id, &out_col_names, &out_col_types,
                                                     &out_col_data, &out_col_count, &out_row_count),
              QUIVER_OK);
    ASSERT_EQ(out_col_count, 2);
    ASSERT_EQ(out_row_count, 1);
    auto* sponsor_ids = static_cast<int64_t*>(out_col_data[1]);
    EXPECT_EQ(sponsor_ids[0], 2);
    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);

    quiver_database_close(db);
}

TEST(DatabaseCApi, CreateElementNoFkColumnsUnchanged) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);

    // basic.sql has no FK columns -- pre-resolve pass is a no-op
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Config 1");
    quiver_element_set_integer(element, "integer_attribute", 42);
    quiver_element_set_float(element, "float_attribute", 3.14);

    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", element, &id), QUIVER_OK);
    EXPECT_EQ(id, 1);
    quiver_element_destroy(element);

    // Verify all values read back correctly
    char** str_values = nullptr;
    size_t str_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Configuration", "label", &str_values, &str_count), QUIVER_OK);
    ASSERT_EQ(str_count, 1);
    EXPECT_STREQ(str_values[0], "Config 1");
    quiver_database_free_string_array(str_values, str_count);

    int64_t* int_values = nullptr;
    size_t int_count = 0;
    ASSERT_EQ(
        quiver_database_read_scalar_integers(db, "Configuration", "integer_attribute", &int_values, &int_count),
        QUIVER_OK);
    ASSERT_EQ(int_count, 1);
    EXPECT_EQ(int_values[0], 42);
    quiver_database_free_integer_array(int_values);

    double* float_values = nullptr;
    size_t float_count = 0;
    ASSERT_EQ(
        quiver_database_read_scalar_floats(db, "Configuration", "float_attribute", &float_values, &float_count),
        QUIVER_OK);
    ASSERT_EQ(float_count, 1);
    EXPECT_DOUBLE_EQ(float_values[0], 3.14);
    quiver_database_free_float_array(float_values);

    quiver_database_close(db);
}

TEST(DatabaseCApi, ScalarFkResolutionFailureCausesNoPartialWrites) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create child with scalar FK referencing nonexistent parent
    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Orphan Child");
    quiver_element_set_string(child, "parent_id", "Nonexistent Parent");

    int64_t child_id = 0;
    EXPECT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_ERROR);
    quiver_element_destroy(child);

    // Verify: no child was created (zero partial writes)
    char** labels = nullptr;
    size_t label_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Child", "label", &labels, &label_count), QUIVER_OK);
    EXPECT_EQ(label_count, 0);

    quiver_database_close(db);
}
