#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

TEST(Database, ExportToCsvBasicCollection) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // Create elements
    quiver::Element e1;
    e1.set("label", std::string("Config 1"))
        .set("integer_attribute", int64_t{42})
        .set("float_attribute", 3.14)
        .set("boolean_attribute", int64_t{1});
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2"))
        .set("integer_attribute", int64_t{100})
        .set("float_attribute", 2.71)
        .set("date_attribute", std::string("2024-01-01"));
    db.create_element("Configuration", e2);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_basic.csv").string();
    db.export_csv("Configuration", csv_path);

    // Read CSV back
    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 data rows

    // Line 0: sep=,
    EXPECT_EQ(lines[0], "sep=,");

    // Line 1: header should not contain id (table has label column)
    EXPECT_EQ(lines[1].find("id"), std::string::npos);
    EXPECT_NE(lines[1].find("label"), std::string::npos);
    EXPECT_NE(lines[1].find("integer_attribute"), std::string::npos);
    EXPECT_NE(lines[1].find("float_attribute"), std::string::npos);
    EXPECT_NE(lines[1].find("boolean_attribute"), std::string::npos);
    EXPECT_NE(lines[1].find("date_attribute"), std::string::npos);

    // Line 2: Config 1 data
    EXPECT_NE(lines[2].find("Config 1"), std::string::npos);
    EXPECT_NE(lines[2].find("42"), std::string::npos);
    EXPECT_NE(lines[2].find("3.14"), std::string::npos);
    EXPECT_NE(lines[2].find("1"), std::string::npos);  // boolean_attribute

    // Line 3: Config 2 data
    EXPECT_NE(lines[3].find("Config 2"), std::string::npos);
    EXPECT_NE(lines[3].find("100"), std::string::npos);
    EXPECT_NE(lines[3].find("2.71"), std::string::npos);
    EXPECT_NE(lines[3].find("2024-01-01"), std::string::npos);
}

TEST(Database, ExportToCsvEmptyCollection) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = "quiver_export_empty.csv";
    db.export_csv("Configuration", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 2);  // sep + header only, no data rows

    // Line 0: sep=,
    EXPECT_EQ(lines[0], "sep=,");

    // Line 1: header with all columns except id
    EXPECT_EQ(lines[1].find("id"), std::string::npos);
    EXPECT_NE(lines[1].find("label"), std::string::npos);
    EXPECT_NE(lines[1].find("integer_attribute"), std::string::npos);
    EXPECT_NE(lines[1].find("float_attribute"), std::string::npos);
}

TEST(Database, ExportToCsvResolveForeignKeys) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element p1;
    p1.set("label", std::string("Parent 1"));
    db.create_element("Parent", p1);

    quiver::Element p2;
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p2);

    quiver::Element c1;
    c1.set("label", std::string("Child 1"));
    db.create_element("Child", c1);

    quiver::Element c2;
    c2.set("label", std::string("Child 2"));
    db.create_element("Child", c2);

    db.update_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");
    db.update_scalar_relation("Child", "parent_id", "Child 2", "Parent 2");
    db.update_scalar_relation("Child", "sibling_id", "Child 2", "Child 1");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_fk.csv").string();
    db.export_csv("Child", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 rows

    // Header keeps original FK column names
    EXPECT_NE(lines[1].find("parent_id"), std::string::npos);
    EXPECT_NE(lines[1].find("sibling_id"), std::string::npos);

    // Row data should contain resolved labels, not integer IDs
    EXPECT_NE(lines[2].find("Child 1"), std::string::npos);
    EXPECT_NE(lines[2].find("Parent 1"), std::string::npos);

    EXPECT_NE(lines[3].find("Child 2"), std::string::npos);
    EXPECT_NE(lines[3].find("Parent 2"), std::string::npos);
    EXPECT_NE(lines[3].find("Child 1"), std::string::npos);  // sibling resolved
}

TEST(Database, ExportToCsvNullForeignKey) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element c1;
    c1.set("label", std::string("Orphan"));
    db.create_element("Child", c1);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_fk_null.csv").string();
    db.export_csv("Child", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 row
    EXPECT_NE(lines[2].find("Orphan"), std::string::npos);
}

TEST(Database, ExportToCsvVectorTableResolveForeignKeys) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element p1;
    p1.set("label", std::string("Parent 1"));
    db.create_element("Parent", p1);

    quiver::Element c1;
    c1.set("label", std::string("Child 1"));
    db.create_element("Child", c1);

    db.update_vector_integers("Child", "parent_ref", 1, {1, 1});

    auto csv_path = (fs::temp_directory_path() / "quiver_export_vector_fk.csv").string();
    db.export_csv("Child_vector_refs", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 rows

    // FK columns should resolve to labels
    EXPECT_NE(lines[2].find("Child 1"), std::string::npos);
    EXPECT_NE(lines[2].find("Parent 1"), std::string::npos);
}

