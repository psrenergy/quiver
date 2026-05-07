# Project: Quiver

SQLite wrapper library with C++ core, C API for FFI, and language bindings (Julia, Dart).

## Architecture

```
include/quiver/           # C++ public headers
  database.h              # Database class - main API
  attribute_metadata.h    # ScalarMetadata, GroupMetadata types
  options.h               # DatabaseOptions, CSVOptions types and factories
  element.h               # Element builder for create operations
  lua_runner.h            # Lua scripting support
include/quiver/binary/      # Binary subsystem headers (binary file I/O)
  binary_file.h               # BinaryFile class (Pimpl) - open_file, read, write, get_metadata
  csv_converter.h              # CSVConverter class - bin_to_csv, csv_to_bin
  iteration.h                 # first_dimensions, next_dimensions free functions
  binary_metadata.h         # BinaryMetadata struct - dimensions, labels, serialization
  dimension.h             # Dimension struct (name, size, optional TimeProperties)
  time_properties.h       # TimeFrequency enum, TimeProperties struct
  time_constants.h        # Time dimension size constraints
include/quiver/expression/  # Expression subsystem headers (lazy expressions on .qvr files)
  expression.h                # Expression value type, + - * / operator overloads, save engine
  node.h                      # Node base + FileNode/ScalarNode/BinaryOpNode + BinaryOp enum
include/quiver/c/         # C API headers (for FFI)
  options.h               # All option types and defaults: LogLevel, DatabaseOptions, CSVOptions
  database.h
  element.h
  lua_runner.h
include/quiver/c/binary/    # Binary C API headers
  binary_file.h               # quiver_binary_file_t opaque handle, open/close/read/write
  csv_converter.h              # bin_to_csv, csv_to_bin functions
  binary_metadata.h         # quiver_binary_metadata_t + flat structs (quiver_dimension_t, quiver_time_properties_t)
src/                      # C++ implementation
  database_csv_export.cpp # CSV export implementation
  database_csv_import.cpp # CSV import implementation
  cli/main.cpp            # quiver_cli CLI entry point
  utils/string.h          # String utilities: new_c_str, trim
src/binary/                 # Binary C++ implementation
  binary_file.cpp             # BinaryFile class (Pimpl impl)
  csv_converter.cpp            # CSVConverter implementation
  iteration.cpp               # first_dimensions/next_dimensions impls + dimension_sizes_at_values helper
  binary_metadata.cpp       # BinaryMetadata factories, serialization, validation
  time_properties.cpp     # TimeFrequency string conversion
src/expression/             # Expression C++ implementation
  expression.cpp              # Expression class, operator overloads, save engine
  nodes.cpp                   # FileNode/ScalarNode/BinaryOpNode + validation/broadcast helpers
src/c/                    # C API implementation
  internal.h              # Shared structs (quiver_database, quiver_element, quiver_binary_file, quiver_binary_metadata), QUIVER_REQUIRE macro
  database_helpers.h      # Marshaling templates, strdup_safe, metadata converters
  options.cpp             # Option defaults: quiver_database_options_default, quiver_csv_options_default
  database.cpp            # Lifecycle: open, close, factory methods, describe
  database_options.h      # Option converters: convert_database_options, convert_csv_options
  database_csv_export.cpp # CSV export function
  database_csv_import.cpp # CSV import function
  database_create.cpp     # Element CRUD: create, update, delete
  database_read.cpp       # All read operations + co-located free functions
  database_update.cpp     # All scalar/vector/set update operations
  database_metadata.cpp   # Metadata get/list + co-located free functions
  database_query.cpp      # Query operations (plain and parameterized)
  database_time_series.cpp # Time series operations + co-located free functions
  database_transaction.cpp # Transaction control (begin, commit, rollback, in_transaction)
src/c/binary/               # Binary C API implementation
  binary_file.cpp             # BinaryFile lifecycle, read/write, getters
  csv_converter.cpp            # CSV conversion wrappers
  binary_metadata.cpp       # Metadata lifecycle, builders, getters, serialization
bindings/julia/           # Julia bindings (Quiver.jl)
  src/binary/               # Binary Julia wrappers (Metadata, File, csv_converter)
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

### CLI Executable
```bash
./build/bin/quiver_cli.exe        # CLI entry point
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
- `test_database_csv_export.cpp` - CSV export operations (scalar, group, options, formatting)
- `test_database_csv_import.cpp` - CSV import operations (round-trip, enum resolution, delimiters, error handling)
- `test_database_read.cpp` - read scalar/vector/set operations
- `test_database_update.cpp` - update scalar/vector/set operations
- `test_database_delete.cpp` - delete element operations
- `test_database_query.cpp` - parameterized and non-parameterized query operations
- `test_database_time_series.cpp` - time series read/update/metadata operations
- `test_database_transaction.cpp` - explicit transaction control (begin/commit/rollback)

C API tests follow same pattern with `test_c_api_database_*.cpp` prefix:
- `test_c_api_database_csv_export.cpp` - CSV export: scalar/group, RFC 4180, enum labels, date formatting, options
- `test_c_api_database_csv_import.cpp` - CSV import: round-trip, semicolons, FK resolution, enum, trailing columns
- `test_c_api_database_metadata.cpp` - Metadata get/list operations (vector, set, time series, scalar)

Binary C API tests with `test_c_api_binary_*.cpp` prefix:
- `test_c_api_binary_file.cpp` - BinaryFile open/close, write/read round-trip, getters, allow_nulls
- `test_c_api_csv_converter.cpp` - bin_to_csv, csv_to_bin, round-trip verification
- `test_c_api_binary_metadata.cpp` - Metadata lifecycle, builders/getters, dimensions, TOML round-trip, from_element

Julia binary tests:
- `test_binary_file.jl` - BinaryFile open/close, write/read round-trip, getters, allow_nulls
- `test_csv_converter.jl` - bin_to_csv, csv_to_bin, round-trip verification
- `test_binary_metadata.jl` - Metadata builder, getters, TOML round-trip, from_element

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

Binary subsystem: `BinaryFile` and `CSVConverter` use Pimpl (hide file I/O dependencies). `BinaryMetadata`, `Dimension`, `TimeProperties` are plain value types.

Expression subsystem: `Expression` is a plain value type wrapping `shared_ptr<Node>` — no Pimpl. `Node` is an abstract base with virtual `metadata()` / `compute_row()`; concrete subclasses `FileNode`, `ScalarNode`, `BinaryOpNode` are exposed via `QUIVER_API` and use Rule of Zero. Polymorphism is justified by the recursive tree shape (`BinaryOpNode` owns child `shared_ptr<Node>` operands).

Classes with no private dependencies (`Element`, `Row`, `Migration`, `Migrations`, `GroupMetadata`, `ScalarMetadata`, `CSVOptions`, `BinaryMetadata`, `Dimension`, `TimeProperties`, `Expression`) are plain value types — direct members, no Pimpl, Rule of Zero (compiler-generated copy/move/destructor).

### Transactions
Public API exposes explicit transaction control:
```cpp
db.begin_transaction();
// multiple write operations...
db.commit();   // or db.rollback();
bool active = db.in_transaction();
```

