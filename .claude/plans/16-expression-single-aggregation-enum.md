# 16 — Expressions: one AggregationOperation enum across C++, C API, Lua and Julia

**Batch** 2 · **Severity** low · **Breaking** yes, for C and Julia callers of `aggregate_agents`: the `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_*` constants and the `quiver_expression_aggregate_agents_operation_t` type are removed. C++ source compiles unchanged, and Lua is unaffected because it takes strings. · **Size** S · **Layers** C++ core (expression), C API, Julia (FFI regen + wrapper + tests), Lua (parser + test)
**Depends on** none · **Overlaps with** 15 (edits other functions in `src/expression/expression_helpers.h` and other bullets of the src/AGENTS.md Expression section), 51 (edits `bind_expression` in `src/lua_runner.cpp` next to the lambdas this plan edits; its notes give the aggregation-parser half to this plan), 12 / 21 / 22 (each also regenerates `bindings/julia/src/c_api.jl` and adds a CHANGELOG 0.11.0 entry), 09 / 10 (edit `src/expression/expression_aggregate.cpp`, which this plan does **not** touch)

## Why

"Reduce with sum / mean / min / max / percentile" is one concept, but it is spelled as two identical types in every layer:

- C++, `include/quiver/expression/expression_node.h` (currently ~L138 and ~L165). Both are `enum class Operation { Sum, Mean, Min, Max, Percentile };`, one nested in `ExpressionAggregate` and one in `ExpressionAggregateAgents`.
- `src/expression/expression_helpers.h` (currently ~L272-303 and ~L379-429). `aggregation_operation_label`, `validate_aggregation_param`, `aggregation_accumulate` and `aggregation_finalize` are each `template <typename Op>`. The template exists only so the functions accept both enums: their only callers are `expression_aggregate.cpp` (~L23/124/129) and `expression_aggregate_agents.cpp` (~L19/23/47/49), one enum per file.
- C API, `include/quiver/c/expression/expression.h` (currently ~L47-63). There are two C enums with the same values 0-4, `quiver_expression_aggregate_operation_t` and `quiver_expression_aggregate_agents_operation_t`. `src/c/expression/expression.cpp` (~L74-104) has two `from_c` overloads that differ only in their types and in the `"Cannot aggregate:"` / `"Cannot aggregate_agents:"` prefix.
- Lua, `src/lua_runner.cpp` (~L1158-1184). There are two string parsers, `parse_aggregate_op` and `parse_aggregate_agents_op`, identical except for the return type and the message prefix.
- Julia, `bindings/julia/src/expression.jl` (~L192/209/227/235). The methods are typed on the two C enums, so callers need two families of constants for one idea.

Reproduction in Julia, the canonical binding:

```julia
e = Quiver.Expression(file)
Quiver.aggregate(e, "row", Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM)       # works
Quiver.aggregate_agents(e, Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN)      # MethodError:
#   no method matching aggregate_agents(::Quiver.Expression, ::Quiver.C.quiver_expression_aggregate_operation_t)
```

In C++, `expr.aggregate_agents(ExpressionAggregate::Operation::Mean)` does not compile, because there is no conversion between the two enum classes. The same happens in C++-compiled C API callers. `bindings/julia/test/test_expression.jl` (~L1274-1277) has to mix both families inside one expression.

The change applies the root principles "Simple solutions over complex abstractions" and "Delete unused code". The templates are an abstraction whose only job is to paper over the duplication. src/AGENTS.md (~L759) records the duplication ("The two aggregation enums are parallel types with identical values") but gives no reason for it. Commit `fa9ee9e` records none either.

## Constraints and decisions

- **Maintainer notes (binding):** "BREAKING (C/Julia constants). Keep per-operation Pattern 1 prefixes by threading the operation name into the single from_c and the single Lua parser. Plan 51 (Lua operator tables) touches nearby Lua code." The break is therefore limited to the C/Julia constants, and both merged converters take the calling operation's name.
- **What "one AggregationOperation enum" means here.** The single enum is the existing `quiver::ExpressionAggregate::Operation`. `ExpressionAggregateAgents` refers to it through `using Operation = ExpressionAggregate::Operation;`. On the C side the one enum is the existing `quiver_expression_aggregate_operation_t`, whose name already fits both functions. This is the policy verifier's corrected proposal, and it keeps the break exactly where the maintainer put it.
- **src/AGENTS.md (~L759):** "All operation enums are nested in their owning class". The alias keeps that convention: every node still has an `Operation` member type.
- **Root AGENTS.md, error patterns:** Pattern 1 is `"Cannot {operation}: {reason}"`, where `{operation}` is the public method the user called. Today's messages are `Cannot aggregate: unknown operation 'bogus'` (Lua), `Cannot aggregate_agents: unknown operation '<op>'` (Lua), and `Cannot aggregate[_agents]: unknown operation enum value` (C). They must stay byte-identical. `tests/test_lua_expression.cpp` (~L178) pins the first one.
- **Root design decision:** "Binary + expression subsystems are exposed in Julia and Lua only." Dart, Python and JS therefore have no FFI declarations to touch. The JS `lua-api.ts` reference documents string operations (`e:aggregate_agents("mean")`), which do not change.
- **Root "Changelog" rule:** the entry goes under the unreleased 0.11.0, prefixed **BREAKING**, and says what a caller must do. There is no manifest bump; 0.11.0 is already a minor bump.
- **bindings/julia/AGENTS.md:** "Regenerate after C API changes: `generator/generator.bat` rewrites `src/c_api.jl`". Never hand-edit it.

