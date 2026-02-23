# Architecture

**Analysis Date:** 2026-02-23

## Pattern Overview

**Overall:** Layered architecture with domain logic in C++ core, C API for FFI interop, and thin language bindings.

**Key Characteristics:**
- Three distinct layers: C++ core → C API → language bindings (Julia, Dart, Python, Lua)
- Pimpl pattern for classes hiding private dependencies (Database, LuaRunner)
- Value types for simple data structures without private state
- Logic lives in C++ layer; bindings are mechanical and thin
- Strict separation between public interfaces (headers) and implementation (cpp files)
- Schema-driven validation and type checking
- Explicit transaction control with nest-aware RAII guards

## Layers

**C++ Core Layer (`include/quiver/`, `src/`):**
- Purpose: Domain logic, database operations, schema validation, type checking
- Location: `include/quiver/` (public headers), `src/` (implementations)
- Contains: Database class (Pimpl), Element builder, Metadata types, Options, Schema/SchemaValidator, TypeValidator
- Depends on: SQLite3, spdlog for logging, Lua (for LuaRunner), RapidCSV (for CSV)
- Used by: C API layer wraps all public methods

**C API Layer (`include/quiver/c/`, `src/c/`):**
- Purpose: FFI-safe C interface for language bindings
- Location: `include/quiver/c/` (header contracts), `src/c/` (thin wrappers)
- Contains: Function definitions wrapping each C++ public method; error handling via `quiver_set_last_error()`
- Depends on: C++ core layer
- Used by: Julia, Dart, Python, and Lua bindings call C API
- Key pattern: Try-catch with binary return codes (QUIVER_OK/QUIVER_ERROR), values via output parameters

**Language Bindings (`bindings/julia/`, `bindings/dart/`, `bindings/python/`):**
- Purpose: Idiomatic language-specific wrappers
- Location: Each in respective binding directory
- Contains: FFI stubs, type conversions, convenience methods (e.g., transaction blocks, composite readers)
- Depends on: C API layer only (via FFI/bindings)
- Used by: End-user applications in Julia, Dart, Python

## Data Flow

**Create Element Flow:**

1. User calls `database.create_element(collection, element)` in C++
2. Element builder holds scalars (map<string, Value>) and arrays (map<string, vector<Value>>)
3. Database::create_element:
   - Validates all scalars against schema/type validator
   - Inserts scalars into main collection table in transaction
   - Routes arrays to vector/set/time-series tables based on schema
   - Zips multi-column arrays together with proper indices
   - Commits transaction guard on success
4. C API wrapper catches exceptions, sets `quiver_last_error`, returns QUIVER_ERROR/QUIVER_OK
5. Language binding calls C API, unmarshals error state, converts to idiomatic exception

**Read Scalar Flow:**

1. User calls `database.read_scalar_integers(collection, attribute)`
2. Database verifies collection/attribute exist via schema
3. Executes `SELECT id, attribute FROM collection ORDER BY id`
4. Returns `vector<int64_t>` to caller
5. C API wrapper allocates new int64_t array, copies data, returns array + count
6. Language binding receives pointers, wraps in language array type, provides free function

**Query Flow:**

1. User calls `database.query_string(sql, params)` with optional Value vector
2. Parameterized queries use positional `?` placeholders
3. Params converted to SQLite types based on Value variant
4. Statement executed, first row first column extracted
5. Returns `optional<T>` (absent if query returns no rows)

**State Management:**

- Database lifecycle managed via RAII (Pimpl holds sqlite3* and logger)
- Transactions explicit: `begin_transaction()`, `commit()`, `rollback()` methods
- `TransactionGuard` (in database_impl.h) nest-aware: only owns transaction if one not already active (checked via `sqlite3_get_autocommit()`)
- Write operations (`create_element`, `update_*`) use TransactionGuard internally to auto-commit standalone operations or participate in explicit transactions
- Schema loaded once at database open, cached in Impl
- Error state thread-local in C API layer via `quiver_set_last_error()`

## Key Abstractions

**Database (Pimpl Pattern):**
- Purpose: Main API gateway for all database operations
- File: `include/quiver/database.h`, `src/database.cpp`, `src/database_impl.h`
- Pattern: Pimpl hides sqlite3 and spdlog dependencies
- Operations organized by category: create/read/update/delete, metadata queries, transactions, CSV export/import, parameterized queries

**Element (Value Type):**
- Purpose: Builder for element creation
- File: `include/quiver/element.h`, `src/element.cpp`
- Pattern: Plain struct with fluent API, no private dependencies
- Holds two maps: scalars and arrays, routed to appropriate tables on create

**Schema & SchemaValidator (Value Type Immutable):**
- Purpose: Parse and validate database schema structure
- File: `include/quiver/schema.h`, `src/schema.cpp`, `src/schema_validator.cpp`
- Pattern: Loaded once at database open, cached immutably
- Determines table classification (collection vs vector vs set vs time-series)
- Validates required tables (Configuration) and naming conventions

