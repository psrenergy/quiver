---
phase: 01-core-implementation
plan: 02
subsystem: bindings
tags: [dart, python, ffi, composite-reader, convenience-method]

# Dependency graph
requires: []
provides:
  - "readElementById Dart method for flat element reads"
  - "read_element_by_id Python method for flat element reads"
affects: [02-extended-bindings, 03-documentation]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Binding-level composition: compose existing C API calls into convenience methods"
    - "Flat map pattern: multi-column groups produce per-column top-level keys"
    - "Nonexistent ID detection: check label==null after scalar reads, return empty"

key-files:
  created:
    - bindings/python/tests/test_database_read_element.py
  modified:
    - bindings/dart/lib/src/database_read.dart
    - bindings/dart/test/database_read_test.dart
    - bindings/dart/hook/build.dart
    - bindings/dart/lib/src/ffi/library_loader.dart
    - bindings/python/src/quiverdb/database.py

key-decisions:
  - "Use per-column iteration for vectors/sets (not group-name keying) to produce flat maps"
  - "Detect nonexistent elements via label==null rather than a separate existence check"

patterns-established:
  - "readElementById/read_element_by_id: flat map with scalars + vector columns + set columns as top-level keys"
  - "id excluded from result, label included as regular scalar"

requirements-completed: [READ-01, READ-02]

# Metrics
duration: 11min
completed: 2026-02-26
---

# Phase 01 Plan 02: Dart and Python read_element_by_id Summary

**Binding-level readElementById/read_element_by_id composing metadata + typed by-id reads into flat maps with DateTime support**

## Performance

- **Duration:** 11 min
- **Started:** 2026-02-26T17:55:41Z
- **Completed:** 2026-02-26T18:06:47Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Dart `readElementById` returns flat `Map<String, Object?>` with all scalar, vector column, and set column values
- Python `read_element_by_id` returns flat `dict` with identical structure and behavior
- Both handle nonexistent IDs (empty map/dict), exclude `id`, include `label`, use native DateTime types
- 300 Dart tests pass, 205 Python tests pass (no regressions)

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement readElementById in Dart binding and add tests** - `82a1cc2` (feat)
2. **Task 2: Implement read_element_by_id in Python binding and add tests** - `772c640` (feat)

## Files Created/Modified
- `bindings/dart/lib/src/database_read.dart` - Added readElementById composite method
- `bindings/dart/test/database_read_test.dart` - Added 3 tests for readElementById
- `bindings/dart/hook/build.dart` - Fixed CMake generator from vs2022 to ninja (environment fix)
- `bindings/dart/lib/src/ffi/library_loader.dart` - Fixed DLL loading with lib prefix from Ninja builds
- `bindings/python/src/quiverdb/database.py` - Added read_element_by_id composite method
- `bindings/python/tests/test_database_read_element.py` - Created 3 tests for read_element_by_id

## Decisions Made
- Used per-column iteration over vector/set groups to produce flat maps (each column is its own top-level key)
- Detect nonexistent elements by checking `label == null` after scalar reads (label is NOT NULL in schema)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed Dart build hook CMake generator**
- **Found during:** Task 1 (Dart implementation)
- **Issue:** `Generator.vs2022` in build.dart references Visual Studio 17 2022, but machine has VS 2026 (18). The Strawberry CMake (3.29.2) doesn't know about VS 2026 generator strings.
- **Fix:** Changed to `Generator.ninja` which works with any CMake version and MSVC toolchain
- **Files modified:** bindings/dart/hook/build.dart
- **Verification:** Dart tests build and run successfully (300 passed)
- **Committed in:** 82a1cc2 (Task 1 commit)

**2. [Rule 3 - Blocking] Fixed Dart DLL loader for Ninja-generated lib prefix**
- **Found during:** Task 1 (Dart implementation)
- **Issue:** Ninja CMake generates `libquiver.dll` (with lib prefix), but the library loader's core preload looked for `quiver.dll` (without prefix). `libquiver_c.dll` loaded but failed because its dependency `libquiver.dll` wasn't preloaded.
- **Fix:** Added fallback `lib` prefix check in the Windows DLL preload logic
- **Files modified:** bindings/dart/lib/src/ffi/library_loader.dart
- **Verification:** All 300 Dart tests pass after fix
- **Committed in:** 82a1cc2 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes were necessary to run Dart tests in the current environment. No scope creep.

## Issues Encountered
None beyond the auto-fixed blocking issues.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Dart and Python bindings now have full read_element_by_id support
- Julia binding implementation may follow in a separate plan if needed
- All existing tests continue to pass in both languages

## Self-Check: PASSED

All files verified present. All commit hashes found in git log.

---
*Phase: 01-core-implementation*
*Completed: 2026-02-26*
