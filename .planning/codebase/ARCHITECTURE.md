# Architecture

**Analysis Date:** 2026-02-25

## Pattern Overview

**Overall:** Layered architecture with domain logic in C++ core, C API for FFI interop, and thin language bindings.

**Key Characteristics:**
- Three distinct layers: C++ core -> C API -> language bindings (Julia, Dart, Python, Lua)
- Pimpl pattern for classes hiding private dependencies (`Database`, `LuaRunner`, `BlobCSV`)
- Value types (Rule of Zero) for simple data structures without private state
- Logic lives in C++ layer; bindings are mechanical and thin
- Strict separation between public interfaces (`include/`) and implementation (`src/`)
- Schema-driven validation and type checking loaded once at open, cached in `Database::Impl`
- Explicit transaction control with nest-aware RAII guards (`TransactionGuard`)
- Two shared libraries: `libquiver` (C++ core) and `libquiver_c` (C API wrapper)

## Layers

**C++ Core Layer:**
- Purpose: Domain logic, database operations, schema validation, type checking, blob I/O
- Location: `include/quiver/` (public headers), `src/` (implementations)
- Contains: `Database` (Pimpl), `Element` builder, `LuaRunner` (Pimpl), `Schema`, `SchemaValidator`, `TypeValidator`, `Blob`/`BlobCSV` subsystem, metadata types, options, value types
- Depends on: SQLite3, spdlog, Lua/sol2 (LuaRunner), RapidCSV (CSV), toml++ (Blob metadata)
- Used by: C API layer wraps all public methods

**C API Layer:**
- Purpose: FFI-safe C interface for language bindings
- Location: `include/quiver/c/` (header contracts), `src/c/` (thin wrappers)
- Contains: Function definitions wrapping each C++ public method; error handling via `quiver_set_last_error()`
- Depends on: C++ core layer
- Used by: Julia, Dart, Python, and Lua bindings call C API
- Key pattern: Try-catch with binary return codes (`QUIVER_OK`/`QUIVER_ERROR`), values via output parameters

**Language Bindings:**
- Purpose: Idiomatic language-specific wrappers
- Location: `bindings/julia/`, `bindings/dart/`, `bindings/python/`
- Contains: FFI stubs, type conversions, convenience methods (transaction blocks, composite readers, DateTime wrappers)
- Depends on: C API layer only (via FFI/CFFI/Dart FFI)
- Used by: End-user applications in Julia, Dart, Python

**Lua Binding (embedded):**
- Purpose: Scripting access to the database via sol2
- Location: `src/lua_runner.cpp` (within C++ core library)
- Not a separate binary; Lua is available through `LuaRunner` in C++ and exposed via C API

## Data Flow

**Create Element Flow:**

1. User calls `database.create_element(collection, element)` in C++
2. `Element` builder holds `scalars` (`map<string, Value>`) and `arrays` (`map<string, vector<Value>>`)
3. `Database::create_element` in `src/database_create.cpp`:
   - Resolves FK labels (string values on FK columns looked up to IDs via `Impl::resolve_element_fk_labels`)
   - Validates resolved scalar types via `TypeValidator`
   - Acquires `Impl::TransactionGuard` (nest-aware: no-op if caller is in an explicit transaction)
   - INSERTs scalars into the main collection table via `execute()`
   - Retrieves `sqlite3_last_insert_rowid()` as element ID
   - Routes each array column to vector, set, or time series tables via `Schema::find_all_tables_for_column()`
   - Zips multi-column arrays into rows; vector tables add `vector_index`
   - Commits `TransactionGuard` on success; RAII rollback on exception
4. C API wrapper in `src/c/database_create.cpp` catches exceptions, calls `quiver_set_last_error()`, returns `QUIVER_ERROR`
5. Language binding retrieves error via `quiver_get_last_error()`, throws idiomatic exception

**Read Scalar Flow:**

1. User calls `database.read_scalar_integers(collection, attribute)`
2. `Database` verifies collection and attribute exist via cached `Schema`
3. Executes `SELECT attribute FROM collection`
4. Returns `vector<int64_t>` from `Result` row iteration
5. C API wrapper in `src/c/database_read.cpp` allocates `new int64_t[]`, copies data, sets `out_values` and `out_count`
6. Language binding wraps pointer in language array type; caller frees with `quiver_database_free_integer_array()`

**Query Flow:**

