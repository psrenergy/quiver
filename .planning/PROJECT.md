# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python). Schema-introspective: the database validates its own schema at open-time and routes reads/writes to correct tables. Supports scalars, vectors, sets, time series, CSV import/export, Lua scripting, and parameterized queries.

## Core Value

A single, consistent API surface across all language bindings — thin wrappers that feel native in each language while all logic lives in the C++ core.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- ✓ C++ core: full CRUD, schema introspection, type validation, transactions, migrations
- ✓ Vector, set, and time series table support (multi-column)
- ✓ CSV import/export with configurable options
- ✓ Parameterized SQL queries
- ✓ Lua scripting via LuaRunner (sol2)
- ✓ C API shim: binary error codes, opaque handles, thread-local error store
- ✓ Julia binding: kwargs-based create/update, complete API coverage
- ✓ Dart binding: dict-based create/update, complete API coverage
- ✓ Python binding: CFFI ABI-mode, complete API coverage
- ✓ Python kwargs-based create_element/update_element — v0.5
- ✓ Element class removed from Python public API — v0.5
- ✓ All Python tests converted to kwargs API — v0.5

### Active

<!-- Current scope. Building toward these. -->

(None — planning next milestone)

### Out of Scope

- Blob subsystem bindings — experimental, not exposed via C API
- Lua binding changes — separate concern from Python alignment
- Dict positional arg for create/update — `**my_dict` unpacking covers this

## Context

Shipped v0.5 on 2026-02-26. Python binding now uses kwargs for create/update, matching Julia and Dart patterns. All 198 Python tests pass with kwargs-only API. Element class is internal-only for FFI marshaling.

Tech stack: C++20, CMake, SQLite, spdlog, sol2, CFFI (Python), CxxWrap (Julia), dart:ffi (Dart).

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| kwargs over dict positional arg | Julia pattern is more Pythonic; `**dict` unpacking covers dict use case | ✓ Good |
| Delete Element class entirely | No use case that kwargs can't handle; "delete unused code, do not deprecate" | ✓ Good |
| kwargs-to-Element try/finally cleanup | Matches Julia binding pattern; ensures C-level Element is always freed | ✓ Good |
| Internal Element import in database.py | `from quiverdb.element import Element` — not in `__init__.py` exports | ✓ Good |

## Constraints

- **Binding philosophy**: Intelligence stays in C++ layer. Python wrapper is thin.
- **Breaking changes**: Acceptable. No backwards compatibility required.
- **Code quality**: Human-readable, clean, maintainable. Delete unused code.

---
*Last updated: 2026-02-26 after v0.5 milestone*
