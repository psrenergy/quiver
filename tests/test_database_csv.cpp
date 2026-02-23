#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/options.h>
#include <sstream>

namespace fs = std::filesystem;

// Helper: read file contents as a string (binary mode to preserve LF)
static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Helper: create a database from the csv_export schema
static quiver::Database make_db() {
    return quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("csv_export.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
}

// Helper: get a unique temp path for a test
static fs::path temp_csv(const std::string& test_name) {
    return fs::temp_directory_path() / ("quiver_test_" + test_name + ".csv");
}

// ============================================================================
// CSV-01: export_csv routing (scalar, vector, set, time series, invalid)
// ============================================================================

TEST(DatabaseCSV, ExportCSV_ScalarExport_HeaderAndData) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("Alpha"))
        .set("status", int64_t{1})
        .set("price", 9.99)
        .set("date_created", std::string("2024-01-15T10:30:00"))
        .set("notes", std::string("first"));
    db.create_element("Items", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item2"))
        .set("name", std::string("Beta"))
        .set("status", int64_t{2})
        .set("price", 19.5)
        .set("date_created", std::string("2024-02-20T08:00:00"))
        .set("notes", std::string("second"));
    db.create_element("Items", e2);

    auto csv_path = temp_csv("ScalarExport");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // Header: schema order columns minus id
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);

    // Data rows
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n"), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_VectorGroupExport) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item2")).set("name", std::string("Beta"));
    auto id2 = db.create_element("Items", e2);

    db.update_vector_floats("Items", "measurement", id1, {1.1, 2.2, 3.3});
    db.update_vector_floats("Items", "measurement", id2, {4.4, 5.5});

    auto csv_path = temp_csv("VectorExport");
    db.export_csv("Items", "measurements", csv_path.string());

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
}