1. User calls `database.query_string(sql, params)` with optional `vector<Value>`
2. Params bound via `std::visit` on `Value` variant to SQLite bind APIs
3. Statement stepped; first row first column extracted
4. Returns `optional<T>` (absent if query returns no rows)
5. C API parameterized variants (`*_params`) use `convert_params()` in `src/c/database_query.cpp` to marshal parallel typed arrays into `vector<Value>`

**Time Series Multi-Column Flow:**

1. User calls `database.read_time_series_group(collection, group, id)`
2. Schema identifies the time series table via `Schema::find_time_series_table()`
3. Returns `vector<map<string, Value>>` -- one map per row, keyed by column name
4. C API returns columnar typed arrays: `char*** out_column_names`, `int** out_column_types`, `void*** out_column_data`
5. `quiver_database_free_time_series_data()` deallocates using column type info to choose per-element vs bulk delete

**Schema Initialization Flow:**

1. `Database` constructor opens SQLite file via `sqlite3_open_v2()`
2. Factory methods (`from_schema`, `from_migrations`) apply SQL, then call `Impl::load_schema_metadata()`
3. `load_schema_metadata()` creates `Schema::from_database()`, runs `SchemaValidator::validate()`, constructs `TypeValidator`
4. Schema cached in `impl_->schema`; all operations reference this cached schema

**State Management:**
- Database lifecycle managed via RAII (`Impl` destructor calls `sqlite3_close_v2()`, spdlog logger auto-cleaned)
- Transactions explicit: `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()` on `Database`
- `TransactionGuard` in `src/database_impl.h` nest-aware: only owns transaction if `sqlite3_get_autocommit()` returns true
- Write operations (`create_element`, `update_*`) use `TransactionGuard` internally for standalone commits or to join explicit transactions
- Error state thread-local in C API layer via `quiver_set_last_error()`

## Key Abstractions

**Database (Pimpl Pattern):**
- Purpose: Main API gateway for all database operations
- Files: `include/quiver/database.h`, `src/database.cpp`, `src/database_impl.h`
- `Database::Impl` (in `src/database_impl.h`): holds `sqlite3*`, `path`, `shared_ptr<spdlog::logger>`, `unique_ptr<Schema>`, `unique_ptr<TypeValidator>`
- Operations split across: `src/database_create.cpp`, `src/database_read.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`, `src/database_query.cpp`, `src/database_metadata.cpp`, `src/database_time_series.cpp`, `src/database_csv.cpp`, `src/database_describe.cpp`
- Private helpers: `src/database_impl.h` (Impl struct + TransactionGuard), `src/database_internal.h` (template helpers for read operations)

**Element (Value Type):**
- Purpose: Builder for element creation; holds field values before a `create_element` call
- Files: `include/quiver/element.h`, `src/element.cpp`
- Pattern: Rule of Zero with fluent `set()` overloads; holds `map<string, Value> scalars_` and `map<string, vector<Value>> arrays_`
- Arrays are type-erased here; routing to vector/set/time-series happens in `Database::create_element`

**Schema (Value Type):**
- Purpose: Parse and cache database structure from SQLite schema tables
- Files: `include/quiver/schema.h`, `src/schema.cpp`
- Loaded via `Schema::from_database(sqlite3*)` at open time
- Provides table classification (`is_collection()`, `is_vector_table()`, etc.), naming convention helpers (`vector_table_name()`, `set_table_name()`, `time_series_table_name()`), column routing (`find_all_tables_for_column()`)

**SchemaValidator (Value Type):**
- Purpose: Validate that a schema follows Quiver conventions
- Files: `include/quiver/schema_validator.h`, `src/schema_validator.cpp`
- Checks: Configuration table exists, collections have id/label columns, vector tables have proper FK and `vector_index`, set tables have UNIQUE constraint, no duplicate attributes
- Throws `std::runtime_error` on first violation

**TypeValidator (Value Type):**
- Purpose: Runtime type checking for scalar and array values before insert
- Files: `include/quiver/type_validator.h`, `src/type_validator.cpp`
- Methods: `validate_scalar(table, column, value)`, `validate_array(table, column, values)`
- Uses `Schema` reference to look up expected `DataType` per column

**Value (Type Alias):**
- Purpose: Type-safe variant for all scalar values throughout the system
- File: `include/quiver/value.h`
- Definition: `using Value = std::variant<std::nullptr_t, int64_t, double, std::string>`
- Used by: `Element` scalars/arrays, parameterized query params, time series row maps, `Row` storage

