---
phase: 02-c-api-quiver-database-add-time-series-row
plan: 02
subsystem: tests
tags: [c-api, ffi, time-series, capi-13, gtest]

# Dependency graph
requires:
  - phase: 02-c-api-quiver-database-add-time-series-row
    plan: 01
    provides: quiver_database_add_time_series_row C API declaration + implementation
provides:
  - GTest coverage at the FFI boundary for quiver_database_add_time_series_row across all CAPI-13 scenarios
  - 12 TEST(DatabaseCApi, AddTimeSeriesRow*) cases asserting Pattern-1 errors + D-07 wrapper-owned error + transaction matrix
affects:
  - 03-* (Phase 3 binding generators / per-language tests will marshal the same 12-scenario contract)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Mirrored existing UpdateTimeSeriesGroup* error-path idiom: inline `std::string msg = quiver_get_last_error();` + `EXPECT_NE(msg.find(...), std::string::npos) << \"Actual: \" << msg;` — no `capture_*_error` helper introduced (CONTEXT.md discretion)"
    - "Multi-dim assertion idiom: sort returned rows by full PK tuple before EXPECT (matches the C++ Phase 1 test idiom)"
    - "Per-row scoped braces in TransactionMatrix to keep `dt_buf` / `val_buf` arrays local — avoids unused-variable warnings across the three phases"

key-files:
  created:
    - .planning/phases/02-c-api-quiver-database-add-time-series-row/02-02-SUMMARY.md
  modified:
    - tests/test_c_api_database_time_series.cpp

key-decisions:
  - "Inline quiver_get_last_error() (no capture helper) — mirrors the seven existing UpdateTimeSeriesGroup* error tests at lines 612, 636, 672, 713, 754, 796; keeps assertion text co-located with the call site"
  - "TEST 5 (PartialValueColumns) verifies via read_time_series_group only — INTEGER NULL → 0 and FLOAT NULL → 0.0 at the read C API; the round-trip confirms partial inserts persist with supplied columns intact. Did NOT supplement with read_time_series_row because multi-dim PK (id, date_time, block) means row-read with a single date_time cannot disambiguate blocks."
  - "Per-row scoped braces around dt_buf/block_buf/load_buf/flag_buf arrays in TransactionMatrix and the multi-dim insert/upsert tests — avoids 'unused variable' diagnostics where the same buffer name would re-declare across phases or rows"
  - "Added <algorithm> include — required by std::sort calls in MultiDim* and PartialValueColumns tests"

requirements-completed: [CAPI-13]

# Metrics
duration: ~25min
completed: 2026-05-19
---

# Phase 02 Plan 02: C API Tests for quiver_database_add_time_series_row Summary

**Twelve GTest cases at the FFI boundary covering every CAPI-13 scenario — nine C++ mirrors plus three FFI-boundary additions (NULL pointer args, unknown `column_types[c]` integer, NULL column arrays with non-zero `column_count`). All pass; full `quiver_c_tests.exe` suite 483/483 with no regressions.**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-05-19 (worktree spawn)
- **Completed:** 2026-05-19
- **Tasks:** 2/2
- **Files modified:** 1 (`tests/test_c_api_database_time_series.cpp` — +797 inserted lines across the two commits)

## Accomplishments

- Appended a new `// add_time_series_row tests (CAPI-11..13)` section to `tests/test_c_api_database_time_series.cpp` immediately after the closing brace of `UpdateTimeSeriesGroupDateTimeStringInterchangeable` (per CONTEXT.md discretion + plan instruction).
- Wrote five happy-path tests (Task 1) and seven error-path / FFI-boundary tests (Task 2) — twelve total — exercising:
  - Schema `collections.sql` (single-dim PK `(id, date_time)`) — TESTs 1, 2, 9, 10.
  - Schema `multi_dim_time_series.sql` (composite PK `(id, date_time, block)`) — TESTs 3, 4, 5, 6, 7, 8, 11, 12.
