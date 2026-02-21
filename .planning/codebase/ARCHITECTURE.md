# Architecture

**Analysis Date:** 2026-02-20

## Pattern Overview

**Overall:** Three-layer FFI wrapper — C++ core with opaque Pimpl, flat C API for cross-language FFI, thin language bindings (Julia, Dart, Lua).

**Key Characteristics:**
- All logic lives in the C++ layer. Bindings and the C API are thin pass-throughs.
- SQLite is the single storage engine, accessed exclusively through `Database::Impl` (never exposed to callers).
- Schema-driven runtime: on open, the database reads its own SQLite schema into a `Schema` object and runs `SchemaValidator` before any operation is allowed.
- Method naming is mechanical: a C++ method name can be mechanically transformed to the C API, Julia, Dart, or Lua name without a lookup table.

## Layers

**C++ Core (`include/quiver/`, `src/`):**
- Purpose: All business logic, validation, SQL construction, and result marshaling.
- Location: Public headers at `include/quiver/*.h`; implementations split into per-concern `.cpp` files under `src/`.
- Contains: `Database`, `Element`, `LuaRunner`, `Schema`, `SchemaValidator`, `TypeValidator`, `Migrations`, `Migration`, `Result`, `Row`, `Blob`.
- Depends on: SQLite3 (via `sqlite3.h`, never in public headers), spdlog, sol2 (Lua).
- Used by: C API layer.

**C API (`include/quiver/c/`, `src/c/`):**
- Purpose: Flat C ABI wrapping the C++ core for FFI consumption by any language.
- Location: Public headers at `include/quiver/c/*.h`; implementations at `src/c/database_*.cpp`.
- Contains: `quiver_database_t` (opaque handle wrapping `quiver::Database`), `quiver_element_t`, binary error codes (`quiver_error_t`), free functions for all allocations.
- Depends on: C++ core library only.
- Used by: Julia (via `ccall`), Dart (via `dart:ffi`), Python (stub, not yet implemented), and any other FFI consumer.

**Language Bindings (`bindings/`):**
- Purpose: Idiomatic wrappers in each target language that call the C API via FFI.
- Locations:
  - Julia: `bindings/julia/src/` — auto-generated C declarations in `c_api.jl`, hand-written wrappers in `database_*.jl`.
  - Dart: `bindings/dart/lib/src/` — auto-generated FFI bindings in `ffi/bindings.dart`, hand-written wrappers in `database_*.dart`.
  - Lua: Embedded in `src/lua_runner.cpp` via sol2; no separate binding files. Exposed as `db` userdata inside scripts.
  - Python: `bindings/python/src/` — stub only, not implemented.
- Depends on: C API shared libraries (`libquiver.dll` / `libquiver_c.dll`).
- Used by: End user application code.

## Data Flow

**Element Creation:**
1. Caller constructs an `Element` using the fluent builder API (`Element().set(...)`).
2. `Database::create_element(collection, element)` validates schema membership, type-checks scalars via `TypeValidator`, opens a `TransactionGuard`.
3. Builds `INSERT INTO {collection}` SQL for scalars; routes array attributes to vector/set/time-series tables by calling `Schema::find_all_tables_for_column()`.
4. Executes SQL via `Database::execute()` which calls `sqlite3_prepare_v2` / `sqlite3_step`.
5. Returns the new row's `rowid` as the element ID.

**Read Flow:**
1. Caller invokes `read_scalar_integers(collection, attribute)`.
2. `Database::Impl::require_collection()` and `require_column()` validate against the in-memory `Schema`.
3. SQL `SELECT {attribute} FROM {collection}` is executed; rows are unpacked into a typed `std::vector<T>`.
4. C API layer copies results into `new`-allocated arrays returned via out-parameters; caller frees with a matching `quiver_database_free_*` function.

**Schema Loading:**
1. On `Database` construction (or factory method), SQLite database is opened.
2. `Impl::load_schema_metadata()` calls `Schema::from_database(db)` — introspects SQLite `sqlite_master` / `PRAGMA table_info` / `PRAGMA foreign_key_list` / `PRAGMA index_list`.
3. `SchemaValidator::validate()` enforces Quiver conventions (Configuration table, collection structure, vector/set table shapes, no duplicate attributes).
4. `TypeValidator` is constructed from the validated schema and held for the database lifetime.

**Error Propagation:**
1. C++ throws `std::runtime_error` using one of three message patterns.
2. C API catch blocks call `quiver_set_last_error(e.what())` (thread-local storage) and return `QUIVER_ERROR`.
3. Language bindings call `quiver_get_last_error()` and throw a language-native exception (`DatabaseException` in Julia/Dart).

**State Management:**
- No application-level global state. Each `Database` instance owns its own `Impl` (sqlite3 handle, logger, Schema, TypeValidator).
- Logging is per-instance; each instance gets a uniquely named spdlog logger.
- Thread-local storage for error message capture in the C API layer (`src/c/common.cpp`).

## Key Abstractions

**`Database` (`include/quiver/database.h`):**
- Purpose: Primary entry point for all database operations. Hides SQLite entirely behind Pimpl.
- Pattern: Pimpl (`struct Impl` in `src/database_impl.h`) owns `sqlite3*`, spdlog logger, `Schema`, `TypeValidator`.
- Non-copyable, movable (RAII resource management).

**`Element` (`include/quiver/element.h`):**
- Purpose: Builder for record creation — accumulates scalar and array attribute values before insertion.
- Pattern: Plain value type (no Pimpl), Rule of Zero. Stores `std::map<string, Value>` for scalars and `std::map<string, vector<Value>>` for arrays.
- `Database::create_element()` routes array attributes to correct vector/set/time-series table using `Schema::find_all_tables_for_column()`.

