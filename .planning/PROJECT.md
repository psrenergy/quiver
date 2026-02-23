# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings. Provides schema-driven typed database operations (CRUD, vectors, sets, time series, relations, queries, CSV export) through a layered architecture where intelligence lives in C++ and bindings are thin wrappers.

## Core Value

Consistent, type-safe database operations across multiple languages through a single C++ implementation.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- C++20 core library with full CRUD, vectors, sets, time series, relations, parameterized queries, CSV export, schema validation, explicit transactions
- C API (FFI layer) wrapping all public C++ methods with binary error codes
- Julia bindings (Quiver.jl) with CFFI via Clang.jl generator
- Dart bindings (quiver_db) with FFI via ffigen generator
- Lua bindings via sol2 (embedded scripting, not standalone)
- Migration support for schema versioning
- Benchmark tooling for transaction performance

### Active

<!-- Current scope. Building toward these. -->

- [ ] Python bindings via CFFI wrapping the C API
- [ ] Test parity across C++, C API, Julia, Dart, and Python layers

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- LuaRunner in Python binding -- niche use case, Lua scripting is for embedded scenarios
- Python <3.13 support -- latest only, smallest maintenance surface
- Python packaging/distribution (PyPI) -- build from source for now, packaging is a separate concern

## Context

- Current version: 0.3.0 (WIP, breaking changes acceptable)
- Existing Python stub at `bindings/python/` with pyproject.toml (package name was "margaux", changing to "quiver")
- CFFI chosen for Python FFI -- standard approach, works well with existing C API
- All test schemas shared in `tests/schemas/valid/` and `tests/schemas/invalid/`
- Codebase map available at `.planning/codebase/`

## Constraints

- **Binding philosophy**: Thin wrappers only. No logic in Python layer. Error messages come from C++/C API.
- **Naming**: Python uses `snake_case` matching C++ method names exactly (same as Julia/Lua pattern).
- **Code quality**: Human-readable, clean code. Simple solutions over complex abstractions.
- **Test schemas**: All bindings reference shared schemas from `tests/schemas/`.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| CFFI for Python FFI | Standard, well-maintained, works with C API directly | -- Pending |
| Package name: quiver | Consistency with project name and other bindings | -- Pending |
| Python >=3.13 only | Smallest maintenance surface, latest features | -- Pending |
| No LuaRunner in Python | Niche use case, not core to Python binding value | -- Pending |

---
*Last updated: 2026-02-23 after v0.4 milestone initialization*
