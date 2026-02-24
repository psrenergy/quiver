---
phase: 04-cleanup
plan: 01
subsystem: api
tags: [c++, c-api, lua, cleanup, relations]

# Dependency graph
requires:
  - phase: 01-create-element-fk
    provides: "resolve_fk_label helper in Database::Impl for FK label resolution in create_element"
  - phase: 02-update-element-fk
    provides: "FK label resolution in update_element via pre-resolve pass"
provides:
  - "C++ and C API fully cleaned of update_scalar_relation and read_scalar_relation"
  - "9 FK resolution tests preserved in test_database_create.cpp"
  - "Lua binding cleaned of relation methods"
affects: [04-cleanup plan 02 (binding cleanup)]

# Tech tracking
tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - "include/quiver/database.h"
    - "include/quiver/c/database.h"
    - "src/CMakeLists.txt"
    - "src/lua_runner.cpp"
    - "tests/test_database_create.cpp"
    - "tests/test_database_errors.cpp"
    - "tests/test_c_api_database_lifecycle.cpp"
    - "tests/test_lua_runner.cpp"
    - "tests/CMakeLists.txt"
    - "CLAUDE.md"

key-decisions:
  - "FK resolution tests relocated to test_database_create.cpp (not a new file) since they exercise create_element behavior"

patterns-established: []

requirements-completed: [CLN-01]

# Metrics
duration: 12min
completed: 2026-02-24
---

# Phase 04 Plan 01: Remove Relation Methods Summary

**Removed update_scalar_relation and read_scalar_relation from C++ core, C API, Lua, and all tests; relocated 9 FK resolution tests to test_database_create.cpp**

## Performance

- **Duration:** 12 min
- **Started:** 2026-02-24T02:31:07Z
- **Completed:** 2026-02-24T02:43:07Z
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments
- Completely removed update_scalar_relation and read_scalar_relation from C++ headers, C API headers, implementations, Lua binding, and all test files
- Deleted src/database_relations.cpp, src/c/database_relations.cpp, and tests/test_database_relations.cpp
- Relocated 9 FK resolution tests to test_database_create.cpp preserving full coverage
- All 442 C++ tests and 271 C API tests pass green

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove relation methods from C++ core, C API, Lua, and CMakeLists** - `69499d3` (feat)
2. **Task 2: Relocate FK resolution tests and remove all relation test code** - `13e1147` (feat)
3. **CLAUDE.md update** - `11fee1d` (chore)

## Files Created/Modified
- `include/quiver/database.h` - Removed relation method declarations
- `include/quiver/c/database.h` - Removed C API relation function declarations
- `src/database_relations.cpp` - Deleted (C++ relation implementation)
- `src/c/database_relations.cpp` - Deleted (C API relation implementation)
- `src/CMakeLists.txt` - Removed relation source files from both library targets
- `src/lua_runner.cpp` - Removed Group 6 Relations registration and helper function
- `tests/test_database_create.cpp` - Added 9 FK resolution tests (relocated from relations)
- `tests/test_database_relations.cpp` - Deleted entirely
- `tests/test_database_errors.cpp` - Removed 7 relation error tests
- `tests/test_c_api_database_lifecycle.cpp` - Removed all C API relation tests (null-checks + valid operations)
- `tests/test_lua_runner.cpp` - Removed LuaRunnerRelationsTest fixture and 2 tests
- `tests/CMakeLists.txt` - Removed test_database_relations.cpp from sources
- `CLAUDE.md` - Removed all relation method references

## Decisions Made
- FK resolution tests placed in test_database_create.cpp since they exercise create_element FK resolution behavior (not standalone relation methods)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Updated CLAUDE.md to remove relation references**
- **Found during:** After Task 2
- **Issue:** CLAUDE.md Self-Updating principle requires keeping documentation current with codebase changes
- **Fix:** Removed all relation method references from architecture listing, test organization, naming examples, error patterns, cross-layer examples table, and Core API listing
- **Files modified:** CLAUDE.md
- **Verification:** Grep confirms no stale relation references remain in CLAUDE.md
- **Committed in:** 11fee1d

---

**Total deviations:** 1 auto-fixed (1 missing critical - documentation consistency)
**Impact on plan:** Documentation update required by project principle. No scope creep.

## Issues Encountered
- C API tests build required re-enabling QUIVER_BUILD_C_API=ON cmake flag (CMake reconfiguration reset the option). Resolved by reconfiguring with the flag.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- C++ core and C API fully cleaned of relation methods
- Ready for Plan 02 (binding cleanup in Julia and Dart)
- Relations schema (tests/schemas/valid/relations.sql) retained for FK resolution tests

## Self-Check: PASSED

- All 10 modified files exist on disk
- All 3 deleted files confirmed absent
- All 3 task commits verified in git log (69499d3, 13e1147, 11fee1d)

---
*Phase: 04-cleanup*
*Completed: 2026-02-24*
