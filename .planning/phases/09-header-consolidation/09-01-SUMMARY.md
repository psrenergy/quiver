---
phase: 09-header-consolidation
plan: 01
subsystem: api
tags: [c-api, cpp, headers, refactor, options]

# Dependency graph
requires:
  - phase: 08-library-integration
    provides: rapidcsv-based CSV export with CSVExportOptions struct in csv.h
provides:
  - Consolidated C API options header (include/quiver/c/options.h) with LogLevel, DatabaseOptions, and CSVExportOptions
  - Consolidated C++ options header (include/quiver/options.h) with DatabaseOptions alias and CSVExportOptions struct
  - Deleted csv.h files at both C and C++ layers
affects: [09-02-ffi-regeneration, bindings]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single options header per layer: all option/config types consolidated into one header"

key-files:
  created:
    - include/quiver/options.h
  modified:
    - include/quiver/c/options.h
    - include/quiver/c/database.h
    - include/quiver/database.h
    - src/c/database_csv.cpp
    - src/database_csv.cpp
    - src/lua_runner.cpp
    - tests/test_database_csv.cpp
    - tests/test_c_api_database_csv.cpp
    - CLAUDE.md

key-decisions:
  - "csv.h deleted outright with no compatibility stub, per project philosophy"
  - "Each .cpp file includes options.h directly rather than relying on transitive includes through database.h"

patterns-established:
  - "Options consolidation: all option types (DatabaseOptions, CSVExportOptions) live in a single header per layer"

requirements-completed: [HDR-01, HDR-02]

# Metrics
duration: 7min
completed: 2026-02-22
---

# Phase 9 Plan 1: Header Consolidation Summary

**Merged CSV option types into consolidated options.h headers at both C API and C++ layers, deleted standalone csv.h files**

## Performance

- **Duration:** 7 min
- **Started:** 2026-02-23T02:47:16Z
- **Completed:** 2026-02-23T02:54:20Z
- **Tasks:** 3
- **Files modified:** 10 (1 created, 2 deleted, 7 modified)

## Accomplishments
- Consolidated quiver_csv_export_options_t and quiver_csv_export_options_default() into include/quiver/c/options.h
- Created include/quiver/options.h with DatabaseOptions alias, CSVExportOptions struct, and factory functions
- Updated all 7 source files to include options.h instead of csv.h
- Removed DatabaseOptions alias and default_database_options() from database.h (now in options.h)
- Deleted include/quiver/c/csv.h and include/quiver/csv.h
- Updated CLAUDE.md file tree to reflect new header layout

## Task Commits

Each task was committed atomically:

1. **Task 1: Merge CSV types into options headers and create C++ options.h** - `7fd8f9e` (feat)
2. **Task 2: Update all include paths and remove moved definitions from database.h** - `cea8a8f` (refactor)
3. **Task 3: Update CLAUDE.md to reflect header consolidation** - `f853812` (docs)

**Plan metadata:** (pending) (docs: complete plan)

## Files Created/Modified
- `include/quiver/c/options.h` - Added #include "common.h", quiver_csv_export_options_t struct with full documentation, and quiver_csv_export_options_default() declaration
- `include/quiver/options.h` - New consolidated C++ options header with DatabaseOptions alias and CSVExportOptions struct
- `include/quiver/c/database.h` - Removed #include "csv.h" (redundant with options.h)
- `include/quiver/database.h` - Replaced quiver/c/options.h + quiver/csv.h includes with quiver/options.h; removed DatabaseOptions alias and factory
- `src/c/database_csv.cpp` - Updated both C and C++ csv.h includes to options.h
- `src/database_csv.cpp` - Updated quiver/csv.h include to quiver/options.h
- `src/lua_runner.cpp` - Updated quiver/csv.h include to quiver/options.h
- `tests/test_database_csv.cpp` - Updated quiver/csv.h include to quiver/options.h
- `tests/test_c_api_database_csv.cpp` - Updated quiver/c/csv.h include to quiver/c/options.h
- `include/quiver/c/csv.h` - Deleted (merged into options.h)
- `include/quiver/csv.h` - Deleted (merged into options.h)
- `CLAUDE.md` - Updated file tree entries for options.h headers

## Decisions Made
- csv.h deleted outright with no compatibility stub, per project philosophy (delete unused code, do not deprecate)
- Each .cpp file includes options.h directly rather than relying on transitive includes through database.h (per user decision)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Consolidated headers ready for FFI binding regeneration (Plan 09-02)
- Julia generator will auto-discover CSV types from options.h
- Dart ffigen will pick up CSV struct transitively through database.h -> options.h

## Self-Check: PASSED

All created files exist, all deleted files confirmed removed, all 3 task commits verified in git log.

---
*Phase: 09-header-consolidation*
*Completed: 2026-02-22*
