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

### Active

<!-- Current scope. Building toward these. -->

- [ ] Python binding follows Julia/Dart pattern: kwargs for create_element/update_element
- [ ] Element class removed from Python public API
- [ ] Python tests updated for new API

### Out of Scope

- Blob subsystem bindings — experimental, not exposed via C API
- New C++ core features — v0.5 is binding alignment only
- Julia/Dart binding changes — already follow the target pattern
- Lua binding changes — separate concern from Python alignment

## Current Milestone: v0.5 Python API Alignment

**Goal:** Make Python binding follow the same kwargs/dict pattern as Julia and Dart for element creation and updates.

**Target features:**
- kwargs-based `create_element` and `update_element`
- Remove Element from public API (delete unused code)
- Update all Python tests

## Context

- Python currently requires `Element().set("k", v)` builder pattern
- Julia uses kwargs: `create_element!(db, "C"; label="x")`
- Dart uses dict: `createElement("C", {"label": "x"})`
- Python should use kwargs: `create_element("C", label="x")`
- `**my_dict` unpacking covers dynamic construction use cases
- CFFI ABI-mode means no compiler needed at install time
- Element is a C-level object — must still be constructed internally for FFI calls

## Constraints

- **Binding philosophy**: Intelligence stays in C++ layer. Python wrapper is thin.
- **Breaking changes**: Acceptable. No backwards compatibility required.
- **Code quality**: Human-readable, clean, maintainable. Delete unused code.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| kwargs over dict positional arg | Julia pattern is more Pythonic; `**dict` unpacking covers dict use case | — Pending |
| Delete Element class entirely | No use case that kwargs can't handle; "delete unused code, do not deprecate" | — Pending |

---
*Last updated: 2026-02-26 after milestone v0.5 started*