- Added `<algorithm>` to the include block (needed for `std::sort` in the multi-dim and partial-columns tests).
- Confirmed `cmake --build build --config Debug` compiles cleanly with no new warnings.
- Confirmed `./build/bin/quiver_c_tests.exe --gtest_filter="DatabaseCApi.AddTimeSeriesRow*"` reports `[  PASSED  ] 12 tests` and exits 0.
- Confirmed `./build/bin/quiver_c_tests.exe` (full suite, no filter) reports `[  PASSED  ] 483 tests` (up from 471 baseline by exactly +12) and exits 0 — no regressions in any sibling test file.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add happy-path tests (insert, upsert single-PK, multi-dim insert, multi-dim upsert, partial value columns)** — `14e0d41` (test)
2. **Task 2: Add error-path and FFI-boundary tests (missing dim, unknown col, type mismatch, transaction matrix, null args, unknown column type int, null column arrays with count)** — `d9aa404` (test)

## Files Created/Modified

- `tests/test_c_api_database_time_series.cpp` — Added 12 new `TEST(DatabaseCApi, AddTimeSeriesRow*)` cases in a contiguous block under the new `// add_time_series_row tests (CAPI-11..13)` section, immediately after the closing brace of `UpdateTimeSeriesGroupDateTimeStringInterchangeable`. Also added `<algorithm>` to the include block.

## Verification