TEST(Database, ExportToCsvSetTableResolveForeignKeys) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element p1;
    p1.set("label", std::string("Parent 1"));
    db.create_element("Parent", p1);

    quiver::Element p2;
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p2);

    quiver::Element c1;
    c1.set("label", std::string("Child 1"));
    db.create_element("Child", c1);

    db.update_set_integers("Child", "parent_ref", 1, {1, 2});

    auto csv_path = (fs::temp_directory_path() / "quiver_export_set_fk.csv").string();
    db.export_csv("Child_set_parents", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 rows

    // FK columns should resolve to labels
    EXPECT_NE(lines[2].find("Child 1"), std::string::npos);
    EXPECT_NE(lines[2].find("Parent 1"), std::string::npos);
    EXPECT_NE(lines[3].find("Child 1"), std::string::npos);
    EXPECT_NE(lines[3].find("Parent 2"), std::string::npos);
}

TEST(Database, ExportToCsvVectorIndexAsNormalValue) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    db.update_vector_integers("Collection", "value_int", 1, {10, 20, 30});

    auto csv_path = (fs::temp_directory_path() / "quiver_export_vecidx.csv").string();
    db.export_csv("Collection_vector_values", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 5);  // sep + header + 3 rows

    // Header should contain vector_index
    EXPECT_NE(lines[1].find("vector_index"), std::string::npos);

    // vector_index values should be integers 1, 2, 3
    EXPECT_NE(lines[2].find("1"), std::string::npos);
    EXPECT_NE(lines[3].find("2"), std::string::npos);
    EXPECT_NE(lines[4].find("3"), std::string::npos);

    // id column should resolve to parent label
    EXPECT_NE(lines[2].find("Item 1"), std::string::npos);
}

TEST(Database, ExportToCsvQuotesFieldsWithCommas) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item, with comma"));
    db.create_element("Configuration", e1);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_quoting.csv").string();
    db.export_csv("Configuration", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 row

    // Field with comma should be quoted per RFC 4180
    EXPECT_NE(lines[2].find("\"Item, with comma\""), std::string::npos);
}

TEST(Database, ExportToCsvTimeSeriesTable) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    // Insert time series data directly
    db.query_string("INSERT INTO Collection_time_series_data VALUES (1, '2024-01-15T10:30:00', 42.5)");
    db.query_string("INSERT INTO Collection_time_series_data VALUES (1, '2024-01-15T11:00:00', 37.2)");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_ts.csv").string();
    db.export_csv("Collection_time_series_data", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 rows

    // collection_id FK should resolve to parent label
    EXPECT_NE(lines[2].find("Item 1"), std::string::npos);

    // date_time should be exported (default format = ISO 8601)
    EXPECT_NE(lines[2].find("2024-01-15T10:30:00"), std::string::npos);
    EXPECT_NE(lines[3].find("2024-01-15T11:00:00"), std::string::npos);

    // value should be exported as-is
    EXPECT_NE(lines[2].find("42.5"), std::string::npos);
}

TEST(Database, ExportToCsvDateTimeCustomFormat) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    db.query_string("INSERT INTO Collection_time_series_data VALUES (1, '2024-01-15T10:30:00', 42.5)");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_datefmt.csv").string();
    db.export_csv("Collection_time_series_data", csv_path, {{"date_time", "%Y-%m"}});

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 row
    EXPECT_NE(lines[2].find("2024-01"), std::string::npos);
    EXPECT_EQ(lines[2].find("10:30"), std::string::npos);  // time should be gone
}

TEST(Database, ExportToCsvEnumResolution) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{1});
    db.create_element("Configuration", e1);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_enum.csv").string();
    quiver::EnumMap enums(quiver::EnumMap::Data{{"integer_attribute", {{"en", {{"Active", 1}, {"Inactive", 2}}}}}});
    db.export_csv("Configuration", csv_path, {}, enums);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 row
    EXPECT_NE(lines[2].find("Active"), std::string::npos);
    EXPECT_EQ(lines[2].find(",1,"), std::string::npos);  // raw integer should not appear
}

TEST(Database, ExportToCsvEnumInvalidIdThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{99});
    db.create_element("Configuration", e1);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_enum_bad.csv").string();
    quiver::EnumMap enums(quiver::EnumMap::Data{{"integer_attribute", {{"en", {{"Active", 1}}}}}});
    EXPECT_THROW(db.export_csv("Configuration", csv_path, {}, enums), std::runtime_error);
    fs::remove(csv_path);
}

TEST(Database, ExportToCsvEnumMultiLocale) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{1});
    db.create_element("Configuration", e1);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_enum_locale.csv").string();
    quiver::EnumMap enums(quiver::EnumMap::Data{
        {"integer_attribute", {{"en", {{"Active", 1}, {"Inactive", 2}}}, {"pt", {{"Ativo", 1}, {"Inativo", 2}}}}}});
    db.export_csv("Configuration", csv_path, {}, enums);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 row
    // First locale ("en") should be used for export
    EXPECT_NE(lines[2].find("Active"), std::string::npos);
    EXPECT_EQ(lines[2].find("Ativo"), std::string::npos);
}

