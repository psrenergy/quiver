# Architecture

**Analysis Date:** 2026-02-26

## Pattern Overview

**Overall:** Layered FFI Library with Thin Bindings

**Key Characteristics:**
- All logic lives in the C++ core (`libquiver`); higher layers are strictly thin wrappers
- A C API shim (`libquiver_c`) wraps the C++ core as an ABI-stable FFI surface
- Language bindings (Julia, Dart, Python) call the C API exclusively — never the C++ layer directly
- Schema-introspective: the database loads and validates its own schema at open-time, which drives routing of reads/writes to correct tables
- RAII throughout: `Database` (Pimpl), `LuaRunner` (Pimpl), `Element` (value type), `Row`/`Result`/`Migration` (value types); resource ownership is explicit and unambiguous

## Layers

**C++ Core Library (`libquiver`):**
- Purpose: All database logic, SQL generation, schema introspection, type validation, migrations, CSV, Lua scripting
- Public headers: `include/quiver/`
- Implementation: `src/*.cpp`
- Main class: `quiver::Database` — Pimpl with `sqlite3*` inside `Database::Impl`
- Depends on: SQLite3, spdlog, sol2/Lua, rapidcsv, tomlplusplus
- Used by: C API shim, tests

**C API Shim (`libquiver_c`):**
- Purpose: C-linkage FFI surface; wraps C++ types in opaque handles; marshals errors to thread-local string + binary return codes
- Public headers: `include/quiver/c/`
- Implementation: `src/c/*.cpp`
- Opaque handles: `quiver_database_t` (wraps `quiver::Database`), `quiver_element_t` (wraps `quiver::Element`)
- Shared internals: `src/c/internal.h` (handle structs, `QUIVER_REQUIRE` macro), `src/c/database_helpers.h` (marshaling templates, `strdup_safe`, metadata converters)
- Depends on: `libquiver`
- Used by: Julia, Dart, Python bindings

**Language Bindings:**
- Purpose: Thin, idiomatic wrappers in each host language; no logic, no error message crafting
- Julia: `bindings/julia/src/` — uses `@ccall` via generated `c_api.jl`; loads `libquiver_c.dll/.so/.dylib`
- Dart: `bindings/dart/lib/src/` — uses `dart:ffi`; generated bindings in `ffi/bindings.dart`
- Python: `bindings/python/src/quiverdb/` — uses CFFI ABI-mode; hand-written cdef in `_c_api.py`, loader in `_loader.py`
- Depends on: `libquiver_c`

**Blob Subsystem (experimental):**
- Purpose: Binary flat-file storage for multi-dimensional numerical data with TOML metadata
- Headers: `include/quiver/blob/`
- Implementation: `src/blob/`
- Not exposed through C API or bindings; standalone subsystem

## Data Flow

**Write Operation (create_element):**

1. Binding calls `quiver_database_create_element(db, collection, element, &id)` in C API
2. `src/c/database_create.cpp` unwraps handles, calls `db->db.create_element(collection, element)`
3. C++ `Database::create_element` validates collection exists via `Schema` introspection
4. `Impl::resolve_element_fk_labels` resolves string FK references to integer IDs before any writes
5. `TypeValidator` checks resolved scalar types against schema column definitions
6. `Impl::TransactionGuard` begins a SQLite transaction (no-op if one is already active — nest-aware)
7. SQL INSERT built dynamically from element scalars; array fields routed to `_vector_`, `_set_`, or `_time_series_` tables via `Schema::find_table_for_column`
8. `TransactionGuard` commits; auto-rollback on exception
9. C API returns `QUIVER_OK` and writes inserted id to out-parameter

**Read Operation:**

