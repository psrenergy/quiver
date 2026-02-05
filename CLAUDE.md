# Project: Quiver

SQLite wrapper library with C++ core, C API for FFI, and language bindings (Julia, Dart).

## Architecture

```
include/quiver/           # C++ public headers
  database.h              # Database class - main API
  element.h               # Element builder for create operations
  lua_runner.h            # Lua scripting support
include/quiver/c/         # C API headers (for FFI)
  database.h
  element.h
  lua_runner.h
src/                      # C++ implementation
src/c/                    # C API implementation
bindings/julia/           # Julia bindings (Quiver.jl)
bindings/dart/            # Dart bindings (quiver)
tests/                    # C++ tests
tests/schemas/            # Shared SQL schemas for all tests
```

## Principles

- **Human-Centric**: Codebase optimized for human readability, not machine parsing
- **Status**: WIP project - breaking changes acceptable, no backwards compatibility required
- **ABI Stability**: Verify correct usage of Pimpl idiom
- **Target Standard**: C++20 - use modern language features where they simplify logic
- **Philosophy**: Clean code over defensive code (assume callers obey contracts, avoid excessive null checks). Simple solutions over complex abstractions. Delete unused code, do not deprecate.
- **Intelligence**: Logic resides in C++ layer. Bindings/wrappers remain thin.
- **Error Messages**: All error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own.
- **Homogeneity**: Binding interfaces must be consistent and intuitive. API surface should feel uniform across wrappers.
- **Ownership**: RAII used strictly. Ownership of pointers/resources must be explicit and unambiguous.
- **Constraint**: Be critical. If code is already optimal, state that clearly. Do not invent useless suggestions just to provide output.
- All public C++ methods should be bound to C API, then to Julia/Dart/Lua
- All *.sql test schemas in `tests/schemas/`, bindings reference from there
- **Self-Updating**: Always keep CLAUDE.md up to date with codebase changes

## Build & Test

### Build
```bash
cmake --build build --config Debug
```

### Build and Test All
```bash
scripts/build-all.bat            # Build everything + run all tests (Debug)
scripts/build-all.bat --release  # Build in Release mode
scripts/test-all.bat             # Run all tests (assumes already built)
```

### Individual Tests
```bash
./build/bin/quiver_tests.exe      # C++ core library tests
./build/bin/quiver_c_tests.exe    # C API tests
bindings/julia/test/test.bat      # Julia tests
bindings/dart/test/test.bat       # Dart tests
```

### Test Organization
Test files organized by functionality:
- `test_database_lifecycle.cpp` - open, close, move semantics, options
- `test_database_create.cpp` - create element operations
- `test_database_read.cpp` - read scalar/vector/set operations
- `test_database_update.cpp` - update scalar/vector/set operations
- `test_database_delete.cpp` - delete element operations
- `test_database_query.cpp` - parameterized and non-parameterized query operations
- `test_database_relations.cpp` - relation operations
- `test_database_time_series.cpp` - time series read/update/metadata operations

C API tests follow same pattern with `test_c_api_database_*.cpp` prefix.

All test schemas located in `tests/schemas/valid/` and `tests/schemas/invalid/`.

## C++ Patterns

### Pimpl
All implementation in `Database::Impl`, public header hides dependencies:
```cpp
// database.h (public)
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// database.cpp (private)
struct Database::Impl {
    sqlite3* db;
    // all implementation details
};
```

### Transactions
Use `Impl::TransactionGuard` RAII or `impl_->with_transaction(lambda)`:
```cpp
// RAII guard
{
    TransactionGuard guard(db);
    // operations...
    guard.commit();
}

// Lambda wrapper
impl_->with_transaction([&]() {
    // operations...
});
```

### Error Handling
Exceptions with descriptive messages, no error codes:
```cpp
throw std::runtime_error("Collection not found: " + name);
```

### Logging
spdlog with debug/info/warn/error levels:
```cpp
spdlog::debug("Opening database: {}", path);
spdlog::error("Failed to execute query: {}", sqlite3_errmsg(db));
```

### Move Semantics
Delete copy, default move for resource types:
```cpp
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&&) = default;
Database& operator=(Database&&) = default;
```

### Factory Methods
Static methods for database creation:
```cpp
static Database from_schema(const std::string& path, const std::string& schema);
static Database from_migrations(const std::string& path, const std::vector<std::string>& migrations);
```

## C API Patterns

### Return Codes
All C API functions return `quiver_error_t`. Values are returned via output parameters.
Exceptions: free functions (void), error/version utilities (direct return), and `quiver_database_options_default` (struct by value).

### Error Handling
Try-catch with `quiver_set_last_error()`, return codes:
```cpp
quiver_error_t quiver_some_function(quiver_database_t* db) {
    if (!db) {
        quiver_set_last_error("Null database pointer");
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        // operation...
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}
```