Rejected alternatives:
- *A new namespace-scope `quiver::AggregationOperation`, plus a renamed C enum.* This renames about 30 C++ test call sites, every Julia `AGGREGATE_OPERATION_*` site and every C `quiver_expression_aggregate` caller, for no behavioural gain. It also breaks the nested-enum convention and widens the break beyond the maintainer's "C/Julia constants".
- *No alias, spelling `ExpressionAggregate::Operation` in `Expression::aggregate_agents` and about 15 test sites.* This forces C++ callers to change with nothing gained. The alias is the node's own name for its operation type, not a deprecation shim.
- *Keeping two enums and templating the parser and `from_c`.* This was plan 51's original proposal, `template <typename Operation> parse_aggregation_op`. It keeps exactly the duplication this item removes.
- *One converter with a neutral message.* It would lose the public-method name that Pattern 1 requires.

## Changes

Anchor every edit on the quoted code, not on line numbers: plan 15 rewrites the broadcast half of `expression_helpers.h` before this plan runs, so lines will have moved.

### 1. `include/quiver/expression/expression_node.h`: one enum

In `class QUIVER_API ExpressionAggregateAgents` (currently ~L163-182), replace

```cpp
class QUIVER_API ExpressionAggregateAgents final : public ExpressionNode {
public:
    enum class Operation { Sum, Mean, Min, Max, Percentile };
```

with

```cpp
class QUIVER_API ExpressionAggregateAgents final : public ExpressionNode {
public:
    // The same five reductions as ExpressionAggregate: one enum serves both (and the C API and Lua).
    using Operation = ExpressionAggregate::Operation;
```

Nothing else in the file changes. `ExpressionAggregate` is declared above `ExpressionAggregateAgents`, so the alias resolves. The constructor, the `operation_` member, `Expression::aggregate_agents(ExpressionAggregateAgents::Operation, ...)` in `include/quiver/expression/expression.h` (~L31), `src/expression/expression.cpp` (~L32) and `operation_ == Operation::Percentile` in `src/expression/expression_aggregate_agents.cpp` (~L42) all compile unchanged, because they name the same type now. **Do not edit** `expression.h`, `expression.cpp`, `expression_aggregate.cpp` or `expression_aggregate_agents.cpp`.

### 2. `src/expression/expression_helpers.h`: the four templates become plain inline functions

These must be `inline`, since the header is included by several translation units. Without `inline` you get an LNK2005 or "multiple definition" link error.

2a. `aggregation_operation_label` (currently ~L272-287). Replace

```cpp
template <typename Op>
std::string aggregation_operation_label(Op op) {
    switch (op) {
    case Op::Sum:
        return "sum";
    case Op::Mean:
        return "mean";
    case Op::Min:
        return "min";
    case Op::Max:
        return "max";
    case Op::Percentile:
        return "percentile";
    }
    throw std::runtime_error("Cannot label aggregation: unhandled Operation variant");
}
```

with

```cpp
inline std::string aggregation_operation_label(ExpressionAggregate::Operation op) {
    switch (op) {
    case ExpressionAggregate::Operation::Sum:
        return "sum";
    case ExpressionAggregate::Operation::Mean:
        return "mean";
    case ExpressionAggregate::Operation::Min:
        return "min";
    case ExpressionAggregate::Operation::Max:
        return "max";
    case ExpressionAggregate::Operation::Percentile:
        return "percentile";
    }
    throw std::runtime_error("Cannot label aggregation: unhandled Operation variant");
}
```

2b. `validate_aggregation_param` (currently ~L289-303). Replace

```cpp
template <typename Op>
void validate_aggregation_param(Op op, std::optional<double> parameter, const std::string& fn_label) {
    const bool needs_param = (op == Op::Percentile);
```

with

