#include "test_lua_runner.h"

namespace {
quiver::Database open_collections() {
    return quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
}
}  // namespace

TEST_F(LuaRunnerTest, DescribeReport) {
    auto db = open_collections();
    db.create_element("Collection", quiver::Element().set("label", "a"));
    db.create_element("Collection", quiver::Element().set("label", "b"));

    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:describe()
        assert(type(report) == "string", "describe should return a string")
        assert(report:find("Collection: Configuration"), "missing Configuration")
        assert(report:find("Collection: Collection %(2 elements%)"), "missing Collection w/ count")
    )LUA");
}

TEST_F(LuaRunnerTest, DescribeCollection) {
    auto db = open_collections();
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:describe_collection("Collection")
        assert(type(report) == "string", "describe_collection should return a string")
        assert(report:find("Collection: Collection"), "missing header")
        assert(report:find("%[date_time%]"), "missing time-series dimension column")
    )LUA");
}

TEST_F(LuaRunnerTest, UiMetadataGettersReturnEmpty) {
    // The Lua-provided db never went through from_hub, so UI metadata is empty (in-memory
    // only). The getters are still bound for cross-layer uniformity and return "".
    auto db = open_collections();
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        assert(db:get_model_name() == "", "model name should be empty")
        assert(db:get_attribute_unit("Collection", "value") == "", "unit should be empty")
    )LUA");
}

TEST_F(LuaRunnerTest, SummarizeCollection) {
    auto db = open_collections();
    db.create_element("Collection",
                      quiver::Element()
                          .set("label", "a")
                          .set("some_integer", static_cast<int64_t>(1))
                          .set("value_int", std::vector<int64_t>{10, 20}));
    db.create_element("Collection", quiver::Element().set("label", "b").set("some_integer", static_cast<int64_t>(1)));
    db.create_element("Collection", quiver::Element().set("label", "c").set("some_integer", static_cast<int64_t>(5)));

    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:summarize_collection("Collection")
        assert(type(report) == "string", "summarize_collection should return a string")
        assert(report:find("some_integer: 3 non%-null, 0 null; values {1: 2, 5: 1}"), "bad some_integer stats")
        assert(report:find("values: 1/3 non%-empty"), "bad vector group stats")
    )LUA");
}