**Result / Row (Value Types):**
- Purpose: Hold SQL query results as typed value collections
- Files: `include/quiver/result.h`, `include/quiver/row.h`
- `Row` wraps `vector<Value>` with typed getters (`get_integer()`, `get_float()`, `get_string()` returning `optional<T>`)
- `Result` wraps `vector<string> columns_` and `vector<Row> rows_`; used only internally (not exposed in public C API)

**Metadata Types (Value Types):**
- Purpose: Describe schema structure for attributes and groups
- File: `include/quiver/attribute_metadata.h`
- `ScalarMetadata`: name, data_type, not_null, primary_key, default_value, is_foreign_key, references_collection, references_column
- `GroupMetadata`: group_name, dimension_column (empty for vector/set, column name for time series), value_columns array
- C API equivalents: `quiver_scalar_metadata_t`, `quiver_group_metadata_t` in `include/quiver/c/database.h`
- Conversion helpers in `src/c/database_helpers.h`: `convert_scalar_to_c()`, `convert_group_to_c()`, `free_scalar_fields()`, `free_group_fields()`

**LuaRunner (Pimpl Pattern):**
- Purpose: Execute Lua scripts with database access via sol2
- Files: `include/quiver/lua_runner.h`, `src/lua_runner.cpp`
- `LuaRunner::Impl` holds `Database&` reference and `sol::state`
- `bind_database()` registers `Database` as a Lua usertype with all public methods bound
- Injects database instance as `db` global userdata in Lua environment
- C API: `include/quiver/c/lua_runner.h`, `src/c/lua_runner.cpp`

**Blob Subsystem (Pimpl Pattern):**
- Purpose: Binary file I/O for multidimensional numeric data with TOML metadata
- Files: `include/quiver/blob/` (blob.h, blob_csv.h, blob_metadata.h, dimension.h, time_properties.h, time_constants.h)
- `Blob`: Base class for binary file access; calculates position via `calculate_file_position()` from dimension map
- `BlobCSV`: Derived from `Blob`; converts between CSV text and binary format
- `BlobMetadata`: Describes dimensions, time properties, labels, units; serialized to/from TOML
- `Dimension`: Named size + optional `TimeProperties` (frequency, initial value, parent dimension)
- Not currently exposed via C API; operates independently of the `Database` class

**Migrations (Value Type):**
- Purpose: Apply versioned SQL migrations to a database file
- Files: `include/quiver/migration.h`, `include/quiver/migrations.h`, `src/migration.cpp`, `src/migrations.cpp`
- `Migration`: version number + path to directory containing `up.sql` / `down.sql`
- `Migrations`: Loads from a directory of numbered subdirectories; provides `pending(current_version)`
- Applied in `Database::migrate_up()` inside per-migration transactions

## Entry Points

**C++ API (Library Users):**
- Umbrella header: `include/quiver/quiver.h` (includes database.h and element.h)
- Key constructors: `Database(path, options)` or static factories `Database::from_schema()`, `Database::from_migrations()`
- Options: `DatabaseOptions` (alias for `quiver_database_options_t`: `read_only`, `console_level`), `CSVOptions` (enum_labels map, date_time_format)
- Triggers: `Impl::load_schema_metadata()` on successful schema/migration apply

**C API (FFI Callers):**
- Headers: `include/quiver/c/database.h`, `include/quiver/c/element.h`, `include/quiver/c/options.h`, `include/quiver/c/common.h`, `include/quiver/c/lua_runner.h`
- Entry functions: `quiver_database_open()`, `quiver_database_from_schema()`, `quiver_database_from_migrations()`
- Pattern: All factories return `QUIVER_OK`/`QUIVER_ERROR`, database handle via `quiver_database_t** out_db`
- Error retrieval: `quiver_get_last_error()` (thread-local storage in `src/c/common.cpp`)

**Julia Binding:**
- Entry: `bindings/julia/src/Quiver.jl` (module root)
- C FFI stubs: `bindings/julia/src/c_api.jl` (generated via `bindings/julia/generator/generator.bat`)
- Database wrapper: `bindings/julia/src/database.jl`

**Dart Binding:**
- Entry: `bindings/dart/lib/quiverdb.dart`
- C FFI stubs: `bindings/dart/lib/src/ffi/bindings.dart` (generated)
- Database class: `bindings/dart/lib/src/database.dart` (uses `part` files for each operation category)