```cpp
inline void validate_aggregation_param(ExpressionAggregate::Operation op,
                                       std::optional<double> parameter,
                                       const std::string& fn_label) {
    const bool needs_param = (op == ExpressionAggregate::Operation::Percentile);
```

The rest of the body, the three `throw`s, is unchanged. The one-line signature would be over the 120-column limit, which is why it is wrapped.

2c. `aggregation_accumulate` (currently ~L379-407). Replace

```cpp
// NaN inputs are skipped; Percentile collects into the caller's scratch buffer.
template <typename Op>
void aggregation_accumulate(Op op, AggregationState& state, std::vector<double>& percentile_scratch, double value) {
    if (std::isnan(value)) {
        return;
    }
    switch (op) {
    case Op::Sum:
    case Op::Mean:
        state.sum += value;
        ++state.count;
        break;
    case Op::Min:
        if (value < state.min) {
            state.min = value;
        }
        ++state.count;
        break;
    case Op::Max:
        if (value > state.max) {
            state.max = value;
        }
        ++state.count;
        break;
    case Op::Percentile:
        percentile_scratch.push_back(value);
        break;
    }
}
```

with

```cpp
// NaN inputs are skipped; Percentile collects into the caller's scratch buffer.
inline void aggregation_accumulate(ExpressionAggregate::Operation op,
                                   AggregationState& state,
                                   std::vector<double>& percentile_scratch,
                                   double value) {
    if (std::isnan(value)) {
        return;
    }
    switch (op) {
    case ExpressionAggregate::Operation::Sum:
    case ExpressionAggregate::Operation::Mean:
        state.sum += value;
        ++state.count;
        break;
    case ExpressionAggregate::Operation::Min:
        if (value < state.min) {
            state.min = value;
        }
        ++state.count;
        break;
    case ExpressionAggregate::Operation::Max:
        if (value > state.max) {
            state.max = value;
        }
        ++state.count;
        break;
    case ExpressionAggregate::Operation::Percentile:
        percentile_scratch.push_back(value);
        break;
    }
}
```

2d. `aggregation_finalize` (currently ~L409-429). Replace

```cpp
// An all-NaN (empty) accumulation yields NaN.
template <typename Op>
double aggregation_finalize(Op op,
                            const AggregationState& state,
                            std::vector<double>& percentile_scratch,
                            const std::optional<double>& parameter) {
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    switch (op) {
    case Op::Sum:
        return (state.count > 0) ? state.sum : nan_value;
    case Op::Mean:
        return (state.count > 0) ? state.sum / static_cast<double>(state.count) : nan_value;
    case Op::Min:
        return (state.count > 0) ? state.min : nan_value;
    case Op::Max:
        return (state.count > 0) ? state.max : nan_value;
    case Op::Percentile:
        return compute_percentile(percentile_scratch, *parameter);
    }
    return nan_value;
}
```

with

```cpp
// An all-NaN (empty) accumulation yields NaN.
inline double aggregation_finalize(ExpressionAggregate::Operation op,
                                   const AggregationState& state,
                                   std::vector<double>& percentile_scratch,
                                   const std::optional<double>& parameter) {
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    switch (op) {
    case ExpressionAggregate::Operation::Sum:
        return (state.count > 0) ? state.sum : nan_value;
    case ExpressionAggregate::Operation::Mean:
        return (state.count > 0) ? state.sum / static_cast<double>(state.count) : nan_value;
    case ExpressionAggregate::Operation::Min:
        return (state.count > 0) ? state.min : nan_value;
    case ExpressionAggregate::Operation::Max:
        return (state.count > 0) ? state.max : nan_value;
    case ExpressionAggregate::Operation::Percentile:
        return compute_percentile(percentile_scratch, *parameter);
    }
    return nan_value;
}
```

Leave the section banner above `struct AggregationState` as it is; "Aggregation accumulation (shared by ExpressionAggregate / ExpressionAggregateAgents)" is still accurate. The header already includes `quiver/expression/expression_node.h`, so `ExpressionAggregate` is visible and no include changes.

### 3. `include/quiver/c/expression/expression.h`: one C enum

Replace (currently ~L47-63)

```c
// Aggregate operation kind (dimension-axis reduction)
typedef enum {
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM = 0,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN = 1,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_MIN = 2,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_MAX = 3,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_PERCENTILE = 4,
} quiver_expression_aggregate_operation_t;

// Aggregate agents operation kind (label-axis reduction)
typedef enum {
    QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_SUM = 0,
    QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_MEAN = 1,
    QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_MIN = 2,
    QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_MAX = 3,
    QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_PERCENTILE = 4,
} quiver_expression_aggregate_agents_operation_t;
```

