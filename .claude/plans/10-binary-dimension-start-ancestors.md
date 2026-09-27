# 10 — Binary: one dimension-start helper that walks the whole parent chain

**Batch** 2 · **Severity** medium · **Breaking** no. This is a bug fix. One side effect: a CSV that the old `bin_to_csv` wrote for an affected file is missing rows, and `csv_to_bin` now rejects it (see Changelog). · **Size** S · **Layers** C++ core (`src/binary/iteration.cpp`, `include/quiver/binary/iteration.h`, `src/expression/expression_aggregate.cpp`); tests in C++, C API, Lua and Julia; `src/AGENTS.md`; `CHANGELOG.md`
**Depends on** none. The fix and its tests do not need 08 or 09, but this plan runs after them in numeric order, so every anchor below is a quoted excerpt, not a line number. · **Overlaps with** 08 (edits `src/binary/` and may add tests to `tests/test_iteration.cpp`), 09 (edits the `ExpressionAggregate` constructor in the same file, a different function, and edits nearby `src/AGENTS.md` text), 16 (later renames the aggregation enum that the new tests use)

## Why

A `.qvr` file whose `initial_datetime` falls mid-period starts its inner time dimensions at a value other than 1. For example, `2025-03-15` under `yearly × monthly × daily` gives initial values `(1, 3, 15)` (`compute_time_dimension_initial_values`, `src/binary/binary_metadata.cpp`). Only the **first** period, March 2025, should start at day 15. The rule that says "this dimension resumes at its `initial_value`" is written twice, and both copies check only the **immediate parent**:

`src/binary/iteration.cpp`, `next_dimensions` (currently ~L115-129):
```cpp
    // Restore initial_value when a child time dim wrapped to 1 but its parent
    // is still at its own initial_value: ...
    for (size_t i = 0; i < next.size(); ++i) {
        const auto& dim = dimensions[i];
        if (!dim.is_time_dimension())
            continue;
        auto initial_value = dim.time->initial_value;
        auto parent_idx = dim.time->parent_dimension_index;  // -1 = no parent
        if (next[i] < initial_value && parent_idx != -1 &&
            next[parent_idx] == dimensions[parent_idx].time->initial_value) {
            next[i] = initial_value;
        }
    }
```

`src/expression/expression_aggregate.cpp`, `ExpressionAggregate::compute_row` (currently ~L97-112):
```cpp
    const auto& reduced_dim = operand_meta.dimensions[reduced_operand_index_];
    int64_t start = 1;
    int64_t end = reduced_dim.size;
    if (reduced_dim.is_time_dimension()) {
        const auto& tp = *reduced_dim.time;
        const int64_t parent_idx = tp.parent_dimension_index;
        const auto sizes = dimension_sizes_at_values(operand_meta, operand_dims_buf_);
        end = sizes[reduced_operand_index_];
        if (parent_idx < 0) {
            start = tp.initial_value;
        } else {
            const auto& parent_dim = operand_meta.dimensions[parent_idx];
            const int64_t parent_initial = parent_dim.is_time_dimension() ? parent_dim.time->initial_value : 1;
            start = (operand_dims_buf_[parent_idx] == parent_initial) ? tp.initial_value : 1;
        }
    }
```

**Reproduction** (traced against HEAD 58dfe7a): yearly(2) × monthly(12) × daily(31), `initial_datetime = 2025-03-15T00:00:00`.
- `next_dimensions(md, {2, 2, 28})`: `dimension_sizes_at_values` finds that Feb 2026 has 28 days. The cascade gives `(2, 3, 1)`. The restore then sees `day 1 < 15 && month == 3` and returns **`(2, 3, 15)`**, although the year is 2, not 1. Cells `(2, 3, 1..14)`, which are the dates 2026-03-01..14, are never visited. `BinaryFile::validate_dimension_values` accepts every one of those cells. For example, `(2,3,1)` maps to 2026-03-15 − 14 days = 2026-03-01.
  - A full walk visits 643 cells instead of 657 (292 days from 2025-03-15 to 12-31, plus 365 days of 2026).
  - `Expression::save` (`src/expression/expression.cpp`) writes through this walk, so the skipped cells stay NaN (`fill_file_with_nulls`).
  - `CSVConverter::bin_to_csv` and `csv_to_bin` (`src/binary/csv_converter.cpp`) leave those rows out or expect them to be missing.