**Python Binding:**
- Entry: `bindings/python/src/quiverdb/__init__.py`
- CFFI declarations: `bindings/python/src/quiverdb/_c_api.py` (hand-written CFFI cdef)
- Library loader: `bindings/python/src/quiverdb/_loader.py` (walks up to find `build/bin/libquiver_c`)
- Database class: `bindings/python/src/quiverdb/database.py`

**Lua (Embedded):**
- Constructor: `LuaRunner(Database&)` injects `db` as global userdata
- Method: `run(script_string)` executes Lua with full database API available

## Error Handling

**Strategy:** Exceptions in C++ layer, binary error codes in C layer, propagated to language bindings.

**C++ Patterns (three forms only):**

```cpp
// 1. Precondition failure
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");

// 2. Not found
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");

// 3. Operation failure
throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
```

**C API Pattern:**
```cpp
quiver_error_t quiver_database_some_op(quiver_database_t* db, ...) {
    QUIVER_REQUIRE(db, other_required_args);  // null check macro; sets error and returns QUIVER_ERROR

    try {
        db->db.some_op(...);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

**QUIVER_REQUIRE Macro:**
- Defined in `src/c/internal.h`
- Validates 1-9 pointer arguments
- Sets `"Null argument: {name}"` error message and returns `QUIVER_ERROR` immediately
- Usage: `QUIVER_REQUIRE(db, collection, attribute, out_value)` expands to per-arg null checks

**Language Binding Error Flow:**
- Julia: `check()` in `src/exceptions.jl` calls `quiver_get_last_error()`, throws `DatabaseException`
- Dart: `check()` in `lib/src/exceptions.dart` calls `quiver_get_last_error()`, throws Dart exception
- Python: `check()` in `src/quiverdb/_helpers.py` calls `quiver_get_last_error()`, raises `QuiverError`

## Cross-Cutting Concerns

**Logging:**
- Framework: spdlog (C++ core only; not exposed to bindings)
- Per-database instance logger created in `Database` constructor; unique name via atomic counter `g_logger_counter`
- Console sink (color, thread-safe) at user-configured `console_level`; file sink (`quiver_database.log` adjacent to DB file, debug level) for file-based databases; console-only for `:memory:` databases
- Usage: `impl_->logger->debug("Verb noun: {}", detail)`

**Validation:**
- Schema structure: `SchemaValidator::validate()` runs at schema load time; checks required tables and naming conventions
- Type checking: `TypeValidator::validate_scalar()` and `validate_array()` run before each write
- Null pointer checking: `QUIVER_REQUIRE` macro in C API layer before any C++ call

**Authentication:**
- Not implemented; file-level access controlled by OS file permissions
- `DatabaseOptions::read_only` maps to `SQLITE_OPEN_READONLY` vs `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE`

**Transaction Control:**
- Public API: `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()` on `Database`
- Internal RAII: `Impl::TransactionGuard` in `src/database_impl.h` -- begins only if `sqlite3_get_autocommit()` returns true; rollbacks on destruction if not committed
- Write methods use `TransactionGuard` to auto-commit standalone calls or join explicit transactions transparently

**Memory Management (C API):**
- All allocations with `new`; callers free with matching free functions
- Scalars: `quiver_database_free_integer_array()`, `quiver_database_free_float_array()`, `quiver_database_free_string_array()`
- Vectors of vectors: `quiver_database_free_integer_vectors()`, `quiver_database_free_float_vectors()`, `quiver_database_free_string_vectors()`
- Metadata: `quiver_database_free_scalar_metadata()`, `quiver_database_free_group_metadata()`, `quiver_database_free_scalar_metadata_array()`, `quiver_database_free_group_metadata_array()`
- Time series: `quiver_database_free_time_series_data()` (uses column type array to choose per-element vs bulk delete for string vs numeric columns)
- Co-location rule: alloc/free pairs in same translation unit (read pairs in `src/c/database_read.cpp`, metadata pairs in `src/c/database_metadata.cpp`, time series pairs in `src/c/database_time_series.cpp`)
- String helper: `strdup_safe()` in `src/c/database_helpers.h`

**DateTime Handling:**
- Columns whose names start with `date_` are treated as `DataType::DateTime` (see `is_date_time_column()` in `include/quiver/data_type.h`)
- Time series dimension column detected by `internal::find_dimension_column()` in `src/database_internal.h`
- Julia/Dart bindings provide convenience wrappers that parse ISO strings into native DateTime types

---

*Architecture analysis: 2026-02-25*
