---
phase: 07-dart-bindings-standardization
plan: 01
subsystem: bindings
tags: [dart, ffi, error-handling, naming-conventions]

# Dependency graph
requires:
  - phase: 05-c-api-naming-error-standardization
    provides: Standardized C API function names and error patterns
  - phase: 06-julia-bindings-standardization
    provides: Established binding-layer error handling patterns (StateError, ArgumentError)
provides:
  - Standardized Dart binding naming (deleteElement, camelCase)
  - Dart error handling via check() for C API errors
  - StateError for use-after-close/dispose guards
  - ArgumentError for type dispatch errors
affects: [08-dart-bindings-api-completion, 09-lua-bindings-standardization]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Dart state guards throw StateError (not DatabaseException)"
    - "Dart type dispatch errors throw ArgumentError (not DatabaseException)"
    - "Dart factory constructors use check() for C API error propagation"
    - "LuaRunner.run() uses quiver_lua_runner_get_error then falls back to check()"

key-files:
  created: []
  modified:
    - bindings/dart/lib/src/database.dart
    - bindings/dart/lib/src/database_delete.dart
    - bindings/dart/lib/src/database_read.dart
    - bindings/dart/lib/src/element.dart
    - bindings/dart/lib/src/lua_runner.dart
    - bindings/dart/test/database_delete_test.dart
    - bindings/dart/test/database_lifecycle_test.dart
    - bindings/dart/test/element_test.dart

key-decisions:
  - "LuaRunner.run() keeps quiver_lua_runner_get_error path with check() fallback for QUIVER_REQUIRE failures"
  - "Empty array in Element.set() throws ArgumentError (Dart-side type dispatch, not database error)"

patterns-established:
  - "Dart binding error taxonomy: DatabaseException for C API errors, StateError for programming errors, ArgumentError for type dispatch"

# Metrics
duration: 14min
completed: 2026-02-10
---

# Phase 7 Plan 1: Dart Bindings Standardization Summary

**Standardized Dart binding naming (deleteElement) and error handling (check(), StateError, ArgumentError) with zero custom error strings in wrapper code**

## Performance

- **Duration:** 14 min
- **Started:** 2026-02-10T19:02:18Z
- **Completed:** 2026-02-10T19:16:27Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Renamed deleteElementById to deleteElement matching C API pattern
- Replaced all manual error handling in factory constructors with check()
- Established consistent error taxonomy: StateError for use-after-close/dispose, ArgumentError for type dispatch, DatabaseException only from C API via check()
- Eliminated all bare Exception throws and custom DatabaseException strings from wrapper code
- All 227 Dart tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Rename deleteElementById and standardize error handling** - `4717c7e` (feat)
2. **Task 2: Fix defensive else branches in database_read.dart** - `c70efd0` (fix)

## Files Created/Modified
- `bindings/dart/lib/src/database.dart` - Factory constructors use check(), _ensureNotClosed throws StateError
- `bindings/dart/lib/src/database_delete.dart` - Renamed deleteElementById to deleteElement
- `bindings/dart/lib/src/database_read.dart` - Defensive branches use ArgumentError instead of bare Exception
- `bindings/dart/lib/src/element.dart` - _ensureNotDisposed throws StateError, type dispatch errors throw ArgumentError
- `bindings/dart/lib/src/lua_runner.dart` - Constructor uses check(), _ensureNotDisposed throws StateError, run() removes 'Lua error:' prefix
- `bindings/dart/test/database_delete_test.dart` - 5 call sites updated from deleteElementById to deleteElement
- `bindings/dart/test/database_lifecycle_test.dart` - 2 assertions updated to StateError
- `bindings/dart/test/element_test.dart` - 1 assertion updated to ArgumentError, 1 to StateError

## Decisions Made
- LuaRunner.run() keeps quiver_lua_runner_get_error path with check() fallback for QUIVER_REQUIRE failures (runner stores errors in runner->last_error, not global quiver_set_last_error)
- Empty array in Element.set() throws ArgumentError (Dart-side type dispatch, not database error)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Dart binding naming and error handling fully standardized
- Ready for Phase 8 (Dart bindings API completion) or Phase 9 (Lua bindings standardization)
- DatabaseException only appears in exceptions.dart (check() function and class definition)
- Zero bare Exception throws in Dart binding source

---
*Phase: 07-dart-bindings-standardization*
*Completed: 2026-02-10*
