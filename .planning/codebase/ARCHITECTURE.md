<!-- refreshed: 2026-10-02 -->
# Architecture

**Analysis Date:** 2026-10-02

Authoritative sources: root `AGENTS.md` (design decisions, naming rules, error patterns), `src/AGENTS.md` (C++ internals, binary/expression, Lua), `src/c/AGENTS.md` (C API), `bindings/*/AGENTS.md`, `tests/AGENTS.md`. This document summarizes; consult those before changing behavior.

## System Overview

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                         Language bindings (thin)                           │
├──────────────┬──────────────┬────────────────┬──────────────┬─────────────┤
│ Julia        │ Dart         │ Python (CFFI)  │ JS (Bun FFI) │ CLI         │
│ bindings/    │ bindings/    │ bindings/      │ bindings/    │ src/cli/    │
│ julia/src/   │ dart/lib/src │ python/src/    │ js/src/      │ main.cpp    │
└──────┬───────┴──────┬───────┴───────┬────────┴──────┬───────┴──────┬──────┘
       │ ccall        │ ffigen        │ cffi ABI      │ dlopen       │ C++ direct
       ▼              ▼               ▼               ▼              │
┌─────────────────────────────────────────────────────────────────┐  │
│ C API (quiver_c) — opaque handles, out-params, quiver_error_t,  │  │
│ thread_local last error   `include/quiver/c/`, `src/c/`         │  │
└──────────────────────────────┬──────────────────────────────────┘  │
                               ▼                                     ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ C++ core (quiver)  `include/quiver/`, `src/`                              │
│  Database (Pimpl → Database::Impl, `src/database_impl.h`)                 │
│  LuaRunner (sol2, `src/lua_runner.cpp`) — binds Database/Binary/Expression │
│  Schema / SchemaValidator / type_validator / ui_metadata                   │
│  csv_read / csv_write (`src/csv/`)                                         │
│  BinaryFile / BinaryMetadata / CSVConverter (`src/binary/`)                │
│  Expression DAG (`src/expression/`)                                        │
└──────────────────────────────┬────────────────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ SQLite 3 (FetchContent, serialized threadsafe) + .qvr/.toml files on disk  │
└───────────────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| `Database` | Public API: lifecycle, CRUD, reads, metadata, time series, query, CSV, describe, transactions, dry runs | `include/quiver/database.h`, `src/database*.cpp` |
| `Database::Impl` | sqlite3 handle, lazy schema, `execute`/`execute_raw`, label + FK resolution, group writes, `TransactionGuard` | `src/database_impl.h`, `src/database.cpp` |
| `internal::` helpers | Read templates, `value_matches_type`, metadata converters | `src/database_internal.h` |
| `Schema` / `SchemaValidator` | Introspect tables, classify vector/set/time-series groups, enforce schema conventions | `src/schema.cpp`, `src/schema_validator.cpp` |
| Type validation | Scalar typing policy (int-for-REAL, DATE_TIME gate) | `src/type_validator.cpp`, `src/utils/datetime.h` |
| `Element` | Fluent builder for create/update payloads | `include/quiver/element.h`, `src/element.cpp` |
| `Value` / `Row` / `Result` | Variant value + query result types | `include/quiver/value.h`, `src/row.cpp`, `src/result.cpp` |
| Migrations | Versioned up/down SQL discovery and application | `src/migration.cpp`, `src/migrations.cpp` |
| CSV | One parser (csv-parser) + one emitter for import/export and Lua CSV | `src/csv/csv_read.cpp`, `src/csv/csv_write.cpp` |
| `LuaRunner` | Sandboxed Lua scripting over a borrowed `Database&`; JSON result encoder | `src/lua_runner.cpp` |
| Binary subsystem | `.qvr` file I/O with TOML metadata, time dimensions | `src/binary/*.cpp`, `include/quiver/binary/` |
| Expression subsystem | Lazy arithmetic/aggregation DAG over `.qvr`, materialized by `save()` | `src/expression/*.cpp`, `include/quiver/expression/` |
| C API | Marshaling, try/catch → `quiver_set_last_error`, free functions for returned buffers | `src/c/*.cpp`, `src/c/internal.h`, `src/c/database_helpers.h` |
| Bindings | Idiomatic wrappers + convenience (DateTime, boolean, composites, transaction blocks) | `bindings/*/` |

## Pattern Overview

**Overall:** Layered library — fat C++ core, flat C ABI, thin per-language FFI wrappers (hourglass).