Internally, `Impl::TransactionGuard` is nest-aware RAII: if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`), the guard becomes a no-op. This allows write methods (`create_element`, etc.) to work both standalone and inside explicit transactions without double-beginning.

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
- **Examples:** `create_element`, `read_vector_floats_by_id`, `get_scalar_metadata`, `list_time_series_groups`

### Error Handling
All `throw std::runtime_error(...)` in the C++ layer use exactly 3 message patterns:

**Pattern 1 -- Precondition failure:** `"Cannot {operation}: {reason}"`
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error("Cannot update_element: element must have at least one scalar or array attribute");
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
Exceptions: `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`, `quiver_database_options_default`, `quiver_csv_options_default` (utility functions with direct return).

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

// Single string cleanup (strings returned by query/read-by-id/element operations)
quiver_database_free_string(char*)

// Binary entity free functions
// Binary metadata lifecycle
quiver_binary_metadata_create/free
quiver_binary_metadata_free_string(char*)
quiver_binary_metadata_free_string_array(char**, size_t)
quiver_binary_metadata_free_dimension(quiver_dimension_t*)

// Binary file lifecycle
quiver_binary_file_open_read/open_write/close
quiver_binary_file_free_string(char*)
quiver_binary_file_free_float_array(double*)
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
Always null-terminate, use `quiver::string::new_c_str()` from `src/utils/string.h`:
```cpp
// src/utils/string.h — namespace quiver::string
inline char* new_c_str(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}
```
C API files call `quiver::string::new_c_str()` with full qualification.

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
- Query: `query_string/integer/float(sql, params = {})` - parameterized SQL with positional `?` placeholders
- Schema inspection: `describe()` - prints schema info to stdout
- CSV: `export_csv()`, `import_csv()` -- CSV export/import with optional enum/date formatting via `CSVOptions`

### Element Class
Builder for element creation with fluent API:
```cpp
Element().set("label", "Item 1").set("value", 42).set("tags", {"a", "b"})
```

> **Python:** Element is internal-only (not part of the public API). Python users pass `**kwargs` to `create_element`/`update_element` instead.

### Binary Subsystem
Standalone binary file I/O layer for `.qvr` files with `.toml` metadata sidecars.

- `BinaryFile` class (Pimpl): `open_file(path, mode, metadata?)`, `read(dims)`, `write(dims, data)`, `get_metadata()`, `get_file_path()`
- `CSVConverter` class (Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)`
- `BinaryMetadata` struct: `dimensions`, `initial_datetime`, `unit`, `labels`, `version`, `number_of_time_dimensions`
  - Factories: `from_toml()`, `from_element()`
  - Serialization: `to_toml()`
  - Builders: `add_dimension()`, `add_time_dimension()`
  - Validation: `validate()`, `validate_time_dimension_metadata()`, `validate_time_dimension_sizes()`
- `Dimension` struct: `name`, `size`, optional `TimeProperties`
- `TimeProperties` struct: `frequency`, `initial_value`, `parent_dimension_index`
- `TimeFrequency` enum: `Yearly`, `Monthly`, `Weekly`, `Daily`, `Hourly`

#### Iteration Helpers
Free functions in `quiver::` for traversing the dimension space of a `BinaryMetadata` (declared in `quiver/binary/iteration.h`):

- `first_dimensions(meta)` — initial position; returns `initial_value` for time dims, `1` for non-time dims
- `next_dimensions(meta, current)` — next position via right-to-left cascade; returns `nullopt` at end. Uses an internal `dimension_sizes_at_values` helper to handle variable-length time dims (Feb=28/29, Jan=31, etc.)

Used by `Expression::save()` and `CSVConverter::{bin_to_csv, csv_to_bin}` — single source of truth for `.qvr` traversal.

#### Write Registry
In-process path registry prevents reading files that are currently open for writing. A `static std::unordered_set<std::string>` in `binary.cpp` tracks canonical paths of files open in write mode. Opening a reader or a second writer on a registered path throws `std::runtime_error`. The registry entry is removed in the `Binary` destructor (before flush). Move semantics work correctly: moved-from objects have null `impl_` and skip unregistration. Not thread-safe or multi-process safe.

#### Binary Performance Bottlenecks
Profiled with 480×500×31 dimensions (~7.3M read/write calls). Main hot-path costs:

1. **`unordered_map<string, int64_t>` dims parameter (~40% of total time):** Every read/write constructs a hash map with string keys. Hashing, heap allocation for strings/buckets, and `find()` lookups dominate. Indexed `vector<int64_t>` overloads were prototyped (D-13/D-14) and dropped (`b9c9ad0`) in favor of API simplicity — the map-based form is the single supported entry point. Hot-path consumers (e.g., `FileNode::compute_row`, `Expression::save`) cache the `unordered_map` across calls to amortize the allocation. Do not reintroduce indexed overloads without revisiting that decision.
2. **`validate_dimension_values` (~19% of total time):** Called on every read/write. Checks dimension count, name existence, bounds, and time-dimension consistency (date arithmetic via `add_offset_from_int`/`datetime_to_int`). Could be made opt-in or skippable when callers guarantee correct values (e.g., when iterating via `next_dimensions()`).
3. **`vector<double>` allocation per read (~3%):** Each `read()` allocates a new vector. A `read_into(buffer, dims)` overload writing into caller-provided storage would eliminate this.

### Expression Subsystem
Lazy expressions over `.qvr` binary files. Build a DAG using `+ - * /` operator overloads, materialize via `save()`.

```cpp
auto a = BinaryFile::open_file("a", 'r');
auto b = BinaryFile::open_file("b", 'r');
Expression result = (a + b) * 2.0;
result.save("output");  // writes output.qvr + output.toml
```

- `Expression` value type (header `quiver/expression/expression.h`):
  - Constructors: `Expression(const BinaryFile&)` (implicit, enables `bf_a + bf_b`), `Expression(shared_ptr<Node>)`
  - Accessors: `metadata()`, `node()`
  - Materialize: `save(path)` — iterates via `first_dimensions`/`next_dimensions`, calls `compute_row()` per cell, writes to a new `.qvr`. Throws if `path` collides (after `weakly_canonical`) with any input file in the DAG.
- Operator overloads (12 total): `+ - * /` × {expr+expr, expr+double, double+expr}.
- `Node` hierarchy (header `quiver/expression/node.h`):
  - `Node` (abstract): `metadata()`, `compute_row(dims, out)`
  - `FileNode`: lazy reads from a `.qvr`. Caches an open `BinaryFile` and a reusable `unordered_map` across calls (mutable members; not thread-safe per instance).
  - `ScalarNode`: broadcasts a constant across the operand's label space.
  - `BinaryOpNode`: combines two operands with `BinaryOp::{Add,Subtract,Multiply,Divide}`. Constructor pre-computes broadcast metadata (`build_broadcast_metadata`), reusable input/output buffers, and `lhs_to_out_`/`rhs_to_out_` index translation tables.
- Validation is **eager** at `BinaryOpNode` construction (units must match, dim sizes broadcast-compatible n×n / 1×n / n×1, time-dim properties match by name not position, label sizes broadcast-compatible, initial datetimes match). Computation is **lazy**: no I/O until `save()`.
- `BinaryOp` enum lives in `quiver/expression/node.h` alongside the node classes.

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
- **C++ to Python:** Same `snake_case` name. Factory methods are `@staticmethod`: `from_schema` -> `Database.from_schema()`. Properties are regular methods (not `@property`). Create/update use `**kwargs`: `create_element("Collection", label="x")`.
- **C++ to Lua:** Same name exactly (1:1 match). Example: `read_scalar_integers` -> `read_scalar_integers`. Lua has no lifecycle methods (open/close) -- database is provided as `db` userdata by LuaRunner.

### Representative Cross-Layer Examples

