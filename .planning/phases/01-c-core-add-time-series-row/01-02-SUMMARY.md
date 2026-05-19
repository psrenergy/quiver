---
phase: 01-c-core-add-time-series-row
plan: 02
subsystem: database
tags: [tests, time-series, gtest, c++20]

requires:
  - phase: 01, plan: 01 — ships Database::add_time_series_row + multi_dim_time_series.sql
provides:
  - GTest coverage for Database::add_time_series_row across all CORE-11..14 scenarios
  - capture_add_row_error helper for asserting Pattern-1 errors on the new method
affects:
  - phase 02 (C API wrapper test mirroring will reference the same scenario list)
  - phase 03 (Julia/Dart/Lua/Python binding tests will mirror these 9 scenarios)

tech-stack:
  added: []
  patterns:
    - "Sibling capture helper (capture_add_row_error) mirrors the existing capture_update_error idiom for assertion ergonomics"
    - "Multi-dim read assertions use std::sort by (date_time, block) before indexed access — read_time_series_group only orders by dim_col"

key-files:
  created: []
  modified:
    - tests/test_database_time_series.cpp

key-decisions:
  - "Single combined TEST(AddTimeSeriesRowErrors) with 5 lambda-scoped sub-cases instead of 5 separate TESTs — keeps the schema setup boilerplate to one block and groups all canonical 'Cannot add_time_series_row: ...' assertions together, matching the planner-intended shape"
  - "Multi-dim assertions sort by (date_time, block) in the test rather than relying on read order — read_time_series_group only ORDER BYs the dim_col so block ordering within a date_time is undefined"
  - "Phase A+B combined into one transaction TEST to prove TransactionGuard nest-detection on both sides of the same db handle (explicit-commit then autocommit)"

requirements-completed: [CORE-14]

metrics:
  duration: 4min
  completed: 2026-05-19
---

# Phase 01 Plan 02: GTest coverage for add_time_series_row Summary

**Nine new TEST cases plus a capture_add_row_error helper exercise every CORE-11..14 scenario for Database::add_time_series_row — insert, upsert (single + composite PK), partial value columns, every Pattern-1 error path, and the full transaction-nesting matrix (rollback / explicit-commit / standalone autocommit).**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-05-19T22:10:58Z
- **Completed:** 2026-05-19T22:15:02Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- 9 new `TEST(Database, AddTimeSeriesRow*)` cases appended to `tests/test_database_time_series.cpp`, all passing via the targeted gtest filter
- `capture_add_row_error` sibling helper placed next to the existing `capture_update_error`, keeping error-assertion ergonomics consistent across the two write APIs
- Multi-dim schema `multi_dim_time_series.sql` exercised by 4 of the 9 tests (insert, upsert, partial columns, errors) — full composite-PK coverage
- All five canonical Pattern-1 error sites covered (missing date_time, missing block, unknown column, REAL←INTEGER mismatch, INTEGER←REAL mismatch)
- Transaction nesting verified end-to-end: rollback discards work, explicit commit persists batched writes, standalone call autocommits — the same TransactionGuard composes cleanly across all three paths
- Full `quiver_tests.exe` suite: 853/853 PASS (was 844 baseline, +9 net new tests, 0 regressions)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add insert, upsert, and multi-dim happy-path tests** — `502a5e8` (test)
2. **Task 2: Add error-path and transaction-nesting tests** — `667c1f1` (test)

## Files Created/Modified

- `tests/test_database_time_series.cpp` (modified, +417 lines net across 2 commits):
  - +6 happy-path TESTs after the existing time-series-files block
  - +`capture_add_row_error` helper directly after `capture_update_error`
  - +3 error/transaction TESTs at end of file

## Decisions Made

