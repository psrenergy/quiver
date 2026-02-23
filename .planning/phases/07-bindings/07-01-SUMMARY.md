---
phase: 07-bindings
plan: 01
subsystem: bindings
tags: [julia, dart, csv, ffi, marshaling, parallel-arrays]

# Dependency graph
requires:
  - phase: 06-c-api
    provides: "5-param quiver_database_export_csv with quiver_csv_export_options_t flat struct"
provides:
  - "Julia export_csv with keyword args for enum_labels and date_time_format"
  - "Dart exportCSV with named params for enumLabels and dateTimeFormat"
  - "build_csv_options helper for Julia Dict-to-flat-struct marshaling"
  - "Arena-based Dart options struct allocation"
affects: [07-02-lua-bindings]

# Tech tracking
tech-stack:
  added: []
  patterns: ["build_csv_options keyword-arg marshaling pattern (Julia)", "Arena-based struct allocation for CSV options (Dart)"]

key-files:
  created:
    - bindings/julia/test/test_database_csv.jl
    - bindings/dart/test/database_csv_test.dart
  modified:
    - bindings/julia/src/database_csv.jl
    - bindings/julia/src/database.jl
    - bindings/dart/lib/src/database_csv.dart

key-decisions:
  - "Julia build_csv_options constructs new immutable struct instance with all 6 positional fields"
  - "Dart allocates quiver_csv_export_options_t via Arena and sets fields directly (no default factory)"
  - "Fixed Julia build_options to use keyword splatting (;kwargs...) for Julia 1.12 compatibility"

patterns-established:
  - "build_csv_options(; kwargs...): Julia pattern for marshaling keyword args to C API flat struct via GC.@preserve"
  - "Arena<quiver_csv_export_options_t>: Dart pattern for FFI struct allocation with automatic cleanup"

requirements-completed: [BIND-01, BIND-02]

# Metrics
duration: 13min
completed: 2026-02-22
---

# Phase 7 Plan 1: Julia and Dart CSV Export Bindings Summary

**Julia export_csv and Dart exportCSV with native Dict/Map-to-flat-struct marshaling for enum labels and date formatting**

## Performance

- **Duration:** 13 min
- **Started:** 2026-02-22T23:49:03Z
- **Completed:** 2026-02-23T00:02:20Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Julia export_csv accepts keyword args (enum_labels, date_time_format) and marshals Dict to C API flat parallel-array struct
- Dart exportCSV accepts named params (enumLabels, dateTimeFormat) and marshals Map to C API struct via Arena
- 19 Julia CSV tests + 5 Dart CSV tests all pass, matching C++ expected output
- All 437 Julia tests and 252 Dart tests pass (no regressions)

## Task Commits

Each task was committed atomically:

1. **Task 1: Julia export_csv with build_csv_options** - `3a8cfd0` (feat)
2. **Task 2: Dart exportCSV with arena-based options** - `bc027ed` (feat)

## Files Created/Modified
- `bindings/julia/src/database_csv.jl` - Full export_csv with build_csv_options marshaling helper
- `bindings/julia/src/database.jl` - Fixed build_options kwargs pattern for Julia 1.12
- `bindings/julia/test/test_database_csv.jl` - 19 CSV export tests (scalar, vector, enum, date, combined)
- `bindings/dart/lib/src/database_csv.dart` - Full exportCSV with arena-based options marshaling
- `bindings/dart/test/database_csv_test.dart` - 5 CSV export tests (scalar, vector, enum, date, combined)

## Decisions Made
- Julia `build_csv_options` constructs a new immutable `quiver_csv_export_options_t` struct with all 6 positional fields (since the Julia struct is immutable, fields cannot be set individually via Ref)
- Dart constructs the options struct manually via Arena allocation (no `quiver_csv_export_options_default` call needed, fields set directly via `.ref.field = value`)
- Fixed Julia `build_options` to use keyword splatting `(; kwargs...)` instead of positional varargs -- required for Julia 1.12.4 compatibility where `haskey` on wrapped `Pairs` tuple fails

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed Julia build_options kwargs handling for Julia 1.12**
- **Found during:** Task 1 (Julia export_csv implementation)
- **Issue:** `build_options(kwargs...)` in `database.jl` uses positional varargs, wrapping kwargs in a Tuple. `haskey` fails on `Tuple{Base.Pairs}` in Julia 1.12.4
- **Fix:** Changed to `build_options(; kwargs...)` (keyword splatting) and updated all call sites to `build_options(; kwargs...)`
- **Files modified:** `bindings/julia/src/database.jl`
- **Verification:** All 437 Julia tests pass including existing lifecycle/create tests that call from_schema
- **Committed in:** `3a8cfd0` (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Auto-fix was necessary to unblock Julia execution. No scope creep. The fix is backward-compatible.

## Issues Encountered
None beyond the auto-fixed deviation.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Julia and Dart CSV export wrappers complete and tested
- Ready for Plan 2: Lua CSV export bindings
- Both bindings produce byte-identical output for same inputs (verified via matching test assertions against C++ expected output)

## Self-Check: PASSED

- All 5 created/modified files verified on disk
- Both task commits (3a8cfd0, bc027ed) verified in git log
- 437 Julia tests pass, 252 Dart tests pass

---
*Phase: 07-bindings*
*Completed: 2026-02-22*
