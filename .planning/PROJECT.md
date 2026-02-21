# Quiver

## What This Is

SQLite wrapper library with a C++ core, flat C API for FFI, and idiomatic language bindings (Julia, Dart, Lua). Provides schema-driven CRUD, typed reads, time series, relations, parameterized queries, CSV, and Lua scripting over a validated SQLite schema.

## Core Value

Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. Pre-GSD milestones (v0.1-v0.2). -->

- CRUD operations (create, read, update, delete elements)
- Typed scalar/vector/set read and write operations (integer, float, string)
- Multi-column time series (read/update with typed columnar data)
- Time series files (singleton file path references)
- Schema validation on open (structural + runtime type checking)
- Metadata introspection (scalar, vector, set, time-series groups)
- Parameterized SQL queries (positional ? placeholders, typed parameters)
- Relations (foreign key update/read via scalar_relation)
- CSV export/import
- Schema migrations (versioned directory discovery)
- Lua scripting (full DB API exposed as `db` userdata)
- C API with binary error codes and thread-local error messages
- Julia bindings with auto-generated FFI declarations
- Dart bindings with auto-generated FFI declarations
- Element builder with fluent API

### Active

<!-- Current scope: v0.3 — Explicit Transactions -->

- [ ] Explicit transaction control (begin/commit/rollback) in C++ public API
- [ ] Nest-aware internal TransactionGuard (no-op when user transaction active)
- [ ] Explicit transactions exposed through C API
- [ ] Explicit transactions exposed through Julia binding
- [ ] Explicit transactions exposed through Dart binding
- [ ] Explicit transactions exposed through Lua binding
- [ ] C++ benchmark measuring create_element + update_time_series performance improvement

### Out of Scope

- Python bindings (stub exists, not implementing this milestone)
- New database operations beyond transaction control
- Schema migration tooling changes

## Context

- SQLite is the single storage engine; `Database::Impl` owns the `sqlite3*` handle
- `TransactionGuard` (RAII, in `database_impl.h`) manages internal transactions for all write methods
- `create_element` and `update_time_series_group` each open their own `TransactionGuard` — calling both for one element means two commits = two fsyncs
- `sqlite3_get_autocommit()` can detect whether a transaction is already active
- All public C++ methods must propagate through C API, then to all bindings

## Constraints

- **ABI**: C API remains flat functions with `quiver_error_t` returns
- **Nesting**: Must not break existing code — when no user transaction is active, behavior is identical to today
- **Standard**: C++20
- **Bindings**: All new C++ methods must be bound to C API, Julia, Dart, and Lua

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Explicit begin/commit/rollback over RAII wrapper | Simplest FFI surface; bindings can wrap in their own RAII if desired | -- Pending |
| TransactionGuard becomes no-op when nested | Avoids SAVEPOINT complexity; user transaction covers atomicity | -- Pending |

---
*Last updated: 2026-02-20 after milestone v0.3 initialization*
