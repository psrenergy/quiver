# Roadmap: Quiver

## Milestones

- ✅ **v1.0 Quiver Refactoring** -- Phases 1-10 (shipped 2026-02-11)
- **v1.1 Time Series Ergonomics** -- Phases 11-14 (in progress)

## Phases

<details>
<summary>v1.0 Quiver Refactoring (Phases 1-10) -- SHIPPED 2026-02-11</summary>

- [x] Phase 1: C++ Impl Header Extraction (1/1 plans) -- 2026-02-09
- [x] Phase 2: C++ Core File Decomposition (2/2 plans) -- 2026-02-09
- [x] Phase 3: C++ Naming and Error Standardization (2/2 plans) -- 2026-02-10
- [x] Phase 4: C API File Decomposition (2/2 plans) -- 2026-02-10
- [x] Phase 5: C API Naming and Error Standardization (2/2 plans) -- 2026-02-10
- [x] Phase 6: Julia Bindings Standardization (1/1 plans) -- 2026-02-10
- [x] Phase 7: Dart Bindings Standardization (1/1 plans) -- 2026-02-10
- [x] Phase 8: Lua Bindings Standardization (1/1 plans) -- 2026-02-10
- [x] Phase 9: Code Hygiene (2/2 plans) -- 2026-02-10
- [x] Phase 10: Cross-Layer Documentation and Final Verification (1/1 plans) -- 2026-02-10

</details>

### v1.1 Time Series Ergonomics

**Milestone Goal:** Make `update_time_series_group` and `read_time_series_group` support N typed value columns across all layers, matching `create_element`'s columnar interface pattern.

- [ ] **Phase 11: C API Multi-Column Time Series** - Redesign C API to support N typed value columns for time series update, read, and free
- [ ] **Phase 12: Julia Binding Migration** - Julia kwargs interface for multi-column time series update and typed multi-column read
- [ ] **Phase 13: Dart Binding Migration** - Dart Map interface for multi-column time series update and typed multi-column read
- [ ] **Phase 14: Verification and Cleanup** - Remove old API, verify Lua coverage, full-stack test gate

## Phase Details

### Phase 11: C API Multi-Column Time Series
**Goal**: Developers can update and read multi-column time series data through the C API with correct types per column
**Depends on**: Phase 10 (v1.0 complete)
**Requirements**: CAPI-01, CAPI-02, CAPI-03, CAPI-04, CAPI-05, MIGR-01
**Success Criteria** (what must be TRUE):
  1. C API function accepts N typed value columns (column_names[], column_types[], column_data[], column_count, row_count) and writes multi-column time series data to the database
  2. C API function reads multi-column time series data back with correct types per column (integers as int64_t, floats as double, strings as char*)
  3. Free function correctly deallocates variable-column read results without leaks or corruption (string columns: per-element cleanup; numeric columns: single delete[])
  4. Column types use existing quiver_data_type_t enum with no new type definitions
  5. C API returns clear error message when column names or types do not match the schema metadata
**Plans**: 2 plans

Plans:
- [ ] 11-01-PLAN.md -- Multi-column C API implementation (schema, header, QUIVER_REQUIRE, update/read/free)
- [ ] 11-02-PLAN.md -- C API test rewrite and multi-column test coverage

### Phase 12: Julia Binding Migration
**Goal**: Julia users can update and read multi-column time series data using idiomatic kwargs syntax
**Depends on**: Phase 11
**Requirements**: BIND-01, BIND-02
**Success Criteria** (what must be TRUE):
  1. Julia user can call `update_time_series_group!(db, col, group, id; date_time=[...], val=[...])` with kwargs matching schema column names
  2. Julia user receives multi-column read results with correct native types per column (Int64 for INTEGER, Float64 for REAL, String for TEXT)
  3. Julia tests pass for multi-column time series schema with mixed types (INTEGER + REAL + TEXT value columns)
**Plans**: TBD

Plans:
- [ ] 12-01: TBD

### Phase 13: Dart Binding Migration
**Goal**: Dart users can update and read multi-column time series data using idiomatic Map interface
**Depends on**: Phase 11
**Requirements**: BIND-03, BIND-04
**Success Criteria** (what must be TRUE):
  1. Dart user can call `updateTimeSeriesGroup(col, grp, id, {'date_time': [...], 'temp': [...]})` with a Map of column names to typed arrays
  2. Dart user receives multi-column read results with correct native types per column (int for INTEGER, double for REAL, String for TEXT)
  3. Dart tests pass for multi-column time series schema with mixed types (INTEGER + REAL + TEXT value columns)
**Plans**: TBD

Plans:
- [ ] 13-01: TBD

### Phase 14: Verification and Cleanup
**Goal**: Old single-column C API functions are removed, all bindings verified against multi-column schemas, full test suite green
**Depends on**: Phase 12, Phase 13
**Requirements**: BIND-05, MIGR-02, MIGR-03
**Success Criteria** (what must be TRUE):
  1. Lua multi-column time series operations have passing test coverage using mixed-type schema
  2. Old single-column C API time series functions are removed from headers and implementation
  3. All 1,213+ tests pass across all 4 suites (C++, C API, Julia, Dart) via `scripts/build-all.bat`
**Plans**: TBD

Plans:
- [ ] 14-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 11 -> 12 -> 13 -> 14

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. C++ Impl Header Extraction | v1.0 | 1/1 | Complete | 2026-02-09 |
| 2. C++ Core File Decomposition | v1.0 | 2/2 | Complete | 2026-02-09 |
| 3. C++ Naming and Error Standardization | v1.0 | 2/2 | Complete | 2026-02-10 |
| 4. C API File Decomposition | v1.0 | 2/2 | Complete | 2026-02-10 |
| 5. C API Naming and Error Standardization | v1.0 | 2/2 | Complete | 2026-02-10 |
| 6. Julia Bindings Standardization | v1.0 | 1/1 | Complete | 2026-02-10 |
| 7. Dart Bindings Standardization | v1.0 | 1/1 | Complete | 2026-02-10 |
| 8. Lua Bindings Standardization | v1.0 | 1/1 | Complete | 2026-02-10 |
| 9. Code Hygiene | v1.0 | 2/2 | Complete | 2026-02-10 |
| 10. Cross-Layer Docs & Final Verification | v1.0 | 1/1 | Complete | 2026-02-10 |
| 11. C API Multi-Column Time Series | 1/2 | In Progress|  | - |
| 12. Julia Binding Migration | v1.1 | 0/0 | Not started | - |
| 13. Dart Binding Migration | v1.1 | 0/0 | Not started | - |
| 14. Verification and Cleanup | v1.1 | 0/0 | Not started | - |
