---
phase: 01-c-core-add-time-series-row
plan: 01
subsystem: database
tags: [sqlite, time-series, upsert, c++20]

requires:
  - phase: (none — this is the first plan of the first phase of milestone v1.1)
    provides: existing update_time_series_group / read_time_series_row primitives
provides:
  - C++ core Database::add_time_series_row(collection, group, id, row) single-row upsert
  - INSERT OR REPLACE-based PK-keyed upsert semantic for time series rows
  - Multi-dimension time series test schema (Resource_time_series_load with PK = id + date_time + block)
affects:
  - 01-02 (C++ tests for add_time_series_row will consume the new schema)
  - phase 02 (C API wrapper for add_time_series_row will mirror this signature)
  - phase 03 (Julia/Dart/Lua/Python bindings will wrap the C API)

tech-stack:
  added: []
  patterns:
    - "Single-row time series upsert via INSERT OR REPLACE INTO keyed on the schema PK"
    - "Derive dimension columns from TableDefinition::column_order filtered by ColumnDefinition::primary_key (no new helper added)"

key-files:
  created:
    - tests/schemas/valid/multi_dim_time_series.sql
  modified:
    - include/quiver/database.h
    - src/database_time_series.cpp

key-decisions:
  - "INSERT OR REPLACE INTO chosen over INSERT ... ON CONFLICT DO UPDATE: REPLACE matches the upsert semantic CORE-11 requires (omitted value columns reset to NULL/DEFAULT on the replaced row), is shorter, and mirrors the SQL style already used for time-series writes in the codebase"
  - "Dimension columns derived locally from TableDefinition::column_order + ColumnDefinition::primary_key — no new helper added; find_dimension_column only returns the date-typed dim, which is insufficient for composite-PK schemas"
  - "Column iteration in INSERT SQL uses std::map iteration order (lexicographic by column name), matching update_time_series_group's pattern; SQL is independent of column order in INSERT lists"

patterns-established:
  - "Pattern: single-row time series upsert keyed on schema PK columns (id + every PK column except id)"
  - "Pattern: Pattern-1 error messages 'Cannot add_time_series_row: ...' for precondition failures (missing dim col, unknown col, type mismatch)"

requirements-completed: [CORE-11, CORE-12, CORE-13]

duration: 6min
completed: 2026-05-19
---

# Phase 01 Plan 01: C++ core add_time_series_row Summary

**Database::add_time_series_row single-row upsert primitive (INSERT OR REPLACE keyed on schema PK), plus a multi-dimension PK test schema (id, date_time, block) for downstream test coverage.**

## Performance

- **Duration:** ~6 min
- **Started:** 2026-05-19T21:48:30Z
- **Completed:** 2026-05-19T21:54:07Z
- **Tasks:** 3
- **Files modified:** 3 (1 created, 2 modified)

## Accomplishments
- `Database::add_time_series_row(collection, group, id, row)` declared and implemented on the public C++ API, sitting between `update_time_series_group` and `has_time_series_files`
- INSERT OR REPLACE INTO upsert semantic: calling twice with the same PK overwrites value columns (CORE-11)
- Full schema-derived validation: every dimension PK column required, unknown columns rejected, type mismatches rejected — all via canonical Pattern 1 `"Cannot add_time_series_row: ..."` messages (CORE-12)
- Wrapped in `Impl::TransactionGuard` so the new method composes cleanly with explicit `begin_transaction` blocks (CORE-13)
- New `multi_dim_time_series.sql` schema with composite PK `(id, date_time, block)` and nullable value columns ready for Plan 02's tests
- Full quiver_tests suite (844 tests) passes with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Add multi-dimension PK time series test schema** — `b90c9ae` (feat)
2. **Task 2: Declare add_time_series_row in the public Database header** — `0918fc7` (feat)
3. **Task 3: Implement Database::add_time_series_row with upsert and TransactionGuard** — `bbf1f8b` (feat)

## Files Created/Modified
- `tests/schemas/valid/multi_dim_time_series.sql` (NEW) — Resource collection + Resource_time_series_load with composite PK (id, date_time, block) and nullable value columns (load REAL, flag INTEGER)
- `include/quiver/database.h` (modified) — Added 11-line declaration block for `add_time_series_row` between `update_time_series_group` and `has_time_series_files`
- `src/database_time_series.cpp` (modified) — Added 80-line `Database::add_time_series_row` implementation between `update_time_series_group` and `read_time_series_row`

## Decisions Made

- **`TableDefinition` PK derivation:** `TableDefinition` does NOT expose PK columns as a dedicated `primary_key_columns` vector. The plan's fallback was selected: iterate `table_def->column_order` and filter by `ColumnDefinition::primary_key && name != "id"`. This preserves PK declaration order (used in the SQL INSERT column list) without introducing a new helper. `find_dimension_column` was not used because it only returns the single date-typed dim, which is insufficient for the composite-PK schema introduced in Task 1.
- **`INSERT OR REPLACE INTO` retained verbatim:** No deviation. REPLACE deletes the conflicting row and inserts the new one; any value column omitted by the caller falls back to its column DEFAULT (NULL for the nullable value columns in `multi_dim_time_series.sql`). This is exactly the upsert semantic CORE-11 requires.

## Deviations from Plan

None — plan executed exactly as written. The plan correctly anticipated the `TableDefinition` PK-derivation fallback path, and all three Pattern-1 error sites and the INSERT OR REPLACE path matched the spec.

## Issues Encountered

None during planned work. The worktree base check expected `f44f81c` (milestone-v1.1 planning tip) but the worktree was spawned at `294a516` (master branch tip after newer expression work landed). The pre-step `<worktree_branch_check>` correction (`git reset --hard f44f81c`) succeeded and the build configured + baseline-built clean against that ref. No code changes were required as a result.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- C++ API surface is complete and verified: declaration in `include/quiver/database.h`, implementation in `src/database_time_series.cpp`, all error messages Pattern 1, transaction-nest-aware.
- `multi_dim_time_series.sql` is ready for Plan 02 to exercise composite-PK upsert paths (Plan 02 will write the test suite for the new method, including: single-dim PK insert, single-dim PK overwrite, composite-PK insert, composite-PK overwrite, validation error paths, transaction nesting).
- C API (phase 02) can mirror the signature directly: `quiver_database_add_time_series_row(db, collection, group, id, column_names, column_types, column_values, column_count, ...)` following the existing `update_time_series_group` columnar marshaling pattern.

## TDD Gate Compliance

This is a plan with `type: execute` (not `type: tdd`), and tasks are `tdd="false"`. Per Plan 02's scope, the test suite for `add_time_series_row` lands separately — Plan 01 ships the implementation against the pre-existing zero-regression baseline (844 tests still pass).

---
*Phase: 01-c-core-add-time-series-row*
*Completed: 2026-05-19*

## Self-Check: PASSED

- File `tests/schemas/valid/multi_dim_time_series.sql` — FOUND
- File `include/quiver/database.h` (modified) — FOUND
- File `src/database_time_series.cpp` (modified) — FOUND
- Commit `b90c9ae` (Task 1) — FOUND
- Commit `0918fc7` (Task 2) — FOUND
- Commit `bbf1f8b` (Task 3) — FOUND
- All three acceptance-criteria grep counts pass (see Task verification logs above)
- `./build/bin/quiver_tests.exe` exit code 0, 844 tests passed, 0 regressions
