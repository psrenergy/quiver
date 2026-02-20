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

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 4);  // sep + header + 2 data rows
    EXPECT_EQ(lines[0], "sep=,");
    EXPECT_EQ(lines[1], "label,boolean_attribute,date_attribute,float_attribute,integer_attribute,string_attribute");
    EXPECT_EQ(lines[2], "Config 1,1,,3.14,42,");
    EXPECT_EQ(lines[3], "Config 2,,2024-01-01T00:00:00,2.71,100,");
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
    EXPECT_EQ(lines[0], "sep=,");
    EXPECT_EQ(lines[1], "label,boolean_attribute,date_attribute,float_attribute,integer_attribute,string_attribute");
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
    EXPECT_EQ(lines[1], "label,parent_id,sibling_id");
    EXPECT_EQ(lines[2], "Child 1,Parent 1,");
    EXPECT_EQ(lines[3], "Child 2,Parent 2,Child 1");
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
    EXPECT_EQ(lines[1], "label,parent_id,sibling_id");
    EXPECT_EQ(lines[2], "Orphan,,");
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
    EXPECT_EQ(lines[1], "id,parent_ref,vector_index");
    EXPECT_EQ(lines[2], "Child 1,Parent 1,1");
    EXPECT_EQ(lines[3], "Child 1,Parent 1,2");
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
    EXPECT_EQ(lines[1], "id,parent_ref");
    EXPECT_EQ(lines[2], "Child 1,Parent 1");
    EXPECT_EQ(lines[3], "Child 1,Parent 2");
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
    EXPECT_EQ(lines[1], "id,value_float,value_int,vector_index");
    EXPECT_EQ(lines[2], "Item 1,,10,1");
    EXPECT_EQ(lines[3], "Item 1,,20,2");
    EXPECT_EQ(lines[4], "Item 1,,30,3");
}

TEST(Database, ExportToCsvTimeSeriesTable) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

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
    EXPECT_EQ(lines[1], "date_time,id,value");
    EXPECT_EQ(lines[2], "2024-01-15T10:30:00,Item 1,42.5");
    EXPECT_EQ(lines[3], "2024-01-15T11:00:00,Item 1,37.2");
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
    EXPECT_EQ(lines[1], "date_time,id,value");
    EXPECT_EQ(lines[2], "2024-01,Item 1,42.5");
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
    EXPECT_EQ(lines[1], "label,boolean_attribute,date_attribute,float_attribute,integer_attribute,string_attribute");
    EXPECT_EQ(lines[2], "Config 1,,,,Active,");
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
    EXPECT_EQ(lines[2], "Config 1,,,,Active,");
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
    EXPECT_EQ(lines[2], "Config 1,,2024-01-15T10:30:00,3.14,42,");
    EXPECT_EQ(lines[3], "Config 2,,2024-06-01T00:00:00,2.71,100,");
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
    EXPECT_EQ(lines[2], "Child 1,Parent 1,");
}

TEST(Database, ExportToCsvNonExistentCollectionThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    auto csv_path = "quiver_export_invalid.csv";
    EXPECT_THROW(db.export_csv("NonExistent", csv_path), std::runtime_error);
}

TEST(Database, ExportToCsvNullValues) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2"));  // Other attributes are NULL
    db.create_element("Configuration", e2);

    auto csv_path = (fs::temp_directory_path() / "quiver_export_nulls.csv").string();
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
    EXPECT_EQ(lines[0], "sep=,");
    EXPECT_EQ(lines[1], "label,boolean_attribute,date_attribute,float_attribute,integer_attribute,string_attribute");
    EXPECT_EQ(lines[2], "Config 1,,,,42,");
    EXPECT_EQ(lines[3], "Config 2,,,,6,");  // integer_attribute defaults to 6 from schema
}

TEST(Database, ExportToCsvTimeSeriesFiles) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    db.query_string(
        "INSERT INTO Collection_time_series_files (data_file, metadata_file) VALUES ('data.csv', 'meta.csv')");

    auto csv_path = (fs::temp_directory_path() / "quiver_export_ts_files.csv").string();
    db.export_csv("Collection_time_series_files", csv_path);

    std::ifstream file(csv_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    fs::remove(csv_path);

    ASSERT_EQ(lines.size(), 3);  // sep + header + 1 data row
    EXPECT_EQ(lines[0], "sep=,");
    EXPECT_EQ(lines[1], "data_file,metadata_file");
    EXPECT_EQ(lines[2], "data.csv,meta.csv");
}

TEST(Database, ExportToCsvInvalidPathThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1"));
    db.create_element("Configuration", e1);

    auto invalid_path = "Z:/nonexistent_drive/invalid_path/file.csv";
    EXPECT_THROW(db.export_csv("Configuration", invalid_path), std::runtime_error);
}
