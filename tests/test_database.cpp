#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/element.h>
#include <string>

namespace fs = std::filesystem;

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
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    // Create table
    db.execute("CREATE TABLE Plant (id INTEGER PRIMARY KEY, label TEXT, capacity REAL)");

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
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    // Create tables
    db.execute("CREATE TABLE Plant (id INTEGER PRIMARY KEY, label TEXT)");
    db.execute(
        "CREATE TABLE Plant_vector_costs (id INTEGER, vector_index INTEGER, costs REAL, PRIMARY KEY (id, vector_index))");

    // Create element with vector
    psr::Element element;
    element.set("label", std::string("Plant 1")).set_vector("costs", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Plant", element);
    EXPECT_EQ(id, 1);

    // Verify main table
    auto result = db.execute("SELECT label FROM Plant WHERE id = ?", {id});
    EXPECT_EQ(result.row_count(), 1);
    EXPECT_EQ(result[0].get_string(0).value(), "Plant 1");

    // Verify vector table
    auto vec_result = db.execute("SELECT vector_index, costs FROM Plant_vector_costs WHERE id = ? ORDER BY vector_index",
                                 {id});
    EXPECT_EQ(vec_result.row_count(), 3);
    EXPECT_EQ(vec_result[0].get_int(0).value(), 1);
    EXPECT_EQ(vec_result[0].get_double(1).value(), 1.5);
    EXPECT_EQ(vec_result[1].get_int(0).value(), 2);
    EXPECT_EQ(vec_result[1].get_double(1).value(), 2.5);
    EXPECT_EQ(vec_result[2].get_int(0).value(), 3);
    EXPECT_EQ(vec_result[2].get_double(1).value(), 3.5);
}

TEST_F(DatabaseFixture, CreateMultipleElements) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    db.execute("CREATE TABLE Config (id INTEGER PRIMARY KEY, name TEXT, value REAL)");

    psr::Element e1;
    e1.set("name", std::string("Config A")).set("value", 100.0);
    int64_t id1 = db.create_element("Config", e1);

    psr::Element e2;
    e2.set("name", std::string("Config B")).set("value", 200.0);
    int64_t id2 = db.create_element("Config", e2);

    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);

    auto result = db.execute("SELECT COUNT(*) FROM Config");
    EXPECT_EQ(result[0].get_int(0).value(), 2);
}
