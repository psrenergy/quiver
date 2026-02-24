# Quiver

## What This Is

SQLite wrapper library with a C++ core, flat C API for FFI, and language bindings (Julia, Dart, Lua). Schema-driven: databases self-describe via SQLite introspection, supporting collections with scalar, vector, set, and time series attributes. Foreign key columns resolve labels to IDs automatically in create and update paths.

## Core Value

A single, clean C++ API that exposes full SQLite schema capabilities through uniform, mechanically-derived bindings — write logic once, use from any language.

## Requirements

### Validated

- Element CRUD (create, read, update, delete) across all attribute types
- Scalar/vector/set/time series read and update operations
- Schema validation and type validation on open/write
- Parameterized queries
- CSV export
- Transaction control (begin/commit/rollback)
- FK label resolution in `create_element` for set FK columns
- Full binding stack for all above (Julia, Dart, Lua)
- ✓ FK label resolution in `create_element` for scalar FK columns — v1.0
- ✓ FK label resolution in `create_element` for vector FK columns — v1.0
- ✓ FK label resolution in `create_element` for time series FK columns — v1.0
- ✓ FK label resolution in `update_element` for scalar FK columns — v1.0
- ✓ FK label resolution in `update_element` for vector FK columns — v1.0
- ✓ FK label resolution in `update_element` for set FK columns — v1.0
- ✓ FK label resolution in `update_element` for time series FK columns — v1.0
- ✓ Shared `resolve_fk_label` helper for all FK resolution — v1.0
- ✓ Clear error on missing FK target label — v1.0
- ✓ Removed redundant `update_scalar_relation` and `read_scalar_relation` — v1.0

## Current Milestone: v1.1 FK Test Coverage

**Goal:** Mirror all 16 C++ FK resolution tests across C API, Julia, Dart, and Lua to ensure FK label resolution is tested end-to-end through every binding layer.

**Target:**
- C API: 16 FK resolution tests (create + update paths)
- Julia: 16 FK resolution tests (create + update paths)
- Dart: 16 FK resolution tests (create + update paths)
- Lua: 16 FK resolution tests (create + update paths)

### Active

- [ ] FK resolution test coverage in C API (create + update + error cases)
- [ ] FK resolution test coverage in Julia (create + update + error cases)
- [ ] FK resolution test coverage in Dart (create + update + error cases)
- [ ] FK resolution test coverage in Lua (create + update + error cases)

### Out of Scope

- FK resolution cache — premature optimization; SQLite page cache handles repeated lookups
- Batch label resolution — marginal perf gain, harder per-label error messages
- FK resolution in typed update methods (`update_scalar_integer`, etc.) — these accept typed values; callers already have IDs
- "Find or create" semantics for FK targets — silent data creation is dangerous for a low-level wrapper

## Context

Shipped v1.0 Relations with 23,493 LOC C++.
Tech stack: C++20, SQLite, spdlog, Lua, Julia FFI, Dart FFI.
Test suite: 1,391 tests across 4 layers (C++ 442, C API 271, Julia 430, Dart 248).
Relation methods (`update_scalar_relation`, `read_scalar_relation`) removed — FK resolution now happens uniformly through `create_element` and `update_element` pre-resolve pass.

## Constraints

- **Binding parity**: All C++ changes must propagate through C API → Julia → Dart → Lua
- **Error consistency**: FK resolution errors use pattern: `"Failed to resolve label 'X' to ID in table 'Y'"`
- **Schema convention**: FK columns are INTEGER type referencing `id` column of target table

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Labels resolve to IDs at create/update time | Natural API — callers think in labels, storage thinks in IDs | ✓ Good — unified pre-resolve pass works for all column types |
| Throw on missing target label | Consistent with existing set FK behavior, fail-fast prevents silent data corruption | ✓ Good — zero partial writes on resolution failure |
| Remove `read_scalar_relation` and `update_scalar_relation` | Redundant after FK resolution in create/update; simpler API surface | ✓ Good — 5 layers cleaned, no functionality lost |
| Pre-resolve pass via `ResolvedElement` struct | Immutable input pattern; resolve all FK labels into separate struct before SQL writes | ✓ Good — same method reused by both create and update |
| `resolve_fk_label` on `Database::Impl` | Internal helper needs schema access and `execute()`; nested class access to private members | ✓ Good — clean separation, no public API leak |

---
*Last updated: 2026-02-24 after v1.1 milestone initialization*
