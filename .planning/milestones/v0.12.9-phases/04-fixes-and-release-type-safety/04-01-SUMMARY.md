---
phase: 04-fixes-and-release-type-safety
plan: 01
subsystem: lua-binding
status: complete
tags: [sol2, lua_runner, release-safety, type-checks, pattern-1-errors]

requires:
  - phase: 03-dedupe
    provides: "single shared decoders (collect_group_columns, table_to_element, option_entries, lua_table_to_dim_map) and the build/dedupe-check/ golden harness"
provides:
  - "internal.h helpers: lua_type_name, lua_type_error, require_table, lua_string_key, optional_from_lua<T>"
  - "every table parameter of a bound function is a const sol::object& checked by require_table in the decoder that first walks it"
  - "four map-key sites through lua_string_key; eight optional-argument sites through optional_from_lua"
  - "CHANGELOG ## [0.13.0] — unreleased section (Changed: two BREAKING entries; Fixed: non-string keys) and its compare link"
  - "gitignored Phase 4 harness build/fixes-check/ (BASE, surface-base.txt, golden.sh, gate.sh, wave_gate.sh, linux.sh, baseline/, red/)"
affects: [04-02, 04-03, 04-04]

actuals:
  tokens: 13955
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Lua argument type check = get_type() test in the first decoder that walks the argument, never is<sol::table>() or a typed sol::table / sol::optional parameter"
    - "Optional argument = const sol::object& + optional_from_lua<T> (nil or missing is absent, anything else checked)"
    - "Two Lua-derived decodes never share one call's argument list: hoist into locals in argument order"

key-files:
  created: []
  modified:
    - src/lua_runner/internal.h
    - src/lua_runner/db_write.cpp
    - src/lua_runner/db_time_series.cpp
    - src/lua_runner/db_core.cpp
    - src/lua_runner/binary.cpp
    - src/lua_runner/csv.cpp
    - tests/test_lua_runner_update.cpp
    - tests/test_lua_runner_time_series.cpp
    - tests/test_lua_runner_create.cpp
    - tests/test_lua_runner_query.cpp
    - tests/test_lua_runner_write_csv.cpp
    - tests/test_lua_binary.cpp
    - tests/test_lua_expression.cpp
    - CHANGELOG.md
    - src/AGENTS.md

key-decisions:
  - "Type-error shape lives once in lua_type_error: Cannot <op>: <what> must be <expected>, got <lua type> (Lua's own type() name, usertype = userdata)"
  - "file:write decodes data then dims; bin_to_csv path then aggregate; file:read dims then allow_nulls; aggregate* op then parameter (all previously compiler-dependent)"
  - "open_file keeps mode -> containment -> metadata; the metadata type check sits after resolve_sandboxed_path"
  - "The golden key write.txt vector_not_array gains ', got number' in the group-writer commit (the plan missed that a probe passes a number column); promoted after review"

patterns-established:
  - "Red run saved to build/fixes-check/red/<key>.txt before the fix; gate (GOLDEN_CHANGE / DEBUG_TEXT_CHANGE only when the commit changes output) before each commit; mutation after the commit, reverted with git checkout"

requirements-completed: [SAFE-01, SAFE-02, SAFE-03]

duration: 38min
completed: 2026-10-03
---

# Phase 4 Plan 01: Release Type Safety for Table, Key and Optional Arguments Summary

**Every wrong-typed Lua table argument, map key and optional argument now raises `Cannot <op>: <what> must be <expected>, got <lua type>` in Debug and Release alike, through five helpers in `internal.h`. In Release, a userdata passed to a group writer no longer silently clears the group.**

## Performance

- **Duration:** about 38 min
- **Started:** 2026-10-03T12:34Z
- **Completed:** 2026-10-03T13:10Z
- **Tasks:** 3 (4 fix commits)
- **Files modified:** 15

## Base

- Base SHA: `b39fe78c8adf3e4ebf861649b7768bb9840dd7b2` (`build/fixes-check/BASE`). `git diff --quiet b8f3246 BASE -- src tests bindings CHANGELOG.md CMakeLists.txt` exits 0.
- Base gates: `GATE PASS lua=444 pairs=36` and `WAVE GATE PASS lua=444 tidy=14`.
- The baseline matches Phase 3 byte for byte: all 10 probe outputs and `debug_text.txt`. The literal `diff -r` / `cmp` is not silent, because three probe outputs (`core.txt` describe/path, `csv.txt` absolute/escape, `handles_run1.txt` bin_open_again) embed the harness directory name. After `sed -b 's/dedupe-check/fixes-check/'` over the Phase 3 files, all 11 compare equal.

## Commits