1. Binding calls typed read function (e.g., `quiver_database_read_scalar_integers`)
2. `src/c/database_read.cpp` calls `db->db.read_scalar_integers(collection, attribute)`
3. C++ executes parameterized SQL via internal `Database::execute`, returning `Result` (rows of `Value` variants)
4. C API marshals result to C arrays using `database_helpers.h` templates (`read_scalars_impl`, `copy_strings_to_c`)
5. Caller receives typed pointer + count out-parameters; must call matching `quiver_database_free_*` function to release memory

**Schema Loading at Open Time:**

1. `Database` constructor opens SQLite file, calls `Impl::load_schema_metadata()`
2. `Schema::from_database(sqlite3*)` queries `sqlite_schema` to build full `TableDefinition` map
3. `SchemaValidator` validates Quiver conventions: Configuration table exists, collections have `id`/`label`, vector/set tables have correct FK and UNIQUE constraints
4. `TypeValidator` constructed from loaded schema for runtime type-checking on all writes

**Transaction Flow:**

1. Explicit transactions: caller calls `begin_transaction` then `commit`/`rollback`
2. Internal writes use `Impl::TransactionGuard`: checks `sqlite3_get_autocommit()` — if an explicit transaction is active, guard becomes a no-op; otherwise it begins and owns the transaction
3. Same C++ write methods work correctly both standalone and inside explicit transaction blocks

## Key Abstractions

**`quiver::Database` — Main API class:**
- Purpose: Single facade for all database operations
- Location: `include/quiver/database.h`; implementation split across `src/database.cpp`, `src/database_create.cpp`, `src/database_read.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`, `src/database_metadata.cpp`, `src/database_time_series.cpp`, `src/database_query.cpp`, `src/database_csv_export.cpp`, `src/database_csv_import.cpp`, `src/database_describe.cpp`
- Pattern: Pimpl (`struct Impl` in `src/database_impl.h`); hides `sqlite3*`, `Schema`, `TypeValidator`, and spdlog logger from public headers

**`quiver::Schema` — Runtime schema model:**
- Purpose: In-memory representation of all SQLite tables, columns, FKs, and indexes; drives routing and validation
- Location: `include/quiver/schema.h`, `src/schema.cpp`
- Pattern: Value type; loaded once per `Database` open via `Schema::from_database(sqlite3*)`
- Key operations: `find_table_for_column` (routes array writes), `is_collection`/`is_vector_table`/`is_set_table`/`is_time_series_table` (table classification), `vector_table_name`/`set_table_name`/`time_series_table_name` (naming convention enforcement)

**`quiver::Element` — Write input builder:**
- Purpose: Fluent builder for element creation and updates; holds scalars and arrays
- Location: `include/quiver/element.h`, `src/element.cpp`
- Pattern: Plain value type (Rule of Zero); no Pimpl
- Usage: `Element().set("label", "x").set("value", 42).set("tags", {"a", "b"})`

**`quiver::Value` — Type-safe scalar variant:**
- Purpose: Holds any supported data value
- Location: `include/quiver/value.h`
- Definition: `using Value = std::variant<std::nullptr_t, int64_t, double, std::string>`

**`quiver::Result` / `quiver::Row` — Internal query results:**
- Purpose: Typed result set returned by `Database::execute` (internal only); `Row` provides per-column typed accessors via `get_integer`/`get_float`/`get_string` returning `std::optional`
- Location: `include/quiver/result.h`, `include/quiver/row.h`
- Pattern: Plain value types; iterable

**`quiver_database_t` / `quiver_element_t` — C API opaque handles:**
- Purpose: Wrap C++ objects for C-linkage FFI callers
- Location: `src/c/internal.h`
- `quiver_database` struct: contains `quiver::Database db` member
- `quiver_element` struct: contains `quiver::Element element` member

**`quiver::SchemaValidator` — Schema convention enforcer:**
- Purpose: Validates loaded schema follows Quiver conventions at `Database` open time
- Location: `include/quiver/schema_validator.h`, `src/schema_validator.cpp`
- Throws `std::runtime_error` on any convention violation

