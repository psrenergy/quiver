---
phase: 03-c-api-string-consistency
plan: 01
subsystem: api
tags: [c-api, string-handling, refactoring, strdup_safe]

# Dependency graph
requires:
  - phase: 02-c-core-refactoring
    provides: C API codebase with established patterns
provides:
  - strdup_safe utility in src/utils/string.h under namespace quiver::string
  - Consistent string allocation across all C API files via single helper
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns: [centralized string allocation via utils/string.h]

key-files:
  created: []
  modified:
    - src/utils/string.h
    - src/CMakeLists.txt
    - src/c/database_helpers.h
    - src/c/database_read.cpp
    - src/c/element.cpp
    - CLAUDE.md

key-decisions:
  - "Only strdup_safe moved to utils/string.h; copy_strings_to_c stays in database_helpers.h to avoid utility-layer dependency on C API types"
  - "using-declaration in database_helpers.h preserves 13+ existing bare strdup_safe call sites without qualification changes"
  - "Replaced std::bad_alloc catch in element.cpp with std::exception for consistency with all other C API functions"

patterns-established:
  - "String utility functions live in src/utils/string.h under namespace quiver::string"
  - "C API files access shared utilities via using-declarations from database_helpers.h"

requirements-completed: [QUAL-04]

# Metrics
duration: 3min
completed: 2026-03-01
---

# Phase 3 Plan 1: C API String Consistency Summary

**Extracted strdup_safe to src/utils/string.h and eliminated all 5 inline new char[] + memcpy/std::copy string allocation patterns from src/c/**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-01T17:17:52Z
- **Completed:** 2026-03-01T17:20:27Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Moved strdup_safe from database_helpers.h to src/utils/string.h under namespace quiver::string
- Replaced all 5 inline string allocation sites (copy_strings_to_c, vector read, set read, scalar string by ID, element to_string)
- Added PRIVATE include path for quiver_c target so C API files can access src/utils/
- All 325 C API tests and 530 C++ tests pass unchanged

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract strdup_safe to utils, update CMake, replace all 5 inline allocation sites** - `b30d738` (refactor)
2. **Task 2: Run tests and update documentation** - `f142ad9` (docs)

## Files Created/Modified
- `src/utils/string.h` - Added strdup_safe function alongside existing trim utility
- `src/CMakeLists.txt` - Added PRIVATE include directory for quiver_c target
- `src/c/database_helpers.h` - Removed strdup_safe definition, added include + using-declaration, simplified copy_strings_to_c
- `src/c/database_read.cpp` - Replaced 3 inline string allocation sites with strdup_safe calls
- `src/c/element.cpp` - Added include + using-declaration, replaced memcpy pattern with strdup_safe, standardized error handling
- `CLAUDE.md` - Updated architecture tree and String Handling section

## Decisions Made
- Only strdup_safe moved to utils/string.h; copy_strings_to_c stays in database_helpers.h because it returns quiver_error_t (C API type), and moving it would create a dependency from utility layer to C API headers
- using-declaration approach in database_helpers.h keeps the diff minimal -- all 13+ existing bare strdup_safe() call sites in metadata converters, query, and time series files continue to compile without qualification changes
- Replaced the dead std::bad_alloc catch in element.cpp's quiver_element_to_string with the standard std::exception pattern used by every other C API function

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- QUAL-04 requirement fully satisfied
- All inline string allocation patterns eliminated from src/c/
- strdup_safe is now a shared utility available to any future code that needs C-string allocation

## Self-Check: PASSED

All 6 modified files verified on disk. Both task commits (b30d738, f142ad9) verified in git log.

---
*Phase: 03-c-api-string-consistency*
*Completed: 2026-03-01*
