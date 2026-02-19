# Requirements: Quiver v1.1 Time Series Ergonomics

**Defined:** 2026-02-12
**Core Value:** Every public C++ method is reachable from every binding through uniform, predictable patterns

## v1.1 Requirements

Requirements for time series update interface redesign. Each maps to roadmap phases.

### C API Redesign

- [ ] **CAPI-01**: C API supports N typed value columns for time series update (columnar parallel arrays: column_names[], column_types[], column_data[], column_count, row_count)
- [ ] **CAPI-02**: C API supports N typed value columns for time series read (returns columnar typed arrays matching schema)
- [ ] **CAPI-03**: Free function correctly deallocates variable-column read results (string columns: per-element cleanup; numeric columns: single delete[])
- [ ] **CAPI-04**: Column types use existing quiver_data_type_t enum (INTEGER, FLOAT, STRING) with no new type definitions
- [ ] **CAPI-05**: C API validates column names and types against schema metadata before processing, returning clear error messages on mismatch

### Binding Ergonomics

- [ ] **BIND-01**: Julia update_time_series_group! accepts kwargs: `update_time_series_group!(db, col, group, id; date_time=[...], val=[...])`
- [ ] **BIND-02**: Julia read_time_series_group returns multi-column data with correct types per column
- [ ] **BIND-03**: Dart updateTimeSeriesGroup accepts Map: `updateTimeSeriesGroup(col, grp, id, {'date_time': [...], 'temp': [...]})`
- [ ] **BIND-04**: Dart readTimeSeriesGroup returns multi-column data with correct types per column
- [ ] **BIND-05**: Lua multi-column time series operations have test coverage (implementation already works via sol2)

### Migration Safety

- [ ] **MIGR-01**: Multi-column test schema with mixed types (INTEGER + REAL + TEXT value columns) in tests/schemas/valid/
- [ ] **MIGR-02**: All existing 1,213+ tests continue passing throughout migration
- [ ] **MIGR-03**: Old C API functions removed only after all bindings migrated to new interface

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Time Series Extensions

- **TSXT-01**: Append semantics (add rows without replacing existing)
- **TSXT-02**: Partial column update (update only specific columns, leave others unchanged)
- **TSXT-03**: Time range filtering on update (replace only rows in date range X-Y)
- **TSXT-04**: Streaming/chunked update for large datasets

## Out of Scope

| Feature | Reason |
|---------|--------|
| C++ layer changes | Already supports multi-column via vector<map<string, Value>> |
| New database operations | This milestone is interface redesign, not new features |
| Lua binding migration | Lua already works for multi-column via sol2 direct C++ binding |
| Named parameters in Dart | Dart named params are compile-time; column names are runtime. Map is correct. |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CAPI-01 | Phase 11 | Pending |
| CAPI-02 | Phase 11 | Pending |
| CAPI-03 | Phase 11 | Pending |
| CAPI-04 | Phase 11 | Pending |
| CAPI-05 | Phase 11 | Pending |
| BIND-01 | Phase 12 | Pending |
| BIND-02 | Phase 12 | Pending |
| BIND-03 | Phase 13 | Pending |
| BIND-04 | Phase 13 | Pending |
| BIND-05 | Phase 14 | Pending |
| MIGR-01 | Phase 11 | Pending |
| MIGR-02 | Phase 14 | Pending |
| MIGR-03 | Phase 14 | Pending |

**Coverage:**
- v1.1 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0

---
*Requirements defined: 2026-02-12*
*Last updated: 2026-02-12 after roadmap creation*