with

```c
// Aggregate operation kind, taken by both quiver_expression_aggregate (dimension-axis reduction)
// and quiver_expression_aggregate_agents (label-axis reduction)
typedef enum {
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM = 0,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN = 1,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_MIN = 2,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_MAX = 3,
    QUIVER_EXPRESSION_AGGREGATE_OPERATION_PERCENTILE = 4,
} quiver_expression_aggregate_operation_t;
```

In the `quiver_expression_aggregate_agents` prototype (currently ~L108-111), replace

```c
QUIVER_C_API quiver_error_t quiver_expression_aggregate_agents(quiver_expression_t* expression,
                                                               quiver_expression_aggregate_agents_operation_t operation,
```

with

```c
QUIVER_C_API quiver_error_t quiver_expression_aggregate_agents(quiver_expression_t* expression,
                                                               quiver_expression_aggregate_operation_t operation,
```

The ABI is unchanged: both enums were `int`-sized with identical values, so only source spellings break.

### 4. `src/c/expression/expression.cpp`: one `from_c`, with the caller's name threaded in

In the anonymous namespace, replace both `from_c` overloads (currently ~L74-104, from `quiver::ExpressionAggregate::Operation from_c(quiver_expression_aggregate_operation_t op) {` through the closing `}` after `throw std::runtime_error("Cannot aggregate_agents: unknown operation enum value");`) with

```cpp
// `caller` is the public operation ("aggregate" / "aggregate_agents") named in the Pattern 1 message.
quiver::ExpressionAggregate::Operation from_c(quiver_expression_aggregate_operation_t op, const std::string& caller) {
    switch (op) {
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM:
        return quiver::ExpressionAggregate::Operation::Sum;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN:
        return quiver::ExpressionAggregate::Operation::Mean;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_MIN:
        return quiver::ExpressionAggregate::Operation::Min;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_MAX:
        return quiver::ExpressionAggregate::Operation::Max;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_PERCENTILE:
        return quiver::ExpressionAggregate::Operation::Percentile;
    }
    throw std::runtime_error("Cannot " + caller + ": unknown operation enum value");
}
```

In `quiver_expression_aggregate` (currently ~L258-276), replace

```cpp
        *out = new quiver_expression(expression->expression.aggregate(dimension, from_c(operation), p));
```

with

```cpp
        *out = new quiver_expression(expression->expression.aggregate(dimension, from_c(operation, "aggregate"), p));
```

In `quiver_expression_aggregate_agents` (currently ~L278-295), change the parameter

```cpp
                                                               quiver_expression_aggregate_agents_operation_t operation,
```

to

```cpp
                                                               quiver_expression_aggregate_operation_t operation,
```

and replace

```cpp
        *out = new quiver_expression(expression->expression.aggregate_agents(from_c(operation), p));
```

with

```cpp
        *out = new quiver_expression(expression->expression.aggregate_agents(from_c(operation, "aggregate_agents"), p));
```

`<string>` is already included, at ~L8.

### 5. `src/lua_runner.cpp`: one string parser, with the caller's name threaded in

Replace both parsers (currently ~L1158-1184, `static ExpressionAggregate::Operation parse_aggregate_op(const std::string& op) {` through the end of `parse_aggregate_agents_op`) with

```cpp
    // `caller` is the public method ("aggregate" / "aggregate_agents") named in the Pattern 1 message.
    static ExpressionAggregate::Operation parse_aggregate_op(const std::string& op, const std::string& caller) {
        if (op == "sum")
            return ExpressionAggregate::Operation::Sum;
        if (op == "mean")
            return ExpressionAggregate::Operation::Mean;
        if (op == "min")
            return ExpressionAggregate::Operation::Min;
        if (op == "max")
            return ExpressionAggregate::Operation::Max;
        if (op == "percentile")
            return ExpressionAggregate::Operation::Percentile;
        throw std::runtime_error("Cannot " + caller + ": unknown operation '" + op + "'");
    }
```

In `bind_expression()` (the `lua.new_usertype<Expression>(` block, currently ~L968-977), replace

```cpp
                return self.aggregate(
                    dimension, parse_aggregate_op(op), parameter ? std::optional<double>(*parameter) : std::nullopt);
```

with

```cpp
                return self.aggregate(dimension,
                                      parse_aggregate_op(op, "aggregate"),
                                      parameter ? std::optional<double>(*parameter) : std::nullopt);
```

and

```cpp
                return self.aggregate_agents(parse_aggregate_agents_op(op),
                                             parameter ? std::optional<double>(*parameter) : std::nullopt);
```

with

