---
phase: 05-path-policy-test-and-docs
plan: 02
subsystem: docs
tags: [planning-ids, comments, agents-md, csv, describe]
status: complete
requires:
  - "05-01 harness: build/phase5-check/ids.sh, ids.regex, gate.sh"
provides:
  - "13 files free of planning IDs (repo-wide 256/19 -> 180/6; only Plan 05-03's six test files remain)"
  - "build/phase5-check/SWEEP_BASE (c50772e) and residual.sh (comment-only proof, gitignored)"
affects: [05-03, 05-04]
tech-stack:
  added: []
  patterns:
    - "A catalogue entry names its pinning test on an indented 'pinned by Suite.Test' line under the message"
key-files:
  created: []
  modified:
    - .gitattributes
    - AGENTS.md
    - tests/test_c_api_database_time_series_row.cpp
    - bindings/python/tests/test_database_query.py
    - bindings/js/test/database-csv.test.ts
    - src/csv/csv_read.cpp
    - src/csv/csv_read.h
    - src/csv/csv_write.cpp
    - src/csv/csv_write.h
    - src/database_describe.cpp
    - src/ui_metadata.cpp
    - src/AGENTS.md
    - tests/AGENTS.md
decisions:
  - "AGENTS.md keeps the f92af8d pointer and the pitfall heading 'SAVEPOINT Complexity Leaking Into the Design'; the .planning path and pitfall number are gone"
  - "The src/AGENTS.md collect_garbage() note no longer cites tests as its guard: the writer tests pass through close_open_handles() first, so they would not catch a second GC pass being needed"
metrics:
  duration: "25 min"
  completed: 2026-10-03
actuals:
  tokens: 12090
  tasks: 3
  commits: 3
---

# Phase 5 Plan 02: Planning-ID Sweep, First Half Summary

The first 13 files of the sweep no longer contain planning IDs. Each ID was either dropped because its sentence already gave the reason, replaced with the rule it stood for, or replaced with the name of the test that pins it. The `src/csv` message catalogue now names real `LuaRunner_WriteCsv*` tests. `tests/AGENTS.md` now says the stack check is off as well as the getter. Only code comments changed (`RESIDUAL=0`), and no test was renamed or added (`gate.sh` list identity).

## Gate trail

| After | `ids.sh` | `residual.sh` | `gate.sh` |
|-------|----------|---------------|-----------|
| base `c50772e` | IDS=256 FILES=19 | - | - |
| Task 1 `f5d23ba` | IDS=249 FILES=14 | RESIDUAL=0 | P5 GATE PASS lua=477 sandboxed=11 capi=27 tests=1454 c=543 |
| Task 2 `a3f7c92` | IDS=217 FILES=10 | RESIDUAL=0 | same |
| Task 3 `7b5335d` | IDS=180 FILES=6 | RESIDUAL=0 | same |

The six files left are exactly the ones Plan 05-03 owns: `test_c_api_database_csv_export.cpp`, `test_database_time_series_row.cpp`, `test_database_ui_metadata.cpp`, `test_database_update.cpp`, `test_lua_runner_read_csv.cpp` and `test_lua_runner_write_csv.cpp`.

Binding checks for Task 1:
- Python `test_database_query.py`: 21 passed, and ruff check and ruff format are clean.
- JS `database-csv.test.ts`: 11 pass. biome reports 6 infos and no errors.

clang-format 22.1.8 dry run is clean on every touched C++ file.

Tracer gate (auto mode): after the Task 1 commit, the tracer's verify was re-run end to end (ids, residual, gate, Python, JS) and passed, so expansion continued.

## Replacements per file

**`.gitattributes`**: `(02-03-PLAN.md, D-24)` became "read byte for byte by `LuaRunner_ReadCsv.EnergiaRegressionJunkRowAboveUnitsRowBelowHeader` and `GdRegressionQuotedCommaAndEnglishMonthNames`". These are the only two tests that read `fixtures/` (confirmed by grep).