TEST(DatabaseCSV, ExportCSV_SetGroupExport) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    db.update_set_strings("Items", "tag", id1, {"red", "green", "blue"});

    auto csv_path = temp_csv("SetExport");
    db.export_csv("Items", "tags", csv_path.string());

    auto content = read_file(csv_path.string());

    // Header: id + tag
    EXPECT_NE(content.find("sep=,\nid,tag\n"), std::string::npos);

    // Data rows
    EXPECT_NE(content.find("Item1,red\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,green\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,blue\n"), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_TimeSeriesGroupExport) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"temperature", 22.5}, {"humidity", int64_t{60}}},
        {{"date_time", std::string("2024-01-01T11:00:00")}, {"temperature", 23.0}, {"humidity", int64_t{55}}}};
    db.update_time_series_group("Items", "readings", id1, rows);

    auto csv_path = temp_csv("TimeSeriesExport");
    db.export_csv("Items", "readings", csv_path.string());

    auto content = read_file(csv_path.string());

    // Header: id + dimension + value columns
    EXPECT_NE(content.find("sep=,\nid,date_time,temperature,humidity\n"), std::string::npos);

    // Data rows ordered by date_time
    EXPECT_NE(content.find("Item1,2024-01-01T10:00:00,22.5,60\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,2024-01-01T11:00:00,23,55\n"), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_InvalidGroup_Throws) {
    auto db = make_db();

    auto csv_path = temp_csv("InvalidGroup");
    EXPECT_THROW(
        {
            try {
                db.export_csv("Items", "nonexistent", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Cannot export_csv: group not found"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// CSV-02: RFC 4180 compliance
// ============================================================================

TEST(DatabaseCSV, ExportCSV_RFC4180_CommaEscaping) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha, Beta")).set("status", int64_t{1});
    db.create_element("Items", e1);

    auto csv_path = temp_csv("CommaEscaping");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // Field with comma must be wrapped in double quotes
    EXPECT_NE(content.find("\"Alpha, Beta\""), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_RFC4180_QuoteEscaping) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("He said \"hello\"")).set("status", int64_t{1});
    db.create_element("Items", e1);

    auto csv_path = temp_csv("QuoteEscaping");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // Field with quotes: wrapped and quotes doubled
    EXPECT_NE(content.find("\"He said \"\"hello\"\"\""), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_RFC4180_NewlineEscaping) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("line1\nline2")).set("status", int64_t{1});
    db.create_element("Items", e1);

    auto csv_path = temp_csv("NewlineEscaping");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // Field with newline must be wrapped in double quotes
    EXPECT_NE(content.find("\"line1\nline2\""), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_LFLineEndings) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha")).set("status", int64_t{1});
    db.create_element("Items", e1);

    auto csv_path = temp_csv("LFLineEndings");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // No CRLF should be present (only LF)
    EXPECT_EQ(content.find("\r\n"), std::string::npos);
    // But LF should be present
    EXPECT_NE(content.find("\n"), std::string::npos);

    fs::remove(csv_path);
}

// ============================================================================
// CSV-03: Empty collection
// ============================================================================

TEST(DatabaseCSV, ExportCSV_EmptyCollection_HeaderOnly) {
    auto db = make_db();

    auto csv_path = temp_csv("EmptyCollection");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // Header row only, followed by LF
    EXPECT_EQ(content, "sep=,\nlabel,name,status,price,date_created,notes\n");

    fs::remove(csv_path);
}

// ============================================================================
// CSV-04: NULL values
// ============================================================================

TEST(DatabaseCSV, ExportCSV_NullValues_EmptyFields) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    // status, price, date_created, notes all left NULL
    db.create_element("Items", e1);

    auto csv_path = temp_csv("NullValues");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // NULL fields appear as empty (just commas)
    // Expected: Item1,Alpha,,,,
    EXPECT_NE(content.find("Item1,Alpha,,,,\n"), std::string::npos);

    fs::remove(csv_path);
}

// ============================================================================
// OPT-01: Default options (raw values)
// ============================================================================

TEST(DatabaseCSV, ExportCSV_DefaultOptions_RawValues) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("Alpha"))
        .set("status", int64_t{1})
        .set("price", 9.99)
        .set("date_created", std::string("2024-01-15T10:30:00"))
        .set("notes", std::string("note"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("DefaultOptions");
    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // With default options, integer enum columns have raw integers
    EXPECT_NE(content.find(",1,"), std::string::npos);
    // DateTime columns have raw strings
    EXPECT_NE(content.find("2024-01-15T10:30:00"), std::string::npos);

    fs::remove(csv_path);
}

// ============================================================================
// OPT-02: Enum resolution
// ============================================================================

TEST(DatabaseCSV, ExportCSV_EnumLabels_ReplacesIntegers) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha")).set("status", int64_t{1});
    db.create_element("Items", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item2")).set("name", std::string("Beta")).set("status", int64_t{2});
    db.create_element("Items", e2);

    quiver::CSVOptions opts;
    opts.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};

    auto csv_path = temp_csv("EnumReplace");
    db.export_csv("Items", "", csv_path.string(), opts);

    auto content = read_file(csv_path.string());

    // status column should have labels instead of integers
    EXPECT_NE(content.find("Item1,Alpha,Active,"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,Inactive,"), std::string::npos);

    // Raw integers 1 and 2 should NOT be present as status values
    EXPECT_EQ(content.find("Item1,Alpha,1,"), std::string::npos);
    EXPECT_EQ(content.find("Item2,Beta,2,"), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_EnumLabels_UnmappedFallback) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha")).set("status", int64_t{1});
    db.create_element("Items", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item2")).set("name", std::string("Beta")).set("status", int64_t{3});
    db.create_element("Items", e2);

    quiver::CSVOptions opts;
    opts.enum_labels["status"]["en"] = {{"Active", 1}};  // only map value 1

    auto csv_path = temp_csv("EnumFallback");
    db.export_csv("Items", "", csv_path.string(), opts);

    auto content = read_file(csv_path.string());

    // Mapped value replaced
    EXPECT_NE(content.find("Item1,Alpha,Active,"), std::string::npos);
    // Unmapped value falls back to raw integer string
    EXPECT_NE(content.find("Item2,Beta,3,"), std::string::npos);

    fs::remove(csv_path);
}

// ============================================================================
// OPT-03: Date formatting
// ============================================================================

TEST(DatabaseCSV, ExportCSV_DateTimeFormat_FormatsDateColumns) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("Alpha"))
        .set("status", int64_t{1})
        .set("date_created", std::string("2024-01-15T10:30:00"));
    db.create_element("Items", e1);

    quiver::CSVOptions opts;
    opts.date_time_format = "%Y/%m/%d";

    auto csv_path = temp_csv("DateFormat");
    db.export_csv("Items", "", csv_path.string(), opts);

    auto content = read_file(csv_path.string());

    // date_created column should be formatted
    EXPECT_NE(content.find("2024/01/15"), std::string::npos);
    // Raw ISO format should NOT appear
    EXPECT_EQ(content.find("2024-01-15T10:30:00"), std::string::npos);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ExportCSV_DateTimeFormat_NonDateColumnsUnaffected) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("2024-01-15T10:30:00"))  // looks like a date but column is not date_*
        .set("status", int64_t{1})
        .set("date_created", std::string("2024-01-15T10:30:00"))
        .set("notes", std::string("2024-01-15T10:30:00"));  // also not a date column
    db.create_element("Items", e1);

    quiver::CSVOptions opts;
    opts.date_time_format = "%Y/%m/%d";

    auto csv_path = temp_csv("NonDateUnaffected");
    db.export_csv("Items", "", csv_path.string(), opts);

    auto content = read_file(csv_path.string());

    // date_created column formatted
    EXPECT_NE(content.find("2024/01/15"), std::string::npos);

    // name and notes columns should still have raw ISO string
    // The content line should have: Item1,2024-01-15T10:30:00,...,2024/01/15,2024-01-15T10:30:00
    // Count occurrences of the raw ISO string (should be 2: name and notes)
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("2024-01-15T10:30:00", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    EXPECT_EQ(count, 2);  // name and notes columns unformatted

    fs::remove(csv_path);
}

