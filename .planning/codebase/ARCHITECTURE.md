# Architecture

**Analysis Date:** 2026-02-09

## Pattern Overview

**Overall:** Layered C++ Library with FFI Bridge

Quiver follows a three-layer architecture:
1. **C++ Core Layer** - Main business logic and SQLite abstraction
2. **C API Layer** - Language-agnostic FFI bridge with error handling
3. **Language Bindings** - Thin wrappers (Julia, Dart, Python)

Key Characteristics:
- Logic resides entirely in C++ core; bindings remain thin
- Pimpl pattern for classes hiding private dependencies; value types use Rule of Zero
- Binary error codes (QUIVER_OK/QUIVER_ERROR) in C API with thread-local error messages
- RAII with explicit ownership and move semantics
- Template-based internals for type-safe generic operations

## Layers

**C++ Core (`include/quiver/`, `src/`):**
- Purpose: Database abstraction, CRUD operations, schema management, SQL execution
- Location: `include/quiver/*.h`, `src/*.cpp`
- Contains: Database class, Element builder, Row/Result types, Schema/Metadata structures
- Depends on: sqlite3, spdlog, optional sol2 (for Lua)
- Used by: C API layer, tests

**C API Layer (`include/quiver/c/`, `src/c/`):**
- Purpose: Language-agnostic FFI interface for binding generators
- Location: `include/quiver/c/*.h`, `src/c/*.cpp`
- Contains: C function declarations, struct definitions (quiver_database_t, quiver_element_t), error handling
- Depends on: C++ core (wraps Database/Element in opaque structs)
- Used by: Language bindings (Julia, Dart, Python)

**Language Bindings (`bindings/julia/`, `bindings/dart/`, `bindings/python/`):**
- Purpose: Language-specific APIs that call C API functions
- Location: `bindings/{language}/src/`, `bindings/{language}/lib/`
- Contains: Generated or hand-written wrappers that invoke C functions
- Depends on: C API (via FFI)
- Used by: End users in respective languages

**Test Layer (`tests/`):**
- Purpose: Comprehensive test coverage of C++ and C APIs
- Location: `tests/test_*.cpp`, `tests/test_schemas/`
- Contains: GTest-based unit tests organized by functionality
- Depends on: C++ core, C API, schema files
- Patterns: Mirrors CRUD/metadata operations across both APIs

## Data Flow

**Element Creation (Create):**

1. User creates `Element` via builder: `Element().set("label", "Item").set("value", 42)`
2. User calls `Database::create_element(collection, element)`
3. C++ layer:
   - Validates collection exists in Schema
   - Routes scalar/vector/set attributes to appropriate tables (via Schema::find_table_for_column)
   - Executes INSERT statements with value binding
   - Returns new element ID
4. C API wraps: User calls `quiver_database_create_element(db, collection, element, &id)` with error checking

**Scalar Read (Read):**

1. User calls `Database::read_scalar_integers("Collection", "attribute")`
2. C++ layer:
   - Queries schema to validate attribute exists
   - Executes SQL: `SELECT id, value FROM Collection WHERE attribute IS NOT NULL ORDER BY id`
   - Uses Result/Row classes to parse data
   - Returns `std::vector<int64_t>`
3. C API wraps: User calls `quiver_database_read_scalar_integers(db, collection, attribute, &values, &count)` with memory management

**Vector/Set Read:**

1. User calls `Database::read_vector_integers("Collection", "attribute")`
2. C++ layer:
   - Resolves to underlying vector table via Schema::find_vector_table (e.g., `Collection_vector_attribute`)
   - Executes SQL with GROUP BY id to reconstruct arrays
   - Uses template helper `read_grouped_values_all<T>` to parse rows into grouped vectors
   - Returns `std::vector<std::vector<int64_t>>`
3. Pattern same for sets (set tables named `Collection_set_attribute`)

**Time Series Read:**

1. User calls `Database::read_time_series_group_by_id(collection, group, id)`
2. C++ layer:
   - Resolves to time series table (e.g., `Collection_time_series_data`) via Schema::find_time_series_table
   - Dimension column identified by prefix `date_` (e.g., `date_time`)
   - Returns `std::vector<std::map<std::string, Value>>` (each row is a map of column:value)
3. GroupMetadata returns dimension_column populated for time series (empty for vectors/sets)

**State Management:**

- **Database State**: Managed via Pimpl (`Database::Impl`)
  - sqlite3 pointer (SQLite connection)
  - Logger instance (spdlog)
  - Schema cache (loaded once on open/creation)
  - TypeValidator (validates runtime inserts/updates)
- **Schema State**: Loaded from introspection of database on connection
  - TableDefinition: columns, indexes, foreign keys
  - Cached throughout Database lifetime
  - Used for validation and query routing
- **Transaction State**: RAII TransactionGuard in Impl
  - Automatic rollback if guard destroyed without explicit commit()
  - Used in update/create operations

## Key Abstractions

**Database Class:**
- Purpose: Main API gateway, lifecycle management
- Examples: `include/quiver/database.h`
- Pattern: Pimpl with move semantics, non-copyable. Factory methods (from_schema, from_migrations).
- Responsibilities: Opens/closes SQLite connection, routes operations, manages schema state

