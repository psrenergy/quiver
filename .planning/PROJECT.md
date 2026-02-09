# Quiver

## What This Is

Quiver is a SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart). It provides structured data access patterns (scalars, vectors, sets, time series) over SQLite databases with schema validation, element builders, and Lua scripting support. The target users are developers embedding structured SQLite access into Julia, Dart, or Lua applications.

## Core Value

Every public C++ method is reachable from every binding (C, Julia, Dart, Lua) through uniform, predictable patterns — the library feels like one product, not four wrappers.

## Requirements

### Validated

- ✓ Database lifecycle management (open, close, move semantics, factory methods) — existing
- ✓ Element creation with fluent builder API — existing
- ✓ Scalar read/write for integers, floats, strings — existing
- ✓ Vector read/write for integers, floats, strings — existing
- ✓ Set read/write for integers, floats, strings — existing
- ✓ Time series read/update/metadata — existing
- ✓ Time series files support — existing
- ✓ Schema validation (Configuration table, naming conventions, foreign keys) — existing
- ✓ Parameterized SQL queries (string, integer, float results) — existing
- ✓ Relation support (set/read scalar relations) — existing
- ✓ Metadata inspection (scalar, vector, set, time series group metadata) — existing
- ✓ C API with binary error codes and thread-local error messages — existing
- ✓ Julia bindings via FFI — existing
- ✓ Dart bindings via FFI — existing
- ✓ Lua scripting via LuaRunner — existing
- ✓ Migration-based schema evolution — existing
- ✓ RAII transaction management — existing
- ✓ Logging via spdlog — existing
- ✓ GoogleTest-based C++ and C API test suites — existing

### Active

- [ ] Uniform naming conventions across all layers (C++, C, Julia, Dart, Lua)
- [ ] Consistent error handling patterns across all layers
- [ ] Complete API surface parity — every C++ method exposed through C, Julia, Dart, and Lua
- [ ] Reduce code duplication in database.cpp (1934 lines) and C API database.cpp (1612 lines)
- [ ] Tests accompany every refactored or new API method
- [ ] Clean up or remove non-functional blob module stubs
- [ ] Address SQL string concatenation in schema queries (use parameterized equivalents)

### Out of Scope

- New features beyond current API surface — refactoring only, no new database operations
- Performance optimization — focus is consistency, not speed
- New language bindings (Python, etc.) — stabilize existing bindings first
- Authentication or access control — not in scope for a library
- GUI or CLI tools — library only

## Context

Quiver is a WIP project where breaking changes are acceptable. The codebase has grown organically with inconsistencies across layers:
- Naming varies between C++ (camelCase methods), C API (snake_case with quiver_ prefix), and bindings
- Error handling differs: C++ uses exceptions, C API uses binary codes + thread-local messages, bindings vary
- API surface is incomplete: some C++ methods lack C API counterparts, and bindings miss C API functions
- The main database.cpp is 1934 lines and the C API wrapper is 1612 lines — both need modular decomposition
- Blob module has stub implementations that create confusion
- Schema queries use string concatenation instead of parameterized queries in some places

The CLAUDE.md file documents conventions and architecture thoroughly. The codebase has good test coverage for core operations but gaps in transaction failure, memory management, and edge case testing.

## Constraints

- **Tech stack**: C++20, CMake, SQLite — no changes to foundational stack
- **Architecture**: Layered (C++ → C API → Bindings) must be preserved
- **Pimpl pattern**: Only for classes hiding private dependencies (Database, LuaRunner)
- **Intelligence in C++**: Logic stays in C++ core; bindings remain thin wrappers
- **Error messages**: Defined in C++/C API layer only; bindings surface them, never craft their own
- **Test schemas**: All in `tests/schemas/`, shared across test suites

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Interleaved refactoring (layer by layer) | Fix one layer at a time (C++ → C → Julia → Dart), making it consistent AND complete per layer | — Pending |
| Breaking changes acceptable | WIP project, no external consumers depending on current API | — Pending |
| Tests with every change | Every refactored API must have passing tests | — Pending |
| Full stack scope | All layers (C++, C API, Julia, Dart, Lua) in scope for refactoring | — Pending |

---
*Last updated: 2026-02-09 after initialization*
