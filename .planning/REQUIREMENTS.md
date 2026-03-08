# Requirements: Quiver

**Defined:** 2026-03-08
**Core Value:** Consistent, typed database access across multiple languages through a single C++ core

## v0.5 Requirements

Requirements for JavaScript/TypeScript binding. Each maps to roadmap phases.

### FFI Foundation

- [ ] **FFI-01**: Library loader detects platform and loads libquiver + libquiver_c shared libraries
- [ ] **FFI-02**: Error handler reads C API error messages and throws typed QuiverError
- [ ] **FFI-03**: Out-parameter helpers allocate and read pointer/integer/float/string out-params
- [ ] **FFI-04**: Type conversion helpers handle int64 BigInt-to-number, C strings, typed arrays

### Database Lifecycle

- [ ] **LIFE-01**: User can open database from SQL schema file via `fromSchema()`
- [ ] **LIFE-02**: User can open database from migration files via `fromMigrations()`
- [ ] **LIFE-03**: User can close database and release native resources via `close()`

### Element & CRUD

- [ ] **CRUD-01**: User can build elements with integer, float, string, and null scalar values
- [ ] **CRUD-02**: User can build elements with integer, float, and string array values
- [ ] **CRUD-03**: User can create an element in a collection and receive its ID
- [ ] **CRUD-04**: User can delete an element from a collection by ID

### Read Operations

- [ ] **READ-01**: User can read all scalar integers/floats/strings for a collection attribute
- [ ] **READ-02**: User can read a scalar integer/float/string by element ID
- [ ] **READ-03**: User can read all element IDs from a collection

### Query Operations

- [ ] **QUERY-01**: User can execute SQL and get string/integer/float results
- [ ] **QUERY-02**: User can execute parameterized SQL with typed parameters

### Transaction Control

- [ ] **TXN-01**: User can begin, commit, and rollback explicit transactions
- [ ] **TXN-02**: User can check if a transaction is active via `inTransaction()`

### Package & Distribution

- [ ] **PKG-01**: Binding is an npm package installable via `bun add`
- [ ] **PKG-02**: Package includes TypeScript type definitions
- [ ] **PKG-03**: Test suite covers all bound operations using `bun:test`
- [ ] **PKG-04**: Test runner script handles PATH/DLL setup for development

## Future Requirements

Deferred to future milestone. Tracked but not in current roadmap.

### Extended API

- **EXT-01**: User can read vector integers/floats/strings (all + by-id)
- **EXT-02**: User can read set integers/floats/strings (all + by-id)
- **EXT-03**: User can update existing elements
- **EXT-04**: User can get scalar/vector/set/time-series metadata
- **EXT-05**: User can list scalar attributes, vector groups, set groups
- **EXT-06**: Composite readers (readScalarsById, readVectorsById, readSetsById)
- **EXT-07**: Symbol.dispose support for auto-close with `using` keyword
- **EXT-08**: Transaction callback wrapper (`transaction((db) => { ... })`)

### Advanced Features

- **ADV-01**: CSV import/export operations
- **ADV-02**: Blob subsystem (binary file I/O)
- **ADV-03**: Time series read/update operations
- **ADV-04**: Lua runner integration
- **ADV-05**: Describe operation (schema inspection)

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Node.js support | Different FFI mechanism (N-API), Bun-only for v0.5 |
| Browser support | bun:ffi is server-side only |
| ORM-like abstractions | Logic belongs in C++ layer per project principles |
| Async/Promise API | bun:ffi calls are synchronous, SQLite ops are fast |
| Bundled native libraries in npm | Complex multi-platform packaging, use PATH for v0.5 |
| Connection pooling | SQLite is single-writer, no benefit |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| FFI-01 | — | Pending |
| FFI-02 | — | Pending |
| FFI-03 | — | Pending |
| FFI-04 | — | Pending |
| LIFE-01 | — | Pending |
| LIFE-02 | — | Pending |
| LIFE-03 | — | Pending |
| CRUD-01 | — | Pending |
| CRUD-02 | — | Pending |
| CRUD-03 | — | Pending |
| CRUD-04 | — | Pending |
| READ-01 | — | Pending |
| READ-02 | — | Pending |
| READ-03 | — | Pending |
| QUERY-01 | — | Pending |
| QUERY-02 | — | Pending |
| TXN-01 | — | Pending |
| TXN-02 | — | Pending |
| PKG-01 | — | Pending |
| PKG-02 | — | Pending |
| PKG-03 | — | Pending |
| PKG-04 | — | Pending |

**Coverage:**
- v0.5 requirements: 22 total
- Mapped to phases: 0
- Unmapped: 22

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-03-08 after initial definition*