- `aggregate("day")` computing output `(2, 3)`: `operand_dims_buf_[month] == 3 == parent_initial`, so `start = 15`, and the March 2026 sum covers days 15..31 (**17**) instead of 31 days.
- The same bug with a January start: from `2025-01-15`, the month's initial value is 1, so every later January skips days 1..14.

Layouts with at most two time dimensions are unaffected. The parent is then the outermost time dimension, so "every ancestor" and "the parent" mean the same thing. No existing test reaches the bug: `tests/test_iteration.cpp` has only monthly+daily from 2025-01-01, and every 3- and 4-level fixture in the repo is metadata-only.

Principles this breaks:
- `src/AGENTS.md` › Iteration Helpers calls `first_dimensions`/`next_dimensions` the "single source of truth for `.qvr` traversal". The aggregate keeps a second copy of the start rule, and both copies are wrong.
- Data is lost silently: NaN cells, missing CSV rows and wrong sums, with no error.

## Constraints and decisions

- **The binary and expression subsystems exist only in C++/C API, Julia and Lua** (root AGENTS.md, Design Decisions). No Dart, Python or JS change or test.
- **No C API, FFI or binding code changes.** The helper is C++-only, like `dimension_sizes_at_values`. Nothing to regenerate: no Julia `c_api.jl` regeneration, no edit to Python `_c_api.py`, JS `loader.ts` or Dart `bindings.dart`. The behaviour does reach the C API, Lua and Julia through `aggregate`/`save`, so each of those layers gets one small end-to-end test.
- **Hot path** (`src/AGENTS.md` › Performance Bottlenecks): `next_dimensions` runs once per cell. The new ancestor walk costs at most 4 integer comparisons per dimension, which is negligible next to the date arithmetic `dimension_sizes_at_values` already does in the same call. `validate_dimension_values` is not touched.
- **Clean over defensive** (root AGENTS.md, Principles): `parent_dimension_index` always points to an earlier time dimension. `from_toml_content` sets it to the previous time dimension, and the `ExpressionAggregate` constructor rewires it to the grandparent or `-1`. So the chain always ends at `-1`: no cycle guard, and no `is_time_dimension()` check on an ancestor. The verifiers' corrected proposals agree: the old `parent_dim.is_time_dimension() ? … : 1` check in the aggregate can never fail, so it is deleted.
- **Changelog** (root AGENTS.md, Principles): a user-visible fix goes under `## [0.12.0] — unreleased` › `### Fixed`. It is not breaking, and no manifest bump is needed (0.12.0 is already the unreleased minor).
- **Self-updating** (root AGENTS.md, Principles): update `src/AGENTS.md` (file map + Iteration Helpers).
- **No new error messages.** The three message patterns do not come into play.
- Maintainer notes for this item: none. Both verifiers upheld the finding. Their refinements are applied here:
  - Evaluate the helper on `next`, after earlier dimensions have been restored, in ascending order.
  - Write the restore as `std::max`.
  - Replace the aggregate's whole start branch.
  - Use a midnight start so the tests do not depend on plan 08's time-of-day fix.

Alternatives considered and rejected:
- **A vector version `dimension_starts_at_values(meta, values)` that mirrors `dimension_sizes_at_values`.** It would compute every start from a snapshot taken before the restore. That is wrong when an ancestor is restored in the same pass. With `[scenario, year, month, day]` from 2025-03-15, rolling over the scenario gives `(2,1,1,1)`. The month must first become 3, and only then does the day see "all ancestors at their start" and become 15. A per-index function called in ascending order handles this.
- **Fix the chain walk in both places.** Two copies of one rule is how the same bug ended up in two places.
- **A date-based test ("is this coordinate's date inside the period of `initial_datetime`?").** It is equivalent, but costs date arithmetic per step, while the integer comparison along the ancestor chain is exact.
- **An internal header in `src/binary/`.** `ExpressionAggregate` is in `src/expression/`, and the helper's sibling `dimension_sizes_at_values` is already declared in the public `iteration.h` for the same caller. Keep the two together.
- **Also rewriting `first_dimensions` on top of the helper.** It already returns `initial_value` for every time dimension, which is what the helper gives at the first coordinate. Rewriting it would be churn.