**Element (Builder):**
- Purpose: Fluent API for constructing element data before creation
- Examples: `include/quiver/element.h`
- Pattern: Value type (Rule of Zero), maps for scalars and arrays
- Responsibilities: Accumulates scalar/vector/set attributes with setter chaining

**Schema & TableDefinition:**
- Purpose: Database schema introspection and validation
- Examples: `include/quiver/schema.h`
- Pattern: Loaded from database via `Schema::from_database(sqlite3*)`, cached in Impl
- Responsibilities:
  - Provides table/column metadata (types, FK constraints)
  - Identifies collection vs. vector/set/time-series tables by naming convention
  - Routes attributes to correct underlying tables (e.g., `Collection_vector_name`)

**Result & Row:**
- Purpose: Type-safe query result parsing
- Examples: `include/quiver/result.h`, `include/quiver/row.h`
- Pattern: Value types holding column names and parsed rows
- Responsibilities: Store and access query results with optional-based type-safe getters

**Value:**
- Purpose: Variant-like type representing int64, double, string, or null
- Examples: `include/quiver/value.h`
- Pattern: Value type, discriminated union
- Responsibilities: Hold and safely retrieve typed values from query results

**Metadata Classes (ScalarMetadata, GroupMetadata):**
- Purpose: Runtime inspection of attribute structure
- Examples: `include/quiver/attribute_metadata.h`
- Pattern: Value structs (no Pimpl)
- Responsibilities: Expose attribute types, nullability, FK relations, dimension column for time series

**LuaRunner:**
- Purpose: Execute Lua scripts with embedded database access
- Examples: `include/quiver/lua_runner.h`
- Pattern: Pimpl (hides sol2/Lua headers), move semantics
- Responsibilities: Compile and run Lua code with db object bound to script context

## Entry Points

**C++ Constructor:**
- Location: `include/quiver/database.h` line 25
- Triggers: User calls `Database(path, options)`
- Responsibilities: Opens SQLite connection, enables foreign keys, creates logger, loads schema if exists

**C++ Factory from_schema:**
- Location: `include/quiver/database.h` line 40-42
- Triggers: User calls `Database::from_schema(db_path, schema_path, options)`
- Responsibilities: Creates empty database, applies SQL schema, caches schema metadata

**C++ Factory from_migrations:**
- Location: `include/quiver/database.h` line 36-38
- Triggers: User calls `Database::from_migrations(db_path, migrations_path, options)`
- Responsibilities: Creates database, applies numbered migrations, tracks version in Configuration table

**C API Database Open:**
- Location: `include/quiver/c/database.h` line 34-36
- Triggers: `quiver_database_open(path, options, &db)`
- Responsibilities: Wraps C++ constructor, returns binary error code, sets last_error on failure

**C API Create Element:**
- Location: `include/quiver/c/database.h` line 54-57
- Triggers: `quiver_database_create_element(db, collection, element, &id)`
- Responsibilities: Calls C++ `create_element()`, converts returned ID to out-parameter

## Error Handling

**Strategy:** Exceptions in C++ layer, binary return codes in C layer with thread-local error messages

**Patterns:**

1. **C++ Exceptions:**
   - All C++ methods throw `std::runtime_error` with descriptive messages
   - No error codes, all state conveyed in exception text
   - Examples: `throw std::runtime_error("Collection not found: " + name);`

2. **C API Error Wrapping:**
   - Every C API function returns `quiver_error_t` (QUIVER_OK or QUIVER_ERROR)
   - Try-catch blocks wrap C++ calls, catch `std::exception`, call `quiver_set_last_error(e.what())`
   - Thread-local storage holds error message for retrieval via `quiver_get_last_error()`
   - Example from `src/c/database.cpp`:
     ```cpp
     quiver_error_t quiver_database_create_element(...) {
         QUIVER_REQUIRE(db, collection, element, out_id);
         try {
             *out_id = db->db.create_element(collection, element->element);
             return QUIVER_OK;
         } catch (const std::exception& e) {
             quiver_set_last_error(e.what());
             return QUIVER_ERROR;
         }
     }
     ```

3. **Validation Macros:**
   - `QUIVER_REQUIRE(a, b, c...)` validates non-null pointers
   - Uses stringification to auto-generate "Null argument: db" messages
   - Defined in `src/c/internal.h` lines 28-80

## Cross-Cutting Concerns

**Logging:**
- Framework: spdlog with dual sinks (console + file)
- Approach: Logger created per Database instance with path-based name
- Levels configured via `DatabaseOptions.console_level`
- Examples: `impl_->logger->debug("Opening database: {}", path);`
- Location: Logger setup in `src/database.cpp` lines 100-140

**Validation:**
- Schema validation: `SchemaValidator` checks for Configuration table, foreign keys, naming conventions
- Type validation: `TypeValidator` checks insert/update values match column types
- Location: `src/schema_validator.cpp`, `src/type_validator.cpp`
- Applied: On database open (schema validation) and element operations (type validation)

**Authentication:**
- Not implemented. No built-in user/role system.
- SQLite enforces file-level access control only.

**Transactions:**
- RAII-based via `TransactionGuard` in `Database::Impl` (lines 237-257 in `src/database.cpp`)
- Begin, commit, rollback methods on Impl
- Automatic rollback on guard destruction without explicit commit
- Used internally for multi-step operations (create with FK, update vectors)

