---
id: 03-04-lua-add-time-series-row
phase: 03
plan: 04
type: execute
wave: 1
depends_on: []
files_modified:
  - src/lua_runner.cpp
  - tests/test_lua_runner.cpp
requirements: [LUA-11]
autonomous: true

must_haves:
  truths:
    - "Lua scripts can call db:add_time_series_row(collection, group, id, row_table) through the LuaRunner-bound function"
    - "Calling db:add_time_series_row twice with the same dimension PK upserts (value columns overwritten)"
    - "Multi-dimension PK schemas (date_time + block) round-trip through the Lua binding"
    - "./build/bin/quiver_tests.exe exits 0 with the new LuaRunnerTest.AddTimeSeriesRow* cases present"
  artifacts:
    - path: "src/lua_runner.cpp"
      provides: "Lua binding for add_time_series_row via add_time_series_row_lua static + bind.set_function"
      contains: "add_time_series_row_lua"
    - path: "tests/test_lua_runner.cpp"
      provides: "LuaRunnerTest.AddTimeSeriesRow* GTest coverage"
      contains: "LuaRunnerTest, AddTimeSeriesRow"
  key_links:
    - from: "src/lua_runner.cpp"
      to: "Database::add_time_series_row"
      via: "Direct C++ call after lua_table_to_value_map conversion"
      pattern: "db\\.add_time_series_row"
    - from: "src/lua_runner.cpp bind.set_function"
      to: "add_time_series_row_lua"
      via: "sol2 binding alongside update_time_series_group at line ~155"
      pattern: "bind\\.set_function\\(\"add_time_series_row\""
---

<objective>
Ship the Lua binding `db:add_time_series_row(collection, group, id, row_table)` exposed through `LuaRunner`. Hand-add an `add_time_series_row_lua` static function in `src/lua_runner.cpp` that reuses the existing `lua_table_to_value_map` helper (already handles `nil` / `int64` / `double` / `string` conversion) and forwards directly to `db.add_time_series_row(collection, group, id, row)`. Bind via `bind.set_function("add_time_series_row", &add_time_series_row_lua)` alongside `update_time_series_group`. Rebuild via `cmake --build build --config Debug` and run `./build/bin/quiver_tests.exe` with the new GTest cases.

Purpose: Closes LUA-11. Per D-04 / D-06 / D-07 / D-08 / D-10 in `.planning/phases/03-language-bindings-documentation/03-CONTEXT.md`.
Output: New static function in `src/lua_runner.cpp` + binding registration + new GTest cases in `tests/test_lua_runner.cpp`. No generator (Lua has none).
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/03-language-bindings-documentation/03-CONTEXT.md
@CLAUDE.md
@include/quiver/database.h
@src/lua_runner.cpp
@tests/test_lua_runner.cpp

<interfaces>
<!-- C++ API the Lua binding forwards to, per CONTEXT.md canonical_refs -->

From include/quiver/database.h (lines 120-129):
```cpp
void add_time_series_row(const std::string& collection,
                         const std::string& group,
                         int64_t id,
                         const std::map<std::string, Value>& row);
```

Sibling binding template (the exact structure to mirror), from src/lua_runner.cpp:795-806:
```cpp
static void update_time_series_group_lua(Database& db,
                                         const std::string& collection,
                                         const std::string& group,
                                         int64_t id,
                                         sol::table rows) {
    std::vector<std::map<std::string, Value>> cpp_rows;
    for (size_t i = 1; i <= rows.size(); ++i) {
        sol::table row = rows[i];
        cpp_rows.push_back(lua_table_to_value_map(row));
    }
    db.update_time_series_group(collection, group, id, cpp_rows);
}
```

For `add_time_series_row_lua`, the parameter is a single `sol::table row` (not a list of tables). Forward via `db.add_time_series_row(collection, group, id, lua_table_to_value_map(row))` — one-line body. Reuse `lua_table_to_value_map` directly per Claude's Discretion in CONTEXT.md (no need for a `_single_row` variant — it is a one-line forward).

