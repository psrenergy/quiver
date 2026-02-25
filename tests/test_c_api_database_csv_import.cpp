#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <quiver/c/options.h>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

// Helper: read file contents as a string (binary mode to preserve LF)
static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Helper: get a unique temp path for a test
static fs::path temp_csv(const std::string& name) {
    return fs::temp_directory_path() / ("quiver_c_test_" + name + ".csv");
}

// Helper: write a CSV file from string
static void write_csv_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

// ============================================================================
// CSV Import tests
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Scalar_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    // Create elements
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    quiver_element_set_float(e1, "price", 9.99);
    quiver_element_set_string(e1, "date_created", "2024-01-15T10:30:00");
    quiver_element_set_string(e1, "notes", "first");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item2");
    quiver_element_set_string(e2, "name", "Beta");
    quiver_element_set_integer(e2, "status", 2);
    quiver_element_set_float(e2, "price", 19.5);
    quiver_element_set_string(e2, "date_created", "2024-02-20T08:00:00");
    quiver_element_set_string(e2, "notes", "second");
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e2, &id2), QUIVER_OK);
    quiver_element_destroy(e2);

    // Export
    auto csv_path = temp_csv("ImportScalarRT");
    auto csv_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_options), QUIVER_OK);

    // Import into fresh DB
    quiver_database_t* db2 = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db2),
              QUIVER_OK);

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db2, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_OK);

    // Verify
    char** names = nullptr;
    size_t name_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db2, "Items", "name", &names, &name_count), QUIVER_OK);
    ASSERT_EQ(name_count, 2u);
    EXPECT_STREQ(names[0], "Alpha");
    EXPECT_STREQ(names[1], "Beta");
    quiver_database_free_string_array(names, name_count);

    fs::remove(csv_path);
    quiver_database_close(db);
    quiver_database_close(db2);
}

