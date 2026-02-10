---
phase: 03-cpp-naming-error-standardization
plan: 01
subsystem: api
tags: [c++, naming, refactoring, database-api]

# Dependency graph
requires:
  - phase: 02-cpp-core-file-decomposition
    provides: "Decomposed source files (database_delete.cpp, database_relations.cpp, database_describe.cpp, database_time_series.cpp)"
provides:
  - "Uniform verb_noun naming across all 60 Database public methods"
  - "Clean base for C API rename phase (Phase 5)"
  - "Clean base for Lua rename phase (Phase 8)"
affects: [05-c-api-consistency, 08-lua-binding-consistency]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "verb_noun method naming: all reads use read_, all mutations use update_/delete_, no prepositions"

key-files:
  created: []
  modified:
    - include/quiver/database.h
    - src/database_delete.cpp
    - src/database_relations.cpp
    - src/database_describe.cpp
    - src/database_time_series.cpp
    - src/lua_runner.cpp
    - src/c/database.cpp
    - tests/test_database_delete.cpp
    - tests/test_database_relations.cpp
    - tests/test_database_errors.cpp
    - tests/test_database_create.cpp
    - tests/test_database_update.cpp
    - tests/test_database_time_series.cpp
    - tests/test_lua_runner.cpp

key-decisions:
  - "C API internal call sites updated alongside C++ renames to keep build passing (C API function signatures unchanged)"

patterns-established:
  - "verb_noun naming: delete_element (not delete_element_by_id), update_scalar_relation (not set_scalar_relation), export_csv (not export_to_csv)"

# Metrics
duration: 10min
completed: 2026-02-10
---

# Phase 3 Plan 1: C++ Method Naming Standardization Summary

**Renamed 5 Database methods to uniform verb_noun convention, updated 14 files across declarations, definitions, tests, and binding call sites**

## Performance

- **Duration:** 10 min (plus ~40 min build/test wait time)
- **Started:** 2026-02-10T02:37:07Z
- **Completed:** 2026-02-10T03:27:17Z
- **Tasks:** 1
- **Files modified:** 14

## Accomplishments
- Renamed all 5 non-conforming public methods to follow verb_noun pattern
- Updated all C++ test call sites (zero stale references in C++ layer)
- Preserved Lua-facing string names for Phase 8 rename
- Updated C API internal call sites to use new method names (C API function signatures unchanged)
- All 385 C++ tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Rename 5 methods in declarations, definitions, and all C++ call sites** - `bd406f6` (refactor)

## Files Created/Modified
- `include/quiver/database.h` - Updated 5 method declarations
- `src/database_delete.cpp` - delete_element_by_id -> delete_element
- `src/database_relations.cpp` - set_scalar_relation -> update_scalar_relation
- `src/database_describe.cpp` - export_to_csv -> export_csv, import_from_csv -> import_csv
- `src/database_time_series.cpp` - read_time_series_group_by_id -> read_time_series_group
- `src/lua_runner.cpp` - Updated C++ method calls (Lua-facing strings preserved)
- `src/c/database.cpp` - Updated internal C++ method calls (C API function names preserved)
- `tests/test_database_delete.cpp` - Updated 5 call sites
- `tests/test_database_relations.cpp` - Updated 7 call sites
- `tests/test_database_errors.cpp` - Updated 6 call sites
- `tests/test_database_create.cpp` - Updated 1 call site
- `tests/test_database_update.cpp` - Updated 1 call site
- `tests/test_database_time_series.cpp` - Updated 9 call sites
- `tests/test_lua_runner.cpp` - Updated 2 C++ call sites (Lua strings preserved)

## Decisions Made
- C API internal call sites (src/c/database.cpp) updated alongside C++ renames to keep the build passing. The C API function signatures remain unchanged -- only the internal C++ method calls were updated. This was necessary as a blocking issue (Deviation Rule 3) since the build would fail otherwise.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated C API internal call sites in src/c/database.cpp**
- **Found during:** Task 1 (build verification)
- **Issue:** The plan stated "Do NOT rename any C API functions" but `src/c/database.cpp` internally calls the C++ methods by name. After renaming the C++ methods, the C API wrapper failed to compile.
- **Fix:** Updated 5 internal C++ method calls in `src/c/database.cpp` while keeping all C API function signatures unchanged. This is not renaming C API functions -- it's updating internal call sites.
- **Files modified:** src/c/database.cpp
- **Verification:** Build succeeds, all 385 C++ tests pass
- **Committed in:** bd406f6 (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Essential for build correctness. No scope creep -- C API function signatures remain unchanged for Phase 5.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 60 Database public methods now follow uniform verb_noun naming
- Ready for Plan 03-02 (error message standardization)
- C API Phase 5 can rename C API function signatures to match
- Lua Phase 8 can rename Lua-facing string names to match

---
*Phase: 03-cpp-naming-error-standardization*
*Completed: 2026-02-10*
