# Roadmap: Quiver v0.4

## Overview

v0.4 delivers Python bindings via CFFI and cross-layer test parity. The work builds from infrastructure (DLL loading, error handling, package setup) through increasing FFI complexity (scalar reads, vector/set marshaling, parameterized queries, time series void** dispatch) and culminates in CSV export, composite helpers, and a full test parity audit across all five language layers. Every phase is independently verifiable before the next begins.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Foundation** - Package setup, DLL loading, error handling, lifecycle, and test scaffolding (completed 2026-02-23)
- [x] **Phase 2: Reads and Metadata** - All scalar/vector/set read operations plus metadata queries with correct memory management (completed 2026-02-23)
- [x] **Phase 3: Writes and Transactions** - All CRUD write operations, scalar/vector/set updates, relation writes, and transaction control (completed 2026-02-23)
- [ ] **Phase 4: Queries and Relations** - Parameterized SQL queries with keepalive marshaling and relation read operations
- [ ] **Phase 5: Time Series** - Multi-column void** read/write dispatch and time series files operations
- [ ] **Phase 6: CSV and Convenience Helpers** - CSV export with struct marshaling and pure-Python composite read helpers
- [ ] **Phase 7: Test Parity** - Comprehensive test coverage audit and gap-fill across C++, C API, Julia, Dart, and Python

## Phase Details

### Phase 1: Foundation
**Goal**: The Python binding is installable, the DLL loads on all platforms, errors surface as Python exceptions, and the Database and Element lifecycle works end-to-end
**Depends on**: Nothing (first phase)
**Requirements**: INFRA-01, INFRA-02, INFRA-03, INFRA-04, INFRA-05, INFRA-06, LIFE-01, LIFE-02, LIFE-03, LIFE-04, LIFE-05, LIFE-06
**Success Criteria** (what must be TRUE):
  1. `import quiver` succeeds and `quiver.version()` returns the library version string
  2. `Database.from_schema()` and `Database.from_migrations()` open a database; `with Database.from_schema(...) as db:` closes it automatically on exit
  3. A C API error (e.g., opening a nonexistent schema) raises `QuiverError` with the exact C++ error message
  4. `Element().set("label", "x").set("value", 42)` constructs an element usable in C API calls without memory errors
  5. `test.bat` runs the Python test suite with correct PATH setup and all lifecycle tests pass
**Plans**: 2 plans
Plans:
- [ ] 01-01-PLAN.md -- Package scaffolding, CFFI declarations, DLL loader, error handling, string helpers, version()
- [ ] 01-02-PLAN.md -- Database class, Element class, test runner, lifecycle tests

### Phase 2: Reads and Metadata
**Goal**: Every read operation over scalars, vectors, sets, and element IDs works correctly with no memory leaks, and metadata queries return typed Python dataclasses
**Depends on**: Phase 1
**Requirements**: READ-01, READ-02, READ-03, READ-04, READ-05, READ-06, READ-07, READ-08, META-01, META-02, META-03
**Success Criteria** (what must be TRUE):
  1. `read_scalar_integers(collection, attribute)` returns a Python list of ints and frees all C-allocated memory before returning (try/finally pattern)
  2. `read_scalar_integer_by_id(collection, id, attribute)` returns an `int` or `None` for absent optional values
  3. `read_vector_integers_by_id(collection, id, attribute)` and `read_set_strings_by_id(collection, id, attribute)` return correctly typed Python lists
  4. `get_scalar_metadata(collection, attribute)` returns a `ScalarMetadata` dataclass with correct field values; `get_vector_metadata` returns a `GroupMetadata` dataclass
  5. `list_scalar_attributes(collection)` and `list_vector_groups(collection)` return Python lists of strings; `read_scalar_relation(collection, id, attribute)` returns the related element ID
**Plans**: 2 plans
Plans:
- [ ] 02-01-PLAN.md -- CFFI declarations, metadata dataclasses, scalar/by-id reads, element IDs, relation reads
- [ ] 02-02-PLAN.md -- Vector/set bulk and by-id reads, metadata get/list operations

### Phase 3: Writes and Transactions
**Goal**: Elements can be created, updated, and deleted; scalar/vector/set attributes and relations can be updated; and explicit transactions and the transaction context manager work correctly
**Depends on**: Phase 2
**Requirements**: WRITE-01, WRITE-02, WRITE-03, WRITE-04, WRITE-05, WRITE-06, WRITE-07, TXN-01, TXN-02
**Success Criteria** (what must be TRUE):
  1. `create_element(collection, element)` creates a new element and returns its integer ID
  2. `update_element(collection, element)` updates an existing element; `delete_element(collection, id)` removes it
  3. `update_scalar_integer(collection, id, attribute, value)` persists the updated value; `update_vector_strings(collection, id, attribute, values)` replaces the vector; `update_set_integers(collection, id, attribute, values)` replaces the set
  4. `update_scalar_relation(collection, id, attribute, related_id)` links two elements
  5. `begin_transaction()` / `commit()` / `rollback()` control transaction state; `with db.transaction():` commits on success and rolls back on exception
