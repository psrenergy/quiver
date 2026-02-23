---
phase: 01-foundation
plan: 01
subsystem: database
tags: [c++, sqlite, foreign-key, label-resolution, refactor]

# Dependency graph
requires: []
provides:
  - "resolve_fk_label helper method on Database::Impl for FK label-to-ID resolution"
  - "Non-FK INTEGER column guard preventing raw SQLite STRICT mode errors"
  - "Child_set_mentors and Child_set_scores test tables in relations.sql"
affects: [02-create, 03-update]

# Tech tracking
tech-stack:
  added: []
  patterns: ["resolve_fk_label helper on Impl called from write paths with Database& reference for execute()"]

key-files:
  created: []
  modified:
    - src/database_impl.h
    - src/database_create.cpp
    - tests/test_database_relations.cpp
    - tests/schemas/valid/relations.sql

key-decisions:
  - "resolve_fk_label placed on Database::Impl (not Database) since it needs schema access and is an internal helper"
  - "Method takes Database& reference to call private execute() for FK lookup queries"
  - "Used Child_set_mentors with unique mentor_id column to avoid routing conflict with shared parent_ref column"

patterns-established:
  - "FK label resolution: resolve_fk_label(table_def, column, value, db) pattern for all write paths"
  - "Non-FK INTEGER guard: clear Quiver error before SQLite STRICT mode catches type mismatch"

requirements-completed: [FOUND-01, CRE-03, ERR-01]

# Metrics
duration: 11min
completed: 2026-02-23
---

# Phase 1 Plan 1: FK Label Resolution Foundation Summary

**Shared resolve_fk_label helper on Database::Impl that resolves string labels to integer FK IDs, with non-FK INTEGER guard and refactored set path in create_element**

## Performance

- **Duration:** 11 min
- **Started:** 2026-02-23T19:23:29Z
- **Completed:** 2026-02-23T19:35:02Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Extracted reusable `resolve_fk_label` method on `Database::Impl` handling all 4 value variants (FK string->int, non-FK INTEGER string->error, TEXT/DATETIME string->passthrough, non-string->passthrough)
- Refactored 17-line inline FK resolution block in set path of `create_element` to single helper call with zero behavioral change
- Added 3 dedicated tests proving FK label resolution, missing label error (ERR-01), and non-FK INTEGER column guard
- All 445 tests pass (442 existing + 3 new)

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract resolve_fk_label helper and refactor set FK path** - `44f4134` (feat)
2. **Task 2: Add dedicated FK label resolution tests** - `f6ae6f4` (test)

## Files Created/Modified
- `src/database_impl.h` - Added `resolve_fk_label` method to `Database::Impl` struct
- `src/database_create.cpp` - Replaced inline FK block in set path with single helper call
- `tests/test_database_relations.cpp` - Added 3 new tests under "FK label resolution" section
- `tests/schemas/valid/relations.sql` - Added `Child_set_mentors` (FK) and `Child_set_scores` (non-FK INTEGER) set tables

## Decisions Made
- Placed `resolve_fk_label` on `Database::Impl` rather than as a free function, since it needs schema access through `table_def` and database access through `db.execute()`
- Used `Database&` parameter (not `Impl&`) because `execute()` is a private method on `Database`, and `Impl` as a nested struct has access to call it
- Created `Child_set_mentors` table with unique `mentor_id` FK column instead of using existing `parent_ref` which exists in both vector and set tables (would cause dual-routing through `find_all_tables_for_column`)
- Added `Child_set_scores` table with non-FK INTEGER column `score` for testing the guard path

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added Child_set_mentors table with unique FK column**
- **Found during:** Task 2 (FK label resolution tests)
- **Issue:** Plan specified using `parent_ref` column for Test 1 and Test 2, but `parent_ref` exists in both `Child_vector_refs` and `Child_set_parents`. The `find_all_tables_for_column` router sends values to both tables, causing string values to reach the vector path (which lacks resolve_fk_label), producing a raw SQLite STRICT mode error instead of testing the set path's FK resolution.
- **Fix:** Added `Child_set_mentors` table with uniquely-named `mentor_id` FK column. Tests use `mentor_id` to route exclusively through the set path.
- **Files modified:** tests/schemas/valid/relations.sql, tests/test_database_relations.cpp
- **Verification:** All 3 new tests pass; all 442 existing tests unchanged.
- **Committed in:** f6ae6f4 (Task 2 commit)

**2. [Rule 3 - Blocking] Adapted Test 3 to use set path instead of scalar path**
- **Found during:** Task 2 (FK label resolution tests)
- **Issue:** Plan specified using `basic.sql` with scalar `integer_attribute`, but `resolve_fk_label` is only called from the set path in this phase. The scalar path does not call the helper, so a string scalar on a non-FK INTEGER column would produce a raw SQLite STRICT mode error rather than testing `resolve_fk_label`'s guard.
- **Fix:** Test 3 uses `relations.sql` with `Child_set_scores.score` (non-FK INTEGER set column) to exercise `resolve_fk_label`'s non-FK INTEGER guard through the set path.
- **Files modified:** tests/test_database_relations.cpp
- **Verification:** Test throws `std::runtime_error` from `resolve_fk_label`, not from SQLite.
- **Committed in:** f6ae6f4 (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both deviations necessary to correctly test `resolve_fk_label` through the only code path that calls it (set insertion). No scope creep. Tests verify the same behaviors specified in the plan.

## Issues Encountered
- CMake build directory did not exist; ran `cmake -B build` to configure before building. Not a regression, just environment setup.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `resolve_fk_label` is ready for Phase 2 (create) to call from scalar and vector paths
- Method interface `(TableDefinition&, string&, Value&, Database&)` works for any table type without modification
- ERR-01 error format established: `"Failed to resolve label 'X' to ID in table 'Y'"`

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 01-foundation*
*Completed: 2026-02-23*
