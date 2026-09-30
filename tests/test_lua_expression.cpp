#include "test_lua_runner.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/lua_runner.h>
#include <string>

namespace fs = std::filesystem;

// Lua bindings for the expression subsystem (operators, unary math, ifelse, aggregate*,
// select/rename_agents, save). Mirrors bindings/julia/test/test_expression.jl. Files live in the
// db-directory sandbox (relative paths resolve against the database directory).
class LuaExpressionTest : public LuaSandboxTest {
protected:
    void SetUp() override {
        LuaSandboxTest::SetUp();
        schema = VALID_SCHEMA("collections.sql");
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
                local f = db:open_file(path, "w", make_md())
                for row = 1, 3 do for col = 1, 2 do f:write({va, vb}, {row = row, col = col}) end end
                f:close()
            end
            local function fill_by_row(path)
                local f = db:open_file(path, "w", make_md())
                for row = 1, 3 do for col = 1, 2 do f:write({row, row}, {row = row, col = col}) end end
                f:close()
            end
        )";
    }

    std::string schema;
};

TEST_F(LuaExpressionTest, ArithmeticAndSave) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 3.0, 3.0)
        fill('expr_b', 4.0, 4.0)
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')
        local expr = (quiver.expression(fa) + fb) * 2.0
        expr:save('expr_out')
        fa:close(); fb:close()
        local r = db:open_file('expr_out', 'r')
        for row=1,3 do for col=1,2 do
          local cell = r:read({row=row, col=col})
          assert(cell[1] == 14.0 and cell[2] == 14.0, 'value at '..row..','..col)
        end end
        r:close()
    )");  // (3+4)*2 = 14
}

TEST_F(LuaExpressionTest, ScalarOnEitherSide) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 5.0, 5.0)
        local fa = db:open_file('expr_a', 'r')
        local expr = 2.0 * quiver.expression(fa) - 1.0
        expr:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        assert(r:read({row=1, col=1})[1] == 9.0, 'scalar both sides')
        r:close()
    )");  // 2*5 - 1 = 9
}

TEST_F(LuaExpressionTest, FilePlusFile) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 1.0, 1.0)
        fill('expr_b', 2.0, 2.0)
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')
        local expr = fa + fb
        expr:save('expr_out')
        fa:close(); fb:close()
        local r = db:open_file('expr_out', 'r')
        assert(r:read({row=2, col=1})[1] == 3.0, 'file + file')
        r:close()
    )");  // BinaryFile + BinaryFile metamethod
}

TEST_F(LuaExpressionTest, UnaryMath) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 4.0, 9.0)
        local fa = db:open_file('expr_a', 'r')
        local expr = quiver.sqrt(quiver.expression(fa))
        expr:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        local cell = r:read({row=1, col=1})
        assert(cell[1] == 2.0 and cell[2] == 3.0, 'sqrt')
        r:close()
    )");  // sqrt(4)=2, sqrt(9)=3
}

TEST_F(LuaExpressionTest, IfElse) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 1.0, 0.0)
        fill('expr_b', 10.0, 10.0)
        fill('expr_c', 20.0, 20.0)
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')
        local fc = db:open_file('expr_c', 'r')
        local expr = quiver.ifelse(fa, fb, fc)
        expr:save('expr_out')
        fa:close(); fb:close(); fc:close()
        local r = db:open_file('expr_out', 'r')
        local cell = r:read({row=1, col=1})
        assert(cell[1] == 10.0, 'ifelse true -> then')
        assert(cell[2] == 20.0, 'ifelse false -> else')
        r:close()
    )");  // condition: v1 true, v2 false
}

TEST_F(LuaExpressionTest, AggregateDimensionSum) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill_by_row('expr_a')
        local fa = db:open_file('expr_a', 'r')
        local agg = quiver.expression(fa):aggregate('row', 'sum')
        agg:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        local md = r:get_metadata()
        local dims = md:get_dimensions()
        assert(#dims == 1 and dims[1].name == 'col', 'row dimension collapsed')
        for col=1,2 do assert(r:read({col=col})[1] == 6.0, 'sum at col '..col) end
        r:close()
    )");  // value == row, so 1+2+3 = 6
}

TEST_F(LuaExpressionTest, AggregateDimensionPercentile) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill_by_row('expr_a')
        local fa = db:open_file('expr_a', 'r')
        local agg = quiver.expression(fa):aggregate('row', 'percentile', 0.5)
        agg:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        assert(r:read({col=1})[1] == 2.0, 'median')
        r:close()
    )");  // median{1,2,3} = 2
}