**Key Characteristics:**
- All logic and all error messages live in C++ (three message patterns, root `AGENTS.md`); C API and bindings only marshal and surface.
- Pimpl only where private deps must be hidden (`Database`, `LuaRunner`, `BinaryFile`, `csv_read::Reader`); value types use Rule of Zero.
- Database methods are split by verb across `src/database_<verb>.cpp`; C API mirrors this split 1:1 in `src/c/database_<verb>.cpp`; Julia/Dart mirror it again.
- Mechanical cross-layer naming (`quiver_database_` prefix, `!` in Julia, camelCase in Dart/JS).

## Layers

**C++ core:**
- Purpose: Schema-driven SQLite access and the binary/expression/Lua subsystems
- Location: `src/`, `include/quiver/`
- Contains: `Database` + `Impl`, validators, CSV, LuaRunner, binary, expression
- Depends on: sqlite3, spdlog, tomlplusplus, lua + sol2, csv-parser (`cmake/Dependencies.cmake`)
- Used by: C API, `quiver_cli`, C++ tests

**C API:**
- Purpose: Stable C ABI for FFI
- Location: `src/c/`, `include/quiver/c/`
- Contains: opaque structs wrapping C++ objects (`struct quiver_database { quiver::Database db; }` in `src/c/internal.h`), `QUIVER_REQUIRE` null-check macro, marshaling templates
- Depends on: C++ core
- Used by: all four FFI bindings, `tests/test_c_api_*.cpp`

**Bindings:**
- Purpose: Idiomatic per-language surface
- Location: `bindings/julia/src/`, `bindings/dart/lib/src/`, `bindings/python/src/quiverdb/`, `bindings/js/src/`
- Contains: generated or hand-written FFI declarations (`c_api.jl`, `ffi/bindings.dart`, `_c_api.py`, `loader.ts`), wrapper classes, error check helper
- Depends on: `libquiver_c` shared library
- Used by: end users

## Data Flow

### Primary Request Path (Python `read_scalar_integers`)

1. `Database.read_scalar_integers` allocates out-params and calls `lib.quiver_database_read_scalar_integers` (`bindings/python/src/quiverdb/database.py:545`)
2. C API checks pointers with `QUIVER_REQUIRE`, calls `db->db.read_scalar_integers`, copies into malloc'd values + mask via `read_scalars_masked_impl`; any exception → `quiver_set_last_error` + `QUIVER_ERROR` (`src/c/database_read.cpp:9`)
3. Core validates via `impl_->require_collection` / `require_column` (lazy schema load in `Impl::require_schema`), builds SQL `... ORDER BY rowid` (`src/database_read.cpp:6`)
4. `Impl::execute` prepares, checks parameter count, binds `Value` variants, steps into a `Result` (`src/database.cpp:133`)
5. `internal::read_column_values_nullable<int64_t>` converts to `vector<optional<int64_t>>` (`src/database_internal.h`)
6. Binding `check()` reads `quiver_get_last_error` on failure and raises (`bindings/python/src/quiverdb/_helpers.py:10`); on success converts mask to `None` and calls `quiver_database_free_integer_array` / `quiver_database_free_mask`

### Write Path (`create_element` / group writers)

1. Binding builds an `Element` via `quiver_element_set_*` (array setters take a `has_value` mask) or passes columnar group data + mask
2. C API decodes into `quiver::Element` / row maps (`src/c/database_create.cpp`, `src/c/database_update.cpp`)
3. Core opens `Impl::TransactionGuard` (no-op inside caller transaction or dry run), validates everything first (`Impl::prepare_group_data`, `type_validator.cpp`), then INSERTs scalar row and group rows (`Impl::insert_group_data`)

### Lua Script Path

1. Binding/CLI creates `LuaRunner(db)` (borrows `Database&`) and calls `run(script)` (`src/lua_runner.cpp`, `src/c/lua_runner.cpp`)
2. sol2 dispatches `db:` methods to the same `Database` methods; file ops go through `resolve_sandboxed_path`
3. First return value is JSON-encoded in C++ and passed through bindings verbatim

**State Management:**
- Per-`Database` state in `Impl`: sqlite3 handle, `mutable unique_ptr<Schema>` (lazy), `dry_run` flag, `ui_metadata` (eager, `from_migrations` only)
- C API error: `static thread_local std::string g_last_error` (`src/c/common.cpp:8`)

## Key Abstractions

