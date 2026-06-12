#include "test_utils.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/lua_runner.h>
#include <string>

namespace fs = std::filesystem;

// Lua bindings for the expression subsystem (operators, unary math, ifelse, aggregate*,
// select/rename_agents, save). Mirrors bindings/julia/test/test_expression.jl.
class LuaExpressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        schema = VALID_SCHEMA("collections.sql");
        path_a = (fs::temp_directory_path() / "quiver_lua_expr_a").string();
        path_b = (fs::temp_directory_path() / "quiver_lua_expr_b").string();
        path_c = (fs::temp_directory_path() / "quiver_lua_expr_c").string();
        path_out = (fs::temp_directory_path() / "quiver_lua_expr_out").string();
        cleanup();
    }
    void TearDown() override { cleanup(); }

    void cleanup() {
        for (const auto& p : {path_a, path_b, path_c, path_out}) {
            for (auto ext : {".qvr", ".toml"}) {
                auto full = p + ext;
                if (fs::exists(full))
                    fs::remove(full);
            }
        }
    }

    static std::string lp(const std::string& p) {
        std::string r = p;
        std::replace(r.begin(), r.end(), '\\', '/');
        return r;
    }

    // Shared Lua helpers: a 3x2 (row,col) metadata with labels {v1,v2}, a constant fill, and a
    // per-row fill (value == row) for aggregation tests.
    static std::string prelude() {
        return R"(
            local function make_md()
                return quiver.metadata{
                    initial_datetime = "2025-01-01T00:00:00", unit = "MW",
                    labels = {"v1", "v2"}, dimensions = {"row", "col"}, dimension_sizes = {3, 2},
                }
            end
            local function fill(path, va, vb)
                local f = quiver.open_file(path, "w", make_md())
                for row = 1, 3 do for col = 1, 2 do f:write({va, vb}, {row = row, col = col}) end end
                f:close()
            end
            local function fill_by_row(path)
                local f = quiver.open_file(path, "w", make_md())
                for row = 1, 3 do for col = 1, 2 do f:write({row, row}, {row = row, col = col}) end end
                f:close()
            end
        )";
    }

    std::string schema, path_a, path_b, path_c, path_out;
};

