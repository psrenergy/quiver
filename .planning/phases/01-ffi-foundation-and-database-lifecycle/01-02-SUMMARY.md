---
phase: 01-ffi-foundation-and-database-lifecycle
plan: 02
subsystem: ffi
tags: [bun-ffi, typescript, database-lifecycle, out-parameters, dll-loading]

requires:
  - phase: 01-01
    provides: FFI loader, QuiverError check() helper, out-parameter allocators, type conversion helpers

provides:
  - Database class with fromSchema, fromMigrations, close lifecycle methods
  - End-to-end validation of out-parameter pointer pattern (read.ptr on BigInt64Array)
  - Integration test suite covering all lifecycle operations
  - Test runner script (test.bat) with PATH setup for DLL discovery

affects: [02-01, all-future-js-binding-plans]

tech-stack:
  added: []
  patterns: [kernel32-preload-pattern, private-constructor-factory, closed-guard-pattern]

key-files:
  created:
    - bindings/js/src/database.ts
    - bindings/js/test/database.test.ts
    - bindings/js/test/test.bat
  modified:
    - bindings/js/src/index.ts
    - bindings/js/src/loader.ts

key-decisions:
  - "Windows DLL pre-load via kernel32 LoadLibraryA (bun:ffi dlopen requires at least one symbol, cannot pre-load dependency-only DLLs)"
  - "bun:ffi cstring return type yields CString object (typeof 'object'), not plain string -- use .toString() for comparisons"
  - "Out-parameter pattern validated: BigInt64Array(1) + ptr() + read.ptr() works correctly for quiver_database_t**"

patterns-established:
  - "Factory method pattern: getSymbols() + makeDefaultOptions() + allocPointerOut() + check() + readPointerOut() for every C API factory call"
  - "Closed guard: private _closed boolean + ensureOpen() throwing QuiverError('Database is closed')"
  - "Internal accessor: @internal _handle getter for cross-module pointer access without public API exposure"
  - "kernel32 LoadLibraryA for Windows DLL dependency pre-loading (replaces broken dlopen({}) approach)"

requirements-completed: [LIFE-01, LIFE-02, LIFE-03]

duration: 5min
completed: 2026-03-08
---

# Phase 1 Plan 02: Database Lifecycle Summary

**Database class with fromSchema/fromMigrations factory methods, idempotent close, and 13 integration tests validating the full FFI pipeline end-to-end**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-08T21:09:49Z
- **Completed:** 2026-03-08T21:14:55Z
- **Tasks:** 1 (TDD: test + feat commits)
- **Files modified:** 5

## Accomplishments
- Database class with fromSchema() and fromMigrations() factory methods, close() with idempotent guard, and ensureOpen() throw
- Full validation of out-parameter pointer pattern (BigInt64Array + read.ptr) -- resolves STATE.md blocker from research phase
- 13 integration tests covering lifecycle, error handling, and library loading
- Fixed Windows DLL pre-load to use kernel32 LoadLibraryA instead of bun:ffi dlopen (which requires at least one symbol)

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Add failing tests for Database lifecycle** - `397db87` (test)
2. **Task 1 (GREEN): Implement Database class with lifecycle methods** - `6c4e2cc` (feat)

## Files Created/Modified
- `bindings/js/src/database.ts` - Database class with fromSchema, fromMigrations, close, ensureOpen, _handle
- `bindings/js/src/index.ts` - Updated barrel export to include Database
- `bindings/js/src/loader.ts` - Fixed Windows DLL pre-load using kernel32 LoadLibraryA
- `bindings/js/test/database.test.ts` - 13 integration tests for lifecycle, errors, library loading
- `bindings/js/test/test.bat` - Test runner script with PATH setup for DLL discovery

## Decisions Made
- Windows DLL pre-load changed from `dlopen(corePath, {})` to `kernel32.LoadLibraryA(corePath)` because bun:ffi requires at least one symbol definition in dlopen, making it unusable for dependency-only loading (libquiver.dll exports C++ symbols only, no C API)
- bun:ffi cstring return type returns a CString object (typeof "object"), not a plain JS string. Tests use `.toString()` for type-safe string comparison.
- The out-parameter pattern (BigInt64Array(1) + ptr() + read.ptr()) is now empirically validated for quiver_database_t** -- resolving the STATE.md concern from research

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Windows DLL pre-load via kernel32 LoadLibraryA**
- **Found during:** Task 1 GREEN phase (all tests failing with "Cannot load native library")
- **Issue:** Plan 01's loader.ts used `dlopen(corePath, {})` for Windows DLL pre-loading. bun:ffi dlopen() throws "Expected at least one symbol" when symbols object is empty. libquiver.dll has no C API symbols to reference.
- **Fix:** Added `preloadWindows()` function that opens kernel32.dll via bun:ffi and calls LoadLibraryA to pre-load the dependency DLL. This is the bun:ffi equivalent of Python's `os.add_dll_directory()` pattern.
- **Files modified:** bindings/js/src/loader.ts
- **Verification:** All 13 tests pass, DLL dependency chain loads correctly
- **Committed in:** 6c4e2cc (Task 1 GREEN commit)

**2. [Rule 1 - Bug] Fixed quiver_version test for CString return type**
- **Found during:** Task 1 GREEN phase (1 test failing)
- **Issue:** Test expected `typeof version === "string"` but bun:ffi cstring returns a CString object (extends String), so typeof is "object"
- **Fix:** Changed test to call `.toString()` before type checking
- **Files modified:** bindings/js/test/database.test.ts
- **Verification:** Test passes correctly
- **Committed in:** 6c4e2cc (Task 1 GREEN commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 bug)
**Impact on plan:** Both fixes necessary for correct operation. The kernel32 pre-load is a strictly better approach. No scope creep.

## Issues Encountered

None beyond the auto-fixed deviations above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Complete FFI pipeline validated end-to-end: library loading, struct construction, out-parameter handling, error propagation, resource lifecycle
- Database class ready for extension in Phase 2 (create.ts, read.ts in later plans)
- Internal _handle accessor provides cross-module pointer access for future Database method files
- test.bat runner and test infrastructure established for all future test plans
- STATE.md blockers resolved: out-parameter pattern validated, DLL loading works

## Self-Check: PASSED

All 5 modified/created files verified present. Both task commits (397db87, 6c4e2cc) verified in git log.

---
*Phase: 01-ffi-foundation-and-database-lifecycle*
*Completed: 2026-03-08*