```cpp
                return self.aggregate_agents(parse_aggregate_op(op, "aggregate_agents"),
                                             parameter ? std::optional<double>(*parameter) : std::nullopt);
```

clang-format decides the final wrapping. Do not touch the bound names `"aggregate"` / `"aggregate_agents"`: `bindings/js/test/lua-api-sync.test.ts` parses them.

### 6. `bindings/julia/src/c_api.jl`: regenerate (do not hand-edit)

After the C++ build (step 1 of Verification), run `bindings/julia/generator/generator.bat`. The generator `dlopen`s `build/bin/libquiver_c.dll`, so it needs the fresh build. The expected diff in this file is exactly two hunks, provided earlier plans committed their own regenerations:
- the `@cenum quiver_expression_aggregate_agents_operation_t::UInt32 begin ... end` block (currently ~L801-807) disappears;
- in `function quiver_expression_aggregate_agents(expression, operation, parameter, out)`, the argument annotation `operation::quiver_expression_aggregate_agents_operation_t` becomes `operation::quiver_expression_aggregate_operation_t`.

### 7. `bindings/julia/src/expression.jl`: both `aggregate_agents` methods take the one enum

In `function aggregate_agents(e::Expression, ...)` (currently ~L207-222), replace

```julia
function aggregate_agents(
    e::Expression,
    operation::C.quiver_expression_aggregate_agents_operation_t,
    parameter::Optional{Real} = nothing,
)
```

with

```julia
function aggregate_agents(
    e::Expression,
    operation::C.quiver_expression_aggregate_operation_t,
    parameter::Optional{Real} = nothing,
)
```

In `function aggregate_agents(f::Binary.File, ...)` (currently ~L233-239), replace

```julia
function aggregate_agents(
    f::Binary.File,
    operation::C.quiver_expression_aggregate_agents_operation_t,
    parameter::Optional{Real} = nothing,
)
```

with

```julia
function aggregate_agents(
    f::Binary.File,
    operation::C.quiver_expression_aggregate_operation_t,
    parameter::Optional{Real} = nothing,
)
```

The two `aggregate` methods (~L192, ~L227) already use `C.quiver_expression_aggregate_operation_t`, so leave them alone. Land this edit in the same change as the regeneration. The method signatures are evaluated at load time, so either half without the other makes `using Quiver` fail with an `UndefVarError`.

### 8. Dart, Python, JS: nothing to do

These bindings do not bind the expression subsystem (root design decision). Confirm with `git grep -n quiver_expression -- bindings/dart bindings/python bindings/js`, which must print nothing. `bindings/js/src/lua-api.ts` (~L826-828) documents the string operations, which are unchanged.

## Tests

### C++ core: `tests/test_expression.cpp`

Change the existing `TEST_F(ExpressionFixture, AgentChainedAfterAggregate)` (currently ~L1450-1465) so that one enum spelling drives both reductions. Replace

```cpp
    Expression(a)
        .aggregate("row", ExpressionAggregate::Operation::Sum)
        .aggregate_agents(ExpressionAggregateAgents::Operation::Mean)
        .save(path_out);
```

with

```cpp
    // One aggregation enum: the same spelling drives both the dimension and the label-axis reduction.
    Expression(a)
        .aggregate("row", ExpressionAggregate::Operation::Sum)
        .aggregate_agents(ExpressionAggregate::Operation::Mean)
        .save(path_out);
```

Before the fix this does not compile, because there is no conversion between the two enum classes. After the fix it passes with the unchanged assertions (`vo.size() == 2`, both `9.0`). Every other `ExpressionAggregateAgents::Operation::*` use in the file (~L1291, 1317, 1335, 1351, 1367, 1385, 1398, 1418, 1437, 1444, 1446, 1473) stays as is. It still compiles through the alias, which also proves the alias works.

### C API: `tests/test_c_api_expression.cpp`

Change the four sites that use the removed constants:
- `TEST_F(ExpressionCApiFixture, AggregateAgentsSumReducesLabels)` (~L1091): `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_SUM` → `QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM`.
- `TEST_F(ExpressionCApiFixture, AggregateAgentsPercentileWithParam)` (~L1123): `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_PERCENTILE` → `QUIVER_EXPRESSION_AGGREGATE_OPERATION_PERCENTILE`.
- `TEST_F(ExpressionCApiFixture, AggregateAgentsNullArguments)` (~L1155 and ~L1157): `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_SUM` → `QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM`, twice.

Add a new test directly after `AggregateAgentsNullArguments`. It is the direct check that the single `from_c` keeps each entry point's Pattern 1 prefix, per the maintainer notes:

