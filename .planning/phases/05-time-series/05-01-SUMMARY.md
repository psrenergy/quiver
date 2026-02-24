---
phase: 05-time-series
plan: 01
subsystem: database
tags: [python, cffi, time-series, void-pointer, columnar-marshaling]

# Dependency graph
requires:
  - phase: 04-queries-and-relations
    provides: _marshal_params void** pattern, CFFI ABI-mode cdef conventions
  - phase: 02-reads-and-metadata
    provides: get_time_series_metadata, _parse_group_metadata, GroupMetadata
provides:
  - CFFI declarations for all 10 time series C API functions
  - read_time_series_group method with void** columnar dispatch
  - update_time_series_group method with columnar typed-array marshaling
  - _marshal_time_series_columns helper with strict type validation
  - mixed_time_series_db test fixture
affects: [05-02-time-series-files]

# Tech tracking
tech-stack:
  added: []
  patterns: [columnar-void-pointer-dispatch, row-to-column-transposition, strict-type-validation]

key-files:
  created:
    - bindings/python/tests/test_database_time_series.py
  modified:
    - bindings/python/src/quiver/_c_api.py
    - bindings/python/src/quiver/database.py
    - bindings/python/tests/conftest.py

key-decisions:
  - "DATE_TIME columns return plain str (no auto-parsing) -- consistent with Phase 4 query_string pattern"
  - "Parameter order follows C++/C API: (collection, group, id) not (collection, id, group)"
  - "Strict type validation: type(v) is int rejects bool subclass; no coercion"
  - "Dimension column type hardcoded to STRING (2) since dimension is always TEXT in SQL"

patterns-established:
  - "_marshal_time_series_columns: row-to-column transposition with keepalive and strict type validation"
  - "void** columnar read dispatch: cast by column_types[c] index, transpose to list[dict]"

requirements-completed: [TS-01, TS-02]

# Metrics
duration: 3min
completed: 2026-02-23
---

# Phase 5 Plan 1: Time Series Group Read/Write Summary

**Python CFFI bindings for multi-column time series read/write with void** columnar dispatch, strict type validation, and row-dict transposition**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-24T02:37:24Z
- **Completed:** 2026-02-24T02:40:58Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- CFFI declarations for all 10 time series C API functions (group read/update/free + files operations)
- read_time_series_group transposes columnar void** data to list[dict] with correct Python types per column
- update_time_series_group marshals list[dict] to columnar typed arrays with keepalive pattern
- Strict type validation rejects wrong types including bool-as-int (type(v) is int, not isinstance)
- 13 tests covering read empty/single/multi/types, write round-trip/clear/overwrite, validation, and single-column

## Task Commits

Each task was committed atomically:

1. **Task 1: Add CFFI declarations and implement read/update time series group** - `8809636` (feat)
2. **Task 2: Add test fixtures and time series group tests** - `9bd13b7` (test)

## Files Created/Modified
- `bindings/python/src/quiver/_c_api.py` - Added CFFI cdef for 10 time series C API functions
- `bindings/python/src/quiver/database.py` - Added read_time_series_group, update_time_series_group, _marshal_time_series_columns
- `bindings/python/tests/conftest.py` - Added mixed_time_series_db and mixed_time_series_schema_path fixtures
- `bindings/python/tests/test_database_time_series.py` - 13 tests for time series group read/write/validation

## Decisions Made
- DATE_TIME columns return plain `str` (no auto-parsing to datetime) -- consistent with Phase 4 `query_string` pattern where `query_date_time` is a separate convenience wrapper
- Parameter order `(collection, group, id)` follows C++/C API order, matching Julia/Dart bindings
- `type(v) is int` used instead of `isinstance(v, int)` to reject `bool` values for INTEGER columns
- Dimension column type hardcoded to STRING (2) in _marshal_time_series_columns since SQL dimension columns are always TEXT

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed Element API usage in tests**
- **Found during:** Task 2 (test creation)
- **Issue:** Tests used `elem.set_string("label", ...)` but Python Element class uses fluent `elem.set("label", ...)` API
- **Fix:** Changed to `elem.set("label", label)` in test helpers
- **Files modified:** bindings/python/tests/test_database_time_series.py
- **Verification:** All 13 tests pass
- **Committed in:** 9bd13b7 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Trivial test code fix. No scope creep.

## Issues Encountered
None -- both the CFFI declarations and implementation worked on first try.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Time series group read/write complete, ready for Plan 02 (time series files operations)
- All 10 CFFI declarations already in place (files operations can be bound immediately)
- mixed_time_series_db fixture available for any additional tests

## Self-Check: PASSED

- All 5 files verified present on disk
- Commit 8809636 (Task 1) verified in git log
- Commit 9bd13b7 (Task 2) verified in git log
- All 146 Python tests pass (including 13 new time series tests)

---
*Phase: 05-time-series*
*Completed: 2026-02-23*
