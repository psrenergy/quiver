#include "test_lua_runner.h"

TEST_F(LuaRunnerTest, QueryString) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection WHERE label = ?", {"Item 1"})
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(LuaRunnerTest, QueryStringNoParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection")
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(LuaRunnerTest, QueryStringReturnsNil) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local result = db:query_string("SELECT label FROM Collection WHERE 1 = 0")
        assert(result == nil, "Expected nil, got " .. tostring(result))
    )");
}

TEST_F(LuaRunnerTest, QueryInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:query_integer("SELECT some_integer FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 42, "Expected 42, got " .. tostring(val))
    )");
}

TEST_F(LuaRunnerTest, QueryIntegerCount) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local count = db:query_integer("SELECT COUNT(*) FROM Collection")
        assert(count == 2, "Expected 2, got " .. tostring(count))
    )");
}

TEST_F(LuaRunnerTest, QueryFloat) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 3.14));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:query_float("SELECT some_float FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 3.14, "Expected 3.14, got " .. tostring(val))
    )");
}

TEST_F(LuaRunnerTest, QueryWithMultipleParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{20}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:query_integer(
            "SELECT some_integer FROM Collection WHERE label = ? AND some_integer > ?",
            {"Item 2", 5}
        )
        assert(val == 20, "Expected 20, got " .. tostring(val))
    )");
}

TEST_F(LuaRunnerTest, IsHealthy) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local healthy = db:is_healthy()
        assert(healthy == true, "Expected is_healthy to return true")
    )");
}

TEST_F(LuaRunnerTest, CurrentVersion) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local version = db:current_version()
        assert(type(version) == "number", "Expected version to be a number")
    )");
}

TEST_F(LuaRunnerTest, Path) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local p = db:path()
        assert(type(p) == "string", "Expected path to be a string")
        assert(p == ":memory:", "Expected ':memory:', got " .. p)
    )");
}

TEST_F(LuaRunnerTest, Describe) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // describe() returns a human-readable report string
    lua.run(R"(
        local report = db:describe()
        assert(report:find("Collection: Configuration"), "describe should mention Configuration")
    )");
}
