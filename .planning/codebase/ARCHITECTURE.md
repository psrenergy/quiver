# Architecture

**Analysis Date:** 2026-02-27

## Pattern Overview

**Overall:** Layered FFI Library with Thin Binding Facade

The system is built as a native C++ core library exposed through a C ABI layer, which language bindings (Julia, Dart, Python, Lua) consume via FFI. All logic lives in the C++ layer. Bindings are pure thin wrappers that translate types and surface errors from the C API without adding logic of their own.

**Key Characteristics:**
- C++ core (`libquiver`) implements all database logic using SQLite
- C API (`libquiver_c`) wraps C++ with stable ABI, binary error codes, and output parameters
- Language bindings are generated or hand-maintained FFI wrappers with no embedded logic
- Schema-driven: the SQLite schema is introspected at database open time and cached as an in-memory `Schema` object
- RAII throughout: `Database` owns the SQLite handle, `LuaRunner` owns the Lua state, `TransactionGuard` is nest-aware RAII for write operations

## Layers

**C++ Core Library:**
- Purpose: All business logic -- schema loading, validation, CRUD, queries, CSV, time series, Lua scripting
- Location: `src/` (implementation), `include/quiver/` (public headers)
- Contains: `Database`, `Element`, `LuaRunner`, `Schema`, `SchemaValidator`, `TypeValidator`, `Row`, `Result`, `Migration`, `Migrations`
- Depends on: SQLite3, spdlog, Lua
- Used by: C API layer, C++ tests, CLI executable

**C API Layer:**
- Purpose: Stable ABI bridge from C++ to FFI consumers; converts exceptions to error codes and output parameters
- Location: `src/c/` (implementation), `include/quiver/c/` (public headers)
- Contains: One `.cpp` per functional domain (`database.cpp`, `database_read.cpp`, `database_create.cpp`, `database_update.cpp`, `database_delete.cpp`, `database_metadata.cpp`, `database_query.cpp`, `database_time_series.cpp`, `database_transaction.cpp`, `database_csv_export.cpp`, `database_csv_import.cpp`, `element.cpp`, `lua_runner.cpp`)
- Depends on: C++ core library
- Used by: Julia, Dart, Python, Lua bindings

**Language Bindings:**
- Purpose: Thin FFI wrappers providing idiomatic API surfaces for each language
- Location: `bindings/julia/src/`, `bindings/dart/lib/src/`, `bindings/python/src/quiverdb/`
- Contains: Per-language wrappers for `Database`, `Element`, `LuaRunner`, metadata types; no logic beyond type marshaling and error surfacing
- Depends on: C API (`libquiver_c.dll`/`.so`/`.dylib`)
- Used by: End users in each language

**CLI:**
- Purpose: Command-line database tool that runs Lua scripts against a database
- Location: `src/cli/main.cpp`
- Depends on: C++ core library, argparse
- Used by: End users via `quiver_cli.exe`

**Tests:**
- Purpose: Validation of C++ core and C API correctness
- Location: `tests/`
- Contains: `test_database_*.cpp` (C++ API tests), `test_c_api_database_*.cpp` (C API tests)
- Depends on: C++ core library, C API library, Catch2

## Data Flow

**Write Path (create/update):**

1. Caller constructs `Element` using builder API (`Element().set("label", "X").set("value", 42)`)
2. `Database::create_element(collection, element)` is called
3. `Impl::resolve_element_fk_labels` resolves any string labels to FK integer IDs via schema lookup
4. `TypeValidator::validate_scalar` checks types against schema
5. `Impl::TransactionGuard` starts a SQLite transaction (no-op if already in a transaction)
6. SQL `INSERT` executes against the main collection table; arrays route to `_vector_`, `_set_`, or `_time_series_` tables via schema table classification
7. `TransactionGuard` commits on scope exit (or rolls back on exception)

**Read Path:**

