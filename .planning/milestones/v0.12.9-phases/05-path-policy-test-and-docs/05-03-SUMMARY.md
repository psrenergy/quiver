---
phase: 05-path-policy-test-and-docs
plan: 03
subsystem: docs
tags: [planning-ids, comments, tests, write_csv, read_csv, ui-metadata]
status: complete
requires:
  - "05-02: the first 13 files swept; build/phase5-check/SWEEP_BASE and residual.sh"
provides:
  - "DOC-01 closed: repo-wide ids.sh prints IDS=0 FILES=0 (180 -> 128 -> 61 -> 0)"
  - "Correct flush comments on the two tests whose w:close() is load-bearing"
affects: [05-04]
tech-stack:
  added: []
  patterns:
    - "A Lua comment inside a raw-string script is edited one line for one line, so [string]:N: line numbers cannot move"
key-files:
  created: []
  modified:
    - tests/test_database_update.cpp
    - tests/test_database_time_series_row.cpp
    - tests/test_c_api_database_csv_export.cpp
    - tests/test_database_ui_metadata.cpp
    - tests/test_lua_runner_read_csv.cpp
    - tests/test_lua_runner_write_csv.cpp
decisions:
  - "WRITE-06 is spelled 'the close-at-exit flush' in every test comment and diagnostic"
  - "FMT-02 is spelled 'the lone-empty-cell quoting' (a record whose only cell is empty is written quoted so it is not a blank line)"
  - "Tracer gate run as a self-verification (user memory rule) although auto mode is off: the tracer's full verify passed, so expansion continued"
metrics:
  duration: "15 min"
  completed: 2026-10-03
actuals:
  tokens: 21282
  tasks: 3
  commits: 3
---

# Phase 5 Plan 03: Planning-ID Sweep, Second Half Summary

The six test files that still carried planning IDs are clean, so DOC-01 is closed: the repo-wide gate prints `IDS=0 FILES=0`. Outside comments, only the 19 approved failure-diagnostic strings changed, and each lost only its ID or its "not yet implemented" note. The two write_csv tests whose `w:close()` is load-bearing now give the current reason: the close-at-exit flush runs when `run()` returns, and that is after the same script's `db:read_csv` reads the file back.

## Gate trail

| After | `ids.sh` | `residual.sh` | `gate.sh` |
|-------|----------|---------------|-----------|
| start (`b6f8ad1`) | IDS=180 FILES=6 | RESIDUAL=0 | - |
| Task 1 `6d8d4cf` | IDS=128 FILES=2 | RESIDUAL=0 | P5 GATE PASS lua=477 sandboxed=11 capi=27 tests=1454 c=543 |
| Task 2 `27dfcac` | IDS=61 FILES=1 | RESIDUAL=34 | same |
| Task 3 `72b6da3` | IDS=0 FILES=0 | RESIDUAL=44 | same |

After every task:
- clang-format 22.1.8 dry run is clean on the touched files.
- `git grep -nE '\]:[0-9]+:' -- tests` is empty.
- The residual filter from the plan prints nothing.

After Task 3, `quiver_tests --gtest_filter='LuaRunner_WriteCsv*'` exits 0 (64 passed). `gate.sh`'s list identity confirms that no test was renamed, added or removed across the whole sweep.

Tracer gate: `.planning/config.json` has auto mode off, which would normally mean stopping for a human check after Task 1. Following the user's standing rule (verify gates yourself instead of asking), I re-ran Task 1's full verify (ids, residual, clang-format, gate) and it passed, so expansion continued.

## Task 1: core and C API tests (`6d8d4cf`)

