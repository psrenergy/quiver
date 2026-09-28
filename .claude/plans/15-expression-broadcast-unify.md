# 15 — Expressions: one N-ary broadcast-metadata builder with the relaxed label rule

**Batch** 2 · **Severity** medium · **Breaking** no. This only loosens a rule: a binary operation on two single-label operands with different label names used to throw and now builds. Every expression that built before produces the same output. The one visible side effect is the error text: a label-set mismatch in a *binary* operation now reports the message `ifelse` already used (see Changelog). · **Size** S · **Layers** C++ core (`src/expression/expression_helpers.h`, `expression_binary.cpp`, `expression_ternary.cpp`); tests in C++, C API, Lua and Julia; `src/AGENTS.md`; `CHANGELOG.md`. No C API signature change, so there are no FFI regenerations or binding wrapper edits.

**Depends on** none. This plan runs after 08–14 in numeric order, so every anchor below is a quoted excerpt, not a line number.

**Overlaps with**
- **16** (one AggregationOperation enum) edits the aggregation templates at the bottom of the same header, `src/expression/expression_helpers.h` (`aggregation_operation_label`, `validate_aggregation_param`, `aggregation_accumulate`, `aggregation_finalize`), and renames the aggregation enums in C++, the C API and Julia. This plan edits only the broadcast block above those templates. The new C++, C API and Julia tests here deliberately avoid `aggregate_agents`, so plan 16 has nothing to rename in them. The new Lua test uses the string ops `'max'`/`'min'`, which plan 16 keeps.
- **10** adds tests to `tests/test_expression.cpp` and `bindings/julia/test/test_expression.jl` at other anchors (after `AggregateSumOverTimeDimVariable` / in the aggregate section). Only the files are shared.
- **09** edits the `ExpressionAggregate` bullet of `src/AGENTS.md`'s Expression Subsystem section. This plan edits the `ExpressionBinary`, `ExpressionTernary` and "Validation is eager" bullets and adds one bullet after the last of these. Only the section is shared.
- **51** re-registers the Lua operator metamethods for `BinaryFile` and `Expression`. The Lua test added here relies on `Expression - Expression`, so it must still pass after plan 51.
- **01–14** append entries to `CHANGELOG.md` under `## [0.12.0] — unreleased`. Append yours; don't overwrite theirs.

## Why

`ExpressionBinary` (`+ - * /`, the six comparisons, `&&`/`||`) and `ExpressionTernary` (`ifelse`) each build their output metadata with their own copy of one algorithm. The two copies' label rules have drifted apart.

`src/expression/expression_helpers.h`, `compute_output_labels` (currently ~L112-131), is the binary rule:
```cpp
    const auto ll = l_labels.size();
    const auto rl = r_labels.size();
    if (ll == rl) {
        if (l_labels != r_labels) {
            throw std::runtime_error("Cannot apply: labels have same size " + std::to_string(ll) +
                                     " but different content");
        }
        return l_labels;
    }
    if (ll == 1 && rl > 1) {
        return r_labels;
    }
```
`compute_ternary_output_labels` (currently ~L179-204) is the ternary rule:
```cpp
    if (non_singleton.empty()) {
        return t_labels;
    }
```
So a single label broadcasts against a multi-label set whatever the single label is called (`{"x"} + {"a","b"}` gives `{"a","b"}`, pinned by `ExpressionFixture.BroadcastLabelsAxis`). But in a binary operation, two single labels with different names throw. In `ifelse` they don't.

Reproduction (Lua, in a `LuaRunner` over a file-backed database, file `a` with labels `{"v1","v2"}`):
```lua
local fa = db:open_file('a', 'r')
local hi = quiver.expression(fa):aggregate_agents('max')   -- labels {"max"}
local lo = quiver.expression(fa):aggregate_agents('min')   -- labels {"min"}
quiver.ifelse(quiver.gt(hi, 0.0), hi, lo)   -- builds
local spread = hi - lo                      -- throws: Cannot apply: labels have same size 1 but different content
```
`ExpressionAggregateAgents` names its single label after the operation (`src/expression/expression_aggregate_agents.cpp`, `output_meta_.labels = {aggregation_operation_label(operation_)};`), so "max minus min over the agents" can never be written. The logical operators hit the same wall. src/AGENTS.md says `&&`/`||` skip the unit check "so conditions on different-unit variables compose", but two single-label files with different names (`(demand > x) & (price < y)`) still throw on labels.

The dimension half of the two builders is the same algorithm written twice. `build_broadcast_metadata` (currently ~L133-177) walks the lhs dims at `max(l, r)`, appends the rhs-only dims, and then remaps each time dim's parent by name, from lhs first:
```cpp
    for (const auto& l_dim : lhs.dimensions) {
        auto r_idx = find_dim_index(rhs.dimensions, l_dim.name);
        int64_t out_size = (r_idx >= 0) ? std::max(l_dim.size, rhs.dimensions[r_idx].size) : l_dim.size;
```
`build_ternary_broadcast_metadata` (currently ~L206-270) does the same over `sources = {&cond, &then_meta, &else_meta}`. It keeps the first occurrence of each name, sizes it as the max over the sources that have it, and takes the parent from the first source holding the dim. Run over `{lhs, rhs}`, that N-ary loop gives exactly the binary output: lhs dims first at `max(l, r)`, then rhs-only dims at their own size and with their own time properties, then the parent from lhs first.

This violates Homogeneity (one rule, and it disagrees with itself) and "Simple solutions over complex abstractions" (two copies of one algorithm, which have already drifted).

## Constraints and decisions

