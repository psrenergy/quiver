# 09 — Binary: recompute initial_value in one place and rebase aggregate output start date

**Batch** 2 · **Severity** high (silent data corruption in saved expression output) · **Breaking** yes, for C++ callers only: `TimeProperties::set_initial_value()` is removed. Behaviour also changes: an aggregate that removes the outermost time dimension now reports a different, correct `initial_datetime`. The C API, the FFI struct shape, and the Julia and Lua surfaces are unchanged · **Size** M · **Layers** C++ core (`include/quiver/binary/{time_properties,binary_metadata}.h`, `src/binary/{time_properties,binary_metadata}.cpp`, `src/expression/expression_aggregate.cpp`); new tests in C++, C API, Lua and Julia; root `CLAUDE.md`, `src/CLAUDE.md`, `CHANGELOG.md`
**Depends on** 08 · **Overlaps with**
- **08:** rewrites the initial-value calculation in `src/binary/binary_metadata.cpp` and moves `validate()` ahead of it in `from_toml_content`. It also makes `add_offset_from_int` keep the time of day, and it may change `datetime_to_int` in `time_properties.h/.cpp`.
- **10:** edits `ExpressionAggregate::compute_row` in the same file as this plan (a different function). It adds tests near the ones added here and edits nearby `src/CLAUDE.md` text.
- **11:** moves the tail of `from_toml_content`, including the `derive_initial_values()` call added here, into an anonymous `build_metadata` function.
- **12:** deletes `add_time_dimension` and `quiver_binary_metadata_set_initial_datetime`. It appends to the `### Removed` section this plan may create, and edits the `BinaryMetadata` bullet list in `src/CLAUDE.md`.
- **15:** rewrites the broadcast helpers in `expression_helpers.h`. The `initial_value` comparison there stays.
- **16:** renames the aggregation enum spellings used by the new tests.

## Why

`TimeProperties::initial_value` is the coordinate of `initial_datetime` in a time dimension. The outermost time dimension always gets 1. An inner one gets its position inside its parent: month-of-year, day-of-month, hour-of-day, and so on. The field is **stored** (`include/quiver/binary/time_properties.h`, currently ~L22):
```cpp
struct QUIVER_API TimeProperties {
    TimeFrequency frequency;
    int64_t initial_value;
    int64_t parent_dimension_index;

    // Setters
    void set_initial_value(int64_t initial_value);
```
Only one place computes it: the tail of `BinaryMetadata::from_toml_content` (`src/binary/binary_metadata.cpp`, currently ~L332-342), through the anonymous-namespace `compute_time_dimension_initial_values`, which starts with `initial_values.push_back(1);  // The largest time dimension always starts at 1` (~L24):
```cpp
    time_dim_index = 0;

    // Compute and set initial values for time dimensions
    std::vector<int64_t> initial_values =
        compute_time_dimension_initial_values(metadata.dimensions, metadata.initial_datetime);
    for (auto& dim : metadata.dimensions) {
        if (dim.is_time_dimension()) {
            dim.time->set_initial_value(initial_values[time_dim_index]);
            time_dim_index++;
        }
    }
```
`to_toml()` never writes it, so it is derived again on every load.

`ExpressionAggregate`'s constructor (`src/expression/expression_aggregate.cpp`) copies the operand's metadata, `output_meta_ = operand_meta;` (~L37), and then rewires only `parent_dimension_index` (~L48-68). When the reduced dimension is the outermost time dimension, its time child becomes outermost. The child keeps its old inner `initial_value`, a state `from_toml_content` can never produce. `compute_row` passes the output coordinate to the operand unchanged (~L93, `operand_dims_buf_[i] = dims[operand_to_out_[i]];`). So output month coordinate *m* holds the operand's month-of-year *m*.

**Reproduction** (traced against HEAD 58dfe7a). Take `year(2) × month(12)`, yearly + monthly, `initial_datetime = 2025-03-01T00:00:00`. Its initial values are (1, 3), so the file holds 2025-03 .. 2026-12. Fill every visited cell with `100*year + month`, then run `Expression(a).aggregate("year", Sum)`:
- **In memory:** the output is `[month]` with `parent_dimension_index = -1`, `initial_value = 3` and `initial_datetime = 2025-03-01`.
- **`save()`** (`src/expression/expression.cpp`) opens the writer with that metadata.
  - `first_dimensions` (`src/binary/iteration.cpp` ~L90, `dim.time->initial_value`) returns `[3]`, and `next_dimensions` stops at 12.
  - So only coordinates 3..12 are computed and written, to file positions 2..11.
  - The January and February sums (201, 202) are never computed.
- **Reopening** re-derives month `initial_value = 1`, because month is now outermost.
  - Coordinate 1, which the file calls 2025-03, reads NaN.
  - Coordinate 3, which it calls 2025-05, holds March's sum (306).
- **Combining the result** with its own saved copy, `out + Expression(BinaryFile::open_file(out_path, 'r'))`, throws `Cannot apply: time dimension 'month' has incompatible TimeProperties`. The cause is `lp.initial_value != rp.initial_value` in `validate_shape_compatibility` (`src/expression/expression_helpers.h` ~L94), which compares 3 against 1.

The existing `ExpressionFixture.AggregateReduceOutermostTimeDimWithChildren` (`tests/test_expression.cpp`) starts on 2025-01-01. Every initial value is already 1 there, and the test never saves, so it cannot see this.

**Corrections to the finding.**
1. The finding says the *last* two months come back empty. It is the *first* two coordinates that read NaN on re-read, and the lost data is January and February. Both verifiers noted this.
2. The finding's fix, "recompute `initial_value` so the outermost dimension gets 1", is not enough on its own. With `initial_datetime` still 2025-03-01, output coordinate 1 would be labelled March 2025 but would hold January's sum. The file would be mislabelled instead of shifted. So `initial_datetime` must also move back to the start of the first reduced period, 2025-01-01. Then output month *m* is calendar month *m*, which is exactly what `compute_row` already forwards.