// ============================================================================
// OPT-04: Default options factory
// ============================================================================

TEST(DatabaseCSV, ExportCSV_DefaultOptionsFactory) {
    auto opts = quiver::default_csv_options();
    EXPECT_TRUE(opts.enum_labels.empty());
    EXPECT_TRUE(opts.date_time_format.empty());
}

// ============================================================================
// Additional: parent directory creation and overwrite behavior
// ============================================================================

TEST(DatabaseCSV, ExportCSV_CreatesParentDirectories) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = fs::temp_directory_path() / "quiver_test_nested" / "subdir" / "output.csv";
    // Ensure parent does not exist
    fs::remove_all(fs::temp_directory_path() / "quiver_test_nested");

    db.export_csv("Items", "", csv_path.string());

    EXPECT_TRUE(fs::exists(csv_path));
    auto content = read_file(csv_path.string());
    EXPECT_NE(content.find("Item1"), std::string::npos);

    // Cleanup
    fs::remove_all(fs::temp_directory_path() / "quiver_test_nested");
}

TEST(DatabaseCSV, ExportCSV_OverwritesExistingFile) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("Overwrite");

    // Write initial content
    {
        std::ofstream f(csv_path, std::ios::binary);
        f << "old content that should be replaced\n";
    }

    db.export_csv("Items", "", csv_path.string());

    auto content = read_file(csv_path.string());

    // Old content gone
    EXPECT_EQ(content.find("old content"), std::string::npos);
    // New content present
    EXPECT_NE(content.find("Item1,Alpha"), std::string::npos);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv helpers
// ============================================================================

// Write a string to a temp CSV file (binary mode to preserve LF)
static void write_csv_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

// Helper: create a database from the relations schema (has FK)
static quiver::Database make_relations_db() {
    return quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
}