TEST_F(LuaExpressionTest, AggregateSumOverInnermostTimeDimFromMidPeriodStart) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    // year x month x day from 2025-03-15: only March 2025 starts on the 15th, so March 2026 sums
    // all 31 days.
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-03-15T00:00:00', unit='MW',
            labels={'v'}, dimensions={'year','month','day'}, dimension_sizes={2,12,31},
            time_dimensions={'year','month','day'}, frequencies={'yearly','monthly','daily'} }
        local f = db:open_file('expr_a', 'w', md)
        for day=15,31 do f:write({1.0}, {year=1, month=3, day=day}) end
        for day=1,31 do f:write({1.0}, {year=2, month=3, day=day}) end
        f:close()
        local fa = db:open_file('expr_a', 'r')
        local agg = quiver.expression(fa):aggregate('day', 'sum')
        agg:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        assert(r:read({year=1, month=3})[1] == 17.0, 'March 2025 starts on the 15th')
        assert(r:read({year=2, month=3})[1] == 31.0, 'March 2026 is a whole month')
        r:close()
    )");
}

TEST_F(LuaExpressionTest, AggregateUnknownOpThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua,
                     prelude() + R"(
        fill_by_row('expr_a')
        local fa = db:open_file('expr_a', 'r')
        quiver.expression(fa):aggregate('row', 'bogus')
    )",
                     "Cannot aggregate: unknown operation 'bogus'");
}

TEST_F(LuaExpressionTest, AggregateAgentsUnknownOpThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua,
                     prelude() + R"(
        fill_by_row('expr_a')
        local fa = db:open_file('expr_a', 'r')
        quiver.expression(fa):aggregate_agents('bogus')
    )",
                     "Cannot aggregate_agents: unknown operation 'bogus'");
}

TEST_F(LuaExpressionTest, AggregateOutermostTimeDimFromMidYearStart) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    // year x month from 2025-03-01 holds 2025-03..2026-12. Reducing 'year' makes month outermost;
    // output month m must be calendar month m, in memory and after a reopen.
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-03-01T00:00:00', unit='MW', labels={'v'},
            dimensions={'year','month'}, dimension_sizes={2,12},
            time_dimensions={'year','month'}, frequencies={'yearly','monthly'} }
        local f = db:open_file('expr_a', 'w', md)
        for year=1,2 do for month=1,12 do
            if year == 2 or month >= 3 then f:write({100 * year + month}, {year=year, month=month}) end
        end end
        f:close()
        local fa = db:open_file('expr_a', 'r')
        local agg = quiver.expression(fa):aggregate('year', 'sum')
        local start = agg:metadata():get_initial_datetime()
        assert(start == '2025-01-01T00:00:00', 'output starts at the first reduced period, got ' .. start)
        assert(agg:metadata():get_dimensions()[1].initial_value == 1, 'month starts at 1')
        agg:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        assert(r:get_metadata():get_initial_datetime() == '2025-01-01T00:00:00', 'saved start')
        assert(r:read({month=1}, true)[1] == 201, 'Jan: 2026 only')
        assert(r:read({month=3}, true)[1] == 306, 'Mar: 103 + 203')
        assert(r:read({month=12}, true)[1] == 324, 'Dec: 112 + 212')
        r:close()
    )");
}

TEST_F(LuaExpressionTest, AggregateAgentsMean) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 10.0, 20.0)
        local fa = db:open_file('expr_a', 'r')
        local agg = quiver.expression(fa):aggregate_agents('mean')
        agg:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        local labels = r:get_metadata():get_labels()
        assert(#labels == 1 and labels[1] == 'mean', 'collapsed to single mean label')
        assert(r:read({row=1, col=1})[1] == 15.0, 'mean value')
        r:close()
    )");  // mean(10,20) = 15
}

TEST_F(LuaExpressionTest, AggregateAgentsMaxMinusMin) {
    // aggregate_agents names its one label after the operation, so this subtracts a {'min'} operand
    // from a {'max'} one. Single labels broadcast whatever they are called; the lhs label is kept.
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 10.0, 25.0)
        local fa = db:open_file('expr_a', 'r')
        local spread = quiver.expression(fa):aggregate_agents('max') - quiver.expression(fa):aggregate_agents('min')
        spread:save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        local labels = r:get_metadata():get_labels()
        assert(#labels == 1 and labels[1] == 'max', 'lhs label kept')
        assert(r:read({row=2, col=1})[1] == 15.0, 'max - min')
        r:close()
    )");  // max(10, 25) - min(10, 25) = 15
}

