# Project: Quiver

SQLite wrapper library with C++ core, C API for FFI, and language bindings (Julia, Dart).

## Architecture

```
include/quiver/           # C++ public headers
  database.h              # Database class - main API
  attribute_metadata.h    # ScalarMetadata, GroupMetadata types
  options.h               # DatabaseOptions, CSVExportOptions types and factories
  element.h               # Element builder for create operations
  lua_runner.h            # Lua scripting support
include/quiver/c/         # C API headers (for FFI)
  options.h               # All option types: LogLevel, DatabaseOptions, CSVExportOptions
  database.h
  element.h
  lua_runner.h
src/                      # C++ implementation
  database_csv.cpp        # CSV export implementation
src/c/                    # C API implementation
  internal.h              # Shared structs (quiver_database, quiver_element), QUIVER_REQUIRE macro
  database_helpers.h      # Marshaling templates, strdup_safe, metadata converters
  database.cpp            # Lifecycle: open, close, factory methods, describe
  database_csv.cpp        # CSV export: options conversion, export function
  database_create.cpp     # Element CRUD: create, update, delete
  database_read.cpp       # All read operations + co-located free functions
  database_update.cpp     # All scalar/vector/set update operations
  database_metadata.cpp   # Metadata get/list + co-located free functions
  database_query.cpp      # Query operations (plain and parameterized)
  database_time_series.cpp # Time series operations + co-located free functions
  database_relations.cpp  # Relation operations
  database_transaction.cpp # Transaction control (begin, commit, rollback, in_transaction)
bindings/julia/           # Julia bindings (Quiver.jl)
bindings/dart/            # Dart bindings (quiver)
bindings/python/          # Python bindings (quiver) - CFFI ABI-mode
tests/                    # C++ tests
tests/schemas/            # Shared SQL schemas for all tests
```

## Principles

- **Human-Centric**: Codebase optimized for human readability, not machine parsing
- **Status**: WIP project - breaking changes acceptable, no backwards compatibility required
- **ABI Stability**: Use Pimpl only when hiding private dependencies; plain value types use Rule of Zero
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
bindings/python/test/test.bat     # Python tests
```

### Benchmark
```bash
./build/bin/quiver_benchmark.exe  # Transaction performance benchmark (run manually)
```
Standalone executable comparing individual vs batched transaction performance. Not part of test suite. Built by `build-all.bat` but never executed automatically.

### Test Organization
Test files organized by functionality:
- `test_database_lifecycle.cpp` - open, close, move semantics, options
- `test_database_create.cpp` - create element operations
- `test_database_csv.cpp` - CSV export operations (scalar, group, options, formatting)
- `test_database_read.cpp` - read scalar/vector/set operations
- `test_database_update.cpp` - update scalar/vector/set operations
- `test_database_delete.cpp` - delete element operations
- `test_database_query.cpp` - parameterized and non-parameterized query operations
- `test_database_relations.cpp` - relation operations
- `test_database_time_series.cpp` - time series read/update/metadata operations
- `test_database_transaction.cpp` - explicit transaction control (begin/commit/rollback)

C API tests follow same pattern with `test_c_api_database_*.cpp` prefix:
- `test_c_api_database_csv.cpp` - CSV export options struct, enum labels, date formatting, scalar/group export

All test schemas located in `tests/schemas/valid/` and `tests/schemas/invalid/`.

## C++ Patterns

### Pimpl vs Value Types
Pimpl is used only for classes that hide private dependencies (e.g., `Database`, `LuaRunner` hide sqlite3/lua headers):
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

Classes with no private dependencies (`Element`, `Row`, `Migration`, `Migrations`, `GroupMetadata`, `ScalarMetadata`, `CSVExportOptions`) are plain value types — direct members, no Pimpl, Rule of Zero (compiler-generated copy/move/destructor).

### Transactions
Public API exposes explicit transaction control:
```cpp
db.begin_transaction();
// multiple write operations...
db.commit();   // or db.rollback();
bool active = db.in_transaction();
```

Internally, `Impl::TransactionGuard` is nest-aware RAII: if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`), the guard becomes a no-op. This allows write methods (`create_element`, `update_vector_*`, etc.) to work both standalone and inside explicit transactions without double-beginning.

```cpp
// Internal RAII guard (nest-aware)
{
    TransactionGuard guard(impl);
    // operations...
    guard.commit();
}
```

