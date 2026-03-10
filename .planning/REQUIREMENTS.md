# Requirements: Quiver

**Defined:** 2026-03-09
**Core Value:** Every public C++ method is accessible from every supported language binding with consistent naming, identical behavior, and comprehensive tests.

## v0.5 Requirements

Requirements for JS binding parity milestone. Each maps to roadmap phases.

### C API Fix

- [x] **CAPI-01**: `quiver_database_in_transaction` uses `int*` instead of `bool*` for consistency with all other boolean out-params

### JS Update

- [x] **JSUP-01**: User can update an element via `updateElement(collection, id, data)`

### JS Vector/Set Reads

- [x] **JSRD-01**: User can read vector integers/floats/strings (bulk)
- [x] **JSRD-02**: User can read vector integers/floats/strings by element ID
- [x] **JSRD-03**: User can read set integers/floats/strings (bulk)
- [x] **JSRD-04**: User can read set integers/floats/strings by element ID

### JS Metadata

- [ ] **JSMETA-01**: User can get scalar/vector/set/time-series metadata
- [ ] **JSMETA-02**: User can list scalar attributes and vector/set/time-series groups

### JS Time Series

- [ ] **JSTS-01**: User can read time series group data
- [ ] **JSTS-02**: User can update time series group data
- [ ] **JSTS-03**: User can check/list/read/update time series files

### JS CSV

- [ ] **JSCSV-01**: User can export a collection/group to CSV
- [ ] **JSCSV-02**: User can import a CSV into a collection/group

### JS Introspection

- [ ] **JSDB-01**: User can call isHealthy, currentVersion, path, describe

### JS LuaRunner

- [ ] **JSLUA-01**: User can execute Lua scripts against a database

### JS Blob

- [ ] **JSBLOB-01**: User can open/read/write/close blob files
- [ ] **JSBLOB-02**: User can convert between blob binary and CSV
- [ ] **JSBLOB-03**: User can create/inspect/serialize blob metadata

### JS Convenience

- [ ] **JSCONV-01**: User can read all scalars/vectors/sets for an element by ID

### Cleanup

- [x] **CLEAN-01**: Delete empty `src/blob/dimension.cpp`

## Future Requirements

None — this milestone brings JS to parity. Future milestones may add new core features.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Node.js support | JS binding targets Bun only (uses bun:ffi) |
| New C++ core features | v0.5 is binding parity, not new functionality |
| Dart/Python blob gaps | Separate milestone if needed |
| JS blob time dimensions | Complex time dimension logic deferred if Bun FFI limitations arise |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CAPI-01 | Phase 1 | Complete |
| CLEAN-01 | Phase 1 | Complete |
| JSUP-01 | Phase 2 | Complete |
| JSRD-01 | Phase 2 | Complete |
| JSRD-02 | Phase 2 | Complete |
| JSRD-03 | Phase 2 | Complete |
| JSRD-04 | Phase 2 | Complete |
| JSMETA-01 | Phase 3 | Pending |
| JSMETA-02 | Phase 3 | Pending |
| JSTS-01 | Phase 4 | Pending |
| JSTS-02 | Phase 4 | Pending |
| JSTS-03 | Phase 4 | Pending |
| JSCSV-01 | Phase 5 | Pending |
| JSCSV-02 | Phase 5 | Pending |
| JSDB-01 | Phase 6 | Pending |
| JSLUA-01 | Phase 6 | Pending |
| JSBLOB-01 | Phase 7 | Pending |
| JSBLOB-02 | Phase 7 | Pending |
| JSBLOB-03 | Phase 7 | Pending |
| JSCONV-01 | Phase 8 | Pending |

**Coverage:**
- v0.5 requirements: 20 total
- Mapped to phases: 20
- Unmapped: 0

---
*Requirements defined: 2026-03-09*
*Last updated: 2026-03-09 after roadmap creation*
