# Quiver

## What This Is

Quiver is a SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Lua). It provides structured data access patterns (scalars, vectors, sets, time series) over SQLite databases with schema validation, element builders, and Lua scripting support. The target users are developers embedding structured SQLite access into Julia, Dart, or Lua applications.

## Core Value

Every public C++ method is reachable from every binding (C, Julia, Dart, Lua) through uniform, predictable patterns -- the library feels like one product, not four wrappers.

## Requirements

### Validated

- ✓ Database lifecycle management (open, close, move semantics, factory methods) -- existing
- ✓ Element creation with fluent builder API -- existing
- ✓ Scalar read/write for integers, floats, strings -- existing
- ✓ Vector read/write for integers, floats, strings -- existing
- ✓ Set read/write for integers, floats, strings -- existing
- ✓ Time series read/update/metadata -- existing
- ✓ Time series files support -- existing
- ✓ Schema validation (Configuration table, naming conventions, foreign keys) -- existing
- ✓ Parameterized SQL queries (string, integer, float results) -- existing
- ✓ Relation support (set/read scalar relations) -- existing
- ✓ Metadata inspection (scalar, vector, set, time series group metadata) -- existing
- ✓ C API with binary error codes and thread-local error messages -- existing
- ✓ Julia bindings via FFI -- existing
- ✓ Dart bindings via FFI -- existing
- ✓ Lua scripting via LuaRunner -- existing
- ✓ Migration-based schema evolution -- existing
- ✓ RAII transaction management -- existing
- ✓ Logging via spdlog -- existing
- ✓ GoogleTest-based C++ and C API test suites -- existing
- ✓ Uniform naming conventions across all layers -- v1.0
- ✓ Consistent error handling patterns across all layers -- v1.0
- ✓ Modular file decomposition (C++ and C API) -- v1.0
- ✓ SQL identifier validation (defense-in-depth) -- v1.0
- ✓ clang-tidy static analysis integration -- v1.0
- ✓ Cross-layer naming documentation in CLAUDE.md -- v1.0

### Active

- [ ] Kwargs-style columnar interface for `update_time_series_group` across all layers (C API, Julia, Dart, Lua)
- [ ] Multi-column time series support through the C API (currently limited to single value column)
- [ ] Complete API surface parity -- every C++ method exposed through C, Julia, Dart, and Lua
- [ ] Automated parity detection script that verifies binding completeness
- [ ] Clean up or remove non-functional blob module stubs

## Current Milestone: v1.1 Time Series Ergonomics

**Goal:** Make `update_time_series_group` match `create_element`'s kwargs/columnar interface pattern across all layers.

**Target features:**
- Redesign C API to support multi-column time series updates
- Julia kwargs interface: `update_time_series_group!(db, col, group, id; date_time=[...], val=[...])`
- Dart named-parameter interface matching Julia ergonomics
- Lua alignment with the same columnar pattern

### Out of Scope

- New features beyond current API surface -- refactoring only, no new database operations
- Performance optimization -- focus is consistency, not speed
- New language bindings (Python, etc.) -- stabilize existing bindings first
- Authentication or access control -- not in scope for a library
- GUI or CLI tools -- library only
- C++20 modules -- breaks FFI generator toolchain
- Async/thread-safe API -- SQLite is single-connection

## Context

Shipped v1.0 refactoring milestone. Codebase is now uniformly structured across all 5 layers:
- C++ core: 10 focused modules (lifecycle, create, read, update, delete, metadata, time series, query, relations, describe)
- C API: 9 focused modules mirroring C++ structure with shared helper header
- Julia, Dart, Lua bindings: idiomatic naming, uniform error surfacing
- 1,213 tests passing across 4 suites (C++ 388, C API 247, Julia 351, Dart 227)
- clang-tidy integrated with zero project-code warnings
- SQL identifier validation at all concatenation sites

Tech stack: C++20, CMake, SQLite, spdlog, GoogleTest, sol2 (Lua), Julia FFI, Dart FFI.

## Constraints

- **Tech stack**: C++20, CMake, SQLite -- no changes to foundational stack
- **Architecture**: Layered (C++ -> C API -> Bindings) must be preserved
- **Pimpl pattern**: Only for classes hiding private dependencies (Database, LuaRunner)
- **Intelligence in C++**: Logic stays in C++ core; bindings remain thin wrappers
- **Error messages**: Defined in C++/C API layer only; bindings surface them, never craft their own
- **Test schemas**: All in `tests/schemas/`, shared across test suites

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Interleaved refactoring (layer by layer) | Fix one layer at a time (C++ -> C -> Julia -> Dart), making it consistent AND complete per layer | ✓ Good -- each phase built on previous, kept all tests passing |
| Breaking changes acceptable | WIP project, no external consumers depending on current API | ✓ Good -- enabled clean renames without deprecation overhead |
| Tests with every change | Every refactored API must have passing tests | ✓ Good -- 1,213 tests green at final gate |
| Full stack scope | All layers (C++, C API, Julia, Dart, Lua) in scope for refactoring | ✓ Good -- uniform naming is now mechanical across all layers |
| Extract Impl header before decomposition | Research recommendation: separate phase enables clean split | ✓ Good -- Phase 1 was 6min, unblocked all subsequent decomposition |
| quiver::internal namespace for shared helpers | ODR safety for inline non-template functions across translation units | ✓ Good -- clean compilation with no duplicate symbol issues |
| 3 canonical error message patterns | Consistent format enables downstream parsing: Cannot/Not found/Failed to | ✓ Good -- C API, Julia, Dart, Lua all surface errors uniformly |
| Alloc/free co-location in C API | Every allocation and its free function in same translation unit | ✓ Good -- clear ownership, no orphaned allocations |
| Representative docs over exhaustive | CLAUDE.md shows transformation rules + examples, not all 60 methods | ✓ Good -- maintainable, drift-resistant documentation |

---
*Last updated: 2026-02-11 after v1.1 milestone start*
