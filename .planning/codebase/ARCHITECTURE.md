<!-- refreshed: 2026-09-14 -->
# Architecture

**Analysis Date:** 2026-09-14

## System Overview

Quiver is a multi-layer SQLite wrapper library with a C++ core, a C API for FFI, and five language bindings (Julia, Dart, Python, JS/Bun, Lua). The architecture strictly separates concerns across layers: business logic lives in C++, the C API marshals between C++ and FFI, and bindings provide idiomatic language surfaces.

```text
┌────────────────────────────────────────────────────────────────────┐
│                    Language Bindings Layer                          │
├──────────────┬──────────────┬──────────────┬──────────────┬────────┤
│   Julia      │    Dart      │   Python     │   JS/Bun     │  Lua*  │
│  (FFI)       │   (FFI)      │   (CFFI)     │   (FFI)      │(sol2)  │
│  c_api.jl    │  ffi/        │  _c_api.py   │  loader.ts   │direct  │
│  db*.jl      │  database.dart database.py  │  database.ts │binding │
└──────┬───────┴────┬─────────┴──────┬───────┴──────┬───────┴────┬───┘
       │            │                │              │            │
       │            └────────────────┴──────────────┘            │
       │                      │                                  │
       │                      ▼                                  │
       │         ┌─────────────────────────────────┐             │
       │         │     C API Layer (FFI)           │             │
       │         │  src/c/ + include/quiver/c/     │             │
       │         │  quiver_database_*              │             │
       │         │  quiver_element_*               │             │
       │         │  quiver_binary_file_*           │             │
       │         │  quiver_*_free_*                │             │
       │         └──────────────┬──────────────────┘             │
       │                        │                               │
       │                        ▼                               │
       │         ┌──────────────────────────────────────────┐   │
       │         │   C++ Core Logic Layer                   │   │
       │         │   src/ + include/quiver/                 │   │
       │         │   Database class, CRUD operations        │   │
       │         │   Schema validation, Type checking       │   │
       │         │   Transaction management                 │   │
       │         │   Migration system                       │   │
       │         └──────────────┬───────────────────────────┘   │
       │                        │                               │
       │                        ▼                               │
       │         ┌──────────────────────────────────────────┐   │
       │         │   SQLite Storage + Subsystems           │   │
       │         ├──────────────────────────────────────────┤   │
       │         │ • SQLite3 database engine               │   │
       │         │ • Binary subsystem (.qvr files)         │   │
       │         │ • Expression subsystem (lazy DAG)       │   │
       │         │ • Lua execution engine (sol2)            │   │
       │         └──────────────────────────────────────────┘   │
       │                                                        │
       └────────────────────────────────────────────────────────┘
*Lua is direct C++ binding via sol2, bypassing C API
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Database | Main API facade; element CRUD, schema introspection, transactions, dry runs | `include/quiver/database.h`, `src/database.cpp` |
| Database::Impl | Private implementation; schema/type validation, label resolution, FK handling, group operations | `src/database_impl.h` |
| TypeValidator | Scalar and array value type checking against schema | `src/type_validator.cpp`, `include/quiver/type_validator.h` |
| Schema | Schema introspection from database, group table classification, metadata lookups | `src/schema.cpp`, `include/quiver/schema.h` |
| SchemaValidator | Schema convention validation (required tables, naming rules, group structure) | `src/schema_validator.cpp` |
| Element | Builder for element creation/updates (fluent API) | `src/element.cpp`, `include/quiver/element.h` |
| Value | Union type for int64/double/string/nullptr, used everywhere | `include/quiver/value.h` |
| Row/Result | Query result wrappers | `src/row.cpp`, `src/result.cpp` |
| LuaRunner | Lua script execution with db access via sol2; no lifecycle methods (db provided externally) | `src/lua_runner.cpp`, `include/quiver/lua_runner.h` |
| BinaryFile | Binary file I/O for `.qvr` files with metadata sidecars (Pimpl) | `src/binary/binary_file.cpp`, `include/quiver/binary/binary_file.h` |
| CSVConverter | Bidirectional CSV↔binary conversion | `src/binary/csv_converter.cpp`, `include/quiver/binary/csv_converter.h` |
| BinaryMetadata | `.qvr` metadata (dimensions, labels, unit, initial datetime) | `src/binary/binary_metadata.cpp`, `include/quiver/binary/binary_metadata.h` |
| Expression | Lazy DAG for computation over `.qvr` files; value type wrapping `shared_ptr<ExpressionNode>` | `src/expression/expression.cpp`, `include/quiver/expression/expression.h` |
| ExpressionNode | Abstract base + concrete subclasses for file reads, scalars, binary/unary/ternary ops, aggregations, label-axis projection | `include/quiver/expression/expression_node.h` |
| C API (quiver_database_*) | FFI wrapper over Database; error codes, out-parameters, memory management | `src/c/database.cpp`, `include/quiver/c/database.h` |
| LuaRunner C API | FFI wrapper for Lua integration | `src/c/lua_runner.cpp`, `include/quiver/c/lua_runner.h` |

## Pattern Overview

**Overall:** Multi-layer factory/strategy with error handling isolation.

**Key Characteristics:**
- **Layered isolation:** Each layer owns its error messages and validation; downstream layers never re-implement. Single `quiver_get_last_error()` channel for all C++ exceptions.
- **Pimpl discipline:** `Database`, `LuaRunner`, `BinaryFile` hide native library dependencies (sqlite3, Lua, file I/O). Value types like `Element`, `Value`, `BinaryMetadata`, `Expression` use Rule of Zero.
- **Cross-layer naming:** Mechanical transformation (C++ → C API prefix, → Dart camelCase, etc.) applied uniformly so one name means the same operation everywhere.
- **Lazy schema loading:** Constructor opens database without reading schema; every metadata/CRUD call triggers `Impl::require_schema()`, making `open()` safe on half-migrated databases.
- **Transaction nestability:** `TransactionGuard` is nest-aware; explicit transactions compose with dry runs and implicit guard usage.
- **One validator per concern:** `TypeValidator` for values, `SchemaValidator` for structure, `BinaryMetadata::validate()` for binary metadata, `Expression` node constructors for DAG coherence.

## Layers

**Layer 1: C++ Core**
- Purpose: All business logic, validation, SQL generation, type checking
- Location: `include/quiver/`, `src/`
- Contains: Database class (public API), validation logic, element builders, schema introspection, CSV export/import, migrations, dry runs, transaction management
- Depends on: SQLite3, spdlog (logging), standard library
- Used by: C API layer, Lua (via sol2)

**Layer 2: C API (FFI Bridge)**
- Purpose: Convert C++ exceptions/return types to C error codes and out-parameters; provide opaque handles; manage memory
- Location: `include/quiver/c/`, `src/c/`
- Contains: Thin wrappers over every C++ public method, marshaling helpers, memory management functions
- Depends on: C++ core, standard C
- Used by: All five language bindings via FFI

**Layer 3: Language Bindings**
- Purpose: Provide idiomatic language surfaces; call C API; marshal parameters, return values, exceptions
- Location: `bindings/{julia,dart,python,js}/src`
- Contains: Generated code (Julia via Clang.jl, Dart via ffigen), hand-written wrappers (Python CFFI, JS FFI), convenience methods (composite readers, transaction blocks)
- Depends on: C API layer
- Used by: End users of Quiver in their respective languages

**Layer 3b: Lua (Direct C++ Binding)**
- Purpose: Execute Lua scripts with database access; sol2 binds C++ classes directly
- Location: `src/lua_runner.cpp` (implementation), `include/quiver/lua_runner.h` (C++ header), also `include/quiver/c/lua_runner.h` (C API wrapper)
- Contains: sol2 userdata definitions for Database, BinaryFile, Expression, LuaRunner; file I/O sandbox
- Depends on: C++ core, Lua 5.4, sol2
- Used by: CLI (`src/cli/main.cpp`), C API wrapper (for FFI bindings), direct C++ callers

## Data Flow

### Primary Request Path

1. **Binding layer** — User calls `db.create_element("Collection", {...})` 
   - Python example: `db.create_element("Items", label="Item1", value=42)`
2. **Parameter marshaling** — Binding validates, converts types, builds parameter list
   - Python `element.py` converts `**kwargs` to internal representation
3. **C API call** — Binding invokes C function (e.g., `quiver_database_create_element()`)
4. **C API marshaling** — Unwraps opaque handles, out-parameters; calls C++ method
   - `src/c/database_create.cpp` calls `db->create_element(collection, element)`
5. **C++ validation** — `TypeValidator` checks types, `Impl::require_collection` checks collection exists
6. **SQL execution** — Core builds INSERT statement via `Element` interface, executes via `Database::execute()`
7. **Result return** — Element ID returned via out-parameter (`int64_t* out_id`)
8. **Error handling** — Any exception caught, message stored in thread-local error buffer (`quiver_get_last_error()`)
9. **Binding unpacking** — C API returns error code; binding checks, retrieves error message if needed, raises language-native exception

### Metadata Reading (Lazy Load)

1. User calls `db.get_scalar_metadata(collection, attribute)` 
2. `Database::get_scalar_metadata()` calls `Impl::require_schema()` (const method)
3. `require_schema()` checks if `impl_->schema` is null; if so, calls `load_schema_metadata()`
4. `load_schema_metadata()` reads SQLite `PRAGMA table_info`, constructs `Schema` object, validates with `SchemaValidator`
5. Both `schema` and `type_validator` members assigned only after validation passes (half-loaded state prevented)
6. Schema cached for all subsequent operations

### Transaction Management

1. User or implicit caller invokes `begin_transaction()`
2. `Database::begin_transaction()` checks dry-run flag; if set, no-op; else creates real SQLite transaction
3. `TransactionGuard` (RAII) checks `sqlite3_get_autocommit()`: if transaction already active, becomes no-op; else opens nested transaction
4. Operations execute within the transaction context
5. `commit()` or `rollback()` called; if dry-run is active, only `end_dry_run()` can commit (and it doesn't—it always rolls back)

### Lua Script Execution

1. User calls `LuaRunner::run(script)` or C API `quiver_lua_runner_run()`
2. `LuaRunner` opens Lua environment, registers `db` userdata (raw `Database&`, no ownership)
3. Lua script runs; calls `db:create_element()`, etc. bind directly to C++ methods
4. Script's return value (or `nil` for no return) collected into a Value
5. Value encoded as JSON: integers/doubles via `std::to_chars`, tables as objects (sorted keys), `nil`/NaN as `null`
6. JSON string returned via out-parameter or C++ return

### Binary File I/O

1. User or Lua calls `BinaryFile::open_file(path, mode, metadata?)`
2. `BinaryFile::Impl` registers canonical path in write registry if opening for write
3. File I/O operations (`read(dims)`, `write(data, dims)`) use lazy-loaded metadata
4. `read()` returns `vector<double>` aligned to dimension space
5. `write()` accepts 1D column-vector, transposed to N-dimensional space via dimension map
6. `close()` or destructor unregisters path from write registry

### Expression Evaluation

1. User or Lua builds `Expression` DAG: `(file_a + file_b) * 2.0 - sqrt(file_c)`
2. Constructor validates shapes, units, dimension consistency eagerly
3. `save(path)` iterates via `first_dimensions()` / `next_dimensions()` over output space
4. For each coordinate, calls `compute_row()` on DAG root (polymorphic dispatch)
5. Each node computes its operand rows, applies operation, yields result
6. Results accumulated into output `.qvr` file

## Key Abstractions

**Database/Impl separation:**
- Purpose: Hide sqlite3, spdlog, schema, validator details from public header; support move semantics
- Examples: `include/quiver/database.h` is the opaque factory; `src/database_impl.h` is private internals
- Pattern: Pimpl with `std::unique_ptr<Impl>`

**Element builder:**
- Purpose: Fluent interface for constructing elements for create/update
- Examples: `Element().set("label", "x").set("value", 42).set("tags", {"a", "b"})`
- Pattern: Value type with chainable `set()` methods, backed by `std::map` for scalars and arrays

**Schema/TableDefinition hierarchy:**
- Purpose: Introspect database structure once, cache it, avoid repeated PRAGMA calls
- Examples: `Schema::has_table()`, `TableDefinition::has_column()`, `get_foreign_key()`, `get_data_type()`
- Pattern: Value types parsed from PRAGMA result sets

**Value union:**
- Purpose: Uniform representation of int64/double/string/nullptr across all layers
- Examples: `Value` variant used in Element, Row, time-series updates, CSV export
- Pattern: `std::variant` with `std::holds_alternative<T>`, `std::get<T>`

**TypeValidator singleton:**
- Purpose: One place where value-to-column type checking lives; threads the operation name for Pattern 1 messages
- Examples: `TypeValidator::validate_value()` called by create_element, update_element, array writers; one consistent message format
- Pattern: Instance created at schema load time, pointed to by `Impl::type_validator`

**GroupMetadata/ScalarMetadata:**
- Purpose: Query result metadata (column names, types, nullability, FK relationships, group type)
- Examples: Returned by `get_vector_metadata()`, `list_scalar_attributes()`
- Pattern: Value types with public members

**TransactionGuard:**
- Purpose: Nest-aware RAII; if transaction already active, no-op; else open one
- Examples: Used by create_element, update_element, all writes (so they work standalone or inside explicit transactions)
- Pattern: RAII with `sqlite3_get_autocommit()` check in constructor, `commit()` / `rollback()` in destructor

**ExpressionNode tree:**
- Purpose: Lazy DAG for computation over binary files
- Examples: `ExpressionFile` (read), `ExpressionBinary` (+ - * /), `ExpressionAggregate` (sum/mean/min/max over a dimension)
- Pattern: Polymorphic tree with `shared_ptr<ExpressionNode>` ownership

## Entry Points

**CLI (`src/cli/main.cpp`)**
- Location: `src/cli/main.cpp`
- Triggers: User runs `quiver_cli <database> <script> [--schema|--migrations] [--read-only] [--dry-run]`
- Responsibilities: Parse arguments, construct Database (from existing file, schema, or migrations), create LuaRunner, execute Lua script, output JSON result
- Main entry: `main(int argc, char* argv[])`

**Database Factory Methods**
- Location: `include/quiver/database.h` (public), `src/database.cpp` (implementation)
- Triggers: User calls `Database(path, options)`, `Database::from_schema()`, `Database::from_migrations()`
- Responsibilities: Open/create database, apply schema or migrations, lazy-load schema metadata, validate consistency
- Key methods: `Database::Database()`, `Database::from_schema()`, `Database::from_migrations()`

**CRUD Operations**
- Location: `src/database_create.cpp`, `database_update.cpp`, `database_delete.cpp`, `database_read.cpp`
- Triggers: User calls `db.create_element()`, `db.update_element()`, `db.read_scalar_integers()`, etc.
- Responsibilities: Validate inputs, execute SQL, marshal results
- Key entry points: Every public method on Database class

**Lua Script Entry (`LuaRunner::run()`)**
- Location: `src/lua_runner.cpp`
- Triggers: User (or C++ caller) invokes `lua.run(script_code)` or C API `quiver_lua_runner_run()`
- Responsibilities: Execute script, provide `db` userdata with full API access, return result as JSON
- Key method: `LuaRunner::run(const std::string& script)` returns `std::string` (JSON)

**Binary File I/O Entry**
- Location: `src/binary/binary_file.cpp`
- Triggers: User calls `BinaryFile::open_file(path, mode)` or Lua `db:open_file()`
- Responsibilities: Open/create .qvr file, load/save metadata, read/write binary data
- Key methods: `BinaryFile::open_file()`, `read()`, `write()`, `close()`

**Expression DAG Entry**
- Location: `src/expression/expression.cpp`
- Triggers: User builds expression with operators or calls `Expression::save(path)`
- Responsibilities: Validate DAG coherence, compute rows lazily, materialize to .qvr
- Key methods: Constructor, operator overloads, `save()`

## Architectural Constraints

- **Threading:** Single-threaded event-loop style. SQLite in serialized mode (default). No per-query locking; entire Database must not be shared across threads. Lua scripts are synchronous single-threaded (sol2 default).
- **Global state:** Thread-local error buffer via `thread_local` for `quiver_get_last_error()`. One logger per Database instance (no static logger). Expression node DAG is fully immutable after construction (safe to pass between threads if no active computation).
- **Circular imports:** `database_impl.h` includes `schema.h` and `type_validator.h`; those include `database.h` only (no circular dependency via headers). Implementation details in .cpp files avoid circular calls.
- **Resource lifecycle:** Pimpl `unique_ptr` ensures exception-safe moves. Lua's `Database&` in `LuaRunner` must outlive the runner instance. Binary file write registry is process-local and non-reentrant.
- **Lazy loading:** Schema loaded on first metadata/CRUD call, not in constructor. Avoids validating half-migrated databases before `migrate_up` runs.
- **Error isolation:** All C++ exceptions caught at C API boundary; never propagate across FFI. LuaRunner catches script errors and encodes them as part of the JSON result.

## Anti-Patterns

### Storing Raw Pointers to Schema Elements

**What happens:** Code reaches for `schema->get_table(name)` and stores the returned `TableDefinition*` across multiple calls.

**Why it's wrong:** Schema is lazy-loaded and rebuilt on each `require_schema()` call; the pointer becomes invalid. Every call site must re-fetch or hold a const reference for the duration of one operation.

**Do this instead:** Call `schema->get_table(name)` immediately before use within a single operation (`require_schema()` is const; you can call it from const methods too). Store the returned pointer only for the current call stack frame.

### Re-implementing Group Table Name Derivation

**What happens:** Code manually builds `_vector_`, `_set_`, `_time_series_` table names instead of using `Schema::group_names()` or `is_group_table()`.

**Why it's wrong:** Violates the single-source-of-truth principle. Naming rules are complex (FK columns can shadow group columns), and drift between hand-rolled and canonical implementations leads to silent inconsistencies.

**Do this instead:** Always use `Schema::group_names(collection, type)` for iteration or `schema->is_group_table(table_name, type)` for classification. The lookup is fast (strings are short).

### Throwing Exceptions Across the FFI Boundary

**What happens:** An unexpected exception escapes a C API function and propagates into C/Lua/FFI caller.

**Why it's wrong:** C++ exception ABI is not stable across FFI; callers cannot catch or safely unwind. Program may crash or exhibit undefined behavior.

**Do this instead:** Every C API entry point wears a try/catch that calls `quiver_set_last_error()` and returns `QUIVER_ERROR`. The error message is retrieved via `quiver_get_last_error()` by the caller.

### Storing Schema State During Schema Load

**What happens:** `load_schema_metadata()` assigns to `schema` or `type_validator` before validation completes.

**Why it's wrong:** If validation fails, the half-loaded state (schema set, validator null) survives and crashes the next metadata call.

**Do this instead:** Accumulate all state in local variables, validate completely, then assign to members **only after** `SchemaValidator::validate()` passes. Use the pattern in `database_impl.h`: load into temp, validate, publish.

### Manually Parameterizing SQL Instead of Calling `execute(sql, parameters)`

**What happens:** Code builds SQL by concatenating strings and supplying bound parameters via raw `sqlite3_bind_*` calls.

**Why it's wrong:** Parameter count mismatch is not validated; misaligned bindings leave trailing placeholders NULL-bound.

**Do this instead:** Always use `Database::execute(sql, parameters)` (private method, used internally). It validates that `parameters.size()` matches `sqlite3_bind_parameter_count()` and throws immediately.

---

*Architecture analysis: 2026-09-14*