## Changes

### 1. `include/quiver/binary/iteration.h` — declare the helper

Current (currently ~L7-19):
```cpp
#include <cstdint>
#include <optional>
#include <vector>
...
QUIVER_API std::vector<int64_t> dimension_sizes_at_values(const BinaryMetadata& meta,
                                                          const std::vector<int64_t>& dimension_values);
```
New: add `#include <cstddef>` above `#include <cstdint>`, and add this declaration directly after `dimension_sizes_at_values`:
```cpp
QUIVER_API int64_t dimension_start_at_values(const BinaryMetadata& meta,
                                             const std::vector<int64_t>& dimension_values,
                                             size_t index);
```
Why: the helper is called from `iteration.cpp` and `expression_aggregate.cpp`. `QUIVER_API` matches the other three declarations.

### 2. `src/binary/iteration.cpp` — define the helper; `next_dimensions` uses it

2a. Add `#include <algorithm>` as the first system include (before `#include <chrono>`), for `std::max`.

2b. Add the definition between `dimension_sizes_at_values` and `first_dimensions`. Anchor: insert immediately before `std::vector<int64_t> first_dimensions(const BinaryMetadata& meta) {`:
```cpp
int64_t dimension_start_at_values(const BinaryMetadata& meta,
                                  const std::vector<int64_t>& dimension_values,
                                  size_t index) {
    const auto& dim = meta.dimensions[index];
    if (!dim.is_time_dimension())
        return 1;

    // Only the period the file starts in begins at initial_value (day 15 of March 2025 for a
    // 2025-03-15 start), and a coordinate is in that period exactly when every time ancestor, up
    // the parent_dimension_index chain, is at its own initial_value. Checking the parent alone
    // restarted every later March at day 15 as well.
    for (auto ancestor = dim.time->parent_dimension_index; ancestor != -1;
         ancestor = meta.dimensions[ancestor].time->parent_dimension_index) {
        if (dimension_values[ancestor] != meta.dimensions[ancestor].time->initial_value)
            return 1;
    }
    return dim.time->initial_value;
}
```

2c. `next_dimensions`: delete the now-unused first line of the body:
```cpp
    const auto& dimensions = meta.dimensions;
```
Then replace the whole restore block, from the comment `// Restore initial_value when a child time dim wrapped to 1 but its parent` through the closing `}` of its `for` loop (quoted in full under Why), with:
```cpp
    // The cascade resets every wrapped dimension to 1, but a file that starts mid-period
    // (2025-03-15 -> month 3, day 15) must resume at initial_value wherever the walk re-enters
    // that starting period: e.g. when a non-time outer dimension (a scenario) rolls over, but not a
    // year later. Ascending order matters: an ancestor restored earlier in this pass is the value
    // its descendants compare against.
    for (size_t i = 0; i < next.size(); ++i) {
        next[i] = std::max(next[i], dimension_start_at_values(meta, next, i));
    }
```
The rest of `next_dimensions` stays unchanged: the `current_sizes` line, the right-to-left cascade, `if (!incremented) return std::nullopt;` and `return next;`.

Why each part holds:
- A non-time dimension has start 1, and `max(v, 1) == v` because every value is ≥ 1.
- The outermost time dimension has start `initial_value`. `from_toml_content` always sets that to 1, and after plan 09 so does the aggregate's recompute, so `max` changes nothing there either. The old code skipped that dimension, so the behaviour is the same.
- Dimensions to the left of the one that was incremented keep their values, and so do their ancestors, so they are already at or above their start. Only dimensions the cascade reset to 1 can move.

### 3. `src/expression/expression_aggregate.cpp` — `ExpressionAggregate::compute_row` uses the helper