1. Caller invokes `Database::read_scalar_integers(collection, attribute)` or similar
2. C++ constructs SQL using schema-introspected table names
3. Executes parameterized SQL via `Database::execute(sql, params)` -- returns `Result` (vector of `Row`)
4. Data is converted from `Row`/`Value` to `std::vector<T>` and returned
5. C API layer marshals `std::vector<T>` to heap-allocated C arrays via `read_scalars_impl<T>` template or `copy_strings_to_c`
6. Caller must call corresponding `quiver_database_free_*` to release memory

**C API Error Flow:**

1. C API function calls C++ method inside `try { } catch (const std::exception& e) { ... }`
2. On exception: `quiver_set_last_error(e.what())` stores message in thread-local storage, returns `QUIVER_ERROR = 1`
3. On success: returns `QUIVER_OK = 0`
4. Caller retrieves details via `quiver_get_last_error()` if return code is `QUIVER_ERROR`

**Binding Error Flow:**

1. Binding calls C API function
2. Checks return code against `QUIVER_OK`
3. On error: calls `quiver_get_last_error()` to retrieve message and raises language-native exception (Julia `DatabaseException`, Dart `QuiverException`, Python `QuiverError`)

**State Management:**

- `Database::Impl` holds: `sqlite3*` handle, `std::string path`, spdlog logger, `std::unique_ptr<Schema>`, `std::unique_ptr<TypeValidator>`
- `Schema` is loaded once at database open and cached in-memory for all subsequent operations
- Transaction state tracked by SQLite itself (`sqlite3_get_autocommit()`); `TransactionGuard` queries this to avoid double-begin
- Thread-local error storage in C API (`thread_local std::string last_error`)

## Key Abstractions

**Database (C++):**
- Purpose: Primary API -- lifecycle, CRUD, read, query, metadata, CSV, transactions
- Location: `include/quiver/database.h`, `src/database.cpp`, `src/database_create.cpp`, `src/database_read.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`, `src/database_query.cpp`, `src/database_metadata.cpp`, `src/database_time_series.cpp`, `src/database_describe.cpp`, `src/database_csv_export.cpp`, `src/database_csv_import.cpp`
- Pattern: Pimpl idiom -- `Database::Impl` in `src/database_impl.h` holds SQLite handle and all private state. `Database` is the public handle with deleted copy, defaulted move.

**Element (C++):**
- Purpose: Builder for element creation and update data
- Location: `include/quiver/element.h`, `src/element.cpp`
- Pattern: Fluent builder, plain value type (Rule of Zero). Stores `map<string, Value>` for scalars and `map<string, vector<Value>>` for arrays.

**Schema:**
- Purpose: In-memory representation of the SQLite schema; routes operations to correct tables
- Location: `include/quiver/schema.h`, `src/schema.cpp`
- Pattern: Loaded via `Schema::from_database(sqlite3*)`, cached in `Impl`. Provides `find_vector_table()`, `find_set_table()`, `find_time_series_table()`, `is_collection()`, `find_table_for_column()`, etc.

**TransactionGuard:**
- Purpose: Nest-aware RAII transaction management for write operations
- Location: `src/database_impl.h` (nested in `Database::Impl`)
- Pattern: On construction, checks `sqlite3_get_autocommit()`. If autocommit is on (no active transaction), begins a transaction and sets `owns_transaction_ = true`. On destructor, rolls back if not committed. No-op if a user explicit transaction is already active.

**Value:**
- Purpose: Typed union for SQLite column values
- Location: `include/quiver/value.h`
- Pattern: `using Value = std::variant<std::nullptr_t, int64_t, double, std::string>`

**quiver_database_t (C API):**
- Purpose: Opaque handle wrapping `quiver::Database` for FFI
- Location: `src/c/internal.h`
- Pattern: Plain struct `struct quiver_database { quiver::Database db; }` -- lifetime managed by `new`/`delete` (`quiver_database_close` calls `delete db`)

## Entry Points

