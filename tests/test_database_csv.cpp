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
    db.export_to_csv("Configuration", csv_path);

    // Read CSV back
    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    // Line 0: sep=,
    // Line 1: header
    // Line 2: row 1
    // Line 3: row 2
    ASSERT_EQ(lines.size(), 4);
    EXPECT_EQ(lines[0], "sep=,");

    // Header should not contain id (table has label column)
    EXPECT_EQ(lines[1].find("id"), std::string::npos);
    EXPECT_NE(lines[1].find("label"), std::string::npos);
    EXPECT_NE(lines[1].find("integer_attribute"), std::string::npos);
    EXPECT_NE(lines[1].find("float_attribute"), std::string::npos);

    // Data rows should contain values
    EXPECT_NE(lines[2].find("Config 1"), std::string::npos);
    EXPECT_NE(lines[2].find("42"), std::string::npos);
    EXPECT_NE(lines[3].find("Config 2"), std::string::npos);
    EXPECT_NE(lines[3].find("100"), std::string::npos);
}

TEST(Database, ExportToCsvEmptyCollection) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = "quiver_export_empty.csv";
    db.export_to_csv("Configuration", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    // sep line + header line only, no data rows
    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0], "sep=,");
    EXPECT_NE(lines[1].find("label"), std::string::npos);
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

    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");
    db.set_scalar_relation("Child", "parent_id", "Child 2", "Parent 2");
    db.set_scalar_relation("Child", "sibling_id", "Child 2", "Child 1");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_fk.csv").string();
    db.export_to_csv("Child", csv_path);

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
    db.export_to_csv("Child", csv_path);

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
    db.export_to_csv("Child_vector_refs", csv_path);

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
    db.export_to_csv("Child_set_parents", csv_path);

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
    db.export_to_csv("Collection_vector_values", csv_path);

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
    db.export_to_csv("Configuration", csv_path);

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
    db.export_to_csv("Collection_time_series_data", csv_path);

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
    db.export_to_csv("Collection_time_series_data", csv_path, {{"date_time", "%Y-%m"}});

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
    db.export_to_csv("Configuration", csv_path, {}, enums);

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
    EXPECT_THROW(db.export_to_csv("Configuration", csv_path, {}, enums), std::runtime_error);
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
    db.export_to_csv("Configuration", csv_path, {}, enums);

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

TEST(Database, ExportToCsvNonExistentCollectionThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = "quiver_export_invalid.csv";
    EXPECT_THROW(db.export_to_csv("NonExistent", csv_path), std::runtime_error);
}

// --- Import tests ---

static std::string write_temp_csv(const std::string& name, const std::string& content) {
    auto path = (fs::temp_directory_path() / name).string();
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

TEST(Database, ImportFromCsvBasicScalar) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path =
        write_temp_csv("quiver_import_basic.csv",
                       "sep=,\n"
                       "label,integer_attribute,float_attribute,string_attribute,date_attribute,boolean_attribute\n"
                       "Config 1,42,3.14,hello,2024-01-01,1\n"
                       "Config 2,100,2.71,world,,0\n");

    db.import_from_csv("Configuration", csv_path);
    fs::remove(csv_path);

    auto labels = db.read_scalar_strings("Configuration", "label");
    ASSERT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0], "Config 1");
    EXPECT_EQ(labels[1], "Config 2");

    auto ints = db.read_scalar_integers("Configuration", "integer_attribute");
    ASSERT_EQ(ints.size(), 2);
    EXPECT_EQ(ints[0], 42);
    EXPECT_EQ(ints[1], 100);

    auto floats = db.read_scalar_floats("Configuration", "float_attribute");
    ASSERT_EQ(floats.size(), 2);
    EXPECT_DOUBLE_EQ(floats[0], 3.14);
    EXPECT_DOUBLE_EQ(floats[1], 2.71);

    auto strings = db.read_scalar_strings("Configuration", "string_attribute");
    ASSERT_EQ(strings.size(), 2);
    EXPECT_EQ(strings[0], "hello");
    EXPECT_EQ(strings[1], "world");

    auto booleans = db.read_scalar_integers("Configuration", "boolean_attribute");
    ASSERT_EQ(booleans.size(), 2);
    EXPECT_EQ(booleans[0], 1);
    EXPECT_EQ(booleans[1], 0);
}

