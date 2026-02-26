# Quiver

## What This Is

A SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Schema-introspective — the database validates its own schema at open time and routes reads/writes to typed tables (scalar, vector, set, time series).

## Core Value

A human-centric database API: clean to read, intuitive to use, consistent across every language binding.

## Current Milestone: v0.5 read_element_by_id

**Goal:** Add a single composite read function that returns all scalars, vectors, and sets for an element by ID, and clean up redundant typed by-id reads from bindings.

**Target features:**
- C++ `read_element_by_id` returning structured `ElementData`
- C API with transparent struct types for FFI
- Thin wrappers in all bindings (Lua, Julia, Dart, Python)
- Remove duplicated composite reads and individual typed vector/set by-id reads from bindings

## Requirements

### Validated

- Factory methods (from_schema, from_migrations)
- CRUD operations (create, read, update, delete elements)
- Scalar, vector, set, time series data types
- Parameterized queries
- CSV import/export
- Embedded Lua scripting
- Schema introspection and validation
- Explicit transaction control
- Migration support
- C API with binary error codes + thread-local error strings
- Julia, Dart, Python bindings

### Active

- [ ] Composite `read_element_by_id` across all layers
- [ ] Binding API cleanup (remove redundant typed by-id reads)
- [ ] Tests at all layers (C++, C, Lua, Julia, Dart, Python)

### Out of Scope

- Changes to individual typed reads in C++ or C API — those stay as-is
- Time series reads — handled by separate `read_time_series_group`
- New data types or schema changes

## Context

- Vectors and sets are always single-column by schema convention
- Julia, Dart, Python, and Lua each independently implement `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` — identical logic duplicated 4 times
- Lua also exposes all 9 individual typed vector/set by-id reads that are rarely used directly
- C API has transparent struct precedent: `quiver_scalar_metadata_t`, `quiver_group_metadata_t`

## Constraints

- **Architecture**: Intelligence stays in C++ layer; bindings remain thin wrappers
- **Breaking changes**: Acceptable — WIP project, no backwards compatibility required
- **Testing**: All layers must have tests (C++, C API, Lua, Julia, Dart, Python)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Single `read_element_by_id` over three separate functions | User wants one call to get everything | — Pending |
| Transparent C API structs over opaque handle | Fewer functions (2 vs 20+), consistent with metadata types | — Pending |
| Keep individual typed reads in C++/C API | Still useful for targeted single-value lookups | — Pending |
| Remove typed vector/set by-id reads from all bindings | Redundant when `read_element_by_id` exists | — Pending |

---
*Last updated: 2026-02-26 after milestone v0.5 initialization*