**C++ Library:**
- Location: `include/quiver/database.h`
- Triggers: Direct C++ usage or included by C API layer
- Responsibilities: Full database lifecycle, all data operations

**C API:**
- Location: `include/quiver/c/database.h`, `include/quiver/c/common.h`, `include/quiver/c/options.h`, `include/quiver/c/element.h`, `include/quiver/c/lua_runner.h`
- Triggers: FFI calls from Julia, Dart, Python; callable from C/C++ directly
- Responsibilities: ABI-stable surface, error code returns, memory allocation/free pairs

**CLI Executable:**
- Location: `src/cli/main.cpp`
- Triggers: `./build/bin/quiver_cli.exe <database> <script> [--schema ...] [--migrations ...]`
- Responsibilities: Opens database, reads Lua script file, runs it via `LuaRunner`, exits with code 0/1/2

**Julia Entry Point:**
- Location: `bindings/julia/src/Quiver.jl`
- Triggers: `using Quiver` in Julia code
- Responsibilities: Loads `libquiver_c`, exports `Database`, `Element`, `LuaRunner`, metadata types

**Dart Entry Point:**
- Location: `bindings/dart/lib/src/database.dart` (assembled via `part` directives)
- Triggers: Dart `import 'package:quiver/quiver.dart'`
- Responsibilities: Loads shared library via `bindings/dart/lib/src/ffi/library_loader.dart`, exposes `Database` class with all operations split across `part` files

**Python Entry Point:**
- Location: `bindings/python/src/quiverdb/__init__.py`
- Triggers: `import quiverdb` or `from quiverdb import Database`
- Responsibilities: CFFI ABI-mode loading via `_loader.py` (pre-loads `libquiver.dll` on Windows), exposes `Database`, `Element`, metadata types

## Error Handling

**Strategy:** Exceptions in C++, converted to binary error codes at the C API boundary, then re-raised as language-native exceptions in bindings.

**Patterns:**
- C++ throws `std::runtime_error` with one of three structured message formats: `"Cannot {op}: {reason}"`, `"{Entity} not found: {identifier}"`, `"Failed to {op}: {reason}"`
- C API catches all `std::exception`, stores `.what()` in thread-local storage, returns `QUIVER_ERROR`
- `QUIVER_REQUIRE(ptr1, ptr2, ...)` macro in `src/c/internal.h` validates non-null pointers, sets `"Null argument: {name}"` error, returns `QUIVER_ERROR` without entering try/catch
- Bindings check return code, call `quiver_get_last_error()`, raise language-native exceptions
- Rollback in `TransactionGuard` destructor does not throw (errors logged, not propagated) because rollback is invoked in error-recovery paths

## Cross-Cutting Concerns

**Logging:** spdlog, instance-per-database with unique logger names. Console sink (stderr, configurable level via `DatabaseOptions.console_level`). File sink (`{db_dir}/{db_name}.log`). In-memory databases skip file logging. Levels: DEBUG, INFO, WARN, ERROR, OFF.

**Validation:** Two-stage -- `SchemaValidator` (`src/schema_validator.cpp`) validates structural schema rules at database open; `TypeValidator` (`src/type_validator.cpp`) validates values against schema types at write time. Both consume the cached `Schema`.

**Authentication:** Not applicable -- SQLite file-based, no auth layer.

**Schema Introspection:** `Schema::from_database(sqlite3*)` queries SQLite internals at open time to load all table definitions, columns, foreign keys, and indexes into memory. Subsequent operations use cached `Schema` for routing (no repeated metadata queries at runtime).

**Memory Ownership (C API):** Every function that allocates heap memory (`new[]`) has a matching `quiver_database_free_*` or `quiver_element_free_*` function in the same translation unit. Alloc/free pairs are co-located: read pairs in `src/c/database_read.cpp`, metadata pairs in `src/c/database_metadata.cpp`, time series pairs in `src/c/database_time_series.cpp`.

---

*Architecture analysis: 2026-02-27*
