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
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    // Create elements
    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42}).set("float_attribute", 3.14);
    db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100}).set("float_attribute", 2.71);
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
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

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

TEST(Database, ExportToCsvNonExistentCollectionThrows) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = quiver::LogLevel::off});

    auto csv_path = "quiver_export_invalid.csv";
    EXPECT_THROW(db.export_to_csv("NonExistent", csv_path), std::runtime_error);
}
