---
phase: 03-dedupe
plan: 02
subsystem: lua-binding
tags: [sol2, cpp20, lua_runner, refactor, structured-bindings, csv]

requires:
  - phase: 03-dedupe
    provides: "03-01 golden harness and per-commit / per-wave gates in build/dedupe-check/"
provides:
  - "internal.h: collect_entries, option_table, option_entries<N> (the one option walk, owning the table check)"
  - "named option slots in every strict decoder, including quiver.metadata's eight"
  - "csv_cell_to_string as one std::visit over lua_to_value"
  - "CsvWriter::write_row / CsvWriter::close members bound as member pointers"
  - "header_object: one no-header rule for read_csv and read_csv_stream"
affects: [03-03, 04-fixes]

actuals:
  tokens: 9608
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - "const auto& [a, b] = option_entries(options, op, {\"a\", \"b\"}): a braced list deduces N, slots are named next to their keys"
    - "usertype behaviour as members registered by member pointer, one bound name per line"

key-files:
  created: []
  modified:
    - src/lua_runner/internal.h
    - src/lua_runner/db_core.cpp
    - src/lua_runner/csv.cpp
    - src/lua_runner/binary.cpp
    - src/csv/csv_write.cpp
    - src/AGENTS.md

key-decisions:
  - "option_entries owns the options-must-be-a-table check; the three CSV decoders keep their nil early return, quiver.metadata keeps none so nil still reports 'Cannot metadata: options must be a table'"
  - "parse_csv_options names each enum_labels level (attributes, locale_tables, codes) before iterating it, because clang-format broke the nested collect_entries(option_table(...)) range-for headers across lines; the check order is unchanged"
  - "Debug-only sol2 text change accepted for the dot calls w.write_row / w.close; Release unchanged"

patterns-established:
  - "Mutation check per item, run after the commit and reverted with git checkout"

requirements-completed: []

duration: 20min
completed: 2026-10-03
status: complete
---

# Phase 3 Plan 02: Option and CSV Dedupe Summary

**Every strict options decoder now runs through one `option_entries<N>` walk, which owns the table check and returns slots that callers bind by name. CSV cells convert through `lua_to_value`. `CsvWriter`'s two methods are members registered by member pointer. Both read forms build their header through `header_object`. Debug and Release golden output is byte-identical to the base. The only Debug text that changed is the two `CsvWriter` dot-call probes.**

## Performance

- **Duration:** 20 min
- **Started:** 2026-10-03T06:33:23Z
- **Completed:** 2026-10-03T06:52:44Z
- **Tasks:** 2 (a tracer and an auto task with three commits)
- **Files modified:** 6

## Gate evidence

| Commit | Item | Gate line |
|--------|------|-----------|
| `fe45e17` | One option walk with named slots (M9 + M15) | `GATE PASS pairs=36` (debug text unchanged) |
| `dd2bf9c` | CSV cells through `lua_to_value` (M10) | `GATE PASS pairs=36` (debug text unchanged) |
| `e10068a` | `CsvWriter` members plus IN-02 (M11) | `GATE PASS pairs=36` (run with `DEBUG_TEXT_CHANGE=1`: only `dot_writer_write_row` and `dot_writer_close` changed; baseline promoted) |
| `4df3e92` | One header rule (M12) | `GATE PASS pairs=36` (debug text unchanged) |

Each gate run checked the following:
- Debug `Lua*`: 444 tests in 12 suites, all pass.
- C API: 27 tests, all pass.
- The sync test: 6 pass, 0 fail.
- The surface is identical to the base.
- `.set_function(` count is 86.
- Every registered name equals its member name.
- clang-format 22.1.8 is clean.
- Every file is at most 450 lines.
- No planning ID appears under `src/lua_runner`.
- The Debug golden output matches the base.

Wave gate at `4df3e92`: `WAVE GATE PASS tidy=14`.
- Release `Lua*`: 444/12 pass. Release C API: 27 pass.
- `GOLDEN release OK`.
- The GCC 14 `-fsyntax-only -Wall -Wextra` pass is empty.

### Tidy warnings (wave gate)