- **Maintainer decision (binding):** the relaxed (ternary) label rule applies to both binary and ternary operations. Label sets with more than one entry must match. A single label broadcasts. When every operand has a single label, the output takes the *primary* operand's labels. The primary is `lhs` for binary and `then_value` for ternary. Keep the current ternary dimension order (condition first) and the current `initial_datetime` fallback. Record it in CHANGELOG as a non-breaking relaxation.
- Root `AGENTS.md` Design Decisions: "Binary + expression subsystems are exposed in Julia and Lua only." Tests therefore go in C++, C API, Lua and Julia. Dart, Python and JS are untouched.
- Root `AGENTS.md` Principles: "Clean code over defensive code … Simple solutions over complex abstractions. Delete unused code, do not deprecate." Delete both old helper pairs outright.
- Root `AGENTS.md` "C++ Error Message Patterns": Pattern 1 `"Cannot {operation}: {reason}"`. The expression subsystem uses `Cannot apply: …` throughout. Keep the ternary's existing message verbatim: `Cannot apply: labels are incompatible across operands (non-singleton label sets must match)`.
- `src/AGENTS.md` Expression Subsystem: logical ops skip the unit check and emit a unitless result via the `is_logical(op)` branch in the `ExpressionBinary` constructor. That branch stays as it is. `ifelse`: "`then` and `else` units must match; `cond`'s unit is ignored". Unchanged.
- "Self-Updating": `src/AGENTS.md` names `build_broadcast_metadata` and `build_ternary_broadcast_metadata`, so it must change in the same commit.

**Why one source list is enough (correction to the facts verifier).** The facts verifier proposed a second order, `{then, else, cond}`, for the ternary labels and datetime. That second order isn't needed:
- *Labels:* every non-singleton label set must be equal, so which one is returned doesn't matter. Order matters only when every operand is a singleton, and `primary` covers that case directly.
- *`initial_datetime`:* `validate_shape_compatibility` (`expression_helpers.h`, currently ~L100-104) throws `initial_datetime differs` for any two time-bearing operands, and the ternary constructor calls it on all three pairs. So "the first source with a time dimension" gives the same value in any order. Only the no-time fallback depends on the order, and that falls back to `primary`, which is `lhs` for binary and `then` for ternary. That is exactly today's behaviour. Binary takes lhs's datetime if lhs has a time dim, else rhs's if rhs has one, else lhs's. Ternary takes the first of then, else, cond that has a time dim, else then's.

