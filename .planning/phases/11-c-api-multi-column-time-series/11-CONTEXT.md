# Phase 11: C API Multi-Column Time Series - Context

**Gathered:** 2026-02-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Redesign the C API time series functions (`update_time_series_group`, `read_time_series_group`, `free_time_series_data`) to support N typed value columns instead of hardcoded single-column `(date_times, values)`. Update all existing callers (C API tests, bindings) to the new signatures. The C++ layer is unchanged -- only C API marshaling changes.

</domain>

<decisions>
## Implementation Decisions

### Dimension column treatment
- Dimension column (date_time) is unified -- treated as just another column in `column_names[]`/`column_data[]` arrays, not a separate parameter
- C API discovers the dimension column from metadata internally (calls `get_time_series_metadata()`) -- caller passes columns in any order
- Dimension column is required: if caller omits it, return `QUIVER_ERROR` with message like `"Cannot update_time_series_group: dimension column 'date_time' missing from column_names"`
- Partial column updates allowed: caller can pass a subset of value columns (only dimension is mandatory)

### Function naming
- Replace existing functions in-place with new signatures (no `_multi`/`_v2` suffixes, no coexistence with old signatures)
- `quiver_database_update_time_series_group` -- new multi-column signature replaces old
- `quiver_database_read_time_series_group` -- new multi-column signature replaces old
- `quiver_database_free_time_series_data` -- new signature with `column_types` for type-aware deallocation
- All existing callers (C API tests, Julia bindings, Dart bindings) updated in their respective phases
- Binding names unchanged: Julia `read_time_series_group` / Dart `readTimeSeriesGroup` keep their names with new multi-column behavior

### Read result layout
- Read returns column names alongside data (self-describing result, no extra metadata call needed by caller)
- Dimension column included as a regular column in output arrays (mirrors unified update approach)
- Column order matches schema definition order (as returned by metadata)
- Empty results: NULL pointers with count 0 (matches existing empty-result convention)

### Schema validation errors
- Fail-fast: error on first bad column found
- Unknown column name: `"Cannot update_time_series_group: column 'temperature' not found in group 'data' for collection 'Items'"`
- Type mismatch: `"Cannot update_time_series_group: column 'status' has type TEXT but received INTEGER"`
- Follows existing error patterns (Pattern 1: `"Cannot {operation}: {reason}"`)

### Claude's Discretion
- Read result format: parallel out-arrays vs result struct (user said "you decide")
- Exact parameter order in new function signatures
- Columnar-to-row conversion implementation details
- Atomic allocation strategy for read-side leak prevention
- Helper function placement in `database_helpers.h`

</decisions>

<specifics>
## Specific Ideas

- Replace in-place means Phase 11 must update C API tests to new signatures (no backward compatibility period)
- Julia and Dart binding callers update in Phases 12 and 13 respectively, but the C API header changes land in Phase 11
- The `convert_params()` pattern from `database_query.cpp` is the established internal precedent for typed parallel arrays through FFI

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 11-c-api-multi-column-time-series*
*Context gathered: 2026-02-12*