**Principles violated.**
- A stored value derived from other fields must be re-derived wherever those fields change. The root CLAUDE.md applies the stricter form, "derived, never stored", to `number_of_time_dimensions()`.
- The result is silent corruption of saved output in a feature exposed through Julia and Lua.

## Constraints and decisions

- **Maintainer decision (binding):**
  - Keep the stored field. It is read on the hot path.
  - Compute it in one `BinaryMetadata` function, called by `from_toml_content` and `ExpressionAggregate`.
  - Rebase the output `initial_datetime` when the outermost time dimension is reduced.
  - The builder path goes away with plan 12, and this plan depends on plan 08.
- **Hot path** (`src/CLAUDE.md` › Performance Bottlenecks):
  - `validate_dimension_values` costs about 19% of a profiled run.
  - `next_dimensions` runs once per cell.
  - Both read `initial_value` (through `add_offset_from_int` and the restore loop), so it stays a plain field. The new function runs once per metadata construction, never per cell.
- **Binary and expression exist only in C++/C API, Julia and Lua** (root CLAUDE.md, Design Decisions). No Dart, Python or JS change or test.
- **No C API or FFI change.** `convert_dimension_to_c` (`src/c/binary/binary_metadata.cpp`) and Lua's `dimension_to_lua` (`src/lua_runner.cpp`) keep reading `dim.time->initial_value`. So there is nothing to regenerate: no change to Julia `c_api.jl`, Python `_c_api.py`, JS `loader.ts` or Dart `bindings.dart`.
- **Why the new public member is not bound to the C API.** The root rule says "all public C++ methods should be bound". `derive_initial_values()` maintains an invariant of a mutable C++ value. No binding can mutate a `BinaryMetadata`: after plan 12 the C API has only factories and getters. This matches `validate_time_dimension_metadata()` / `validate_time_dimension_sizes()`, which are public, validation-only and unbound. Say so in `src/CLAUDE.md` (see Docs).
- **Clean over defensive** (root Principles): `parent_dimension_index` always points to an earlier time dimension. So the first time dimension in order is the outermost one, before and after the aggregate's rewiring. `compute_time_dimension_initial_values` already relies on this.
- **Delete, do not deprecate** (root Principles). After this change `set_initial_value` has no caller except its own test. It goes, with a **BREAKING** changelog line.
- **Changelog** (root Principles): 0.11.0 is unreleased and already a minor bump. No manifest bump.
- **Self-updating:** update `src/CLAUDE.md` (Binary Subsystem, Expression Subsystem). Also extend the root CLAUDE.md design-decision bullet about `number_of_time_dimensions()`, so the stored-field decision is not relitigated.

