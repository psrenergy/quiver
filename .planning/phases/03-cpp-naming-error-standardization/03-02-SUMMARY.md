---
phase: 03-cpp-naming-error-standardization
plan: 02
subsystem: api
tags: [c++, error-handling, standardization, error-messages]

# Dependency graph
requires:
  - phase: 03-cpp-naming-error-standardization
    plan: 01
    provides: "Uniform verb_noun naming across all 60 Database public methods"
provides:
  - "3 canonical error message patterns across all C++ throw sites"
  - "require_collection validation on all operations with collection parameter"
  - "Documented naming convention and error patterns in CLAUDE.md"
affects: [05-c-api-consistency, 08-lua-binding-consistency]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Error Pattern 1: 'Cannot {operation}: {reason}' for precondition failures"
    - "Error Pattern 2: '{Entity} not found: {identifier}' for entity lookups"
    - "Error Pattern 3: 'Failed to {operation}: {reason}' for operation failures"
    - "require_collection used for all ops with collection param (except list_*_groups)"

key-files:
  created: []
  modified:
    - src/database_impl.h
    - src/database.cpp
    - src/database_create.cpp
    - src/database_read.cpp
    - src/database_update.cpp
    - src/database_delete.cpp
    - src/database_metadata.cpp
    - src/database_time_series.cpp
    - src/database_relations.cpp
    - src/database_internal.h
    - CLAUDE.md

key-decisions:
  - "list_*_groups functions use require_schema (not require_collection) because nonexistent collections should return empty lists, not throw"
  - "Operation names in require_collection/require_schema match actual method names (e.g., 'read_vector_integers' not 'read vector')"
  - "Upgraded require_schema to require_collection in vector/set/time series update operations (missing critical validation)"

patterns-established:
  - "Error message format: all C++ throws use exactly 3 patterns (Cannot/Not found/Failed to)"
  - "require_collection operation names match method names with underscores"

# Metrics
duration: 22min
completed: 2026-02-10
---

# Phase 3 Plan 2: C++ Error Message Standardization Summary

**Standardized ~50 throw sites across 10 C++ source files to 3 canonical error patterns, upgraded vector/set/time series validation to require_collection, documented conventions in CLAUDE.md**

## Performance

- **Duration:** 22 min
- **Started:** 2026-02-10T03:32:53Z
- **Completed:** 2026-02-10T03:54:29Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments
- All ~50 throw sites in C++ source follow exactly 3 message patterns (Cannot/Not found/Failed to)
- require_collection used for all operations with a collection parameter (12 vector/set reads, 6 vector/set updates, time series ops, metadata ops, relations ops)
- list_*_groups intentionally kept with require_schema (nonexistent collection returns empty list)
- CLAUDE.md documents naming convention (verb_[category_]type[_by_id]) and all 3 error patterns

## Task Commits

Each task was committed atomically:

1. **Task 1: Standardize require_collection and add missing collection validation** - `d64d8bc` (feat)
2. **Task 2: Standardize all error messages to 3 patterns and update CLAUDE.md** - `f0b5484` (feat)

## Files Created/Modified
- `src/database_impl.h` - require_collection message standardized to "Cannot {op}: collection not found: {name}"
- `src/database_read.cpp` - All 19 read methods use require_collection with proper method names
- `src/database.cpp` - 8 throw sites standardized (Blob/Type -> Pattern 3, migration/schema errors)
- `src/database_create.cpp` - 7 throw sites standardized with "Cannot create_element:" prefix
- `src/database_update.cpp` - 6 throw sites + upgraded 6 vector/set updates from require_schema to require_collection
- `src/database_delete.cpp` - Operation name standardized to "delete_element"
- `src/database_metadata.cpp` - 7 throw sites standardized, replaced manual schema/collection checks with require_collection
- `src/database_time_series.cpp` - 7 throw sites standardized, upgraded 5 operations to require_collection
- `src/database_relations.cpp` - 5 throw sites standardized, replaced manual checks with require_collection
- `src/database_internal.h` - find_dimension_column error follows Pattern 2 with table name
- `CLAUDE.md` - Added Naming Convention and expanded Error Handling with 3 patterns

## Decisions Made
- list_*_groups (list_vector_groups, list_set_groups, list_time_series_groups) use require_schema, not require_collection. These iterate tables by prefix and a nonexistent collection naturally returns empty. Existing test `TimeSeriesCollectionNotFound` verified this behavior.
- Operation names in require_collection/require_schema strings match actual method names with underscores (e.g., "read_vector_integers" not "read vector") for consistent error messages.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Upgraded require_schema to require_collection in vector/set/time series update operations**
- **Found during:** Task 2
- **Issue:** update_vector_*, update_set_* methods used require_schema (only checked schema loaded), not require_collection (also validates collection exists). Same gap as the vector/set reads fixed in Task 1.
- **Fix:** Changed all 6 vector/set update methods and 5 time series operations to use require_collection
- **Files modified:** src/database_update.cpp, src/database_time_series.cpp
- **Verification:** Build succeeds, all 385 tests pass
- **Committed in:** f0b5484 (Task 2 commit)

**2. [Rule 2 - Missing Critical] Standardized operation names in all require_collection/require_schema calls**
- **Found during:** Task 2
- **Issue:** Existing require_collection calls used informal names like "read scalar", "update element", "delete element". These produce error messages like "Cannot read scalar: collection not found" which don't identify the actual method.
- **Fix:** Changed all operation strings to match method names (e.g., "read_scalar_integers", "update_element", "delete_element")
- **Files modified:** src/database_read.cpp, src/database_create.cpp, src/database_update.cpp, src/database_delete.cpp, src/database_metadata.cpp, src/database_time_series.cpp, src/database_relations.cpp
- **Verification:** Build succeeds, all 385 tests pass
- **Committed in:** f0b5484 (Task 2 commit)

**3. [Rule 1 - Bug] Reverted list_*_groups from require_collection back to require_schema**
- **Found during:** Task 2 (test verification)
- **Issue:** Changing list_time_series_groups to require_collection caused test failure -- TimeSeriesCollectionNotFound expects empty list for nonexistent collection, not an exception.
- **Fix:** Kept list_vector_groups, list_set_groups, list_time_series_groups with require_schema
- **Files modified:** src/database_metadata.cpp, src/database_time_series.cpp
- **Verification:** All 385 tests pass
- **Committed in:** f0b5484 (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (2 missing critical, 1 bug)
**Impact on plan:** All deviations improve consistency and correctness. No scope creep -- all changes within the error standardization objective.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 3 complete: all 60 Database methods use uniform verb_noun naming, all ~50 throw sites follow 3 canonical error patterns
- Ready for Phase 4 (C++ API Surface Completeness) or Phase 5 (C API Consistency)
- Downstream C API and bindings can rely on predictable error message format

---
*Phase: 03-cpp-naming-error-standardization*
*Completed: 2026-02-10*