Keep everything up to and including this line (currently ~L95):
```cpp
    operand_dims_buf_[reduced_operand_index_] = 1;
```
Replace the whole block quoted under Why, from `const auto& reduced_dim = operand_meta.dimensions[reduced_operand_index_];` through the closing `}` of `if (reduced_dim.is_time_dimension())`, with:
```cpp
    // Reduce over exactly the cells the traversal visits at this coordinate: from where
    // next_dimensions starts the reduced dimension to its actual size here (Feb = 28, ...).
    const int64_t start = dimension_start_at_values(operand_meta, operand_dims_buf_, reduced_operand_index_);
    const int64_t end = dimension_sizes_at_values(operand_meta, operand_dims_buf_)[reduced_operand_index_];
```
The accumulation loop `for (int64_t v = start; v <= end; ++v)` stays unchanged.

Why:
- `dimension_sizes_at_values` returns `dim.size` for a non-time dimension, which is the old `end = reduced_dim.size`. So the `is_time_dimension()` branch is no longer needed.
- The helper returns 1 for a non-time dimension and `initial_value` for an outermost time dimension. That is the old `parent_idx < 0` case.
- The helper reads only the reduced dimension's ancestors, never the reduced slot itself, so setting that slot to 1 first is harmless. `dimension_sizes_at_values` still needs it set.
- The extra `dimension_sizes_at_values` call for a non-time reduction is one small vector per *output* row. Each output row already does `size` operand reads, so the cost does not matter.
- `reduced_operand_index_` is an `int`. Passing it as `size_t` is the same implicit conversion the existing `operand_dims_buf_[reduced_operand_index_]` does, so no cast is needed.
- If plan 09 has edited this block, for example if the start is no longer read from `tp.initial_value`: still replace the whole start/end computation with the two lines above. Plan 09 keeps the stored `initial_value` field (maintainer decision), and that field is what the helper reads.

No other file changes. `first_dimensions`, `dimension_sizes_at_values`, `csv_converter.cpp`, `expression.cpp`, the C API, `lua_runner.cpp` and the Julia sources all stay unchanged.

## Tests

Write the tests first, build, and confirm they fail as described. Then apply Changes 1-3.

### C++ — `tests/test_iteration.cpp` (append at end of file, after `NextDimensionsTraversesTimeMetadata`)

```cpp
TEST(IterationTest, NextDimensionsResumesMidPeriodStartOnlyInTheStartingPeriod) {
    // yearly(2) x monthly(12) x daily(31) from 2025-03-15: initial values (1, 3, 15). Only March
    // 2025 starts on the 15th; March 2026 is a whole month. Checking only the parent (month == 3)
    // used to resume day 15 there too, skipping 2026-03-01..14.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-03-15T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"year", "month", "day"})
                                               .set("dimension_sizes", {2, 12, 31})
                                               .set("time_dimensions", {"year", "month", "day"})
                                               .set("frequencies", {"yearly", "monthly", "daily"})
                                               .set("labels", {"v"}));
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 3, 15}));

    auto after_feb_2026 = next_dimensions(md, std::vector<int64_t>{2, 2, 28});
    ASSERT_TRUE(after_feb_2026.has_value());
    EXPECT_EQ(*after_feb_2026, (std::vector<int64_t>{2, 3, 1}));

    size_t count = 1;
    std::vector<int64_t> dims = first_dimensions(md);
    while (auto nxt = next_dimensions(md, dims)) {
        dims = *nxt;
        ++count;
    }
    // 2025-03-15..2025-12-31 = 292 days, plus the 365 days of 2026.
    EXPECT_EQ(count, 657u);
}

TEST(IterationTest, NextDimensionsResumesMidPeriodStartWhenANonTimeOuterDimensionRollsOver) {
    // scenario(2) x monthly(2) x daily(31) from 2025-01-05: each scenario restarts the calendar,
    // so scenario 2 resumes on day 5, not day 1. This is why the restore step exists.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-05T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"scenario", "month", "day"})
                                               .set("dimension_sizes", {2, 2, 31})
                                               .set("time_dimensions", {"month", "day"})
                                               .set("frequencies", {"monthly", "daily"})
                                               .set("labels", {"v"}));
    auto second_scenario = next_dimensions(md, std::vector<int64_t>{1, 2, 28});
    ASSERT_TRUE(second_scenario.has_value());
    EXPECT_EQ(*second_scenario, (std::vector<int64_t>{2, 1, 5}));

    size_t count = 1;
    std::vector<int64_t> dims = first_dimensions(md);
    while (auto nxt = next_dimensions(md, dims)) {
        dims = *nxt;
        ++count;
    }
    // Per scenario: 2025-01-05..31 (27 days) + February 2025 (28 days) = 55.
    EXPECT_EQ(count, 110u);
}
```
- Before the fix, the first test fails in two places: `*after_feb_2026` is `{2, 3, 15}`, and `count` is 643.
- The second test passes both before and after the fix. It guards against over-correction: dropping the restore, or making it compare against a snapshot taken before the restore.