**Plans**: 2 plans
Plans:
- [x] 03-01-PLAN.md -- CFFI write declarations, update/delete element, scalar/vector/set update methods, create/update/delete tests
- [x] 03-02-PLAN.md -- Transaction CFFI declarations, begin/commit/rollback/in_transaction, context manager, transaction tests

### Phase 4: Queries and Relations
**Goal**: Parameterized SQL queries return correctly typed Python results with no GC-premature-free bugs, completing the query surface
**Depends on**: Phase 3
**Requirements**: QUERY-01, QUERY-02
**Success Criteria** (what must be TRUE):
  1. `query_string(sql)` executes a SQL SELECT and returns a list of strings
  2. `query_integer_params(sql, params)` with mixed int/float/str/None parameters returns correct integer results without segfault or premature GC of param buffers
  3. `query_string_params(sql, params)` with a `None` parameter correctly marshals NULL to the C API and returns expected results
**Plans**: 1 plan
Plans:
- [ ] 04-01-PLAN.md -- CFFI query declarations, _marshal_params helper, 4 unified query methods, query tests

### Phase 5: Time Series
**Goal**: Multi-column time series groups can be read and written using void** column dispatch by type, and time series file references can be read and updated
**Depends on**: Phase 4
**Requirements**: TS-01, TS-02, TS-03
**Success Criteria** (what must be TRUE):
  1. `read_time_series_group(collection, id, group)` returns a list of row dicts with correct Python types per column (int for INTEGER, float for FLOAT, str for STRING/DATE_TIME)
  2. `update_time_series_group(collection, id, group, rows)` persists rows correctly; calling with empty rows clears all rows for that group
  3. `has_time_series_files(collection)` returns a bool; `read_time_series_files(collection)` returns a dict of file paths; `update_time_series_files(collection, data)` persists changes
**Plans**: 2 plans
Plans:
- [ ] 05-01-PLAN.md -- CFFI declarations, read/update time series group with void** dispatch, group tests
- [ ] 05-02-PLAN.md -- Time series files methods (has/list/read/update), files tests

### Phase 6: CSV and Convenience Helpers
**Goal**: CSV export works with all CSVExportOptions fields correctly marshaled, and composite read helpers compose existing operations into single-call results
**Depends on**: Phase 5
**Requirements**: CSV-01, CSV-02, CONV-01, CONV-02, CONV-03, CONV-04, CONV-05
**Success Criteria** (what must be TRUE):
  1. `export_csv(collection, path, options)` writes a CSV file with correct scalar values, enum labels, and date formatting per the provided options
  2. `read_all_scalars_by_id(collection, id)` returns a dict of all scalar attribute values without the caller needing to know attribute names
  3. `read_all_vectors_by_id(collection, id)` and `read_all_sets_by_id(collection, id)` return dicts of all group values for a given element
  4. `read_vector_group_by_id(collection, id, group)` and `read_set_group_by_id(collection, id, group)` return lists of row dicts from multi-column group reads

**Coverage Note**: CSV-02 (`import_csv`) — research indicates this is not implemented in the C++ layer. Verify the C API before writing the binding. If unimplemented in C++, drop CSV-02 from scope and update REQUIREMENTS.md to defer it.
**Plans**: TBD

### Phase 7: Test Parity
**Goal**: Every language layer has test coverage for each functional area, with all gaps identified and filled, and the Python test suite matches Julia/Dart in structure and depth
**Depends on**: Phase 6
**Requirements**: TEST-01, TEST-02, TEST-03, TEST-04, TEST-05
**Success Criteria** (what must be TRUE):
  1. Python test suite has one test file per functional area (lifecycle, create, read, update, delete, query, relations, time series, transaction, CSV, metadata) and all tests pass
  2. C++ and C API coverage audits are complete; any functional areas covered by binding tests but absent from C++ or C API tests have new test files added
  3. Julia and Dart audits are complete; any coverage gaps relative to the Python or C++ layer have new tests added
  4. Running all five test suites (C++, C API, Julia, Dart, Python) produces zero failures
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 0/2 | Complete    | 2026-02-23 |
| 2. Reads and Metadata | 0/2 | Complete | 2026-02-23 |
| 3. Writes and Transactions | 0/2 | Not started | - |
| 4. Queries and Relations | 0/1 | Not started | - |
| 5. Time Series | 0/? | Not started | - |
| 6. CSV and Convenience Helpers | 0/? | Not started | - |
| 7. Test Parity | 0/? | Not started | - |
