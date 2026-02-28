# Quiver

## What This Is

SQLite wrapper library with C++ core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Provides schema-driven database access with typed scalar, vector, set, and time series operations.

## Core Value

Reliable, type-safe database operations across all language bindings with consistent API surface.

## Current Milestone: v0.5 Code Quality

**Goal:** Fix bugs, reduce duplication, and improve consistency. Breaking changes acceptable.

**Target features:**
- Fix `update_element` missing type validation for arrays
- Fix `describe()` duplicate category headers
- Fix `import_csv` parameter naming inconsistency
- Apply RAII pattern to `current_version()`
- Use `read_grouped_values_by_id` template for scalar reads
- Extract shared helper for group insertion (create/update dedup)
- Use `strdup_safe` consistently in C API string copies

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- ✓ Schema-driven database creation (from_schema, from_migrations)
- ✓ Typed scalar read/write (integers, floats, strings)
- ✓ Vector and set operations
- ✓ Time series multi-column read/update
- ✓ Parameterized queries
- ✓ CSV export/import
- ✓ Transaction control (begin/commit/rollback)
- ✓ Element CRUD operations
- ✓ Metadata and schema inspection
- ✓ C API with FFI bindings (Julia, Dart, Python, Lua)
- ✓ CLI interface

### Active

<!-- Current scope. Building toward these. -->

- [ ] Bug fixes (update_element validation, describe headers, import_csv param)
- [ ] Code quality (RAII, template usage, deduplication, strdup_safe)

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- New features — v0.5 is strictly internal quality improvements
- New bindings — no new language support this milestone
- Performance optimization — not the focus, unless incidental to refactoring

## Context

- Codebase is mature with established patterns documented in CLAUDE.md
- All changes must pass existing tests across C++, C API, Julia, Dart, Python
- Breaking changes acceptable per project principles
- Codebase map available in `.planning/codebase/`

## Constraints

- **Testing**: All changes need tests in C++, C API, Julia, Dart, Python
- **Consistency**: Follow established patterns in CLAUDE.md
- **Scope**: Internal refactoring only — no new public API surface

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Skip research | Internal refactoring, no new domain to explore | — Pending |
| Breaking changes OK | WIP project per CLAUDE.md principles | — Pending |

---
*Last updated: 2026-02-28 after milestone v0.5 started*