TEST_F(LuaExpressionTest, SelectAndRenameAgents) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 10.0, 20.0)
        local fa = db:open_file('expr_a', 'r')
        local sel = quiver.expression(fa):select_agents({'v2'})
        sel:save('expr_out')
        local r = db:open_file('expr_out', 'r')
        local labels = r:get_metadata():get_labels()
        assert(#labels == 1 and labels[1] == 'v2', 'selected label')
        assert(r:read({row=1, col=1})[1] == 20.0, 'selected value')
        r:close()
        local ren = quiver.expression(fa):rename_agents({v1 = 'alpha'})
        local rlabels = ren:metadata():get_labels()
        assert(#rlabels == 2 and rlabels[1] == 'alpha' and rlabels[2] == 'v2', 'renamed labels')
        fa:close()
    )");
}

// Both halves of each mapping entry used to go through unchecked as<std::string>(): in Release a
// boolean target became "" (the rename succeeded) and a number key became the text "1".
TEST_F(LuaExpressionTest, RenameAgentsRejectsNonStringNames) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + "fill('expr_a', 1.0, 1.0)");
    expect_lua_error(lua,
                     "quiver.expression(db:open_file('expr_a', 'r')):rename_agents({ v1 = true })",
                     "Cannot rename_agents: value for 'v1' has unsupported Lua type");
    expect_lua_error(lua,
                     "quiver.expression(db:open_file('expr_a', 'r')):rename_agents({ 'alpha' })",
                     "Cannot rename_agents: key has unsupported Lua type");
    expect_lua_error(lua,
                     "quiver.expression(db:open_file('expr_a', 'r')):rename_agents(5)",
                     "Cannot rename_agents: mapping must be a table");
}

TEST_F(LuaExpressionTest, SaveOutputCollisionThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua,
                     prelude() + R"(
        fill('expr_a', 1.0, 1.0)
        local fa = db:open_file('expr_a', 'r')
        local expr = quiver.expression(fa) * 2.0
        expr:save('expr_a')
    )",
                     "Cannot save: output path collides with input file");
}

// --- db-directory sandbox ---

TEST_F(LuaExpressionTest, SaveRelativeResolvesAgainstDbDir) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 1.0, 1.0)
        local fa = db:open_file('expr_a', 'r')
        local expr = quiver.expression(fa) * 2.0
        expr:save('out_rel')
        fa:close()
    )");
    EXPECT_TRUE(fs::exists(sandbox / "out_rel.qvr"));
}

TEST_F(LuaExpressionTest, SaveEscapeThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua,
                     prelude() + R"(
        fill('expr_a', 1.0, 1.0)
        local fa = db:open_file('expr_a', 'r')
        local expr = quiver.expression(fa) * 2.0
        expr:save('../out')
    )",
                     "Cannot save: path '../out' escapes the database directory");
}

TEST_F(LuaExpressionTest, ComparisonFreeFunctions) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 5.0, 1.0)
        fill('expr_b', 3.0, 3.0)
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')
        -- gt between two files: 5>3 -> 1, 1>3 -> 0
        quiver.gt(fa, fb):save('expr_out')
        local r = db:open_file('expr_out', 'r')
        local cell = r:read({row=1, col=1})
        assert(cell[1] == 1.0 and cell[2] == 0.0, 'gt file/file')
        r:close()
        -- lte with a scalar on the right: 5<=4 -> 0, 1<=4 -> 1
        quiver.lte(quiver.expression(fa), 4.0):save('expr_out2')
        local r2 = db:open_file('expr_out2', 'r')
        local c2 = r2:read({row=1, col=1})
        assert(c2[1] == 0.0 and c2[2] == 1.0, 'lte expr/scalar')
        r2:close()
        fa:close(); fb:close()
    )");
}

TEST_F(LuaExpressionTest, ComparisonDrivesIfElse) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 5.0, 1.0)
        fill('expr_b', 10.0, 10.0)
        fill('expr_c', 20.0, 20.0)
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')
        local fc = db:open_file('expr_c', 'r')
        -- where a > 3 pick then(10) else else(20): v1 5>3 -> 10, v2 1>3 -> 20
        local expr = quiver.ifelse(quiver.gt(quiver.expression(fa), 3.0), fb, fc)
        expr:save('expr_out')
        fa:close(); fb:close(); fc:close()
        local r = db:open_file('expr_out', 'r')
        local cell = r:read({row=1, col=1})
        assert(cell[1] == 10.0 and cell[2] == 20.0, 'comparison drives ifelse')
        r:close()
    )");
}

