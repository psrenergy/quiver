#include "test_lua_runner.h"

// Scripts hand a value back to the host as JSON. Only the first returned value is encoded.

namespace {

// Every case here is schema-independent, so one in-memory database serves them all.
quiver::Database return_database() {
    return quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
}

}  // namespace

TEST_F(LuaRunnerTest, ReturnNothingYieldsEmptyString) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    EXPECT_EQ(lua.run("local x = 1"), "");
}

TEST_F(LuaRunnerTest, ReturnNilYieldsNull) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    // Distinct from returning nothing at all.
    EXPECT_EQ(lua.run("return nil"), "null");
}

TEST_F(LuaRunnerTest, ReturnScalars) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    EXPECT_EQ(lua.run("return 42"), "42");
    EXPECT_EQ(lua.run("return -7"), "-7");
    EXPECT_EQ(lua.run("return 1.5"), "1.5");
    // Shortest round-trippable form, not "0.10000000000000001".
    EXPECT_EQ(lua.run("return 0.1"), "0.1");
    EXPECT_EQ(lua.run("return true"), "true");
    EXPECT_EQ(lua.run("return false"), "false");
    EXPECT_EQ(lua.run("return 'hello'"), "\"hello\"");
    // int64 beyond double precision must survive as an integer.
    EXPECT_EQ(lua.run("return 9007199254740993"), "9007199254740993");
}

TEST_F(LuaRunnerTest, ReturnOnlyFirstValue) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    EXPECT_EQ(lua.run("return 1, 2, 3"), "1");
}

TEST_F(LuaRunnerTest, ReturnNonFiniteNumbersBecomeNull) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    // JSON has no NaN/Infinity literals.
    EXPECT_EQ(lua.run("return 0/0"), "null");
    EXPECT_EQ(lua.run("return math.huge"), "null");
    EXPECT_EQ(lua.run("return -math.huge"), "null");
}

TEST_F(LuaRunnerTest, ReturnStringsAreEscaped) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    EXPECT_EQ(lua.run(R"(return 'a"b\\c')"), R"("a\"b\\c")");
    EXPECT_EQ(lua.run("return 'line\\nbreak\\ttab'"), "\"line\\nbreak\\ttab\"");
    // Control characters below 0x20 without a short escape use \u00XX.
    EXPECT_EQ(lua.run("return string.char(1)"), "\"\\u0001\"");
}

TEST_F(LuaRunnerTest, ReturnArrays) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    EXPECT_EQ(lua.run("return {1, 2, 3}"), "[1,2,3]");
    EXPECT_EQ(lua.run("return {'a', 'b'}"), "[\"a\",\"b\"]");
    EXPECT_EQ(lua.run("return {{1, 2}, {3}}"), "[[1,2],[3]]");
    // An empty table is ambiguous; it encodes as an array.
    EXPECT_EQ(lua.run("return {}"), "[]");
}

TEST_F(LuaRunnerTest, ReturnObjectsWithSortedKeys) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    // Lua's pairs order is unspecified, so keys are sorted to keep the output deterministic.
    EXPECT_EQ(lua.run("return {b = 2, a = 1, c = 3}"), R"({"a":1,"b":2,"c":3})");
    EXPECT_EQ(lua.run("return {outer = {inner = 'v'}}"), R"({"outer":{"inner":"v"}})");
    // Non-contiguous integer keys are not an array; they stringify as object keys.
    EXPECT_EQ(lua.run("return {[1] = 'a', [3] = 'c'}"), R"({"1":"a","3":"c"})");
    // A mixed table is an object too.
    EXPECT_EQ(lua.run("return {[1] = 'a', name = 'x'}"), R"({"1":"a","name":"x"})");
}

TEST_F(LuaRunnerTest, ReturnDatabaseReads) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 10 })
        db:create_element("Collection", { label = "Item 2", some_integer = 20 })
    )");

    EXPECT_EQ(lua.run(R"(return db:read_scalar_integers("Collection", "some_integer"))"), "[10,20]");
    EXPECT_EQ(lua.run(R"(return { ids = db:read_element_ids("Collection") })"), R"({"ids":[1,2]})");
}

TEST_F(LuaRunnerTest, ReturnUnsupportedTypeThrows) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, "return function() end", "Cannot run: script returned an unsupported Lua type");
    expect_lua_error(lua, "return coroutine.create(function() end)", "unsupported Lua type");
    // A userdata (the db global itself) is not encodable either.
    expect_lua_error(lua, "return db", "unsupported Lua type");
}

TEST_F(LuaRunnerTest, ReturnUnsupportedTableKeyThrows) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, "return {[1.5] = 'x'}", "Cannot run: script returned a table with an unsupported key type");
}

TEST_F(LuaRunnerTest, ReturnTooDeeplyNestedThrows) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    // The depth cap is what stops a self-referencing table from blowing the stack.
    expect_lua_error(lua,
                     R"(
        local t = {}
        t[1] = t
        return t
    )",
                     "nests deeper than 32 levels");

    // A merely deep (acyclic) table trips the same cap.
    expect_lua_error(lua,
                     R"(
        local root = {}
        local node = root
        for _ = 1, 40 do
            node[1] = {}
            node = node[1]
        end
        return root
    )",
                     "nests deeper than 32 levels");
}

TEST_F(LuaRunnerTest, ReturnAtTheDepthLimitSucceeds) {
    auto db = return_database();
    quiver::LuaRunner lua(db);

    // 31 nested tables plus the scalar leaf sits just inside the cap.
    auto result = lua.run(R"(
        local root = {}
        local node = root
        for _ = 1, 30 do
            node[1] = {}
            node = node[1]
        end
        node[1] = 7
        return root
    )");
    EXPECT_EQ(std::count(result.begin(), result.end(), '['), 31);
    EXPECT_NE(result.find('7'), std::string::npos);
}
