# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Provides schema-driven CRUD, queries, time series, CSV import/export, and Lua scripting — all through a uniform API surface across languages.

## Core Value

Clean, human-readable binding code that feels idiomatic in each language while maintaining a uniform API surface across all wrappers.

## Current Milestone: v0.5 Binding Quality

**Goal:** Improve binding code quality — eliminate repetition, remove dead/misplaced code, add missing tests.

**Target improvements:**
- Dart: Replace inline record types with proper metadata classes
- Julia: Remove dead code and application-specific helpers
- Add tests for all changes across C++, C, Dart, Julia, and Python layers

## Requirements

### Validated

- ✓ C++ core library with full CRUD, queries, transactions, CSV, time series, Lua — all layers
- ✓ C API layer with stable ABI, error codes, memory management
- ✓ Julia bindings with idiomatic API (! suffix, multiple dispatch, kwargs)
- ✓ Dart bindings with extensions pattern and Arena-based memory
- ✓ Python bindings with CFFI ABI-mode, context managers, kwargs
- ✓ Lua bindings via LuaRunner
- ✓ Comprehensive test suites across C++ and C API

### Active

- [ ] Dart metadata types (ScalarMetadata, GroupMetadata classes)
- [ ] Julia dead code removal (quiver_database_sqlite_error)
- [ ] Julia helper_maps.jl removal (homogeneity violation)

### Out of Scope

- New C++ features — v0.5 is binding quality only
- New C API functions — no API changes needed
- Lua binding changes — no issues found

## Context

- Codebase map at `.planning/codebase/`
- Breaking changes acceptable (WIP project)
- Clean code over defensive code
- Bindings are thin wrappers; intelligence stays in C++

## Constraints

- **Homogeneity**: API surface must feel uniform across bindings
- **Tests required**: All changes must have tests in affected layers

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Dart: classes over records for metadata | Julia/Python use named types; records are copy-pasted 15+ times | — Pending |
| Julia: delete helper_maps | Application-specific, breaks homogeneity, not in other bindings | — Pending |

---
*Last updated: 2026-03-01 after v0.5 milestone start*