**Alternatives considered and rejected**
- Keep the strict binary rule and make ternary strict too. The maintainer chose the relaxed rule, and the strict rule contradicts itself (`{"x"}` broadcasts against `{"a","b"}` whatever it is called, but not against `{"y"}`).
- One builder that falls back to the *first* source for labels and datetime (the finding's sketch). That silently switches ternary's no-time `initial_datetime` fallback and all-singleton label fallback from `then` to `cond`. Both verifiers flagged it, and the maintainer said to keep the fallback.
- Three separate helpers, `broadcast_dimensions`, a datetime picker and `broadcast_labels` (the original proposal). Each would have one caller, and all three need the same `sources`. One builder that takes `primary` has fewer moving parts. Only the label rule stays a separately named function, because it is the rule the docs refer to.
- Merge the pairwise `validate_shape_compatibility` calls into an N-ary validator. Out of scope: its messages name `lhs`/`rhs`, and nothing is wrong with it.

## Changes

Make the changes in this order. To see the new tests fail first, do the **Tests** section before step 1, build, and run the filters in **Verification**.

### 1. `src/expression/expression_helpers.h`: replace both helper pairs with one builder

**1a.** Add `<initializer_list>` to the include block at the top (currently ~L7-15), keeping alphabetical order.

Current:
```cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
```
New:
```cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
```

**1b.** Delete these four functions, which are contiguous and sit between `validate_compatibility` and `template <typename Op> std::string aggregation_operation_label(Op op)`:
- `compute_output_labels` (starts `inline std::vector<std::string> compute_output_labels(const std::vector<std::string>& l_labels,`, currently ~L112)
- `build_broadcast_metadata`, the old two-operand one (starts with the two lines `inline BinaryMetadata` and `build_broadcast_metadata(const BinaryMetadata& lhs, const BinaryMetadata& rhs, std::vector<std::string> output_labels) {`, currently ~L133)
- `compute_ternary_output_labels` (starts `inline std::vector<std::string> compute_ternary_output_labels(const std::vector<std::string>& c_labels,`, currently ~L179)
- `build_ternary_broadcast_metadata` (starts `inline BinaryMetadata build_ternary_broadcast_metadata(const BinaryMetadata& cond,`, currently ~L206). It ends with the lines `    return out;` and `}`, followed by a blank line and `template <typename Op>`.

Put these two functions in their place, directly after `validate_compatibility`'s closing brace:

```cpp
// The one label rule for every broadcasting node: every operand with more than one label must carry
// the same label set, and a single-label operand broadcasts its one value across it whatever that
// label is called. When every operand has a single label, the output takes the primary operand's.
inline std::vector<std::string> broadcast_labels(std::initializer_list<const BinaryMetadata*> sources,
                                                 const BinaryMetadata& primary) {
    const std::vector<std::string>* labels = nullptr;
    for (const auto* src : sources) {
        if (src->labels.size() <= 1) {
            continue;
        }
        if (labels == nullptr) {
            labels = &src->labels;
        } else if (src->labels != *labels) {
            throw std::runtime_error("Cannot apply: labels are incompatible across operands "
                                     "(non-singleton label sets must match)");
        }
    }
    return labels != nullptr ? *labels : primary.labels;
}

// Output metadata of a broadcasting node, shared by ExpressionBinary ({lhs, rhs}, primary lhs) and
// ExpressionTernary ({cond, then, else}, primary then). The order of `sources` is the output
// dimension order: the union of dimension names, first occurrence first, each sized as the max over
// the sources that have it (validate_shape_compatibility has already checked the sizes broadcast),
// with time properties and the parent link taken from the first source that has it. version and
// unit come from `primary`. initial_datetime comes from the first source with a time dimension,
// else from `primary`. The pairwise validate_shape_compatibility calls force every time-bearing
// source to agree, so only that fallback depends on which operand is primary.
inline BinaryMetadata build_broadcast_metadata(std::initializer_list<const BinaryMetadata*> sources,
                                               const BinaryMetadata& primary) {
    BinaryMetadata out;
    out.version = primary.version;
    out.unit = primary.unit;
    out.labels = broadcast_labels(sources, primary);
    out.initial_datetime = primary.initial_datetime;
    for (const auto* src : sources) {
        if (any_time_dim(src->dimensions)) {
            out.initial_datetime = src->initial_datetime;
            break;
        }
    }

    std::unordered_map<std::string, int> output_index_by_name;
    for (const auto* src : sources) {
        for (const auto& dim : src->dimensions) {
            if (output_index_by_name.count(dim.name)) {
                continue;
            }
            int64_t out_size = dim.size;
            for (const auto* other : sources) {
                const auto idx = find_dim_index(other->dimensions, dim.name);
                if (idx >= 0) {
                    out_size = std::max(out_size, other->dimensions[idx].size);
                }
            }
            out.dimensions.push_back(Dimension{dim.name, out_size, dim.time});
            output_index_by_name[dim.name] = static_cast<int>(out.dimensions.size()) - 1;
        }
    }

    for (auto& out_d : out.dimensions) {
        if (!out_d.is_time_dimension()) {
            continue;
        }
        const BinaryMetadata* src_meta = nullptr;
        int src_idx = -1;
        for (const auto* s : sources) {
            src_idx = find_dim_index(s->dimensions, out_d.name);
            if (src_idx >= 0) {
                src_meta = s;
                break;
            }
        }
        const int64_t src_parent_idx = src_meta->dimensions[src_idx].time->parent_dimension_index;
        if (src_parent_idx < 0) {
            out_d.time->parent_dimension_index = -1;
            continue;
        }
        const std::string& parent_name = src_meta->dimensions[src_parent_idx].name;
        out_d.time->parent_dimension_index = output_index_by_name.find(parent_name)->second;
    }
    return out;
}
```

The body is the ternary builder with three differences:
- It takes `primary` instead of hard-coding `then_meta`.
- It picks `initial_datetime` with the loop described above.
- It drops the `if (other == src) continue;` skip in the max-size loop. Taking the max with the source's own size is a no-op, so nothing changes, including when both operands are the same node (`e + e`).

Leave `find_dim_index`, `any_time_dim`, `parent_name_of`, `validate_unit_match`, `validate_shape_compatibility` and `validate_compatibility` untouched. The broadcast-operand helpers (`make_broadcast_operand`, `compute_broadcast_operand_row`, `broadcast_label_index`) are also unchanged. `broadcast_label_index` already maps any single-label operand to index 0, so `{"alpha"} - {"beta"}` computes correctly with no change there.

### 2. `src/expression/expression_binary.cpp`: `ExpressionBinary::ExpressionBinary`

Current (currently ~L94-95):
```cpp
    auto output_labels = compute_output_labels(lhs_meta.labels, rhs_meta.labels);
    broadcast_meta_ = build_broadcast_metadata(lhs_meta, rhs_meta, std::move(output_labels));
    if (is_logical(operation_)) {
        broadcast_meta_.unit = "";
    }
```
New:
```cpp
    broadcast_meta_ = build_broadcast_metadata({&lhs_meta, &rhs_meta}, lhs_meta);
    if (is_logical(operation_)) {
        broadcast_meta_.unit = "";
    }
```
Keep everything else: the `is_logical` validation branch above, `broadcast_meta_.validate();` and the two `make_broadcast_operand` calls. `<utility>` is still used by the `std::move`s in the member-initializer list, so don't touch the includes.

### 3. `src/expression/expression_ternary.cpp`: `ExpressionTernary::ExpressionTernary`

Current (currently ~L40-41):
```cpp
    auto output_labels = compute_ternary_output_labels(condition_meta.labels, then_meta.labels, else_meta.labels);
    broadcast_meta_ = build_ternary_broadcast_metadata(condition_meta, then_meta, else_meta, std::move(output_labels));
    broadcast_meta_.validate();
```
New:
```cpp
    broadcast_meta_ = build_broadcast_metadata({&condition_meta, &then_meta, &else_meta}, then_meta);
    broadcast_meta_.validate();
```
The sources stay condition-first, because that order sets the output dimension order (maintainer decision). Don't reorder them to `{then, else, cond}`. Keep the four validation calls above unchanged: `validate_unit_match(then_meta, else_meta)` and the three pairwise `validate_shape_compatibility` calls.

### 4. Other layers

Nothing else changes:
- **C API:** `src/c/expression/expression.cpp` only forwards to the C++ constructors, and no signature changes.
- **FFI declarations:** no Julia `c_api.jl` regeneration, Python cdef, JS `loader.ts` or Dart `bindings.dart` change.
- **Bindings:** no wrapper change. `bindings/julia/src/expression.jl` has no label logic, and Dart/Python/JS don't expose expressions.
- **Lua:** `src/lua_runner.cpp` binds the C++ operators directly (`binop_dispatch`, `quiver::ifelse`), so it inherits the rule unchanged.

## Tests

The behaviour is visible in C++, the C API, Lua and Julia, and each layer gets a test. Tests marked **fails before** fail on the current code. The two ternary tests pass before and after. They pin the order-sensitive parts of the refactor (condition-first dims, `then` fallback for labels and datetime), so a later edit that reorders `sources` fails loudly.

### C++: `tests/test_expression.cpp` (fixture `ExpressionFixture`, no schema)

**T1. Fixture helper.** In `class ExpressionFixture`, directly after `static BinaryMetadata make_simple_metadata() { ... }` (currently ~L46-54), add:
```cpp
    // 3 x 2 (row, col) metadata with a single label.
    static BinaryMetadata make_single_label_metadata(const char* label, const char* unit = "MW") {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", unit)
                                                .set("dimensions", {"row", "col"})
                                                .set("dimension_sizes", {3, 2})
                                                .set("labels", {label}));
    }
```
(`{label}` with a `const char*` picks the `std::initializer_list<const char*>` overload of `Element::set`, exactly as the existing `.set("labels", {"single"})` calls do.)

**T2. Three binary tests.** Insert them directly after the closing `}` of `TEST_F(ExpressionFixture, LabelMismatchThrows)` (currently ~L373-393), before `TEST_F(ExpressionFixture, MirrorTimeNonTimeMismatchAThrows)`:

```cpp
TEST_F(ExpressionFixture, LabelSetsOfDifferentSizesThrow) {
    // Only a single label broadcasts: two multi-label operands must carry the same label set.
    auto md_a = make_simple_metadata();  // {val1, val2}
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"val1", "val2", "val3"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW(
        {
            try {
                auto e = Expression(a) + Expression(b);
            } catch (const std::runtime_error& err) {
                EXPECT_NE(std::string(err.what()).find("non-singleton label sets must match"), std::string::npos)
                    << err.what();
                throw;
            }
        },
        std::runtime_error);
}

TEST_F(ExpressionFixture, SingleLabelOperandsWithDifferentNamesBroadcast) {
    // A single label broadcasts whatever it is called, also against another single label, and the
    // output takes the lhs label. aggregate_agents names its label after the operation, so max - min
    // over one file is this case (LuaExpressionTest.AggregateAgentsMaxMinusMin).
    write_qvr(path_a, make_single_label_metadata("alpha"), [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return static_cast<double>(dims[0] * 10 + dims[1]);
    });
    write_qvr(path_b, make_single_label_metadata("beta"), [](const std::vector<int64_t>&, size_t) { return 1.0; });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) - Expression(b);
    EXPECT_EQ(e.metadata().labels, std::vector<std::string>{"alpha"});
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), va.size());
    for (size_t i = 0; i < vo.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] - 1.0) << " at index " << i;
}

TEST_F(ExpressionFixture, LogicalOnSingleLabelOperandsWithDifferentNames) {
    // Conditions on two different variables usually come from single-label files with different
    // names (and units); && ignores the units and must combine the labels too.
    write_qvr(path_a,
              make_single_label_metadata("demand", "MW"),
              [](const std::vector<int64_t>& dims, size_t /*k*/) { return static_cast<double>(dims[0]); });
    write_qvr(path_b,
              make_single_label_metadata("price", "USD"),
              [](const std::vector<int64_t>& dims, size_t /*k*/) { return static_cast<double>(dims[1]); });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = (Expression(a) > 1.0) && (Expression(b) < 2.0);
    EXPECT_EQ(e.metadata().labels, std::vector<std::string>{"demand"});
    EXPECT_EQ(e.metadata().unit, "");
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), va.size());
    for (size_t i = 0; i < vo.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], (va[i] > 1.0 && vb[i] < 2.0) ? 1.0 : 0.0) << " at index " << i;
}
```
- `LabelSetsOfDifferentSizesThrow`, **fails before:** it throws `Cannot apply: labels have incompatible sizes 2 vs 3`, so the substring check fails.
- `SingleLabelOperandsWithDifferentNamesBroadcast`, **fails before:** `Expression(a) - Expression(b)` throws `labels have same size 1 but different content`.
- `LogicalOnSingleLabelOperandsWithDifferentNames`, **fails before:** same throw.

**T3. Two ternary tests.** Insert them directly after the closing `}` of `TEST_F(ExpressionFixture, IfElseBroadcastsLabels)` (currently ~L1813-1846), before `TEST_F(ExpressionFixture, IfElseUnitMismatchThenElseThrows)`:

```cpp
TEST_F(ExpressionFixture, IfElseSingleLabelOperandsTakeThenLabels) {
    // Every operand has a single label, each named differently: the output takes the then label.
    write_qvr(path_a, make_single_label_metadata("c", "flag"), [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return (dims[0] == 1) ? 1.0 : 0.0;
    });
    write_qvr(path_b, make_single_label_metadata("t"), [](const std::vector<int64_t>&, size_t) { return 10.0; });
    write_qvr(path_c, make_single_label_metadata("e"), [](const std::vector<int64_t>&, size_t) { return 20.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    EXPECT_EQ(e.metadata().labels, std::vector<std::string>{"t"});
    e.save(path_out);

    auto vc = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), vc.size());
    for (size_t i = 0; i < vo.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], (vc[i] != 0.0) ? 10.0 : 20.0) << " at index " << i;
}

TEST_F(ExpressionFixture, IfElseDimensionsFollowConditionAndDatetimeFollowsThen) {
    // Output dimensions come condition-first. With no time dimension anywhere, initial_datetime comes
    // from the then operand, not the condition.
    auto md_cond = BinaryMetadata::from_element(Element()
                                                    .set("version", "1")
                                                    .set("initial_datetime", "2030-01-01T00:00:00")
                                                    .set("unit", "flag")
                                                    .set("dimensions", {"scenario"})
                                                    .set("dimension_sizes", {2})
                                                    .set("labels", {"c"}));
    auto md_branch = BinaryMetadata::from_element(Element()
                                                      .set("version", "1")
                                                      .set("initial_datetime", "2025-01-01T00:00:00")
                                                      .set("unit", "MW")
                                                      .set("dimensions", {"row"})
                                                      .set("dimension_sizes", {3})
                                                      .set("labels", {"val1", "val2"}));
    write_qvr(
        path_a, md_cond, [](const std::vector<int64_t>& dims, size_t /*k*/) { return (dims[0] == 1) ? 1.0 : 0.0; });
    write_qvr(path_b, md_branch, [](const std::vector<int64_t>&, size_t) { return 10.0; });
    write_qvr(path_c, md_branch, [](const std::vector<int64_t>&, size_t) { return 20.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));

    const auto& m = e.metadata();
    ASSERT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.dimensions[0].name, "scenario");
    EXPECT_EQ(m.dimensions[0].size, 2);
    EXPECT_EQ(m.dimensions[1].name, "row");
    EXPECT_EQ(m.dimensions[1].size, 3);
    EXPECT_EQ(m.labels, (std::vector<std::string>{"val1", "val2"}));
    EXPECT_EQ(m.unit, "MW");
    EXPECT_TRUE(m.initial_datetime == then_v.get_metadata().initial_datetime);
    EXPECT_FALSE(m.initial_datetime == cond.get_metadata().initial_datetime);

    e.save(path_out);
    auto reopened = BinaryFile::open_file(path_out, 'r');
    auto cell_12 = reopened.read({{"scenario", 1}, {"row", 2}}, true);
    auto cell_23 = reopened.read({{"scenario", 2}, {"row", 3}}, true);
    EXPECT_DOUBLE_EQ(cell_12[0], 10.0);
    EXPECT_DOUBLE_EQ(cell_12[1], 10.0);
    EXPECT_DOUBLE_EQ(cell_23[0], 20.0);
    EXPECT_DOUBLE_EQ(cell_23[1], 20.0);
}
```
Both pass before and after. Two assertions to note:
- `EXPECT_FALSE(... == cond...)` guards the test itself: it proves the two datetimes differ, so the `then` assertion can't pass by coincidence.
- `initial_datetime` is compared with `EXPECT_TRUE(a == b)` rather than `EXPECT_EQ`, so gtest never has to print a `time_point`.

**Existing C++ tests.** None change. The ones that pin behaviour this plan touches:
- `LabelMismatchThrows` (`{v1,v2}` vs `{v1,v3}`, bare `EXPECT_THROW`) still throws, now with the non-singleton message.
- `BroadcastLabelsAxis` (`{"single"}` + `{l1,l2,l3}` → `{l1,l2,l3}`), `IfElseBroadcastsLabels`, `UnionDimsAcrossOperands` (pins lhs-first dimension order), `OperandDimsInDifferentOrder`, `ParentDimMatchByNameAcceptsCrossPosition`, `InitialDatetimeMismatchThrows` and `LogicalIsUnitlessAcrossUnits` must all still pass unchanged.

`grep -rn "different content\|incompatible sizes\|labels are incompatible" tests bindings` finds nothing, so no test pins the old or new message text.

### C API: `tests/test_c_api_expression.cpp` (fixture `ExpressionCApiFixture`)

Insert directly after the closing `}` of `TEST_F(ExpressionCApiFixture, LabelMismatchReturnsError)` (currently ~L584-599), before the `// Save collision` banner:
```cpp
TEST_F(ExpressionCApiFixture, SingleLabelOperandsWithDifferentNamesBroadcast) {
    auto* md_a = make_metadata_v({"row", "col"}, {2, 2}, {"alpha"});
    auto* md_b = make_metadata_v({"row", "col"}, {2, 2}, {"beta"});
    write_dense(path_a, md_a, {"row", "col"}, {2, 2}, 1, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return static_cast<double>(dims[0] * 10 + dims[1]);
    });
    write_dense(path_b, md_b, {"row", "col"}, {2, 2}, 1, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    quiver_binary_metadata_free(md_a);
    quiver_binary_metadata_free(md_b);

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* diff = nullptr;
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OPERATION_SUBTRACT, a, b, &diff), QUIVER_OK)
        << quiver_get_last_error();

    quiver_binary_metadata_t* out_md = nullptr;
    ASSERT_EQ(quiver_expression_get_metadata(diff, &out_md), QUIVER_OK);
    char** labels = nullptr;
    size_t label_count = 0;
    ASSERT_EQ(quiver_binary_metadata_get_labels(out_md, &labels, &label_count), QUIVER_OK);
    ASSERT_EQ(label_count, 1u);
    EXPECT_STREQ(labels[0], "alpha");  // both operands single-label: the lhs label wins
    quiver_binary_metadata_free_string_array(labels, label_count);
    quiver_binary_metadata_free(out_md);

    ASSERT_EQ(quiver_expression_save(diff, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(b);
    quiver_expression_close(diff);

    auto cell_21 = read_one_cell(path_out, {"row", "col"}, {2, 1});
    ASSERT_EQ(cell_21.size(), 1u);
    EXPECT_DOUBLE_EQ(cell_21[0], (2.0 * 10 + 1.0) - 1.0);
}
```
**Fails before:** `quiver_expression_apply` returns `QUIVER_ERROR` with `Cannot apply: labels have same size 1 but different content`. The existing `LabelMismatchReturnsError` (`{val1,val2}` vs `{val1,val3}`) and `BroadcastLabelsAxis` stay unchanged and must still pass.

### Lua: `tests/test_lua_expression.cpp` (fixture `LuaExpressionTest`, schema `valid/collections.sql` from `SetUp`)

Insert directly after the closing `}` of `TEST_F(LuaExpressionTest, AggregateAgentsMean)` (currently ~L181-196), before `TEST_F(LuaExpressionTest, SelectAndRenameAgents)`:
```cpp
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
```
**Fails before:** `lua.run` throws `Failed to run Lua script: ... Cannot apply: labels have same size 1 but different content`, and the test errors on the uncaught exception.

It builds two `quiver.expression(fa)` nodes on purpose, the same pattern `LogicalComposesIfElse` uses, so the test doesn't depend on two branches sharing one file node.

This adds no new `db:`/`quiver.*` name, so `bindings/js/test/lua-api-sync.test.ts` is unaffected.

### Julia: `bindings/julia/test/test_expression.jl`

Insert directly after the `end` that closes `@testset "Label mismatch throws" begin` (currently ~L460-476), before `@testset "Self-save collision throws" begin`:
```julia
    @testset "Single-label operands with different names broadcast" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md_a = make_metadata_full(dimensions = ["row", "col"], dimension_sizes = [2, 2], labels = ["alpha"])
            md_b = make_metadata_full(dimensions = ["row", "col"], dimension_sizes = [2, 2], labels = ["beta"])
            write_dense(path_a, md_a, [:row, :col], [2, 2], 1, (dims, _) -> dims[1] * 10 + dims[2])
            write_dense(path_b, md_b, [:row, :col], [2, 2], 1, (_, _) -> 1.0)

            with_expr(path_a) do a
                with_expr(path_b) do b
                    diff = a - b
                    try
                        # Both operands carry a single label: the lhs label wins.
                        @test Quiver.Binary.get_labels(Quiver.get_metadata(diff)) == ["alpha"]
                        Quiver.save(diff, path_out)
                    finally
                        Quiver.close!(diff)
                    end
                end
            end

            @test read_one_cell(path_out; row = 2, col = 1) == [20.0]  # (2 * 10 + 1) - 1
        finally
            cleanup(path_a, path_b, path_out)
        end
    end
```
**Fails before:** `a - b` throws `Quiver.DatabaseException`, so the testset errors. The existing `"Label mismatch throws"` (`["v1","v2"]` vs `["v1","v3"]`, `@test_throws Quiver.DatabaseException`) and `"Broadcast labels axis (1 label vs 3)"` stay unchanged and must still pass.

### No new schemas

These tests need no `tests/schemas/` files and add no test files, so `tests/AGENTS.md` needs no edit.

## Docs and changelog

### `src/AGENTS.md`: Expression Subsystem section

1. `ExpressionBinary` bullet (currently ~L751). Replace
   > Constructor pre-computes broadcast metadata (`build_broadcast_metadata`) and one `BroadcastOperand` per operand

   with
   > Constructor pre-computes broadcast metadata (`build_broadcast_metadata({&lhs, &rhs}, lhs)`, see the broadcast-metadata bullet below) and one `BroadcastOperand` per operand

2. `ExpressionTernary` bullet (currently ~L753). Replace
   > pre-builds broadcast metadata via `build_ternary_broadcast_metadata` and one `BroadcastOperand` per operand

   with
   > pre-builds broadcast metadata via the same `build_broadcast_metadata({&cond, &then, &else}, then)` and one `BroadcastOperand` per operand

3. "Validation is **eager**" bullet (currently ~L757). Replace
   > (units/dim sizes/time-dim properties/label sizes/initial datetimes for binary and ternary;

   with
   > (units/dim sizes/time-dim properties/label sets/initial datetimes for binary and ternary;

4. Insert this new bullet directly after the "Validation is **eager**" bullet and before the bullet starting "All operation enums are nested in their owning class":
   ```markdown
   - **One broadcast-metadata builder for every arity**: `build_broadcast_metadata(sources, primary)` (`expression_helpers.h`) builds the output metadata of `ExpressionBinary` (`{lhs, rhs}`, primary `lhs`) and `ExpressionTernary` (`{cond, then, else}`, primary `then`). Source order sets the output dimension order: the union of dimension names, first occurrence first, each sized as the max over the sources that have it, with time properties and parent link from the first source that has it (so `ifelse` output dimensions are condition-first). `version` and `unit` come from the primary (a logical op then clears the unit); `initial_datetime` comes from the first source with a time dimension, else from the primary — the pairwise `validate_shape_compatibility` calls already force every time-bearing source to agree, so only that no-time fallback depends on which operand is primary. Labels follow one rule, `broadcast_labels`: every operand with more than one label must carry the same label set, a single-label operand broadcasts whatever its label is called, and when every operand has a single label the output takes the primary's (`{"max"} - {"min"}` is `{"max"}`; `ifelse({"c"}, {"t"}, {"e"})` is `{"t"}`). A mismatch throws `Cannot apply: labels are incompatible across operands (non-singleton label sets must match)`. There used to be a separate two-operand builder whose stricter rule rejected two differently named single labels (so `aggregate_agents("max") - aggregate_agents("min")` threw while `ifelse` over the same operands worked) — don't reintroduce a per-arity copy.
   ```

The file-map line `expression_helpers.h # Shared inline helpers (validation, broadcast metadata/operands, ...)` (currently ~L68) is still accurate and stays. The root `AGENTS.md`, `bindings/julia/AGENTS.md` and `src/c/AGENTS.md` don't mention these helpers or the label rule, so they need no edit.

### Other docs

None. `bindings/js/src/lua-api.ts` (the agent-facing Lua reference) documents no broadcast rules at all (units, shapes or labels), so nothing there is stale. `docs/*.md` and the Julia README don't cover expressions' label handling.

### `CHANGELOG.md`

Under `## [0.12.0] — unreleased`, append this entry as the last item of `### Changed`, after the last existing `### Changed` entry (currently the `export_csv()` quoting one, or whatever plans 01–14 appended after it) and before `### Fixed`. It has no **BREAKING** prefix (maintainer decision):
```markdown
- **Expressions: a binary operation accepts two single-label operands whatever their labels are
  called.** `+ - * /`, the comparisons and `&&`/`||` (Julia and Lua `&`/`|`) threw `Cannot apply:
  labels have same size 1 but different content` when each operand carried one label with a
  different name, while `ifelse` over the same operands worked. So
  `e:aggregate_agents("max") - e:aggregate_agents("min")` failed, and so did combining conditions
  on two single-label files, e.g. `(demand > x) & (price < y)`. Binary operations now follow the
  `ifelse` rule: operands with more than one label must carry the same label set, a single label
  broadcasts, and when every operand has a single label the result takes the left operand's (for
  `ifelse`, the `then` operand's). Every expression that built before builds the same output. A
  label-set mismatch in a binary operation now reports `Cannot apply: labels are incompatible
  across operands (non-singleton label sets must match)`, replacing `labels have same size N but
  different content` and `labels have incompatible sizes N vs M`.
```

## Verification

Run from the repo root (Git Bash). Run the `.bat` scripts from `cmd`/PowerShell, or from Git Bash as `cmd //c "bindings\julia\test\test.bat test_expression.jl"`.

1. `cmake --build build --config Debug`. It must build with no new warnings in `src/expression/`.
2. `./build/bin/quiver_tests.exe --gtest_filter='ExpressionFixture.*:LuaExpressionTest.*'`. All pass, including the six new tests:
   - `ExpressionFixture.LabelSetsOfDifferentSizesThrow`
   - `ExpressionFixture.SingleLabelOperandsWithDifferentNamesBroadcast`
   - `ExpressionFixture.LogicalOnSingleLabelOperandsWithDifferentNames`
   - `ExpressionFixture.IfElseSingleLabelOperandsTakeThenLabels`
   - `ExpressionFixture.IfElseDimensionsFollowConditionAndDatetimeFollowsThen`
   - `LuaExpressionTest.AggregateAgentsMaxMinusMin`
3. `./build/bin/quiver_c_tests.exe --gtest_filter='ExpressionCApiFixture.*'`. All pass, including `ExpressionCApiFixture.SingleLabelOperandsWithDifferentNamesBroadcast`.
4. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe` (the full suites). All pass.
5. `bindings/julia/test/test.bat test_expression.jl`, then `bindings/julia/test/test.bat`. Both pass, including `"Single-label operands with different names broadcast"`. The Julia suite loads the freshly built `build/` libraries, so run step 1 first. No generator run is needed, since the C API is unchanged.
6. `grep -rn "compute_output_labels\|compute_ternary_output_labels\|build_ternary_broadcast_metadata" src include tests bindings --include=*.h --include=*.cpp --include=*.jl --include=*.md`. It must print nothing.
7. `scripts/format.bat`, then `git status`. Only the files this plan touched may show as modified. If clang-format rewrapped the new tests, keep its output.
8. `scripts/test-all.bat`. Steps 1–6 (C++, C API, Julia, Dart, JS, Python) must report PASS. Step 7 (the CLI smoke test) currently fails because `example/example1.lua` no longer exists. That failure predates this plan and is fixed by plan 65; don't fix it here.

## Acceptance criteria

- [x] `src/expression/expression_helpers.h` has exactly one broadcast-metadata builder, `build_broadcast_metadata(std::initializer_list<const BinaryMetadata*>, const BinaryMetadata& primary)`, plus `broadcast_labels(...)`. `compute_output_labels`, `compute_ternary_output_labels` and `build_ternary_broadcast_metadata` no longer exist anywhere in the repo.
- [x] `ExpressionBinary` calls `build_broadcast_metadata({&lhs_meta, &rhs_meta}, lhs_meta)`, and `ExpressionTernary` calls `build_broadcast_metadata({&condition_meta, &then_meta, &else_meta}, then_meta)`.
- [x] Logical ops still clear the unit, and `ifelse` still validates `then`/`else` units and all three shape pairs.
- [x] `{"alpha"} - {"beta"}` builds with labels `{"alpha"}` in C++, the C API, Lua and Julia.
- [x] `{val1,val2}` vs `{val1,val2,val3}` throws `... non-singleton label sets must match`.
- [x] Ternary output is unchanged: condition-first dims, `then` labels when every operand is single-label, `then`'s `initial_datetime` when no operand has a time dim (pinned by the two new ternary tests).
- [x] All pre-existing expression tests pass unchanged in C++, the C API, Lua and Julia.
- [x] `src/AGENTS.md` names the single builder and describes the label rule, and there is no remaining mention of `build_ternary_broadcast_metadata`.
- [x] `CHANGELOG.md` has the non-BREAKING entry under 0.12.0 `### Changed`. (Landed under `[0.12.4]`, the open section. See Implementation notes.)
- [x] `scripts/format.bat` leaves no diff beyond this plan's files.

## Pitfalls

- **Source order is behaviour.** `{&condition_meta, &then_meta, &else_meta}` sets the ternary output dimension order. `{&then_meta, &else_meta, &condition_meta}` would silently reorder the dimensions in every saved `ifelse` `.toml`. The new test `IfElseDimensionsFollowConditionAndDatetimeFollowsThen` catches that.
- **The fallback is `primary`, not `sources.begin()`.** For ternary the first source is `cond`, but the no-time `initial_datetime` fallback and the all-singleton label fallback are `then`. Both have a pinning test.
- **Don't drop `#include <initializer_list>`.** MSVC may compile without it through transitive includes, but GCC/Clang in CI may not.
- **The `initializer_list` holds pointers to the children's metadata.** `lhs_->metadata()` returns a reference to a member of the child node, which the `shared_ptr` keeps alive, and the braced list lives until the end of the call. Don't change `lhs_meta`/`rhs_meta` from `const auto&` to by-value copies taken in a temporary scope.
- **`broadcast_labels` treats `size() <= 1` as a singleton**, mirroring the old ternary `> 1` test. `BinaryMetadata::validate()` rejects 0 labels, so `== 1` would behave identically. Keep `<= 1` so the port is exact.
- **Plan 16 renames the aggregation enums** (`ExpressionAggregateAgents::Operation`, `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_*`, the Julia `Quiver.C.` constants). The new C++, C API and Julia tests deliberately use two files with single labels instead of `aggregate_agents`, so they don't collide. Keep it that way.
- **clang-format** (`ColumnLimit: 120`) may rewrap the long `write_qvr(...)` lines and the Lua raw string is left alone. Run `scripts/format.bat` and accept its output. None of the files touched here are `.bat`, so line endings are not a concern.
- **The Julia tests run against `build/`.** A stale DLL from before step 1 makes the new Julia testset fail even when the C++ is correct, so rebuild first.

## Out of scope

- Collapsing the pairwise `validate_shape_compatibility` calls into an N-ary validator. Their messages name `lhs`/`rhs` and they work; the maintainer asked to keep them.
- Richer label-mismatch messages (naming the two label sets). Not requested; the existing ternary message is kept verbatim.
- Unifying the aggregation enums and parsers: plan **16**.
- Registering the Lua operator metamethods once: plan **51**.
- Documenting expression broadcast rules (units, shapes, labels) in the agent-facing Lua reference `bindings/js/src/lua-api.ts`. It documents none of them today, so nothing there is stale. Plans **43/44** own that file's accuracy fixes.
- The pre-existing CLI smoke-test failure in `scripts/test-all.bat` step 7: plan **65**.

## Implementation notes

Implemented on `rs/plan15`. The branch was already at `master` `784fdc0` when work started (plans
06–14 and the 0.12.4 bump had landed), so `git merge origin/master` was a no-op. A read-only
verification pass ran before any edit: every quoted excerpt, symbol, fixture, helper signature and
insertion anchor matched, in all four test layers, and the three core files were byte-identical to
the ones the plan quotes. An adversarial review (equivalence prover, test auditor, devil's
advocate, each finding re-verified) found no input the library can build on which the new builder
differs from the old ones, beyond the two changes the plan names. The Changes, Tests and Docs were
applied as written, apart from the drift below. No C API, FFI or binding code changed, so no
generator ran.

### Regression proof (tests written first, run against the unchanged `src/`)

- **C++ and Lua** (`--gtest_filter` over the six new tests): four failed and the two ternary pins
  passed, exactly as predicted.
  - `LabelSetsOfDifferentSizesThrow`: the substring check failed on `Cannot apply: labels have
    incompatible sizes 2 vs 3`.
  - `SingleLabelOperandsWithDifferentNamesBroadcast` and
    `LogicalOnSingleLabelOperandsWithDifferentNames`: an uncaught `Cannot apply: labels have same
    size 1 but different content`.
  - `LuaExpressionTest.AggregateAgentsMaxMinusMin`: `Failed to run Lua script: Cannot apply: labels
    have same size 1 but different content ... in metamethod 'sub'`.
  - `IfElseSingleLabelOperandsTakeThenLabels` and
    `IfElseDimensionsFollowConditionAndDatetimeFollowsThen`: passed.
- **C API.** `SingleLabelOperandsWithDifferentNamesBroadcast`: `quiver_expression_apply` returned
  `QUIVER_ERROR` (1) with the same message.
- **Julia.** `"Single-label operands with different names broadcast"` errored with `Got exception
  outside of a @test: Cannot apply: labels have same size 1 but different content` from
  `_binop` (`expression.jl:27`). `runtests.jl` is fail-fast, so the run stopped there (18 passed,
  1 errored).
- After the change all of them pass.

### Verification

- The build shows no warnings in `src/expression/`. The build's other warnings (C4458 in
  `database_impl.h`/`database_update.cpp`, C4100 in `LogicalAndOrNot` in
  `test_c_api_expression.cpp`) were there before this plan.
- `ExpressionFixture.*:LuaExpressionTest.*`: 137/137 pass. `ExpressionCApiFixture.*`: 72/72 pass.
- Full suites: `quiver_tests` 1361/1361, `quiver_c_tests` 563/563, Julia `test_expression.jl`
  186/186.
- The step-6 grep for `compute_output_labels`, `compute_ternary_output_labels` and
  `build_ternary_broadcast_metadata` prints nothing.
- `scripts/test-all.bat`: every suite PASSES. C++ 1361, C API 563, Julia 1489, Dart 426, JS 212,
  Python 309.
- `scripts/format.bat` then leaves only this plan's nine files modified. It did not get there on
  the first run in this fresh checkout. See Environment.

### Drift fixed

- **CHANGELOG section.** The entry is the last bullet of `### Changed` under
  `## [0.12.4] — unreleased`, the open section (0.12.0–0.12.3 are tagged). The plan said 0.12.0.
  It sits after plan 13's `bin_to_csv`/`csv_to_bin` entry and before `### Removed`. The text is the
  plan's, with no BREAKING prefix and no manifest bump.
- **A fourth `src/AGENTS.md` mention of the old helper.** Plan 12 (`4b7fece`) added a sentence to the
  `BinaryMetadata` factories bullet: "Inside the library, `build_broadcast_metadata` /
  `build_ternary_broadcast_metadata` (`expression_helpers.h`) still assemble `dimensions`
  directly." The plan predates it. It now names only `build_broadcast_metadata`, so the acceptance
  criterion "no remaining mention of `build_ternary_broadcast_metadata`" holds.
- **Plan 09's `derive_initial_values()` rule.** Plan 09 landed first and documents: "Any code that
  changes `dimensions` or `initial_datetime` must end with `BinaryMetadata::derive_initial_values()`."
  The broadcast builder copies each time dimension's `TimeProperties` from the first source instead.
  That is still correct. `validate_shape_compatibility` forces a shared time dimension to agree on
  frequency, `initial_value` and parent name, and every time-bearing source to share
  `initial_datetime`, so each copied `initial_value` already equals the derived one. One sentence
  saying so was added to the new "One broadcast-metadata builder" bullet. No call was added, so the
  plan's code is unchanged.
- **Test-all has no step 7 any more.** Commit `01e78d7` (in the plan 06 PR) removed the CLI smoke
  test from `scripts/test-all.bat`, which now runs six steps. The plan's "step 7 currently fails"
  note is stale, and every step passes.
- **Line numbers.** The `src/AGENTS.md` bullets were at ~L849/851/856, not ~L751/753/757. The test
  anchors had moved too (plans 09/10 added tests). Every edit was re-anchored by the quoted text.
- **README.** It lists this plan as depending on "08, 14". That misreads the plan header's "runs
  after 08–14 in numeric order". The dependency is ordering only, and both had landed anyway.

### Kept as written, on purpose

- **`broadcast_labels` keeps `size() <= 1`**, as the plan says. A reviewer pointed out that `== 1`
  would also reject a 0-label operand. With `<= 1`, a hand-built, unvalidated 0-label
  `BinaryMetadata` (only reachable through the public `ExpressionScalar(double, BinaryMetadata)`
  constructor or a custom node) would build and then read past an empty row buffer, and so would a
  duplicate dimension name. Every metadata that files, scalar operators, the C API, Julia and Lua
  produce goes through `validate()`, which rejects both. So this is a contract violation, not a
  reachable bug, and the old ternary had the same hole.
- **The builder comments** are the plan's verbatim text, although they repeat the new
  `src/AGENTS.md` bullet.

### Environment (fresh checkout, not code)

- `build/` was configured from scratch.
- The first `scripts/format.bat` run touched unrelated files:
  - **Dart:** 25 files rewrapped at 80 columns. `.dart_tool` did not exist yet, so `dart format`
    could not resolve `analysis_options.yaml` (`include: package:lints/...`) and ignored
    `page_width: 120`. After reverting and running `dart pub get`, it changes 0 files.
  - **Python:** `uv` failed while creating `.venv` with "Failed to update Windows PE resources".
    Deleting the half-created `.venv` and running `uv sync` again fixed it.
  - **JS:** `biome` was not installed. After `bun install`, it "fixed" all 42 files, but only by
    rewriting the CRLF working copies (`core.autocrlf=true`) as LF. `git diff` showed no content
    change, and `git checkout -- bindings/js` restored them.

### For later plans

- **Plan 16** edits the aggregation templates right below the new builder in
  `expression_helpers.h`; one blank line separates the two blocks. Plan 16's anchor (`template
  <typename Op> std::string aggregation_operation_label`) is unchanged. In `src/AGENTS.md`, the new
  broadcast bullet sits directly above the "All operation enums are nested in their owning class"
  bullet that plan 16 replaces. That is an adjacent-line conflict if the two are merged in
  parallel: keep both.
- **Plan 51** re-registers the Lua operator metamethods. `LuaExpressionTest.AggregateAgentsMaxMinusMin`
  relies on `Expression - Expression` (`__sub`) and must keep passing.
- **Noticed, not owned.** When two time dimensions each appear in only one operand (A has only
  `year`, B has only `month`, same `initial_datetime`), both the old and the new builder output two
  root time dimensions (parent -1 each). `validate()` accepts that, but `from_toml_content` always
  chains time dimensions, so the saved `.toml` reloads with `month` as a child of `year`: a
  different parent and a different derived `initial_value`. This plan keeps the behaviour exactly.
  It is a candidate for a follow-up.