**TypeValidator (Value Type):**
- Purpose: Runtime type checking for scalar and array values
- File: `include/quiver/type_validator.h`, `src/type_validator.cpp`
- Validates scalar values match expected DataType before insert
- Validates array element types before insert

**Metadata Types (Value Types):**
- Purpose: Describe schema of attributes and groups
- Files: `include/quiver/attribute_metadata.h`
- ScalarMetadata: name, data_type, not_null, primary_key, default_value, foreign key info
- GroupMetadata: group_name, dimension_column (empty for vector/set, populated for time-series), value_columns array

**LuaRunner (Pimpl Pattern):**
- Purpose: Execute Lua scripts with database access
- File: `include/quiver/lua_runner.h`, `src/lua_runner.cpp`
- Pattern: Pimpl hides lua_State dependency
- Injects database as `db` userdata in Lua environment

**Result (Value Type):**
- Purpose: Hold query results
- File: `include/quiver/result.h`, `src/result.cpp`
- Holds columns (vector<string>) and rows (vector<Row>)
- Provides row indexing and iteration support

**Value (Type Alias):**
- Purpose: Type-safe variant for scalar values in queries/elements
- File: `include/quiver/value.h`
- Definition: `using Value = std::variant<std::nullptr_t, int64_t, double, std::string>`
- Used throughout: Element scalars, parameterized query params, time series rows

## Entry Points

**C++ API Entry (Library):**
- Location: `include/quiver/quiver.h` (includes all public headers)
- Key constructor: `Database(path, options)` or static factories `from_schema()`, `from_migrations()`
- Options: DatabaseOptions (version, log_level), CSVExportOptions (enum_labels, date formatting)

**C API Entry (FFI):**
- Location: `include/quiver/c/database.h`, `include/quiver/c/element.h`, `include/quiver/c/options.h`
- Functions: `quiver_database_open()`, `quiver_database_from_schema()`, etc.
- Pattern: Factory functions return `QUIVER_OK`/`QUIVER_ERROR`, values via output parameters
- Error retrieval: `quiver_get_last_error()` (thread-local)

**Lua Entry:**
- Location: `include/quiver/lua_runner.h`
- Constructor: `LuaRunner(database&)` injects db into Lua environment
- Method: `run(script_string)` executes Lua with `db` userdata available

**Language Bindings Entry:**
- Julia: `bindings/julia/src/Quiver.jl` (module root)
- Dart: `bindings/dart/lib/src/database.dart` (Database class)
- Python: `bindings/python/src/quiver/*.py`

## Error Handling

**Strategy:** Exceptions in C++ layer, binary error codes in C layer, propagated to language bindings

**Patterns:**

1. **C++ Precondition Failure:**
   ```cpp
   throw std::runtime_error("Cannot {operation}: {reason}");
   // Example: "Cannot create_element: element must have at least one scalar attribute"
   ```

2. **C++ Not Found:**
   ```cpp
   throw std::runtime_error("{Entity} not found: {identifier}");
   // Example: "Scalar attribute not found: 'value' in collection 'Items'"
   ```

3. **C++ Operation Failure:**
   ```cpp
   throw std::runtime_error("Failed to {operation}: {reason}");
   // Example: "Failed to execute statement: {sqlite3_errmsg(db)}"
   ```

4. **C API Wrapper:**
   ```cpp
   try {
       // C++ call
       return QUIVER_OK;
   } catch (const std::exception& e) {
       quiver_set_last_error(e.what());
       return QUIVER_ERROR;
   }
   ```

5. **Language Binding:** Retrieves error via `quiver_get_last_error()`, throws idiomatic exception (Julia exception, Dart exception, Python exception)

**Precondition Checks (C API):** QUIVER_REQUIRE macro validates null pointers, sets error message, returns QUIVER_ERROR

## Cross-Cutting Concerns

**Logging:**
- Framework: spdlog
- Per-database instance logger with unique name
- Console sink (color) at configured level; file sink (if file-based DB) at debug level
- Typical usage: `impl_->logger->debug("Opening database: {}", path)`

**Validation:**
- Schema validation: `SchemaValidator` checks required tables (Configuration), naming conventions
- Type validation: `TypeValidator` validates scalar and array values match schema before insert
- Null pointer validation: `QUIVER_REQUIRE` macro in C API layer

**Authentication:**
- Not implemented; SQLite operates at file level
- File permissions managed by OS

**Transaction Control:**
- Explicit API: `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()`
- Implicit RAII: `TransactionGuard` in write operations, nest-aware (only owns if one not already active)
- Autocommit disabled when explicit transaction active

**Memory Management (C API):**
- Allocate with `new`, deallocate with provided free functions
- String arrays: `quiver_database_free_string_array(char**, size_t)`
- Typed arrays: `quiver_database_free_integer_array(int64_t*)`, etc.
- Metadata: `quiver_database_free_scalar_metadata()`, `quiver_database_free_group_metadata()`
- Alloc/free pairs co-located in same translation unit (read ops in database_read.cpp, metadata ops in database_metadata.cpp, etc.)

---

*Architecture analysis: 2026-02-23*
