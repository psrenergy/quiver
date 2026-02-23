---
phase: 02-reads-and-metadata
plan: 01
subsystem: database
tags: [cffi, python-bindings, scalar-reads, metadata-dataclasses, ffi-memory-management]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: "Database/Element classes, CFFI lifecycle declarations, _helpers.py check/decode functions"
provides:
  - "CFFI declarations for all Phase 2 read/metadata/free functions (30+ read, 10+ metadata)"
  - "ScalarMetadata and GroupMetadata frozen dataclasses"
  - "Scalar read methods (bulk + by-id) for integers, floats, strings"
  - "read_element_ids for collection element listing"
  - "read_scalar_relation and update_scalar_relation for FK operations"
  - "create_element helper method on Database"
  - "collections_db and relations_db test fixtures"
affects: [02-reads-and-metadata, 03-writes-and-transactions]

# Tech tracking
tech-stack:
  added: [dataclasses]
  patterns: [try-finally-free, bulk-read-null-skipping, by-id-optional-return, relation-label-mapping]

key-files:
  created:
    - bindings/python/src/quiver/metadata.py
    - bindings/python/tests/test_database_read_scalar.py
  modified:
    - bindings/python/src/quiver/_c_api.py
    - bindings/python/src/quiver/database.py
    - bindings/python/src/quiver/__init__.py
    - bindings/python/tests/conftest.py

key-decisions:
  - "Bulk scalar reads skip NULL values (matches C++ behavior, not plan assumption)"
  - "read_scalar_relation maps both NULL pointers and empty strings to None (matches Julia binding)"
  - "Added update_scalar_relation declaration and method to support relation test setup (Rule 3 auto-fix)"

patterns-established:
  - "try/finally free pattern: C API call -> early return for empty -> try marshal -> finally free"
  - "Bulk reads return only non-NULL values (C++ filters NULLs, by-id variants use has_value flag)"
  - "String by-id reads use quiver_element_free_string (not quiver_database_free_string_array)"

requirements-completed: [READ-01, READ-02, READ-07, READ-08]

# Metrics
duration: 6min
completed: 2026-02-23
---

# Phase 02 Plan 01: Scalar Reads Summary

**CFFI read/metadata declarations, ScalarMetadata/GroupMetadata dataclasses, scalar read methods with try/finally free pattern, 22 tests for integers/floats/strings/nulls/relations**

## Performance

- **Duration:** 6 min
- **Started:** 2026-02-23T14:25:49Z
- **Completed:** 2026-02-23T14:32:06Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Added all 30+ CFFI declarations for read operations, 10+ metadata functions, and all free functions needed for entire Phase 2
- Implemented frozen ScalarMetadata and GroupMetadata dataclasses for type-safe metadata representation
- Implemented 10 scalar read methods (3 bulk, 3 by-id, read_element_ids, read_scalar_relation, update_scalar_relation, create_element)
- 22 passing tests covering all scalar read variants including null handling, empty collections, defaults, relations, and self-references
- Zero ResourceWarning issues under `-W error::ResourceWarning`

## Task Commits

Each task was committed atomically:

1. **Task 1: CFFI declarations, metadata dataclasses, create_element helper, test fixtures** - `dde9375` (feat)
2. **Task 2: Scalar read methods, element ID reads, relation reads, and tests** - `b657a12` (feat)

## Files Created/Modified
- `bindings/python/src/quiver/_c_api.py` - Extended with 130+ lines of CFFI declarations for all Phase 2 read/metadata/free functions
- `bindings/python/src/quiver/metadata.py` - New: ScalarMetadata and GroupMetadata frozen dataclasses
- `bindings/python/src/quiver/database.py` - Added create_element, 9 scalar read methods, read_element_ids, relation read/update
- `bindings/python/src/quiver/__init__.py` - Exports ScalarMetadata and GroupMetadata
- `bindings/python/tests/conftest.py` - Added collections_db, relations_db fixtures and Element import
- `bindings/python/tests/test_database_read_scalar.py` - New: 22 tests for scalar reads, by-id reads, element IDs, relations

## Decisions Made
- **Bulk reads skip NULLs:** The C++ layer filters NULL values from bulk `read_scalar_*` results. Tests adjusted to match actual behavior rather than plan assumption. The `_by_id` variants correctly return None for NULL values via the `has_value` flag.
- **Relation NULL/empty mapping:** `read_scalar_relation` maps both NULL pointers AND empty strings to None, matching Julia binding behavior (`isempty(s) ? nothing : s`).
- **update_scalar_relation added early:** Added the C API declaration and Python method for `update_scalar_relation` in this plan (originally Phase 3 scope) because relation read tests require the ability to set FK relationships by label.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected test expectations for bulk string reads with NULLs**
- **Found during:** Task 2 (test execution)
- **Issue:** Plan assumed `read_scalar_strings` would return `[value, None]` for elements with NULL strings. The C++ implementation filters NULLs from bulk reads, returning only non-NULL values.
- **Fix:** Updated test expectations to match actual C++ behavior: bulk reads skip NULLs, `_by_id` reads return None.
- **Files modified:** `bindings/python/tests/test_database_read_scalar.py`
- **Verification:** All 22 tests pass
- **Committed in:** b657a12

**2. [Rule 3 - Blocking] Added update_scalar_relation declaration and method**
- **Found during:** Task 2 (relation test setup)
- **Issue:** Cannot test `read_scalar_relation` without being able to set FK relationships. `update_scalar_relation` is the C API function for this, but wasn't in the CFFI declarations.
- **Fix:** Added CFFI declaration for `quiver_database_update_scalar_relation` and corresponding Database method.
- **Files modified:** `bindings/python/src/quiver/_c_api.py`, `bindings/python/src/quiver/database.py`
- **Verification:** Relation tests pass including self-reference test
- **Committed in:** b657a12

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Both auto-fixes necessary for correctness and test completeness. No scope creep.

## Issues Encountered
None beyond the documented deviations.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All CFFI declarations for Phase 2 are in place (vector, set, metadata functions already declared)
- ScalarMetadata/GroupMetadata dataclasses ready for metadata plan
- try/finally free pattern established for all subsequent read implementations
- collections_db and relations_db fixtures available for vector/set/metadata tests

## Self-Check: PASSED

All 7 created/modified files verified on disk. Both task commits (dde9375, b657a12) found in git log.

---
*Phase: 02-reads-and-metadata*
*Completed: 2026-02-23*
