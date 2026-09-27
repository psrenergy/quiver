# 03 — update_time_series_group: build INSERT from the union of row keys

**Batch** 1 · **Severity** m · **Breaking** no. Only direct C++ callers passing non-uniform rows see a difference, and what they see is the fix. · **Size** S · **Layers** C++ core (fix + test), C API (comment only), Lua (comment only), docs (src/AGENTS.md, src/c/AGENTS.md, public header comment, CHANGELOG)
**Depends on** none · **Overlaps with** 02, 04, 57, 53 (same file `src/database_time_series.cpp`); 46, 44 (`src/lua_runner.cpp`, next to the comment edited here); 23, 17 (`src/c/database_helpers.h` / `src/c/AGENTS.md`, next to the text edited here). Details under Pitfalls.

## Why

`Database::update_time_series_group` validates every row, but it builds the INSERT column list from `rows[0]` alone. A value column that first appears in a later row passes validation and is then thrown away. The call reports success and the value reads back as NULL.

Current code in `src/database_time_series.cpp`, `Database::update_time_series_group` (currently ~L172-204):

```cpp
    // Get column names from first row (excluding dimension column which we handle specially)
    std::vector<std::string> value_columns;
    for (const auto& [col_name, _] : rows[0]) {
        if (col_name != dim_col) {
            value_columns.push_back(col_name);
        }
    }

    // Build INSERT SQL
    auto insert_sql = "INSERT INTO " + ts_table + " (id, " + dim_col;
    for (const auto& col : value_columns) {
        insert_sql += ", " + col;
    }
    ...
        for (const auto& col : value_columns) {
            auto it = row.find(col);
            if (it != row.end()) {
                parameters.emplace_back(it->second);
            } else {
                parameters.emplace_back(nullptr);
            }
        }
```

Validation (currently ~L156-159) runs `validate_time_series_row` over **every** row. That function (currently ~L22-53) checks the dimension columns, schema membership and types per row. It never requires the rows to share keys.

**Reproduction** (`tests/schemas/valid/nullable_time_series.sql`, table `Sensor_time_series_readings(id, date_time TEXT NOT NULL, temperature REAL, counter INTEGER, status TEXT, PRIMARY KEY (id, date_time))`):

```cpp
db.update_time_series_group("Sensor", "readings", id, {
    {{"date_time", std::string("2024-01-01T00:00:00")}, {"temperature", 1.5}},
    {{"date_time", std::string("2024-01-02T00:00:00")}, {"temperature", 2.5}, {"counter", int64_t{7}}}});
```

Row 2's `counter` passes validation (int64 into INTEGER). The INSERT is `INSERT INTO Sensor_time_series_readings (id, date_time, temperature) VALUES (?, ?, ?)`, so the 7 is never bound. `read_time_series_group` returns `counter = NULL` for both rows, and nothing throws.

