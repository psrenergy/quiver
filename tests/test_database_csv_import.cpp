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
// import_csv helpers
// ============================================================================

// Helper: create a database from the csv_export schema
static quiver::Database make_db() {
    return quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("csv_export.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
}

// Helper: get a unique temp path for a test
static fs::path temp_csv(const std::string& test_name) {
    return fs::temp_directory_path() / ("quiver_test_" + test_name + ".csv");
}

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

    quiver::CSVOptions options;
    options.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};

    db.import_csv("Items", "", csv_path.string(), options);

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

    quiver::CSVOptions options;
    options.enum_labels["status"]["en"] = {{"Active", 1}};

    db.import_csv("Items", "", csv_path.string(), options);

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

    quiver::CSVOptions options;
    options.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};
    options.enum_labels["status"]["pt"] = {{"Ativo", 1}, {"Inativo", 2}};

    db.import_csv("Items", "", csv_path.string(), options);

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

    quiver::CSVOptions options;
    options.date_time_format = "%Y/%m/%d";

    db.import_csv("Items", "", csv_path.string(), options);

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

    quiver::Element update1;
    update1.set("measurement", std::vector<double>{1.1, 2.2, 3.3});
    db.update_element("Items", id1, update1);

    // Export
    auto csv_path = temp_csv("ImportVectorRT");
    db.export_csv("Items", "measurements", csv_path.string());

    // Clear vector data and re-import (parent element must exist for group import)
    db.update_element("Items", id1, quiver::Element().set("measurement", std::vector<double>{}));
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

    quiver::Element update1;
    update1.set("tag", std::vector<std::string>{"red", "green", "blue"});
    db.update_element("Items", id1, update1);

    // Export
    auto csv_path = temp_csv("ImportSetRT");
    db.export_csv("Items", "tags", csv_path.string());

    // Clear set data and re-import (parent element must exist for group import)
    db.update_element("Items", id1, quiver::Element().set("tag", std::vector<std::string>{}));
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

    quiver::Element update;
    update.set("tag", std::vector<std::string>{"red", "green"});
    db.update_element("Items", id1, update);

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

    quiver::CSVOptions options;
    options.enum_labels["status"]["en"] = {{"Active", 1}, {"Inactive", 2}};

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string(), options);
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

    quiver::CSVOptions options;
    options.date_time_format = "%Y/%m/%d";

    db.import_csv("Items", "readings", csv_path.string(), options);

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

// ============================================================================
// import_csv: Semicolon delimiter handling
// ============================================================================