- `test_database_update.cpp`: `(BUG-01)` dropped. `test_database_time_series_row.cpp`: `(CORE-11..14)` dropped.
- `test_c_api_database_csv_export.cpp`: each of the eight `CSV-0x:` / `OPT-0x:` headers keeps the text after its colon.
- `test_database_ui_metadata.cpp` (42 lines):
  - Section headers keep the text after the colon (`Task 1-0x-0x:`, `Plan 02-01:`). `(READ-0x)`, `(RENDER-03)`, `(SAFE-02)` and `(TDD)` were dropped.
  - `D-0x`, `D2-0x`, `READ-0x` and `D-12` test tags were dropped, because each comment already states the rule.
  - `SAFE-01` became "no-sidecar". `SAFE-02` became "malformed-sidecar", since the only caller of `expect_reports_match` is the four `Malformed*RendersIdentical` tests.
  - `T-01-03` became "Terminal-escape regression". `(T-01-07)` was dropped, because the sentence already says the guard restores the process-wide CWD when an assertion fails.
  - "plan 01/02 of this phase" and "so plan 01 and plan 02 can assert" became present tense (every test below, the tests below).
  - `01-PATTERNS.md`'s idiom became "a per-test temp directory created in SetUp and removed in TearDown". `(CONTEXT.md "Two resolution traps")` was dropped, because the sentence already names both traps (trailing slash, bare relative path).
  - "D-03's prose" and "D-06's `enum {}` clause" now name the thing itself.
  - The "C1 block" Unicode prose is untouched: the count is 1 before and 1 after.

## Task 2: read_csv tests (`27dfcac`)

- **Comments:**
  - `LUA-08` became the rule (no standard-library message reaches a script unprefixed).
  - `LUA-03` was dropped where the sentence already says the two forms must not diverge.
  - `Finding 1` became the name of the test that covers that case, `HeaderRowPastEndOfFileThrowsExactMessage`.
  - `D-13` became "a blank line is never a row".
  - `D-22` became "Reader catalogue" / "the csv-parser wrapper entry".
  - `02-02-PLAN.md Task 2`'s aside became a plain statement of the boundary case.
  - The history of the "twelve" planning draft and the "since the option did not exist before this plan" sentence were dropped.
  - The `TEST-0x`, `PARSE-0x`, `D-xx` and `Phase N` tags were dropped.
- **17 diagnostic strings:** the `"PARSE-0x: "` / `"LUA-06: "` prefix was removed by one regex limited to the start of a string literal. Every `assert` condition and every compared value is byte-identical.
- **6 embedded Lua comments**, edited in place (line counts below).

| Script (test) | Lua comment lines edited | Raw-string lines before | After |
|---------------|--------------------------|-------------------------|-------|
| `EnergiaRegressionJunkRowAboveUnitsRowBelowHeader` | 2 (`D-22`, `D-23`) | 26 | 26 |
| `GdRegressionQuotedCommaAndEnglishMonthNames` | 4 (`LUA-07`, `D-20` + `plan 02-01` over 2 lines, `PARSE-02`) | 27 | 27 |

The diff hunks for both scripts are `-622 +620`, `-630 +628`, `-654 +652`, `-660,2 +658,2` and `-667 +665`, each removing as many lines as it adds. All 30 multi-line raw strings in the file have the same line count before and after (awk check). `RESIDUAL=34` is exactly what the plan predicted: 17 removed lines plus 17 added. The one diagnostic written over two Lua lines (`both ANO positions ...`) has its string on its own line, so it still counts once on each side.

## Task 3: write_csv tests (`72b6da3`)

- **Comments:**
  - `FMT-/WRITE-/TEST-/LUA-/D-` tags were dropped where the sentence already gives the reason.
  - `FMT-02` became "the lone-empty-cell quoting".
  - `TEST-09 shape` became "the single-column empty-cell shape".
  - `WRITE-06` became "the close-at-exit flush".
  - `WRITE-08` became "the truncate-at-open behaviour".
  - `EDGE FMT-07/...` became "Empty-row edge case" / "Encoding edge case".
  - `ROADMAP criterion N`, `RESEARCH.md Q4`, `Pitfall 2`, `04-01 task 3` (now "INTERIOR by design"), `DOC-05`, `D-49` and `(the ROADMAP criterion 4 trap)` were dropped. The surviving sentence still says the runner is never destroyed, moved or reset between the two runs.
- **Stale flush comments (D-11), all three rewritten:**
  - The header comment of `RejectedNonFiniteRowLeavesFileIntactAfterPcallAndClose` now says the close-at-exit flush runs when `run()` returns, which is after the `db:read_csv` in the same script, so without the close the read-back hits the reader's empty-file error.
  - The header paragraph of `RejectedRowLeavesFileIntactProvenBothHalves` says the same. It also drops the old explanation that "nothing calls lua_close, so an abandoned writer is never finalized", because `close_open_handles()` now closes it.
  - The trailing comment on that test's `w:close()` now reads `-- load-bearing: the run-exit flush comes after the read-back below`.
  - Checks: `grep -c 'only flush'` = 0, and `grep -B8 ... | grep -c 'run()'` = 1 for each of the two tests.