**Test count check (from grep):**
- `grep -c "TEST(DatabaseCApi, AddTimeSeriesRow" tests/test_c_api_database_time_series.cpp` → **12** (expected ≥ 12)
- `grep -c "Cannot add_time_series_row:" tests/test_c_api_database_time_series.cpp` → **6** (expected ≥ 4)
- `grep -c "multi_dim_time_series.sql" tests/test_c_api_database_time_series.cpp` → **8** (expected ≥ 3)
- `grep -c "quiver_database_add_time_series_row" tests/test_c_api_database_time_series.cpp` → **23** (expected ≥ 5; counts include comment + 22 call sites across 12 TESTs)
- `grep -c "Null argument" tests/test_c_api_database_time_series.cpp` → **5** (baseline 3 from UpdateTimeSeriesGroup* error tests + 2 new from TEST 10 / TEST 12)
- `grep -c "quiver_database_begin_transaction\|quiver_database_commit\|quiver_database_rollback\|quiver_database_in_transaction" tests/test_c_api_database_time_series.cpp` → **9** (all four primitives exercised in TEST 9's three phases)

**Filtered test run (`*AddTimeSeriesRow*`):**
```
[==========] Running 12 tests from 1 test suite.
[       OK ] DatabaseCApi.AddTimeSeriesRowInsert (8 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowUpsertSamePK (5 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowMultiDimInsert (7 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowMultiDimUpsert (4 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowPartialValueColumns (8 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowMissingDimension (4 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowUnknownColumn (5 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowTypeMismatch (4 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowTransactionMatrix (9 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowNullArguments (6 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowUnknownColumnType (4 ms)
[       OK ] DatabaseCApi.AddTimeSeriesRowNullColumnArraysWithCount (7 ms)
[==========] 12 tests from 1 test suite ran. (75 ms total)
[  PASSED  ] 12 tests.
```
**12 PASSED, 0 FAILED.**

**Full suite (no filter):**
```
[==========] 483 tests from 11 test suites ran. (6516 ms total)
[  PASSED  ] 483 tests.
```
**Exit 0, no regressions.** Baseline before this plan: 471 tests; delta exactly +12 matches the added test count.

**Mapping new tests → D-09 CAPI-13 sub-scenarios:**

C++ mirror (9):
| Scenario | Test | TEST # |
|----------|------|--------|
| simple insert | `AddTimeSeriesRowInsert` | TEST 1 |
| single-PK upsert | `AddTimeSeriesRowUpsertSamePK` | TEST 2 |
| multi-dim insert | `AddTimeSeriesRowMultiDimInsert` | TEST 3 |
| multi-dim upsert | `AddTimeSeriesRowMultiDimUpsert` | TEST 4 |
| partial value columns | `AddTimeSeriesRowPartialValueColumns` | TEST 5 |
| missing-dim error | `AddTimeSeriesRowMissingDimension` | TEST 6 |
| unknown-column error | `AddTimeSeriesRowUnknownColumn` | TEST 7 |
| type-mismatch error | `AddTimeSeriesRowTypeMismatch` | TEST 8 |
| transaction matrix | `AddTimeSeriesRowTransactionMatrix` (three phases) | TEST 9 |

FFI-boundary additions (3):
| Scenario | Test | TEST # |
|----------|------|--------|
| null pointer arguments | `AddTimeSeriesRowNullArguments` | TEST 10 |
| unknown `column_types[c]` integer (D-07) | `AddTimeSeriesRowUnknownColumnType` | TEST 11 |
| null column arrays with `column_count` > 0 | `AddTimeSeriesRowNullColumnArraysWithCount` | TEST 12 |

Per D-10, `AddTimeSeriesRowDateTimeStringInterchangeable` was intentionally skipped — `UpdateTimeSeriesGroupDateTimeStringInterchangeable` (line 796) already covers the shared marshaling branch.

## Decisions Made

Followed the plan as written. Discretion calls applied:

- **Helper extraction skipped (CONTEXT.md discretion).** Did NOT introduce a `capture_add_row_error` analog. Used the existing inline assertion idiom (`std::string msg = quiver_get_last_error(); EXPECT_NE(msg.find(...), std::string::npos) << "Actual: " << msg;`) that the seven `UpdateTimeSeriesGroup*` error tests already use. This keeps assertion text co-located with the call site and avoids one-off helper churn for a single test file.
- **TEST 5 (PartialValueColumns) verification scope.** The plan suggested optionally supplementing with `read_time_series_row` for the FLOAT NULL case. Skipped that supplement because (a) the multi-dim PK `(id, date_time, block)` means `read_time_series_row(..., date_time)` cannot disambiguate which block to read, and (b) the round-trip via `read_time_series_group` already confirms partial inserts succeed and the supplied columns persist verbatim. Documented the C API NULL → 0 / NULL → 0.0 behavior inline as part of the test assertions.
- **Per-row scoped braces** around buffer arrays in TEST 3 (multi-dim insert loop), TEST 4 (three sequential upserts), TEST 5 (two partial inserts), and TEST 9 (three transaction phases). Avoids unused-variable diagnostics and keeps each insert's typed buffers visually isolated.

## Deviations from Plan

None — plan executed exactly as written. No Rule 1/2/3/4 deviations occurred. All canonical Pattern-1 error substrings asserted in TEST 6/7/8 match the strings emitted by `src/database_time_series.cpp::Database::add_time_series_row` verbatim (per D-05).

## Issues Encountered

None. CMake configuration + build worked first try; the test binary built cleanly with no new warnings. All 12 new tests passed on the first run.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

**CAPI-11..13 verified — Phase 3 (language bindings) unblocked.** The C API symbol `quiver_database_add_time_series_row` is exposed in `include/quiver/c/database.h` (Plan 02-01, D-11) and now has a 12-scenario regression net at the FFI boundary. Phase 3 binding generators (Julia, Dart, Python, Lua) can marshal against this contract with the assurance that:

- The columnar typed-arrays input shape (`column_names[]` / `column_types[]` / `column_data[]` / `column_count`) is exercised across `INTEGER`, `FLOAT`, `STRING`, and `DATE_TIME` types.
- Upsert semantics on the full composite PK are locked.
- All canonical Pattern-1 error messages reach the caller verbatim via `quiver_get_last_error`.
- The D-07 wrapper-owned `"Cannot add_time_series_row: unknown column type N"` default-branch error is verified end-to-end through the C ABI.
- Transaction nesting works through the C handle (rollback discards, commit persists, standalone autocommits).

## Self-Check: PASSED

- `tests/test_c_api_database_time_series.cpp` exists and contains 12 new `TEST(DatabaseCApi, AddTimeSeriesRow*)` cases (verified via grep count = 12).
- Commit `14e0d41` exists in `git log` (Task 1).
- Commit `d9aa404` exists in `git log` (Task 2).
- `cmake --build build --config Debug` succeeded with no new warnings.
- `./build/bin/quiver_c_tests.exe --gtest_filter="DatabaseCApi.AddTimeSeriesRow*"` reports `[  PASSED  ] 12 tests`, exits 0.
- `./build/bin/quiver_c_tests.exe` reports `[  PASSED  ] 483 tests`, exits 0 (baseline 471 + delta 12 matches).

---
*Phase: 02-c-api-quiver-database-add-time-series-row*
*Completed: 2026-05-19*