TEST_F(LuaExpressionTest, ArithmeticAndSave) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), b = lp(path_b), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 3.0, 3.0)\n"
            "fill('" +
            b +
            "', 4.0, 4.0)\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local fb = quiver.open_file('" +
            b +
            "', 'r')\n"
            "local expr = (quiver.expression(fa) + fb) * 2.0\n"  // (3+4)*2 = 14
            "expr:save('" +
            out +
            "')\n"
            "fa:close(); fb:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "for row=1,3 do for col=1,2 do\n"
            "  local cell = r:read({row=row, col=col})\n"
            "  assert(cell[1] == 14.0 and cell[2] == 14.0, 'value at '..row..','..col)\n"
            "end end\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, ScalarOnEitherSide) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 5.0, 5.0)\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local expr = 2.0 * quiver.expression(fa) - 1.0\n"  // 2*5 - 1 = 9
            "expr:save('" +
            out +
            "')\n"
            "fa:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "assert(r:read({row=1, col=1})[1] == 9.0, 'scalar both sides')\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, FilePlusFile) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), b = lp(path_b), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 1.0, 1.0)\n"
            "fill('" +
            b +
            "', 2.0, 2.0)\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local fb = quiver.open_file('" +
            b +
            "', 'r')\n"
            "local expr = fa + fb\n"  // BinaryFile + BinaryFile metamethod
            "expr:save('" +
            out +
            "')\n"
            "fa:close(); fb:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "assert(r:read({row=2, col=1})[1] == 3.0, 'file + file')\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, UnaryMath) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 4.0, 9.0)\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local expr = quiver.sqrt(quiver.expression(fa))\n"  // sqrt(4)=2, sqrt(9)=3
            "expr:save('" +
            out +
            "')\n"
            "fa:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "local cell = r:read({row=1, col=1})\n"
            "assert(cell[1] == 2.0 and cell[2] == 3.0, 'sqrt')\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, IfElse) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), b = lp(path_b), c = lp(path_c), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 1.0, 0.0)\n"  // condition: v1 true, v2 false
            "fill('" +
            b +
            "', 10.0, 10.0)\n"  // then
            "fill('" +
            c +
            "', 20.0, 20.0)\n"  // else
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local fb = quiver.open_file('" +
            b +
            "', 'r')\n"
            "local fc = quiver.open_file('" +
            c +
            "', 'r')\n"
            "local expr = quiver.ifelse(fa, fb, fc)\n"
            "expr:save('" +
            out +
            "')\n"
            "fa:close(); fb:close(); fc:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "local cell = r:read({row=1, col=1})\n"
            "assert(cell[1] == 10.0, 'ifelse true -> then')\n"
            "assert(cell[2] == 20.0, 'ifelse false -> else')\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, AggregateDimensionSum) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), out = lp(path_out);
    lua.run(prelude() + "fill_by_row('" + a +
            "')\n"  // value == row
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local agg = quiver.expression(fa):aggregate('row', 'sum')\n"  // 1+2+3 = 6
            "agg:save('" +
            out +
            "')\n"
            "fa:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "local md = r:get_metadata()\n"
            "local dims = md:get_dimensions()\n"
            "assert(#dims == 1 and dims[1].name == 'col', 'row dimension collapsed')\n"
            "for col=1,2 do assert(r:read({col=col})[1] == 6.0, 'sum at col '..col) end\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, AggregateDimensionPercentile) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), out = lp(path_out);
    lua.run(prelude() + "fill_by_row('" + a +
            "')\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local agg = quiver.expression(fa):aggregate('row', 'percentile', 0.5)\n"  // median{1,2,3}=2
            "agg:save('" +
            out +
            "')\n"
            "fa:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "assert(r:read({col=1})[1] == 2.0, 'median')\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, AggregateUnknownOpThrows) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    EXPECT_THROW(lua.run(prelude() + "fill_by_row('" + a +
                         "')\n"
                         "local fa = quiver.open_file('" +
                         a +
                         "', 'r')\n"
                         "quiver.expression(fa):aggregate('row', 'bogus')\n"),
                 std::exception);
}

TEST_F(LuaExpressionTest, AggregateAgentsMean) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 10.0, 20.0)\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local agg = quiver.expression(fa):aggregate_agents('mean')\n"  // mean(10,20)=15
            "agg:save('" +
            out +
            "')\n"
            "fa:close()\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "local labels = r:get_metadata():get_labels()\n"
            "assert(#labels == 1 and labels[1] == 'mean', 'collapsed to single mean label')\n"
            "assert(r:read({row=1, col=1})[1] == 15.0, 'mean value')\n"
            "r:close()\n");
}

TEST_F(LuaExpressionTest, SelectAndRenameAgents) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a), out = lp(path_out);
    lua.run(prelude() + "fill('" + a +
            "', 10.0, 20.0)\n"
            "local fa = quiver.open_file('" +
            a +
            "', 'r')\n"
            "local sel = quiver.expression(fa):select_agents({'v2'})\n"
            "sel:save('" +
            out +
            "')\n"
            "local r = quiver.open_file('" +
            out +
            "', 'r')\n"
            "local labels = r:get_metadata():get_labels()\n"
            "assert(#labels == 1 and labels[1] == 'v2', 'selected label')\n"
            "assert(r:read({row=1, col=1})[1] == 20.0, 'selected value')\n"
            "r:close()\n"
            "local ren = quiver.expression(fa):rename_agents({v1 = 'alpha'})\n"
            "local rlabels = ren:metadata():get_labels()\n"
            "assert(#rlabels == 2 and rlabels[1] == 'alpha' and rlabels[2] == 'v2', 'renamed labels')\n"
            "fa:close()\n");
}

TEST_F(LuaExpressionTest, SaveOutputCollisionThrows) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    EXPECT_THROW(lua.run(prelude() + "fill('" + a +
                         "', 1.0, 1.0)\n"
                         "local fa = quiver.open_file('" +
                         a +
                         "', 'r')\n"
                         "local expr = quiver.expression(fa) * 2.0\n"
                         "expr:save('" +
                         a + "')\n"),  // output collides with input
                 std::exception);
}