### Naming Convention
Public Database methods follow `verb_[category_]type[_by_id]` pattern:
- **Verbs:** create, read, update, delete, get, list, has, query, describe, export, import
- **`_by_id` suffix:** Only for reads where both "all elements" and "single element" variants exist (e.g., `read_scalar_integers` vs `read_scalar_integer_by_id`)
- **Singular vs plural:** Type name matches return cardinality (`read_scalar_integers` returns vector, `read_scalar_integer_by_id` returns optional)
- **Examples:** `create_element`, `read_vector_floats_by_id`, `get_scalar_metadata`, `list_time_series_groups`, `update_scalar_relation`

### Error Handling
All `throw std::runtime_error(...)` in the C++ layer use exactly 3 message patterns:

**Pattern 1 -- Precondition failure:** `"Cannot {operation}: {reason}"`
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error("Cannot update_scalar_relation: attribute 'x' is not a foreign key in collection 'Y'");
```

**Pattern 2 -- Not found:** `"{Entity} not found: {identifier}"`
```cpp
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");
throw std::runtime_error("Schema file not found: path/to/schema.sql");
throw std::runtime_error("Time series table not found: Items_time_series_data");
```

**Pattern 3 -- Operation failure:** `"Failed to {operation}: {reason}"`
```cpp
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
throw std::runtime_error("Failed to execute statement: " + sqlite3_errmsg(db));
```

No ad-hoc formats. Downstream layers (C API, bindings) can rely on consistent error structure.

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
All C API functions return binary `quiver_error_t` (`QUIVER_OK = 0` or `QUIVER_ERROR = 1`). Values are returned via output parameters.
Exceptions: `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`, `quiver_database_options_default` (utility functions with direct return).

### Error Handling
Try-catch with `quiver_set_last_error()`, binary return codes. Error details come from `quiver_get_last_error()`:
```cpp
quiver_error_t quiver_some_function(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        // operation...
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

### Factory Functions
Factory functions use out-parameters and return `quiver_error_t`:
```cpp
auto options = quiver_database_options_default();
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
`new`/`delete`, provide matching `quiver_{entity}_free_*` functions:
```cpp
// Factory functions return error code, use out-parameter for handle
quiver_error_t quiver_database_from_schema(..., quiver_database_t** out_db);
void quiver_database_close(quiver_database_t* db);

// Database entity free functions (arrays, vectors, metadata, time series)
quiver_database_free_integer_array(int64_t*)
quiver_database_free_float_array(double*)
quiver_database_free_string_array(char**, size_t)

// Element entity free function (strings returned by element/query operations)
quiver_element_free_string(char*)
```

### Metadata Types
Unified `quiver_group_metadata_t` for vector, set, and time series groups. `dimension_column` is `NULL` for vectors/sets, populated for time series. Single free functions:
```cpp
quiver_database_free_scalar_metadata(quiver_scalar_metadata_t*)
quiver_database_free_group_metadata(quiver_group_metadata_t*)
quiver_database_free_scalar_metadata_array(quiver_scalar_metadata_t*, size_t)
quiver_database_free_group_metadata_array(quiver_group_metadata_t*, size_t)
```
Internal helpers `convert_scalar_to_c`, `convert_group_to_c`, `free_scalar_fields`, `free_group_fields` in `src/c/database_helpers.h` avoid duplication.

### Alloc/Free Co-location
Every allocation function and its corresponding free function live in the same translation unit. Read alloc/free pairs in `database_read.cpp`, metadata alloc/free pairs in `database_metadata.cpp`, time series alloc/free pairs in `database_time_series.cpp`.

### String Handling
Always null-terminate, use `strdup_safe()`:
```cpp
static char* strdup_safe(const std::string& str) {
    char* result = new char[str.size() + 1];
    std::memcpy(result, str.c_str(), str.size() + 1);
    return result;
}
```

### Multi-Column Time Series
The C API uses a columnar typed-arrays pattern for time series read and update:
- `quiver_database_update_time_series_group()` accepts parallel arrays: `column_names[]`, `column_types[]`, `column_data[]`, `column_count`, `row_count`. Pass `column_count == 0` and `row_count == 0` with NULL arrays to clear all rows.
- `quiver_database_read_time_series_group()` returns columnar typed arrays matching the schema. Column data arrays are typed: `INTEGER` -> `int64_t*`, `FLOAT` -> `double*`, `STRING`/`DATE_TIME` -> `char**`.
- `quiver_database_free_time_series_data()` deallocates read results. String columns require per-element cleanup; numeric columns use a single `delete[]`.

This pattern mirrors the `convert_params()` approach from `database_query.cpp` for type-safe FFI marshaling across N typed columns.

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
- Transaction control: `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()`
- CRUD: `create_element(collection, element)`
- Scalar readers: `read_scalar_integers/floats/strings(collection, attribute)`
- Vector readers: `read_vector_integers/floats/strings(collection, attribute)`
- Set readers: `read_set_integers/floats/strings(collection, attribute)`
- Time series: `read_time_series_group()`, `update_time_series_group()` — both use multi-column `vector<map<string, Value>>` rows with N typed value columns per time series group
- Time series files: `has_time_series_files()`, `list_time_series_files_columns()`, `read_time_series_files()`, `update_time_series_files()`
- Metadata: `get_scalar_metadata()`, `get_vector_metadata()`, `get_set_metadata()`, `get_time_series_metadata()` — all group metadata returns unified `GroupMetadata` with `dimension_column` (populated for time series, empty for vectors/sets)
- List groups: `list_scalar_attributes()`, `list_vector_groups()`, `list_set_groups()`, `list_time_series_groups()`
- Relations: `update_scalar_relation()`, `read_scalar_relation()`
- Query: `query_string/integer/float(sql, params = {})` - parameterized SQL with positional `?` placeholders
- Schema inspection: `describe()` - prints schema info to stdout
- CSV: `export_csv()` -- exports collection scalars or groups to CSV file with optional enum/date formatting via `CSVExportOptions`

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

## Cross-Layer Naming Conventions

### Transformation Rules

| From C++ | To C API | To Julia | To Dart | To Python | To Lua |
|----------|----------|----------|---------|-----------|--------|
| `method_name` | `quiver_database_method_name` | `method_name` (+ `!` if mutating) | `methodName` | `method_name` | `method_name` |
| `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | `Database.from_schema()` | N/A |

- **C++ to C API:** Prefix `quiver_database_` to the C++ method name. Example: `create_element` -> `quiver_database_create_element`
- **C++ to Julia:** Same name. Add `!` suffix for mutating operations (create, update, delete). Example: `create_element` -> `create_element!`, `read_scalar_integers` -> `read_scalar_integers`
- **C++ to Dart:** Convert `snake_case` to `camelCase`. Example: `read_scalar_integers` -> `readScalarIntegers`. Factory methods use Dart named constructors: `from_schema` -> `Database.fromSchema()`
- **C++ to Python:** Same `snake_case` name. Factory methods are `@staticmethod`: `from_schema` -> `Database.from_schema()`. Properties are regular methods (not `@property`).
- **C++ to Lua:** Same name exactly (1:1 match). Example: `read_scalar_integers` -> `read_scalar_integers`. Lua has no lifecycle methods (open/close) -- database is provided as `db` userdata by LuaRunner.

### Representative Cross-Layer Examples

| Category | C++ | C API | Julia | Dart | Python | Lua |
|----------|-----|-------|-------|------|--------|-----|
| Factory | `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | `Database.from_schema()` | N/A |
| Transaction | `begin_transaction()` | `quiver_database_begin_transaction()` | `begin_transaction!()` | `beginTransaction()` | `begin_transaction()` | `begin_transaction()` |
| Transaction | `commit()` | `quiver_database_commit()` | `commit!()` | `commit()` | `commit()` | `commit()` |
| Transaction | `rollback()` | `quiver_database_rollback()` | `rollback!()` | `rollback()` | `rollback()` | `rollback()` |
| Transaction | `in_transaction()` | `quiver_database_in_transaction()` | `in_transaction()` | `inTransaction()` | `in_transaction()` | `in_transaction()` |
| Create | `create_element()` | `quiver_database_create_element()` | `create_element!()` | `createElement()` | `create_element()` | `create_element()` |
| Read scalar | `read_scalar_integers()` | `quiver_database_read_scalar_integers()` | `read_scalar_integers()` | `readScalarIntegers()` | `read_scalar_integers()` | `read_scalar_integers()` |
| Read by ID | `read_scalar_integer_by_id()` | `quiver_database_read_scalar_integer_by_id()` | `read_scalar_integer_by_id()` | `readScalarIntegerById()` | `read_scalar_integer_by_id()` | `read_scalar_integer_by_id()` |
| Update scalar | `update_scalar_integer()` | `quiver_database_update_scalar_integer()` | `update_scalar_integer!()` | `updateScalarInteger()` | `update_scalar_integer()` | `update_scalar_integer()` |
| Update vector | `update_vector_strings()` | `quiver_database_update_vector_strings()` | `update_vector_strings!()` | `updateVectorStrings()` | `update_vector_strings()` | `update_vector_strings()` |
| Delete | `delete_element()` | `quiver_database_delete_element()` | `delete_element!()` | `deleteElement()` | `delete_element()` | `delete_element()` |
| Metadata | `get_scalar_metadata()` | `quiver_database_get_scalar_metadata()` | `get_scalar_metadata()` | `getScalarMetadata()` | `get_scalar_metadata()` | `get_scalar_metadata()` |
| List groups | `list_vector_groups()` | `quiver_database_list_vector_groups()` | `list_vector_groups()` | `listVectorGroups()` | `list_vector_groups()` | `list_vector_groups()` |
| Time series read | `read_time_series_group()` | `quiver_database_read_time_series_group()` | `read_time_series_group()` | `readTimeSeriesGroup()` | `read_time_series_group()` | `read_time_series_group()` |
| Time series update | `update_time_series_group()` | `quiver_database_update_time_series_group()` | `update_time_series_group!()` | `updateTimeSeriesGroup()` | `update_time_series_group()` | `update_time_series_group()` |
| Query | `query_string()` | `quiver_database_query_string()` | `query_string()` | `queryString()` | `query_string()` | `query_string()` |
| Relations | `update_scalar_relation()` | `quiver_database_update_scalar_relation()` | `update_scalar_relation!()` | `updateScalarRelation()` | `update_scalar_relation()` | `update_scalar_relation()` |
| CSV | `export_csv()` | `quiver_database_export_csv()` | `export_csv()` | `exportCSV()` | `export_csv()` | `export_csv()` |
| Describe | `describe()` | `quiver_database_describe()` | `describe()` | `describe()` | `describe()` | `describe()` |

The transformation rules are mechanical. Given any C++ method name, you can derive the equivalent in any layer without consulting a lookup table.

### Binding-Only Convenience Methods

Julia, Dart, and Lua provide additional convenience methods that compose core operations. These have no direct C++ or C API counterpart.

**DateTime wrappers (Julia and Dart only):**

| Julia | Dart | Wraps |
|-------|------|-------|
| `read_scalar_date_time_by_id` | `readScalarDateTimeById` | string read + date parsing |
| `read_vector_date_time_by_id` | `readVectorDateTimesById` | string vector read + date parsing |
| `read_set_date_time_by_id` | `readSetDateTimesById` | string set read + date parsing |
| `query_date_time` | `queryDateTime` | string query + date parsing |

**Composite read helpers (Julia, Dart, and Lua):**

| Julia | Dart | Lua | Wraps |
|-------|------|-----|-------|
| `read_all_scalars_by_id` | `readAllScalarsById` | `read_all_scalars_by_id` | `list_scalar_attributes` + typed reads |
| `read_all_vectors_by_id` | `readAllVectorsById` | `read_all_vectors_by_id` | `list_vector_groups` + typed reads |
| `read_all_sets_by_id` | `readAllSetsById` | `read_all_sets_by_id` | `list_set_groups` + typed reads |

**Transaction block wrappers (Julia, Dart, and Lua):**

| Julia | Dart | Lua | Wraps |
|-------|------|-----|-------|
| `transaction(db) do db...end` | `db.transaction((db) {...})` | `db:transaction(function(db)...end)` | begin + fn + commit/rollback |

**Multi-column group readers (Julia and Dart only):**

| Julia | Dart | Wraps |
|-------|------|-------|
| `read_vector_group_by_id` | `readVectorGroupById` | metadata + per-column vector reads |
| `read_set_group_by_id` | `readSetGroupById` | metadata + per-column set reads |

## Bindings

### Regenerating FFI Bindings
When C API changes, regenerate:
```bash
bindings/julia/generator/generator.bat   # Julia
bindings/dart/generator/generator.bat    # Dart
bindings/python/generator/generator.bat  # Python
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

### Python Notes
- Uses CFFI ABI-mode (no compiler required at install time)
- `_loader.py` pre-loads `libquiver.dll` on Windows for dependency chain resolution
- `_c_api.py` contains hand-written CFFI cdef declarations (generator output in `_declarations.py` for reference)
- Properties are regular methods, not `@property` (per design decision)
- test.bat prepends `build/bin/` to PATH for DLL discovery
