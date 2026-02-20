---
phase: 12-julia-binding-migration
plan: 01
subsystem: bindings
tags: [julia, ffi, time-series, kwargs, columnar, datetime]

# Dependency graph
requires:
  - phase: 11-c-api-multi-column-time-series
    provides: Multi-column typed C API for time series read/update/free
provides:
  - Julia update_time_series_group! with kwargs-to-columnar marshaling
  - Julia read_time_series_group with columnar-to-Dict unmarshaling
  - Regenerated c_api.jl with new 9-arg time series signatures
affects: [12-02-julia-binding-tests]

# Tech tracking
tech-stack:
  added: []
  patterns: [kwargs-to-columnar marshaling, columnar-to-Dict unmarshaling, refs collector GC.@preserve pattern]

key-files:
  created: []
  modified:
    - bindings/julia/src/c_api.jl
    - bindings/julia/src/database_update.jl
    - bindings/julia/src/database_read.jl

key-decisions:
  - "Dict{String, Vector} with abstract Vector value type for simplicity over Union type"
  - "Single refs::Vector{Any} collector for GC.@preserve in update marshaling"
  - "Metadata fetch per update call for auto-coercion (Int->Float)"

patterns-established:
  - "Kwargs-to-columnar: convert Julia kwargs to parallel C arrays (names, types, data) with GC safety"
  - "Columnar-to-Dict: switch on column_types for typed unmarshaling, dimension column gets DateTime parsing"

requirements-completed: [BIND-01, BIND-02]

# Metrics
duration: 3min
completed: 2026-02-19
---

# Phase 12 Plan 01: Julia Time Series Binding Migration Summary

**Regenerated Julia FFI bindings and rewrote update_time_series_group!/read_time_series_group for multi-column C API with kwargs marshaling and typed Dict return**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-19T23:55:40Z
- **Completed:** 2026-02-19T23:58:36Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Regenerated c_api.jl with new 9-arg signatures for multi-column time series read/update/free
- Rewrote update_time_series_group! to accept kwargs with auto-coercion (Int->Float) and DateTime formatting
- Rewrote read_time_series_group to return Dict{String, Vector} with typed columns and DateTime dimension parsing

## Task Commits

Each task was committed atomically:

1. **Task 1: Regenerate c_api.jl and rewrite update_time_series_group!** - `aa91bd1` (feat)
2. **Task 2: Rewrite read_time_series_group to return Dict{String, Vector}** - `d408bae` (feat)

## Files Created/Modified
- `bindings/julia/src/c_api.jl` - Regenerated FFI bindings with 9-arg time series signatures
- `bindings/julia/src/database_update.jl` - Kwargs-based update_time_series_group! with auto-coercion
- `bindings/julia/src/database_read.jl` - Columnar read_time_series_group returning Dict{String, Vector}

## Decisions Made
- Used `Dict{String, Vector}` (abstract Vector) for return type simplicity -- runtime types are concrete (Vector{Int64}, etc.)
- Single `refs::Vector{Any}` collector pattern for GC.@preserve in update marshaling -- matches existing marshal_params pattern
- Metadata fetch per update call for auto-coercion -- required by user decision, C++ layer caches schema introspection

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Julia time series binding functions are rewritten and ready for testing
- Plan 12-02 can proceed with test rewrites using the new kwargs/columnar interface
- _get_value_data_type helper retained (used by read_all_vectors_by_id and read_all_sets_by_id)

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 12-julia-binding-migration*
*Completed: 2026-02-19*
