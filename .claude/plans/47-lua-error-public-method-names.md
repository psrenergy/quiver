# 47 — Lua: converter errors name the public method the script called

**Batch** 5 · **Severity** medium · **Breaking** no (error text only; no signature or behaviour change) · **Size** S · **Layers** C++ Lua runner, C++ and C API Lua tests, CHANGELOG
**Depends on** 46 (introduces `lua_to_value(obj, caller, what)`; this plan changes the `caller` passed to it) · **Overlaps with** 48 (strict decoders use their own caller names), 44 (key check in `collect_group_columns`, which already gets the public caller)

## Why

Root AGENTS.md, "C++ Error Message Patterns": *"Validators thread the calling operation's name
through so the `{operation}` is the public method the user called (e.g., type mismatches report
`"Cannot update_element: ..."`)."* Three Lua converters in `src/lua_runner.cpp` report the name of
an **internal helper** instead:

| Helper (currently) | Public callers | Message today |
|---|---|---|
| `table_to_element(const sol::table&)` (~L1429) | `create_element_lua`, `update_element_lua`, `update_element_by_label_lua` (~L1473-1491), and `quiver.metadata_from_element` (~L948: `BinaryMetadata::from_element(table_to_element(t))`) | `Cannot table_to_element: attribute 'enabled' has unsupported Lua type`; arrays: `Cannot table_to_element: array 'tags': cell #N ...` (via `array_caller = "table_to_element: array '" + k + "'"`, ~L1442) |
| `lua_table_to_value_map(const sol::table&)` (~L1400) | `upsert_time_series_row` / `_by_label` (~L2225, ~L2233) | `Cannot lua_table_to_value_map: column 'x' has unsupported Lua type` |
| `lua_table_to_values(const sol::table&)` (~L1713) | `query_string_lua`, `query_integer_lua`, `query_float_lua` (~L1740, ~L1751, ~L1762) | `Cannot lua_table_to_values: parameter #1 has unsupported Lua type` |

A script author reading `Cannot table_to_element: ...` cannot find a `db:table_to_element`. The
siblings `collect_group_columns(caller, ...)` and `columns_to_cpp_rows(caller, ...)` already take
the public name.

Pinned today (these tests must change):
- `tests/test_lua_runner_create.cpp` ~L148: `"Cannot table_to_element: attribute 'enabled'"`
- `tests/test_lua_runner_create.cpp` ~L162: `"Cannot table_to_element: array 'tags'"`
- `tests/test_c_api_lua_runner.cpp` ~L497: `"Cannot table_to_element: attribute 'enabled'"`
- `tests/test_lua_runner_errors.cpp` ~L314: `"Cannot lua_table_to_values: parameter #1"`

## Constraints and decisions

- **Maintainer notes (binding):** not BREAKING (message text only). Add a CHANGELOG entry under
  Fixed/Changed. Depends on plan 46.
- Give each helper a `const std::string& caller` parameter, in the same position as
  `collect_group_columns(caller, ...)` (leading).
- Public names to pass: `"create_element"`, `"update_element"`, `"update_element_by_label"`,
  `"metadata_from_element"`, `"upsert_time_series_row"`, `"upsert_time_series_row_by_label"`,
  `"query_string"`, `"query_integer"`, `"query_float"`.
- The array slot becomes `caller + ": array '" + k + "'"`, so an array message reads
  `Cannot create_element: array 'tags': cell #2 has unsupported Lua type`. Check the exact form
  `lua_cell_as` produces with the `what` it receives: `grep -n "static T lua_cell_as" -A20 src/lua_runner.cpp`.
- Do not bundle any other change.

## Changes — `src/lua_runner.cpp`

1. `lua_table_to_value_map(const sol::table& t)` becomes
   `lua_table_to_value_map(const std::string& caller, const sol::table& t)`. Its body (after plan
   46):
   ```cpp
   result[key] = lua_to_value(pair.second, caller, "column '" + key + "'");
   ```