TEST(Database, ImportFromCsvRoundTrip) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1"))
        .set("integer_attribute", int64_t{42})
        .set("float_attribute", 3.14)
        .set("string_attribute", std::string("hello"));
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2"))
        .set("integer_attribute", int64_t{100})
        .set("float_attribute", 2.71)
        .set("string_attribute", std::string("world"));
    db.create_element("Configuration", e2);

    // Export
    auto csv_path = (fs::temp_directory_path() / "quiver_import_roundtrip.csv").string();
    db.export_to_csv("Configuration", csv_path);

    // Clear and reimport
    db.query_string("DELETE FROM Configuration");
    EXPECT_EQ(db.read_scalar_strings("Configuration", "label").size(), 0);

    db.import_from_csv("Configuration", csv_path);
    fs::remove(csv_path);

    // Verify identical
    auto labels = db.read_scalar_strings("Configuration", "label");
    ASSERT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0], "Config 1");
    EXPECT_EQ(labels[1], "Config 2");

    auto ints = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(ints[0], 42);
    EXPECT_EQ(ints[1], 100);

    auto floats = db.read_scalar_floats("Configuration", "float_attribute");
    EXPECT_DOUBLE_EQ(floats[0], 3.14);
    EXPECT_DOUBLE_EQ(floats[1], 2.71);
}

TEST(Database, ImportFromCsvEmptyCollectionWipesTable) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);
    EXPECT_EQ(db.read_scalar_strings("Configuration", "label").size(), 1);

    // Header-only CSV (no data rows)
    auto csv_path =
        write_temp_csv("quiver_import_empty.csv",
                       "sep=,\n"
                       "label,integer_attribute,float_attribute,string_attribute,date_attribute,boolean_attribute\n");

    db.import_from_csv("Configuration", csv_path);
    fs::remove(csv_path);

    EXPECT_EQ(db.read_scalar_strings("Configuration", "label").size(), 0);
}

TEST(Database, ImportFromCsvNonExistentTableThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = write_temp_csv("quiver_import_bad_table.csv",
                                   "sep=,\n"
                                   "label\n"
                                   "Item 1\n");

    EXPECT_THROW(db.import_from_csv("NonExistent", csv_path), std::runtime_error);
    fs::remove(csv_path);
}

TEST(Database, ImportFromCsvResolveForeignKeys) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    // Create parents first (they must exist for FK resolution)
    quiver::Element p1;
    p1.set("label", std::string("Parent 1"));
    db.create_element("Parent", p1);

    quiver::Element p2;
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p2);

    // Also create existing children so self-referencing FK (sibling_id) can resolve
    quiver::Element c1;
    c1.set("label", std::string("Child 1"));
    db.create_element("Child", c1);

    quiver::Element c2;
    c2.set("label", std::string("Child 2"));
    db.create_element("Child", c2);

    // CSV with FK labels instead of IDs
    auto csv_path = write_temp_csv("quiver_import_fk.csv",
                                   "sep=,\n"
                                   "label,parent_id,sibling_id\n"
                                   "Child 1,Parent 1,\n"
                                   "Child 2,Parent 2,Child 1\n");

    db.import_from_csv("Child", csv_path);
    fs::remove(csv_path);

    auto labels = db.read_scalar_strings("Child", "label");
    ASSERT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0], "Child 1");
    EXPECT_EQ(labels[1], "Child 2");

    // Verify FK resolution: parent_id should be integer IDs
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 2);
    EXPECT_EQ(parent_ids[0], 1);  // Parent 1's id
    EXPECT_EQ(parent_ids[1], 2);  // Parent 2's id

    csv_path = write_temp_csv("quiver_import_fk_parent.csv",
                              "sep=,\n"
                              "id,label\n"
                              "1,Parent 1\n"
                              "2,Parent 2\n");

    db.import_from_csv("Parent", csv_path);
    fs::remove(csv_path);

    // Verify that importing parents with same labels doesn't break child FK resolution

    parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 2);
    EXPECT_EQ(parent_ids[0], 1);  // Parent 1's id
    EXPECT_EQ(parent_ids[1], 2);  // Parent 2's id
}

