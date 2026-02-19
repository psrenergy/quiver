---
phase: 11-c-api-multi-column-time-series
plan: 01
subsystem: api
tags: [c-api, ffi, time-series, columnar, typed-arrays]

# Dependency graph
requires: []
provides:
  - Multi-column C API signatures for update_time_series_group, read_time_series_group, free_time_series_data
  - Schema validation with dimension column presence check and type mismatch detection
  - Mixed-type test schema (INTEGER + REAL + TEXT) at tests/schemas/valid/mixed_time_series.sql
  - QUIVER_REQUIRE macro extended to 9 arguments
affects: [12-julia-multi-column-time-series, 13-dart-multi-column-time-series]

# Tech tracking
tech-stack:
  added: []
  patterns: [columnar typed-array FFI pattern, type-aware deallocation, schema validation with metadata lookup]

key-files:
  created:
    - tests/schemas/valid/mixed_time_series.sql
  modified:
    - include/quiver/c/database.h
    - src/c/database_time_series.cpp
    - src/c/internal.h
    - tests/test_c_api_database_time_series.cpp
    - tests/test_c_api_database_create.cpp
    - tests/test_c_api_database_update.cpp

key-decisions:
  - "Split QUIVER_REQUIRE into two calls for 9-arg read function instead of using QUIVER_REQUIRE_9, for readability"
  - "Dimension column type stored as QUIVER_DATA_TYPE_STRING in schema lookup (since it maps to TEXT in SQLite)"
  - "c_type_name() helper is file-local static in database_time_series.cpp, not in database_helpers.h"

patterns-established:
  - "Columnar typed-array C API: parallel column_names[], column_types[], column_data[] with size_t column_count + row_count"
  - "Type-aware deallocation: free function takes column_types[] to dispatch delete[] vs per-element delete[]"
  - "Schema validation before conversion: metadata lookup, dimension check, per-column name/type validation"

requirements-completed: [CAPI-01, CAPI-02, CAPI-03, CAPI-04, CAPI-05, MIGR-01]

# Metrics
duration: 5min
completed: 2026-02-19
---

# Phase 11 Plan 01: C API Multi-Column Time Series Summary

**Columnar typed-array C API for N-column time series with schema validation, type-aware deallocation, and mixed-type test schema**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-19T22:37:44Z
- **Completed:** 2026-02-19T22:43:13Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Replaced single-column (date_times, values) C API time series functions with multi-column typed-array signatures supporting N typed value columns
- Implemented schema validation that checks dimension column presence, column name existence, and column type matches against metadata
- Created mixed_time_series.sql test schema with INTEGER (humidity), REAL (temperature), and TEXT (status) value columns
- Extended QUIVER_REQUIRE macro to support 7, 8, and 9 argument variants
- Updated all C API test callers (time series, create, update) to the new multi-column signatures; all 19 tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Create mixed-type test schema and extend QUIVER_REQUIRE macro** - `0b4d1d9` (feat)
2. **Task 2: Replace C API time series header declarations and implement multi-column update/read/free** - `f2edcfa` (feat)

## Files Created/Modified
- `tests/schemas/valid/mixed_time_series.sql` - Mixed-type time series schema with Sensor_time_series_readings (REAL temperature, INTEGER humidity, TEXT status)
- `src/c/internal.h` - Extended QUIVER_REQUIRE macro to support 7-9 arguments
- `include/quiver/c/database.h` - New multi-column signatures for read/update/free time series functions
- `src/c/database_time_series.cpp` - Full implementation: columnar-to-row conversion, row-to-columnar conversion, schema validation, type-aware free, c_type_name() helper
- `tests/test_c_api_database_time_series.cpp` - Updated all read/update/clear/null-arg tests to new multi-column signatures
- `tests/test_c_api_database_create.cpp` - Updated CreateElementWithTimeSeries and CreateElementWithMultiTimeSeries tests
- `tests/test_c_api_database_update.cpp` - Updated UpdateElementWithTimeSeries test

## Decisions Made
- Split QUIVER_REQUIRE into two calls for the read function (5 + 3 args) instead of using a single QUIVER_REQUIRE_9, for readability
- Placed c_type_name() as file-local static in database_time_series.cpp rather than in database_helpers.h (only used in this file)
- Dimension column stored as QUIVER_DATA_TYPE_STRING in the schema lookup map (TEXT in SQLite, STRING in C API enum)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated C API test callers to new signatures**
- **Found during:** Task 2 (build verification)
- **Issue:** Three test files (test_c_api_database_time_series.cpp, test_c_api_database_create.cpp, test_c_api_database_update.cpp) used old single-column function signatures, causing compilation failure
- **Fix:** Converted all test call sites to use the new multi-column parallel-array signatures with proper columnar data setup and result extraction
- **Files modified:** tests/test_c_api_database_time_series.cpp, tests/test_c_api_database_create.cpp, tests/test_c_api_database_update.cpp
- **Verification:** Build succeeds, all 19 C API time series tests pass
- **Committed in:** f2edcfa (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Test file updates were necessary for compilation. No scope creep -- these callers are part of the C API layer being modified.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- C API multi-column time series signatures are complete and tested
- Julia bindings (Phase 12) and Dart bindings (Phase 13) need updating to new signatures
- Binding generators need re-running in those phases to pick up header changes
- mixed_time_series.sql schema available for multi-type integration testing

## Self-Check: PASSED

All 8 files verified present. Both task commits (0b4d1d9, f2edcfa) verified in git log.

---
*Phase: 11-c-api-multi-column-time-series*
*Completed: 2026-02-19*
