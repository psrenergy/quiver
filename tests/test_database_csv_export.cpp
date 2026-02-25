#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/options.h>
#include <sstream>

namespace fs = std::filesystem;

// ============================================================================
// export_csv helpers
// ============================================================================

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
// export_csv: export_csv routing (scalar, vector, set, time series, invalid)
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

    quiver::Element update1;
    update1.set("measurement", std::vector<double>{1.1, 2.2, 3.3});
    db.update_element("Items", id1, update1);

    quiver::Element update2;
    update2.set("measurement", std::vector<double>{4.4, 5.5});
    db.update_element("Items", id2, update2);

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

    quiver::Element update;
    update.set("tag", std::vector<std::string>{"red", "green", "blue"});
    db.update_element("Items", id1, update);

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
// export_csv: RFC 4180 compliance
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
// export_csv: Empty collection
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
// export_csv: NULL values
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
// export_csv: Default options (raw values)
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
// export_csv: Enum resolution
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
// export_csv: Date formatting
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

TEST(DatabaseCSV, ExportCSV_DateTimeFormat_InvalidDateReturnsRaw) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("Alpha"))
        .set("date_created", std::string("not-a-date"));  // invalid ISO 8601
    db.create_element("Items", e1);

    quiver::CSVOptions opts;
    opts.date_time_format = "%Y/%m/%d";

    auto csv_path = temp_csv("InvalidDateRaw");
    db.export_csv("Items", "", csv_path.string(), opts);

    auto content = read_file(csv_path.string());

    // Invalid datetime should be returned as-is (raw value)
    EXPECT_NE(content.find("not-a-date"), std::string::npos);

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
// export_csv: Default options factory
// ============================================================================

TEST(DatabaseCSV, ExportCSV_DefaultOptionsFactory) {
    auto opts = quiver::default_csv_options();
    EXPECT_TRUE(opts.enum_labels.empty());
    EXPECT_TRUE(opts.date_time_format.empty());
}

// ============================================================================
// export_csv: parent directory creation and overwrite behavior
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

TEST(DatabaseCSV, ExportCSV_CannotOpenFile_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    // Use a path that exists as a directory, so opening it as a file fails
    auto dir_path = fs::temp_directory_path() / "quiver_test_dir_not_file";
    fs::create_directories(dir_path);

    EXPECT_THROW(
        {
            try {
                db.export_csv("Items", "", dir_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Failed to export_csv: could not open file"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove_all(dir_path);
}