TEST(Database, ImportFromCsvNullForeignKey) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = write_temp_csv("quiver_import_fk_null.csv",
                                   "sep=,\n"
                                   "label,parent_id,sibling_id\n"
                                   "Orphan,,\n");

    db.import_from_csv("Child", csv_path);
    fs::remove(csv_path);

    auto labels = db.read_scalar_strings("Child", "label");
    ASSERT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Orphan");

    // parent_id should be NULL (not returned by read_scalar_integers which skips NULLs)
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    EXPECT_EQ(parent_ids.size(), 0);
}

TEST(Database, ImportFromCsvEnumResolution) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path =
        write_temp_csv("quiver_import_enum.csv",
                       "sep=,\n"
                       "label,integer_attribute,float_attribute,string_attribute,date_attribute,boolean_attribute\n"
                       "Config 1,Active,,,,\n"
                       "Config 2,Inactive,,,,\n");

    quiver::EnumMap enums(quiver::EnumMap::Data{{"integer_attribute", {{"en", {{"Active", 1}, {"Inactive", 2}}}}}});
    db.import_from_csv("Configuration", csv_path, {}, enums);
    fs::remove(csv_path);

    auto ints = db.read_scalar_integers("Configuration", "integer_attribute");
    ASSERT_EQ(ints.size(), 2);
    EXPECT_EQ(ints[0], 1);
    EXPECT_EQ(ints[1], 2);
}

TEST(Database, ImportFromCsvVectorTable) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    // Import vector table data (id FK resolves to "Item 1" -> 1)
    auto csv_path = write_temp_csv("quiver_import_vector.csv",
                                   "sep=,\n"
                                   "id,vector_index,value_int,value_float\n"
                                   "Item 1,1,10,1.1\n"
                                   "Item 1,2,20,2.2\n"
                                   "Item 1,3,30,3.3\n");

    db.import_from_csv("Collection_vector_values", csv_path);
    fs::remove(csv_path);

    auto values = db.read_vector_integers_by_id("Collection", "value_int", 1);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
}

TEST(Database, ImportFromCsvSetTable) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    auto csv_path = write_temp_csv("quiver_import_set.csv",
                                   "sep=,\n"
                                   "id,tag\n"
                                   "Item 1,alpha\n"
                                   "Item 1,beta\n");

    db.import_from_csv("Collection_set_tags", csv_path);
    fs::remove(csv_path);

    auto tags = db.read_set_strings_by_id("Collection", "tag", 1);
    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0], "alpha");
    EXPECT_EQ(tags[1], "beta");
}

TEST(Database, ImportFromCsvTimeSeriesTable) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    auto csv_path = write_temp_csv("quiver_import_ts.csv",
                                   "sep=,\n"
                                   "id,date_time,value\n"
                                   "Item 1,2024-01-15T10:30:00,42.5\n"
                                   "Item 1,2024-01-15T11:00:00,37.2\n");

    db.import_from_csv("Collection_time_series_data", csv_path);
    fs::remove(csv_path);

    auto ts = db.read_time_series_group_by_id("Collection", "data", 1);
    ASSERT_EQ(ts.size(), 2);
    EXPECT_EQ(std::get<std::string>(ts[0].at("date_time")), "2024-01-15T10:30:00");
    EXPECT_EQ(std::get<double>(ts[0].at("value")), 42.5);
    EXPECT_EQ(std::get<std::string>(ts[1].at("date_time")), "2024-01-15T11:00:00");
    EXPECT_EQ(std::get<double>(ts[1].at("value")), 37.2);
}