### C++ — `tests/test_expression.cpp` (insert immediately after the `TEST_F(ExpressionFixture, AggregateSumOverTimeDimVariable)` block, before `TEST_F(ExpressionFixture, AggregateSumSkipsNaNs)`)

```cpp
TEST_F(ExpressionFixture, AggregateSumOverInnermostTimeDimFromMidPeriodStart) {
    // year(2) x month(12) x day(31) from 2025-03-15. Only March 2025 starts on the 15th, so the
    // March 2026 sum must cover all 31 days. Both the walk that writes the input (write_qvr) and
    // the aggregate window used to start every March at day 15.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-03-15T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"year", "month", "day"})
                                               .set("dimension_sizes", {2, 12, 31})
                                               .set("time_dimensions", {"year", "month", "day"})
                                               .set("frequencies", {"yearly", "monthly", "daily"})
                                               .set("labels", {"v1"}));
    // Every visited cell is 1.0, so each output cell counts the days summed.
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("day", ExpressionAggregate::Operation::Sum).save(path_out);

    // Output [year, month] walks Mar..Dec 2025, then Jan..Dec 2026.
    auto vo = read_all_cells(path_out);
    EXPECT_EQ(vo, (std::vector<double>{17, 30, 31, 30, 31, 31, 30, 31, 30, 31,
                                        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}));
}
```
Before the fix, element 12 (March 2026) is 17. Both halves of the fix are needed: `write_qvr` walks with `next_dimensions`, and the sum comes from `compute_row`.

### C API — `tests/test_c_api_expression.cpp` (insert immediately before `TEST_F(ExpressionCApiFixture, AggregateAgentsSumReducesLabels) {`)

The test writes cells directly rather than walking, so it isolates the `compute_row` half. It uses the fixture's existing `make_metadata_v`, `expr_from_file` and `read_one_cell` (which reads with `allow_nulls=1`).
```cpp
TEST_F(ExpressionCApiFixture, AggregateSumOverInnermostTimeDimFromMidPeriodStart) {
    // year(2) x month(12) x day(31) from 2025-03-15: only March 2025 starts on the 15th, so the
    // March 2026 sum covers all 31 days, not 15..31.
    auto* md = make_metadata_v({"year", "month", "day"},
                               {2, 12, 31},
                               {"v1"},
                               "MW",
                               "2025-03-15T00:00:00",
                               {"year", "month", "day"},
                               {"yearly", "monthly", "daily"});
    quiver_binary_file_t* f = nullptr;
    ASSERT_EQ(quiver_binary_file_open_file(path_a.c_str(), 'w', md, &f), QUIVER_OK);
    quiver_binary_metadata_free(md);  // open_file copies the metadata
    const char* dim_names[] = {"year", "month", "day"};
    const double one[] = {1.0};
    for (int64_t day = 15; day <= 31; ++day) {
        int64_t dim_values[] = {1, 3, day};
        ASSERT_EQ(quiver_binary_file_write(f, dim_names, dim_values, 3, one, 1), QUIVER_OK);
    }
    for (int64_t day = 1; day <= 31; ++day) {
        int64_t dim_values[] = {2, 3, day};
        ASSERT_EQ(quiver_binary_file_write(f, dim_names, dim_values, 3, one, 1), QUIVER_OK);
    }
    ASSERT_EQ(quiver_binary_file_close(f), QUIVER_OK);

    auto* a = expr_from_file(path_a);
    quiver_expression_t* agg = nullptr;
    ASSERT_EQ(quiver_expression_aggregate(a, "day", QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM, nullptr, &agg),
              QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(agg, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(agg);

    EXPECT_DOUBLE_EQ(read_one_cell(path_out, {"year", "month"}, {1, 3})[0], 17.0);  // 2025-03-15..31
    EXPECT_DOUBLE_EQ(read_one_cell(path_out, {"year", "month"}, {2, 3})[0], 31.0);  // all of March 2026
}
```
Before the fix, `(2, 3)` reads 17.0.