| # | SHA | Subject | Gate line |
|---|-----|---------|-----------|
| 1 | `70d2470` | fix(04-01): reject a non-table payload in the group writers | `GATE PASS lua=446 pairs=36` |
| 2 | `6014bb2` | fix(04-01): check every other table argument and report its Lua type | `GATE PASS lua=454 pairs=36` |
| 3 | `61f0297` | fix(04-01): reject non-string keys in element, row, dims and files tables | `GATE PASS lua=457 pairs=36` |
| 4 | `30a169f` | fix(04-01): reject wrong-typed optional arguments | `GATE PASS lua=460 pairs=36` |

Wave gate after commit 4: **`WAVE GATE PASS lua=460 tidy=14`**. Release `Lua*` = 460 tests in 12 suites, all pass. C API `LuaRunnerCApiTest` = 27 in both builds. Release golden output equals the Debug baseline. GCC 14 syntax pass is clean with the defines read from `src/CMakeLists.txt`. Each commit's red run, gate and commit ran in that order.

## Red Runs

| File | Build | Failing assertions |
|------|-------|--------------------|
| `red/c1-groups.txt` | **Debug only** (a non-table reaching `lua_next` is undefined in Release before the fix) | Number and string payloads got sol2's raw `stack index 5, expected table, received number/string: value is not a table or a userdata that can behave like one (bad argument into 'void(quiver::Database&, ..., sol::basic_table_core<...>)')`. Every userdata payload (`db`) **did not throw**: `expected script to throw: db:update_vector_group("Collection", "values", 1, db)`, and likewise for the other three. `{ value_int = db }` got `columns [value_int] contain no rows`. Both groups were then empty: `read_vector_integers_by_id` returned `{}` instead of `{1, 2, 3}`, and the time series had 0 rows instead of 1. 2/2 FAILED. |
| `red/c1-rest.txt` | **Debug only** (same reason, plus `lua_len` on a non-table in `file:write` / `select_agents`) | sol2 raw `expected table` text for `create_element(…, 5)`, `update_element(…, "x")`, `update_element_by_label(…, true)`, `upsert_time_series_row(…, 5)`, `update_time_series_files(…, 5)`, `r:read(5)`, `f:write(5, …)`, `f:write({…}, 'x')`, `quiver.metadata_from_element(5)`, `select_agents(5)`. `create_element("Collection", db)` walked the userdata as empty (`element must have at least one scalar attribute`). `upsert_time_series_row_by_label(…, db)` gave `row missing required 'date_time' column`. The hand-written checks lacked the suffix (`options must be a table`, `field 'labels' must be a table`, `mapping must be a table`, `row must be a table`, `option 'header' must be a table`). 8/8 FAILED. |
| `red/c2-keys.txt` | **Debug only** (a boolean key builds `std::string` from `nullptr` in Release before the fix) | sol2 raw `stack index -1, expected string, received number` for `{ "x" }`, `{ "2024-01-01T00:00:00" }`, `{ "a.bin" }`, `read({ 1 })`. `received boolean` for `{ label = "y", [true] = 1 }`. 3/3 FAILED. |
| `red/c5-optionals.txt` | Debug | `query_*('SELECT 1', 5)` did not throw (params silently ignored). `query_*('SELECT 1', db)` hit a sol2 panic `stack index 1, expected string, received sol.quiver::Database*`. `open_file('bin_a','w',{})` and `…db)` got `Metadata must be provided when opening a file in write mode.` `bin_to_csv('x', 1)` got `File not found: …\x`. `read({row=1}, 'yes')` did not throw. `aggregate('row','percentile','0.5')` and `aggregate_agents('percentile', true)` got `operation 'percentile' requires a parameter`. 3/3 FAILED. |
| `red/c5-optionals-release.txt` | Release | Same as Debug, except `query_*('SELECT 1', db)` gave `attempt to get length of a sol.quiver::Database* value`. That is a Lua error longjmp'd through C++ frames, technically undefined, and observed harmless here. 3/3 FAILED. |

Every committed test is defined in both builds, because it only ever runs with its fix in place. Release ran each new test green at the wave gate.

## Mutations (after commit, then reverted)

- **Commit 1:** `require_table` also accepted `sol::type::userdata`. Both `GroupWritersRejectNonTableColumns` and `TimeSeriesGroupWritersRejectNonTableColumns` FAILED (0/2 passed). Reverted with `git checkout -- src/lua_runner/internal.h`, and `git diff --exit-code -- src/lua_runner` exited 0.
- **Commit 2 (extra):** removed the userdata-attribute check in `table_to_element`. `CreateElementRejectsUserdataAttribute` FAILED (the value fell through to `has unsupported Lua type`). Reverted the same way.
- **Tracer gate (delegated self-verification):** the tracer `<verify>` was re-run end to end after commit 1. The Debug gate passed, and the Release `quiver_tests` filter passed 2/2. Together with the mutation, the verdict is that the group writers reject a non-table in Release and leave the group intact. Per the standing preference, this was recorded as delegated rather than stopping for a human.

