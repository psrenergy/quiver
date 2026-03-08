# Roadmap: Quiver v0.5 -- JavaScript/TypeScript Binding

## Overview

Deliver a Bun-native JS/TS binding for Quiver's C API, covering database lifecycle, CRUD, reads, queries, and transactions. The binding follows established patterns from existing Julia/Dart/Python wrappers: thin layer over `libquiver_c`, no logic in the binding, all intelligence in C++. Phases progress from FFI foundation through feature layers to npm packaging.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: FFI Foundation and Database Lifecycle** - Load native libraries, establish error handling and memory helpers, open and close databases
- [ ] **Phase 2: Element Builder and CRUD** - Create and delete elements with scalar and array attributes
- [ ] **Phase 3: Read Operations** - Read scalar values (bulk and by-ID) and element IDs from collections
- [ ] **Phase 4: Query and Transaction Control** - Execute plain and parameterized SQL queries, manage explicit transactions
- [ ] **Phase 5: Package and Distribution** - npm package structure, TypeScript types, test suite, development tooling

## Phase Details

### Phase 1: FFI Foundation and Database Lifecycle
**Goal**: Users can load the native library and open/close Quiver databases from JS/TS code
**Depends on**: Nothing (first phase)
**Requirements**: FFI-01, FFI-02, FFI-03, FFI-04, LIFE-01, LIFE-02, LIFE-03
**Success Criteria** (what must be TRUE):
  1. Calling `Database.fromSchema(dbPath, schemaPath)` opens a database and returns a Database instance without errors
  2. Calling `Database.fromMigrations(dbPath, migrationPaths)` opens a database from migration files
  3. Calling `db.close()` releases native resources and subsequent operations on that instance throw
  4. When a C API call fails, a `QuiverError` is thrown with the error message from the C layer (not a generic FFI error)
  5. Library loading works on Windows (pre-loading libquiver.dll before libquiver_c.dll)
**Plans**: TBD

Plans:
- [ ] 01-01: TBD
- [ ] 01-02: TBD

### Phase 2: Element Builder and CRUD
**Goal**: Users can create and delete elements with typed scalar and array attributes
**Depends on**: Phase 1
**Requirements**: CRUD-01, CRUD-02, CRUD-03, CRUD-04
**Success Criteria** (what must be TRUE):
  1. User can create an element with integer, float, string, and null scalar values and receive its numeric ID back
  2. User can create an element with integer, float, and string array values
  3. User can delete an element by ID from a collection
  4. Creating an element with invalid data (e.g., missing required attribute) throws a QuiverError with a descriptive message from the C layer
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: Read Operations
**Goal**: Users can read scalar attribute values and element IDs from collections
**Depends on**: Phase 2
**Requirements**: READ-01, READ-02, READ-03
**Success Criteria** (what must be TRUE):
  1. User can read all scalar integers, floats, or strings for a given collection attribute and get a typed JS array
  2. User can read a single scalar integer, float, or string by element ID
  3. User can read all element IDs from a collection
  4. Read operations on an empty collection return empty arrays (not errors)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: Query and Transaction Control
**Goal**: Users can execute SQL queries with parameters and control transaction boundaries
**Depends on**: Phase 1
**Requirements**: QUERY-01, QUERY-02, TXN-01, TXN-02
**Success Criteria** (what must be TRUE):
  1. User can execute a plain SQL query and get string, integer, or float results
  2. User can execute a parameterized SQL query with typed parameters (string, integer, float, null) using positional placeholders
  3. User can begin, commit, and rollback explicit transactions
  4. User can check whether a transaction is currently active via `inTransaction()`
**Plans**: TBD

Plans:
- [ ] 04-01: TBD
- [ ] 04-02: TBD

### Phase 5: Package and Distribution
**Goal**: The binding is a well-structured npm package with TypeScript types, a test suite, and development tooling
**Depends on**: Phase 2, Phase 3, Phase 4
**Requirements**: PKG-01, PKG-02, PKG-03, PKG-04
**Success Criteria** (what must be TRUE):
  1. Running `bun add <package>` from an external project installs the binding (package.json is valid, exports are correct)
  2. All public API functions and types have TypeScript type definitions (no `any` in public surface)
  3. Running the test script executes all tests via `bun:test` and they pass
  4. The test runner script handles PATH/DLL setup so tests work out of the box in the development environment
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. FFI Foundation and Database Lifecycle | 0/? | Not started | - |
| 2. Element Builder and CRUD | 0/? | Not started | - |
| 3. Read Operations | 0/? | Not started | - |
| 4. Query and Transaction Control | 0/? | Not started | - |
| 5. Package and Distribution | 0/? | Not started | - |

---
*Roadmap created: 2026-03-08*
*Last updated: 2026-03-08*