```cpp
TEST_F(ExpressionCApiFixture, AggregateUnknownOperationNamesTheCaller) {
    write_fixture(path_a, [](int, int, int) { return 1.0; });
    auto* a = expr_from_file(path_a);
    quiver_expression_t* agg = nullptr;
    // 5 is past PERCENTILE but inside the enum's value range (0..7), so the cast is well-defined.
    const auto unknown = static_cast<quiver_expression_aggregate_operation_t>(5);

    EXPECT_EQ(quiver_expression_aggregate(a, "row", unknown, nullptr, &agg), QUIVER_ERROR);
    EXPECT_EQ(agg, nullptr);
    EXPECT_NE(std::string(quiver_get_last_error()).find("Cannot aggregate: unknown operation enum value"),
              std::string::npos);

    EXPECT_EQ(quiver_expression_aggregate_agents(a, unknown, nullptr, &agg), QUIVER_ERROR);
    EXPECT_EQ(agg, nullptr);
    EXPECT_NE(std::string(quiver_get_last_error()).find("Cannot aggregate_agents: unknown operation enum value"),
              std::string::npos);

    quiver_expression_close(a);
}
```

`"Cannot aggregate: "` (colon straight after `aggregate`) is not a substring of `"Cannot aggregate_agents: ..."`, so the two assertions are distinct. The call to `quiver_expression_aggregate_agents` with `quiver_expression_aggregate_operation_t` does not compile before the fix. After it, the test fails if either call site passes the wrong caller name. Do **not** use a value above 7: that is outside the enum's value range and is undefined behaviour in C++.

### Lua: `tests/test_lua_expression.cpp`

`TEST_F(LuaExpressionTest, AggregateUnknownOpThrows)` (~L169-179) must keep passing unchanged: `"Cannot aggregate: unknown operation 'bogus'"`. Add, directly after it:

```cpp
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
```

It passes both before and after the fix. It is the guard that the merged `parse_aggregate_op` receives `"aggregate_agents"` from the `aggregate_agents` lambda, since the existing test only covers the `aggregate` side. A merged parser with a hard-coded `"aggregate"` would fail it.

### Julia: `bindings/julia/test/test_expression.jl`

Replace `Quiver.C.QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_<OP>` with `Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_<OP>` at all seven sites:
- `@testset "Aggregate_agents sum reduces labels"` (~L1184): `..._SUM`
- `@testset "Aggregate_agents mean"` (~L1203): `..._MEAN`
- `@testset "Aggregate_agents percentile 0.5"` (~L1220): `..._PERCENTILE`
- `@testset "Aggregate_agents skips NaN"` (~L1237): `..._SUM`
- `@testset "Aggregate_agents preserves dimensions"` (~L1254): `..._MEAN`
- `@testset "Aggregate_agents chained after aggregate"` (~L1276): `..._MEAN`. After the change, this testset passes one constant family to both `aggregate` and `aggregate_agents`, which is the Julia-visible proof of the fix. Before it, that call is a `MethodError`.
- `@testset "Aggregate_agents on Binary.File shortcut"` (~L1314): `..._MEAN`

No new Julia testset is needed. After the regeneration, the old constants no longer exist, so any missed site fails with `UndefVarError`.

### Dart / Python / JS

No expression surface, so no tests (root design decision).

## Docs and changelog

**`src/AGENTS.md`, Expression Subsystem section:**

1. The `Aggregation:` sub-bullet (currently ~L740). Old sentence:
   > `op` is the nested enum `ExpressionAggregate::Operation` (for `aggregate`) or `ExpressionAggregateAgents::Operation` (for `aggregate_agents`), each with `Sum / Mean / Min / Max / Percentile`.

   New sentence:
   > `op` is `ExpressionAggregate::Operation` (`Sum / Mean / Min / Max / Percentile`) for both; `ExpressionAggregateAgents::Operation` is an alias of it, not a second enum.

2. The `ExpressionAggregateAgents` sub-bullet (currently ~L755). Old sentence:
   > Shares the accumulation templates in `expression_helpers.h` with `ExpressionAggregate`.

   New sentence:
   > Shares `ExpressionAggregate`'s operation enum (`using Operation = ExpressionAggregate::Operation;`) and the accumulation helpers in `expression_helpers.h`.

