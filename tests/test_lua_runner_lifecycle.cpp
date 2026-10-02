#include "test_lua_runner.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

static_assert(
    sizeof(quiver::LuaRunner) == sizeof(void*),
    "LuaRunner must hold only its heap Impl: run state outside Impl dangles after a move"
);

// A move hands the heap Impl over whole, so the moved-to runner's bindings still reach the registries
// that close every CSV writer and binary file at run() exit.
class LuaRunner_Lifecycle : public LuaSandboxTest {
protected:
    void SetUp() override {
        LuaSandboxTest::SetUp();
        schema = VALID_SCHEMA("collections.sql");
    }

    // Leaves a CSV writer and a binary writer open in globals, so only run()'s exit can close them.
    static std::string open_handles(const std::string& name) {
        return "local NAME = '" + name + "'\n" + R"(
            local md = quiver.metadata{ initial_datetime = '2025-01-01T00:00:00', unit = 'x',
                labels = {'v'}, dimensions = {'row'}, dimension_sizes = {1} }
            origin = NAME
            w = db:write_csv(NAME .. '.csv')
            w:write_row({ 'x' })
            g = db:open_file(NAME, 'w', md)
            g:write({ 1.0 }, { row = 1 })
        )";
    }

    // Run after open_handles(name): both handles were closed (and flushed) when that run returned.
    static std::string expect_handles_closed(const std::string& name) {
        return "local NAME = '" + name + "'\n" + R"(
            assert(not g:is_open(), 'binary handle outlived its run()')
            local csv = db:read_csv(NAME .. '.csv', { header_row = 0 })
            assert(#csv.rows == 1 and csv.rows[1][1] == 'x', 'csv writer was not closed at run() exit')
            local r = db:open_file(NAME, 'r')
            assert(r:read({ row = 1 })[1] == 1.0, 'binary writer was not flushed')
            local doubled = quiver.expression(r) * 2.0
            doubled:save(NAME .. '_doubled')
            r:close()
        )";
    }

    std::string schema;
};

TEST_F(LuaRunner_Lifecycle, MoveConstructor) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner source(db);
    source.run(open_handles("first"));

    // `source` stays alive to the end, so a run() that closes through a different object than the bindings
    // register into fails the checks below instead of dangling. State reached only through the moved-from
    // runner still works while `source` lives; the OutlivesSource pins and the static_assert catch that.
    quiver::LuaRunner moved = std::move(source);
    moved.run("assert(origin == 'first', 'moved-to runner lost the source Lua state')");
    moved.run(open_handles("second"));
    moved.run(expect_handles_closed("second"));
    EXPECT_TRUE(std::filesystem::exists(sandbox / "second_doubled.qvr"));
}

TEST_F(LuaRunner_Lifecycle, MoveAssignment) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner source(db);
    source.run(open_handles("first"));
    quiver::LuaRunner target(db);
    target.run(open_handles("target"));  // destroyed with live globals by the assignment below

    target = std::move(source);
    target.run("assert(origin == 'first', 'moved-to runner lost the source Lua state')");
    target.run(open_handles("second"));
    target.run(expect_handles_closed("second"));
    EXPECT_TRUE(std::filesystem::exists(sandbox / "second_doubled.qvr"));
}

// The source is freed before the moved-to runner runs again, so run state still reached through the
// moved-from runner dangles instead of silently working.
TEST_F(LuaRunner_Lifecycle, MoveConstructorOutlivesSource) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    auto source = std::make_unique<quiver::LuaRunner>(db);
    source->run(open_handles("first"));

    quiver::LuaRunner moved = std::move(*source);
    source.reset();
    moved.run("assert(origin == 'first', 'moved-to runner lost the source Lua state')");
    moved.run(open_handles("second"));
    moved.run(expect_handles_closed("second"));
    EXPECT_TRUE(std::filesystem::exists(sandbox / "second_doubled.qvr"));
}

TEST_F(LuaRunner_Lifecycle, MoveAssignmentOutlivesSource) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    auto source = std::make_unique<quiver::LuaRunner>(db);
    source->run(open_handles("first"));
    quiver::LuaRunner target(db);
    target.run(open_handles("target"));

    target = std::move(*source);
    source.reset();
    target.run("assert(origin == 'first', 'moved-to runner lost the source Lua state')");
    target.run(open_handles("second"));
    target.run(expect_handles_closed("second"));
    EXPECT_TRUE(std::filesystem::exists(sandbox / "second_doubled.qvr"));
}
