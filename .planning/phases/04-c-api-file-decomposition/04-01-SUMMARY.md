---
phase: 04-c-api-file-decomposition
plan: 01
subsystem: api
tags: [c-api, ffi, refactoring, file-decomposition]

# Dependency graph
requires:
  - phase: 03-cpp-naming-error-standardization
    provides: "Standardized C++ method names and error messages used by C API wrappers"
provides:
  - "database_helpers.h with shared marshaling templates and inline converters for C API"
  - "database_create.cpp with create/update/delete element C API functions"
  - "database_read.cpp with all read and free functions co-located"
  - "database_relations.cpp with relation C API functions"
affects: [04-02-PLAN, bindings]

# Tech tracking
tech-stack:
  added: []
  patterns: ["C API helpers use inline functions and templates in shared header for ODR safety across translation units"]

key-files:
  created:
    - src/c/database_helpers.h
    - src/c/database_create.cpp
    - src/c/database_read.cpp
    - src/c/database_relations.cpp
  modified:
    - src/c/database.cpp
    - src/CMakeLists.txt

key-decisions:
  - "Helpers use inline keyword (non-templates) and templates (inherently ODR-safe) in header -- no anonymous namespace needed"
  - "convert_params stays static in database.cpp for now, moves to database_query.cpp in Plan 02"
  - "All alloc/free pairs co-located in database_read.cpp for maintainability"

patterns-established:
  - "C API file decomposition: one file per operation category (create, read, relations, etc.)"
  - "Shared C API helpers in database_helpers.h, included by operation files that need marshaling"

# Metrics
duration: 19min
completed: 2026-02-10
---

# Phase 4 Plan 1: Extract Helpers and Split CRUD/Read/Relations Summary

**Shared marshaling header with inline templates, plus 3 new C API source files for create/read/relations operations**

## Performance

- **Duration:** 19 min
- **Started:** 2026-02-10T12:45:56Z
- **Completed:** 2026-02-10T13:04:27Z
- **Tasks:** 1
- **Files modified:** 6

## Accomplishments
- Created database_helpers.h with all shared marshaling templates (read_scalars_impl, read_vectors_impl, free_vectors_impl) and inline converters (copy_strings_to_c, to_c_data_type, strdup_safe, convert_scalar_to_c, free_scalar_fields, convert_group_to_c, free_group_fields)
- Extracted create/update/delete element functions into database_create.cpp (3 functions)
- Extracted all read functions and co-located free functions into database_read.cpp (22 read + 6 free = 28 functions)
- Extracted relation functions into database_relations.cpp (2 functions)
- Reduced database.cpp from 1611 to 973 lines (remaining update/metadata/query/time_series for Plan 02)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create database_helpers.h and extract CRUD/read/relations into separate files** - `0832431` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `src/c/database_helpers.h` - Shared marshaling templates and inline converters for C API
- `src/c/database_create.cpp` - create_element, update_element, delete_element_by_id
- `src/c/database_read.cpp` - All 22 read functions + 6 free functions co-located
- `src/c/database_relations.cpp` - update_scalar_relation, read_scalar_relation
- `src/c/database.cpp` - Trimmed to lifecycle + remaining functions for Plan 02
- `src/CMakeLists.txt` - Added 3 new source files to quiver_c SHARED library

## Decisions Made
- Helpers use inline keyword for non-template functions and templates (inherently ODR-safe) in header -- no anonymous namespace wrapping needed
- convert_params stays static in database.cpp temporarily, will move to database_query.cpp in Plan 02
- All alloc/free pairs co-located in database_read.cpp for maintainability

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- database_helpers.h foundation established for all subsequent C API file extractions
- Plan 02 can now extract update, metadata, query, time_series, and describe into their own files
- database.cpp at 973 lines, targeting under 200 after Plan 02

## Self-Check: PASSED

- [x] src/c/database_helpers.h exists
- [x] src/c/database_create.cpp exists
- [x] src/c/database_read.cpp exists
- [x] src/c/database_relations.cpp exists
- [x] Commit 0832431 exists

---
*Phase: 04-c-api-file-decomposition*
*Completed: 2026-02-10*