TEST(Database, ImportFromCsvTimeSeriesFiles) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = write_temp_csv("quiver_import_tsf.csv",
                                   "sep=,\n"
                                   "data_file,metadata_file\n"
                                   "/path/to/data.csv,/path/to/meta.csv\n");

    db.import_from_csv("Collection_time_series_files", csv_path);
    fs::remove(csv_path);

    auto files = db.read_time_series_files("Collection");
    EXPECT_EQ(files.at("data_file").value(), "/path/to/data.csv");
    EXPECT_EQ(files.at("metadata_file").value(), "/path/to/meta.csv");
}

TEST(Database, ImportFromCsvDoesNotCascadeDelete) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // Create element with vector data
    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("some_integer", int64_t{50}).set("some_float", 1.5);
    db.create_element("Collection", e1);

    db.update_vector_integers("Collection", "value_int", 1, {10, 20, 30});

    // Verify vector data exists
    auto before = db.read_vector_integers_by_id("Collection", "value_int", 1);
    ASSERT_EQ(before.size(), 3);

    // Import CSV for Collection (scalar table only) - should NOT cascade delete vectors
    auto csv_path = write_temp_csv("quiver_import_no_cascade.csv",
                                   "sep=,\n"
                                   "label,some_integer,some_float\n"
                                   "Item 1,99,2.5\n");

    db.import_from_csv("Collection", csv_path);
    fs::remove(csv_path);

    // Verify scalar was updated
    auto ints = db.read_scalar_integers("Collection", "some_integer");
    ASSERT_EQ(ints.size(), 1);
    EXPECT_EQ(ints[0], 99);

    // Verify vector data STILL EXISTS (cascade did NOT happen)
    auto after = db.read_vector_integers_by_id("Collection", "value_int", 1);
    ASSERT_EQ(after.size(), 3);
    EXPECT_EQ(after[0], 10);
    EXPECT_EQ(after[1], 20);
    EXPECT_EQ(after[2], 30);
}

TEST(Database, ImportFromCsvNullValues) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path =
        write_temp_csv("quiver_import_nulls.csv",
                       "sep=,\n"
                       "label,integer_attribute,float_attribute,string_attribute,date_attribute,boolean_attribute\n"
                       "Config 1,,,,,\n");

    db.import_from_csv("Configuration", csv_path);
    fs::remove(csv_path);

    auto labels = db.read_scalar_strings("Configuration", "label");
    ASSERT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Config 1");

    // NULL values are skipped by read_scalar_integers/floats/strings
    auto ints = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(ints.size(), 0);

    auto floats = db.read_scalar_floats("Configuration", "float_attribute");
    EXPECT_EQ(floats.size(), 0);

    auto strings = db.read_scalar_strings("Configuration", "string_attribute");
    EXPECT_EQ(strings.size(), 0);
}

TEST(Database, ImportFromCsvSemicolonSeparator) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path =
        write_temp_csv("quiver_import_semicolon.csv",
                       "sep=;\n"
                       "label;integer_attribute;float_attribute;string_attribute;date_attribute;boolean_attribute\n"
                       "Config 1;42;3.14;;;1\n"
                       "Config 2;100;2.71;;;0\n");

    db.import_from_csv("Configuration", csv_path);
    fs::remove(csv_path);

    auto labels = db.read_scalar_strings("Configuration", "label");
    ASSERT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0], "Config 1");
    EXPECT_EQ(labels[1], "Config 2");

    auto ints = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(ints[0], 42);
    EXPECT_EQ(ints[1], 100);
}

TEST(Database, ImportFromCsvSemicolonSeparatorNoHeader) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // No sep= line - separator should be auto-detected from header
    auto csv_path =
        write_temp_csv("quiver_import_semicolon_noheader.csv",
                       "label;integer_attribute;float_attribute;string_attribute;date_attribute;boolean_attribute\n"
                       "Config 1;42;3.14;;;1\n"
                       "Config 2;100;2.71;;;0\n");

    db.import_from_csv("Configuration", csv_path);
    fs::remove(csv_path);

    auto labels = db.read_scalar_strings("Configuration", "label");
    ASSERT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0], "Config 1");
    EXPECT_EQ(labels[1], "Config 2");

    auto ints = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(ints[0], 42);
    EXPECT_EQ(ints[1], 100);
}