### Lua — `tests/test_lua_expression.cpp` (insert immediately before `TEST_F(LuaExpressionTest, AggregateUnknownOpThrows) {`)

```cpp
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
```
Before the fix, `lua.run` throws `Failed to run Lua script: ... March 2026 is a whole month`.

### Julia — `bindings/julia/test/test_expression.jl`

Insert immediately before the comment block
```julia
    # ==========================================================================
    # Aggregation: label-axis reduction (Quiver.aggregate_agents)
```
(that is, right after the `@testset "Aggregate percentile out of range throws"` block ends):
```julia
    @testset "Aggregate sum over innermost time dim from mid-period start" begin
        # year x month x day from 2025-03-15: only March 2025 starts on the 15th, so the
        # March 2026 sum covers all 31 days.
        path_a, path_out = make_path("a"), make_path("out")
        try
            md = make_metadata_full(
                dimensions = ["year", "month", "day"],
                dimension_sizes = [2, 12, 31],
                labels = ["v1"],
                initial_datetime = "2025-03-15T00:00:00",
                time_dimensions = ["year", "month", "day"],
                frequencies = ["yearly", "monthly", "daily"],
            )
            file = Quiver.Binary.open_file(path_a; mode = 'w', metadata = md)
            for day in 15:31
                Quiver.Binary.write!(file; data = [1.0], year = 1, month = 3, day = day)
            end
            for day in 1:31
                Quiver.Binary.write!(file; data = [1.0], year = 2, month = 3, day = day)
            end
            Quiver.Binary.close!(file)
            with_expr(path_a) do e
                out = Quiver.aggregate(e, "day", Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM)
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            @test read_one_cell(path_out; year = 1, month = 3)[1] == 17.0  # 2025-03-15..31
            @test read_one_cell(path_out; year = 2, month = 3)[1] == 31.0  # all of March 2026
        finally
            cleanup(path_a, path_out)
        end
    end
```
Before the fix, the second `@test` sees 17.0.

### Existing tests
None need to change. I checked every iteration and aggregate test (`IterationTest.*`, `ExpressionFixture.Aggregate*`, `ExpressionCApiFixture.Aggregate*`, `LuaExpressionTest.Aggregate*`, and the Julia aggregate testsets). Each uses at most two time dimensions or a start on the 1st, where the old and new rules agree. All 22 `IterationTest.*:ExpressionFixture.Aggregate*` tests pass at HEAD. No schema files are involved.

## Docs and changelog

### `src/AGENTS.md`
1. File map, `include/quiver/binary/` block. Old:
   `  iteration.h                 # first_dimensions, next_dimensions, dimension_sizes_at_values`
   New:
   `  iteration.h                 # first_dimensions, next_dimensions, dimension_sizes_at_values, dimension_start_at_values`
2. File map, `src/binary/` block. Old:
   `  iteration.cpp               # first_dimensions/next_dimensions impls + dimension_sizes_at_values`
   New:
   `  iteration.cpp               # first_dimensions/next_dimensions impls + dimension_sizes_at_values/dimension_start_at_values`
3. `### Iteration Helpers`. Replace the `next_dimensions` bullet
   `- \`next_dimensions(meta, current)\` — next position via right-to-left cascade; returns \`nullopt\` at end. Uses \`dimension_sizes_at_values\` to handle variable-length time dims (Feb=28/29, Jan=31, etc.)`
   with:
   `- \`next_dimensions(meta, current)\` — next position via right-to-left cascade; returns \`nullopt\` at end. Uses \`dimension_sizes_at_values\` to handle variable-length time dims (Feb=28/29, Jan=31, etc.), then lifts each reset dimension to \`dimension_start_at_values\` in ascending index order (an ancestor restored earlier in the same pass is what its descendants compare against).`
   After the `dimension_sizes_at_values` bullet, add:
   `- \`dimension_start_at_values(meta, values, index)\` — where dimension \`index\` starts at that coordinate: its \`initial_value\` if it is a time dim and **every** time ancestor on the \`parent_dimension_index\` chain is at its own \`initial_value\` (the period the file starts in), else \`1\`. It is the only place the mid-period start rule is written: \`next_dimensions()\` and \`ExpressionAggregate::compute_row\` both call it, so an aggregate reduces over \`[dimension_start_at_values, dimension_sizes_at_values]\`, exactly the cells the traversal visits. Checking only the immediate parent (the old rule, which both places had) restarted every later March of a \`yearly × monthly × daily\` file starting 2025-03-15 at day 15.`