- **2 `FAIL()` diagnostics:** "(WRITE-06 not yet implemented)" became "(the close-at-exit flush did not reach disk)", and "(... for the throw path)" became "(... on the throw path)". Both still span 2 lines.
- **Embedded Lua comments, edited in place:**

| Script (test) | Lua comment edited | Raw-string lines before | After |
|---------------|--------------------|-------------------------|-------|
| `ReferenceWorkedExampleRunsAndRoundTripsItsOwnData` | `(FMT-08)` dropped | 26 | 26 |
| `RejectedRowLeavesFileIntactProvenBothHalves` | trailing comment on `w:close()` | 9 | 9 |
| `UnclosedWriterIsFlushedWhenRunReturns` | `WRITE-06 must flush this` | 4 | 4 |
| `ScriptErrorMidWriteStillLeavesEarlierRowsReadable` | `WRITE-06's flush ... (D-47)` | 5 | 5 |

All 141 multi-line raw strings in the file have the same line count before and after.

## Whole-sweep residual classification (Plans 05-02 and 05-03)

Plan 05-02 left `RESIDUAL=0`, so every residual line comes from this plan. The final `RESIDUAL=44` breaks down into 19 diagnostic sites (42 lines) and 1 trailing-comment change (2 lines). No line falls into any other category.

| # | File | Line(s) | Kind |
|---|------|---------|------|
| 1-6 | test_lua_runner_read_csv.cpp | `DirtyFileParsesEveryParserRequirement`, six `assert(..., "PARSE-0x: ...")` | diagnostic string (6 sites, 12 lines) |
| 7 | test_lua_runner_read_csv.cpp | `LfAndCrlfEndingsParseIdentically`, `EXPECT_EQ(crlf_json, lf_json) << "PARSE-06: ..."` | diagnostic string (1 site, 2 lines) |
| 8-11 | test_lua_runner_read_csv.cpp | `BomStrippedUnderExplicitHeaderRowAndNoHeader`, four `assert(..., "PARSE-05: ...")` | diagnostic string (4 sites, 8 lines) |
| 12-17 | test_lua_runner_read_csv.cpp | `RepeatedAndBlankHeaderNamesAllReachable`, six `"LUA-06: ..."` messages (two are continuation lines starting with `"`) | diagnostic string (6 sites, 12 lines) |
| 18 | test_lua_runner_write_csv.cpp | `UnclosedWriterIsFlushedWhenRunReturns` `FAIL() <<` + continuation | diagnostic string (1 site, 4 lines) |
| 19 | test_lua_runner_write_csv.cpp | `ScriptErrorMidWriteStillLeavesEarlierRowsReadable` `FAIL() <<` + continuation | diagnostic string (1 site, 4 lines) |
| - | test_lua_runner_write_csv.cpp | `w:close()  -- load-bearing: ...` in `RejectedRowLeavesFileIntactProvenBothHalves` | trailing-comment change (`w:close()` itself unchanged; 2 lines) |

Total: 34 + 8 + 2 = 44 lines.

## Deviations from Plan

**1. [Rule 1 - Accuracy] Two more stale sentences corrected beyond the three named comments**
- **Found during:** Task 3
- **Issue:** The header of `RejectedRowLeavesFileIntactProvenBothHalves` also explained that "nothing calls lua_close", so an abandoned writer is never finalized. That stopped being true once `close_open_handles()` landed.
- **Fix:** The whole paragraph was rewritten to the current reason. It is still the same comment block the plan names.
- **Commit:** 72b6da3

**2. Width:** A few substitutions produced comment lines of 106 to 113 columns, under the 120 `ColumnLimit`, so clang-format is clean. The worst ones (up to 120) were re-wrapped before committing.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND commits: 6d8d4cf, 27dfcac, 72b6da3 (`git log`)
- FOUND: all six modified test files. Each task commit lists exactly its own files (`git show --name-only`).
- `ids.sh` prints `IDS=0 FILES=0`; `gate.sh` prints `P5 GATE PASS lua=477 sandboxed=11 capi=27 tests=1454 c=543`
- `.planning/config.json` and `.gsd/` are not staged