These are the same 14 warnings as at the end of 03-01. Only line numbers moved.

| File | Check | 03-01 end | Now |
|------|-------|-----------|-----|
| binary.cpp | modernize-return-braced-init-list | 1 | 1 |
| binary.cpp | modernize-raw-string-literal | 1 | 1 |
| binary.cpp | bugprone-unchecked-optional-access | 3 | 3 |
| csv.cpp | readability-identifier-naming (`header_width_`, `kMaxWidth`) | 2 | 2 |
| db_core.cpp | bugprone-empty-catch (`run_in_scope`) | 1 | 1 |
| lua_runner.cpp | bugprone-empty-catch | 2 | 2 |
| return_json.cpp | readability-identifier-naming | 3 | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 | 1 |
| **Total** | | **14** | **14** |

### Invariants checked

- The body of `csv_max_integer_key` is identical to the base (the diff is empty).
- `resolve_sandboxed_path(` counts match the base in every file: csv.cpp 3/3, db_core.cpp 3/3, binary.cpp 4/4.
- `path_policy.cpp` is unchanged against the base.
- `git diff --stat BASE HEAD -- tests bindings CHANGELOG.md CMakeLists.txt` is empty.
- Planning-ID line counts are unchanged: `src/AGENTS.md` 15, `src/csv/csv_write.cpp` 10.
- Single throw sites, counted under `src/lua_runner`:

  | Literal | Count |
  |---------|-------|
  | `: options must be a table"` | 1 |
  | `' must be a table"` | 1 |
  | `option 'header_row' must be an integer` | 1 |
  | `has unsupported Lua type` in csv.cpp | 0 |
  | `option_entries(` call sites | 4 |
  | `for_each(` | 1 |

### Line counts (`wc -l src/lua_runner/*`)

| File | Lines |
|------|-------|
| binary.cpp | 373 (was 377) |
| csv.cpp | 420 (was 446) |
| db_core.cpp | 181 (was 197) |
| db_metadata.cpp | 98 |
| db_read.cpp | 129 |
| db_time_series.cpp | 286 |
| db_write.cpp | 307 |
| internal.h | 256 (was 240) |
| lua_runner.cpp | 144 |
| path_policy.cpp | 63 |
| return_json.cpp | 225 |

## Accomplishments

- **M9 + M15 (DEDUP-04, DEDUP-05):** `internal.h` replaces `csv_options_entries` with three helpers:
  - `collect_entries` is the one `for_each` collect.
  - `option_table` raises `option '<what>' must be a table`.
  - `option_entries<N>` does the table check, then collects, then runs the key-type and unknown-key checks.

  How each caller uses them:
  - `parse_csv_options` binds `[date_time_format, enum_labels]`.
  - `write_csv_options_from_lua` binds `[separator, header]` and decodes the header through `option_table(*header, ...)`.
  - `read_csv_options_from_lua` binds `[separator, header_row_value]` and keeps only the `is<int64_t>()` test.
  - `build_metadata_from_lua` binds all eight slots in key order. It still sits above `bind_binary` (line 58, before the first usertype at line 244), and its comment now says why.
  - `rename_agents` keeps its own `mapping must be a table` check and collects through `collect_entries`.
  - `table_entries` is deleted. `<initializer_list>` is replaced by `<array>`.
- **M10 (DEDUP-04):** `csv_cell_to_string` is one `std::visit` over `lua_to_value(cell, operation, "cell #N")`:
  - nil becomes `""` and a string passes through verbatim.
  - An int64 or a double goes through `append_number`. The `isfinite` check sits in the double branch with its message.
  - A boolean arrives as INTEGER 1/0 and writes `1`/`0`.
  - The unsupported-type text now comes from `lua_to_value` and is byte-identical.

  The comment in `src/csv/csv_write.cpp` now names the `internal.h` raisers in its lead sentence, and the `src/AGENTS.md` boolean bullet lists `csv_cell_to_string` among the callers of `lua_to_value`.
