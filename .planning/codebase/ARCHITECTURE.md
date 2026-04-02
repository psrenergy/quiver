# Architecture

**Analysis Date:** 2026-03-08

## Pattern Overview

**Overall:** Layered FFI Library with Pimpl Core

**Key Characteristics:**
- C++20 core library exposes a clean OO API; a C API wraps it for cross-language FFI
- Thin language bindings (Julia, Dart, Python) sit above the C API — they contain no logic
- SQLite is the sole storage engine; all SQL is generated dynamically from in-memory Schema introspection
- Pimpl pattern hides external dependencies (sqlite3, spdlog, Lua) from public headers
- Logic lives exclusively in the C++ layer; bindings only marshal types and delegate

## Layers

**C++ Core (`include/quiver/`, `src/`):**
- Purpose: All business logic — CRUD, query, schema validation, type coercion, transactions, CSV I/O
- Location: `include/quiver/*.h`, `src/*.cpp`, `src/*.h`
- Contains: `Database`, `Element`, `Schema`, `SchemaValidator`, `TypeValidator`, `LuaRunner`, `Migration`, `Migrations`, value types
- Depends on: sqlite3, spdlog, sol2 (Lua), toml++ (blob), argparse (CLI)
- Used by: C API layer, CLI

**C API Layer (`include/quiver/c/`, `src/c/`):**
- Purpose: ABI-stable C interface for FFI; wraps C++ objects in opaque handle structs
- Location: `include/quiver/c/*.h`, `src/c/*.cpp`, `src/c/*.h`
- Contains: `quiver_database_t`, `quiver_element_t`, `quiver_blob_t`, `quiver_blob_metadata_t` opaque handles; all `quiver_*` functions
- Depends on: C++ core (via `internal.h` structs that embed C++ objects)
- Used by: Julia (via `ccall`), Dart (via `dart:ffi`), Python (via CFFI)

**Blob Subsystem (`include/quiver/blob/`, `src/blob/`):**
- Purpose: Standalone binary file I/O for `.qvr` files with TOML metadata sidecars; orthogonal to the database layer
- Location: `include/quiver/blob/*.h`, `src/blob/*.cpp`
- Contains: `Blob` (Pimpl), `BlobCSV` (Pimpl), `BlobMetadata`, `Dimension`, `TimeProperties`, `TimeFrequency`
- Depends on: toml++, standard file I/O
- Used by: Blob C API (`include/quiver/c/blob/`, `src/c/blob/`), Julia blob bindings

**Language Bindings (`bindings/`):**
- Purpose: Idiomatic wrappers in each target language; thin delegation to C API
- Location: `bindings/julia/src/`, `bindings/dart/lib/src/`, `bindings/python/src/quiverdb/`
- Contains: Database, Element, LuaRunner, BlobMetadata, Blob, BlobCSV wrappers; convenience composites
- Depends on: C API shared libraries (`libquiver.dll`, `libquiver_c.dll`)
- Used by: End users of each language

**CLI (`src/cli/main.cpp`):**
- Purpose: Standalone executable that opens a database and executes a Lua script
- Location: `src/cli/main.cpp`
- Contains: Argument parsing, database construction, `LuaRunner` invocation
- Depends on: C++ core (`Database`, `LuaRunner`), argparse

## Data Flow

**Write Path (create/update element):**
1. Caller constructs `Element` with `set()` calls (fluent builder)
2. `Database::create_element(collection, element)` called
3. `Impl::resolve_element_fk_labels()` resolves string labels to IDs for FK columns via `Schema` lookup
4. `Impl::insert_group_data()` routes arrays to vector/set/time-series tables using `Schema::find_all_tables_for_column()`
5. `Impl::TransactionGuard` wraps SQL execution; auto-begins if no explicit transaction is active
6. `Database::execute()` prepares and executes SQLite statements

**Read Path (scalar/vector/set):**
1. Caller invokes e.g. `Database::read_scalar_integers(collection, attribute)`
2. `Database::execute()` runs `SELECT id, {attr} FROM {collection}` via SQLite
3. Results returned as `Result` (vector of `Row`); `Row::get_integer/float/string()` extract typed values
4. Internal templates in `src/database_internal.h` (`read_column_values`, `read_grouped_values_all`) handle typed extraction

**C API Delegation:**
1. C function (e.g. `quiver_database_create_element`) checks args with `QUIVER_REQUIRE`
2. Calls C++ method on embedded `quiver::Database` inside `quiver_database` struct
3. On exception: `quiver_set_last_error(e.what())` + return `QUIVER_ERROR`
4. On success: out-parameters populated, return `QUIVER_OK`

**Binding Delegation (Julia example):**
1. Julia calls `C.quiver_database_create_element(db.ptr, collection, elem.ptr, out_id)`
2. Return code checked via `check()` — throws `DatabaseException` on `QUIVER_ERROR`
3. `quiver_get_last_error()` provides the message when an error is signalled

**State Management:**
- All state lives in `Database::Impl` (sqlite3 handle, schema, logger)
- `Schema` is loaded once at database open and cached for the lifetime of the `Database`
- Transaction state is tracked via `sqlite3_get_autocommit()` — no separate bool flag