TEST_F(LuaExpressionTest, ComparisonPropagatesNaN) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        -- 0/0 produces NaN in the file's first label
        local f = db:open_file('expr_a', 'w', make_md())
        for row=1,3 do for col=1,2 do f:write({0/0, 5.0}, {row=row, col=col}) end end
        f:close()
        local fa = db:open_file('expr_a', 'r')
        quiver.gt(quiver.expression(fa), 3.0):save('expr_out')
        fa:close()
        local r = db:open_file('expr_out', 'r')
        local cell = r:read({row=1, col=1}, true)  -- allow NaN
        assert(cell[1] ~= cell[1], 'NaN operand propagates (NaN ~= NaN)')
        assert(cell[2] == 1.0, '5 > 3 -> 1')
        r:close()
    )");
}

TEST_F(LuaExpressionTest, LogicalOperators) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 1.0, 0.0)   -- v1 true, v2 false
        fill('expr_b', 1.0, 1.0)   -- both true
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')

        local e_and = fa & fb; e_and:save('expr_and')
        local ra = db:open_file('expr_and', 'r'); local ca = ra:read({row=1, col=1})
        assert(ca[1] == 1.0 and ca[2] == 0.0, '&: true&true=1, false&true=0'); ra:close()

        local e_or = fa | fb; e_or:save('expr_or')
        local ro = db:open_file('expr_or', 'r'); local co = ro:read({row=1, col=1})
        assert(co[1] == 1.0 and co[2] == 1.0, '|: true|true=1, false|true=1'); ro:close()

        local e_not = ~fa; e_not:save('expr_not')
        local rn = db:open_file('expr_not', 'r'); local cn = rn:read({row=1, col=1})
        assert(cn[1] == 0.0 and cn[2] == 1.0, '~: ~true=0, ~false=1'); rn:close()

        fa:close(); fb:close()
    )");
}

TEST_F(LuaExpressionTest, LogicalComposesIfElse) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 5.0, 0.5)    -- value to test against a range
        fill('expr_b', 10.0, 10.0)  -- then
        fill('expr_c', 20.0, 20.0)  -- else
        local fa = db:open_file('expr_a', 'r')
        local fb = db:open_file('expr_b', 'r')
        local fc = db:open_file('expr_c', 'r')
        -- pick "then" where (a > 1) and not(a > 8): v1 5 -> then(10), v2 0.5 -> else(20)
        local cond = quiver.gt(quiver.expression(fa), 1.0) & ~quiver.gt(quiver.expression(fa), 8.0)
        quiver.ifelse(cond, fb, fc):save('expr_out')
        fa:close(); fb:close(); fc:close()
        local r = db:open_file('expr_out', 'r')
        local cell = r:read({row=1, col=1})
        assert(cell[1] == 10.0, 'v1 in (1,8] -> then')
        assert(cell[2] == 20.0, 'v2 not in range -> else')
        r:close()
    )");
}

// Every operator metamethod on both usertypes. Each usertype is the left operand against a number,
// so Lua (which tries the left operand's metamethod first) cannot fall back to the other one's.
TEST_F(LuaExpressionTest, OperatorMetamethodsOnFileAndExpression) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(prelude() + R"(
        fill('expr_a', 6.0, 0.0)
        local fa = db:open_file('expr_a', 'r')
        local n = 0
        local function check(kind, expr, v1, v2, op)
            n = n + 1
            local path = 'expr_out' .. n
            expr:save(path)
            local r = db:open_file(path, 'r')
            local cell = r:read({row = 1, col = 1})
            r:close()
            assert(cell[1] == v1 and cell[2] == v2, op .. ' on ' .. kind)
        end
        for kind, x in pairs({file = fa, expression = quiver.expression(fa)}) do
            check(kind, x + 2.0, 8.0, 2.0, '+')
            check(kind, x - 2.0, 4.0, -2.0, '-')
            check(kind, x * 2.0, 12.0, 0.0, '*')
            check(kind, x / 2.0, 3.0, 0.0, '/')
            check(kind, -x, -6.0, 0.0, 'unary -')
            check(kind, x & 1.0, 1.0, 0.0, '&')
            check(kind, x | 0.0, 1.0, 0.0, '|')
            check(kind, ~x, 0.0, 1.0, '~')
        end
        fa:close()
    )");
}
