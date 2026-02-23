# Quiver

## What This Is

SQLite wrapper library with a C++ core, flat C API for FFI, and idiomatic language bindings (Julia, Dart, Lua). Provides schema-driven CRUD, typed reads, time series, relations, parameterized queries, explicit transaction control, CSV, and Lua scripting over a validated SQLite schema.

## Core Value

Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.

## Requirements

### Validated

- CRUD operations (create, read, update, delete elements) -- v0.2
- Typed scalar/vector/set read and write operations (integer, float, string) -- v0.2
- Multi-column time series (read/update with typed columnar data) -- v0.2
- Time series files (singleton file path references) -- v0.2
- Schema validation on open (structural + runtime type checking) -- v0.2
- Metadata introspection (scalar, vector, set, time-series groups) -- v0.2
- Parameterized SQL queries (positional ? placeholders, typed parameters) -- v0.2
- Relations (foreign key update/read via scalar_relation) -- v0.2
- Schema migrations (versioned directory discovery) -- v0.2
- Lua scripting (full DB API exposed as `db` userdata) -- v0.2
- C API with binary error codes and thread-local error messages -- v0.2
- Julia bindings with auto-generated FFI declarations -- v0.2
- Dart bindings with auto-generated FFI declarations -- v0.2
- Element builder with fluent API -- v0.2
- Explicit transaction control (begin/commit/rollback) in C++ public API -- v0.3
- Nest-aware internal TransactionGuard (no-op when user transaction active) -- v0.3
- Explicit transactions exposed through C API -- v0.3
- Explicit transactions exposed through Julia binding -- v0.3
- Explicit transactions exposed through Dart binding -- v0.3
- Explicit transactions exposed through Lua binding -- v0.3
- Transaction convenience block wrappers in all bindings -- v0.3
- Benchmark proving 20x+ write throughput from batched transactions -- v0.3
- CSV export: scalars and groups (vector/set/time series) with RFC 4180 output -- v0.4
- CSV export options: enum label resolution and DateTime formatting -- v0.4
- CSV export through C API with flat options struct -- v0.4
- CSV export through Julia, Dart, and Lua bindings -- v0.4

### Active

<!-- Current scope: v0.5 — CSV Refactor -->

- [ ] Replace hand-rolled CSV writer with a CSV library (read+write capable for future import)
- [ ] Merge include/quiver/c/csv.h into include/quiver/c/options.h (consolidate all C API option types)

### Out of Scope

- Python bindings (stub exists, not implementing)
- CSV import (defer to future milestone; this milestone is refactor only)
- Multi-language columns in CSV (caller picks one language per call)
- SAVEPOINT-based nested transactions (no-op TransactionGuard is simpler)
- Transaction behavior modes (DEFERRED/IMMEDIATE/EXCLUSIVE) -- single-connection, DEFERRED is correct
- C++ RAII Transaction wrapper in public API -- flat begin/commit/rollback simplest for FFI
- Autocommit mode toggle -- explicit begin/commit is the correct escape hatch
- Automatic retry on SQLITE_BUSY -- single-connection, cannot occur
- Async transaction support -- synchronous by design

## Current Milestone: v0.5 CSV Refactor

**Goal:** Replace hand-rolled CSV writer with a proper CSV library (enabling future import), and consolidate C API option headers.

**Target features:**
- Integrate CSV library for read+write (replacing ~15-line hand-rolled RFC 4180 writer)
- Merge include/quiver/c/csv.h into include/quiver/c/options.h
- Update all downstream references (includes, FFI generators, bindings)
- All existing CSV export tests must continue to pass

## Context

Shipped v0.4 with full CSV export across all layers. Codebase: C++20, SQLite, spdlog, sol2, CMake.
Test suites: C++ 442, C API 282, Julia 437, Dart 252 tests.
Current CSV writer is hand-rolled (~15 lines RFC 4180) in src/database_csv.cpp.
C API has two separate option headers: options.h (DatabaseOptions) and csv.h (CSVExportOptions).

## Constraints

- **ABI**: C API remains flat functions with `quiver_error_t` returns
- **Standard**: C++20
- **Bindings**: All new C++ methods must be bound to C API, Julia, Dart, and Lua
- **Single connection**: No concurrent access; SQLITE_BUSY cannot occur

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Explicit begin/commit/rollback over RAII wrapper | Simplest FFI surface; bindings can wrap in their own RAII if desired | Good |
| TransactionGuard becomes no-op when nested | Avoids SAVEPOINT complexity; user transaction covers atomicity | Good |
| sqlite3_get_autocommit() for transaction detection | Authoritative state from SQLite itself, cannot drift | Good |
| bool* out_active for in_transaction C API | User preference; first bool* in C API | Good |
| #include <stdbool.h> in C API header | C standard compliance for FFI generators (Dart ffigen, Julia Clang.jl) | Good |
| Transaction block wrappers use best-effort rollback | Swallow rollback error, rethrow original — correct semantics | Good |
| Julia transaction(fn, db) takes fn first | Required for do-block syntax desugaring | Good |
| Benchmark uses file-based DB (not :memory:) | Reflects real-world I/O cost; the point of benchmarking transactions | Good |

---
*Last updated: 2026-02-22 after v0.5 milestone initialization*