// ============================================================================
// import_csv: Happy path tests
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Scalar_RoundTrip) {
    auto db = make_db();

    // Create elements
    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("Alpha"))
        .set("status", int64_t{1})
        .set("price", 9.99)
        .set("date_created", std::string("2024-01-15T10:30:00"))
        .set("notes", std::string("first"));
    db.create_element("Items", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item2"))
        .set("name", std::string("Beta"))
        .set("status", int64_t{2})
        .set("price", 19.5)
        .set("date_created", std::string("2024-02-20T08:00:00"))
        .set("notes", std::string("second"));
    db.create_element("Items", e2);

    // Export
    auto csv_path = temp_csv("ImportScalarRT");
    db.export_csv("Items", "", csv_path.string());

    // Import into fresh DB
    auto db2 = make_db();
    db2.import_csv("Items", "", csv_path.string());

    auto names = db2.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "Alpha");
    EXPECT_EQ(names[1], "Beta");

    auto price1 = db2.read_scalar_float_by_id("Items", "price", 1);
    auto price2 = db2.read_scalar_float_by_id("Items", "price", 2);
    ASSERT_TRUE(price1.has_value());
    ASSERT_TRUE(price2.has_value());
    EXPECT_NEAR(price1.value(), 9.99, 0.001);
    EXPECT_NEAR(price2.value(), 19.5, 0.001);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_WithNulls) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportScalarNulls");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,,\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "Alpha");

    // Nullable columns should be null
    auto status = db.read_scalar_integer_by_id("Items", "status", 1);
    EXPECT_FALSE(status.has_value());

    auto price = db.read_scalar_float_by_id("Items", "price", 1);
    EXPECT_FALSE(price.has_value());

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_EnumResolution) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportScalarEnum");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Active,,,\n");

    quiver::CSVOptions opts;
    opts.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};

    db.import_csv("Items", "", csv_path.string(), opts);

    auto status = db.read_scalar_integer_by_id("Items", "status", 1);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status.value(), 1);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_EnumCaseInsensitive) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportScalarEnumCase");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\n"
                   "Item1,Alpha,ACTIVE,,,\n"
                   "Item2,Beta,active,,,\n"
                   "Item3,Gamma,Active,,,\n");

    quiver::CSVOptions opts;
    opts.enum_labels["status"]["en"] = {{"Active", 1}};

    db.import_csv("Items", "", csv_path.string(), opts);

    auto statuses = db.read_scalar_integers("Items", "status");
    ASSERT_EQ(statuses.size(), 3u);
    for (const auto& s : statuses) {
        EXPECT_EQ(s, 1);
    }

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_EnumMultiLanguage) {
    auto db = make_db();

    // CSV uses Portuguese labels for status
    auto csv_path = temp_csv("ImportScalarEnumMultiLang");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\n"
                   "Item1,Alpha,Ativo,,,\n"
                   "Item2,Beta,Inactive,,,\n"
                   "Item3,Gamma,Inativo,,,\n");

    quiver::CSVOptions opts;
    opts.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};
    opts.enum_labels["status"]["pt"] = {{"Ativo", 1}, {"Inativo", 2}};

    db.import_csv("Items", "", csv_path.string(), opts);

    auto statuses = db.read_scalar_integers("Items", "status");
    ASSERT_EQ(statuses.size(), 3u);
    EXPECT_EQ(statuses[0], 1);  // Ativo -> 1 (pt)
    EXPECT_EQ(statuses[1], 2);  // Inactive -> 2 (en)
    EXPECT_EQ(statuses[2], 2);  // Inativo -> 2 (pt)

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_DateTimeFormat) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportScalarDateTime");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,2024/01/15,\n");

    quiver::CSVOptions opts;
    opts.date_time_format = "%Y/%m/%d";

    db.import_csv("Items", "", csv_path.string(), opts);

    auto date = db.read_scalar_string_by_id("Items", "date_created", 1);
    ASSERT_TRUE(date.has_value());
    EXPECT_EQ(date.value(), "2024-01-15T00:00:00");

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_HeaderOnly_ClearsTable) {
    auto db = make_db();

    // Populate DB
    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    // Import header-only CSV
    auto csv_path = temp_csv("ImportScalarHeaderOnly");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    EXPECT_TRUE(names.empty());

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_Whitespace_Trimmed) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportScalarTrim");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\n Item1 , Alpha ,,,, note \n");

    db.import_csv("Items", "", csv_path.string());

    auto labels = db.read_scalar_strings("Items", "label");
    ASSERT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item1");

    auto names = db.read_scalar_strings("Items", "name");
    EXPECT_EQ(names[0], "Alpha");

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Vector_RoundTrip) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    db.update_vector_floats("Items", "measurement", id1, {1.1, 2.2, 3.3});

    // Export
    auto csv_path = temp_csv("ImportVectorRT");
    db.export_csv("Items", "measurements", csv_path.string());

    // Clear and re-import
    db.update_vector_floats("Items", "measurement", id1, {});
    db.import_csv("Items", "measurements", csv_path.string());

    auto vals = db.read_vector_floats_by_id("Items", "measurement", id1);
    ASSERT_EQ(vals.size(), 3);
    EXPECT_NEAR(vals[0], 1.1, 0.001);
    EXPECT_NEAR(vals[1], 2.2, 0.001);
    EXPECT_NEAR(vals[2], 3.3, 0.001);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Set_RoundTrip) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    db.update_set_strings("Items", "tag", id1, {"red", "green", "blue"});

    // Export
    auto csv_path = temp_csv("ImportSetRT");
    db.export_csv("Items", "tags", csv_path.string());

    // Clear and re-import
    db.update_set_strings("Items", "tag", id1, {});
    db.import_csv("Items", "tags", csv_path.string());

    auto tags = db.read_set_strings_by_id("Items", "tag", id1);
    ASSERT_EQ(tags.size(), 3);

    std::set<std::string> tag_set(tags.begin(), tags.end());
    EXPECT_TRUE(tag_set.count("red"));
    EXPECT_TRUE(tag_set.count("green"));
    EXPECT_TRUE(tag_set.count("blue"));

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_TimeSeries_RoundTrip) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"temperature", 22.5}, {"humidity", int64_t{60}}},
        {{"date_time", std::string("2024-01-01T11:00:00")}, {"temperature", 23.0}, {"humidity", int64_t{55}}}};
    db.update_time_series_group("Items", "readings", id1, rows);

    // Export
    auto csv_path = temp_csv("ImportTSRT");
    db.export_csv("Items", "readings", csv_path.string());

    // Clear and re-import
    db.update_time_series_group("Items", "readings", id1, {});
    db.import_csv("Items", "readings", csv_path.string());

    auto ts_rows = db.read_time_series_group("Items", "readings", id1);
    ASSERT_EQ(ts_rows.size(), 2);
    EXPECT_EQ(std::get<std::string>(ts_rows[0].at("date_time")), "2024-01-01T10:00:00");
    EXPECT_NEAR(std::get<double>(ts_rows[0].at("temperature")), 22.5, 0.001);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Group_HeaderOnly_ClearsGroup) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    db.update_set_strings("Items", "tag", id1, {"red", "green"});

    // Import header-only CSV
    auto csv_path = temp_csv("ImportGroupHeaderOnly");
    write_csv_file(csv_path.string(), "sep=,\nid,tag\n");

    db.import_csv("Items", "tags", csv_path.string());

    auto tags = db.read_set_strings_by_id("Items", "tag", id1);
    EXPECT_TRUE(tags.empty());

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Validation error tests
// ============================================================================