**`quiver::TypeValidator` — Runtime type checker:**
- Purpose: Validates values match schema column types before writes
- Location: `include/quiver/type_validator.h`, `src/type_validator.cpp`

**`quiver::LuaRunner` — Embedded scripting host:**
- Purpose: Executes Lua scripts with a bound `db` userdata exposing the full Database API
- Location: `include/quiver/lua_runner.h`, `src/lua_runner.cpp`
- Pattern: Pimpl (hides sol2/Lua headers from public API)

**`quiver::Migration` / `quiver::Migrations` — Schema versioning:**
- Purpose: Load numbered migration directories (`1/up.sql`, `2/up.sql`, …); apply pending migrations in order
- Location: `include/quiver/migration.h`, `include/quiver/migrations.h`, `src/migration.cpp`, `src/migrations.cpp`
- Pattern: Plain value types

## Entry Points

**C++ Core:**
- Direct open: `Database db(path, options)` — opens existing database
- Factory (schema): `Database::from_schema(db_path, schema_path)` — creates/opens with SQL schema file
- Factory (migrations): `Database::from_migrations(db_path, migrations_path)` — applies pending versioned migrations

**C API:**
- `quiver_database_open(path, options, &out_db)` in `src/c/database.cpp`
- `quiver_database_from_schema(db_path, schema_path, options, &out_db)` in `src/c/database.cpp`
- `quiver_database_from_migrations(db_path, migrations_path, options, &out_db)` in `src/c/database.cpp`
- All return `quiver_error_t`; error detail via `quiver_get_last_error()`

**Bindings:**
- Julia: `Quiver.from_schema(db_path, schema_path)` → `Database` — `bindings/julia/src/database.jl`
- Dart: `Database.fromSchema(dbPath, schemaPath)` → `Database` — `bindings/dart/lib/src/database.dart`
- Python: `Database.from_schema(db_path, schema_path)` → `Database` — `bindings/python/src/quiverdb/database.py`

## Error Handling

**Strategy:** C++ exceptions caught at the C API boundary; converted to binary return codes plus thread-local error strings; bindings re-raise as native exceptions

**Patterns:**
- C++ throws `std::runtime_error` with exactly three message formats: `"Cannot {op}: {reason}"`, `"{Entity} not found: {id}"`, `"Failed to {op}: {reason}"`
- C API wraps every function in `try { ... return QUIVER_OK; } catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`
- `QUIVER_REQUIRE(args...)` macro (`src/c/internal.h`) validates non-null pointer arguments before the try-block; auto-generates `"Null argument: {name}"` messages via stringification
- Bindings check return code with a `check()` helper and raise language-native exceptions using `quiver_get_last_error()` for the message (Julia: `DatabaseException`, Dart: `QuiverException`, Python: `QuiverError`)
- Rollback does not throw — errors during rollback are logged only

## Cross-Cutting Concerns

**Logging:** Per-instance spdlog logger (`std::shared_ptr<spdlog::logger>` in `Database::Impl`); dual-sink to stdout (colored, configurable level) and `quiver_database.log` file in the database directory; in-memory databases log to stdout only

**Validation:** Schema validation at open time (`SchemaValidator`); type validation at write time (`TypeValidator`); FK label-to-ID resolution before writes (`Impl::resolve_element_fk_labels` in `src/database_impl.h`)

**Memory Ownership in C API:** All heap allocations by C API functions are matched with explicit `quiver_database_free_*` / `quiver_element_free_*` free functions co-located in the same translation unit as the allocating function (`database_read.cpp` owns read alloc+free pairs, `database_metadata.cpp` owns metadata alloc+free pairs, etc.)

**Thread Safety:** SQLite initialized once via `std::call_once` in `src/database.cpp`; each `Database` instance has its own connection and logger; thread-local storage for C API error messages (`quiver_get_last_error`)

---

*Architecture analysis: 2026-02-26*