- **M11 + IN-02 (DEDUP-04):**
  - `CsvWriter::write_row` holds the old lambda body verbatim with `self.` removed. `CsvWriter::close` is one line.
  - The usertype registers `&CsvWriter::write_row` and `&CsvWriter::close`, with each bound name alone on its own line.
  - `bind_csv`'s parameter is now `state` in csv.cpp and in internal.h.
  - The row-width paragraph in `src/AGENTS.md` names `CsvWriter::write_row`.
- **M12 (DEDUP-04):** `header_object(lua, header)` returns nil for an empty header and the name table otherwise. `read_csv` assigns it before `rows`, and `read_csv_stream` passes it to `on_row`. The two "must not diverge" comments are merged into its single comment. No comment under `src/lua_runner/` calls no-header mode a future feature.

## Check-order ledger

reordered checks: none

Each entry gives the order before and after, which is identical.

- **parse_csv_options:**
  1. nil or missing returns the defaults.
  2. The table check (now inside `option_entries`).
  3. Per key: is it a string, is it a known option.
  4. `date_time_format` type.
  5. `enum_labels`: the table check, then per attribute the string key, the map entry, the locale table check, then per locale the string key, the map entry, the codes table check, then per label the string key and the code conversion.

  The nil early return still precedes the `option_entries(` call.
- **write_csv_options_from_lua:**
  1. nil returns the defaults.
  2. The table check.
  3. Per key: is it a string, is it a known option.
  4. `separator`.
  5. `header`: the table check (`option_table`), then `csv_header_from_lua`.

  The nil early return still precedes `option_entries(`.
- **read_csv_options_from_lua:**
  1. nil returns the defaults.
  2. The table check.
  3. Per key: is it a string, is it a known option.
  4. `separator`.
  5. `header_row`: is it an integer (one test, same message), then is it negative.

  The nil early return still precedes `option_entries(`. Dropping the `get_type() != number` test changes no outcome: `is<int64_t>()` is `lua_isinteger` under `SOL_SAFE_NUMERICS`, and both tests raised the same text.
- **build_metadata_from_lua:**
  1. The table check, which still fires for nil.
  2. Per key: is it a string, is it a known option.
  3. The eight `el.set` calls in the old statement order: version, initial_datetime, unit, labels, dimensions, dimension_sizes, time_dimensions, frequencies.
  4. `from_element`.
- **rename_agents:**
  1. `mapping must be a table`.
  2. Collect every entry.
  3. Per entry: the key, then the value (`lua_cell_as`).
- **csv_cell_to_string:**
  1. nil.
  2. boolean.
  3. int64.
  4. double, with the finite check.
  5. string.
  6. unsupported.

  The old function listed the same order explicitly. It now lives in `lua_to_value`, which is unchanged.
- **CsvWriter::write_row:**
  1. Row type.
  2. Closed.
  3. Row keys and the width cap (`csv_max_integer_key`).
  4. Cells.
  5. Header width.
  6. Write.
  7. Increment the ordinal.
- **read_csv:**
  1. Sandbox.
  2. Options.
  3. Reader.
  4. Rows.
  5. Header (assigned before `rows` in the result table, as before).
- **read_csv_stream:**
  1. `on_row` type.
  2. Sandbox.
  3. Options.
  4. Reader.
  5. Header.
  6. Rows.

## Debug-only text changes (Release: none)

Both probes share the prefix `[string "-- schema: collections.sql..."]:<line>: ` (lines 70 and 71), which is left out of the table. No `control_` probe changed: `control_writer_write_row` and every other `control_` line are identical, and the `control_` diff between `debug_text.base.pretty.txt` and the promoted baseline is 0 lines.

| Probe | Base text | New text |
|-------|-----------|----------|
| `dot_writer_write_row` | `stack index 1, expected userdata, received table: value is not a valid userdata (bad argument into 'void(quiver::lua_internal::CsvWriter&, const sol::basic_object<sol::basic_reference<0> >&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| `dot_writer_close` | `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'void(quiver::lua_internal::CsvWriter&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |

No test pins either text.

## Mutation results

Each mutation was run after its commit and then reverted. `git diff --exit-code -- src/lua_runner` was clean after every revert.

