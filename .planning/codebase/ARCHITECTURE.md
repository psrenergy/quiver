<!-- refreshed: 2026-09-17 -->
# Architecture

**Analysis Date:** 2026-09-17

## System Overview

```text
┌──────────────────────────────────────────────────────────────────────────┐
│                          Binding Layer (FFI)                             │
├──────────────┬──────────────┬──────────────┬──────────────┬──────────────┤
│   Julia      │    Dart      │   Python     │      JS      │   Lua        │
│  (native)    │ (ffigen+hook)│   (CFFI)     │  (Bun FFI)   │  (sol2)      │
└──────┬───────┴──────┬───────┴──────┬───────┴──────┬───────┴──────┬───────┘
       │              │              │              │              │
       └──────────────┼──────────────┼──────────────┼──────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         C API Layer (FFI Bridge)                         │
│                    `include/quiver/c/`, `src/c/`                         │
│  Marshaling, error handling, memory management, all entry points via    │
│  `quiver_database_*`, `quiver_element_*`, `quiver_lua_runner_*` etc.   │
└──────────────────────────────────────────────────────────┬───────────────┘
                                                          │
                      ┌───────────────────────────────────┘
                      │
                      ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                      C++ Core Layer (Logic)                              │
│          `include/quiver/`, `src/` (Database, LuaRunner, etc.)           │
│  • Database CRUD, transactions, schema metadata, validation              │
│  • LuaRunner: Executes Lua scripts with db access (sol2)                │
│  • Binary subsystem: .qvr file I/O (Julia + Lua only)                   │
│  • Expression subsystem: Lazy expressions (Julia + Lua only)            │
└──────────────────────────────────┬───────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                            SQLite Database                               │
│              sqlite3 (v3.50.2) via libsqlite3, in-process               │
└──────────────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| **Database (C++)** | Main CRUD API, transactions, schema lazy-loading, validation | `include/quiver/database.h`, `src/database*.cpp` |
| **Database::Impl** | Private: pimpl holder, schema/type validators, label→id resolution | `src/database_impl.h` |
| **C API** | FFI marshaling, memory management, error propagation | `include/quiver/c/database.h`, `src/c/database*.cpp` |
| **LuaRunner** | Lua script execution with db access, sandboxed file I/O | `src/lua_runner.cpp` |
| **BinaryFile** | .qvr file I/O (read/write with metadata) | `include/quiver/binary/binary_file.h`, `src/binary/binary_file.cpp` |
| **Expression** | Lazy DAG expressions over .qvr files | `include/quiver/expression/expression.h`, `src/expression/*.cpp` |
| **Schema** | Introspection: tables, columns, groups, metadata | `src/schema.cpp` |
| **TypeValidator** | Value-vs-column type checking (scalars, arrays, DATE_TIME) | `src/type_validator.cpp` |
| **SchemaValidator** | Schema convention checks (required Configuration table, FK actions, etc.) | `src/schema_validator.cpp` |
| **FFI Bindings** | Language-specific wrappers (Julia, Dart, Python, JS) | `bindings/{julia,dart,python,js}/` |

## Pattern Overview

**Overall:** Layered architecture with thin FFI bindings over a C API facade to a C++ core

**Key Characteristics:**
- **FFI-first design**: C API is the single public interface; all external code speaks C
- **Pimpl encapsulation**: Database, LuaRunner, BinaryFile hide private dependencies (sqlite3, lua headers)
- **Lazy schema loading**: Metadata not loaded at `Database(path)` but on first use (`require_schema`)
- **Nest-aware RAII**: TransactionGuard knows when a transaction is already active; dry runs reuse transactions
- **Single error channel**: `quiver_get_last_error()` is the only error reporting surface across all layers
- **Binding uniformity**: All bindings expose the same API surface (cross-layer naming conventions)

## Layers

**C++ Core (`src/`, `include/quiver/`):**
- Purpose: All business logic, validation, SQL execution, file I/O
- Location: `include/quiver/database.h` (public API), `src/database*.cpp` (implementation)
- Contains: Database CRUD, metadata introspection, transactions, migration validation, CSV import/export
- Depends on: sqlite3, spdlog, lua (for LuaRunner), binary/expression libraries
- Used by: C API layer, which is used by all FFI bindings

**C API Layer (`src/c/`, `include/quiver/c/`):**
- Purpose: FFI marshaling, memory management, error translation, opaque handles
- Location: `include/quiver/c/database.h` (public API), `src/c/database*.cpp` (implementation)
- Contains: `quiver_database_*` functions, element builder, metadata, time-series group readers/writers
- Depends on: C++ core (calls C++ methods, catches exceptions, marshals to C types)
- Used by: All FFI bindings (Julia Clang.jl, Dart ffigen, Python CFFI, JS Bun FFI)

**Lua Runtime (`src/lua_runner.cpp`):**
- Purpose: Execute Lua scripts against a database (direct sol2 binding, NOT through C API)
- Location: `include/quiver/lua_runner.h`, `src/lua_runner.cpp`
- Contains: Lua→C++ converters, sandboxed file I/O, JSON encoder for script return values
- Depends on: C++ core directly, sol2 library
- Used by: C API (exposed as `quiver_lua_runner_*`), then by all FFI bindings

**Binary Subsystem (`src/binary/`, `include/quiver/binary/`):**
- Purpose: .qvr file I/O with metadata sidecars
- Location: `include/quiver/binary/binary_file.h`, `src/binary/binary_file.cpp`
- Contains: BinaryFile (Pimpl), CSVConverter, BinaryMetadata, dimension iteration
- Depends on: sqlite3 (for CSV parsing), no C API
- **Bound in: Julia and Lua only** (deliberate — see root design decisions)

**Expression Subsystem (`src/expression/`, `include/quiver/expression/`):**
- Purpose: Lazy arithmetic expressions over .qvr files
- Location: `include/quiver/expression/expression.h`, `src/expression/*.cpp`
- Contains: Expression DAG nodes (binary ops, unary math, aggregations, label projections), save/compute
- Depends on: BinaryFile, binary metadata, no C API
- **Bound in: Julia and Lua only** (deliberate — see root design decisions)

## Data Flow

### Primary Request Path: `create_element` (Dart example)

1. **Dart caller** (`bindings/dart/lib/src/database.dart`): `db.createElement("Items", item)`
   - Calls `_marshalParams` to convert dart types to C pointers
   - Invokes `bindings.quiver_database_create_element(...)` (ffigen-generated)

2. **Bun FFI loader** (`bindings/js/src/loader.ts` — if using JS; Dart uses native-assets):
   - Loads `libquiver_c.dll` / `.dylib` / `.so` with native-assets build hook
   - Maps symbol names to C function pointers

3. **C API entry point** (`src/c/database_create.cpp`):
   ```cpp
   quiver_error_t quiver_database_create_element(...) {
       QUIVER_REQUIRE(db);
       try {
           // Unmarshal C element to C++ Element
           Element cpp_elem = ...;
           // Call C++ core
           int64_t id = db->database->create_element(collection, cpp_elem);
           *out_id = id;
           return QUIVER_OK;
       } catch (const std::exception& e) {
           quiver_set_last_error(e.what());
           return QUIVER_ERROR;
       }
   }
   ```

4. **C++ Core** (`src/database_create.cpp`, `Database::create_element`):
   - Lazy-loads schema (via `impl_->require_schema()`) if not yet loaded
   - Validates element (via `impl_->type_validator`)
   - Inserts into collection table: `INSERT INTO {collection} (...) VALUES (...)`
   - Inserts into group tables (vector/set/time-series) if element has groups
   - Returns new row ID or throws Pattern 1/2/3 error

5. **SQLite** (`build/lib/libsqlite3.a`):
   - Executes INSERT, returns ROWID or constraint error
   - Manages indexes, foreign keys, transactions

6. **Return path**:
   - C++ exception (or SQLite error) → caught by C API → `quiver_set_last_error()` → return `QUIVER_ERROR`
   - Success → C API marshals return value to C types → Dart caller reads via FFI

### Schema Metadata Lazy-Loading Path

1. **Caller (any layer)** invokes any read/metadata/CRUD operation
2. **C++ core** checks `if (!impl_->schema)` in `require_schema()` call
3. **First time only**: Load schema from `PRAGMA table_info()`, validate via `SchemaValidator`
4. **Publish**: Assign to both `impl_->schema` and `impl_->type_validator` (or throw; neither is set on error)
5. **All subsequent calls**: Skip load, use cached `impl_->schema`

Rationale: `Database(path)` doesn't validate schema, so `open()` works even on half-migrated DBs. Validation on first use allows `migrate_up()` to run before any metadata reads.

### Transaction Nesting (TransactionGuard)

1. **Caller** (C++, Lua, binding wrapper) invokes `db.begin_transaction()`
   - Sets `sqlite3_get_autocommit() = 0` → transaction active

2. **Write method** (e.g., `create_element`) called inside transaction
   - Creates `TransactionGuard guard(impl_)` in method body
   - Guard checks `sqlite3_get_autocommit()` → finds transaction already active → becomes no-op
   - Proceeds with INSERT without nesting `BEGIN`

3. **Normal flow**:
   - Explicit: `begin_transaction()` → `create_element()` → ... → `commit()`
   - Implicit (standalone write): `create_element()` → guard begins, executes, commits inside method

4. **Dry run**:
   - `begin_dry_run()` → sets flag, opens real transaction
   - Subsequent `begin_transaction()` calls → no-op (flag check)
   - `end_dry_run()` → clears flag, calls `sqlite3_rollback()` directly (not through public `rollback()`)

## Key Abstractions

**Pimpl (Pointer to Implementation):**
- `Database::Impl` (contains `sqlite3*`, validators, logger) — hides sqlite3 headers from public API
- `BinaryFile::Impl` (contains file I/O state) — hides platform-specific file ops
- `LuaRunner` is NOT Pimpl (its `impl_` is a `lua_State*`, which is internal-use-only, but the pattern is inversion: C++ wrapper over C library)

**Element Builder:**
- `Element` value type with fluent `.set(name, value)` API, converts to SQL INSERT/UPDATE
- Used in C++, wrapped in C API opaque `quiver_element_t`, language bindings provide their own builders (Dart `Element`, Python `**kwargs`, etc.)

**Result Types:**
- `Result<T>` (C++): Always has a value; exceptions surface errors (not used after C API)
- `Row` (C++): Query result row, typed accessors `get<T>(column)`
- Bindings: Language-native types (lists, dicts, objects)

**Metadata Types:**
- `ScalarMetadata`: column name, type, not_null flag, primary_key flag
- `GroupMetadata`: group name, dimension_column (NULL for vectors/sets, populated for time-series), column names + types
- C API: parallel `quiver_scalar_metadata_t`, `quiver_group_metadata_t` structs

## Entry Points

**C++ Library:**
- `quiver/database.h` — `Database` class, factories, CRUD, transactions
- `quiver/lua_runner.h` — `LuaRunner`, script execution
- `quiver/binary/binary_file.h` — `BinaryFile` (Julia/Lua only)
- `quiver/expression/expression.h` — `Expression` (Julia/Lua only)

**C API:**
- `quiver/c/database.h` — All `quiver_database_*` and `quiver_element_*` functions
- `quiver/c/common.h` — `quiver_get_last_error`, version
- `quiver/c/lua_runner.h` — `quiver_lua_runner_*` functions

**CLI:**
- `src/cli/main.cpp` — `quiver_cli` executable, reads `--schema`, `--migrations`, runs Lua script

**Bindings:**
- Julia: `bindings/julia/src/Quiver.jl` (module root), `src/database.jl` (main API)
- Dart: `bindings/dart/lib/quiverdb.dart` (public import), `lib/src/database.dart` (implementation)
- Python: `bindings/python/quiverdb/__init__.py`, `quiverdb/database.py`
- JS: `bindings/js/src/index.ts`, `bindings/js/src/database.ts`

## Architectural Constraints

- **Threading:** Single-threaded event loop within Lua; C++ core and C API are not thread-safe (sqlite3 built with `SQLITE_THREADSAFE=0`)
- **Global state:** 
  - C API error string via thread-local `std::string` in `common.cpp` (safe across threads, though database access itself is not)
  - Binary subsystem write registry: static `unordered_set<string>` in `binary_file.cpp` (single-process, in-memory only)
- **Circular imports:** None by design (C API depends on C++, bindings depend on C API, binary/expression depend on core data types but NOT on C API)
- **Lazy loading:** Schema metadata loads on first use, must not be accessed during half-migrated DB state

## Anti-Patterns

### Accessing Struct Members Across Layers

**What happens:** A binding tries to read a struct field directly from the C API (e.g., accessing `quiver_scalar_metadata_t.not_null` directly instead of a getter function)

**Why it's wrong:** Layout stability is not guaranteed; ffigen-generated bindings may change if the C header changes; future refactoring (e.g., turning a field into a computed property) breaks binary compatibility

**Do this instead:** All struct access in FFI bindings goes through the C API surface. Define getter/setter functions in `src/c/` if a binding needs to access a new field.

### Replicating Type Validation in Bindings

**What happens:** A binding creates its own `isinstance(value, int)` check before calling the C API, duplicating the core's validation

**Why it's wrong:** Inconsistency across layers. The core's `TypeValidator` is the single source of truth for what values are valid. A binding-level guard catches different cases (e.g., Dart's `bool` is not an `int`, but Python's is), and divergence means one binding accepts what another rejects.

**Do this instead:** All type validation happens in the C++ core. Bindings pass values through as-is (with their language's natural representation) and let the C API unmarshaler handle it. Marshaling errors that the core cannot see (e.g., "cell #3 has unsupported Lua type") are caught locally with a locally-crafted message naming the column/cell.

### Reimplementing Group Inserts

**What happens:** Adding a new group-write path (e.g., `update_vector_group_sparse`) duplicates the validation, row transposition, or constraint-checking logic from `insert_rows_into_group_table`

**Why it's wrong:** `insert_rows_into_group_table` is the unified helper. Every group insert (vector/set/time-series, scalar/array, create/update) routes through it. Duplicating it means bugs fixed in one path are not fixed in others, and load-bearing details (validation before DELETE, row-count seeding from first column) are easy to miss.

**Do this instead:** All group inserts call `Impl::insert_rows_into_group_table()`. New write operations (create, update, upsert) route through it or delegate to an existing write that does.

## Error Handling

**Strategy:** Exceptions in C++, translated to C error codes + string message

**Patterns:**
- **Pattern 1 (Precondition failure):** `"Cannot {operation}: {reason}"` — caller violated a contract
  ```cpp
  throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
  ```
- **Pattern 2 (Not found):** `"{Entity} not found: {identifier}"` — data doesn't exist
  ```cpp
  throw std::runtime_error("Element not found: 42 in collection 'Items'");
  ```
- **Pattern 3 (Operation failure):** `"Failed to {operation}: {reason}"` — I/O or constraint error
  ```cpp
  throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
  ```

All error strings propagate through `quiver_set_last_error()` to the C API, then to every binding via `quiver_get_last_error()`. No exception types are used; only the message matters.

## Cross-Cutting Concerns

**Logging:** Per-database logger via spdlog (`impl_->logger`, named `quiver_database_<id>`), sent to stderr + a per-database log file. Controlled by `DatabaseOptions::console_level`.

**Validation:**
- **Type validation** (scalars, arrays, DATE_TIME grammar): `TypeValidator` in C++, called on every write before group INSERT
- **Schema validation** (Configuration table exists, FKs use CASCADE, no duplicate attributes): `SchemaValidator` on schema load
- **Migration validation** (up then down round-trip, no tables left behind): `Database::validate_migrations()`

**Authentication:** None (Quiver is a local/embedded library, not a server)

---

*Architecture analysis: 2026-09-17*