### Factory Functions
Factory functions use out-parameters and return `quiver_error_t`:
```cpp
quiver_database_t* db = nullptr;
quiver_error_t err = quiver_database_from_schema(db_path, schema_path, &options, &db);
if (err != QUIVER_OK) {
    const char* msg = quiver_get_last_error();
    // handle error
}
// use db...
quiver_database_close(db);
```

### Memory Management
`new`/`delete`, provide matching `quiver_free_*` functions:
```cpp
// Factory functions return error code, use out-parameter for handle
quiver_error_t quiver_database_from_schema(..., quiver_database_t** out_db);
void quiver_database_close(quiver_database_t* db);

char** quiver_read_strings(...);
void quiver_free_strings(char** strings, int count);
```

### String Handling
Always null-terminate, use `strdup_safe()`:
```cpp
static char* strdup_safe(const std::string& str) {
    char* result = new char[str.size() + 1];
    std::memcpy(result, str.c_str(), str.size() + 1);
    return result;
}
```

### Null Checks
Validate all pointer arguments first:
```cpp
if (!db) { quiver_set_last_error("Null db"); return QUIVER_ERROR; }
if (!collection) { quiver_set_last_error("Null collection"); return QUIVER_ERROR; }
```

### Parameterized Queries
`_params` variants use parallel arrays for typed parameters:
```c
// param_types[i]: QUIVER_DATA_TYPE_INTEGER(0), FLOAT(1), STRING(2), NULL(4)
// param_values[i]: pointer to int64_t, double, const char*, or NULL
quiver_database_query_string_params(db, sql, param_types, param_values, param_count, &out, &has);
```

## Schema Conventions

### Configuration Table (Required)
Every schema must have a Configuration table:
```sql
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;
```

### Collections
Standard collection pattern:
```sql
CREATE TABLE MyCollection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    -- scalar attributes
) STRICT;
```

### Vector Tables
Named `{Collection}_vector_{name}` with composite PK:
```sql
CREATE TABLE Items_vector_values (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

### Set Tables
Named `{Collection}_set_{name}` with UNIQUE constraint:
```sql
CREATE TABLE Items_set_tags (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    value TEXT NOT NULL,
    UNIQUE (id, value)
) STRICT;
```

### Time Series Tables
Named `{Collection}_time_series_{name}` with a dimension (ordering) column whose name starts with `date_` (e.g., `date_time`):
```sql
CREATE TABLE Items_time_series_data (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

### Time Series Files Tables
Named `{Collection}_time_series_files`, singleton table for file path references:
```sql
CREATE TABLE Items_time_series_files (
    data_file TEXT,
    metadata_file TEXT
) STRICT;
```

### Foreign Keys
Always use `ON DELETE CASCADE ON UPDATE CASCADE` for parent references.

## Core API

### Database Class
- Factory methods: `from_schema()`, `from_migrations()`
- CRUD: `create_element(collection, element)`
- Scalar readers: `read_scalar_integers/floats/strings(collection, attribute)`
- Vector readers: `read_vector_integers/floats/strings(collection, attribute)`
- Set readers: `read_set_integers/floats/strings(collection, attribute)`
- Time series: `read_time_series_group_by_id()`, `update_time_series_group()`
- Time series metadata: `get_time_series_metadata()`, `list_time_series_groups()` - includes `dimension_column` (the ordering column name)
- Time series files: `has_time_series_files()`, `list_time_series_files_columns()`, `read_time_series_files()`, `update_time_series_files()`
- Relations: `set_scalar_relation()`, `read_scalar_relation()`
- Query: `query_string/integer/float(sql, params = {})` - parameterized SQL with positional `?` placeholders
- Schema inspection: `describe()` - prints schema info to stdout

### Element Class
Builder for element creation with fluent API:
```cpp
Element().set("label", "Item 1").set("value", 42).set("tags", {"a", "b"})
```

### LuaRunner Class
Executes Lua scripts with database access:
```cpp
LuaRunner lua(db);
lua.run(R"(
    db:create_element("Collection", { label = "Item", value = 42 })
    local values = db:read_scalar_integers("Collection", "value")
)");
```

## Bindings

### Regenerating FFI Bindings
When C API changes, regenerate:
```bash
bindings/julia/generator/generator.bat   # Julia
bindings/dart/generator/generator.bat    # Dart
```

### Julia Notes
- Delete `bindings/julia/Manifest.toml` if version conflicts, then:
  ```bash
  julia --project=bindings/julia -e "using Pkg; Pkg.instantiate()"
  ```

### Dart Notes
- `libquiver_c.dll` depends on `libquiver.dll` - both must be in PATH
- test.bat handles PATH setup automatically
- When C API struct layouts change, clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` to force a fresh DLL rebuild