**Collections and groups:**
- Purpose: Schema conventions map tables to `Collection`, `{C}_vector_{g}`, `{C}_set_{g}`, `{C}_time_series_{g}`, `{C}_time_series_files`
- Examples: `src/schema.cpp`, `tests/schemas/valid/*.sql`
- Pattern: Convention-over-configuration, validated by `SchemaValidator`

**Value:**
- Purpose: `variant<nullptr_t, int64_t, double, string>` crossing every internal boundary
- Examples: `include/quiver/value.h`
- Pattern: `std::visit` dispatch

**Presence masks:**
- Purpose: Carry SQL NULL across the C ABI (`uint8_t*` parallel to data; `nullptr` entries for strings)
- Examples: `src/c/database_read.cpp`, `src/c/database_time_series.cpp`

**Expression node DAG:**
- Purpose: Lazy computation over `.qvr` files
- Examples: `include/quiver/expression/expression_node.h`, `src/expression/expression_*.cpp`
- Pattern: Composite node tree, value-type `Expression` handle

## Entry Points

**C++ library:** `include/quiver/quiver.h` (umbrella) — `Database::from_schema`, `from_migrations`, `Database(path, options)`
**C API:** `include/quiver/c/database.h` — `quiver_database_open`, `quiver_database_from_schema`, `quiver_database_from_migrations`
**CLI:** `src/cli/main.cpp` — `quiver_cli` runs a Lua script against a database
**Bindings:** `bindings/julia/src/Quiver.jl`, `bindings/dart/lib/quiverdb.dart`, `bindings/python/src/quiverdb/__init__.py`, `bindings/js/src/index.ts` (+ `bindings/js/mod.ts`)
**Benchmark/sandbox:** `tests/benchmark/`, `tests/sandbox/` (manual only)

## Architectural Constraints

- **Threading:** No internal threads. SQLite built serialized; C API error is thread-local. A `Database` is not designed for concurrent multi-statement consistency — callers wrap related reads in a transaction.
- **Global state:** `g_last_error` (`src/c/common.cpp`); binary write registry in `src/binary/binary_file.cpp`; per-run CSV writer registry in `src/lua_runner.cpp`.
- **Circular imports:** None detected; `database_impl.h` is private to `src/` and included by `database_*.cpp` only.
- **Ownership:** `LuaRunner` holds a raw `Database&` — must not outlive the database. C API returned arrays must be freed with the matching `quiver_*_free_*`.
- **No SAVEPOINTs:** writers validate fully before first write (root `AGENTS.md`).
- **Subsystem exposure:** binary/expression only in Julia + Lua.

## Anti-Patterns

### Crafting error messages in a binding

**What happens:** A binding raises its own text for a condition the core could detect.
**Why it's wrong:** Breaks the single-owner error rule and cross-binding homogeneity.
**Do this instead:** Throw a Pattern 1/2/3 `std::runtime_error` in C++, surface via `check()` (`bindings/python/src/quiverdb/_helpers.py`). Only pre-FFI marshalling errors are local.

### Writing before validating

**What happens:** Inserting a scalar row, then failing on an array.
**Why it's wrong:** `TransactionGuard` no-ops inside caller transactions, leaving half a write.
**Do this instead:** Resolve/validate all input first, like `Impl::prepare_group_data` in `src/database_impl.h`.

### Passing arrays through `update_element` for one group

**What happens:** Arrays route by column name and fan out to every group sharing the name.
**Do this instead:** Use `update_vector_group` / `update_set_group` (`src/database_update.cpp`).

## Error Handling

**Strategy:** C++ throws `std::runtime_error` with one of three patterns; C API catches at every entry point and returns `QUIVER_ERROR`; bindings read `quiver_get_last_error` and raise their native exception (`QuiverError` in `bindings/python/src/quiverdb/exceptions.py`, `bindings/dart/lib/src/exceptions.dart`, `bindings/julia/src/exceptions.jl`, `bindings/js/src/errors.ts`).

**Patterns:**
- `QUIVER_REQUIRE(...)` for null pointer args (`src/c/internal.h:44`)
- `try { ... return QUIVER_OK; } catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`

## Cross-Cutting Concerns

**Logging:** spdlog logger per `Database` (`Impl::logger`), level from `DatabaseOptions::console_level`
**Validation:** C++ only — `SchemaValidator`, `type_validator.cpp`, `datetime::is_valid_iso8601`
**Authentication:** Not applicable (embedded library); Lua sandbox restricts file access to the DB directory

---

*Architecture analysis: 2026-10-02*