- **Combined error-path TEST:** The plan listed sub-cases a-e under a single `AddTimeSeriesRowErrors` test. Followed that shape rather than splitting into 5 separate TESTs — it groups all `Cannot add_time_series_row: ...` assertions in one place and reuses one schema setup, matching the planner-intended structure and keeping the file readable.
- **Sort before assert for multi-dim reads:** `read_time_series_group` SQL is `ORDER BY date_time` only — block order within a date_time is undefined by SQLite. Tests with multi-dim PK sort the returned vector by `(date_time, block)` before indexed access. This is a stability decision the plan called out explicitly in TEST 4.
- **`int64_t{...}` literal disambiguation:** GTest's variant assertions and the `Value` constructor are sensitive to integer literal type — used `int64_t{1}` rather than plain `1` for all `block` / `flag` values so `Value` resolves to the `int64_t` arm rather than ambiguous-integer overload, matching the existing test style.

## Deviations from Plan

None — plan executed exactly as written. The implementation produced the exact error strings the plan predicted (verified against `src/database_time_series.cpp`), so no adjustments to assertion substrings were needed. The 5 acceptance-criteria grep counts (TEST count ≥6+3, capture_add_row_error ≥2, multi_dim_time_series.sql ≥2, 'Cannot add_time_series_row:' ≥5, filtered suite reports 9 PASSED) all pass on the first build.

## Issues Encountered

- **Initial worktree base:** The worktree was spawned with `master` as its base (`294a516`), not the planning branch tip `853901d`. The pre-step `<worktree_branch_check>` correction (`git reset --hard 853901d`) succeeded; planning files were then available and Plan 01's implementation was already on the branch. No code changes were required from this.
- **CMake configure cost:** First `cmake -B build` took ~160s on this worktree (cold cache, fetches third-party deps). Subsequent `cmake --build` calls were sub-3s for the single `test_database_time_series.cpp` translation unit.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

**CORE-11..14 verified — Phase 2 (C API wrapper) unblocked.**

- All four CORE-11..14 requirements have executable C++ verification: insert (CORE-11), upsert + multi-dim PK upsert (CORE-11), partial value columns + every error path (CORE-12), transaction nesting (CORE-13), full test coverage (CORE-14).
- Phase 2 can mirror these 9 scenarios in `tests/test_c_api_database_time_series.cpp` using the columnar typed-arrays marshaling pattern from `quiver_database_update_time_series_group`.
- Phase 3 binding tests (Julia / Dart / Lua / Python) can mirror the same scenario list, with each language wrapping `quiver_database_add_time_series_row` via its existing FFI conventions.
- No code or schema gaps remain in the C++ core for this milestone.

## TDD Gate Compliance

This plan has `type: execute` with `tdd="false"` tasks. The implementation under test (`Database::add_time_series_row`) was shipped in Plan 01; this plan ships the test suite that locks the contract. The full `quiver_tests.exe` suite passes (853/853), confirming the implementation matches the test expectations and that no regressions were introduced.

---
*Phase: 01-c-core-add-time-series-row*
*Completed: 2026-05-19*

## Self-Check: PASSED

- File `tests/test_database_time_series.cpp` (modified) — FOUND
- Commit `502a5e8` (Task 1: happy-path tests) — FOUND
- Commit `667c1f1` (Task 2: errors + transaction tests) — FOUND
- `grep -c "TEST(Database, AddTimeSeriesRow" tests/test_database_time_series.cpp` = 9 (≥9) — PASS
- `grep -c "capture_add_row_error" tests/test_database_time_series.cpp` = 6 (≥2) — PASS
- `grep -c "multi_dim_time_series.sql" tests/test_database_time_series.cpp` = 4 (≥2/≥3) — PASS
- `grep -c "Cannot add_time_series_row:" tests/test_database_time_series.cpp` = 5 (≥5) — PASS
- `./build/bin/quiver_tests.exe --gtest_filter="Database.AddTimeSeriesRow*"` reports [  PASSED  ] 9 tests, exit 0 — PASS
- `./build/bin/quiver_tests.exe` (full suite) reports [  PASSED  ] 853 tests, exit 0 — PASS
