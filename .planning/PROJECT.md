# Quiver

## What This Is

SQLite wrapper library with a C++ core, flat C API for FFI, and language bindings (Julia, Dart, Lua). Schema-driven: databases self-describe via SQLite introspection, supporting collections with scalar, vector, set, and time series attributes.

## Core Value

A single, clean C++ API that exposes full SQLite schema capabilities through uniform, mechanically-derived bindings — write logic once, use from any language.

## Current Milestone: v1.0 Relations

**Goal:** Make foreign key relations work naturally through `create_element` and `update_element` by resolving labels to IDs, eliminating the need for a separate relation API.

**Target features:**
- FK label resolution in `create_element` for scalar and vector FK columns
- FK label resolution in `update_element` for scalar, vector, and set FK columns
- Uniform error behavior: throw on missing target label
- Full binding stack: C++ → C API → Julia → Dart → Lua

## Requirements

### Validated

- Element CRUD (create, read, update, delete) across all attribute types
- Scalar/vector/set/time series read and update operations
- Schema validation and type validation on open/write
- Parameterized queries
- CSV export
- Transaction control (begin/commit/rollback)
- Relation read (`read_scalar_relation`) and write (`update_scalar_relation`)
- FK label resolution in `create_element` for set FK columns
- Full binding stack for all above (Julia, Dart, Lua)

### Active

- [ ] FK label resolution in `create_element` for scalar FK columns
- [ ] FK label resolution in `create_element` for vector FK columns
- [ ] FK label resolution in `update_element` for scalar FK columns
- [ ] FK label resolution in `update_element` for vector FK columns
- [ ] FK label resolution in `update_element` for set FK columns
- [ ] Remove `update_scalar_relation` if redundant after above changes

### Out of Scope

- Changes to `read_scalar_relation` — stays as-is
- New relation read methods (e.g., reverse lookups, by_id reads) — future milestone
- Relation support in time series FK columns — no current schema uses this pattern

## Context

- FK label resolution already works for set FK columns in `create_element` (src/database_create.cpp:151-167). The pattern is proven — it needs to be applied uniformly to scalar and vector paths in both create and update.
- `TypeValidator` currently rejects strings for INTEGER FK columns. The resolution must happen before or bypass type validation for FK columns.
- `update_scalar_relation` uses label-to-label lookup (from_label → to_label). `update_element` uses ID-to-label. They serve different access patterns.

## Constraints

- **Binding parity**: All C++ changes must propagate through C API → Julia → Dart → Lua
- **Error consistency**: FK resolution errors use existing pattern: `"Failed to resolve label 'X' to ID in table 'Y'"`
- **Schema convention**: FK columns are INTEGER type referencing `id` column of target table

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Labels resolve to IDs at create/update time | Natural API — callers think in labels, storage thinks in IDs | — Pending |
| Throw on missing target label | Consistent with existing set FK behavior, fail-fast prevents silent data corruption | — Pending |
| Keep `read_scalar_relation` unchanged | Still useful for reading resolved labels; no alternative exists | — Pending |

---
*Last updated: 2026-02-23 after milestone v1.0 initialization*