3. The enum paragraph (currently ~L759, starting "- All operation enums are nested in their owning class:"). Replace the whole bullet with:
   > - All operation enums are nested in their owning class: `ExpressionBinary::Operation`, `ExpressionUnary::Operation`, `ExpressionTernary::Operation`, `ExpressionAggregate::Operation`. There is **one** aggregation enum (`Sum / Mean / Min / Max / Percentile`): `ExpressionAggregateAgents::Operation` is `using Operation = ExpressionAggregate::Operation;`, so `aggregate` and `aggregate_agents` take the same type and `aggregation_operation_label` / `validate_aggregation_param` / `aggregation_accumulate` / `aggregation_finalize` (`expression_helpers.h`) are plain functions on it. It used to be two parallel enums with identical values, which doubled the C enum, the C `from_c` switch, the Lua string parser and the Julia constants; do not re-split it. Label-axis projection nodes (`ExpressionSelectAgents`, `ExpressionRenameAgents`) have no operation enum — their behavior is fully specified by the label list / rename map. The C API mirrors this with four enums: `quiver_expression_operation_t` (now `ADD..DIVIDE`, the comparisons `GT/LT/GTE/LTE/EQ/NEQ`, and the logical `AND/OR`), `quiver_expression_unary_operation_t` (math ops plus `NOT`), `quiver_expression_ternary_operation_t`, and `quiver_expression_aggregate_operation_t`, which both `quiver_expression_aggregate` and `quiver_expression_aggregate_agents` take. The one C `from_c` and the one Lua `parse_aggregate_op` take the calling operation's name, so their Pattern 1 messages still read `Cannot aggregate: ...` or `Cannot aggregate_agents: ...`. Comparisons and logical ops reuse the `quiver_expression_apply*` / `quiver_expression_apply_unary` entry points (no new C functions); the Julia FFI enum (`src/c_api.jl`) must carry the same values.

**`src/c/AGENTS.md`, File Map (currently ~L19).** Old:
```
  expression.h                # quiver_expression_t handle + node constructors + five operation enums
```
New:
```
  expression.h                # quiver_expression_t handle + node constructors + four operation enums
```

**`bindings/julia/AGENTS.md`, root `AGENTS.md`, `tests/AGENTS.md`, `docs/*.md`, `bindings/js/src/lua-api.ts`:** none mention the two aggregation enums, so there are no edits. The Julia `aggregate_agents` surface is not documented anywhere but its tests.

**`CHANGELOG.md`:** under `## [0.11.0] — unreleased` → `### Changed`, append this bullet after the existing BREAKING entries. Earlier plans may have added more; keep all of them.

```markdown
- **BREAKING — expressions: one aggregation operation enum.** `quiver_expression_aggregate_agents`
  now takes `quiver_expression_aggregate_operation_t`, the same enum as `quiver_expression_aggregate`;
  `quiver_expression_aggregate_agents_operation_t` and its `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_*`
  constants are removed (their values were identical). In C++, `ExpressionAggregateAgents::Operation`
  is now an alias of `ExpressionAggregate::Operation`, so C++ code compiles unchanged and can pass
  either spelling to either method. Lua takes the operation as a string and is unaffected.

  *Adapt:* in C and Julia, replace `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_<OP>` with
  `QUIVER_EXPRESSION_AGGREGATE_OPERATION_<OP>` — in Julia,
  `Quiver.aggregate_agents(e, Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN)`.
```

## Verification

Run from the repo root `C:\Development\Quiver\quiver3`. The paths are for PowerShell; from Git Bash, prefix the `.bat` calls with `cmd //c`.

1. `cmake --build build --config Debug`. The build must succeed with no new warnings in `expression_helpers.h` or `src/c/expression/expression.cpp`.
2. `./build/bin/quiver_tests.exe --gtest_filter='ExpressionFixture.*:LuaExpressionTest.*'`. All tests pass, including the edited `ExpressionFixture.AgentChainedAfterAggregate` and the new `LuaExpressionTest.AggregateAgentsUnknownOpThrows`, and `LuaExpressionTest.AggregateUnknownOpThrows` still passes.
3. `./build/bin/quiver_c_tests.exe --gtest_filter='ExpressionCApiFixture.*'`. All pass, including the new `ExpressionCApiFixture.AggregateUnknownOperationNamesTheCaller`.
4. `bindings\julia\generator\generator.bat`, then `git diff bindings/julia/src/c_api.jl`. Expect only the two hunks listed in Change 6. If other hunks appear, an earlier plan left its C header change unregenerated; check that before continuing.
5. `bindings\julia\test\test.bat`. All Julia tests pass, including the seven edited `Aggregate_agents*` testsets.
6. `git grep -n -e AGGREGATE_AGENTS_OPERATION -e aggregate_agents_operation_t -e parse_aggregate_agents_op -- ":!CHANGELOG.md"` prints nothing.
7. `git grep -n "template <typename Op>" -- src/expression` prints nothing.
8. `scripts\format.bat`, then `git diff --stat` again. Formatting may re-wrap the edited lines in `expression_helpers.h`, `src/c/expression/expression.cpp`, `src/lua_runner.cpp`, `tests/test_c_api_expression.cpp` and `test_expression.jl`; accept those changes. No `.bat` file may show as modified.
9. `scripts\test-all.bat`. All six suites and the CLI smoke test pass. Dart, Python and JS are untouched, but this confirms the rebuilt native libraries.