**`AGENTS.md`**: `` `git show f92af8d:.planning/research/PITFALLS.md`, Pitfall 4 `` became `` commit `f92af8d`, "SAVEPOINT Complexity Leaking Into the Design" ``. The reason (the nesting complexity the no-op guard exists to avoid) is kept, and `grep -c f92af8d AGENTS.md` = 1.

**`tests/test_c_api_database_time_series_row.cpp`**: `(CAPI-11..13)` was dropped.

**`bindings/python/tests/test_database_query.py`**: `(QUERY-01)` and `(QUERY-02)` were dropped, and the dashes were padded so each banner stays 79 columns.

**`bindings/js/test/database-csv.test.ts`**: `(JSCSV-01)` and `(JSCSV-02)` were dropped.

**`src/csv/csv_write.cpp`**:

| Was | Now |
|-----|-----|
| `TEST-12 message catalogue (D-36: ...)` | "Message catalogue, asserted by the tests in tests/test_lua_runner_write_csv.cpp ({operation} is always the public Lua method the script called)" |
| parent directory missing `(WRITE-07)` | pinned (prefix only) by `LuaRunner_WriteCsv.MissingParentDirectoryThrowsAndDoesNotCreateIt` |
| already closed `(WRITE-05)` | pinned by `LuaRunner_WriteCsv.UnsupportedCellOnClosedWriterReportsClosed` (asserts the exact text) |
| not a finite number `(FMT-05)` | pinned by `LuaRunner_WriteCsvErrors.NonFiniteNumberCellIsPrefixedWriteRowError` |
| row width `(FMT-07)` | pinned by `LuaRunner_WriteCsv.RowLongerThanHeaderThrowsNamingOrdinalAndCounts` |
| options table `(LUA-10)` | pinned by `LuaRunner_WriteCsv.EscapingPathTakesPrecedenceOverInvalidSeparator` |
| `(FMT-02, extended to ...)`, `(FMT-03)`, `(WRITE-07)` in the constructor | dropped (the reason was already in the sentence) |
| `(WRITE-08, threat T-04-02, accepted)` | "Opening over an existing file truncates it; there is no overwrite guard, by decision." |

Each test name was checked with `grep -c '<name>)' tests/test_lua_runner_write_csv.cpp`, and each returned 1.

**`src/csv/csv_write.h`**:
- `D-37`, `D-40`, `FMT-01`, `FMT-02`, `WRITE-08` and `T-04-02` were dropped, because each sentence already gave the reason.
- `TEST-12 matches on those exact strings` became `tests/test_lua_runner_write_csv.cpp matches on those exact strings`.
- `per D-36:` became a colon.

**`src/csv/csv_read.cpp`**:
- `per D-12` was dropped.
- "this milestone exists for" became "db:read_csv exists for (a junk row above the header, a units row below it)".
- `PARSE-09 requires` became "so it is bounded and memory does not grow with the file".
- `(D-13's guarantee)` was dropped.
- `(D-22)` became "this is the constructor's Pattern 1 catalogue, in evaluation order".
- Each `LUA-08` (three places) became the rule itself: no csv-parser or standard-library message reaches Lua without a Pattern 1 prefix.
- `D-20` became "the option is 1-based, default 1".
- The second `(D-22)` was dropped.
- `per D-08` became "a Lua error raised inside the callback, re-thrown verbatim".

**`src/csv/csv_read.h`**: `(D-20)` was dropped. `(LUA-03)` was dropped because the sentence already says the two Lua forms share one Reader and so cannot diverge.

**`src/database_describe.cpp`**:
- `D-03, and the T-01-03 mitigation` became "the mitigation against terminal escape sequences smuggled in through sidecar text".
- `D-03's named` became "the named".
- `D-02`, `D-04`, `D-01`, `D-07/D-08`, `D-06` and `D2-12` were dropped.
- `per D-05 ... suppressed by D-04` became "Compared against the sidecar label too, even when the label clause itself was suppressed as redundant".
- `D2-06 / project decision D-09: deliberate divergence from D-06's enum clause` became "Deliberate divergence from the enum clause in write_ui_clauses".