TEST(Database, ExportToCsvTrimsWhitespace) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // Insert data with leading/trailing spaces directly via SQL
    db.query_string("INSERT INTO Configuration (label, integer_attribute, float_attribute, date_attribute) "
                    "VALUES ('  Config 1  ', 42, 3.14, '  2024-01-15T10:30:00  ')");

    db.query_string("INSERT INTO Configuration (label, integer_attribute, float_attribute, date_attribute) "
                    "VALUES ('Config 2   ', 100, 2.71, '   2024-06-01T00:00:00')");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_trim.csv").string();
    db.export_csv("Configuration", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 data rows

    // Labels should be trimmed
    EXPECT_NE(lines[2].find("Config 1"), std::string::npos);
    EXPECT_EQ(lines[2].find("  Config 1"), std::string::npos);  // no leading spaces
    EXPECT_EQ(lines[2].find("Config 1  "), std::string::npos);  // no trailing spaces

    EXPECT_NE(lines[3].find("Config 2"), std::string::npos);
    EXPECT_EQ(lines[3].find("Config 2   "), std::string::npos);  // no trailing spaces
}

TEST(Database, ExportToCsvTrimsWhitespaceForeignKeys) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    // Insert parent with padded label
    db.query_string("INSERT INTO Parent (label) VALUES ('  Parent 1  ')");

    quiver::Element c1;
    c1.set("label", std::string("Child 1"));
    db.create_element("Child", c1);

    db.update_scalar_relation("Child", "parent_id", "Child 1", "  Parent 1  ");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_trim_fk.csv").string();
    db.export_csv("Child", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 row

    // FK resolved label should be trimmed
    EXPECT_NE(lines[2].find("Parent 1"), std::string::npos);
    EXPECT_EQ(lines[2].find("  Parent 1"), std::string::npos);  // no leading spaces
    EXPECT_EQ(lines[2].find("Parent 1  "), std::string::npos);  // no trailing spaces
}

TEST(Database, ExportToCsvNonExistentCollectionThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = "quiver_export_invalid.csv";
    EXPECT_THROW(db.export_csv("NonExistent", csv_path), std::runtime_error);
}

TEST(Database, ExportToCsvNullValues) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // Create element with NULL values
    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2"));  // Other attributes are NULL
    db.create_element("Configuration", e2);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_nulls.csv").string();
    db.export_csv("Configuration", csv_path);

    // Read CSV back
    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 data rows

    // Line 0: sep=,
    EXPECT_EQ(lines[0], "sep=,");

    // Line 1: header
    EXPECT_EQ(lines[1].find("id"), std::string::npos);  // No id when table has label
    EXPECT_NE(lines[1].find("label"), std::string::npos);
    EXPECT_NE(lines[1].find("integer_attribute"), std::string::npos);
    EXPECT_NE(lines[1].find("float_attribute"), std::string::npos);

    // Line 2: Config 1 with values
    EXPECT_NE(lines[2].find("Config 1"), std::string::npos);
    EXPECT_NE(lines[2].find("42"), std::string::npos);

    // Line 3: Config 2 with NULL values exported as empty strings
    EXPECT_NE(lines[3].find("Config 2"), std::string::npos);
    // NULL values should appear as empty fields (consecutive commas)
    // The default integer_attribute value is 6 (from schema), so we expect it
    EXPECT_NE(lines[3].find("6"), std::string::npos);  // Default value from schema
}

TEST(Database, ExportToCsvTimeSeriesFiles) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // Insert time series file references
    db.query_string(
        "INSERT INTO Collection_time_series_files (data_file, metadata_file) VALUES ('data.csv', 'meta.csv')");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_ts_files.csv").string();
    db.export_csv("Collection_time_series_files", csv_path);

    // Read CSV back
    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 data row

    // Line 0: sep=,
    EXPECT_EQ(lines[0], "sep=,");

    // Line 1: header should not contain id or label (singleton table, no pk)
    EXPECT_EQ(lines[1].find("id"), std::string::npos);
    EXPECT_EQ(lines[1].find("label"), std::string::npos);
    EXPECT_NE(lines[1].find("data_file"), std::string::npos);
    EXPECT_NE(lines[1].find("metadata_file"), std::string::npos);

    // Line 2: data row with file paths
    EXPECT_NE(lines[2].find("data.csv"), std::string::npos);
    EXPECT_NE(lines[2].find("meta.csv"), std::string::npos);
}

TEST(Database, ExportToCsvInvalidPathThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1"));
    db.create_element("Configuration", e1);

    // Try to write to an invalid/inaccessible path
    // On Windows, paths with invalid characters or non-existent directories should fail
    auto invalid_path = "Z:/nonexistent_drive/invalid_path/file.csv";
    EXPECT_THROW(db.export_csv("Configuration", invalid_path), std::runtime_error);
}