## Golden and Debug-Text Changes

**Commit 1** (promoted with `GOLDEN_CHANGE=1`, review diff checked key by key):

| Key | Old | New |
|-----|-----|-----|
| `write.txt` `vector_not_array` | `Cannot update_vector_group: column 'value_int' must be an array of values` | `… must be an array of values, got number` |

**Commit 2** (`GOLDEN_CHANGE=1 DEBUG_TEXT_CHANGE=1`; 33 changed lines, each the old text plus `, got <type>`, nothing else):

| File / key(s) | Old text | Suffix added |
|---|---|---|
| `options.txt` `export_csv_02_number` / `_03_string` / `_04_userdata` / `_09_false` | `Cannot export_csv: options must be a table` | `, got number` / `string` / `userdata` / `boolean` |
| `options.txt` `import_csv_02..09` (same four) | `Cannot import_csv: options must be a table` | same four |
| `options.txt` `read_csv_02..09` (same four) | `Cannot read_csv: options must be a table` | same four |
| `options.txt` `read_csv_stream_02..09` (same four) | `Cannot read_csv_stream: options must be a table` | same four |
| `options.txt` `write_csv_02..09` (same four) | `Cannot write_csv: options must be a table` | same four |
| `options.txt` `enum_level1` | `Cannot export_csv: option 'enum_labels' must be a table` | `, got number` |
| `options.txt` `enum_level2` | `Cannot export_csv: option 'enum_labels['some_integer']' must be a table` | `, got number` |
| `options.txt` `enum_level3` | `Cannot export_csv: option 'enum_labels['some_integer']['en']' must be a table` | `, got number` |
| `options.txt` `header_number` | `Cannot write_csv: option 'header' must be a table` | `, got number` |
| `options.txt` `metadata_none` / `metadata_number` | `Cannot metadata: options must be a table` | `, got nil` / `, got number` |
| `options.txt` `metadata_labels_number` | `Cannot metadata: field 'labels' must be a table` | `, got number` |
| `csv.txt` `row_none` / `row_number` / `row_string` / `row_userdata` | `Cannot write_row: row must be a table` | `, got nil` / `number` / `string` / `userdata` |
| `binary.txt` `mapping_number` | `Cannot rename_agents: mapping must be a table` | `, got number` |
| `debug_text` `control_writer_write_row` | `Cannot write_row: row must be a table` | `, got number` |

**Commit 3:** none (gate run without `GOLDEN_CHANGE`).

**Commit 4** (`DEBUG_TEXT_CHANGE=1` only; golden outputs unchanged). This is the Debug-only sol2 signature in the bad-argument text, where the third parameter type changed:
- `badarg_query_string`: `… bad argument into 'std::optional<std::basic_string<…> >(quiver::Database&, const std::basic_string<…>&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)'` became `… const sol::basic_object<sol::basic_reference<0> >&)'`
- `badarg_query_integer`: `'std::optional<__int64>(…, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)'` became `'std::optional<__int64>(…, const sol::basic_object<sol::basic_reference<0> >&)'`
- `badarg_query_float`: `'std::optional<double>(…, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)'` became `'std::optional<double>(…, const sol::basic_object<sol::basic_reference<0> >&)'`

Snapshots of each earlier baseline are kept under `build/fixes-check/baseline-base`, `baseline-c1-groups` and `baseline-c2-keys`.

## Check-Order Ledger

| Function | Old order | New order |
|---|---|---|
| `file:write` | `data` and `dims` decoded as two arguments of one `self.write(...)` call, so the order was compiler-dependent (MSVC/GCC usually right to left) | `data` (require_table + cells), then `dims`, then `self.write`. This is the one deliberate reorder; `write(5, 'x')` reports `data` (pinned) |
| `db:bin_to_csv` | resolve and `aggregate.value_or` were both arguments of one call (compiler-dependent) | containment, then `aggregate`, then the converter |
| `file:read` | dims decode and `allow_nulls.value_or` were both arguments of `self.read` (compiler-dependent; allow_nulls could not fail) | dims (table, key, cell), then `allow_nulls`, then `self.read` |
| `expr:aggregate` / `expr:aggregate_agents` | `parse_aggregate_op` and the parameter ternary were both arguments of one call (the parameter could not fail) | op, then parameter, then the Expression call |
| `db:open_file` | mode, containment, then metadata silently absent | mode, then containment, then metadata (new check, placed after `resolve_sandboxed_path`; pinned by `open_file('../x','w',{})` and `open_file('x','q',{})`) |
| `table_to_element` | (sol2 up-front table check in Debug only) per entry: unchecked key, then loose `is<sol::table>()` dispatch | `element_table` check, then per entry: key (`lua_string_key`), then the userdata check, then array/value dispatch |