## Acceptance criteria

- [ ] `include/quiver/expression/expression_node.h` has exactly one `enum class Operation { Sum, Mean, Min, Max, Percentile };` (in `ExpressionAggregate`); `ExpressionAggregateAgents` declares `using Operation = ExpressionAggregate::Operation;`.
- [ ] The four aggregation helpers in `src/expression/expression_helpers.h` are `inline` non-template functions taking `ExpressionAggregate::Operation`.
- [ ] `include/quiver/c/expression/expression.h` has no `quiver_expression_aggregate_agents_operation_t`; `quiver_expression_aggregate_agents` takes `quiver_expression_aggregate_operation_t`.
- [ ] `src/c/expression/expression.cpp` has one `from_c(op, caller)`; the messages are `Cannot aggregate: unknown operation enum value` and `Cannot aggregate_agents: unknown operation enum value`.
- [ ] `src/lua_runner.cpp` has one `parse_aggregate_op(op, caller)`; `parse_aggregate_agents_op` is gone; the Lua messages are unchanged.
- [ ] `bindings/julia/src/c_api.jl` is regenerated, not hand-edited, and both `aggregate_agents` methods in `expression.jl` take `C.quiver_expression_aggregate_operation_t`.
- [ ] The new tests `AggregateUnknownOperationNamesTheCaller` (C API) and `AggregateAgentsUnknownOpThrows` (Lua) pass; the edited C++, C API and Julia tests pass.
- [ ] src/AGENTS.md (three spots) and src/c/AGENTS.md are updated as quoted; there is a CHANGELOG **BREAKING** entry under 0.11.0 → Changed.
- [ ] `scripts/test-all.bat` is green.

## Pitfalls

- **`inline` is mandatory** on the four de-templated functions. `expression_helpers.h` is included by `expression_aggregate.cpp`, `expression_aggregate_agents.cpp`, `expression_binary.cpp`, `expression_ternary.cpp` and others. A non-inline function defined in a header is an ODR violation, reported as LNK2005 on MSVC and "multiple definition" on GCC/Clang.
- **Regenerate and edit `expression.jl` together.** After the regeneration, `C.quiver_expression_aggregate_agents_operation_t` no longer exists, so the old signatures fail at package load. The generator also needs the freshly built `build/bin/libquiver_c.dll`: build first.
- **Line numbers have drifted.** Plan 15 rewrites the broadcast helpers above the aggregation helpers in `expression_helpers.h`, plans 09/10 edit `expression_aggregate.cpp`, and plan 12 regenerates `c_api.jl`. Anchor on the quoted code.
- **Do not "clean up" the alias into a namespace-scope enum**, and do not rename `quiver_expression_aggregate_operation_t`. Either would break every `aggregate` caller, which is outside the maintainer's "C/Julia constants" scope.
- **Unknown-enum test value:** use `5` (0..7 is the value range of an unscoped enum whose enumerators are 0..4). A cast to `99` is UB in C++17+ and can be optimised into anything.
- **Message bytes.** `test_lua_expression.cpp` pins `Cannot aggregate: unknown operation 'bogus'`. Build the message as `"Cannot " + caller + ": unknown operation '" + op + "'"`, with no extra quoting around `caller`.
- **Formatting.** `scripts/format.bat` runs clang-format and JuliaFormatter; let them re-wrap. Working-tree `.bat` files are CRLF, and nothing in this plan edits one; if `git diff` shows a `.bat`, restore it.
- **CHANGELOG position.** Earlier plans in this batch may have added entries or a `### Removed` section under 0.11.0. Append to `### Changed` and do not reorder or delete others.

## Out of scope

- The Lua operator metamethod tables (`bind_expression` / `bind_binary` `sol::meta_function::*` lists): plan 51. Plan 51 must not re-add a templated aggregation parser, because this plan owns that half.
- The binary/ternary broadcast helpers in `expression_helpers.h`: plan 15.
- The start/offset logic in `ExpressionAggregate::compute_row` and the constructor: plans 09 and 10.
- Renaming `validate_aggregation_param`'s `fn_label` parameter to `caller`: cosmetic, not needed.
- Caller names in other Lua converters (`table_to_element`, `lua_table_to_values`, ...): plan 47.
- The other three C expression enums (`quiver_expression_operation_t`, `_unary_`, `_ternary_`): these are distinct concepts and stay as they are.