TEST(DatabaseCSV, ImportCSV_SemicolonSepHeader_RoundTrip) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportSemicolonSep");
    write_csv_file(csv_path.string(), "sep=;\nlabel;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "Alpha");

    auto price = db.read_scalar_float_by_id("Items", "price", 1);
    ASSERT_TRUE(price.has_value());
    EXPECT_NEAR(price.value(), 9.99, 0.001);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_SemicolonAutoDetect_RoundTrip) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportSemicolonAuto");
    // No sep= header, semicolons present, no commas -> auto-detected as semicolon-delimited
    write_csv_file(csv_path.string(), "label;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "Alpha");

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Cannot open file
// ============================================================================

TEST(DatabaseCSV, ImportCSV_CannotOpenFile_Throws) {
    auto db = make_db();

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", "/nonexistent/path/file.csv");
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Cannot import_csv: could not open file"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

// ============================================================================
// import_csv: Custom datetime format parse failure
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Scalar_BadCustomDateTimeFormat_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportBadCustomDateTime");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,not-a-date,\n");

    quiver::CSVOptions options;
    options.date_time_format = "%Y/%m/%d";

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string(), options);
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Timestamp not-a-date is not valid"), std::string::npos);
                EXPECT_NE(msg.find("format %Y/%m/%d"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Per-row column count mismatch
// ============================================================================

TEST(DatabaseCSV, ImportCSV_RowColumnCountMismatch_Throws) {
    auto db = make_db();
    auto csv_path = temp_csv("ImportRowColMismatch");
    // Header has 6 columns, data row has 7 (extra column)
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,1,9.99,,note,EXTRA\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("but the header has"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Self-FK label not found
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Scalar_SelfFK_InvalidLabel_Throws) {
    auto db = make_relations_db();

    quiver::Element p1;
    p1.set("label", std::string("Parent1"));
    db.create_element("Parent", p1);

    auto csv_path = temp_csv("ImportSelfFKBad");
    // Child2 references NonExistent via self-FK sibling_id
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,parent_id,sibling_id\n"
                   "Child1,Parent1,\n"
                   "Child2,Parent1,NonExistent\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Child", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Could not find an existing element from collection Child with label NonExistent"),
                          std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Group FK tests (vector with FK)
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Vector_WithFK_RoundTrip) {
    auto db = make_relations_db();

    // Create parent elements
    quiver::Element p1;
    p1.set("label", std::string("Parent1"));
    db.create_element("Parent", p1);

    quiver::Element p2;
    p2.set("label", std::string("Parent2"));
    db.create_element("Parent", p2);

    // Create child element
    quiver::Element c1;
    c1.set("label", std::string("Child1")).set("parent_id", int64_t{1});
    db.create_element("Child", c1);

    // Import vector group with FK column (parent_ref -> Parent)
    auto csv_path = temp_csv("ImportVectorFK");
    write_csv_file(csv_path.string(),
                   "sep=,\nid,vector_index,parent_ref\n"
                   "Child1,1,Parent1\n"
                   "Child1,2,Parent2\n");

    db.import_csv("Child", "refs", csv_path.string());

    auto vals = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(vals.size(), 2);
    EXPECT_EQ(vals[0], 1);  // Parent1 -> id 1
    EXPECT_EQ(vals[1], 2);  // Parent2 -> id 2

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Vector_FK_InvalidLabel_Throws) {
    auto db = make_relations_db();

    quiver::Element p1;
    p1.set("label", std::string("Parent1"));
    db.create_element("Parent", p1);

    quiver::Element c1;
    c1.set("label", std::string("Child1")).set("parent_id", int64_t{1});
    db.create_element("Child", c1);

    auto csv_path = temp_csv("ImportVectorFKBad");
    write_csv_file(csv_path.string(), "sep=,\nid,vector_index,parent_ref\nChild1,1,NonExistent\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Child", "refs", csv_path.string());
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                EXPECT_NE(msg.find("Could not find an existing element from collection Parent with label NonExistent"),
                          std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Group NOT NULL validation
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Group_NotNull_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportGroupNotNull");
    // tag is NOT NULL in Items_set_tags — empty cell should fail
    write_csv_file(csv_path.string(), "sep=,\nid,tag\nItem1,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "tags", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Column tag cannot be NULL"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Group enum resolution (integer column with enum labels)
// ============================================================================

TEST(DatabaseCSV, ImportCSV_TimeSeries_EnumInGroup_RoundTrip) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    auto id1 = db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportTSEnum");
    write_csv_file(csv_path.string(),
                   "sep=,\nid,date_time,temperature,humidity\n"
                   "Item1,2024-01-01T10:00:00,22.5,Low\n"
                   "Item1,2024-01-01T11:00:00,23.0,High\n");

    quiver::CSVOptions options;
    options.enum_labels["humidity"]["en"] = {{"Low", 60}, {"High", 90}};

    db.import_csv("Items", "readings", csv_path.string(), options);

    auto ts_rows = db.read_time_series_group("Items", "readings", id1);
    ASSERT_EQ(ts_rows.size(), 2);
    EXPECT_EQ(std::get<int64_t>(ts_rows[0].at("humidity")), 60);
    EXPECT_EQ(std::get<int64_t>(ts_rows[1].at("humidity")), 90);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Group invalid enum (no mapping provided for INTEGER column)
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Group_InvalidEnum_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportGroupBadEnum");
    // humidity is INTEGER NOT NULL — "Unknown" is not in the enum mapping
    write_csv_file(csv_path.string(),
                   "sep=,\nid,date_time,temperature,humidity\n"
                   "Item1,2024-01-01T10:00:00,22.5,Unknown\n");

    quiver::CSVOptions options;
    options.enum_labels["humidity"]["en"] = {{"Low", 60}, {"High", 90}};

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "readings", csv_path.string(), options);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Invalid enum value"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Group duplicate entries (UNIQUE constraint violation)
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Group_DuplicateEntries_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportGroupDuplicates");
    // Duplicate (id, tag) pair — violates UNIQUE constraint
    write_csv_file(csv_path.string(), "sep=,\nid,tag\nItem1,red\nItem1,red\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "tags", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("duplicate entries"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

// ============================================================================
// import_csv: Vector non-numeric index
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Vector_NonNumericIndex_Throws) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportVectorNonNumericIdx");
    write_csv_file(csv_path.string(), "sep=,\nid,vector_index,measurement\nItem1,abc,1.1\n");

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

// ============================================================================
// import_csv: Trailing empty columns (Excel artifact)
// ============================================================================

TEST(DatabaseCSV, ImportCSV_Scalar_TrailingEmptyColumns) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportTrailingEmpty");
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "label,name,status,price,date_created,notes,,,,\n"
                   "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,,,\n"
                   "Item2,Beta,2,19.5,2024-02-20T08:00:00,second,,,,\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "Alpha");
    EXPECT_EQ(names[1], "Beta");

    auto prices = db.read_scalar_floats("Items", "price");
    ASSERT_EQ(prices.size(), 2);
    EXPECT_NEAR(prices[0], 9.99, 0.001);
    EXPECT_NEAR(prices[1], 19.5, 0.001);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_TrailingEmptyColumnsWithWhitespace) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportTrailingWhitespace");
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "label,name,status,price,date_created,notes, ,\t, \t\n"
                   "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first, ,\t, \t\n"
                   "Item2,Beta,2,19.5,2024-02-20T08:00:00,second, ,\t, \t\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "Alpha");
    EXPECT_EQ(names[1], "Beta");

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_TrailingEmptyColumnsFewerOnDataRows) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportTrailingFewer");
    // Header has 4 trailing commas, data rows have only 2
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "label,name,status,price,date_created,notes,,,,\n"
                   "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,\n"
                   "Item2,Beta,2,19.5,2024-02-20T08:00:00,second,,\n");

    db.import_csv("Items", "", csv_path.string());

    auto names = db.read_scalar_strings("Items", "name");
    ASSERT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "Alpha");
    EXPECT_EQ(names[1], "Beta");

    auto notes = db.read_scalar_strings("Items", "notes");
    ASSERT_EQ(notes.size(), 2);
    EXPECT_EQ(notes[0], "first");
    EXPECT_EQ(notes[1], "second");

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Scalar_TrailingEmptyColumnsMoreOnDataRows) {
    auto db = make_db();

    auto csv_path = temp_csv("ImportTrailingMore");
    // Header has 2 trailing commas, data rows have 4 — should error
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "label,name,status,price,date_created,notes,,\n"
                   "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,,,\n"
                   "Item2,Beta,2,19.5,2024-02-20T08:00:00,second,,,,\n");

    EXPECT_THROW(
        {
            try {
                db.import_csv("Items", "", csv_path.string());
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("but the header has"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);

    fs::remove(csv_path);
}

TEST(DatabaseCSV, ImportCSV_Vector_TrailingEmptyColumns) {
    auto db = make_db();

    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("A")).set("status", int64_t{1}).set("price", 1.0);
    db.create_element("Items", e1);

    auto csv_path = temp_csv("ImportVectorTrailingEmpty");
    write_csv_file(csv_path.string(),
                   "sep=,\n"
                   "id,vector_index,measurement,,,\n"
                   "Item1,1,1.1,,,\n"
                   "Item1,2,2.2,,,\n");

    db.import_csv("Items", "measurements", csv_path.string());

    auto vals = db.read_vector_floats_by_id("Items", "measurement", 1);
    ASSERT_EQ(vals.size(), 2);
    EXPECT_NEAR(vals[0], 1.1, 0.001);
    EXPECT_NEAR(vals[1], 2.2, 0.001);

    fs::remove(csv_path);
}