The sibling writer was already fixed for this exact bug. `Impl::update_group_rows` (`src/database_update.cpp`, currently ~L163-173) collects a `std::set<std::string>` of every row's keys, and CHANGELOG 0.10.0 records that fix for vector/set groups only ("A column present only in a later row is now written instead of dropped"). The test comment that pins the vector fix says the time-series writer already behaves this way (`tests/test_database_update.cpp`, currently ~L1300-1301): `// rows[0]. update_time_series_group validates (and keeps) every row's keys.` The "keeps" part is false.

The workaround spread outward instead of being fixed in C++. The C API decoder (`src/c/database_helpers.h`) and Lua's `columns_to_cpp_rows` (`src/lua_runner.cpp`) both justify their row padding with "the core builds its INSERT column list from rows[0]". That puts core logic in the outer layers and breaks the root principle "Logic resides in C++ layer". It also violates the "Clean code" principle, because a public method silently loses data.

**Reachability:** only direct callers of the public C++ `Database::update_time_series_group` / `update_time_series_group_by_label` can hit this. The C API decoder and the Lua transpose always hand the core uniform rows, so Julia, Dart, Python, JS and Lua cannot reach it today.

## Constraints and decisions

- **Maintainer decisions (binding):** "No C API test (the decoder always emits uniform rows). Remove the rows[0] caveat text from all five places." The five places are `src/c/database_helpers.h` (comment above `unmarshal_group_columns_to_rows`), `src/lua_runner.cpp` (comment above `columns_to_cpp_rows`), `src/c/AGENTS.md` (twice, in "Multi-Column Time Series") and `src/AGENTS.md` (the `time_series_rows_from_lua` bullet). Running `grep -rn 'from rows\[0\]\|from \`rows\[0\]\`' src/ include/` today matches exactly those five lines.
- Root AGENTS.md "Intelligence: Logic resides in C++ layer. Bindings/wrappers remain thin." The fix belongs in the core.
- Root AGENTS.md "Self-Updating". `src/AGENTS.md` is the AGENTS.md nearest to `src/database_time_series.cpp`, so it gets a sentence. `src/c/AGENTS.md` is nearest to `src/c/database_helpers.h`.
- Root AGENTS.md "Changelog". This is a user-visible fix for C++ callers, so it goes under `## [0.11.0] — unreleased` → `### Fixed`. It is not BREAKING, and no manifest bump is needed (0.11.0 is already the unreleased minor).
- Root design decision "Time-series group NULLs round-trip via a per-cell presence mask … Lua is mask- and sentinel-free: NULL is plain `nil` … value columns may be short/sparse/empty (missing cells write NULL)". This plan keeps all of that. The padding code in the C API decoder and in Lua stays; only its stated rationale changes.
- `tests/AGENTS.md`: `test_database_time_series_group.cpp` is where "group read/update + validation" tests live, so the new C++ test goes there, not in `test_database_update.cpp`.
- Error-message patterns are unaffected, because no message is added or changed.

**Keep the padding in the C API decoder and in Lua.** It stays load-bearing after the fix. A named column whose cells are *all* NULL (Lua `flag = {}`, or a fully masked C API column) would otherwise appear in no row map. The core would then (a) never validate its name, so `typo = {}` would pass silently where it throws today (`column 'typo' not found in group ...`), and (b) leave it to the column DEFAULT instead of writing the explicit NULL the caller asked for. Only the "because rows[0]" justification is wrong. This refines the finding's proposal to "delete the caveats": the text is reworded to the real reason, not dropped.