**`src/ui_metadata.cpp`**:
- `READ-04`, `READ-02/D-17`, `D-18` and `D-19` were dropped.
- `RESEARCH.md Pattern 3` became "a value is either a string or a table with an "en" key".
- `RESEARCH.md Pattern 4, which corrects CONTEXT.md's "[[vocab]]" shorthand` became `there is no "[[vocab]]" wrapper to read`.
- `SAFE-01` became "the no-sidecar baseline".

**`src/AGENTS.md`**:
- `Phase 2's header_row option, D-20` was dropped, because the rule is in place.
- `D-37`, `D-34`, `D-40`, `WRITE-08`, `D-38` and the "Phase 1 of the "UI Metadata in describe" milestone" aside were dropped.
- `FMT-07's row-width enforcement` became "The row-width enforcement".
- `D-05` became "the tooltip suppression rule".
- `**D-09 (deliberate divergence from D-06):**` became `` **Deliberate divergence from the `enum {}` clause:** ``.
- "breaking LUA-08" became "breaking the rule that no standard-library, csv-parser or sol2 message reaches a script without a Pattern 1 prefix".
- `(RESEARCH.md Q1)` became "a one-off executed probe ... (no standing test guards it: the writer tests pass through `close_open_handles()` first)". See Deviations.

**`tests/AGENTS.md`**:
- `SAFE-01` became "the no-`ui/` baseline, `NoUiDirReportsUnchanged`" (the name was confirmed in `tests/test_database_ui_metadata.cpp`).
- `(Phase 2, TEST-02)` and `(D-24)` were dropped.
- `the WRITE-08 truncate-at-open behaviour` became "the truncate-at-open behaviour".
- In the mixed-array bullet, the past-tense account of a Phase 2 decoder ("291/291 ... TEST-05") became a present-tense instruction: after touching `lua_table_to_vector` or adding another `sol::object` type check, run `Lua*` in Release and Debug and expect identical results.
- The same bullet now says that the getter **and the stack check** (`SOL_SAFE_STACK_CHECK=0`) are off in every build and every other sol2 safety is on (the D-08 doc nit; `awk ... | grep -c 'stack check'` = 1).

## Deviations from Plan

**1. [Rule 1 - Accuracy] No tests named for the `collect_garbage()` note in `src/AGENTS.md`**
- **Found during:** Task 3
- **Issue:** The plan suggested replacing `(RESEARCH.md Q1)` with tests that would fail if one `collect_garbage()` call were not enough, for example `UnclosedWriterIsFlushedWhenRunReturns` and `WriterHeldInAGlobalIsClosedWhenRunReturns`. Both exist, but `close_open_handles()` closes every writer and binary file before the collection runs (`src/lua_runner/lua_runner.cpp`, `GcGuard`). Neither test would notice if a second GC pass were needed, so naming them would claim a guard that does not exist.
- **Fix:** The note now says the probe was a one-off, that no standing test guards the call, and why. The rule against "hardening" the call into a loop is kept.
- **Commit:** 7b5335d

**2. [Rule 1 - Accuracy] The parent-directory catalogue entry is marked "prefix only"**
- **Found during:** Task 2
- **Issue:** No test asserts the full text "parent directory does not exist" (`git grep` over tests/ and bindings/ finds nothing). `MissingParentDirectoryThrowsAndDoesNotCreateIt` asserts only the `Cannot write_csv:` prefix and checks that the directory was not created.
- **Fix:** That entry reads "pinned (prefix only) by ...", and the catalogue header says "asserted by" rather than "matched exactly by".
- **Commit:** a3f7c92

**3. Width:** Some re-wrapped lines in `src/AGENTS.md` and two C++ comments ran past roughly 100 columns after the substitutions, so they were re-wrapped again before the Task 3 commit. The "C1 block" phrase stays on one line, so the gate's false-positive exclusion still applies.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: build/phase5-check/SWEEP_BASE, build/phase5-check/residual.sh
- FOUND commits: f5d23ba, a3f7c92, 7b5335d
- Every test name written into code or docs exists in tests/ (grep counts of at least 1 above)
- `.planning/config.json` and `.gsd/` are not staged