| Item | Mutation | Result |
|------|----------|--------|
| M9 (a) | Delete `option_entries`' table check | 5 of 10 `*OptionsMustBeATable*:*NonTableOptions*` tests failed: `PositionalSeparatorStringThrowsOptionsMustBeATable`, `NumberInOptionsSlotThrowsOptionsMustBeATable`, `BooleanInOptionsSlotThrowsOptionsMustBeATable`, `StreamPositionalSeparatorStringThrowsOptionsMustBeATable`, `LuaRunner_WriteCsv.NonTableOptionsThrows` |
| M9 (b) | Delete the `is<int64_t>()` test | `LuaRunner_ReadCsv.HeaderRowAsFractionThrowsMustBeAnInteger` failed (substring not found) |
| M15 (c) | Swap `dimensions` and `time_dimensions` in the binding list | **Differs from the plan's expectation:** the tests do catch this swap. `Lua*` reported 399/444 passing; the 45 failures are every `quiver.metadata` user in `LuaBinaryTest`, `LuaExpressionTest` and `LuaRunner_Lifecycle`. `golden.sh debug` also failed (`FAIL: binary exited nonzero`). |
| M15 (c2, extra) | Swap `version` and `unit` | Also caught by tests (`LuaBinaryTest.*` failures). `golden.sh debug` exited 1. |
| M10 | Delete the `isfinite` check | 3 of 4 `*NonFinite*` tests failed: `NonFiniteNumberCellThrowsNamingWriteRowAndRowOrdinal`, `RejectedNonFiniteRowLeavesFileIntactAfterPcallAndClose`, `NonFiniteNumberCellIsPrefixedWriteRowError` |
| M11 | Move the closed check above the table-type check | `LuaRunner_WriteCsv.NonTableRowOnClosedWriterReportsTheType` failed |
| M12 | `header_object` always returns the table | Both `StreamHeaderIsNilWhenWholeFileHeaderIsAbsent` and `BomStrippedUnderExplicitHeaderRowAndNoHeader` failed |

The plan expected that "nothing pins the mapping". For these two swaps that is too pessimistic: `BinaryMetadata` validation rejects the swapped values, so the tests fail. A swap of two slots with the same type and compatible values, which the tests would miss, is still caught by the golden `metadata_slots` probe.

## Tracer gate (delegated self-verification)

`auto_advance` is false, so the tracer's human-verify checkpoint applied. Following the user's standing preference, I verified it myself:
- `GATE PASS pairs=36`, with the surface and debug text unchanged.
- All four decoders go through `option_entries`.
- Mutations (a) through (c) were each caught.

Verdict: pass, so execution continued.

## Decisions Made

- `parse_csv_options` names each `enum_labels` level (`attributes`, `locale_tables`, `codes`) before iterating it. The literal `collect_entries(option_table(...))` range-for headers were reformatted by clang-format into multi-line `for (` headers, which is hard to read. Every `option_table` call still runs at the same point relative to the map insertions and string-key checks.
- `read_csv_options_from_lua` binds its second slot as `header_row_value` because the function already has a local `header_row`.

## Deviations from Plan

None in shipped behaviour. The M15 mutation result differs from the plan's prediction, as recorded above.

**Total deviations:** 0 auto-fixed.

## Issues Encountered

- `golden.sh` deletes `build/dedupe-check/tmp` on every run, so gate logs redirected into `tmp/` vanished. Logs now go to `build/dedupe-check/m10_gate.txt`, `m11_gate.txt`, `m12_gate.txt` and `wave_0302.txt`.

## Next Phase Readiness

- 03-03 (binary, registry, docs) can reuse the harness unchanged. The promoted debug-text baseline now holds the M11 text.
- DEDUP-04 is delivered by this plan. DEDUP-05 (M15 here, M16 in 03-01) and DEDUP-06 still wait on 03-03 (M13, M14). I did not mark any requirement complete in REQUIREMENTS.md.

## Self-Check: PASSED

- All 4 commits were found (`fe45e17`, `dd2bf9c`, `e10068a`, `4df3e92`). No commit deletes a file.
- All 6 modified files exist.