TEST(DatabaseCSV, ImportCSV_EmptyFile_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportEmpty");
    write_csv_file(csv_path.string(), "");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("CSV file is empty"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_MissingLabelColumn_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportMissingLabel");
    write_csv_file(csv_path.string(), "sep=,\nname,status,price,date_created,notes,extra\nAlpha,1,9.99,,,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("does not contain a 'label' column"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_ColumnCountMismatch_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportColCount");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name\nItem1,Alpha\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("number of columns in the CSV file does not match"),
                          std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_ColumnNameMismatch_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportColName");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,wrong\nItem1,Alpha,1,9.99,,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("columns in the CSV file do not match"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_NotNull_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportNotNull");
    // 'name' is NOT NULL in the schema
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,,,,, \n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Column name cannot be NULL"), std::string::npos);
                // Should NOT contain row number or quotes around column name
                EXPECT_EQ(msg.find("(row"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_InvalidEnum_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportBadEnum");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,BadValue,,,\n");

    // No enum_labels provided, so non-integer value triggers error
    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Invalid integer value"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_InvalidEnumWithMapping_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportBadEnumMap");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Unknown,,,\n");

    quiver::CSVOptions opts;
    opts.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string(), opts);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Invalid enum value"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_BadDateTime_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportBadDateTime");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,2020-02,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Timestamp 2020-02 is not valid"), std::string::npos);
                EXPECT_NE(msg.find("format %Y-%m-%dT%H:%M:%S"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_DuplicateEntries_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportDuplicates");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\n"
                   "Item1,Alpha,,,, \n"
                   "Item1,Beta,,,, \n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("duplicate entries"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_FKNotFound_Throws) {
    auto db = make_relations_db();

    // Create a Parent element
    quiver::Element p1;
    p1.set("label", std::string("Parent1"));
    db.create_element("Parent", p1);

    auto csv_path = temp_csv("ImportFKNotFound");
    write_csv_file(csv_path.string(), "sep=,\nlabel,parent_id,sibling_id\nChild1,NonExistent,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Child", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Could not find an existing element from collection Parent with label NonExistent"),
                          std::string::npos);
                EXPECT_NE(msg.find("Create the element before referencing it"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Group_InvalidGroup_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportBadGroup");
    write_csv_file(csv_path.string(), "sep=,\nid,value\nItem1,42\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "nonexistent", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("group not found"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Group_IdNotInCollection_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportGroupBadId");
    write_csv_file(csv_path.string(), "sep=,\nid,tag\nNonExistent,red\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "tags", csv_path.string());
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Element with id NonExistent does not exist in collection Items"),
                          std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Vector_BadVectorIndex_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportBadVectorIndex");
    // vector_index starts at 0 instead of 1
    write_csv_file(csv_path.string(), "sep=,\nid,vector_index,measurement\nItem1,0,1.1\nItem1,1,2.2\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "measurements", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("vector_index must be consecutive"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_TimeSeries_DateTimeParsing) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportTSDateTime");
    write_csv_file(csv_path.string(), "sep=,\nid,date_time,temperature,humidity\nItem1,2024/01/15,22.5,60\n");

    quiver::CSVOptions opts;
    opts.date_time_format = "%Y/%m/%d";

    db.import_csv("Items", "readings", csv_path.string(), opts);

    auto ts_rows = db.read_time_series_group("Items", "readings", id1);
    ASSERT_EQ(ts_rows.size(), 1);
    EXPECT_EQ(std::get<std::string>(ts_rows[0].at("date_time")), "2024-01-15T00:00:00");

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_InvalidFloatValue_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportBadFloat");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,not_a_number,,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Invalid float value"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: FK-specific tests (relations.sql schema)
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Scalar_SelfReferenceFK_RoundTrip) {
    auto db = make_relations_db();

    // Create parent (needed for FK)
    quiver::Element p1;
    p1.set("label", std::string("Parent1"));
    db.create_element("Parent", p1);

    // Import children via CSV with label-based FK references
    auto csv_path = temp_csv("ImportSelfFK");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,parent_id,sibling_id\n"
                   "Child1,Parent1,\n"
                   "Child2,Parent1,Child1\n");

    db.import_csv("Child", "", csv_path.string());

    auto labels = db.read_scalar_strings("Child", "label");
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels[0], "Child1");
    EXPECT_EQ(labels[1], "Child2");

    // Verify self-FK was resolved (Child2.sibling_id -> Child1.id)
    auto sibling = db.read_scalar_integer_by_id("Child", "sibling_id", 2);
    ASSERT_TRUE(sibling.has_value());
    EXPECT_EQ(sibling.value(), 1);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_CrossCollectionFK_RoundTrip) {
    auto db = make_relations_db();

    // Create parent (needed for FK)
    quiver::Element p1;
    p1.set("label", std::string("Parent1"));
    db.create_element("Parent", p1);

    // Import child via CSV with label-based FK reference
    auto csv_path = temp_csv("ImportCrossFK");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,parent_id,sibling_id\n"
                   "Child1,Parent1,\n");

    db.import_csv("Child", "", csv_path.string());

    auto parent_id = db.read_scalar_integer_by_id("Child", "parent_id", 1);
    ASSERT_TRUE(parent_id.has_value());
    EXPECT_EQ(parent_id.value(), 1);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_20000Rows) {
    auto db = make_db();
    auto csv_path = temp_csv("Import20000Rows");

    // Generate a 20000-row CSV file
    std::ofstream out(csv_path, std::ios::binary);
    out << "sep=,\nlabel,name,status,price,date_created,notes\n";
    for (int i = 1; i <= 20000; ++i) {
        out << "Item" << i << "," << "Name" << i << "," << (i % 3) << "," << (i * 0.5) << "," << "2024-01-15T10:30:00,"
            << "note" << i << "\n";
    }
    out.close();

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 20000);
    EXPECT_EQ(names[0], "Name1");
    EXPECT_EQ(names[19999], "Name20000");

    auto prices = db.read_scalar_floats("Items", "price");
    ASSERT_EQ(prices.size(), 20000);
    EXPECT_NEAR(prices[0], 0.5, 0.001);
    EXPECT_NEAR(prices[19999], 10000.0, 0.001);

    fs::remove(csv_path);
}