TEST(DatabaseCApiCSV, ImportCSV_Vector_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    double vec_values[] = {1.1, 2.2, 3.3};
    quiver_element_set_array_float(update, "measurement", vec_values, 3);
    ASSERT_EQ(quiver_database_update_element(db, "Items", id1, update), QUIVER_OK);
    quiver_element_destroy(update);

    // Export
    auto csv_path = temp_csv("ImportVectorRT");
    auto csv_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "measurements", csv_path.string().c_str(), &csv_options), QUIVER_OK);

    // Clear vector data and re-import (parent element must exist for group import)
    quiver_element_t* clear_vec = nullptr;
    ASSERT_EQ(quiver_element_create(&clear_vec), QUIVER_OK);
    quiver_element_set_array_float(clear_vec, "measurement", nullptr, 0);
    ASSERT_EQ(quiver_database_update_element(db, "Items", id1, clear_vec), QUIVER_OK);
    quiver_element_destroy(clear_vec);

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "measurements", csv_path.string().c_str(), &import_options),
              QUIVER_OK);

    // Verify
    double* vals = nullptr;
    size_t val_count = 0;
    ASSERT_EQ(quiver_database_read_vector_floats_by_id(db, "Items", "measurement", id1, &vals, &val_count), QUIVER_OK);
    ASSERT_EQ(val_count, 3u);
    EXPECT_NEAR(vals[0], 1.1, 0.001);
    EXPECT_NEAR(vals[1], 2.2, 0.001);
    EXPECT_NEAR(vals[2], 3.3, 0.001);
    quiver_database_free_float_array(vals);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_Set_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* set_values[] = {"red", "green", "blue"};
    quiver_element_set_array_string(update, "tag", set_values, 3);
    ASSERT_EQ(quiver_database_update_element(db, "Items", id1, update), QUIVER_OK);
    quiver_element_destroy(update);

    // Export
    auto csv_path = temp_csv("ImportSetRT");
    auto csv_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "tags", csv_path.string().c_str(), &csv_options), QUIVER_OK);

    // Clear set data and re-import (parent element must exist for group import)
    quiver_element_t* clear_set = nullptr;
    ASSERT_EQ(quiver_element_create(&clear_set), QUIVER_OK);
    const char* empty_set[] = {nullptr};
    quiver_element_set_array_string(clear_set, "tag", empty_set, 0);
    ASSERT_EQ(quiver_database_update_element(db, "Items", id1, clear_set), QUIVER_OK);
    quiver_element_destroy(clear_set);

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "tags", csv_path.string().c_str(), &import_options), QUIVER_OK);

    // Verify
    char** tags = nullptr;
    size_t tag_count = 0;
    ASSERT_EQ(quiver_database_read_set_strings_by_id(db, "Items", "tag", id1, &tags, &tag_count), QUIVER_OK);
    ASSERT_EQ(tag_count, 3u);

    std::set<std::string> tag_set;
    for (size_t i = 0; i < tag_count; ++i) {
        tag_set.insert(tags[i]);
    }
    EXPECT_TRUE(tag_set.count("red"));
    EXPECT_TRUE(tag_set.count("green"));
    EXPECT_TRUE(tag_set.count("blue"));
    quiver_database_free_string_array(tags, tag_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_TimeSeries_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    const char* col_names[] = {"date_time", "temperature", "humidity"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};
    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-01T11:00:00"};
    double temperatures[] = {22.5, 23.0};
    int64_t humidities[] = {60, 55};
    const void* col_data[] = {date_times, temperatures, humidities};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Items", "readings", id1, col_names, col_types, col_data, 3, 2),
        QUIVER_OK);

    // Export
    auto csv_path = temp_csv("ImportTSRT");
    auto csv_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "readings", csv_path.string().c_str(), &csv_options), QUIVER_OK);

    // Clear and re-import
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Items", "readings", id1, nullptr, nullptr, nullptr, 0, 0),
              QUIVER_OK);

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "readings", csv_path.string().c_str(), &import_options), QUIVER_OK);

    // Verify via read
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Items",
                                                     "readings",
                                                     id1,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_count,
                                                     &out_row_count),
              QUIVER_OK);
    EXPECT_EQ(out_row_count, 2u);
    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_Scalar_HeaderOnly_ClearsTable) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    // Populate
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    // Import header-only CSV
    auto csv_path = temp_csv("ImportHeaderOnly");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\n");

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_OK);

    // Verify table is empty
    char** names = nullptr;
    size_t name_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Items", "name", &names, &name_count), QUIVER_OK);
    EXPECT_EQ(name_count, 0u);
    quiver_database_free_string_array(names, name_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Semicolon delimiter handling
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_SemicolonSepHeader_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    auto csv_path = temp_csv("ImportSemicolonSep");
    write_csv_file(csv_path.string(), "sep=;\nlabel;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n");

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_OK);

    char** names = nullptr;
    size_t name_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Items", "name", &names, &name_count), QUIVER_OK);
    ASSERT_EQ(name_count, 1u);
    EXPECT_STREQ(names[0], "Alpha");
    quiver_database_free_string_array(names, name_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_SemicolonAutoDetect_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    auto csv_path = temp_csv("ImportSemicolonAuto");
    write_csv_file(csv_path.string(), "label;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n");

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_OK);

    char** names = nullptr;
    size_t name_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Items", "name", &names, &name_count), QUIVER_OK);
    ASSERT_EQ(name_count, 1u);
    EXPECT_STREQ(names[0], "Alpha");
    quiver_database_free_string_array(names, name_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Cannot open file
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_CannotOpenFile_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Items", "", "/nonexistent/path/file.csv", &import_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Cannot import_csv: could not open file"), std::string::npos);

    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Custom datetime format parse failure
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Scalar_BadCustomDateTimeFormat_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    auto csv_path = temp_csv("ImportBadCustomDateTime");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,not-a-date,\n");

    quiver_csv_options_t csv_options = {};
    csv_options.date_time_format = "%Y/%m/%d";
    csv_options.enum_attribute_names = nullptr;
    csv_options.enum_locale_names = nullptr;
    csv_options.enum_entry_counts = nullptr;
    csv_options.enum_labels = nullptr;
    csv_options.enum_values = nullptr;
    csv_options.enum_group_count = 0;

    EXPECT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &csv_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Timestamp not-a-date is not valid"), std::string::npos);
    EXPECT_NE(err.find("format %Y/%m/%d"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Per-row column count mismatch
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_RowColumnCountMismatch_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    auto csv_path = temp_csv("ImportRowColMismatch");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,1,9.99,,note,EXTRA\n");

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("but the header has"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Self-FK label not found
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Scalar_SelfFK_InvalidLabel_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create a Parent element
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent1");
    int64_t pid = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &pid), QUIVER_OK);
    quiver_element_destroy(p1);

    auto csv_path = temp_csv("ImportSelfFKBad");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,parent_id,sibling_id\n"
                   "Child1,Parent1,\n"
                   "Child2,Parent1,NonExistent\n");

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Child", "", csv_path.string().c_str(), &import_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Could not find an existing element from collection Child with label NonExistent"),
              std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Group FK tests (vector with FK)
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Vector_WithFK_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    // Create parent elements
    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent1");
    int64_t pid1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &pid1), QUIVER_OK);
    quiver_element_destroy(p1);

    quiver_element_t* p2 = nullptr;
    ASSERT_EQ(quiver_element_create(&p2), QUIVER_OK);
    quiver_element_set_string(p2, "label", "Parent2");
    int64_t pid2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p2, &pid2), QUIVER_OK);
    quiver_element_destroy(p2);

    // Create child element
    quiver_element_t* c1 = nullptr;
    ASSERT_EQ(quiver_element_create(&c1), QUIVER_OK);
    quiver_element_set_string(c1, "label", "Child1");
    quiver_element_set_integer(c1, "parent_id", pid1);
    int64_t cid = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", c1, &cid), QUIVER_OK);
    quiver_element_destroy(c1);

    // Import vector group with FK (parent_ref -> Parent)
    auto csv_path = temp_csv("ImportVectorFK");
    write_csv_file(csv_path.string(),
                   "sep=,\nid,vector_index,parent_ref\n"
                   "Child1,1,Parent1\n"
                   "Child1,2,Parent2\n");

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Child", "refs", csv_path.string().c_str(), &import_options), QUIVER_OK);

    int64_t* vals = nullptr;
    size_t val_count = 0;
    ASSERT_EQ(quiver_database_read_vector_integers_by_id(db, "Child", "parent_ref", cid, &vals, &val_count), QUIVER_OK);
    ASSERT_EQ(val_count, 2u);
    EXPECT_EQ(vals[0], pid1);
    EXPECT_EQ(vals[1], pid2);
    quiver_database_free_integer_array(vals);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_Vector_FK_InvalidLabel_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    quiver_element_t* p1 = nullptr;
    ASSERT_EQ(quiver_element_create(&p1), QUIVER_OK);
    quiver_element_set_string(p1, "label", "Parent1");
    int64_t pid = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Parent", p1, &pid), QUIVER_OK);
    quiver_element_destroy(p1);

    quiver_element_t* c1 = nullptr;
    ASSERT_EQ(quiver_element_create(&c1), QUIVER_OK);
    quiver_element_set_string(c1, "label", "Child1");
    quiver_element_set_integer(c1, "parent_id", pid);
    int64_t cid = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", c1, &cid), QUIVER_OK);
    quiver_element_destroy(c1);

    auto csv_path = temp_csv("ImportVectorFKBad");
    write_csv_file(csv_path.string(), "sep=,\nid,vector_index,parent_ref\nChild1,1,NonExistent\n");

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Child", "refs", csv_path.string().c_str(), &import_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Could not find an existing element from collection Parent with label NonExistent"),
              std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Group NOT NULL validation
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Group_NotNull_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportGroupNotNull");
    write_csv_file(csv_path.string(), "sep=,\nid,tag\nItem1,\n");

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Items", "tags", csv_path.string().c_str(), &import_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Column tag cannot be NULL"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Group enum resolution (integer column with enum labels)
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_TimeSeries_EnumInGroup_RoundTrip) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportTSEnum");
    write_csv_file(csv_path.string(),
                   "sep=,\nid,date_time,temperature,humidity\n"
                   "Item1,2024-01-01T10:00:00,22.5,Low\n"
                   "Item1,2024-01-01T11:00:00,23.0,High\n");

    const char* attr_names[] = {"humidity"};
    const char* locale_names[] = {"en"};
    size_t entry_counts[] = {2};
    const char* labels[] = {"Low", "High"};
    int64_t values[] = {60, 90};

    quiver_csv_options_t csv_options = {};
    csv_options.date_time_format = "";
    csv_options.enum_attribute_names = attr_names;
    csv_options.enum_locale_names = locale_names;
    csv_options.enum_entry_counts = entry_counts;
    csv_options.enum_labels = labels;
    csv_options.enum_values = values;
    csv_options.enum_group_count = 1;

    ASSERT_EQ(quiver_database_import_csv(db, "Items", "readings", csv_path.string().c_str(), &csv_options), QUIVER_OK);

    // Read back time series to verify enum resolution
    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    size_t out_col_count = 0;
    size_t out_row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Items",
                                                     "readings",
                                                     id1,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_count,
                                                     &out_row_count),
              QUIVER_OK);
    ASSERT_EQ(out_row_count, 2u);

    // Find the humidity column index
    size_t humidity_idx = SIZE_MAX;
    for (size_t i = 0; i < out_col_count; ++i) {
        if (std::string(out_col_names[i]) == "humidity") {
            humidity_idx = i;
            break;
        }
    }
    ASSERT_NE(humidity_idx, SIZE_MAX);
    ASSERT_EQ(out_col_types[humidity_idx], QUIVER_DATA_TYPE_INTEGER);

    auto* humidity_data = static_cast<int64_t*>(out_col_data[humidity_idx]);
    EXPECT_EQ(humidity_data[0], 60);
    EXPECT_EQ(humidity_data[1], 90);

    quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Group invalid enum (no mapping provided for INTEGER column)
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Group_InvalidEnum_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportGroupBadEnum");
    write_csv_file(csv_path.string(),
                   "sep=,\nid,date_time,temperature,humidity\n"
                   "Item1,2024-01-01T10:00:00,22.5,Unknown\n");

    const char* attr_names[] = {"humidity"};
    const char* locale_names[] = {"en"};
    size_t entry_counts[] = {2};
    const char* labels[] = {"Low", "High"};
    int64_t values[] = {60, 90};

    quiver_csv_options_t csv_options = {};
    csv_options.date_time_format = "";
    csv_options.enum_attribute_names = attr_names;
    csv_options.enum_locale_names = locale_names;
    csv_options.enum_entry_counts = entry_counts;
    csv_options.enum_labels = labels;
    csv_options.enum_values = values;
    csv_options.enum_group_count = 1;

    EXPECT_EQ(quiver_database_import_csv(db, "Items", "readings", csv_path.string().c_str(), &csv_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Invalid enum value"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Group duplicate entries (UNIQUE constraint violation)
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Group_DuplicateEntries_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportGroupDuplicates");
    write_csv_file(csv_path.string(), "sep=,\nid,tag\nItem1,red\nItem1,red\n");

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Items", "tags", csv_path.string().c_str(), &import_options), QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("duplicate entries"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV Import: Vector non-numeric index
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Vector_NonNumericIndex_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportVectorNonNumericIdx");
    write_csv_file(csv_path.string(), "sep=,\nid,vector_index,measurement\nItem1,abc,1.1\n");

    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Items", "measurements", csv_path.string().c_str(), &import_options),
              QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("vector_index must be consecutive"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// import_csv: Trailing empty columns (Excel artifact)
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Scalar_TrailingEmptyColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    auto csv_path = temp_csv("ImportScalarTrailing");
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "label,name,status,price,date_created,notes,,,,\n"
                   "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,,,\n"
                   "Item2,Beta,2,19.5,2024-02-20T08:00:00,second,,,,\n");

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_OK);

    char** names = nullptr;
    size_t name_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Items", "name", &names, &name_count), QUIVER_OK);
    ASSERT_EQ(name_count, 2u);
    EXPECT_STREQ(names[0], "Alpha");
    EXPECT_STREQ(names[1], "Beta");
    quiver_database_free_string_array(names, name_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_Vector_TrailingEmptyColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportVectorTrailing");
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "id,vector_index,measurement,,,\n"
                   "Item1,1,1.1,,,\n"
                   "Item1,2,2.2,,,\n");

    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "measurements", csv_path.string().c_str(), &import_options),
              QUIVER_OK);

    double* vals = nullptr;
    size_t val_count = 0;
    ASSERT_EQ(quiver_database_read_vector_floats_by_id(db, "Items", "measurement", id1, &vals, &val_count), QUIVER_OK);
    ASSERT_EQ(val_count, 2u);
    EXPECT_NEAR(vals[0], 1.1, 0.001);
    EXPECT_NEAR(vals[1], 2.2, 0.001);
    quiver_database_free_float_array(vals);

    fs::remove(csv_path);
    quiver_database_close(db);
}
