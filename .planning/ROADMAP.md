# Roadmap: Quiver v0.5

## Overview

Milestone v0.5 brings the JS/Bun binding to full parity with Julia, Dart, and Python bindings. Starting with prerequisite C API fixes and dead code cleanup, we progressively build out CRUD, reads, metadata, time series, CSV, introspection, Lua, blob, and convenience operations -- each phase delivering a testable, complete capability slice.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Fixes & Cleanup** - Fix C API bool inconsistency and delete dead code
- [x] **Phase 2: Update & Collection Reads** - JS update element + vector/set bulk and by-id reads (completed 2026-03-10)
- [x] **Phase 3: Metadata** - JS get/list metadata for all attribute types (completed 2026-03-10)
- [x] **Phase 4: Time Series** - JS time series read, update, and files operations (completed 2026-03-10)
- [ ] **Phase 5: CSV I/O** - JS CSV export and import
- [ ] **Phase 6: Introspection & Lua** - JS database introspection and LuaRunner binding
- [ ] **Phase 7: Blob Subsystem** - JS blob file I/O, CSV conversion, and metadata
- [ ] **Phase 8: Convenience Composites** - JS composite read helpers (readScalarsById, etc.)

## Phase Details

### Phase 1: Fixes & Cleanup
**Goal**: C API boolean out-params are consistent and dead code is removed
**Depends on**: Nothing (first phase)
**Requirements**: CAPI-01, CLEAN-01
**Success Criteria** (what must be TRUE):
  1. `quiver_database_in_transaction` uses `int*` out-param, and all existing bindings (Julia, Dart, Python, JS) pass their `in_transaction` tests with the new signature
  2. `src/blob/dimension.cpp` no longer exists in the repository
  3. JS binding `inTransaction()` uses `Int32Array(1)` (not `Uint8Array(1)`)
**Plans:** 1 plan

Plans:
- [ ] 01-01-PLAN.md -- Fix in_transaction bool*/int* across C API and all bindings, delete dead code

### Phase 2: Update & Collection Reads
**Goal**: JS users can modify elements and read vector/set data in bulk and by element ID
**Depends on**: Phase 1
**Requirements**: JSUP-01, JSRD-01, JSRD-02, JSRD-03, JSRD-04
**Success Criteria** (what must be TRUE):
  1. User can call `db.updateElement(collection, id, data)` in JS and verify the updated values persist
  2. User can call `db.readVectorIntegers/Floats/Strings(collection, attribute)` and receive all elements' vector data
  3. User can call `db.readVectorIntegers/Floats/StringsById(collection, attribute, id)` and receive a single element's vector data
  4. User can call `db.readSetIntegers/Floats/Strings(collection, attribute)` and receive all elements' set data
  5. User can call `db.readSetIntegers/Floats/StringsById(collection, attribute, id)` and receive a single element's set data
**Plans:** 1/1 plans complete

Plans:
- [ ] 02-01-PLAN.md -- Add updateElement, vector/set bulk reads, and vector/set by-id reads to JS binding

### Phase 3: Metadata
**Goal**: JS users can inspect schema metadata for all attribute types
**Depends on**: Phase 2
**Requirements**: JSMETA-01, JSMETA-02
**Success Criteria** (what must be TRUE):
  1. User can call `db.getScalarMetadata/getVectorMetadata/getSetMetadata/getTimeSeriesMetadata(collection, name)` and receive attribute metadata (type, nullable, etc.)
  2. User can call `db.listScalarAttributes/listVectorGroups/listSetGroups/listTimeSeriesGroups(collection)` and receive the list of available attributes/groups
**Plans:** 1/1 plans complete

Plans:
- [x] 03-01-PLAN.md -- Add 8 metadata methods (4 get + 4 list) with struct deserialization to JS binding

### Phase 4: Time Series
**Goal**: JS users can read and write time series data and manage time series file references
**Depends on**: Phase 3
**Requirements**: JSTS-01, JSTS-02, JSTS-03
**Success Criteria** (what must be TRUE):
  1. User can call `db.readTimeSeriesGroup(collection, group, id)` and receive multi-column time series rows
  2. User can call `db.updateTimeSeriesGroup(collection, group, id, data)` to write time series rows and verify them with a subsequent read
  3. User can call `db.hasTimeSeriesFiles/listTimeSeriesFilesColumns/readTimeSeriesFiles/updateTimeSeriesFiles` to manage time series file path references
**Plans:** 1/1 plans complete

Plans:
- [ ] 04-01-PLAN.md -- Add 6 time series methods (read/update group + 4 files CRUD) with columnar FFI marshaling to JS binding

### Phase 5: CSV I/O
**Goal**: JS users can export collections to CSV and import CSV data into collections
**Depends on**: Phase 2
**Requirements**: JSCSV-01, JSCSV-02
**Success Criteria** (what must be TRUE):
  1. User can call `db.exportCsv(collection, filePath, options?)` and a valid CSV file is produced on disk
  2. User can call `db.importCsv(collection, filePath, options?)` to load CSV data into a collection, and the imported data is readable via existing read operations
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

### Phase 6: Introspection & Lua
**Goal**: JS users can inspect database state and execute Lua scripts
**Depends on**: Phase 1
**Requirements**: JSDB-01, JSLUA-01
**Success Criteria** (what must be TRUE):
  1. User can call `db.isHealthy()`, `db.currentVersion()`, `db.path()`, and `db.describe()` and receive correct results
  2. User can create a `LuaRunner` in JS, execute a Lua script that modifies the database, and verify the changes via JS reads
**Plans**: TBD

Plans:
- [ ] 06-01: TBD

### Phase 7: Blob Subsystem
**Goal**: JS users can work with blob binary files, convert between binary and CSV, and manage blob metadata
**Depends on**: Phase 1
**Requirements**: JSBLOB-01, JSBLOB-02, JSBLOB-03
**Success Criteria** (what must be TRUE):
  1. User can open a blob file for writing, write float data with dimension indexing, close it, reopen for reading, and read back the same data
  2. User can call `binToCsv` and `csvToBin` to convert between blob binary files and CSV representation
  3. User can create blob metadata with dimensions (including time dimensions), serialize to TOML, deserialize from TOML, and create metadata from a database element
**Plans**: TBD

Plans:
- [ ] 07-01: TBD

### Phase 8: Convenience Composites
**Goal**: JS users can read all attributes for a single element in one call
**Depends on**: Phase 3
**Requirements**: JSCONV-01
**Success Criteria** (what must be TRUE):
  1. User can call `db.readScalarsById(collection, id)` and receive a dictionary of all scalar attribute values for that element
  2. User can call `db.readVectorsById(collection, id)` and `db.readSetsById(collection, id)` and receive grouped collection data for that element
**Plans**: TBD

Plans:
- [ ] 08-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8

Note: Phases 5, 6, and 7 depend only on Phases 1-2 (not on each other) and could execute in any order after their dependencies are met. The linear ordering is a default; plan-phase may parallelize.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Fixes & Cleanup | 0/1 | Not started | - |
| 2. Update & Collection Reads | 1/1 | Complete   | 2026-03-10 |
| 3. Metadata | 1/1 | Complete   | 2026-03-10 |
| 4. Time Series | 1/1 | Complete   | 2026-03-10 |
| 5. CSV I/O | 0/? | Not started | - |
| 6. Introspection & Lua | 0/? | Not started | - |
| 7. Blob Subsystem | 0/? | Not started | - |
| 8. Convenience Composites | 0/? | Not started | - |
