#include "test_lua_runner.h"
#include "test_ui_fixture.h"

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

// DESC-07: the C++ core's enum/unit/hidden/label/header rendering (waves 1-3) reaches Lua through
// sol2 byte-for-byte. `TEST(...)` rather than `TEST_F(LuaRunnerTest, ...)` -- LuaRunnerTest's
// system-temp-directory sandbox is the shape D-28 rules out here; the sidecar must sit beside the
// db file inside the shared tests/schemas/ui/<fixture>/ directory. Every literal below is quoted
// verbatim from tests/schemas/ui/README.md's `## Rendered literals` table (L1, L3, L6, L9, L10).
// All assertions use string.find(..., 1, true) -- plain-find mode -- because the strings contain
// `{`, `}`, `(`, `)` and `-`, all Lua pattern metacharacters; a pattern-mode assertion would match
// strings the renderer never produced.

TEST(LuaRunnerDescribe, DescribeCollectionRendersDeclaredVocabulary) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "lua_declared");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:describe_collection("Storage")
        assert(report:find("enum bool {0: Disabled, 1: Enabled}", 1, true), "missing declared vocabulary list")
    )LUA");
}

TEST(LuaRunnerDescribe, SummarizeCollectionRendersEnumLabels) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "lua_histogram");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        for i = 1, 8 do
            db:create_element("Storage", { label = "disabled_" .. i, has_commitment = 0 })
        end
        for i = 1, 4 do
            db:create_element("Storage", { label = "enabled_" .. i, has_commitment = 1 })
        end
        local report = db:summarize_collection("Storage")
        assert(report:find("values {0: 8 (Disabled), 1: 4 (Enabled)}", 1, true), "bad enum histogram")
    )LUA");
}

TEST(LuaRunnerDescribe, DescribeRendersHeaderAndCollectionLabel) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "lua_header");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:describe()
        assert(report:find("UI config: ", 1, true), "missing UI config header")
        assert(report:find(" (locale: en)", 1, true), "missing locale suffix")
        assert(report:find("Storage Units", 1, true), "missing collection label")
    )LUA");
}

TEST(LuaRunnerDescribe, HiddenAndUnitRenderedInLua) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "lua_hidden_unit");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:describe_collection("Storage")
        assert(report:find("[hidden]", 1, true), "missing [hidden] tag")
        assert(report:find("[MW]", 1, true), "missing [MW] unit")
    )LUA");
}

// The assertion the C++ core suite cannot substitute for: Lua strings are byte strings and sol2
// copies them as such, so this is a real boundary check, not a formality. L9 resolves via its own
// label.en; L10 resolves via the first-key-in-map-order fallback leg -- neither depends on a
// locale Phase 1 never resolves. Written as C++ hex-byte escapes (never a raw non-ASCII source
// character, matching src/database_describe.cpp's kEmDash precedent) and spliced into the script
// text, because a raw string literal (R"LUA(...)LUA") cannot itself contain a C++ escape.
TEST(LuaRunnerDescribe, NonAsciiLabelSurvivesSol2) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "foresight_like", "lua_unicode");
    quiver::LuaRunner lua(db);

    const std::string seasonal_naive = "Seasonal Na\xC3\xAFve";
    const std::string regresion_lineal = "Regresi\xC3\xB3n Lineal";
    const std::string script = "local report = db:describe_collection(\"EconomicDriver\")\n"
                                "assert(report:find(\"" +
                                seasonal_naive + "\", 1, true), \"missing Seasonal Naive label\")\n" +
                                "assert(report:find(\"" + regresion_lineal +
                                "\", 1, true), \"missing Regresion Lineal label\")\n";
    lua.run(script);
}

TEST(LuaRunnerDescribe, NoHeaderWithoutSidecarInLua) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_ui_dir", "lua_no_sidecar");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(
        local report = db:describe()
        assert(not report:find("UI config: ", 1, true), "should not have UI config header")
    )LUA");
}