| Category | C++ | C API | Julia | Dart | Lua |
|----------|-----|-------|-------|------|-----|
| Factory | `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | N/A |
| Open existing | `Database(path, options)` | `quiver_database_open()` | `open()` | `Database.open()` | N/A |
| Transaction | `begin_transaction()` | `quiver_database_begin_transaction()` | `begin_transaction!()` | `beginTransaction()` | `begin_transaction()` |
| Transaction | `commit()` | `quiver_database_commit()` | `commit!()` | `commit()` | `commit()` |
| Transaction | `rollback()` | `quiver_database_rollback()` | `rollback!()` | `rollback()` | `rollback()` |
| Transaction | `in_transaction()` | `quiver_database_in_transaction()` | `in_transaction()` | `inTransaction()` | `in_transaction()` |
| Create | `create_element()` | `quiver_database_create_element()` | `create_element!()` | `createElement()` | `create_element()` |

> **Note:** Python's `create_element` and `update_element` accept `**kwargs` instead of a positional Element argument: `db.create_element("Collection", label="x", value=42)`.
| Read scalar | `read_scalar_integers()` | `quiver_database_read_scalar_integers()` | `read_scalar_integers()` | `readScalarIntegers()` | `read_scalar_integers()` |
| Read by Id | `read_scalar_integer_by_id()` | `quiver_database_read_scalar_integer_by_id()` | `read_scalar_integer_by_id()` | `readScalarIntegerById()` | N/A (use composites) |
| Delete | `delete_element()` | `quiver_database_delete_element()` | `delete_element!()` | `deleteElement()` | `delete_element()` |
| Metadata | `get_scalar_metadata()` | `quiver_database_get_scalar_metadata()` | `get_scalar_metadata()` | `getScalarMetadata()` | `get_scalar_metadata()` |
| List groups | `list_vector_groups()` | `quiver_database_list_vector_groups()` | `list_vector_groups()` | `listVectorGroups()` | `list_vector_groups()` |
| Time series read | `read_time_series_group()` | `quiver_database_read_time_series_group()` | `read_time_series_group()` | `readTimeSeriesGroup()` | `read_time_series_group()` |
| Time series update | `update_time_series_group()` | `quiver_database_update_time_series_group()` | `update_time_series_group!()` | `updateTimeSeriesGroup()` | `update_time_series_group()` |
| Query | `query_string()` | `quiver_database_query_string()` | `query_string()` | `queryString()` | `query_string()` |
| CSV | `export_csv()` | `quiver_database_export_csv()` | `export_csv()` | `exportCSV()` | `export_csv()` |
| Describe | `describe()` | `quiver_database_describe()` | `describe()` | `describe()` | `describe()` |

**Binary cross-layer examples:**

| Category | C++ | C API | Julia |
|----------|-----|-------|-------|
| Open read | `BinaryFile::open_file(path, 'r')` | `quiver_binary_file_open_read()` | `open_file(path; mode=:read)` |
| Open write | `BinaryFile::open_file(path, 'w', md)` | `quiver_binary_file_open_write()` | `open_file(path; mode=:write, metadata=md)` |
| Close | (destructor) | `quiver_binary_file_close()` | `close!(file)` |
| Read | `binary_file.read(dims)` | `quiver_binary_file_read()` | `read(file; dims...)` |
| Write | `binary_file.write(dims, data)` | `quiver_binary_file_write()` | `write!(file; data=data, dims...)` |
| Get metadata | `binary_file.get_metadata()` | `quiver_binary_file_get_metadata()` | `get_metadata(file)` |
| Get file path | `binary_file.get_file_path()` | `quiver_binary_file_get_file_path()` | `get_file_path(file)` |
| Bin to CSV | `CSVConverter::bin_to_csv()` | `quiver_csv_converter_bin_to_csv()` | `bin_to_csv()` |
| CSV to bin | `CSVConverter::csv_to_bin()` | `quiver_csv_converter_csv_to_bin()` | `csv_to_bin()` |
| Metadata builder | `BinaryMetadata{}` | `quiver_binary_metadata_create()` | `Metadata(; kwargs...)` |
| Metadata from TOML | `BinaryMetadata::from_toml()` | `quiver_binary_metadata_from_toml()` | `from_toml()` |
| Metadata from Element | `BinaryMetadata::from_element()` | `quiver_binary_metadata_from_element()` | `from_element()` |

The transformation rules are mechanical. Given any C++ method name, you can derive the equivalent in any layer without consulting a lookup table.

### Binding-Only Convenience Methods

Julia, Dart, Lua, and JS provide additional convenience methods that compose core operations. These have no direct C++ or C API counterpart.

**DateTime wrappers (Julia and Dart only):**

|             Julia             |           Dart            |               Wraps               |
| ----------------------------- | ------------------------- | --------------------------------- |
| `read_scalar_date_time_by_id` | `readScalarDateTimeById`  | string read + date parsing        |
| `read_vector_date_time_by_id` | `readVectorDateTimesById` | string vector read + date parsing |
| `read_set_date_time_by_id`    | `readSetDateTimesById`    | string set read + date parsing    |
| `query_date_time`             | `queryDateTime`           | string query + date parsing       |

**Composite read helpers (Julia, Dart, Lua, and JS):**

|        Julia         |       Dart        |         Lua          |        JS         |                 Wraps                  |
| -------------------- | ----------------- | -------------------- | ----------------- | -------------------------------------- |
| `read_scalars_by_id` | `readScalarsById` | `read_scalars_by_id` | `readScalarsById` | `list_scalar_attributes` + typed reads |
| `read_vectors_by_id` | `readVectorsById` | `read_vectors_by_id` | `readVectorsById` | `list_vector_groups` + typed reads     |
| `read_sets_by_id`    | `readSetsById`    | `read_sets_by_id`    | `readSetsById`    | `list_set_groups` + typed reads        |

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
- `_c_api.py` contains hand-written CFFI cdef declarations; `generator/generator.py` prints current declarations from the headers to stdout as a diff aid when hand-updating it
- Properties are regular methods, not `@property` (per design decision)
- test.bat prepends `build/bin/` to PATH for DLL discovery
- `create_element` and `update_element` accept `**kwargs` (not an Element builder). Element class is internal.
- Dict unpacking supported: `db.create_element("Collection", **my_dict)`

<!-- GSD:project-start source:PROJECT.md -->
## Project

**Quiver**

Quiver is a structured-data library wrapping SQLite, plus a standalone binary-file (`.qvr`) toolkit for dense numeric tensors with TOML metadata sidecars. It ships as a C++20 core, a stable C ABI, and homogeneous bindings for Julia, Dart, Python, JS/Deno, and embedded Lua — built so logic lives in C++ and bindings stay thin.

**Core Value:** A single C++ implementation reaches every supported language with identical semantics: every public method on `Database`, `BinaryFile`, `BinaryMetadata`, etc. is mechanically translatable to each binding via the cross-layer naming rules in CLAUDE.md.

### Constraints

- **Language:** C++20, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` — already set in `CMakeLists.txt`. No C++23 features.
- **ABI stability:** Every new feature must reach the C API and bindings; CRTP / template-leaking surfaces are forbidden in public headers.
- **Naming:** Expression symbols live in the top-level `quiver::` namespace (no sub-namespace, same as `BinaryFile` and friends); headers are organized under `include/quiver/expression/`. C API and bindings will use `quiver_expression_*` (C) and `Expression`/`expression` per binding's casing convention when added.
- **Errors:** Use the three CLAUDE.md error patterns (`Cannot {operation}: {reason}` / `{Entity} not found: {identifier}` / `Failed to {operation}: {reason}`); messages live in C++ core, bindings surface them unchanged.
- **Pimpl rule:** Pimpl only when hiding private dependencies. `Expression` is a value type wrapping `shared_ptr<Node>` — no Pimpl. `Node` and its subclasses use Rule of Zero where possible.
- **No new third-party deps** for v1 — the design only uses `BinaryFile`, `BinaryMetadata`, the standard library.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

