# Quiver

## What This Is

SQLite wrapper library with a C++20 core, C API for FFI, and language bindings (Julia, Dart, Python, JS/Bun). Provides schema-driven CRUD, typed queries, time series, CSV I/O, blob file storage, and Lua scripting — all through a uniform API surface across languages.

## Core Value

Every public C++ method is accessible from every supported language binding with consistent naming, identical behavior, and comprehensive tests.

## Current Milestone: v0.5 JS Binding Parity

**Goal:** Bring the JS/Bun binding to full parity with Julia, Dart, and Python bindings, and fix a C API inconsistency.

**Target features:**
- Complete JS binding: update, vector/set reads, metadata, time series, CSV, LuaRunner, blob
- Fix `bool*` → `int*` inconsistency in `quiver_database_in_transaction`
- Delete dead code (`src/blob/dimension.cpp`)
- Tests across all layers for every new operation

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- C++20 core: CRUD, queries, transactions, CSV I/O, time series, blob, schema validation, migrations
- C API: Full wrapper of C++ surface with opaque handles and binary error codes
- Julia binding: Complete parity with C API + convenience methods
- Dart binding: Complete parity with C API + convenience methods
- Python binding: Complete parity with C API + convenience methods (CFFI ABI-mode)
- JS/Bun binding: Partial — lifecycle, scalar CRUD, scalar reads, queries, transactions
- Lua scripting: Full database access via LuaRunner
- CLI: quiver_cli executes Lua scripts against databases

### Active

<!-- Current scope. Building toward these. -->

- [ ] JS binding: updateElement
- [ ] JS binding: vector reads (bulk + by-id)
- [ ] JS binding: set reads (bulk + by-id)
- [ ] JS binding: metadata operations (get/list for scalar/vector/set/time series)
- [ ] JS binding: time series read/update
- [ ] JS binding: time series files operations
- [ ] JS binding: CSV export/import
- [ ] JS binding: LuaRunner
- [ ] JS binding: blob subsystem (blob, blob_csv, blob_metadata)
- [ ] JS binding: database introspection (isHealthy, currentVersion, path, describe)
- [ ] JS binding: convenience composites (readScalarsById, readVectorsById, readSetsById)
- [ ] C API: fix `bool*` → `int*` in `in_transaction` + update all bindings
- [ ] Dead code: delete `src/blob/dimension.cpp`

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- New C++ features — v0.5 is binding parity, not new core functionality
- Node.js compatibility — JS binding targets Bun only (uses bun:ffi)
- Blob bindings for Dart/Python — not yet started, separate milestone

## Context

- JS binding was added in commit `86852b7` as "initial" — covers ~30% of API surface
- JS uses `bun:ffi` with `declare module` augmentation pattern for method organization
- Existing JS patterns (create.ts, read.ts, query.ts, transaction.ts) establish the implementation style
- The `bool*` inconsistency in `in_transaction` affects JS binding (uses `Uint8Array(1)` instead of `Int32Array(1)`)
- All test schemas are shared from `tests/schemas/`

## Constraints

- **Runtime**: Bun >= 1.0.0 (bun:ffi required)
- **Pattern**: JS binding must follow established augmentation pattern (declare module + prototype assignment)
- **Tests**: Every new JS operation needs a test. C API fix needs tests in all 6 layers.
- **No logic in bindings**: All intelligence stays in C++ layer; JS is thin FFI wrapper

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Bun-only JS binding | bun:ffi is simpler than node-addon-api, direct FFI | -- Pending |
| Prototype augmentation pattern | Keeps files small, mirrors C API organization | -- Pending |
| `bool*` → `int*` for `in_transaction` | Consistency with all other boolean out-params in C API | -- Pending |

---
*Last updated: 2026-03-09 after milestone v0.5 initialization*
