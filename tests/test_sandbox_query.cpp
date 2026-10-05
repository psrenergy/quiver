#include "test_sandbox.h"

TEST_F(SandboxTest, QueryString) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection WHERE label = ?", {"Item 1"})
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(SandboxTest, QueryStringNoParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection")
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(SandboxTest, QueryStringReturnsNil) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local result = db:query_string("SELECT label FROM Collection WHERE 1 = 0")
        assert(result == nil, "Expected nil, got " .. tostring(result))
    )");
}

TEST_F(SandboxTest, QueryInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local val = db:query_integer("SELECT some_integer FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 42, "Expected 42, got " .. tostring(val))
    )");
}

TEST_F(SandboxTest, QueryIntegerCount) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local count = db:query_integer("SELECT COUNT(*) FROM Collection")
        assert(count == 2, "Expected 2, got " .. tostring(count))
    )");
}

TEST_F(SandboxTest, QueryFloat) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 3.14));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local val = db:query_float("SELECT some_float FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 3.14, "Expected 3.14, got " .. tostring(val))
    )");
}

TEST_F(SandboxTest, QueryWithMultipleParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{20}));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local val = db:query_integer(
            "SELECT some_integer FROM Collection WHERE label = ? AND some_integer > ?",
            {"Item 2", 5}
        )
        assert(val == 20, "Expected 20, got " .. tostring(val))
    )");
}

TEST_F(SandboxTest, IsHealthy) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local healthy = db:is_healthy()
        assert(healthy == true, "Expected is_healthy to return true")
    )");
}

TEST_F(SandboxTest, CurrentVersion) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local version = db:current_version()
        assert(version == 0, "Expected version 0 for a schema database, got " .. tostring(version))
    )");
}

TEST_F(SandboxTest, CurrentVersionAfterMigrations) {
    auto db = quiver::Database::from_migrations(":memory:", SCHEMA_PATH("schemas/migrations"));
    quiver::Sandbox lua(db);

    lua.run(R"(
        local version = db:current_version()
        assert(version == 3, "Expected version 3 after three migrations, got " .. tostring(version))
    )");
}

TEST_F(SandboxTest, Path) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local p = db:path()
        assert(type(p) == "string", "Expected path to be a string")
        assert(p == ":memory:", "Expected ':memory:', got " .. p)
    )");
}

TEST_F(SandboxTest, Describe) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    // describe() returns a human-readable report string
    lua.run(R"(
        local report = db:describe()
        assert(report:find("Collection: Configuration"), "describe should mention Configuration")
    )");
}

TEST_F(SandboxTest, QueryParameterCountMismatch) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::Sandbox lua(db);

    // Too few parameters for the single placeholder
    expect_lua_error(
        lua,
        R"(db:query_string("SELECT label FROM Collection WHERE some_integer = ?", {}))",
        "expected 1 bound"
    );

    // Too many parameters for the single placeholder
    expect_lua_error(
        lua,
        R"(db:query_string("SELECT label FROM Collection WHERE some_integer = ?", {42, 43}))",
        "expected 1 bound"
    );

    // Exactly one parameter succeeds
    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection WHERE some_integer = ?", {42})
        assert(label == "Item 1", "expected Item 1, got " .. tostring(label))
    )");
}

// Lua stores no key for a nil, so a query parameter table's length is the `#` border: an interior
// nil in a constructor ({ nil, 5 }) is counted and binds NULL, a trailing one ({ 5, nil }) is not.
TEST_F(SandboxTest, QueryInteriorNilParamBindsNull) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        local r = db:query_integer("SELECT CASE WHEN ? IS NULL THEN ? ELSE -1 END", { nil, 5 })
        assert(r == 5, "interior nil must bind NULL, got " .. tostring(r))
    )");
}

TEST_F(SandboxTest, QueryTrailingNilParamIsACountMismatch) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    expect_lua_error(lua, R"(db:query_integer("SELECT ? + ?", { 5, nil }))", "expected 2 bound parameter(s) but got 1");
}

// A wrong-typed params argument used to be ignored, so the query ran with no parameters.
TEST_F(SandboxTest, QueryRejectsWrongTypedParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    for (const std::string op : {"query_string", "query_integer", "query_float"}) {
        expect_lua_error(lua, "db:" + op + "('SELECT 1', 5)", "Cannot " + op + ": params must be a table, got number");
        expect_lua_error(
            lua,
            "db:" + op + "('SELECT 1', db)",
            "Cannot " + op + ": params must be a table, got userdata"
        );
    }
    EXPECT_EQ(lua.run("return db:query_integer('SELECT 1', nil)"), "1");
    EXPECT_EQ(lua.run("return db:query_integer('SELECT ?', { 7 })"), "7");
}
