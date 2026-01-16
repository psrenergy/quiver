#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/element.h>
#include <psr/lua_runner.h>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

class LuaRunnerTest : public ::testing::Test {
protected:
    void SetUp() override { collections_schema = schema_path("schemas/valid/collections.sql"); }
    std::string collections_schema;
};

TEST_F(LuaRunnerTest, CreateElementFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42, some_float = 3.14 })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers[0], 42);
}

TEST_F(LuaRunnerTest, ReadScalarStringsFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Test Config"));
    db.create_element("Collection", psr::Element().set("label", "Item 1"));
    db.create_element("Collection", psr::Element().set("label", "Item 2"));

    psr::LuaRunner lua(db);

    // Read from Lua and verify count
    lua.run(R"(
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels")
        assert(labels[1] == "Item 1", "First label mismatch")
        assert(labels[2] == "Item 2", "Second label mismatch")
    )");
}

TEST_F(LuaRunnerTest, LuaScriptError) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", psr::Element().set("label", "Test Config"));

    psr::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("invalid lua syntax !!!"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementWithArrays) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

    // Note: vector columns in the same table must have the same length
    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "Item 1",
            value_int = {1, 2, 3},
            value_float = {1.5, 2.5, 3.5}
        })
    )");

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));

    auto floats = db.read_vector_doubles("Collection", "value_float");
    EXPECT_EQ(floats.size(), 1);
    EXPECT_EQ(floats[0], (std::vector<double>{1.5, 2.5, 3.5}));
}

TEST_F(LuaRunnerTest, ReuseRunner) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

    lua.run(R"(db:create_element("Configuration", { label = "Test Config" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 1" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 2" }))");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
}
