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

// ============================================================================
// CSV-01: export_csv routing (scalar, vector, set, time series, invalid)
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_ScalarExport_HeaderAndData) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create element 1
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

    // Create element 2
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

    auto csv_path = temp_csv("ScalarExport");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Header: schema order columns minus id
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);

    // Data rows
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_VectorGroupExport) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item2");
    quiver_element_set_string(e2, "name", "Beta");
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e2, &id2), QUIVER_OK);
    quiver_element_destroy(e2);

    double vec_values1[] = {1.1, 2.2, 3.3};
    ASSERT_EQ(quiver_database_update_vector_floats(db, "Items", "measurement", id1, vec_values1, 3), QUIVER_OK);
    double vec_values2[] = {4.4, 5.5};
    ASSERT_EQ(quiver_database_update_vector_floats(db, "Items", "measurement", id2, vec_values2, 2), QUIVER_OK);

    auto csv_path = temp_csv("VectorExport");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "measurements", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Header: id + vector_index + value columns
    EXPECT_NE(content.find("sep=,\nid,vector_index,measurement\n"), std::string::npos);

    // Data rows: one row per vector element with vector_index
    EXPECT_NE(content.find("Item1,1,1.1\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,2,2.2\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,3,3.3\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,1,4.4\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,2,5.5\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_SetGroupExport) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    const char* set_values[] = {"red", "green", "blue"};
    ASSERT_EQ(quiver_database_update_set_strings(db, "Items", "tag", id1, set_values, 3), QUIVER_OK);

    auto csv_path = temp_csv("SetExport");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "tags", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Header: id + tag
    EXPECT_NE(content.find("sep=,\nid,tag\n"), std::string::npos);

    // Data rows
    EXPECT_NE(content.find("Item1,red\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,green\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,blue\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_TimeSeriesGroupExport) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    // Update time series via C API columnar interface
    const char* col_names[] = {"date_time", "temperature", "humidity"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER};
    const char* date_times[] = {"2024-01-01T10:00:00", "2024-01-01T11:00:00"};
    double temperatures[] = {22.5, 23.0};
    int64_t humidities[] = {60, 55};
    const void* col_data[] = {date_times, temperatures, humidities};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Items", "readings", id1, col_names, col_types, col_data, 3, 2),
        QUIVER_OK);

    auto csv_path = temp_csv("TimeSeriesExport");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "readings", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Header: id + dimension + value columns
    EXPECT_NE(content.find("sep=,\nid,date_time,temperature,humidity\n"), std::string::npos);

    // Data rows ordered by date_time
    EXPECT_NE(content.find("Item1,2024-01-01T10:00:00,22.5,60\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,2024-01-01T11:00:00,23,55\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_InvalidGroup_ReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto csv_path = temp_csv("InvalidGroup");
    auto csv_opts = quiver_csv_export_options_default();
    EXPECT_EQ(quiver_database_export_csv(db, "Items", "nonexistent", csv_path.string().c_str(), &csv_opts),
              QUIVER_ERROR);

    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("Cannot export_csv: group not found"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV-02: RFC 4180 compliance
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_RFC4180_CommaEscaping) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha, Beta");
    quiver_element_set_integer(e1, "status", 1);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("CommaEscaping");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Field with comma must be wrapped in double quotes
    EXPECT_NE(content.find("\"Alpha, Beta\""), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_RFC4180_QuoteEscaping) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "He said \"hello\"");
    quiver_element_set_integer(e1, "status", 1);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("QuoteEscaping");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Field with quotes: wrapped and quotes doubled
    EXPECT_NE(content.find("\"He said \"\"hello\"\"\""), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_RFC4180_NewlineEscaping) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "line1\nline2");
    quiver_element_set_integer(e1, "status", 1);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("NewlineEscaping");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Field with newline must be wrapped in double quotes
    EXPECT_NE(content.find("\"line1\nline2\""), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_LFLineEndings) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("LFLineEndings");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // No CRLF should be present (only LF)
    EXPECT_EQ(content.find("\r\n"), std::string::npos);
    // But LF should be present
    EXPECT_NE(content.find("\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV-03: Empty collection
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_EmptyCollection_HeaderOnly) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    auto csv_path = temp_csv("EmptyCollection");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Header row only, followed by LF
    EXPECT_EQ(content, "sep=,\nlabel,name,status,price,date_created,notes\n");

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// CSV-04: NULL values
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_NullValues_EmptyFields) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    // status, price, date_created, notes all left NULL
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("NullValues");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // NULL fields appear as empty (just commas)
    // Expected: Item1,Alpha,,,,
    EXPECT_NE(content.find("Item1,Alpha,,,,\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// OPT-01: Default options (raw values)
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_DefaultOptions_RawValues) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    quiver_element_set_float(e1, "price", 9.99);
    quiver_element_set_string(e1, "date_created", "2024-01-15T10:30:00");
    quiver_element_set_string(e1, "notes", "note");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("DefaultOptions");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // With default options, integer enum columns have raw integers
    EXPECT_NE(content.find(",1,"), std::string::npos);
    // DateTime columns have raw strings
    EXPECT_NE(content.find("2024-01-15T10:30:00"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// OPT-02: Enum resolution
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_EnumLabels_ReplacesIntegers) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item2");
    quiver_element_set_string(e2, "name", "Beta");
    quiver_element_set_integer(e2, "status", 2);
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e2, &id2), QUIVER_OK);
    quiver_element_destroy(e2);

    // Set up enum labels via C API flat struct
    const char* attr_names[] = {"status"};
    size_t entry_counts[] = {2};
    int64_t values[] = {1, 2};
    const char* labels[] = {"Active", "Inactive"};

    quiver_csv_export_options_t csv_opts = {};
    csv_opts.date_time_format = "";
    csv_opts.enum_attribute_names = attr_names;
    csv_opts.enum_entry_counts = entry_counts;
    csv_opts.enum_values = values;
    csv_opts.enum_labels = labels;
    csv_opts.enum_attribute_count = 1;

    auto csv_path = temp_csv("EnumReplace");
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // status column should have labels instead of integers
    EXPECT_NE(content.find("Item1,Alpha,Active,"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,Inactive,"), std::string::npos);

    // Raw integers 1 and 2 should NOT be present as status values
    EXPECT_EQ(content.find("Item1,Alpha,1,"), std::string::npos);
    EXPECT_EQ(content.find("Item2,Beta,2,"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_EnumLabels_UnmappedFallback) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item2");
    quiver_element_set_string(e2, "name", "Beta");
    quiver_element_set_integer(e2, "status", 3);
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e2, &id2), QUIVER_OK);
    quiver_element_destroy(e2);

    // Only map value 1
    const char* attr_names[] = {"status"};
    size_t entry_counts[] = {1};
    int64_t values[] = {1};
    const char* labels[] = {"Active"};

    quiver_csv_export_options_t csv_opts = {};
    csv_opts.date_time_format = "";
    csv_opts.enum_attribute_names = attr_names;
    csv_opts.enum_entry_counts = entry_counts;
    csv_opts.enum_values = values;
    csv_opts.enum_labels = labels;
    csv_opts.enum_attribute_count = 1;

    auto csv_path = temp_csv("EnumFallback");
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Mapped value replaced
    EXPECT_NE(content.find("Item1,Alpha,Active,"), std::string::npos);
    // Unmapped value falls back to raw integer string
    EXPECT_NE(content.find("Item2,Beta,3,"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// OPT-03: Date formatting
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_DateTimeFormat_FormatsDateColumns) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    quiver_element_set_string(e1, "date_created", "2024-01-15T10:30:00");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_csv_export_options_t csv_opts = {};
    csv_opts.date_time_format = "%Y/%m/%d";
    csv_opts.enum_attribute_names = nullptr;
    csv_opts.enum_entry_counts = nullptr;
    csv_opts.enum_values = nullptr;
    csv_opts.enum_labels = nullptr;
    csv_opts.enum_attribute_count = 0;

    auto csv_path = temp_csv("DateFormat");
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // date_created column should be formatted
    EXPECT_NE(content.find("2024/01/15"), std::string::npos);
    // Raw ISO format should NOT appear
    EXPECT_EQ(content.find("2024-01-15T10:30:00"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_DateTimeFormat_NonDateColumnsUnaffected) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "2024-01-15T10:30:00");  // looks like a date but column is not date_*
    quiver_element_set_integer(e1, "status", 1);
    quiver_element_set_string(e1, "date_created", "2024-01-15T10:30:00");
    quiver_element_set_string(e1, "notes", "2024-01-15T10:30:00");  // also not a date column
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_csv_export_options_t csv_opts = {};
    csv_opts.date_time_format = "%Y/%m/%d";
    csv_opts.enum_attribute_names = nullptr;
    csv_opts.enum_entry_counts = nullptr;
    csv_opts.enum_values = nullptr;
    csv_opts.enum_labels = nullptr;
    csv_opts.enum_attribute_count = 0;

    auto csv_path = temp_csv("NonDateUnaffected");
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // date_created column formatted
    EXPECT_NE(content.find("2024/01/15"), std::string::npos);

    // name and notes columns should still have raw ISO string
    // Count occurrences of the raw ISO string (should be 2: name and notes)
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("2024-01-15T10:30:00", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    EXPECT_EQ(count, 2u);  // name and notes columns unformatted

    fs::remove(csv_path);
    quiver_database_close(db);
}

// ============================================================================
// OPT-04: Default options factory
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_DefaultOptionsFactory) {
    auto opts = quiver_csv_export_options_default();

    // date_time_format is empty string (not NULL)
    ASSERT_NE(opts.date_time_format, nullptr);
    EXPECT_STREQ(opts.date_time_format, "");

    // No enum mappings
    EXPECT_EQ(opts.enum_attribute_count, 0u);
    EXPECT_EQ(opts.enum_attribute_names, nullptr);
    EXPECT_EQ(opts.enum_entry_counts, nullptr);
    EXPECT_EQ(opts.enum_values, nullptr);
    EXPECT_EQ(opts.enum_labels, nullptr);
}

// ============================================================================
// Additional: parent directory creation and overwrite behavior
// ============================================================================

TEST(DatabaseCApiCSV, ExportCSV_CreatesParentDirectories) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = fs::temp_directory_path() / "quiver_c_test_nested" / "subdir" / "output.csv";
    // Ensure parent does not exist
    fs::remove_all(fs::temp_directory_path() / "quiver_c_test_nested");

    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    EXPECT_TRUE(fs::exists(csv_path));
    auto content = read_file(csv_path.string());
    EXPECT_NE(content.find("Item1"), std::string::npos);

    // Cleanup
    fs::remove_all(fs::temp_directory_path() / "quiver_c_test_nested");
    quiver_database_close(db);
}

// ============================================================================
// CSV Import tests
// ============================================================================

// Helper: write a CSV file from string
static void write_csv_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

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
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    // Import into fresh DB
    quiver_database_t* db2 = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db2),
              QUIVER_OK);

    auto import_opts = quiver_csv_import_options_default();
    ASSERT_EQ(quiver_database_import_csv(db2, "Items", "", csv_path.string().c_str(), &import_opts), QUIVER_OK);

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

    double vec_values[] = {1.1, 2.2, 3.3};
    ASSERT_EQ(quiver_database_update_vector_floats(db, "Items", "measurement", id1, vec_values, 3), QUIVER_OK);

    // Export
    auto csv_path = temp_csv("ImportVectorRT");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "measurements", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    // Clear and re-import
    ASSERT_EQ(quiver_database_update_vector_floats(db, "Items", "measurement", id1, nullptr, 0), QUIVER_OK);

    auto import_opts = quiver_csv_import_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "measurements", csv_path.string().c_str(), &import_opts),
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

    const char* set_values[] = {"red", "green", "blue"};
    ASSERT_EQ(quiver_database_update_set_strings(db, "Items", "tag", id1, set_values, 3), QUIVER_OK);

    // Export
    auto csv_path = temp_csv("ImportSetRT");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "tags", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    // Clear and re-import
    ASSERT_EQ(quiver_database_update_set_strings(db, "Items", "tag", id1, nullptr, 0), QUIVER_OK);

    auto import_opts = quiver_csv_import_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "tags", csv_path.string().c_str(), &import_opts), QUIVER_OK);

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
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "readings", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    // Clear and re-import
    ASSERT_EQ(quiver_database_update_time_series_group(db, "Items", "readings", id1, nullptr, nullptr, nullptr, 0, 0),
              QUIVER_OK);

    auto import_opts = quiver_csv_import_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "readings", csv_path.string().c_str(), &import_opts), QUIVER_OK);

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

    auto import_opts = quiver_csv_import_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_opts), QUIVER_OK);

    // Verify table is empty
    char** names = nullptr;
    size_t name_count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Items", "name", &names, &name_count), QUIVER_OK);
    EXPECT_EQ(name_count, 0u);
    quiver_database_free_string_array(names, name_count);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_OverwritesExistingFile) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("Overwrite");

    // Write initial content
    {
        std::ofstream f(csv_path, std::ios::binary);
        f << "old content that should be replaced\n";
    }

    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());

    // Old content gone
    EXPECT_EQ(content.find("old content"), std::string::npos);
    // New content present
    EXPECT_NE(content.find("Item1,Alpha"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}