## Languages
- **C++20** — Core library (`src/*.cpp`, `src/binary/*.cpp`). Configured via `CMakeLists.txt` (`set(CMAKE_CXX_STANDARD 20)`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`). Style mandated by `.clang-format` (`Standard: c++20`).
- **C (C99 ABI)** — Public FFI surface (`include/quiver/c/*.h`, `src/c/*.cpp`). Project declares both languages in `CMakeLists.txt` (`LANGUAGES CXX C`). Implementations are C++ internally; headers use `extern "C"` blocks with `stdbool.h`/`stdint.h` for ABI portability.
- **Julia 1.11+** — Binding at `bindings/julia/`. `Project.toml` declares `julia = "1.11"`, `CEnum = "0.5"`, `Dates = "1"`. Tested on 1.11 and 1.12 per `.github/workflows/ci.yml` (`julia-coverage` matrix). Current `Manifest.toml` generated by `julia_version = "1.12.5"`.
- **Dart SDK ^3.10.0** — Binding at `bindings/dart/`. `pubspec.yaml` specifies `sdk: ^3.10.0`. CI pins `3.10.7` (`.github/workflows/ci.yml`).
- **Python >=3.13** — Binding at `bindings/python/`. `pyproject.toml` declares `requires-python = ">=3.13"`. `cibuildwheel` builds `cp313-win_amd64` / `cp313-manylinux_x86_64`.
- **TypeScript on Deno 2.7.x** — Binding at `bindings/js/`. `deno.json` (name `@psrenergy/quiver`, version 0.1.0). CI pins `deno-version: "2.7.12"`; publish workflow uses `v2.7.x`. Recently migrated from Node.js + koffi to Deno FFI (see `bindings/js/MIGRATION.md`).
- **Lua 5.4** — Embedded scripting layer exposed through `LuaRunner` (`src/lua_runner.cpp`, `include/quiver/lua_runner.h`). No standalone binding package; Lua runs in-process via `sol2`.
## Runtime
- **CMake >=3.26.0** required for configuration (`cmake_minimum_required(VERSION 3.26.0)` in `CMakeLists.txt`). `CMakePresets.json` declares a lower minimum of 3.21 for preset schema, but actual configure requires 3.26.
- **Generators supported:** Visual Studio 17 2022 (Windows preset), Unix Makefiles (Linux preset), Ninja (default in `scripts/build-all.bat` via `-G Ninja`), Xcode (Dart build hook on macOS).
- **C++ toolchain:** MSVC (with `/W4 /permissive- /Zc:__cplusplus /utf-8 /bigobj` for `lua_runner.cpp`) on Windows; GCC/Clang with `-Wall -Wextra -Wpedantic` elsewhere. MinGW path applies `-static-libgcc -static-libstdc++` and links `winpthread` statically (`cmake/CompilerOptions.cmake`).
- **Symbol visibility:** default-hidden across C and C++ (`CMAKE_C_VISIBILITY_PRESET hidden`, `CMAKE_CXX_VISIBILITY_PRESET hidden`, `CMAKE_VISIBILITY_INLINES_HIDDEN ON` in `cmake/Platform.cmake`). Explicit export annotations via `QUIVER_API`/`QUIVER_C_API` macros.
- **RPATH:** Linux `$ORIGIN/../lib`, macOS `@executable_path/../lib`, `CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE` (`cmake/Platform.cmake`). For Python wheels on Linux, `libquiver_c.so` RPATH patched to `$ORIGIN` so it finds `libquiver.so` beside it (publish workflow uses `patchelf`, CMake sets `INSTALL_RPATH "$ORIGIN"` when `SKBUILD` defined).
- **clang-format:** 21 (custom target `format` tries `clang-format-21` first, then `clang-format`; CI installs LLVM 21 via `apt.llvm.org/llvm.sh`).
- **clang-tidy:** Driven by `scripts/tidy.bat` to avoid Windows command-line-length limits; custom target `tidy` in `CMakeLists.txt`.
- **CMake FetchContent** — All C/C++ dependencies vendored at configure time (no system packages). See `cmake/Dependencies.cmake`.
- **Julia Pkg** — `Project.toml` + `Manifest.toml` in `bindings/julia/`. Separate sub-projects for `generator/` (Clang.jl), `format/` (JuliaFormatter), `revise/` (Revise.jl), `binary_builder/` (BinaryBuilder.jl).
- **pub.dev (Dart)** — `pubspec.yaml` in `bindings/dart/`. Lockfile `pubspec.lock` present but `.gitignore` marks it ignored.
- **uv** — Python package manager, `bindings/python/uv.lock` committed. `Makefile` uses `uv sync`, `uv run`, `uv build`, `uv publish`. CI installs via `astral-sh/setup-uv@v7`.
- **JSR** — Deno registry for JS publishing (`.github/workflows/publish.yml` `publish-jsr` job runs `npx jsr publish`). `deno.json` declares `"nodeModulesDir": "none"`.
- **cibuildwheel 3.4.1** — Builds Python wheels via `pypa/cibuildwheel@v3.4.1` on ubuntu-latest and windows-latest (`.github/workflows/publish.yml`, `.github/workflows/build-wheels.yml`).
- **scikit-build-core >=0.10** — Python wheel build backend (`pyproject.toml`). Forces `QUIVER_BUILD_C_API=ON` and disables tests when `SKBUILD` is defined (`CMakeLists.txt` lines 22-25).
- `bindings/python/uv.lock` — present, committed.
- `bindings/js/deno.lock` — present, committed.
- `bindings/julia/Manifest.toml` — present, committed (despite `Manifest.toml` in `.gitignore`, this one is tracked).
- `bindings/dart/pubspec.lock` — present, `.gitignore` excludes it.
## Frameworks
- **SQLite 3.50.2** — Primary data store. Pulled via FetchContent from `https://github.com/sjinks/sqlite3-cmake.git`, tag `v3.50.2`. Linked as `SQLite::SQLite3` (PUBLIC dep of `quiver`).
- **spdlog 1.17.0** — Logging. `gabime/spdlog.git`, tag `v1.17.0`. Used with `basic_file_sink_mt` + `stderr_color_sink_mt` for per-database loggers.
- **sol2 3.5.0** — C++/Lua binding layer. `ThePhD/sol2.git`, tag `v3.5.0`. Compile definitions `SOL_SAFE_NUMERICS=1`, `SOL_SAFE_FUNCTION=1`.
- **Lua 5.4.8** — Embedded scripting engine. Pulled via lua-cmake wrapper from `https://gitlab.com/codelibre/lua/lua-cmake.git`, tag `lua-cmake/v5.4.8.0`. Configured with `LUA_BUILD_INTERPRETER OFF`, `LUA_BUILD_COMPILER OFF`, `LUA_TESTS None`.
- **toml++ 3.4.0** — TOML parser for `.qvr` metadata sidecars. `marzer/tomlplusplus.git`, tag `v3.4.0`.
- **rapidcsv 8.92** — Header-only CSV reader/writer. `d99kris/rapidcsv.git`, tag `v8.92`. Used by `src/database_csv_export.cpp`, `src/database_csv_import.cpp`.
- **argparse 3.2** — CLI argument parser (header-only). `p-ranav/argparse.git`, tag `v3.2`. Used only by `quiver_cli` (`src/cli/main.cpp`).
- **GoogleTest 1.17.0** — `google/googletest.git`, tag `v1.17.0`. Pulled only when `QUIVER_BUILD_TESTS=ON`. Configured with `gtest_force_shared_crt ON` for MSVC. Test discovery via `gtest_discover_tests` in `tests/CMakeLists.txt`.
- **Catch2** — Not used (not present in dependencies).
- **CMake** — Build system. `CMakeLists.txt` + `CMakePresets.json` + modular files under `cmake/` (`Dependencies.cmake`, `Platform.cmake`, `CompilerOptions.cmake`, `quiverConfig.cmake.in`).
- **clang-format 21** — Code formatting (`.clang-format`).
- **clang-tidy** — Static analysis (`.clang-tidy`).
- **cppcheck** — Pre-commit hook (`.pre-commit-config.yaml`).
- **cmake-format 0.6.13** — CMake file formatting (`.pre-commit-config.yaml`).
- **pre-commit 6.0.0** — Git hook framework (`.pre-commit-config.yaml`).
## Key Dependencies
- `SQLite::SQLite3` (3.50.2) — PUBLIC dependency. Database engine; all read/write paths call `sqlite3_*` functions (`src/database.cpp`, `src/schema.cpp`, `src/database_create.cpp`).
- `spdlog::spdlog` (1.17.0) — PRIVATE. Structured logging; per-database logger instance with both console and file sinks.
- `lua_library` + `sol2` — PRIVATE. Lua 5.4.8 interpreter plus C++ binding DSL (`src/lua_runner.cpp`).
- `tomlplusplus::tomlplusplus` (3.4.0) — PRIVATE. TOML sidecar metadata (`src/binary/binary_metadata.cpp`).
- `rapidcsv` (8.92) — PRIVATE. CSV import/export (`src/database_csv_export.cpp`, `src/database_csv_import.cpp`).
- `${CMAKE_DL_LIBS}` — PRIVATE. Dynamic loader (libdl on Linux, nothing needed on Windows/macOS).
- Links PRIVATE against `quiver` (core) and `quiver_compiler_options`. No independent dependencies — wraps the C++ layer in `extern "C"` functions with opaque-handle + out-parameter idiom.
- Links `quiver`, `quiver_compiler_options`, and `argparse`. Reads Lua scripts, strips UTF-8 BOM, and dispatches through `LuaRunner`.
- `CEnum` 0.5 — C enum wrapping.
- `Dates` (stdlib) — DateTime parsing for date helpers.
- Generator sub-project: `Clang` 0.19.2 (pinned) + `Libdl` for ABI introspection (`bindings/julia/generator/Project.toml`).
- Format sub-project: `JuliaFormatter` 2.x.
- Revise sub-project: `Revise` 3.x.
- Binary builder sub-project: `BinaryBuilder`.
- `ffi` ^2.1.0 — Typed native memory / string conversion.
- `code_assets` ^1.0.0 — Native asset API.
- `hooks` ^1.0.0 — Build hook framework.
- `native_toolchain_cmake` ^0.2.2 — CMake integration for Dart build hooks (`bindings/dart/hook/build.dart` invokes CMake).
- `logging` ^1.3.0 — Hook logger.
- Dev: `ffigen` ^11.0.0 (binding generator), `test` ^1.24.0, `coverage` ^1.7.0, `lints` ^3.0.0, `path` ^1.9.1.
- `cffi >=2.0.0` — FFI-mode ABI binding (no compiler required at install time).
- Build: `scikit-build-core >=0.10` (PEP-517 backend invoking CMake).
- Dev: `pytest >=8.4.1`, `ruff >=0.12.2`, `dotenv >=0.9.9`.
- Runtime: Deno built-in FFI (`Deno.dlopen`). No npm dependencies required at runtime; `deno.json` sets `"nodeModulesDir": "none"`.
- Standard lib: `jsr:@std/path@^1.1.4` (imported dynamically inside `src/loader.ts`).
- Dev (pre-existing in `node_modules/`): `@biomejs/biome 2.4.6` (format/lint), `vitest` + helpers (historical, from pre-Deno era). CI uses `deno task test` only.
## Configuration
- No runtime environment variables required by the library itself.
- `LD_LIBRARY_PATH` used by Linux CI to locate `build/lib/libquiver.so` and `build/lib/libquiver_c.so` at test time.
- Windows binding tests prepend `build/bin/` to `PATH` via their respective `test.bat` scripts for DLL discovery.
- `.env` file ignored via `.gitignore` (generic `.env*` lines not present — project does not use runtime env files).
- `QUIVER_BUILD_SHARED` (default `ON`) — SHARED vs STATIC library target (`CMakeLists.txt`, `src/CMakeLists.txt`).
- `QUIVER_BUILD_TESTS` (default `ON`) — Pulls GoogleTest and builds `quiver_tests`, `quiver_c_tests`, `quiver_benchmark`, `quiver_sandbox`.
- `QUIVER_BUILD_C_API` (default `OFF`) — Builds `quiver_c` shared library. Forced `ON` when building Python wheel (`SKBUILD` defined).
- `SKBUILD` — Set automatically by scikit-build-core; forces `QUIVER_BUILD_C_API=ON`, `QUIVER_BUILD_TESTS=OFF`, and installs `quiver` + `quiver_c` into `quiverdb/_libs/` instead of standard system dirs.
- `dev` — Debug, tests ON, C API ON.
- `release` — Release, tests OFF.
- `windows-release` — Visual Studio 17 2022, Release, tests ON, C API ON.
- `linux-release` — Unix Makefiles, Release, tests ON, C API ON.
- `all-bindings` — Release, tests ON, C API ON, plus `QUIVER_BUILD_PYTHON_BINDING=ON` (variable not wired into CMakeLists.txt; vestigial).
- `CMakeLists.txt` (top-level) — Version declared via `project(quiver VERSION x.x.x ...)`.
- `CMakePresets.json` — Preset definitions for IDE/CI configuration.
- `cmake/Dependencies.cmake` — All FetchContent declarations.
- `cmake/Platform.cmake` — OS detection + RPATH + visibility defaults.
- `cmake/CompilerOptions.cmake` — `quiver_compiler_options` INTERFACE target with warnings.
- `cmake/quiverConfig.cmake.in` — find_package() config template.
- `.clang-format` — LLVM base, 4-space indent, 120 columns, LF line endings, C++20 standard.
- `.clang-tidy` — Static analysis rules (not fully inspected).
- `.clangd` — clangd LSP configuration.
- `bindings/dart/analysis_options.yaml` — `package:lints/recommended.yaml`, excludes generated FFI bindings, `page_width: 120`.
- `bindings/python/ruff.toml` — line-length 120, target-version `py313`, isort checks, double quote style.
- `bindings/js/biome.json` — Biome 2.4.6, 2-space indent, 100-char lines, double quotes, semicolons required.
- `bindings/js/deno.json` (`fmt` block) — 2-space indent, 100-char lines.
- `.pre-commit-config.yaml` — pre-commit 6.0.0, clang-format, cppcheck, cmake-format 0.6.13, trailing whitespace, LF line endings, 1MB file size cap.
## Platform Requirements
- **Any:** CMake 3.26+, C++20 compiler, Git.
- **Windows:** MSVC (Visual Studio 17 2022 preset) or MinGW. Build outputs DLLs to `build/bin/`. `quiver_cli.exe`, `quiver_tests.exe`, `quiver_c_tests.exe`, `quiver_benchmark.exe`, `quiver_sandbox.exe` copy required DLLs post-build (`tests/CMakeLists.txt`).
- **Linux:** GCC/Clang. Outputs `.so` to `build/lib/`. CI runs on `ubuntu-latest`.
- **macOS:** Clang (Apple). Outputs `.dylib` to `build/lib/`. CI runs on `macos-latest`.
- **PyPI wheels (`quiverdb`):** `cp313-win_amd64`, `cp313-manylinux_x86_64` (`bindings/python/pyproject.toml` `[tool.cibuildwheel]`).
- **JSR package (`@psrenergy/quiver`):** Bundles `libs/windows-x86_64/` and `libs/linux-x86_64/` native libraries alongside TypeScript sources (`.github/workflows/publish.yml` `publish-jsr` job).
- **Julia package (Quiver.jl):** Distributed separately at `github.com/psrenergy/Quiver.jl` per README; this repo contains the binding source that loads `libquiver_c` from local `build/` path by default (`bindings/julia/src/c_api.jl`).
- **Dart package (`quiverdb`):** Builds native library on the fly via Dart's native-assets hook (`bindings/dart/hook/build.dart`) using `native_toolchain_cmake`.
- **CLI (`quiver_cli`):** Platform-native binary produced per build.
- Windows: `libquiver.dll`, `libquiver_c.dll` (CMake preserves `lib` prefix on Windows DLLs).
- macOS: `libquiver.dylib`, `libquiver_c.dylib`.
- Linux: `libquiver.so`, `libquiver_c.so` (with SONAME `libquiver.so.0` for major version 0).
- `libquiver_c` depends on `libquiver`. All bindings load `libquiver` first (where needed) before `libquiver_c`: Dart (`bindings/dart/lib/src/ffi/library_loader.dart`), JS/Deno (`bindings/js/src/loader.ts`), Python (`bindings/python/src/quiverdb/_loader.py`), Julia (`ccall` against absolute path in `bindings/julia/src/c_api.jl`).
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

## Naming Patterns
### Files
- C++ implementation files split by verb category: `src/database_create.cpp`, `src/database_read.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`, `src/database_metadata.cpp`, `src/database_query.cpp`, `src/database_time_series.cpp`, `src/database_csv_export.cpp`, `src/database_csv_import.cpp`, `src/database_describe.cpp`.
- C API files mirror C++ counterparts under `src/c/`: `src/c/database.cpp`, `src/c/database_create.cpp`, `src/c/database_read.cpp`, etc. Binary subsystem uses `src/c/binary/*`.
- Private implementation helpers use `_impl.h` / `_internal.h` suffixes: `src/database_impl.h`, `src/database_internal.h`, `src/c/internal.h`, `src/c/database_helpers.h`, `src/c/database_options.h`.
- Public headers live under `include/quiver/` (C++) and `include/quiver/c/` (C API), grouped by subsystem (e.g. `include/quiver/binary/`, `include/quiver/c/binary/`).
- Tests mirror source files with `test_` prefix: `tests/test_database_read.cpp`, `tests/test_c_api_database_read.cpp`, `tests/test_c_api_binary_file.cpp`.
- Schemas: `tests/schemas/valid/*.sql`, `tests/schemas/invalid/*.sql`, `tests/schemas/migrations/{version}/{up,down}.sql`.
### C++ Identifiers (enforced by `.clang-tidy`)
- **Classes/structs/enums:** `CamelCase` (`Database`, `BinaryFile`, `ScalarMetadata`, `LogLevel`).
- **Enum constants:** `CamelCase` (`LogLevel::Debug`, `TimeFrequency::Monthly`).
- **Functions/methods/variables/members/parameters:** `lower_case` with underscores.
- **Private members:** trailing underscore (`impl_`, `last_error_`). Public data members (plain value types) have no suffix.
- **Namespaces:** `lower_case` (`quiver`, `quiver::string`, `quiver::test`).
- **Constants / `constexpr` / globals:** `lower_case`.
### Method Naming: `verb_[category_]type[_by_id]`
- **Verbs:** `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`, `begin_transaction`, `commit`, `rollback`, `in_transaction`.
- **Single-element reads end with `_by_id`** (e.g., `read_scalar_integer_by_id`). Plural form returns all elements (`read_scalar_integers`).
- **Typed read variants** use `_integers` / `_floats` / `_strings` suffixes.
- **Group categories** are `scalar`, `vector`, `set`, `time_series`.
- Examples: `create_element`, `read_vector_floats`, `read_vector_floats_by_id`, `update_time_series_group`, `list_set_groups`, `get_scalar_metadata`.
### C API Identifiers
- All C API symbols prefixed with `quiver_`: `quiver_database_create_element`, `quiver_binary_file_open_read`, `quiver_database_options_default`.
- Types: `quiver_{entity}_t` (`quiver_database_t`, `quiver_element_t`, `quiver_binary_file_t`, `quiver_scalar_metadata_t`, `quiver_group_metadata_t`, `quiver_error_t`, `quiver_data_type_t`).
- Enum constants use `UPPER_SNAKE_CASE` with shared prefix: `QUIVER_OK`, `QUIVER_ERROR`, `QUIVER_LOG_DEBUG`, `QUIVER_DATA_TYPE_INTEGER`.
- Output parameters use `out_` prefix (`out_db`, `out_values`, `out_count`, `out_healthy`).
- Free functions mirror allocator: `quiver_database_free_integer_array`, `quiver_database_free_string_array`, `quiver_database_free_scalar_metadata`, `quiver_binary_metadata_free_dimension`.
### Cross-Layer Name Transformation
| From C++ | To C API | To Julia | To Dart | To Python | To Lua | To JS |
|----------|----------|----------|---------|-----------|--------|-------|
| `method_name` | `quiver_database_method_name` | `method_name` (+`!` if mutating) | `methodName` | `method_name` | `method_name` | `methodName` |
| `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | `Database.from_schema()` | N/A | `Database.fromSchema()` |
| `create_element` | `quiver_database_create_element` | `create_element!` | `createElement` | `create_element` (`**kwargs`) | `create_element` | `createElement` |
| `read_scalar_integers` | `quiver_database_read_scalar_integers` | `read_scalar_integers` | `readScalarIntegers` | `read_scalar_integers` | `read_scalar_integers` | `readScalarIntegers` |
- Julia adds `!` only for mutating operations (`create_element!`, `update_element!`, `delete_element!`, `close!`, `begin_transaction!`, `commit!`, `rollback!`).
- Python `create_element` / `update_element` accept `**kwargs` in place of the Element builder: `db.create_element("Collection", label="x", value=42)`.
- Dart factory methods become named constructors: `Database.fromSchema()`, `Database.fromMigrations()`.
- Lua has no lifecycle methods; database is provided as `db` userdata by `LuaRunner`.
### Binding-Only Convenience Methods
- Transaction block wrappers: `transaction(db) do db...end` (Julia), `db.transaction((db) {...})` (Dart), `db:transaction(function(db)...end)` (Lua).
- Composite readers: `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id` (Julia, Dart, Lua, JS).
- DateTime wrappers: `read_scalar_date_time_by_id`, `query_date_time` (Julia, Dart).
- Multi-column group readers: `read_vector_group_by_id`, `read_set_group_by_id` (Julia, Dart).
## Code Style
### Formatting
- Tool: `clang-format-21` (configured in `.clang-format`).
- Based on `LLVM` style with `C++20` standard.
- **Line length:** 120 columns.
- **Indent:** 4 spaces, no tabs.
- **Line ending:** LF (`UseCRLF: false`).
- `BreakBeforeBraces: Attach` (opening brace on same line).
- `PointerAlignment: Left` (`int* x`, not `int *x`).
- `NamespaceIndentation: None`, `FixNamespaceComments: true`.
- `IncludeBlocks: Regroup`, `SortIncludes: true`.
- `BinPackArguments: false`, `BinPackParameters: false` (one-per-line when wrapping).
- `AllowShortFunctionsOnASingleLine: Inline` only.
- Python: `ruff` with `line-length = 120`, `indent-width = 4`, `quote-style = "double"`, `target-version = "py313"`.
- Dart: `analysis_options.yaml` with `page_width: 120`, `trailing_commas: preserve`, `lints/recommended.yaml`.
- CI enforces formatting via `find include src tests -name '*.cpp' -o -name '*.h' | xargs clang-format-21 --dry-run --Werror` in `.github/workflows/ci.yml` (`clang-format` job).
### Linting
- Tool: `clang-tidy` (configured in `.clang-tidy`).
- Enabled checks: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`.
- Disabled checks: `bugprone-easily-swappable-parameters`, `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `bugprone-narrowing-conversions`, `modernize-use-nodiscard`, `modernize-use-ranges`, `modernize-use-designated-initializers`, `performance-inefficient-string-concatenation`, `performance-enum-size`.
- `HeaderFilterRegex: '(include/quiver/|src/)'` restricts tidy to first-party headers.
- Run via `scripts/tidy.bat` or CMake target `tidy`.
- `WarningsAsErrors: ''` (warnings do not fail build); CI does not run clang-tidy.
- Python: `ruff` configured with only isort checks (`select = ["I"]`) in `bindings/python/ruff.toml`.
## Import / Include Organization
#include "database_impl.h"
#include "quiver/migrations.h"
#include "quiver/result.h"
#include "utils/string.h"
#include <atomic>
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <sqlite3.h>
#include <stdexcept>
- Julia: `using` then `include("fixture.jl")` at top of module (`bindings/julia/src/Quiver.jl`).
- Dart: `package:` imports first, then `dart:io`, then local `../` imports (`bindings/dart/test/database_lifecycle_test.dart`).
- Python: `from __future__ import annotations` at top, then stdlib, then project imports (`bindings/python/src/quiverdb/database.py`).
- TypeScript/Deno: `jsr:@std/*` imports then local `../src/*.ts` (`bindings/js/test/database.test.ts`).
## Error Handling
### C++: Three Canonical `std::runtime_error` Patterns
### C API: Binary Return Codes + Thread-Local Error Message
- Exceptions to the `quiver_error_t` rule (direct return): `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`, `quiver_database_options_default`, `quiver_csv_options_default`.
- `QUIVER_REQUIRE(...)` macro in `src/c/internal.h` (lines 39-127) validates up to 9 pointer arguments and emits `"Null argument: <name>"` via stringification.
### Bindings: Never Craft Errors
- Python `check()` (`bindings/python/src/quiverdb/_helpers.py:7-17`) — calls `quiver_get_last_error()` and raises `QuiverError(detail)`.
- Julia `check(...)` in `bindings/julia/src/` raises `DatabaseException` with the C API message.
- Dart throws `DatabaseException` wrapping `quiver_get_last_error()` (`bindings/dart/lib/src/exceptions.dart`).
- JS throws `QuiverError` (`bindings/js/src/errors.ts`).
## Logging
- Framework: `spdlog` (linked privately to `quiver`, see `src/CMakeLists.txt:69`).
- Per-database logger created in `src/database.cpp:43-88` via `create_database_logger()`:
- Levels map directly from `quiver::LogLevel` (`Debug`, `Info`, `Warn`, `Error`, `Off`) via `to_spdlog_level()`.
- Usage: `impl_->logger->debug("Opening database: {}", path);`, `impl_->logger->info(...)`, `impl_->logger->warn(...)`, `impl_->logger->error(...)`.
- 41 call sites across `src/database.cpp`, `src/database_create.cpp`, `src/database_delete.cpp`, `src/database_time_series.cpp`, `src/database_update.cpp`, `src/database_impl.h`.
- No free `spdlog::info(...)` calls — always use the database's owned logger.
## Class / Type Patterns
### Pimpl vs Value Types
- `quiver::Database` (hides `sqlite3*`, spdlog, `Schema`, `TypeValidator`): `include/quiver/database.h:18-28`, `src/database_impl.h:25-30`.
- `quiver::LuaRunner` (hides sol2/lua headers).
- `quiver::BinaryFile`, `quiver::CSVConverter` (hide file I/O).
- `Element`, `Row`, `Result`, `Migration`, `Migrations`, `ScalarMetadata`, `GroupMetadata`, `DatabaseOptions`, `CSVOptions`, `BinaryMetadata`, `Dimension`, `TimeProperties`.
### Move Semantics
### Factory Methods
### Internal RAII Guards
## C API Memory Management
- Uses `new`/`delete` (not `malloc`/`free`) — matches `std::string`/`std::vector` allocators on Windows runtimes.
- Every allocation has a matching `quiver_{entity}_free_*` in the **same translation unit** ("alloc/free co-location").
- String allocation uses `quiver::string::new_c_str()` from `src/utils/string.h` (null-terminated, heap-allocated char array).
- Factory functions use out-parameters (`out_db`, `out_values`, `out_count`) and return `quiver_error_t`.
- Close functions are idempotent; passing `nullptr` returns `QUIVER_OK` (see `quiver_database_close`).
- Helper templates in `src/c/database_helpers.h` avoid duplicating marshaling logic: `read_scalars_impl<T>`, `read_vectors_impl<T>`, `free_vectors_impl<T>`, `copy_strings_to_c`, `convert_scalar_to_c`, `convert_group_to_c`, `to_c_data_type`.
## Schema Conventions
### Required Elements
### Collections
### Group Tables — `{Collection}_{category}_{name}`
- **Vector:** `{Collection}_vector_{name}` with `(id, vector_index)` composite PK.
- **Set:** `{Collection}_set_{name}` with `UNIQUE(id, value)`.
- **Time series:** `{Collection}_time_series_{name}` with dimension column prefixed `date_` (e.g., `date_time`), composite PK `(id, date_*)`.
- **Time series files:** `{Collection}_time_series_files` — singleton table with `data_file TEXT`, `metadata_file TEXT`.
## Comments
- No `@file` / banner headers. Files begin with includes or the conditional guard.
- Section dividers use 76-char equal-sign lines (test files) when splitting related groups of tests:
- Inline comments explain non-obvious mechanics ("Pre-resolve pass: resolve all FK labels before any writes" in `src/database_create.cpp:14`).
- No JSDoc-style annotations in C++. Docstrings used in Python (triple-quoted) and Julia (`"""` above function).
- Avoid redundant comments; the code is expected to be self-documenting (philosophy line in `CLAUDE.md`: "Codebase optimized for human readability, not machine parsing").
## Function Design
- **Parameters:** `const std::string&` for strings, value types by value (`int64_t id`), options structs by const reference with default (`const DatabaseOptions& options = {}`).
- **Return values:** `std::vector<T>` for all-elements reads, `std::optional<T>` for `_by_id` reads (single-element nullable), `int64_t` for generated IDs, `void` for mutations.
- **Designated initializers** used for options structs (C++20): `quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});`
- **Trailing return types** avoided (disabled in `.clang-tidy`).
## Module Design
- Public C++ API exported via `QUIVER_API` macro (declared in `include/quiver/export.h`) and `QUIVER_C_API` in `include/quiver/c/common.h`.
- C API wraps C++ — logic resides in C++ layer; C API is a thin marshaling layer.
- Bindings remain thin — no business logic duplicated in Julia/Dart/Python/JS/Lua wrappers.
- Each binding package mirrors C++ file structure:
## Philosophy (from `CLAUDE.md`)
- **Human-Centric:** Codebase optimized for human readability.
- **Status:** WIP project; breaking changes acceptable; no backwards compatibility required.
- **Clean code over defensive code:** Assume callers obey contracts; avoid excessive null checks.
- **ABI stability:** Pimpl only when hiding private dependencies; plain value types use Rule of Zero.
- **C++20 target:** Use modern language features when they simplify logic (designated initializers, `std::variant`, structured bindings, `std::optional`, concepts).
- **Intelligence:** Logic in C++; bindings stay thin.
- **Error messages:** Defined once in C++/C API; bindings surface them.
- **Ownership:** RAII strictly; ownership of pointers/resources must be explicit and unambiguous.
- **Simple solutions over complex abstractions.** Delete unused code; do not deprecate.
- **Self-updating:** Keep `CLAUDE.md` up to date with codebase changes.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

## Pattern Overview
```
```
- Two subsystems share the three-tier structure: **Database** (SQL-backed) and **Binary** (standalone `.qvr` files with TOML sidecars).
- Logic lives in C++; bindings are thin marshaling layers (see `bindings/julia/src/database_read.jl`, `bindings/dart/lib/src/database_read.dart`, `bindings/python/src/quiverdb/database.py`, `bindings/js/src/read.ts`).
- Error messages are authored in the C++ core (`src/database_create.cpp`, etc.) and surfaced unchanged through all layers via `quiver_get_last_error()`.
- C API is split by concern into small translation units mirroring the test suite, co-locating allocator/deallocator pairs.
## Layers
- Purpose: Business logic, SQL generation, schema validation, binary file I/O, Lua scripting.
- Location: `include/quiver/` (headers), `src/` (implementation).
- Contains: `Database` class (`include/quiver/database.h`), `Element` builder (`include/quiver/element.h`), `BinaryFile` / `CSVConverter` / `BinaryMetadata` (`include/quiver/binary/`), `LuaRunner` (`include/quiver/lua_runner.h`), plus supporting value types (`Value`, `Row`, `Result`, `Schema`, `Migration`).
- Depends on: sqlite3, spdlog, tomlplusplus, sol2+lua, rapidcsv.
- Used by: C API (`src/c/*.cpp`), CLI (`src/cli/main.cpp`), benchmark/sandbox (`tests/benchmark/`, `tests/sandbox/`), C++ tests (`tests/test_*.cpp`).
- Build target: `quiver` shared library (`libquiver.dll` / `libquiver.so` / `libquiver.dylib`).
- Purpose: Stable ABI bridge between C++ and bindings; translates exceptions to error codes, marshals types.
- Location: `include/quiver/c/` (headers), `src/c/` (implementation).
- Contains: `extern "C"` wrappers for every C++ method, prefixed `quiver_database_*` / `quiver_binary_file_*` / `quiver_binary_metadata_*` / `quiver_element_*` / `quiver_lua_runner_*` / `quiver_csv_converter_*`.
- Depends on: C++ core (`quiver` library).
- Used by: Julia (`bindings/julia/src/c_api.jl`), Dart (`bindings/dart/lib/src/ffi/bindings.dart`), Python (`bindings/python/src/quiverdb/_c_api.py`), JS/Deno (`bindings/js/src/loader.ts`).
- Build target: `quiver_c` shared library (`libquiver_c.dll` / etc.), optional — enabled via `-DQUIVER_BUILD_C_API=ON`.
- Purpose: Idiomatic language wrappers calling into the C API via each platform's FFI mechanism.
- Locations:
- Depends on: `libquiver_c` + `libquiver` shared libraries at runtime.
- Homogeneity: Each binding mirrors the C++ API with language-specific naming (see "Cross-Layer Naming Conventions" in `CLAUDE.md`).
## Data Flow
## Key Abstractions
- Purpose: Main API object for SQL-backed collections.
- Implementation: Pimpl (`struct Impl` in `src/database_impl.h:25`) hides `sqlite3*`, `Schema`, `TypeValidator`, `spdlog::logger`.
- Pattern: Factory methods `from_schema()` / `from_migrations()`; non-copyable, movable; `~Database()` closes the SQLite connection.
- Purpose: Fluent builder for attribute data passed to `create_element` / `update_element`.
- Implementation: Plain value type holding `std::map<std::string, Value>` scalars and `std::map<std::string, std::vector<Value>>` arrays. Rule of Zero — no Pimpl.
- Pattern: `Element().set("label", "x").set("values", {1, 2, 3})`.
- Purpose: Standalone binary file reader/writer for N-dimensional double arrays.
- Implementation: Pimpl (`struct Impl` in `src/binary/binary_file.cpp:20`) hides `std::iostream`, path, metadata, and tracked file position.
- Pattern: Factory `open_file(path, 'r'|'w', metadata?)`; write-registry prevents concurrent open-for-write + read within a process.
- Purpose: Bidirectional conversion between `.qvr` binary format and CSV.
- Implementation: Pimpl.
- Purpose: TOML-backed sidecar metadata describing `.qvr` file structure.
- Implementation: Plain struct — `std::vector<Dimension>`, `std::chrono::system_clock::time_point`, strings. Rule of Zero.
- Factories: `from_toml()`, `from_element()`; serializer: `to_toml()`.
- Purpose: Execute Lua scripts against a `Database` with `db` userdata bound.
- Implementation: Pimpl (`struct Impl` in `src/lua_runner.cpp:14`) hides `sol::state`. Binds Database methods 1:1 via `sol::new_usertype`.
- `quiver_database` wraps `quiver::Database`.
- `quiver_element` wraps `quiver::Element`.
- `quiver_binary_file` wraps `quiver::BinaryFile`.
- `quiver_binary_metadata` wraps `quiver::BinaryMetadata`.
- Each binding treats these as opaque pointers.
## Entry Points
- Purpose: Command-line tool to open a database and optionally run a Lua script.
- Triggers: `./build/bin/quiver_cli.exe <db> [script] [--schema path | --migrations dir] [--read-only] [--log-level ...]`.
- Responsibilities: Arg parsing (`argparse`), instantiating `Database`, invoking `LuaRunner::run()`.
- Purpose: C++ core GoogleTest suite.
- Triggers: `./build/bin/quiver_tests.exe` or `ctest`.
- Sources: All `tests/test_*.cpp` excluding `test_c_api_*.cpp`.
- Purpose: C API GoogleTest suite (built only if `QUIVER_BUILD_C_API=ON`).
- Triggers: `./build/bin/quiver_c_tests.exe`.
- Sources: All `tests/test_c_api_*.cpp`.
- Purpose: Transaction-performance benchmark comparing individual vs. batched transactions.
- Not part of test suite; built alongside tests, run manually.
- Purpose: Prototyping scratchpad. Links against both `quiver` and `quiver_c`.
- Julia: `bindings/julia/test/runtests.jl` via `bindings/julia/test/test.bat`.
- Dart: `bindings/dart/test/*.dart` via `bindings/dart/test/test.bat` (uses native assets + hooks to build DLLs).
- Python: `bindings/python/tests/test_*.py` via `bindings/python/tests/test.bat` (pytest).
- JS/Deno: `bindings/js/test/*.test.ts` via `bindings/js/test/test.bat` (vitest / Deno test).
## Transaction Model
- `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()` — explicit user control.
- `in_transaction()` queries `sqlite3_get_autocommit(db) == 0`.
- Constructor: if `sqlite3_get_autocommit(db)` is true (no active transaction), begins a new one and sets `owns_transaction_ = true`. Otherwise becomes a no-op (guard does nothing on commit/rollback).
- Destructor: rolls back if guard owns the transaction and it wasn't explicitly committed.
- Used in `create_element`, `update_element`, `delete_element`, `update_time_series_group`, etc. — write operations are safe both standalone and inside user-managed explicit transactions (nest-aware).
- Julia: `transaction(db) do db ... end` (`bindings/julia/src/database_transaction.jl`).
- Dart: `db.transaction((db) { ... })` (`bindings/dart/lib/src/database_transaction.dart`).
- Lua: `db:transaction(function(db) ... end)` (wired in `src/lua_runner.cpp:53`).
- Each wraps begin → user function → commit, rolling back on exception.
## Error Handling
- Precondition: `"Cannot {operation}: {reason}"` — e.g., `"Cannot create_element: element must have at least one scalar attribute"` (`src/database_create.cpp:11`).
- Not found: `"{Entity} not found: {identifier}"` — e.g., `"Scalar attribute not found: 'value' in collection 'Items'"`.
- Operation failure: `"Failed to {operation}: {reason}"` — e.g., `"Failed to open database: " + sqlite3_errmsg(db)` (`src/database.cpp:112`).
```cpp
```
- `static thread_local std::string g_last_error;`
- Exposed via `quiver_get_last_error()` / `quiver_clear_last_error()`.
- Julia: `check()` throws `DatabaseException` (`bindings/julia/src/exceptions.jl:12`).
- Python: `check()` raises `QuiverError` (`bindings/python/src/quiverdb/_helpers.py:7`).
- Dart: `check()` throws `QuiverException` (`bindings/dart/lib/src/exceptions.dart`).
- JS/Deno: `check()` throws `QuiverError` (`bindings/js/src/errors.ts:10`).
- `Impl::rollback()` logs and swallows errors from `sqlite3_exec("ROLLBACK;")` — rollback is often called in destructors/error paths, so throwing would cause terminate.
## Memory and Ownership
- All resources managed via `std::unique_ptr`, destructors close files/connections.
- `Database::Impl::~Impl()` closes the sqlite3 connection (`src/database_impl.h:293`).
- `BinaryFile::Impl::~Impl()` flushes the iostream and unregisters from the write-registry (`src/binary/binary_file.cpp:36`).
- Move-only types: `Database`, `BinaryFile`, `CSVConverter`, `LuaRunner` (copy deleted, move defaulted).
- Value types use Rule of Zero: `Element`, `Row`, `Result`, `BinaryMetadata`, `Dimension`, `TimeProperties`, `ScalarMetadata`, `GroupMetadata`, `CSVOptions`.
- Factory functions `new` the handle, `quiver_*_close` / `*_destroy` / `*_free` `delete` it — matching pairs always co-located in the same translation unit.
- Output buffers (`int64_t**`, `char***`, etc.) are allocated with `new[]` inside the C API and must be freed via matching `quiver_database_free_*` / `quiver_binary_file_free_*` / `quiver_binary_metadata_free_*` functions.
- Single strings: `quiver_database_free_string(char*)`, `quiver_binary_file_free_string(char*)`, `quiver_binary_metadata_free_string(char*)`.
- Arrays: `quiver_database_free_integer_array`, `quiver_database_free_float_array`, `quiver_database_free_string_array` (per-element strdup free + array free).
- Metadata: `quiver_database_free_scalar_metadata`, `quiver_database_free_group_metadata` (+ `_array` variants) — internal converters `convert_scalar_to_c` / `free_scalar_fields` live in `src/c/database_helpers.h`.
- `src/c/database_read.cpp` — scalar/vector read + their free functions (integer/float/string arrays).
- `src/c/database_metadata.cpp` — metadata get/list + their free functions.
- `src/c/database_time_series.cpp` — time series read + `quiver_database_free_time_series_data`.
- Bindings copy data out of C buffers into native collections, then call the matching free.
- Handles (`quiver_database_t*` etc.) are wrapped in language-native classes that call close/destroy on disposal (`close()`/`dispose()`/`__del__`/`Dispose`/Julia finalizer).
## Cross-Cutting Concerns
- spdlog-based per-database logger (`src/database.cpp:43` — `create_database_logger`).
- Each `Database` instance gets a unique logger name (`quiver_database_<counter>`), stderr console sink, plus a file sink at `<db-dir>/quiver_database.log`.
- In-memory databases (`:memory:`) skip file logging.
- Level configurable via `DatabaseOptions::console_level` (`LogLevel::Debug|Info|Warn|Error|Off`).
- Bindings have no logging; all log output comes from the C++ core.
- Schema validation: `SchemaValidator` (`src/schema_validator.cpp`) runs once on database open, ensures Configuration table exists, FKs have `ON DELETE/UPDATE CASCADE`, vector/set/time-series tables conform to naming + PK conventions.
- Type validation: `TypeValidator` (`src/type_validator.cpp`) used by `create_element`, `update_element` before insert.
- Binary metadata validation: `BinaryMetadata::validate()`, `validate_time_dimension_metadata()`, `validate_time_dimension_sizes()`.
- `DatabaseOptions` (`include/quiver/options.h:20`): `read_only`, `console_level`.
- `CSVOptions` (`include/quiver/options.h:25`): enum label maps, `date_time_format` strftime string.
- C API mirrors: `quiver_database_options_t`, `quiver_csv_options_t` (`include/quiver/c/options.h`), with `quiver_database_options_default()` / `quiver_csv_options_default()` factories.
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->
## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