No other doc mentions this rule. `docs/*.md`, `bindings/julia/AGENTS.md` and `bindings/js/src/lua-api.ts` need no change; `lua-api.ts` mentions `initial_value` only as a field of `get_dimensions()`, and no Lua binding is added.

### `CHANGELOG.md`
Under `## [0.12.0] — unreleased` › `### Fixed`, add this as the **last** bullet of that list, immediately before the line `## [0.10.9] — 2026-09-25`:
```markdown
- **Binary files with three or more time dimensions that start mid-period no longer skip cells.**
  For a `yearly × monthly × daily` file starting `2025-03-15`, the traversal behind an expression
  `save`, `bin_to_csv` and `csv_to_bin` resumed every later March at day 15, so 2026-03-01..14 were
  never visited. A saved expression left them NaN, `bin_to_csv` left their rows out, and
  `aggregate("day")` summed March 2026 from the 15th only. This affected C++, the C API, Julia and
  Lua. A time dimension now resumes at its starting value only while every enclosing time
  dimension is still in the starting period. Files with one or two time dimensions are
  unaffected. A CSV that `bin_to_csv` wrote for an affected file lacks those rows, so `csv_to_bin`
  now rejects it: convert the `.qvr` again.
```

## Verification

From the repo root, in order:
1. Add the tests only, build (`cmake --build build --config Debug`), and confirm the new tests fail as described:
   - `./build/bin/quiver_tests.exe --gtest_filter='IterationTest.NextDimensionsResumesMidPeriodStartOnlyInTheStartingPeriod:ExpressionFixture.AggregateSumOverInnermostTimeDimFromMidPeriodStart:LuaExpressionTest.AggregateSumOverInnermostTimeDimFromMidPeriodStart'` should show 3 failures.
   - `./build/bin/quiver_c_tests.exe --gtest_filter='ExpressionCApiFixture.AggregateSumOverInnermostTimeDimFromMidPeriodStart'` should show 1 failure.
   - `IterationTest.NextDimensionsResumesMidPeriodStartWhenANonTimeOuterDimensionRollsOver` passes, as it should.
2. Apply Changes 1-3, then run `cmake --build build --config Debug`.
3. `./build/bin/quiver_tests.exe --gtest_filter='IterationTest.*:ExpressionFixture.*:LuaExpressionTest.*:LuaBinaryTest.*:CSVConverterFixture.*:BinaryTempFileFixture.*:TimeProperties*'`. Everything passes, including the 2 new `IterationTest` tests, `ExpressionFixture.AggregateSumOverInnermostTimeDimFromMidPeriodStart` and `LuaExpressionTest.AggregateSumOverInnermostTimeDimFromMidPeriodStart`.
4. `./build/bin/quiver_c_tests.exe --gtest_filter='ExpressionCApiFixture.*:BinaryCApiFixture.*:BinaryCApiCSVFixture.*'`. All pass.
5. `bindings/julia/test/test.bat test_expression.jl`, then `bindings/julia/test/test.bat`. All pass, including "Aggregate sum over innermost time dim from mid-period start". No generator run is needed, since the C API is unchanged.
6. `scripts/format.bat`. Then run `git status`: only the files this plan touches should show as changed. If the formatter rewrote unrelated files, revert those.
7. `scripts/test-all.bat`. The six suites must pass. Step 7/7, the CLI smoke test, fails at this HEAD because `example/example1.lua` no longer exists. Plan 65 fixes that, so ignore it here.

## Acceptance criteria

