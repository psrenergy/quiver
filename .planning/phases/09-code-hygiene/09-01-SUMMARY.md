---
phase: 09-code-hygiene
plan: 01
subsystem: database
tags: [sql-injection, identifier-validation, defense-in-depth, schema-validation]

# Dependency graph
requires:
  - phase: 01-cpp-impl-header-extraction
    provides: Database::Impl struct in database_impl.h
  - phase: 02-cpp-core-file-decomposition
    provides: Decomposed database_read.cpp, database_update.cpp, database_time_series.cpp
provides:
  - require_column() validation helper on Database::Impl
  - is_safe_identifier() character validation for PRAGMA queries in schema.cpp
  - Column-level identifier validation on all read and vector/set update paths
affects: [c-api, bindings]

# Tech tracking
tech-stack:
  added: []
  patterns: [require_column defense-in-depth validation, is_safe_identifier PRAGMA guard]

key-files:
  created: []
  modified:
    - src/database_impl.h
    - src/schema.cpp
    - src/database_read.cpp
    - src/database_update.cpp
    - src/database_time_series.cpp
    - tests/test_database_read.cpp
    - tests/test_database_update.cpp

key-decisions:
  - "require_column validates column existence against loaded Schema before SQL concatenation"
  - "is_safe_identifier uses alphanumeric+underscore character class for PRAGMA table/index names"
  - "Vector/set read and update paths add require_column after find_*_table (defense-in-depth)"
  - "Scalar update paths already validated by TypeValidator -- no additional validation needed"
  - "Time series read/update paths already validated by schema metadata -- only update_time_series_files needed caller-provided column validation"

patterns-established:
  - "require_column pattern: call after require_collection and find_*_table, before SQL concatenation"
  - "is_safe_identifier pattern: guard PRAGMA queries that concatenate identifiers"

# Metrics
duration: 13min
completed: 2026-02-10
---

# Phase 9 Plan 1: Identifier Validation Summary

**require_column() and is_safe_identifier() defense-in-depth validation across all SQL concatenation sites in C++ core**

## Performance

- **Duration:** 13 min
- **Started:** 2026-02-10T21:30:46Z
- **Completed:** 2026-02-10T21:43:31Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments
- Added require_column() helper to Database::Impl for column existence validation against loaded Schema
- Added is_safe_identifier() character-class validation guarding all 4 PRAGMA concatenation sites in schema.cpp
- Added require_column calls to all 18 read methods (6 scalar, 6 vector, 6 set) in database_read.cpp
- Added require_column calls to all 6 vector and 6 set update methods in database_update.cpp
- Added column validation to update_time_series_files for caller-provided column names
- Confirmed database_create.cpp, database_relations.cpp, database_delete.cpp, and database_time_series.cpp read paths are already validated by existing mechanisms (TypeValidator, FK lookup, schema metadata)
- Added 3 targeted tests verifying invalid identifier rejection with error message verification

## Task Commits

Each task was committed atomically:

1. **Task 1: Add require_column helper and is_safe_identifier, validate read and update paths** - `a452d9c` (feat)
2. **Task 2: Validate remaining paths and confirm already-validated sites** - `4f65e8c` (feat)
3. **Task 3: Add identifier validation tests** - `a12d553` (test)

## Files Created/Modified
- `src/database_impl.h` - Added require_column() validation helper on Database::Impl
- `src/schema.cpp` - Added is_safe_identifier() and guards on 4 PRAGMA concatenation sites
- `src/database_read.cpp` - Added require_column calls to all 18 read methods
- `src/database_update.cpp` - Added require_column calls to all 6 vector and 6 set update methods
- `src/database_time_series.cpp` - Added column validation to update_time_series_files
- `tests/test_database_read.cpp` - Added 2 identifier validation tests
- `tests/test_database_update.cpp` - Added 1 identifier validation test

## Decisions Made
- require_column is defense-in-depth: find_vector_table already validates the table exists, but require_column explicitly validates the column name before it's concatenated into SQL
- Scalar update paths (update_scalar_integer/float/string) do not need require_column because TypeValidator::validate_scalar already validates column existence
- update_element scalar section validated by TypeValidator; array routing validated by find_table_for_column -- no additional validation needed
- Time series read/update column names come from schema metadata (trusted) -- only update_time_series_files needed validation because caller provides column name map keys

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All SQL concatenation sites now have identifier validation
- 388 C++ tests and 247 C API tests pass
- Ready for 09-02 (remaining code hygiene tasks)

## Self-Check: PASSED

All 7 modified files verified present. All 3 task commit hashes verified in git log.

---
*Phase: 09-code-hygiene*
*Completed: 2026-02-10*
