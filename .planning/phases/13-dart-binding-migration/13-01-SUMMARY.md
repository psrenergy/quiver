---
phase: 13-dart-binding-migration
plan: 01
subsystem: bindings
tags: [dart, ffi, time-series, columnar, map, datetime]

# Dependency graph
requires:
  - phase: 11-c-api-multi-column-time-series
    provides: Multi-column typed C API for time series read/update/free
  - phase: 12-julia-binding-migration
    provides: Julia binding migration as behavioral reference
provides:
  - Dart updateTimeSeriesGroup with Map-based columnar marshaling
  - Dart readTimeSeriesGroup with typed columnar unmarshaling and DateTime dimension parsing
  - Regenerated FFI bindings with new 9-arg time series signatures
affects: [13-02-dart-binding-tests]

# Tech tracking
tech-stack:
  added: []
  patterns: [Map-to-columnar marshaling, columnar-to-Map unmarshaling, strict type enforcement]

key-files:
  created: []
  modified:
    - bindings/dart/lib/src/ffi/bindings.dart
    - bindings/dart/lib/src/database_update.dart
    - bindings/dart/lib/src/database_read.dart

key-decisions:
  - "Map<String, List<Object>> for both update parameter and read return type (symmetry)"
  - "Strict type enforcement -- no auto-coercion, throw on unsupported types"
  - "DateTime auto-formatting on update, auto-parsing on read dimension column via metadata"

patterns-established:
  - "Map-to-columnar: convert Dart Map to parallel C arrays (names, types, data) in Arena"
  - "Columnar-to-Map: switch on column_types for typed unmarshaling, dimension column gets DateTime parsing"

requirements-completed: [BIND-03, BIND-04]

# Metrics
duration: 3min
completed: 2026-02-20
---

# Phase 13 Plan 01: Dart Time Series Binding Migration Summary

**Regenerated Dart FFI bindings and rewrote updateTimeSeriesGroup/readTimeSeriesGroup for multi-column C API with Map-based columnar interface and DateTime dimension parsing**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-20T00:42:02Z
- **Completed:** 2026-02-20T00:45:18Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Regenerated bindings.dart with new 9-arg signatures for multi-column time series read/update/free
- Rewrote updateTimeSeriesGroup to accept Map<String, List<Object>> with strict type enforcement, DateTime support, empty-map clear semantics, and row count validation
- Rewrote readTimeSeriesGroup to return Map<String, List<Object>> with typed columns (List<int>, List<double>, List<String>, List<DateTime> for dimension column)

## Task Commits

Each task was committed atomically:

1. **Task 1: Regenerate FFI bindings and rewrite updateTimeSeriesGroup** - `f6856ac` (feat)
2. **Task 2: Rewrite readTimeSeriesGroup to return Map with typed columns** - `84942d2` (feat)

## Files Created/Modified
- `bindings/dart/lib/src/ffi/bindings.dart` - Regenerated FFI bindings with 9-arg time series signatures
- `bindings/dart/lib/src/database_update.dart` - Map-based updateTimeSeriesGroup with columnar marshaling
- `bindings/dart/lib/src/database_read.dart` - Map-based readTimeSeriesGroup with typed columnar unmarshaling

## Decisions Made
- Used `Map<String, List<Object>>` for both update parameter and read return type for API symmetry
- Strict type enforcement (no auto-coercion) per user decision -- int, double, String, DateTime only
- DateTime auto-formatting on update via dateTimeToString(), auto-parsing on read dimension column via stringToDateTime()
- Row count validation in Dart binding (C API trusts row_count parameter)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Dart time series binding methods are rewritten and compile cleanly
- Plan 13-02 can proceed with test rewrites using the new Map-based interface
- Existing time series tests need migration from old List<Map> to new Map<String, List> interface

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 13-dart-binding-migration*
*Completed: 2026-02-20*
