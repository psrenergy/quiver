# Requirements: Quiver

**Defined:** 2026-03-09
**Core Value:** Every public C++ method is accessible from every supported language binding with consistent naming, identical behavior, and comprehensive tests.

## v0.5 Requirements

Requirements for JS binding parity milestone. Each maps to roadmap phases.

### C API Fix

- [ ] **CAPI-01**: `quiver_database_in_transaction` uses `int*` instead of `bool*` for consistency with all other boolean out-params

### JS Update

- [ ] **JSUP-01**: User can update an element via `updateElement(collection, id, data)`

### JS Vector/Set Reads

- [ ] **JSRD-01**: User can read vector integers/floats/strings (bulk)
- [ ] **JSRD-02**: User can read vector integers/floats/strings by element ID
- [ ] **JSRD-03**: User can read set integers/floats/strings (bulk)
- [ ] **JSRD-04**: User can read set integers/floats/strings by element ID

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

- [ ] **CLEAN-01**: Delete empty `src/blob/dimension.cpp`

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
| CAPI-01 | -- | Pending |
| JSUP-01 | -- | Pending |
| JSRD-01 | -- | Pending |
| JSRD-02 | -- | Pending |
| JSRD-03 | -- | Pending |
| JSRD-04 | -- | Pending |
| JSMETA-01 | -- | Pending |
| JSMETA-02 | -- | Pending |
| JSTS-01 | -- | Pending |
| JSTS-02 | -- | Pending |
| JSTS-03 | -- | Pending |
| JSCSV-01 | -- | Pending |
| JSCSV-02 | -- | Pending |
| JSDB-01 | -- | Pending |
| JSLUA-01 | -- | Pending |
| JSBLOB-01 | -- | Pending |
| JSBLOB-02 | -- | Pending |
| JSBLOB-03 | -- | Pending |
| JSCONV-01 | -- | Pending |
| CLEAN-01 | -- | Pending |

**Coverage:**
- v0.5 requirements: 20 total
- Mapped to phases: 0
- Unmapped: 20

---
*Requirements defined: 2026-03-09*
*Last updated: 2026-03-09 after initial definition*
