# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python). Provides schema-introspective CRUD, time series, CSV import/export, parameterized queries, and embedded Lua scripting over SQLite databases that follow Quiver schema conventions.

## Core Value

A single, consistent API surface for reading and writing structured data in SQLite — from any language, with zero boilerplate.

## Current Milestone: v0.5 Lua CLI

**Goal:** Ship a standalone `quiver_lua` executable that opens a Quiver database and runs a Lua script against it.

**Target features:**
- CLI executable (`quiver_lua.exe`) with argument parsing
- Database opening via `from_schema` or `from_migrations`
- Lua script execution via existing `LuaRunner`

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- C++ core library with full CRUD, queries, time series, CSV, metadata, schema validation
- C API shim with opaque handles, binary error codes, thread-local errors
- Julia bindings (thin FFI wrapper)
- Dart bindings (thin FFI wrapper)
- Python bindings (CFFI ABI-mode)
- Embedded Lua scripting via LuaRunner
- Schema-introspective routing (vectors, sets, time series)
- Migration-based schema versioning
- Explicit transaction control (begin/commit/rollback)

### Active

<!-- Current scope. Building toward these. -->

- [ ] Standalone `quiver_lua` CLI executable
- [ ] Argument parsing (database path, schema/migrations path, script path)
- [ ] Database opening via `from_schema` or `from_migrations` modes

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- REPL mode — Interactive Lua shell deferred to future milestone
- Script arguments — Passing extra args to Lua scripts deferred
- New Lua API methods — v0.5 uses existing LuaRunner capabilities as-is

## Context

- LuaRunner already exists (`include/quiver/lua_runner.h`) — takes a `Database&` reference, exposes `run(script)` method
- Factory methods `Database::from_schema()` and `Database::from_migrations()` already exist with `DatabaseOptions`
- Build system is CMake; new executable target needed alongside existing `quiver_tests`, `quiver_benchmark`
- Will need a C++ argument parsing library (e.g., argparse)

## Constraints

- **Tech stack**: C++20, CMake build system, must link against `libquiver`
- **Platform**: Windows primary (`.exe`), but standard C++ for portability
- **Dependencies**: Minimize — one arg parsing library at most

## Key Decisions

<!-- Decisions that constrain future work. Add throughout project lifecycle. -->

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Executable name `quiver_lua` | Consistent with `quiver_tests`, `quiver_benchmark` naming | — Pending |
| Use argparse library | Avoid hand-rolling CLI parsing | — Pending |

---
*Last updated: 2026-02-27 after milestone v0.5 started*