**`Schema` (`include/quiver/schema.h`):**
- Purpose: In-memory representation of the SQLite schema. Classifies tables as collections, vector tables, set tables, or time-series tables. Provides table/column lookup and naming-convention helpers.
- Pattern: Loaded via `Schema::from_database(sqlite3*)` factory; immutable after construction.
- Key methods: `find_all_tables_for_column()`, `is_collection()`, `is_vector_table()`, `vector_table_name()`, etc.

**`SchemaValidator` (`include/quiver/schema_validator.h`):**
- Purpose: Validates that a loaded `Schema` conforms to Quiver conventions before the database is usable.
- Throws `std::runtime_error` with a descriptive message on any violation.

**`TypeValidator` (`include/quiver/type_validator.h`):**
- Purpose: Validates that `Value` variants match the expected SQLite column type before write operations.
- Held by `Database::Impl` for the database lifetime.

**`LuaRunner` (`include/quiver/lua_runner.h`):**
- Purpose: Embeds a Lua 5.x interpreter (via sol2) with the database bound as `db` userdata. All C++ methods are exposed to Lua scripts with identical names.
- Pattern: Pimpl (`struct Impl` in `src/lua_runner.cpp`) hides sol2 headers.

**`Result` / `Row` (`include/quiver/result.h`, `include/quiver/row.h`):**
- Purpose: Generic SQL result set. `Database::execute()` returns `Result`; typed reader methods unpack it into concrete typed vectors.
- Pattern: Plain value types (Rule of Zero).

**`Value` (`include/quiver/value.h`):**
- Purpose: Discriminated union for SQLite-compatible values. `using Value = std::variant<nullptr_t, int64_t, double, string>`.

**`Migrations` / `Migration` (`include/quiver/migrations.h`, `include/quiver/migration.h`):**
- Purpose: Discovers and applies versioned schema migrations from a directory of numbered subdirectories (`1/up.sql`, `2/up.sql`, …).
- Pattern: Plain value types, sorted by version number.

**`Blob` (`include/quiver/blob/blob.h`):**
- Purpose: Read/write access to external binary data files (time-series blobs). Separate from the main database path.
- Pattern: Pimpl, non-copyable, movable.

## Entry Points

**C++ direct usage:**
- Location: `include/quiver/database.h`
- Triggers: Application code links against `libquiver` and includes the header.
- Responsibilities: Full CRUD, query, time series, metadata, migrations, CSV, Lua scripting.

**C API (FFI):**
- Location: `include/quiver/c/database.h`, `include/quiver/c/element.h`, `include/quiver/c/lua_runner.h`
- Triggers: FFI consumer calls `quiver_database_open()` or `quiver_database_from_schema()`.
- Responsibilities: Lifecycle, CRUD, read, update, delete, query, metadata, time series — all operations wrapped.

**Julia binding:**
- Location: `bindings/julia/src/Quiver.jl` (module entry), `bindings/julia/src/database.jl`
- Triggers: `using Quiver; db = from_schema(db_path, schema_path)`
- Responsibilities: Thin Julia wrappers around C API; auto-closes database via `finalizer`.

**Dart binding:**
- Location: `bindings/dart/lib/src/database.dart`
- Triggers: `final db = Database.fromSchema(dbPath, schemaPath);`
- Responsibilities: Thin Dart wrappers around C API via `dart:ffi`; caller must call `db.close()`.

**Lua scripting:**
- Location: Initiated from C++ via `LuaRunner::run(script)` (`include/quiver/lua_runner.h`).
- Triggers: Application constructs `LuaRunner(db)` and calls `run(script)`.
- Responsibilities: Full database API exposed as `db` userdata in script scope.

## Error Handling

**Strategy:** C++ exceptions (`std::runtime_error`) at the core layer, caught at C API boundary, stored thread-locally, retrieved by bindings.

**Patterns:**
- Three canonical message formats: `"Cannot {op}: {reason}"`, `"{Entity} not found: {identifier}"`, `"Failed to {op}: {reason}"`.
- C API: `QUIVER_REQUIRE(ptr1, ptr2, ...)` macro validates non-null arguments; sets error and returns `QUIVER_ERROR` without entering the try block.
- C API: Every non-utility function wraps its body in `try { ... } catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`.
- Julia/Dart: `check()` helper calls `quiver_get_last_error()` and raises `DatabaseException` when the return code is `QUIVER_ERROR`.
- Rollback: `TransactionGuard` destructor calls `impl_->rollback()` if `commit()` was not reached — ensures no partial writes on exception.

## Cross-Cutting Concerns

**Logging:** spdlog per-instance logger. Console + file sinks (file in same directory as database). In-memory databases get console-only. Log levels configurable via `quiver_database_options_t`. Log file: `quiver_database.log` in the database directory.

**Validation:** Two-phase — structural (`SchemaValidator` on open) and runtime (`TypeValidator` on each write). Both throw `std::runtime_error`; no silent failures.

**Transactions:** `Impl::TransactionGuard` RAII class in `src/database_impl.h`. All write operations (create, update, delete, time series update) use a `TransactionGuard` for atomicity and automatic rollback on exception.

**Memory Ownership:** C API allocates with `new` / `new[]`; every allocation has a matching `quiver_database_free_*` or `quiver_element_free_*` function. Alloc and free pairs always live in the same translation unit. Language bindings own the call to the free function.

---

*Architecture analysis: 2026-02-20*