Sibling binding registration at src/lua_runner.cpp:155: `bind.set_function("update_time_series_group", &update_time_series_group_lua);` — the new binding goes immediately after on a new line.
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Hand-add add_time_series_row_lua + bind.set_function</name>
  <files>src/lua_runner.cpp</files>
  <read_first>
    src/lua_runner.cpp (lines 210-231 — the `lua_table_to_value_map` helper)
    src/lua_runner.cpp (lines 795-806 — the sibling `update_time_series_group_lua` static function)
    src/lua_runner.cpp (lines 145-170 — the `bind.set_function(...)` registration block)
    include/quiver/database.h (lines 120-129 — `Database::add_time_series_row` C++ signature)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-04)
  </read_first>
  <behavior>
    - A static function `add_time_series_row_lua(Database& db, const std::string& collection, const std::string& group, int64_t id, sol::table row)` exists in `src/lua_runner.cpp`, placed immediately after `update_time_series_group_lua` (around line 806)
    - The function body is one effective line: `db.add_time_series_row(collection, group, id, lua_table_to_value_map(row));`
    - A new registration line `bind.set_function("add_time_series_row", &add_time_series_row_lua);` exists in the binding block at line ~155, placed immediately after `bind.set_function("update_time_series_group", &update_time_series_group_lua);`
    - Build succeeds via `cmake --build build --config Debug`
    - No binding-layer "Cannot add_time_series_row:" string introduced — that comes from C++ `Database::add_time_series_row` (Phase 1) and surfaces unchanged through sol2's exception-to-Lua-error conversion (per D-10)
  </behavior>
  <action>
    Make two edits to `src/lua_runner.cpp`:
    
    1. Immediately after `update_time_series_group_lua` (which ends at line ~806), add:
       ```
       static void add_time_series_row_lua(Database& db,
                                           const std::string& collection,
                                           const std::string& group,
                                           int64_t id,
                                           sol::table row) {
           db.add_time_series_row(collection, group, id, lua_table_to_value_map(row));
       }
       ```
       Match the existing 4-space indentation and `static void <name>_lua(...)` signature convention used by every other Lua-binding helper in the file.
    
    2. At line ~155, immediately after the line `bind.set_function("update_time_series_group", &update_time_series_group_lua);`, add a new line:
       ```
       bind.set_function("add_time_series_row", &add_time_series_row_lua);
       ```
    
    No new helpers; reuse `lua_table_to_value_map` directly — it already handles `nil`/`int64_t`/`double`/`std::string`. Then rebuild via `cmake --build build --config Debug` to compile the changes into `quiver_tests.exe`.
  </action>
  <verify>
    <automated>findstr /C:"add_time_series_row_lua" src\lua_runner.cpp &amp;&amp; findstr /C:"\"add_time_series_row\"" src\lua_runner.cpp &amp;&amp; cmake --build build --config Debug</automated>
  </verify>
  <done>`src/lua_runner.cpp` contains the new `add_time_series_row_lua` static function and the new `bind.set_function("add_time_series_row", ...)` registration. `cmake --build build --config Debug` succeeds with no errors.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: Add LuaRunnerTest.AddTimeSeriesRow* GTest cases</name>
  <files>tests/test_lua_runner.cpp</files>
  <read_first>
    tests/test_lua_runner.cpp (lines 1189-1212 — the sibling `LuaRunnerTest.UpdateTimeSeriesGroup` test as direct precedent)
    tests/test_lua_runner.cpp (lines 1-50 — fixture/header for `LuaRunnerTest`, `collections_schema` access)
    tests/schemas/valid/collections.sql (primary single-dim schema)
    tests/schemas/valid/multi_dim_time_series.sql (composite-PK schema for multi-dim test — find the file's schema-loading approach in test_lua_runner.cpp)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-07 Lua scenario list)
  </read_first>
  <behavior>
    - Three new GTest cases exist in `tests/test_lua_runner.cpp`, placed immediately after `LuaRunnerTest.UpdateTimeSeriesGroup` (around line 1213):
      - `TEST_F(LuaRunnerTest, AddTimeSeriesRowInsert)` — runs Lua script calling `db:add_time_series_row("Collection", "data", id, { date_time = "2024-06-01", value = 10.0 })`, then reads back via `db.read_time_series_group(...)` and asserts the row is present
      - `TEST_F(LuaRunnerTest, AddTimeSeriesRowUpsert)` — runs the same Lua script twice with the same `date_time` and different `value` (10.0 then 99.0); asserts `read_time_series_group` returns one row with `value == 99.0`
      - `TEST_F(LuaRunnerTest, AddTimeSeriesRowMultiDim)` — uses `tests/schemas/valid/multi_dim_time_series.sql`; runs Lua script with both `date_time` and `block` populated plus a value column; asserts round-trip via `read_time_series_group`
    - All three new tests pass under `./build/bin/quiver_tests.exe`
  </behavior>
  <action>
    In `tests/test_lua_runner.cpp`, append three new `TEST_F(LuaRunnerTest, AddTimeSeriesRow*)` cases immediately after `LuaRunnerTest.UpdateTimeSeriesGroup` (line ~1212), before the "Time series files tests" banner (line ~1214). Match the existing test-setup pattern: `auto db = quiver::Database::from_schema(":memory:", collections_schema);` + create `Configuration` + create one `Collection` element + `quiver::LuaRunner lua(db);` + build a Lua `script` string + `lua.run(script);` + assert via direct `db.read_time_series_group(...)` calls (as `LuaRunnerTest.UpdateTimeSeriesGroup` does at lines 1206-1211). For `AddTimeSeriesRowMultiDim`, load `tests/schemas/valid/multi_dim_time_series.sql` via the same `from_schema(":memory:", <schema string>)` pattern — find the schema-string handle by reading how other multi-dim tests in this file (if any) load it; otherwise embed the schema content. Per D-07 Lua, no error-path test is required.
  </action>
  <verify>
    <automated>findstr /C:"LuaRunnerTest, AddTimeSeriesRowInsert" tests\test_lua_runner.cpp &amp;&amp; findstr /C:"LuaRunnerTest, AddTimeSeriesRowUpsert" tests\test_lua_runner.cpp &amp;&amp; findstr /C:"LuaRunnerTest, AddTimeSeriesRowMultiDim" tests\test_lua_runner.cpp</automated>
  </verify>
  <done>`tests/test_lua_runner.cpp` contains the three new `LuaRunnerTest.AddTimeSeriesRow*` GTest cases (Insert, Upsert, MultiDim). Existing tests unmodified.</done>
</task>

<task type="auto">
  <name>Task 3: Rebuild and run quiver_tests.exe with the new Lua tests</name>
  <files>build/bin/quiver_tests.exe</files>
  <read_first>
    CLAUDE.md (section "Build & Test" — confirms `cmake --build build --config Debug` is the build command and `./build/bin/quiver_tests.exe` is the test runner)
    tests/test_lua_runner.cpp (verify Task 2 committed the new tests)
  </read_first>
  <action>
    Run `cmake --build build --config Debug` to compile the changes from Task 2 (Task 1 already triggered a build, but Task 2 added new test source). Then run `./build/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.AddTimeSeriesRow*` to confirm the three new cases pass in isolation, then `./build/bin/quiver_tests.exe` (full suite) to confirm no regressions.
  </action>
  <verify>
    <automated>cmake --build build --config Debug &amp;&amp; .\build\bin\quiver_tests.exe --gtest_filter=LuaRunnerTest.AddTimeSeriesRow* &amp;&amp; .\build\bin\quiver_tests.exe</automated>
  </verify>
  <done>`cmake --build build --config Debug` succeeds. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.AddTimeSeriesRow*` exits 0 with three passing tests. The full `./build/bin/quiver_tests.exe` exits 0 with no regressions.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Lua table → C++ map | Caller-supplied Lua values cross into C++ through `lua_table_to_value_map`; the helper already handles the supported types (nil/int64/double/string) and silently skips unknown sol2 types. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-10 | Tampering | `add_time_series_row_lua` value coercion | mitigate | The binding reuses the locked `lua_table_to_value_map` helper; no new conversion code introduced. |
| T-03-11 | Information Disclosure | Error messages | accept | C++ exceptions from `Database::add_time_series_row` are converted to Lua errors by sol2's default exception handler. Error strings come from C++ only (per CLAUDE.md). |
| T-03-12 | Denial of Service | Malformed Lua table | mitigate | Unknown sol2 value types are silently skipped by `lua_table_to_value_map`; missing dimensions surface the canonical C++ "Cannot add_time_series_row:" error. |
</threat_model>

<verification>
- `src/lua_runner.cpp` contains both the new static function and the new `bind.set_function` registration (Task 1 acceptance)
- `cmake --build build --config Debug` succeeds end-to-end
- `./build/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.AddTimeSeriesRow*` passes with three cases
- Full `./build/bin/quiver_tests.exe` shows no regressions
- No new "Cannot add_time_series_row:" string introduced inside `src/lua_runner.cpp` (grep should match zero lines — that string lives in `src/database_time_series.cpp` from Phase 1)
</verification>

<success_criteria>
- All three tasks' acceptance criteria pass
- `db:add_time_series_row(...)` round-trips through Lua ↔ sol2 ↔ C++ ↔ SQLite for insert, upsert, and multi-dim paths
- LUA-11 is satisfied: signature matches roadmap success criterion #4 verbatim, exposed through LuaRunner with the existing test harness covering insert + upsert
</success_criteria>

<output>
Create `.planning/phases/03-language-bindings-documentation/03-04-SUMMARY.md` when done.
</output>