- [ ] `dimension_start_at_values(meta, values, index)` is declared in `include/quiver/binary/iteration.h` with `QUIVER_API` and defined in `src/binary/iteration.cpp`.
- [ ] `next_dimensions` restores through the helper in ascending order, evaluated on `next`. The old parent-only restore block and the unused `dimensions` local are gone.
- [ ] `ExpressionAggregate::compute_row` computes `start`/`end` with the helper and `dimension_sizes_at_values` only. The `reduced_dim`/`tp`/`parent_idx`/`parent_initial` code is gone.
- [ ] Nowhere else in `src/` compares a parent's value against its `initial_value`: `grep -n "parent_initial\|parent_idx" src/binary/iteration.cpp src/expression/expression_aggregate.cpp` prints nothing. The `parent_idx` names in `expression_helpers.h` are broadcast-metadata code and are unrelated.
- [ ] The 2 new `IterationTest` tests, and the new aggregate tests in C++, C API, Lua and Julia, pass. The 4 aggregate tests and the first iteration test failed before the fix.
- [ ] All existing binary, expression, iteration and CSV-converter tests pass unchanged.
- [ ] `src/AGENTS.md` file map and Iteration Helpers are updated. The CHANGELOG `### Fixed` bullet is added under 0.12.0.
- [ ] `scripts/format.bat` is clean. No `.bat` file is touched.

## Pitfalls

- **Keep the restore loop in ascending order, and pass `next`, not `current` or a copy.** A dimension's start depends on its ancestors' values *after* they have been restored in the same pass. With `[scenario, year, month, day]` the month must become 3 before the day can see "all ancestors at start". A precomputed vector of starts would be wrong.
- **`std::max` needs `<algorithm>`.** Include it explicitly rather than relying on transitive includes, which differ across MSVC, MinGW and the Linux/macOS toolchains CI builds with.
- **Plan 08 may have added 3-level walk tests with a non-midnight start** (e.g. `monthly × daily × hourly` from `2025-01-01T06:00:00`). Under the old rule the hour is restored to 7 whenever the day is 1, so every month's first day loses its hours before 06:00. If a plan-08 test hard-coded a cell count, that count reflects the bug and is now too small. Recompute it as every calendar hour from the start to the end of the last period; do not revert this fix. Run `grep -n "hourly" tests/test_iteration.cpp tests/test_binary_file.cpp` after rebasing onto plan 08 to find such tests.
- **Plan 09 edits the `ExpressionAggregate` constructor** (rebasing `initial_datetime`, recomputing initial values) in the same file. This plan edits only `compute_row`. If plan 09 also touched the start block, replace whatever is there with the two helper lines (Change 3).
- **Keep the test start at midnight and before day 29.** A non-midnight start depends on plan 08's `add_offset_from_int` fix. A day ≥ 29 start adds calendar normalization noise: see Out of scope for the Feb-29 case.
- **The new tests use the current aggregation enum spellings:**
  - C++: `ExpressionAggregate::Operation::Sum`
  - C: `QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM`
  - Julia: `Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM`

  Plan 16 renames them later and owns updating these tests with the rest.
- **gtest and `std::optional`:** the iteration tests `ASSERT_TRUE(opt.has_value())` and then compare `*opt`. That avoids relying on gtest's optional printer.
- `scripts/format.bat` also runs JuliaFormatter, which may re-wrap the new Julia testset. That is expected.

## Out of scope

- Parent-aware time-coordinate validation, time of day in `add_offset_from_int`, and the weekly grid are owned by **08**.
- A stale `initial_value` after `aggregate` and rebasing the output `initial_datetime` are owned by **09**.
- Unifying the aggregation enum is owned by **16**.
- Making `validate_dimension_values` opt-in for performance (src/AGENTS.md Performance Bottlenecks) is not planned.
- **Noticed, not owned by any listed plan:** a `yearly × monthly × daily` file starting on `2024-02-29` cannot be written past year 1. `add_offset_from_int` Yearly turns `2025-02-29` into `2025-03-01` before the Monthly offset runs, so coordinate `(2,1,1)` maps to 2025-01-04, and `validate_dimension_values` throws. This belongs with plan 08's `add_offset_from_int` work. It is flagged for the maintainer and not fixed here.