## Key Abstractions

**`Database` / `quiver_database_t`:**
- Purpose: Primary entry point; owns sqlite3 connection via Pimpl
- Examples: `include/quiver/database.h`, `src/c/internal.h`
- Pattern: Pimpl; non-copyable, movable; factory static methods `from_schema()`, `from_migrations()`

**`Schema` (introspection model):**
- Purpose: In-memory representation of the database's table/column/FK structure; drives all SQL generation
- Examples: `include/quiver/schema.h`, `src/schema.cpp`
- Pattern: Loaded via `Schema::from_database(sqlite3*)` at open time; queried by all write/read paths to resolve table names and column types

**`Element` (write builder):**
- Purpose: Fluent builder for data to be written; holds scalars and arrays in typed maps
- Examples: `include/quiver/element.h`
- Pattern: Plain value type; `set(name, value)` returns `Element&` for chaining; arrays routed by `Database` to the correct group table

**`Value` (typed variant):**
- Purpose: Uniform representation for scalar values across all operations
- Examples: `include/quiver/value.h`
- Pattern: `std::variant<std::nullptr_t, int64_t, double, std::string>`

**`Impl::TransactionGuard` (nest-aware RAII):**
- Purpose: Auto-begins a transaction for write operations; no-op if an explicit transaction is already active
- Examples: `src/database_impl.h`
- Pattern: Checks `sqlite3_get_autocommit()` in constructor; rolls back on destruction if not committed

**`Blob` / `BlobCSV` (binary file I/O):**
- Purpose: Read/write typed multi-dimensional float arrays from `.qvr` binary files with TOML metadata sidecars
- Examples: `include/quiver/blob/blob.h`, `include/quiver/blob/blob_csv.h`
- Pattern: Pimpl; factory static method `Blob::open(path, mode, metadata?)`

**`LuaRunner` (scripting):**
- Purpose: Executes Lua scripts with the database exposed as `db` userdata
- Examples: `include/quiver/lua_runner.h`, `src/lua_runner.cpp`
- Pattern: Pimpl (hides sol2/Lua headers); receives `Database&` reference

## Entry Points

**C++ Library (`libquiver`):**
- Location: `include/quiver/quiver.h` (umbrella), `include/quiver/database.h`
- Triggers: Linked into consuming applications or the C API shared library
- Responsibilities: Full database lifecycle, CRUD, queries, CSV, blob operations

**C API Shared Library (`libquiver_c`):**
- Location: `include/quiver/c/database.h`, `include/quiver/c/blob/blob.h`
- Triggers: Loaded via FFI by Julia/Dart/Python bindings
- Responsibilities: ABI-stable wrapper; exposes all C++ functionality as C functions with opaque handles

**CLI Executable (`quiver_cli`):**
- Location: `src/cli/main.cpp` → built to `build/bin/quiver_cli.exe`
- Triggers: Command line invocation with `quiver_cli <database> <script.lua> [--schema ...] [--migrations ...]`
- Responsibilities: Parse args, open database, run Lua script via `LuaRunner`

## Error Handling

**Strategy:** Exceptions in C++ layer; binary error codes in C API; exception re-throws in bindings.

**Patterns:**
- C++ throws `std::runtime_error` with one of three message formats: `"Cannot {op}: {reason}"`, `"{Entity} not found: {id}"`, `"Failed to {op}: {reason}"`
- C API catches all exceptions: `quiver_set_last_error(e.what())` + return `QUIVER_ERROR`
- Callers retrieve the message via `quiver_get_last_error()` (thread-local storage)
- Null argument validation: `QUIVER_REQUIRE(ptr1, ptr2, ...)` macro auto-generates `"Null argument: {name}"` error and returns `QUIVER_ERROR`
- Bindings translate `QUIVER_ERROR` return code to a language-native exception (e.g. `DatabaseException` in Julia, `QuiverError` in Python/Dart)

## Cross-Cutting Concerns

**Logging:** spdlog with per-`Database`-instance logger; console + file sinks. File sink writes to `{db_path}.log`. In-memory databases use console-only. Log level configured via `DatabaseOptions::console_level`.

**Validation:** `SchemaValidator` validates the schema on open. `TypeValidator` validates column types during writes. FK label resolution happens in `Impl::resolve_element_fk_labels()` before any INSERT.

**Memory Management:** C++ uses RAII throughout. C API uses `new`/`delete` with matching `quiver_{entity}_free_*` functions. Alloc and free for each data category co-locate in the same translation unit (e.g. `src/c/database_read.cpp` for read results, `src/c/database_metadata.cpp` for metadata).

**Schema Naming Conventions (SQL):** Vector tables: `{Collection}_vector_{name}`; Set tables: `{Collection}_set_{name}`; Time series tables: `{Collection}_time_series_{name}`; Time series files: `{Collection}_time_series_files`. All derived by `Schema::vector_table_name()` etc.

---

*Architecture analysis: 2026-03-08*
