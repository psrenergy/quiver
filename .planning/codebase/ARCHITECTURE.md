# Architecture

## Pattern: Layered Library with FFI Bridge

Quiver is a **3-layer library**: C++ core, C API wrapper, and language bindings. Logic resides exclusively in C++. Each outer layer is a thin translation shim.

```
┌─────────────────────────────────────────────────┐
│  Language Bindings (Julia, Dart, Python, Lua)    │
│  Thin wrappers — no logic, no error messages     │
├─────────────────────────────────────────────────┤
│  C API (libquiver_c)                             │
│  FFI surface — try/catch, error codes, memory    │
├─────────────────────────────────────────────────┤
│  C++ Core (libquiver)                            │
│  All logic — Database, Element, Schema, Binary   │
│  Dependencies: SQLite, spdlog, toml++, sol2,     │
│  rapidcsv, Lua                                   │
└─────────────────────────────────────────────────┘
```

## C++ Core Layer

### Database Subsystem

Central class: `quiver::Database` (Pimpl pattern, hides `sqlite3*`).

**Lifecycle:**
1. `Database(path, options)` or `from_schema()`/`from_migrations()` — opens SQLite, loads schema metadata
2. `Schema::from_database(db)` introspects tables/columns/FKs at startup
3. `SchemaValidator` verifies structural conventions (Configuration table, naming, constraints)
4. `TypeValidator` validates data types against schema on every write

**Data flow (write):**
```
Element (builder) → resolve FK labels → validate types → SQL INSERT/UPDATE → SQLite
```

**Data flow (read):**
```
SQL SELECT → SQLite → type-aware extraction → std::vector/map/optional → caller
```

**Transaction model:**
- Public: `begin_transaction()`, `commit()`, `rollback()`
- Internal: `TransactionGuard` (RAII, nest-aware via `sqlite3_get_autocommit()`)
- Write methods work both standalone (auto-transaction) and inside explicit transactions

### Schema Subsystem

- `Schema` — in-memory model of all tables, columns, FKs, parsed from `sqlite_master`
- `SchemaValidator` — enforces conventions (Configuration table, naming patterns, FK actions)
- `TypeValidator` — runtime type checking against column definitions
- Schema conventions drive table discovery: `{Collection}_vector_{name}`, `{Collection}_set_{name}`, `{Collection}_time_series_{name}`

### Binary Subsystem

Standalone file I/O for multi-dimensional numeric arrays:
- `BinaryFile` (Pimpl) — read/write `double` arrays with dimension-based positioning
- `BinaryMetadata` (value type) — dimension definitions, TOML serialization
- `CSVConverter` (Pimpl) — binary-to-CSV and CSV-to-binary conversion
- Write registry — process-global path set prevents concurrent writers

### Embedded Scripting

- `LuaRunner` (Pimpl, hides Lua/sol2) — executes Lua scripts with `db` userdata
- Exposes nearly full Database API to Lua

## C API Layer

Translation layer: catches C++ exceptions, converts to error codes, manages C-style memory.

**Pattern per function:**
```cpp
quiver_error_t quiver_database_xyz(quiver_database_t* db, ..., out_type* out) {
    QUIVER_REQUIRE(db, ...);  // null-check macro
    try {
        // call C++ method, marshal result to C types
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

**Memory ownership:** Every allocation has a co-located `_free_*` function in the same translation unit.

**File organization mirrors C++ core:** `database_read.cpp`, `database_create.cpp`, etc.

## Binding Layer

Each binding follows identical structure:
1. FFI declarations (generated or hand-written)
2. Lifecycle wrapper (open/close with RAII or finalizer)
3. Method wrappers (thin: call C API, check error, extract result)
4. Convenience methods (composites like `read_scalars_by_id`, `transaction {}`)

No binding crafts error messages — they retrieve and surface C API errors.

## Entry Points

| Entry Point | Location | Purpose |
|-------------|----------|---------|
| C++ library | `include/quiver/database.h` | Direct C++ usage |
| C API | `include/quiver/c/database.h` | FFI surface for all bindings |
| CLI | `src/cli/main.cpp` | Command-line interface |
| Julia | `bindings/julia/src/Quiver.jl` | Julia module |
| Dart | `bindings/dart/lib/quiver.dart` | Dart package |
| Python | `bindings/python/src/quiverdb/__init__.py` | Python package |

## Key Abstractions

| Abstraction | Type | Purpose |
|-------------|------|---------|
| `Database` | Pimpl class | All database operations |
| `Element` | Value type | Builder for create/update operations |
| `Schema` | Value type | In-memory schema model |
| `BinaryFile` | Pimpl class | Binary file I/O |
| `BinaryMetadata` | Value type | Dimension metadata for binary files |
| `Value` | `std::variant<int64_t, double, std::string, std::nullptr_t>` | Type-safe value container |
| `Result` / `Row` | Value types | Query result containers |
