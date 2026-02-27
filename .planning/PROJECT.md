# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Provides schema-driven CRUD, typed queries, time series, CSV import/export, and Lua scripting — all through a layered architecture where intelligence lives in C++ and bindings are thin wrappers.

## Core Value

A single C++ implementation powers every language binding identically — same behavior, same error messages, same guarantees.

## Requirements

### Validated

- C++ core library with schema-driven CRUD, queries, time series, CSV, transactions, metadata, Lua scripting
- C API layer with 100% coverage of C++ public methods, binary error codes, co-located alloc/free
- Julia binding with full API coverage, finalizers, DateTime convenience, composite helpers
- Dart binding with full API coverage, Arena-based memory management, lifecycle tracking
- Python binding with CFFI ABI-mode, kwargs-based create/update, CSV support
- Lua binding via LuaRunner embedded in C++ core
- CLI executable (`quiver_cli`) for running Lua scripts against databases
- Comprehensive test suites across C++, C API, Julia, Dart, Python layers
- Schema validation at database open time
- Nest-aware RAII transaction guard

### Active

- [ ] Fix layer inversion: C++ should own `DatabaseOptions` and `CSVOptions` types
- [ ] Fix `quiver_element_free_string` semantic confusion — rename or add proper alias
- [ ] Add Python LuaRunner binding
- [ ] Replace Python hardcoded data type magic numbers with named constants
- [ ] Add tests for Python convenience helpers
- [ ] Add `is_healthy()` and `path()` tests across all bindings

### Out of Scope

- Blob subsystem — placeholder for future, not part of v0.5
- Performance optimizations (WAL mode, streaming cursors) — acceptable for current scale
- Multi-row query API — deferred
- Schema caching — acceptable startup cost for current schema sizes

## Context

- Current version: 0.5.0
- WIP project — breaking changes acceptable, no backwards compatibility required
- Codebase is mature with consistent patterns across all layers
- CONCERNS.md documents known issues including SQL identifier safety, import_csv FK pragma handling, and silent update/delete on nonexistent IDs — these are tracked but not in v0.5 scope

## Constraints

- **C++20**: Target standard, use modern features where they simplify logic
- **Clean code**: Optimized for human readability, not machine parsing
- **Thin bindings**: All logic in C++ layer; bindings only marshal types and surface errors
- **Test coverage**: Changes must include tests in all affected layers (C++, C, Julia, Dart, Python)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| C++ owns options types, C API converts | Fixes layer inversion; C++ is the source of truth | -- Pending |
| Rename free function for query strings | Follows consistent `quiver_{entity}_free_*` pattern | -- Pending |
| Keep Blob classes as placeholders | Future implementation planned | -- Pending |

---
*Last updated: 2026-02-27 after milestone v0.5 initialization*
