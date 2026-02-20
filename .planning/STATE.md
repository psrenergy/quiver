# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-11)

**Core value:** Every public C++ method is reachable from every binding through uniform, predictable patterns
**Current focus:** v1.1 Time Series Ergonomics -- Phase 13 In Progress

## Current Position

Phase: 13 of 14 (Dart Binding Migration)
Plan: 1 of 2 in current phase
Status: Plan 13-01 Complete
Last activity: 2026-02-20 -- Completed 13-01 (Dart FFI regeneration + time series method rewrite)

Progress: [█████░░░░░] 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 15 (v1.0)
- Average duration: ~12 min (v1.0)
- Total execution time: ~3 hours (v1.0)

**By Phase (v1.0):**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1-10 | 15 | ~3h | ~12m |

**By Phase (v1.1):**

| Phase | Plan | Duration | Tasks | Files |
|-------|------|----------|-------|-------|
| 11 | 01 | 5min | 2 | 7 |
| 11 | 02 | 5min | 2 | 1 |
| 12 | 01 | 3min | 2 | 3 |
| 12 | 02 | 6min | 2 | 2 |
| 13 | 01 | 3min | 2 | 3 |

*v1.1 metrics start fresh at Phase 11*

## Accumulated Context

### Decisions

See PROJECT.md Key Decisions table for full log with outcomes.

Recent decisions affecting current work:
- v1.0: Alloc/free co-location in C API (read + free in same translation unit)
- v1.0: 3 canonical error message patterns (Cannot/Not found/Failed to)
- v1.1: Columnar typed-arrays pattern (reuse convert_params() approach from database_query.cpp)
- v1.1: Split QUIVER_REQUIRE into two calls for 9-arg functions (readability over single macro call)
- v1.1: c_type_name() helper is file-local static in database_time_series.cpp (single use site)
- v1.1: Dimension column stored as QUIVER_DATA_TYPE_STRING in schema lookup map
- v1.1: Value columns returned in alphabetical order (std::map in TableDefinition), not CREATE TABLE definition order
- v1.1: Partial column updates fail on NOT NULL schemas (SQLite constraint enforced at C++ layer)
- v1.1: Dict{String, Vector} with abstract Vector value type for Julia read_time_series_group return
- v1.1: Single refs::Vector{Any} collector for GC.@preserve in kwargs-to-columnar marshaling
- v1.1: Metadata fetch per update call for auto-coercion (Int->Float when schema expects REAL)
- v1.1: Fixed test_database_create.jl time series assertions as blocking deviation during test migration
- v1.1: Map<String, List<Object>> for both Dart update parameter and read return type (API symmetry)
- v1.1: Strict type enforcement in Dart (no auto-coercion, unlike Julia)

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-20
Stopped at: Completed 13-01-PLAN.md
Resume file: None