2. `table_to_element(const sol::table& values)` becomes
   `table_to_element(const std::string& caller, const sol::table& values)`. Inside:
   - `const std::string array_caller = "table_to_element: array '" + k + "'";` becomes
     `const std::string array_caller = caller + ": array '" + k + "'";`
   - the scalar branch (after 46): `lua_to_value(val, caller, "attribute '" + k + "'")`
   - any other throw inside the function that hardcodes `"Cannot table_to_element: ..."` (e.g. the
     empty-array / unsupported element type branch, `"Cannot table_to_element: array '" + k + "' has unsupported element type"`)
     becomes `"Cannot " + caller + ": array '" + k + "' has unsupported element type"`.
     Find them all with `grep -n "table_to_element" src/lua_runner.cpp`.
3. `lua_table_to_values(const sol::table& parameters)` becomes
   `lua_table_to_values(const std::string& caller, const sol::table& parameters)`. The body (after
   46):
   ```cpp
   values.push_back(lua_to_value(parameters[i], caller, "parameter #" + std::to_string(i)));
   ```
4. Call sites:
   - `create_element_lua`: `table_to_element("create_element", values)`
   - `update_element_lua`: `table_to_element("update_element", values)`
   - `update_element_by_label_lua`: `table_to_element("update_element_by_label", values)`
   - `quiver.metadata_from_element` lambda (~L948): `table_to_element("metadata_from_element", t)`
   - `query_string_lua` / `query_integer_lua` / `query_float_lua`:
     `lua_table_to_values("query_string", *parameters)` and the equivalents for the other two
   - the upsert wrappers (~L2225, ~L2233): `lua_table_to_value_map("upsert_time_series_row", row)` /
     `lua_table_to_value_map("upsert_time_series_row_by_label", row)`

   Check afterwards: `grep -n "\"table_to_element\|\"lua_table_to_value_map\|\"lua_table_to_values" src/lua_runner.cpp`
   prints nothing.

## Tests

Update the four pinned assertions:
- `tests/test_lua_runner_create.cpp` ~L148: `"Cannot table_to_element: attribute 'enabled'"`
  becomes `"Cannot create_element: attribute 'enabled'"`. The script is
  `db:create_element("Configuration", { label = "Item", enabled = print })`.
- `tests/test_lua_runner_create.cpp` ~L162: `"Cannot table_to_element: array 'tags'"` becomes
  `"Cannot create_element: array 'tags'"`.
- `tests/test_c_api_lua_runner.cpp` ~L497: `"Cannot create_element: attribute 'enabled'"`.
- `tests/test_lua_runner_errors.cpp` ~L314: `"Cannot lua_table_to_values: parameter #1"` becomes
  `"Cannot query_integer: parameter #1"`. The script is `db:query_integer(..., { print })`.

Add two tests for the other callers, in the same style (try/catch + `find`, or `expect_lua_error`
if the file uses it):
- `tests/test_lua_runner_update.cpp`: `db:update_element("Configuration", 1, { integer_attribute = print })`
  gives `"Cannot update_element: attribute 'integer_attribute' has unsupported Lua type"`. It
  needs an element with id 1 first; copy the setup from a neighbouring test.
- `tests/test_lua_runner_time_series.cpp`:
  `db:upsert_time_series_row("Collection", "data", 1, { date_time = "2024-01-01T00:00:00", value = print })`
  gives `"Cannot upsert_time_series_row: column 'value' has unsupported Lua type"`.

## Docs and changelog

- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Fixed`:
  ```markdown
  - **Lua: conversion errors name the method the script called.** An unsupported value passed to
    `db:create_element`, `db:update_element`(`_by_label`), `db:upsert_time_series_row`(`_by_label`),
    `db:query_*` or `quiver.metadata_from_element` now reports e.g. `Cannot create_element: attribute
    'x' has unsupported Lua type`, instead of an internal helper name (`table_to_element`,
    `lua_table_to_value_map`, `lua_table_to_values`).
  ```
- `bindings/js/src/lua-api.ts`: if it quotes any of the old helper-named messages
  (`grep -n "table_to_element\|lua_table_to_value" bindings/js/src/lua-api.ts`), update the quote.
- No AGENTS.md change needed. The rule already says to thread the public name.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
3. `./build/bin/quiver_c_tests.exe --gtest_filter=*LuaRunner*`
4. `bindings/js/test/test.bat test/lua-api-sync.test.ts`
5. `scripts/format.bat`

## Acceptance criteria

- [x] No internal helper name appears in any thrown message.
- [x] Four pinned tests are updated and two new tests added. Lua and C API suites are green.
- [x] CHANGELOG entry added.

## Pitfalls

- `quiver.metadata_from_element` is the fourth `table_to_element` caller. It is easy to miss
  because it sits in the binary section (~L948).
- Plan 46 must already be in. Otherwise these helpers have no `lua_to_value` call to pass `caller`
  to, and you would have to edit each of the old chains' throws instead.

## Out of scope

- Other decoders' messages (plan 48).
- Any C++ core message.

## Implementation notes

Landed on `rs/plan47` on top of plans 43–46 (merged via master, `2692911`).

**Tests first.** With the test changes in and the fix not yet made, 8 tests failed, each still
naming a helper: 7 C++ (`CreateElementUnsupportedAttributeTypeThrows`,
`CreateElementUnsupportedArrayElementTypeThrows`, `CreateElementArrayCellTypeMismatchThrows`,
`QueryParameterUnsupportedTypeThrows`, `UpsertTimeSeriesRowUnsupportedValueTypeThrows`,
`UpdateElementRefusesArrayWithNilHole`, `UpdateElementUnsupportedAttributeTypeThrows`) and 1 C API
(`RunScriptUnsupportedAttributeTypeFails`). For example:
`Failed to run Lua script: Cannot lua_table_to_value_map: column 'value' has unsupported Lua type`.
After the fix: `LuaRunner*` 369/369, C API `*LuaRunner*` 27/27, full `quiver_tests` 1384/1384, full
`quiver_c_tests` 571/571, JS `lua-api-sync` 6/6, full JS suite 242/242.

**Drift fixed:**
- `require_dense_array` (a separate helper called from `table_to_element`) also hardcoded
  `Cannot table_to_element: array '...' has a nil hole ...`. The plan's "any other throw inside the
  function" didn't cover it, and plan 18's notes hand this message to 47. It now takes a leading
  `caller`, like the other three. `UpdateElementRefusesArrayWithNilHole`'s first assertion is
  tightened to `Cannot update_element: array 'value_int' has a nil hole` to pin it.
- `CreateElementArrayCellTypeMismatchThrows` also asserts `Cannot create_element: array '`, which
  pins the `array_caller` → `lua_cell_as` form (`Cannot create_element: array 'tag': cell #2 has
  unsupported Lua type`).
- The plan's final grep (`"table_to_element\|...`) can't match a `"Cannot table_to_element` literal.
  The check used instead was
  `grep -nE "Cannot (table_to_element|lua_table_to_value)|\"(table_to_element|lua_table_to_value)" src/lua_runner.cpp`,
  which is empty. Comments still name the helpers (they are function names).
- The plan's new update test named `Configuration.integer_attribute`, which `collections.sql`
  doesn't have (it would pass anyway, since conversion runs before the column lookup). The test uses
  `Collection.some_integer` instead.
- The C API pinned assertion is at `tests/test_c_api_lua_runner.cpp:500` (the plan says ~497).
- CHANGELOG: the plan's `[0.12.0]` was stale. The entry is under `## [0.12.7] — unreleased` →
  `### Fixed`, which plans 43–45 had created, with one added sentence for the nil-hole message.
- `bindings/js/test/test.bat test/lua-api-sync.test.ts` expands to `bun test test test/...`, which
  runs the whole JS suite. `bun test test/lua-api-sync.test.ts` from `bindings/js` runs only the
  sync test. Both were run.
- `bindings/js/src/lua-api.ts` quotes no helper-named message (re-checked after 43/44), so it is
  unchanged.

**For later plans:**
- Plan 72: take substrings from the public names: `Cannot create_element:`,
  `Cannot update_element:`, `Cannot update_element_by_label:`, `Cannot query_{string,integer,float}:`,
  `Cannot upsert_time_series_row(_by_label):`, `Cannot metadata_from_element:`. The nil-hole
  rejection now names the public method too.
- Not fixed (out of scope): the nil-hole hint says "write NULL cells with update_vector_group or
  update_set_group", which is the wrong advice when the caller is `quiver.metadata_from_element`.
  Also, the C++ core's own `BinaryMetadata::from_element` errors still say `Cannot from_element:`
  after the Lua conversion step says `metadata_from_element`.
- No Lua test calls `quiver.metadata_from_element`, or the `_by_label` / `query_string` /
  `query_float` conversion errors. They share the helper strings the tested callers pin.