**Alternatives considered and rejected:**
- *Build the INSERT from every schema column (all table columns except `id`/`dim_col`).* Rejected: a column no row names would get an explicit NULL instead of its DEFAULT, and a `NOT NULL DEFAULT x` value column would start failing. It also diverges from `update_group_rows`.
- *Reject non-uniform rows with a Pattern 1 error.* Rejected: the vector/set writers accept them (union + NULL fill), and the two group writers must behave the same.
- *Add a C API test named `UpdateTimeSeriesGroupKeepsColumnPresentOnlyInALaterRow`* (the finding's proposal). Rejected by the maintainer and by both verifiers. `unmarshal_group_columns_to_rows` always emits every column in every row, so no C API input reaches the bug and the test could not fail before the fix.
- *Delete the NULL pre-fill in Lua `columns_to_cpp_rows`.* Rejected for the validation/DEFAULT reason above.

## Changes

### 1. `src/database_time_series.cpp`: build `value_columns` from the union of every row's keys

**1a. Include.** Current top of file:

```cpp
#include "database_impl.h"
#include "database_internal.h"
#include "utils/datetime.h"

namespace quiver {
```

New:

```cpp
#include "database_impl.h"
#include "database_internal.h"
#include "utils/datetime.h"

#include <set>

namespace quiver {
```

(`.clang-format` has `IncludeBlocks: Regroup`, so `scripts/format.bat` keeps it in its own block. `src/database_update.cpp` uses the same layout.)

**1b. `Database::update_time_series_group`.** Replace only the `value_columns` block (currently ~L172-178). Current:

```cpp
    // Get column names from first row (excluding dimension column which we handle specially)
    std::vector<std::string> value_columns;
    for (const auto& [col_name, _] : rows[0]) {
        if (col_name != dim_col) {
            value_columns.push_back(col_name);
        }
    }
```

New:

```cpp
    // INSERT column list: the union of every row's keys (minus the dimension column, bound
    // first), as update_group_rows does - not rows[0]'s, which dropped a value column named only
    // in a later row after validating it. A row that omits a column binds NULL for it below.
    std::set<std::string> value_columns;
    for (const auto& row : rows) {
        for (const auto& [col_name, _] : row) {
            if (col_name != dim_col) {
                value_columns.insert(col_name);
            }
        }
    }
```

Leave the rest unchanged, and the implementer must not touch it:
- The `// Build INSERT SQL` loop (`for (const auto& col : value_columns)`) and the placeholder loop (`for (size_t i = 0; i <= value_columns.size(); ++i)`) compile unchanged against a `std::set`.
- The per-row bind loop already falls back to NULL: `auto it = row.find(col); if (it != row.end()) { ... } else { parameters.emplace_back(nullptr); }`.
- Column order stays alphabetical: `rows[0]` was a `std::map`, so its iteration was already sorted, and `std::set` sorts the same way.
- Secondary dimension columns (e.g. `block` in `multi_dim_time_series.sql`) are still included. `validate_time_series_row` requires every `dim_cols` entry in every row, so they are always in the union.
- `id` can never reach the union. `time_series_schema_types` excludes it, so `validate_time_series_row` already throws `column 'id' not found in group ...`.

If plan 02 has changed how `dim_col` is defined (the line `const auto& dim_col = dim_cols.front();`, currently ~L153), keep plan 02's definition. This step only replaces the `value_columns` block.

### 2. `include/quiver/database.h`: state the contract on the public method

In `class Database`, current (currently ~L180):

```cpp
    // Update time series group - replaces all rows for element
    void update_time_series_group(const std::string& collection,
```

New:

```cpp
    // Update time series group - replaces all rows for element. Every row must carry every
    // dimension column; a value column named in any row is written for every row, as NULL where
    // a row omits it (a column no row names is left to its DEFAULT).
    void update_time_series_group(const std::string& collection,
```

### 3. `src/c/database_helpers.h`: reword the rationale above `unmarshal_group_columns_to_rows` (comment only, no code change)

Current (currently ~L212-216):

```cpp
// Decodes the columnar typed-arrays + per-cell mask form into the row-shaped data the C++ core
// takes - the inverse of marshal_group_rows_to_c, shared by every group update C function
// (time series, vector, set). A NULL mask (for the whole parameter or for one column) means dense,
// and a NULL cell becomes an explicit Value{nullptr} in every row so rows stay uniform (the core
// builds its INSERT column list from rows[0]).
```

New:

```cpp
// Decodes the columnar typed-arrays + per-cell mask form into the row-shaped data the C++ core
// takes - the inverse of marshal_group_rows_to_c, shared by every group update C function
// (time series, vector, set). A NULL mask (for the whole parameter or for one column) means dense,
// and a NULL cell becomes an explicit Value{nullptr}, so every row names every column: a column
// whose cells are all NULL is still validated by the core (an unknown name throws) and is written
// as NULL rather than left to the column DEFAULT.
```

### 4. `src/lua_runner.cpp`: reword the rationale above `columns_to_cpp_rows` (comment only, no code change)

Current (currently ~L2024-2026, directly above `static std::vector<std::map<std::string, Value>> columns_to_cpp_rows(`):

```cpp
    // Transpose the collected columns into uniform row maps: the C++ core derives the INSERT
    // column list from rows[0], so every row carries every named column, with explicit NULL for
    // the cells the caller left out (which is how nil holes from a read round-trip).
```

New:

```cpp
    // Transpose the collected columns into row maps where every row carries every named column,
    // with explicit NULL for the cells the caller left out (which is how nil holes from a read
    // round-trip). The NULL pre-fill is what makes an all-nil column such as `flag = {}` reach the
    // core at all: it is validated (an unknown name still throws) and written as NULL rather than
    // left to the column DEFAULT.
```

Leave the body (`for (auto& row : cpp_rows) { row[column.name] = nullptr; }` and the type dispatch) untouched.

### 5. The Julia, Dart, Python and JS bindings, FFI declarations, and Lua bindings

No change. No C API signature changes, so no generator run and no edits to `c_api.jl`, `bindings.dart`, `_c_api.py` or `loader.ts`. `grep -rn "rows\[0\]\|INSERT column" bindings --include=*.jl --include=*.dart --include=*.py --include=*.ts` finds no binding comment that repeats the caveat. `bindings/js/src/lua-api.ts` documents the column-oriented Lua surface, which is unchanged.

## Tests

### C++ core: new test in `tests/test_database_time_series_group.cpp`

Place it in the `// Time series update tests` section, directly after `TEST(Database, UpdateTimeSeriesGroupEmpty)` (which ends currently ~L134, before `TEST(Database, TimeSeriesOrdering)`). The file already includes `test_utils.h`, `<gtest/gtest.h>`, `<quiver/database.h>` and `<quiver/element.h>`. The schema is `tests/schemas/valid/nullable_time_series.sql` (existing, unchanged). No new schema file is needed.

```cpp
// A value column named only in a later row used to be dropped silently: the INSERT column list
// came from the first row's keys, after every row had been validated. The twin of
// UpdateGroupKeepsColumnPresentOnlyInALaterRow (test_database_update.cpp) for vector/set groups.
TEST(Database, UpdateTimeSeriesGroupKeepsColumnPresentOnlyInALaterRow) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("nullable_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    auto id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T00:00:00")}, {"temperature", 1.5}},
        {{"date_time", std::string("2024-01-02T00:00:00")}, {"temperature", 2.5}, {"counter", int64_t{7}}}};
    db.update_time_series_group("Sensor", "readings", id, rows);

    auto result = db.read_time_series_group("Sensor", "readings", id);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0].at("temperature")), 1.5);
    EXPECT_DOUBLE_EQ(std::get<double>(result[1].at("temperature")), 2.5);

    // The row that omitted "counter" gets SQL NULL; the later row's value is written, not dropped.
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0].at("counter")));
    ASSERT_TRUE(std::holds_alternative<int64_t>(result[1].at("counter")));
    EXPECT_EQ(std::get<int64_t>(result[1].at("counter")), 7);

    // "status" is named by no row, so it is not in the INSERT and stays NULL (its DEFAULT).
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0].at("status")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[1].at("status")));
}
```

The `quiver::Element().set("label", "Config")` spelling is already used with this schema in `tests/test_lua_runner_time_series.cpp` (`TimeSeriesGroupAllNullColumnRoundTrip`).

**Fails before the fix:** with the `rows[0]` column list the INSERT is `(id, date_time, temperature)`, so `result[1].at("counter")` holds `nullptr`. `ASSERT_TRUE(std::holds_alternative<int64_t>(...))` fails. The other assertions pass both before and after.

### C++ core: fix the false comment in `tests/test_database_update.cpp`

Above `TEST(Database, UpdateGroupKeepsColumnPresentOnlyInALaterRow)` (currently ~L1300-1301). Current:

```cpp
// A column named only in a later row used to be dropped silently: the column set came from
// rows[0]. update_time_series_group validates (and keeps) every row's keys.
```

New:

```cpp
// A column named only in a later row used to be dropped silently: the column set came from the
// first row's keys. update_time_series_group had the same bug; its twin is
// UpdateTimeSeriesGroupKeepsColumnPresentOnlyInALaterRow (test_database_time_series_group.cpp).
```

The test body does not change.

### Existing tests that pin behaviour touched here

None assert the dropping behaviour, and all of the following must still pass unchanged:
- `Database.UpdateTimeSeriesGroupMissingBlockInLaterRow` (validation still throws before the DELETE).
- `Database.UpdateTimeSeriesGroupMultiDimHappyPath` (the `block` secondary dimension is still in the INSERT).
- `Database.TimeSeriesNullValue`, `Database.UpdateTimeSeriesGroup*`, `Database.TimeSeries*`.
- The C API `DatabaseCApi.*TimeSeries*` tests.
- The Lua `LuaRunnerTest.*TimeSeries*` tests, including `UpdateTimeSeriesGroupMultiDimNulls` and `TimeSeriesGroupAllNullColumnRoundTrip`.

### C API, Lua, Julia, Dart, Python and JS tests

No new tests, and this is deliberate (maintainer decision). The C API decoder and the Lua transpose give the core one key set per row, so the behaviour is not visible in any layer except the C++ API. A test there could not fail before the fix. The binding suites are run only as regression checks (see Verification).

## Docs and changelog

### `src/AGENTS.md`

**(a)** In the `Impl::update_group_rows` bullet (currently ~L385-389). Current:

```
- **`Impl::update_group_rows`** (`database_update.cpp`) is the shared body of
  `update_vector_group`/`update_set_group`. It validates the **union of every row's keys** (not
  `rows[0]`, which dropped later-row-only columns and skipped validating them) against the group
  table, rejects the derived `id`/`vector_index`, and calls `require_element` — all before
  `transpose_group_rows`, so a named-but-empty column list cannot fall through to the DELETE.
```

Append one sentence at the end of that bullet:

```
  `update_time_series_group` (`database_time_series.cpp`) builds its INSERT column list from the
  same union (minus the primary dimension column, bound first); it used to take only the first
  row's keys and silently drop a value column that a later row named.
```

**(b)** In the `time_series_rows_from_lua` bullet (currently ~L625-626). Current:

```
  must be present and dense; value columns may be shorter, sparse, or empty — missing indices become
  `Value{nullptr}` (rows stay uniform: the core builds its INSERT list from `rows[0]`).
```

New:

```
  must be present and dense; value columns may be shorter, sparse, or empty — missing indices become
  `Value{nullptr}`, so every row carries every named column and an all-nil column such as
  `flag = {}` is still validated and written as NULL, not left to the column DEFAULT.
```

### `src/c/AGENTS.md` ("Multi-Column Time Series" section)

**(a)** In the `quiver_database_update_time_series_group()` bullet (currently ~L184-187). Current:

```
  column can be tagged `FLOAT` with zeroed data regardless of the schema column's type. The row map
  receives an explicit `Value{nullptr}` for masked cells in **every** row, keeping rows uniform (the
  core builds the INSERT column list from `rows[0]`). Masking a dimension/PK cell surfaces as the
  SQLite NOT NULL/constraint error. Pass `column_count == 0` and `row_count == 0` with NULL arrays
```

New (replace those four lines with these five; the following line `  to clear all rows.` stays):

```
  column can be tagged `FLOAT` with zeroed data regardless of the schema column's type. The row map
  receives an explicit `Value{nullptr}` for masked cells, so every row names every column and an
  all-NULL column is still validated and written as NULL, not left to the column DEFAULT. Masking
  a dimension/PK cell surfaces as the SQLite NOT NULL/constraint error. Pass `column_count == 0`
  and `row_count == 0` with NULL arrays
```

**(b)** The third contract bullet under "One decoder for every group and row write" (currently ~L217-218). Current:

```
- Masked cells become an explicit `Value{nullptr}` in **every** row, keeping rows uniform (the core
  builds its INSERT column list from `rows[0]`).
```

New:

```
- Masked cells become an explicit `Value{nullptr}`, so every row names every column: an all-NULL
  column is still validated by the core (an unknown name throws) and written as NULL rather than
  left to the column DEFAULT.
```

### Root `AGENTS.md`, `tests/AGENTS.md`, `docs/*.md`, `bindings/js/src/lua-api.ts`, READMEs

No change. None of them mention the `rows[0]` behaviour. `docs/time_series.md` and `lua-api.ts` describe the column-oriented binding surfaces, which always send every column.

### `CHANGELOG.md`

Under `## [0.11.0] — unreleased` → `### Fixed`, append after the last existing bullet (the Julia `Artifacts.toml` entry, just before `## [0.10.9] — 2026-09-25`):

```
- **`update_time_series_group()` writes a value column that only a later row names.** The C++
  method (and `update_time_series_group_by_label()`) built its INSERT column list from the first
  row's keys, so a column that appeared only from the second row on passed validation and was
  then silently dropped, reading back as NULL. It now uses the union of every row's keys, as
  `update_vector_group()` / `update_set_group()` already do; a row that omits such a column writes
  NULL for it. The C API, Lua and the bindings always pass every column in every row and were not
  affected.
```

## Verification

Run from the repo root `C:\Development\Quiver\quiver3`:

1. `cmake --build build --config Debug`. It must build with no new warnings.
2. The new test and its neighbours:
   `./build/bin/quiver_tests.exe --gtest_filter='Database.UpdateTimeSeriesGroup*:Database.TimeSeries*:Database.ReadTimeSeriesGroup*:Database.UpdateGroup*'`
   Expect all to pass, including the new `Database.UpdateTimeSeriesGroupKeepsColumnPresentOnlyInALaterRow`. To confirm the test catches the bug, temporarily revert step 1b, rebuild, and check that the new test fails on `holds_alternative<int64_t>(result[1].at("counter"))`. Then restore step 1b.
3. The Lua paths that own the reworded comment:
   `./build/bin/quiver_tests.exe --gtest_filter='LuaRunnerTest.*TimeSeries*:LuaRunnerTest.*Group*'`. Expect all to pass.
4. The C API paths that own the reworded comment:
   `./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.*TimeSeries*:DatabaseCApi.*Group*'`. Expect all to pass.
5. Full native suites: `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`. Expect all to pass.
6. The caveat must be gone:
   `grep -rn 'from rows\[0\]\|from \`rows\[0\]\`' src/ include/`. Expect no output (today it prints 5 lines).
7. Binding regression runs (no binding code changed, but they load the rebuilt native library): `bindings/julia/test/test.bat`, `bindings/dart/test/test.bat`, `bindings/js/test/test.bat`, `bindings/python/tests/test.bat`. Expect all to pass.
8. `scripts/format.bat`, then `git diff` to confirm it only touched the files listed below (it may regroup the new `#include <set>`).
9. `scripts/test-all.bat`. Expect the six suites to PASS. The `quiver_cli` smoke step currently fails because `example/` no longer exists. That failure predates this plan and plan 65 fixes it, so do not try to fix it here.
10. `git status`. Only these files should be modified: `src/database_time_series.cpp`, `include/quiver/database.h`, `src/c/database_helpers.h`, `src/lua_runner.cpp`, `tests/test_database_time_series_group.cpp`, `tests/test_database_update.cpp`, `src/AGENTS.md`, `src/c/AGENTS.md`, `CHANGELOG.md`.

## Acceptance criteria

- [ ] `Database::update_time_series_group` builds `value_columns` as a `std::set<std::string>` over every row's keys minus `dim_col`, and `#include <set>` is added.
- [ ] The `row.find(col)` → `nullptr` fallback in the bind loop is unchanged.
- [ ] `Database.UpdateTimeSeriesGroupKeepsColumnPresentOnlyInALaterRow` exists in `tests/test_database_time_series_group.cpp`, passes, and fails with step 1b reverted.
- [ ] The comment above `UpdateGroupKeepsColumnPresentOnlyInALaterRow` in `tests/test_database_update.cpp` no longer claims time series "keeps" every row's keys.
- [ ] All five "INSERT column list from rows[0]" rationales are reworded (`src/c/database_helpers.h`, `src/lua_runner.cpp`, `src/c/AGENTS.md` ×2, `src/AGENTS.md` time_series_rows_from_lua bullet), and the grep in Verification step 6 prints nothing.
- [ ] The padding/pre-fill code in `unmarshal_group_columns_to_rows` and `columns_to_cpp_rows` is unchanged.
- [ ] `src/AGENTS.md`'s `Impl::update_group_rows` bullet notes that `update_time_series_group` uses the same union.
- [ ] The public header comment on `update_time_series_group` states the column contract.
- [ ] The CHANGELOG `0.11.0 → Fixed` entry is present and not marked BREAKING.
- [ ] No C API, Lua or binding test was added. No generator was run and no FFI declaration changed.
- [ ] All C++ / C API / binding suites pass.

## Pitfalls

- **Plan ordering and shifted lines.** Plan 02 (runs first) rewrites `find_dimension_column` in `src/database_internal.h` and may touch the read functions and possibly the `dim_col` line in this same file. Plan 04 (runs first) edits `read_time_series_row` in this file. Anchor on the quoted `// Get column names from first row` block, not on line numbers. Plans 53 (moves `execute` into `Impl`) and 57 (replaces the `find_time_series_table` lookup at the top of this function) run later and will rebase over this change.
- **Neighbouring edits.** In `src/lua_runner.cpp`, plan 46 later rewrites the type-dispatch body of `columns_to_cpp_rows` directly under the comment edited here, and plan 44 edits `collect_group_columns` just above it. In `src/c/AGENTS.md`, plan 17 edits the `quiver_database_read_time_series_row()` bullet a few lines below edit (a). In `src/c/database_helpers.h`, plan 23 edits `marshal_group_rows_to_c` below the decoder. Change only the quoted comment text so those plans apply cleanly.
- **Do not delete the NULL pre-fill** in Lua (`for (auto& row : cpp_rows) { row[column.name] = nullptr; }`) or the explicit `row[col_name] = nullptr` in the C API decoder. Both look redundant once the core takes the union, but they are what gets an all-nil / all-masked column validated and written as NULL (see Constraints).
- **Behaviour nuance, not a regression.** A column named only in later rows now gets an explicit NULL in the earlier rows, not its DEFAULT. For a `NOT NULL DEFAULT x` non-key value column, that means the earlier rows now fail with a SQLite NOT NULL constraint error, where before the later value was silently lost. That is the same semantics `update_vector_group` / `update_set_group` already have. No test schema has a DEFAULT on a time-series value column (the only `DEFAULT`s under `tests/schemas` are on collection tables in issue52/issue70).
- **Line endings.** `src/*.cpp` / `*.h` are LF (`.gitattributes`). `src/AGENTS.md`, `src/c/AGENTS.md` and `CHANGELOG.md` are CRLF in the working tree (LF in the index, `text=auto`). The Edit tool preserves either, but do not run `sed` over them. No `.bat` file is touched.
- **`std::get` on a NULL cell throws `std::bad_variant_access`.** That is why the new test does `ASSERT_TRUE(holds_alternative<int64_t>)` before `std::get<int64_t>`, so the pre-fix run fails as a readable assertion rather than as an uncaught exception.
- **`2u` vs `2`.** Both compile, since many tests in this file use a plain `3`. The new test uses `2u` to match its vector twin in `tests/test_database_update.cpp`.

## Out of scope

- The array fan-out in `update_element` / `create_element` across groups sharing a column name (root AGENTS.md "Not yet fixed"). This plan does not touch it, and plan 05 covers validate-before-write for those paths.
- The dimension-column definition (`find_dimension_column` vs `find_dimension_columns`) belongs to plan 02.
- `read_time_series_row` multi-dimension/NULL handling belongs to plan 04.
- The Pattern 2 message and the lookup helper for a missing time-series group (`find_time_series_table`) belong to plan 57.
- The comment in `src/database_update.cpp` `Impl::update_group_rows` ("update_time_series_group validates every row too") is accurate and stays.
- The Lua read side `read_time_series_group_lua` iterating `rows[0]` (`src/lua_runner.cpp`, currently ~L1967) is correct, because the core read always returns every table column in every row. Leave it.
- `upsert_time_series_row` takes a single row, so the bug cannot occur there. No change.