Every other check kept its position. Each `require_table` sits in the decoder that first walks the argument, which runs before any core call in every body, where sol2's Debug up-front check fired before. `collect_group_columns` is still the first statement of both group decoders. `w:write_row` still checks the type before the closed state. No check moved ahead of `resolve_sandboxed_path`. The `query_*` functions have a single Lua-derived decode.

## Tidy (wave gate, `src/lua_runner/`: 14 unique, same set as the base)

| File | Check | Count |
|---|---|---|
| binary.cpp | bugprone-unchecked-optional-access | 3 |
| binary.cpp | modernize-return-braced-init-list | 1 |
| binary.cpp | modernize-raw-string-literal | 1 |
| csv.cpp | readability-identifier-naming | 2 |
| db_core.cpp | bugprone-empty-catch | 1 |
| lua_runner.cpp | bugprone-empty-catch | 2 |
| return_json.cpp | readability-identifier-naming | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 |

`performance-unnecessary-value-param` = 0, `clang-diagnostic-error` = 0. Only line numbers in `binary.cpp` moved.

## `src/lua_runner/` Line Counts (all at most 450)

binary.cpp 334, csv.cpp 416, db_core.cpp 180, db_metadata.cpp 98, db_read.cpp 129, db_time_series.cpp 286, db_write.cpp 308, internal.h 318, lua_runner.cpp 163, path_policy.cpp 63, return_json.cpp 225.

## Accomplishments

- Five helpers in `internal.h` (`lua_type_name`, `lua_type_error`, `require_table`, `lua_string_key`, `optional_from_lua<T>`) hold the one type-error shape.
- Twenty table parameters are now `const sol::object&`. The five hand-written checks route through `require_table`, and the two value-level sites (a group column, an element attribute) are checked. No string literal under `src/lua_runner/` spells the table message any more.
- Four key sites and eight optional sites are covered. `relation_target_from_lua`, `string_key`, `option_entries`' key check and the `collect_group_columns` key check keep their pinned texts. The `has unsupported Lua type` converter messages and the cell-key `must be an array of values` throw are byte-identical.
- 16 new tests in existing files and suites (Lua* 444 → 460, still 12 suites). No existing test line was edited.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Plan defect] Unlisted golden change in commit 1**
- **Found during:** Task 1 green gate
- **Issue:** The plan said no probe passes a wrong-typed group payload. But `write.txt` `vector_not_array` passes `{ value_int = 5 }`, so it gains `, got number`, which is exactly the message the plan prescribes for that case.
- **Fix:** Reviewed the full diff (that one key only, debug text unchanged), promoted with `golden.sh debug --capture`, and re-ran the gate clean.
- **Commit:** `70d2470`

**2. [Rule 3 - Harness] Base-equality check needs path normalisation**
- **Issue:** The literal `diff -r` / `cmp` between the Phase 3 and Phase 4 baselines cannot be silent, because three probes print the harness work directory.
- **Fix:** Compared with `sed -b 's/dedupe-check/fixes-check/'` (byte-preserving), and all 11 files are identical. Plain `sed` drops the CR and gave a false mismatch.

**3. [Rule 2 - Docs] `lua_string_key` documented in commit 3**
- The plan put both helper notes in commit 4. `lua_string_key` landed in commit 3, so its `src/AGENTS.md` note went with it to keep each commit's docs true of its tree. `optional_from_lua` was added in commit 4.

**4. [Rule 1 - Test compile] `Element::set("some_integer", 7)` is ambiguous on MSVC**
- Changed to `int64_t{7}` in the new `UpdateElementRejectsNonTableElement` before the red run.

**Total deviations:** 4 (one plan defect, one harness, one doc timing, one test compile). No scope creep.

## Issues Encountered

None beyond the deviations above.

## Known Stubs

None.

## Threat Flags

None. No new network, file or auth surface. Every change narrows what the Lua boundary accepts (T-04-01..T-04-06 mitigated as planned).

## Next Phase Readiness

- 04-02 (`run_in_scope` C6/C4, empty arrays C7) can use `lua_type_error` for `fn must be a function, got <type>`.
- SAFE-05 continues in 04-02/04-03 and is not marked complete here.
- Counts to carry: Lua* = 460 tests in 12 suites in both builds, C API 27. The golden baseline is `build/fixes-check/baseline/` as of `30a169f`.

## Self-Check: PASSED

- Commits found: `70d2470`, `6014bb2`, `61f0297`, `30a169f`.
- Red files exist and contain `FAILED`: `c1-groups.txt`, `c1-rest.txt`, `c2-keys.txt`, `c5-optionals.txt`, `c5-optionals-release.txt`.
- `build/fixes-check/BASE` and `surface-base.txt` exist. `uv run python scripts/assert_version.py` exits 0. `git diff --stat BASE HEAD -- bindings CMakeLists.txt src/CMakeLists.txt` is empty.