Alternatives considered and rejected:
- **Delete the field and derive it on read** (the finding's proposal). The maintainer rejected it: it adds calendar arithmetic to per-cell `validate_dimension_values` / `next_dimensions`.
- **"Minimal alternative": re-run the assignment loop at the end of the constructor.** `compute_time_dimension_initial_values` is in an anonymous namespace and cannot be called from `src/expression/`. It also leaves the mislabel (Correction 2).
- **Floor `initial_datetime` per frequency with a switch** (Jan 1 / 1st of month / week start / midnight). It duplicates calendar rules, and the week start is exactly what plan 08 re-defines ("anchor Weekly parents on initial_datetime"). Offsetting each remaining time dimension to coordinate 1 through the operand's own `add_offset_from_int` gives the start of the first reduced period in the operand's coordinate system, whichever week anchoring plan 08 settled on. It is also 5 lines.
- **Translate coordinates in `compute_row` instead of rebasing the metadata.** That puts arithmetic on every output row, and the output metadata would still need a correct start.
- **A private member plus `friend class ExpressionAggregate`.** Plan 11's anonymous `build_metadata` also has to call it. A friend list for every caller is worse than one public, documented member.
- **Also fixing `add_time_dimension` (stores 0) and `quiver_binary_metadata_set_initial_datetime` (does not re-derive).** Plan 12 deletes both, per the maintainer.

## Changes

Plan 08 runs first and edits the same code. Read each function in full before editing. Anchor every edit on the quoted code and function names; line numbers are approximate.

### 1. `include/quiver/binary/time_properties.h` — drop the setter, document the field

Current (in `struct QUIVER_API TimeProperties`, ~L20-26):
```cpp
struct QUIVER_API TimeProperties {
    TimeFrequency frequency;
    int64_t initial_value;
    int64_t parent_dimension_index;

    // Setters
    void set_initial_value(int64_t initial_value);

```
New:
```cpp
struct QUIVER_API TimeProperties {
    TimeFrequency frequency;
    // Coordinate of the metadata's initial_datetime in this dimension (1 for the outermost time
    // dimension). Stored because traversal and validation read it per cell; computed only by
    // BinaryMetadata::derive_initial_values().
    int64_t initial_value;
    int64_t parent_dimension_index;

```
Leave the `datetime_to_int` / `add_offset_from_int` declarations as they are, including any signature plan 08 gave them.

### 2. `src/binary/time_properties.cpp` — delete the setter definition

Delete this block (~L39-41), together with the blank line after it:
```cpp
void TimeProperties::set_initial_value(int64_t initial_value) {
    this->initial_value = initial_value;
}
```

### 3. `include/quiver/binary/binary_metadata.h` — declare the one computation

Current (~L22-28):
```cpp
    BinaryMetadata();
    ~BinaryMetadata();

    // Derived from dimensions: count of entries carrying time properties
    int64_t number_of_time_dimensions() const;

    // Create metadata from Element
```
New (insert the declaration after `number_of_time_dimensions`):
```cpp
    BinaryMetadata();
    ~BinaryMetadata();

    // Derived from dimensions: count of entries carrying time properties
    int64_t number_of_time_dimensions() const;

    // Sets every time dimension's TimeProperties::initial_value from initial_datetime and the parent
    // chain. The one place initial_value is computed. It is never serialized, so every producer of
    // metadata (from_toml_content, ExpressionAggregate) calls this after setting dimensions and
    // initial_datetime.
    void derive_initial_values();

    // Create metadata from Element
```

### 4. `src/binary/binary_metadata.cpp` — define it; `from_toml_content` calls it

4a. Add the definition directly after `BinaryMetadata::number_of_time_dimensions()` (which ends `    return count;\n}`), before `BinaryMetadata BinaryMetadata::from_element(const Element& element) {`:
```cpp
void BinaryMetadata::derive_initial_values() {
    const std::vector<int64_t> values = compute_time_dimension_initial_values(dimensions, initial_datetime);
    size_t next = 0;
    for (auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            dim.time->initial_value = values[next++];
        }
    }
}
```
At HEAD, `compute_time_dimension_initial_values` returns one value per time dimension in dimension order: 1 for the first, then one per inner dimension. **If plan 08 renamed it or reshaped it**, still move 08's calculation here unchanged. For example, 08 may have written an inline loop in `from_toml_content` over its parent-aware position function. In that case make 08's block the body of `derive_initial_values()`: replace `metadata.` with member access, and turn any `set_initial_value(x)` into `initial_value = x`. After this step, nothing outside `derive_initial_values()` may compute or assign an initial value.

4b. In `BinaryMetadata::from_toml_content`, replace the tail. Current (HEAD, ~L332-346):
```cpp
    time_dim_index = 0;

    // Compute and set initial values for time dimensions
    std::vector<int64_t> initial_values =
        compute_time_dimension_initial_values(metadata.dimensions, metadata.initial_datetime);
    for (auto& dim : metadata.dimensions) {
        if (dim.is_time_dimension()) {
            dim.time->set_initial_value(initial_values[time_dim_index]);
            time_dim_index++;
        }
    }

    metadata.validate();
    return metadata;
}
```
New:
```cpp
    metadata.validate();
    metadata.derive_initial_values();
    return metadata;
}
```
- `validate()` goes **before** the derivation. Plan 08's note makes the same move. The calculation throws a bare `std::logic_error` ("WEEKLY frequency not implemented ...") for layouts that `validate()` rejects with a proper message, and `validate()` reads no initial values. If plan 08 already moved `validate()` up, the result is the same three lines.
- The `time_dim_index = 0;` reset goes. `time_dim_index` is still declared and used by the dimension-building loop above it. Plan 11 relies on this: "If plan 09 replaced that assignment loop with a single function call, no counter is left".

4c. In the same function's dimension-building loop, mark the placeholder. Current (~L324):
```cpp
            TimeProperties time_props{freq, 0, previous_time_dim_index};
```
New:
```cpp
            TimeProperties time_props{freq, 0, previous_time_dim_index};  // initial_value: derive_initial_values()
```
(If clang-format moves the comment onto its own line, accept that.) Do **not** touch `add_time_dimension`'s identical `{freq_enum, 0, parent_index}` line, because plan 12 deletes that function.

### 5. `src/expression/expression_aggregate.cpp` — rebase the start, derive the values

In `ExpressionAggregate::ExpressionAggregate(...)`, keep everything up to and including the parent-rewiring `for (size_t out_i = 0; ...)` loop. Current tail of the constructor (~L68-75):
```cpp
    }

    output_meta_.validate();

    operand_dims_buf_.resize(operand_meta.dimensions.size());
    operand_row_buf_.resize(operand_meta.labels.size());
    percentile_scratch_.resize(operand_meta.labels.size());
}
```
New:
```cpp
    }

    // Removing the outermost time dimension promotes its time child to outermost, but compute_row
    // still forwards the child's coordinate to the operand unchanged: output month 3 is the operand's
    // month 3, i.e. March. So the output must start where the operand's first reduced period starts,
    // which is the operand's datetime at coordinate 1 of every remaining time dimension
    // (year x month from 2025-03-01 -> 2025-01-01; day x hour from 06:00 -> 00:00). This has to run
    // before derive_initial_values(): add_offset_from_int reads the operand's initial values, which
    // the copied dimensions still hold.
    if (reduced_dim.is_time_dimension() && reduced_dim.time->parent_dimension_index == -1) {
        for (const auto& dim : output_meta_.dimensions) {
            if (dim.is_time_dimension()) {
                output_meta_.initial_datetime = dim.time->add_offset_from_int(output_meta_.initial_datetime, 1);
            }
        }
    }

    output_meta_.validate();
    output_meta_.derive_initial_values();

    operand_dims_buf_.resize(operand_meta.dimensions.size());
    operand_row_buf_.resize(operand_meta.labels.size());
    percentile_scratch_.resize(operand_meta.labels.size());
}
```
Why this is correct:
- **It only moves the start back.** `add_offset_from_int(base, 1)` shifts `base` by `1 - initial_value` periods of that dimension. Applied outer to inner, in the same order `validate_dimension_values` and `dimension_sizes_at_values` accumulate offsets, it yields the operand's datetime at coordinate (1, 1, …) of the remaining time dimensions.
- **The reduced dimension is outside the loop.** It was erased from `output_meta_` above. That matters: before plan 08, a Yearly/Monthly offset, even of 0, drops the time of day.
- **`derive_initial_values()` then gives every output time dimension 1.** The rebased datetime is, by construction, at position 1 of each of them. It also gives the outermost dimension 1 in the non-rebase cases, and for a non-time or innermost reduction it recomputes exactly the values that were copied.
- **Which reductions reach the rebase.**
  - Reducing a *middle* time dimension never gets here, because `validate()` rejects it. The re-parented child's size is out of bounds for its new parent: `day(31)` under yearly needs 365-366, and `hour(24)` under monthly needs 672-744.
  - Reducing the outermost time dimension when no time dimension remains leaves the loop with nothing to do.
- `compute_row` is **not** touched; plan 10 rewrites its start rule. `reduced_dim` is a reference into `operand_meta` (`operand_->metadata()`), not into `output_meta_`, so it is still valid after the `erase`.

No other file changes. Stay out of these: `iteration.cpp`, `binary_file.cpp`, `csv_converter.cpp`, `expression_helpers.h` (its `initial_value` comparison is now correct as it stands), the C API, `lua_runner.cpp`, and the Julia sources.

## Tests

Write the four aggregate tests first, build, and confirm they fail as described. Then apply Changes 1-5. Add the `derive_initial_values` unit test afterwards: it cannot compile before Change 3.

Test layout rules, and why:
- Use two time levels only. A three-level mid-period start hits plan 10's restore bug when the operand is written.
- Never put a variable-size child under the reduced dimension (month over day, or year over day with a leap first year). That throws for a pre-existing reason (see Out of scope).
- Keep month-under-year starts at midnight. A non-midnight start there would depend on plan 08's time-of-day fix.

### C++ — `tests/test_expression.cpp`

Insert both tests immediately after the `TEST_F(ExpressionFixture, AggregateReduceOutermostTimeDimWithChildren)` block, before `TEST_F(ExpressionFixture, AggregateDimensionNotFoundThrows) {`. They use the fixture's `write_qvr` (walks `first_dimensions`/`next_dimensions`, so cells before the start stay NaN) and `read_all_cells` (reopens, walks, reads with `allow_nulls = true`).

```cpp
TEST_F(ExpressionFixture, AggregateOutermostTimeDimFromMidYearStart) {
    // year(2) x month(12) from 2025-03-01 holds 2025-03..2026-12. Reducing "year" makes month outermost;
    // output month m must be calendar month m in memory, on disk, and after a reopen.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-03-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"year", "month"})
                                               .set("dimension_sizes", {2, 12})
                                               .set("time_dimensions", {"year", "month"})
                                               .set("frequencies", {"yearly", "monthly"})
                                               .set("labels", {"v1"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t) {
        return static_cast<double>(100 * dims[0] + dims[1]);  // 2025-01/02 are never visited: NaN
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate("year", ExpressionAggregate::Operation::Sum);

    const auto jan_1 =
        std::chrono::system_clock::time_point{std::chrono::sys_days{std::chrono::year{2025} / std::chrono::January / 1}};
    EXPECT_EQ(out.metadata().initial_datetime, jan_1);
    EXPECT_EQ(out.metadata().dimensions[0].time->initial_value, 1);

    out.save(path_out);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 12u);
    EXPECT_DOUBLE_EQ(vo[0], 201.0);  // Jan: 2026 only
    EXPECT_DOUBLE_EQ(vo[1], 202.0);  // Feb: 2026 only
    for (int64_t m = 3; m <= 12; ++m) {
        EXPECT_DOUBLE_EQ(vo[m - 1], static_cast<double>(300 + 2 * m)) << "month " << m;  // (100+m) + (200+m)
    }

    auto reopened = BinaryFile::open_file(path_out, 'r');
    EXPECT_EQ(reopened.get_metadata().initial_datetime, jan_1);
    EXPECT_EQ(reopened.get_metadata().dimensions[0].time->initial_value, 1);
    EXPECT_NO_THROW(out + Expression(reopened));  // in-memory and saved metadata agree
}

TEST_F(ExpressionFixture, AggregateOutermostTimeDimFromMidDayStart) {
    // day(2) x hour(24) from 2025-01-01T06:00 holds Jan 1 06:00 .. Jan 2 23:00. Reducing "day" makes hour
    // outermost; output hour h must be hour-of-day h, so the output starts at midnight.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T06:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"day", "hour"})
                                               .set("dimension_sizes", {2, 24})
                                               .set("time_dimensions", {"day", "hour"})
                                               .set("frequencies", {"daily", "hourly"})
                                               .set("labels", {"v1"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t) {
        return static_cast<double>(100 * dims[0] + dims[1]);  // Jan 1 00:00..05:00 are never visited: NaN
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate("day", ExpressionAggregate::Operation::Sum);

    const auto midnight =
        std::chrono::system_clock::time_point{std::chrono::sys_days{std::chrono::year{2025} / std::chrono::January / 1}};
    EXPECT_EQ(out.metadata().initial_datetime, midnight);
    EXPECT_EQ(out.metadata().dimensions[0].time->initial_value, 1);

    out.save(path_out);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 24u);
    for (int64_t h = 1; h <= 6; ++h) {
        EXPECT_DOUBLE_EQ(vo[h - 1], static_cast<double>(200 + h)) << "hour " << h;  // Jan 2 only
    }
    for (int64_t h = 7; h <= 24; ++h) {
        EXPECT_DOUBLE_EQ(vo[h - 1], static_cast<double>(300 + 2 * h)) << "hour " << h;  // (100+h) + (200+h)
    }
}
```
Before the fix:
- **First test.** `initial_datetime` is 2025-03-01 and `initial_value` is 3. `vo[0]` and `vo[1]` are NaN. `out + Expression(reopened)` throws `incompatible TimeProperties`. `vo[2..11]` already pass: the positions were right, only the start and the labels were wrong.
- **Second test.** `initial_datetime` is 06:00 and `initial_value` is 7. `vo[0..5]` are NaN.
- **Both tests are independent of plan 08.** Daily/hourly is one of the pairs that already validates, and the Daily offset keeps the time of day. The first test starts at midnight.

### C++ — `tests/test_binary_metadata.cpp`

Insert immediately before `TEST(BinaryMetadataFromTomlContent, MixedTimeAndNonTime) {`. The file already has `using namespace std::chrono;` and `make_valid_toml()`, which is monthly(4) × daily(31) from 2025-01-01.
```cpp
TEST(BinaryMetadataDeriveInitialValues, RecomputesFromCurrentInitialDatetime) {
    auto md = BinaryMetadata::from_toml_content(make_valid_toml());
    ASSERT_EQ(md.dimensions[1].time->initial_value, 1);

    md.initial_datetime = sys_days{2025y / March / 15d};
    md.derive_initial_values();

    EXPECT_EQ(md.dimensions[0].time->initial_value, 1);   // the outermost time dimension always starts at 1
    EXPECT_EQ(md.dimensions[1].time->initial_value, 15);  // day of the month
}
```

### C++ — `tests/test_binary_time_properties.cpp` (existing test to delete)

Delete the whole `TimePropertiesSetters` section, from its banner through the end of the test (~L75-83):
```cpp
// ============================================================================
// TimePropertiesSetters
// ============================================================================

TEST(TimePropertiesSetters, SetInitialValue) {
    TimeProperties props{TimeFrequency::Daily, 1, -1};
    props.set_initial_value(15);
    EXPECT_EQ(props.initial_value, 15);
}
```
It tests the removed setter. Aggregate initialization `TimeProperties{freq, value, parent}` still compiles, because removing a member function keeps the struct an aggregate. So no other test in that file, or in `test_binary_metadata.cpp`, changes.

### C API — `tests/test_c_api_expression.cpp`

Insert immediately before `TEST_F(ExpressionCApiFixture, FromUnopenedBinaryFile) {`, after the `AggregateChainedWithBinary` block. It uses the fixture's `make_metadata_v`, `expr_from_file` and `read_one_cell` (`allow_nulls = 1`). `quiver_binary_file_open_file` copies the metadata, so `md` can be freed right away.
```cpp
TEST_F(ExpressionCApiFixture, AggregateOutermostTimeDimFromMidYearStart) {
    // year x month from 2025-03-01 holds 2025-03..2026-12. Reducing "year" makes month outermost;
    // output month m must be calendar month m, in memory and on disk.
    auto* md = make_metadata_v({"year", "month"},
                               {2, 12},
                               {"v1"},
                               "MW",
                               "2025-03-01T00:00:00",
                               {"year", "month"},
                               {"yearly", "monthly"});
    quiver_binary_file_t* f = nullptr;
    ASSERT_EQ(quiver_binary_file_open_file(path_a.c_str(), 'w', md, &f), QUIVER_OK);
    quiver_binary_metadata_free(md);
    const char* dim_names[] = {"year", "month"};
    for (int64_t year = 1; year <= 2; ++year) {
        for (int64_t month = (year == 1 ? 3 : 1); month <= 12; ++month) {
            int64_t dim_values[] = {year, month};
            const double data[] = {static_cast<double>(100 * year + month)};
            ASSERT_EQ(quiver_binary_file_write(f, dim_names, dim_values, 2, data, 1), QUIVER_OK);
        }
    }
    ASSERT_EQ(quiver_binary_file_close(f), QUIVER_OK);

    auto* a = expr_from_file(path_a);
    quiver_expression_t* agg = nullptr;
    ASSERT_EQ(quiver_expression_aggregate(a, "year", QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM, nullptr, &agg),
              QUIVER_OK);

    quiver_binary_metadata_t* out_md = nullptr;
    ASSERT_EQ(quiver_expression_get_metadata(agg, &out_md), QUIVER_OK);
    char* start = nullptr;
    ASSERT_EQ(quiver_binary_metadata_get_initial_datetime(out_md, &start), QUIVER_OK);
    EXPECT_STREQ(start, "2025-01-01T00:00:00");
    quiver_binary_metadata_free_string(start);
    quiver_dimension_t month_dim{};
    ASSERT_EQ(quiver_binary_metadata_get_dimension(out_md, 0, &month_dim), QUIVER_OK);
    EXPECT_EQ(month_dim.time_properties.initial_value, 1);
    quiver_binary_metadata_free_dimension(&month_dim);
    quiver_binary_metadata_free(out_md);

    ASSERT_EQ(quiver_expression_save(agg, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(agg);

    EXPECT_DOUBLE_EQ(read_one_cell(path_out, {"month"}, {1})[0], 201.0);   // Jan: 2026 only
    EXPECT_DOUBLE_EQ(read_one_cell(path_out, {"month"}, {2})[0], 202.0);   // Feb: 2026 only
    EXPECT_DOUBLE_EQ(read_one_cell(path_out, {"month"}, {3})[0], 306.0);   // Mar: 103 + 203
    EXPECT_DOUBLE_EQ(read_one_cell(path_out, {"month"}, {12})[0], 324.0);  // Dec: 112 + 212
}
```
Before the fix, the start is `"2025-03-01T00:00:00"`, `initial_value` is 3, and months 1 and 2 read NaN.

### Lua — `tests/test_lua_expression.cpp`

Insert immediately before `TEST_F(LuaExpressionTest, AggregateAgentsMean) {`. Plan 10 later inserts its own test before `AggregateUnknownOpThrows`, one block earlier, so the two do not collide.
```cpp
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
```
Before the fix, `lua.run` throws `Failed to run Lua script: ... output starts at the first reduced period, got 2025-03-01T00:00:00`. The `true` second argument to `r:read` allows NaN, so a regression fails on the value assertion rather than on `contains null values`.

### Julia — `bindings/julia/test/test_expression.jl`

Insert immediately before `    @testset "Aggregate dimension not found throws" begin`, after the `"Aggregate composed with binary"` testset. It uses the file's `make_metadata_full`, `with_expr`, `read_one_cell` (`allow_nulls = true`) and `cleanup`.
```julia
    @testset "Aggregate outermost time dim from mid-year start" begin
        # year x month from 2025-03-01 holds 2025-03..2026-12. Reducing "year" makes month outermost;
        # output month m must be calendar month m, in memory and after a reopen.
        path_a, path_out = make_path("a"), make_path("out")
        try
            md = make_metadata_full(
                dimensions = ["year", "month"],
                dimension_sizes = [2, 12],
                labels = ["v1"],
                initial_datetime = "2025-03-01T00:00:00",
                time_dimensions = ["year", "month"],
                frequencies = ["yearly", "monthly"],
            )
            file = Quiver.Binary.open_file(path_a; mode = 'w', metadata = md)
            for year in 1:2, month in 1:12
                year == 1 && month < 3 && continue  # before the file starts
                Quiver.Binary.write!(file; data = [100.0 * year + month], year = year, month = month)
            end
            Quiver.Binary.close!(file)

            with_expr(path_a) do e
                out = Quiver.aggregate(e, "year", Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM)
                md_out = Quiver.get_metadata(out)
                @test Quiver.Binary.get_initial_datetime(md_out) == "2025-01-01T00:00:00"
                @test Quiver.Binary.get_dimensions(md_out)[1].initial_value == 1
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end

            reopened = Quiver.Binary.open_file(path_out; mode = 'r')
            @test Quiver.Binary.get_initial_datetime(Quiver.Binary.get_metadata(reopened)) == "2025-01-01T00:00:00"
            Quiver.Binary.close!(reopened)
            @test read_one_cell(path_out; month = 1)[1] == 201.0   # Jan: 2026 only
            @test read_one_cell(path_out; month = 2)[1] == 202.0   # Feb: 2026 only
            @test read_one_cell(path_out; month = 3)[1] == 306.0   # Mar: 103 + 203
            @test read_one_cell(path_out; month = 12)[1] == 324.0  # Dec: 112 + 212
        finally
            cleanup(path_a, path_out)
        end
    end

```
Before the fix, the start test, the `initial_value` test and the month 1/2 tests fail (NaN is never `== 201.0`).

### Existing tests

No existing test changes except the deletion above. I checked every test that aggregates a time dimension or reads `initial_value`:
- **`ExpressionFixture.AggregateReduceOutermostTimeDimWithChildren`:** starts 2025-01-01, so every offset in the rebase is 0 and the start is unchanged.
- **`AggregateSumOverTimeDimSimple`:** reduces the only time dimension, so the loop has nothing to do.
- **`AggregateTimeDimRewireParents` and `AggregateSumOverTimeDimVariable`:** a non-time or innermost reduction, where the derived values equal the copied ones.
- **The `initial_value` assertions in `test_binary_metadata.cpp`, `test_lua_binary.cpp` and `bindings/julia/test/test_binary_metadata.jl`:** they go through `from_toml_content`, whose result is unchanged.
- **Every C API, Lua and Julia aggregate test:** each reduces the non-time `row`.

All 17 `ExpressionFixture.Aggregate*` tests pass at HEAD. No schema files are involved.

## Docs and changelog

`CLAUDE.md`, `src/CLAUDE.md` and `CHANGELOG.md` are **CRLF** in the working tree. Use the Edit tool, which keeps line endings.

### Root `CLAUDE.md` — Design Decisions

Old:
```
- **`BinaryMetadata::number_of_time_dimensions()` is derived** from `dimensions`, never stored.
```
New:
```
- **`BinaryMetadata::number_of_time_dimensions()` is derived** from `dimensions`, never stored.
  `TimeProperties::initial_value` is the deliberate opposite: it is stored, because the per-cell
  traversal and validation read it, and it is computed only by `BinaryMetadata::derive_initial_values()`,
  which `from_toml_content` and the `ExpressionAggregate` constructor call. Do not turn it into an
  on-read derivation.
```

### `src/CLAUDE.md` — `## Binary Subsystem`

1. In the `BinaryMetadata` bullet list, add a new line directly after `  - Serialization: \`to_toml()\``. Leave the `Factories` and `Builders` lines exactly as they are: plan 12 anchors on them.
   ```
     - Initial values: `derive_initial_values()` — the one computation of every time dimension's `initial_value` (from `initial_datetime` and the parent chain). `from_toml_content` calls it after `validate()`; the `ExpressionAggregate` constructor calls it on its output metadata. `to_toml()` never writes the values, so every load re-derives them. It is public for those callers, and deliberately not bound to the C API/Julia/Lua: no binding can mutate a `BinaryMetadata`.
   ```
2. Replace the bullet
   ```
   - `TimeProperties` struct: `frequency`, `initial_value`, `parent_dimension_index`
   ```
   with
   ```
   - `TimeProperties` struct: `frequency`, `initial_value`, `parent_dimension_index`. `initial_value` is the dimension's coordinate at `initial_datetime` (1 for the outermost time dimension). It is **stored**, not derived on read, because `first_dimensions`, `next_dimensions`, `dimension_sizes_at_values` and `validate_dimension_values` read it per cell (Performance Bottlenecks below). Any code that changes `dimensions` or `initial_datetime` must end with `BinaryMetadata::derive_initial_values()`.
   ```

### `src/CLAUDE.md` — `## Expression Subsystem`, the `ExpressionAggregate` bullet

Old:
```
  - `ExpressionAggregate`: collapses a named dimension. `Operation::{Sum,Mean,Min,Max,Percentile}` (nested enum). Constructor eagerly removes the dim from output metadata, rewires child time-dim `parent_dimension_index` transitively (a time dim whose parent was removed re-points to the removed dim's grandparent, or `-1`), and pre-allocates index translation + reusable buffers. Skips NaN inputs during accumulation; all-NaN range yields NaN.
```
New:
```
  - `ExpressionAggregate`: collapses a named dimension. `Operation::{Sum,Mean,Min,Max,Percentile}` (nested enum). Constructor eagerly removes the dim from output metadata, rewires child time-dim `parent_dimension_index` transitively (a time dim whose parent was removed re-points to the removed dim's grandparent, or `-1`), and pre-allocates index translation + reusable buffers. When the removed dim is the **outermost time dimension**, the constructor also moves `initial_datetime` back to the operand's datetime at coordinate 1 of every remaining time dimension (the start of the first reduced period: `year × month` from 2025-03-01 gives 2025-01-01), then calls `derive_initial_values()`. `compute_row` forwards the promoted child's coordinate to the operand unchanged, so output month *m* must still mean calendar month *m*. Without the rebase, the in-memory output kept month's start at 3 while the saved file re-read it as 1, shifting the data by two months. The rebase runs before `derive_initial_values()` because `add_offset_from_int` reads the operand's initial values. Skips NaN inputs during accumulation; all-NaN range yields NaN.
```
If plan 08 already edited these bullets, apply the same additions to whatever text is there.

No other docs change:
- `docs/*.md`, `bindings/julia/CLAUDE.md`, `tests/CLAUDE.md` and `src/c/CLAUDE.md` do not mention `initial_value` or aggregate start dates.
- `bindings/js/src/lua-api.ts` lists `initial_value` only as a `get_dimensions()` field. No Lua binding is added or removed, so `lua-api-sync.test.ts` is unaffected.

### `CHANGELOG.md` (under `## [0.11.0] — unreleased`)

1. `### Removed`. This is Keep a Changelog order (Changed, Removed, Fixed), and it is the section plan 12 appends to. If no earlier plan created it, add it immediately before the `### Fixed` heading of 0.11.0, after the last `### Changed` entry. If it already exists, append this bullet to it.
```markdown
### Removed

- **BREAKING (C++ only) — `TimeProperties::set_initial_value()`.** `BinaryMetadata::derive_initial_values()`
  is now the one place a time dimension's `initial_value` is computed, and nothing else called the
  setter. No C API function or binding exposed it.

  *Adapt:* after changing a `BinaryMetadata`'s `dimensions` or `initial_datetime`, call
  `derive_initial_values()` instead of setting each value by hand.
```
2. `### Fixed`. Add as the last bullet of that list, immediately before the line `## [0.10.9] — 2026-09-25`:
```markdown
- **Expressions: aggregating away the outermost time dimension no longer shifts the result.** For
  `year × month` data starting 2025-03-01, `aggregate("year", ...)` kept the month's start at 3 in
  memory while the saved file re-read it as 1. The file came back shifted by two months, the
  January and February sums were never computed, and the in-memory result could not be combined
  with its own saved output (`incompatible TimeProperties`). The output now starts where the first
  reduced period starts: its `initial_datetime` becomes 2025-01-01 and output month *m* is calendar
  month *m*, in memory and on disk. The same holds for every frequency: a `day × hour` file from
  06:00 aggregated over `day` starts at 00:00. Reducing `year` over a daily grid whose first year is
  a leap year still fails at 29 February, as it already did for a 1 January start. Affects C++, the
  C API, Julia and Lua.
```

## Verification

From the repo root, in order:
1. Add the four aggregate tests only (C++ ×2, C API, Lua, Julia). Build with `cmake --build build --config Debug`, then confirm they fail:
   - `./build/bin/quiver_tests.exe --gtest_filter='ExpressionFixture.AggregateOutermostTimeDimFromMid*:LuaExpressionTest.AggregateOutermostTimeDimFromMidYearStart'` shows 3 failed tests.
   - `./build/bin/quiver_c_tests.exe --gtest_filter='ExpressionCApiFixture.AggregateOutermostTimeDimFromMidYearStart'` shows 1 failed test.
2. Apply Changes 1-5. Delete `TimePropertiesSetters.SetInitialValue` and add `BinaryMetadataDeriveInitialValues.RecomputesFromCurrentInitialDatetime`. Then run `cmake --build build --config Debug`.
3. Run `./build/bin/quiver_tests.exe --gtest_filter='ExpressionFixture.*:BinaryMetadata*:TimeFrequencyConversion.*:TimeProperties*:IterationTest.*:BinaryTempFileFixture.*:CSVConverterFixture.*:LuaExpressionTest.*:LuaBinaryTest.*'`. All pass, including:
   - `ExpressionFixture.AggregateOutermostTimeDimFromMidYearStart`
   - `ExpressionFixture.AggregateOutermostTimeDimFromMidDayStart`
   - `BinaryMetadataDeriveInitialValues.RecomputesFromCurrentInitialDatetime`
   - `LuaExpressionTest.AggregateOutermostTimeDimFromMidYearStart`
4. Run `./build/bin/quiver_c_tests.exe --gtest_filter='ExpressionCApiFixture.*:BinaryCApiFixture.*:BinaryCApiCSVFixture.*:BinaryCApiMetadata.*'`. All pass.
5. Run `./build/bin/quiver_tests.exe --gtest_list_tests | grep TimePropertiesSetters`. It prints nothing.
6. `git grep -n "set_initial_value" -- . ':!CHANGELOG.md'` prints nothing.
7. `git grep -n "compute_time_dimension_initial_values" -- src` shows only the anonymous-namespace function itself (its signature and the name inside its own `Unhandled frequency` message) and the one call inside `BinaryMetadata::derive_initial_values`. If plan 08 renamed or inlined it, the same rule applies to 08's name: one call site, inside `derive_initial_values`.
8. `bindings/julia/test/test.bat test_expression.jl`, then `bindings/julia/test/test.bat`. All pass, including "Aggregate outermost time dim from mid-year start". No generator run is needed: the C API is unchanged.
9. `scripts/format.bat`, then `git status`. Only the files this plan touches should be modified; revert anything else the formatters rewrote.
10. `scripts/test-all.bat`. The six suites must pass. The final CLI smoke step fails at this HEAD because `example/example1.lua` no longer exists; plan 65 fixes that, so ignore it here.

## Acceptance criteria

- [ ] `TimeProperties::initial_value` is still a plain stored field. `TimeProperties::set_initial_value` is gone from the header, the `.cpp` and the tests.
- [ ] `BinaryMetadata::derive_initial_values()` is declared in `binary_metadata.h` and defined in `binary_metadata.cpp`. It is the only code that computes or assigns an initial value; `add_time_dimension`'s literal `0` stays for plan 12 to delete.
- [ ] `from_toml_content` ends with `metadata.validate(); metadata.derive_initial_values(); return metadata;`. The `time_dim_index = 0;` reset and the assignment loop are gone.
- [ ] The `ExpressionAggregate` constructor rebases `output_meta_.initial_datetime` when the reduced dimension is the outermost time dimension. It then calls `validate()` and `derive_initial_values()`, in that order. `compute_row` is unchanged.
- [ ] The two new `ExpressionFixture` tests, and the new C API, Lua and Julia tests, pass. Each failed before the fix. `BinaryMetadataDeriveInitialValues.RecomputesFromCurrentInitialDatetime` passes.
- [ ] Every existing binary, expression, iteration and CSV-converter test passes unchanged.
- [ ] The root `CLAUDE.md` decision bullet, the two `src/CLAUDE.md` Binary Subsystem bullets and the `ExpressionAggregate` bullet are updated. CRLF is preserved.
- [ ] `CHANGELOG.md` 0.11.0 has the `### Removed` **BREAKING** entry with an *Adapt:* line and the `### Fixed` entry.
- [ ] No C API, FFI declaration, binding source or `.bat` file is touched.

## Pitfalls

- **Rebase before derive.** `add_offset_from_int` computes `value - this->initial_value`. Before `derive_initial_values()` runs, the copied output dimensions still hold the **operand's** values (3 for March), which is what the rebase needs. Calling derive first turns every offset into 0 and silently restores the mislabel.
- **Iterate `output_meta_.dimensions`, not `operand_meta.dimensions`.** The operand list still contains the reduced dimension. Offsetting it by 0 is harmless after plan 08, but before plan 08 a Yearly/Monthly offset rebuilds the date from `sys_days` and drops the time of day, so a `year × … × hour` rebase would land 6 hours early.
- **`validate()` before `derive_initial_values()`, in both callers.** The calculation throws a bare `std::logic_error` for layouts `validate()` rejects with a proper message (a weekly inner dimension, a yearly child). `validate()` reads no initial values.
- **Do not change `compute_row`.** Its start rule is plan 10's. The rebase is what makes its one-to-one coordinate forwarding correct, so no translation is needed there.
- **Test layouts.** Keep the tests exactly as written:
  - A variable-size child under the reduced dimension (month over day, or year over day with a leap first year) makes `compute_row` read an operand cell that does not exist (`2025-02-29`), and `save()` throws. That is a pre-existing problem; see Out of scope.
  - A three-level mid-period start hits plan 10's bug while `write_qvr` builds the operand.
- **`EXPECT_EQ` on a `time_point`.** Compare against `std::chrono::system_clock::time_point{std::chrono::sys_days{...}}`, as `BinaryTempFileFixture.InitialDatetimeYear1960` does. gtest compiles and prints it on every CI toolchain.
- **Plan 08 reshaped this code first.** Read `from_toml_content`, the calculation function and `time_properties.h` before editing. The rule is the same whatever 08 named things: one assigning function (`derive_initial_values`), called after `validate()`, and no other assignment.
- **Plan 11 moves the `from_toml_content` tail** into an anonymous `build_metadata` function, and it calls `metadata.derive_initial_values()` there. That is why the member is public.
- **Plan 16 renames the aggregation enum later** (`ExpressionAggregate::Operation::Sum`, `QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM`, `Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM`). It owns updating these new tests along with the rest.
- **Formatting.** `scripts/format.bat` runs clang-format and JuliaFormatter, which may re-wrap the new code and the Julia testset. Accept that. Touch no `.bat` file.
- **Line endings.** `.cpp`, `.h` and `.jl` are LF (`.gitattributes`). The three `.md` files are CRLF in the working tree.

## Out of scope

- **Parent-aware position function, time of day in `add_offset_from_int`, weekly anchoring, and validating before computing initial values:** plan **08**.
- **The start rule in `next_dimensions` / `ExpressionAggregate::compute_row`**, which checks only the immediate parent: plan **10**.
- **Restructuring `from_toml_content`** (length checks, missing keys, no TOML round trip in `from_element`): plan **11**.
- **The builder path.** `add_time_dimension` stores `initial_value = 0`, and `quiver_binary_metadata_set_initial_datetime` never re-derives, so builder-made metadata still starts at coordinate 0 until plan **12** deletes both.
- **Broadcast metadata** (`build_broadcast_metadata` copies each source's `TimeProperties`): plan **15**. **The aggregation enum:** plan **16**.
- **Lua reference** (`bindings/js/src/lua-api.ts`): the surface is unchanged, so there is no edit.
- **Noticed, not owned by any listed plan; flag it to the maintainer.** Reducing an outermost time dimension whose child's size varies by period still throws in `save()`.
  - **Why:** the output grid takes the child's maximum size (31 days, 744 hours), but `compute_row` reads the operand at that child coordinate for every value of the reduced dimension, including periods where it does not exist. `validate_dimension_values` then rejects the cell.
  - **Example:** `month(4) × day(31)` from 2025-01-01, `aggregate("month")`. Output day 29 asks for operand `(2, 29)`, which is 2025-03-01, so the read throws `Invalid values for time dimensions`.
  - **Same with a leap first year:** `year(2) × month × day` from 2024-01-01, `aggregate("year")`, fails at operand `(2, 2, 29)`.
  - **Effect of this plan:** it makes a mid-period start behave exactly like a period-aligned one. For a `year × month × day` file whose first year is a leap year and which starts after February, the old code silently dropped January and February; it now reaches 29 February and throws, as a 1 January start already did. The CHANGELOG entry says so.
  - **Likely fix:** in `compute_row`, skip reduced values for which the child coordinate is past that period's `dimension_sizes_at_values` size. That is a behaviour decision (skip, or NaN), not part of this plan.
