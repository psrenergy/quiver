#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/element.h>
#include <string>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

TEST_F(DatabaseFixture, OpenFileOnDisk) {
    psr::Database db(path);
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), path);
}

TEST_F(DatabaseFixture, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), ":memory:");
}

TEST_F(DatabaseFixture, DestructorClosesDatabase) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    // Database should be closed after destructor
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(DatabaseFixture, MoveConstructor) {
    psr::Database db1(path);
    EXPECT_TRUE(db1.is_healthy());

    psr::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(DatabaseFixture, MoveAssignment) {
    psr::Database db1(path);
    psr::Database db2(":memory:");

    db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
}

TEST_F(DatabaseFixture, LogLevelDebug) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::debug});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(DatabaseFixture, LogLevelOff) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_TRUE(db.is_healthy());
}

TEST_F(DatabaseFixture, CreatesFileOnDisk) {
    {
        psr::Database db(path);
        EXPECT_TRUE(db.is_healthy());
    }
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(DatabaseFixture, CreateElementWithScalars) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("test_database_schema.sql"), {.console_level = psr::LogLevel::off});

    // Create element
    psr::Element element;
    element.set("label", std::string("Plant 1")).set("capacity", 50.0);

    int64_t id = db.create_element("Plant", element);
    EXPECT_EQ(id, 1);

    // Verify
    auto result = db.execute("SELECT label, capacity FROM Plant WHERE id = ?", {id});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(0).value(), "Plant 1");
    EXPECT_EQ(result[0].get_double(1).value(), 50.0);
}

TEST_F(DatabaseFixture, CreateElementWithVector) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("test_database_schema.sql"), {.console_level = psr::LogLevel::off});

    // Create element with vector group (single attribute per row)
    psr::Element element;
    psr::VectorGroup costs_group = {
        {{"costs", 1.5}},
        {{"costs", 2.5}},
        {{"costs", 3.5}}
    };
    element.set("label", std::string("Plant 1")).add_vector_group("costs", costs_group);

    int64_t id = db.create_element("Plant", element);
    EXPECT_EQ(id, 1);

    // Verify main table
    auto result = db.execute("SELECT label FROM Plant WHERE id = ?", {id});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(0).value(), "Plant 1");

    // Verify vector table
    auto vec_result =
        db.execute("SELECT vector_index, costs FROM Plant_vector_costs WHERE id = ? ORDER BY vector_index", {id});
    EXPECT_EQ(vec_result.row_count(), 3);
    EXPECT_EQ(vec_result[0].get_int(0).value(), 1);
    EXPECT_EQ(vec_result[0].get_double(1).value(), 1.5);
    EXPECT_EQ(vec_result[1].get_int(0).value(), 2);
    EXPECT_EQ(vec_result[1].get_double(1).value(), 2.5);
    EXPECT_EQ(vec_result[2].get_int(0).value(), 3);
    EXPECT_EQ(vec_result[2].get_double(1).value(), 3.5);
}

TEST_F(DatabaseFixture, CreateElementWithVectorGroup) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("test_database_schema.sql"), {.console_level = psr::LogLevel::off});

    // Create element with vector group containing multiple attributes per row
    psr::Element element;
    psr::VectorGroup multi_attr_group = {
        {{"factor", 1.5}, {"quantity", int64_t{10}}},
        {{"factor", 2.5}, {"quantity", int64_t{20}}},
        {{"factor", 3.5}, {"quantity", int64_t{30}}}
    };
    element.set("label", std::string("Plant 1")).add_vector_group("multi_attr", multi_attr_group);

    int64_t id = db.create_element("Plant", element);
    EXPECT_EQ(id, 1);

    // Verify vector table with multiple attributes
    auto vec_result = db.execute(
        "SELECT vector_index, factor, quantity FROM Plant_vector_multi_attr WHERE id = ? ORDER BY vector_index", {id});
    EXPECT_EQ(vec_result.row_count(), 3);
    
    EXPECT_EQ(vec_result[0].get_int(0).value(), 1);
    EXPECT_EQ(vec_result[0].get_double(1).value(), 1.5);
    EXPECT_EQ(vec_result[0].get_int(2).value(), 10);
    
    EXPECT_EQ(vec_result[1].get_int(0).value(), 2);
    EXPECT_EQ(vec_result[1].get_double(1).value(), 2.5);
    EXPECT_EQ(vec_result[1].get_int(2).value(), 20);
    
    EXPECT_EQ(vec_result[2].get_int(0).value(), 3);
    EXPECT_EQ(vec_result[2].get_double(1).value(), 3.5);
    EXPECT_EQ(vec_result[2].get_int(2).value(), 30);
}

TEST_F(DatabaseFixture, CreateElementWithSetGroup) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("test_database_schema.sql"), {.console_level = psr::LogLevel::off});

    // Create element with set group
    psr::Element element;
    psr::SetGroup tags_set = {
        {{"tag_name", std::string("important")}, {"priority", int64_t{1}}},
        {{"tag_name", std::string("urgent")}, {"priority", int64_t{2}}},
        {{"tag_name", std::string("review")}, {"priority", int64_t{3}}}
    };
    element.set("label", std::string("Plant 1")).add_set_group("tags", tags_set);

    int64_t id = db.create_element("Plant", element);
    EXPECT_EQ(id, 1);

    // Verify set table (no vector_index, unordered)
    auto set_result = db.execute(
        "SELECT tag_name, priority FROM Plant_set_tags WHERE id = ? ORDER BY priority", {id});
    EXPECT_EQ(set_result.row_count(), 3);
    
    EXPECT_EQ(set_result[0].get_string(0).value(), "important");
    EXPECT_EQ(set_result[0].get_int(1).value(), 1);
    
    EXPECT_EQ(set_result[1].get_string(0).value(), "urgent");
    EXPECT_EQ(set_result[1].get_int(1).value(), 2);
    
    EXPECT_EQ(set_result[2].get_string(0).value(), "review");
    EXPECT_EQ(set_result[2].get_int(1).value(), 3);
}

TEST_F(DatabaseFixture, CreateMultipleElements) {
    auto db = psr::Database::from_schema(
        ":memory:", schema_path("test_database_schema.sql"), {.console_level = psr::LogLevel::off});

    // Use Configuration table which has name and value columns
    psr::Element e1;
    e1.set("label", std::string("Config A")).set("name", std::string("Setting 1")).set("value", 100.0);
    int64_t id1 = db.create_element("Configuration", e1);

    psr::Element e2;
    e2.set("label", std::string("Config B")).set("name", std::string("Setting 2")).set("value", 200.0);
    int64_t id2 = db.create_element("Configuration", e2);

    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);

    auto result = db.execute("SELECT COUNT(*) FROM Configuration");
    EXPECT_EQ(result[0].get_int(0).value(), 2);
}

TEST_F(DatabaseFixture, CreateTable) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
    db.execute("INSERT INTO test (value) VALUES (?)", {std::string("hello")});

    auto result = db.execute("SELECT * FROM test");
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(1).value(), "hello");
}

TEST_F(DatabaseFixture, CurrentVersion) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    EXPECT_EQ(db.current_version(), 0);
}
