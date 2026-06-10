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
  iteration.h                 # first_dimensions, next_dimensions, dimension_sizes_at_values free functions
  binary_metadata.h         # BinaryMetadata struct - dimensions, labels, serialization
  dimension.h             # Dimension struct (name, size, optional TimeProperties)
  time_properties.h       # TimeFrequency enum, TimeProperties struct
  time_constants.h        # Time dimension size constraints
include/quiver/expression/  # Expression subsystem headers (lazy expressions on .qvr files)
  expression.h                # Expression value type, + - * / operator overloads, save engine
  expression_node.h           # ExpressionNode base + ExpressionFile/Scalar/Binary + Unary/Ternary/Aggregation scaffolds
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
  utils/csv_format.h      # locale-aware CSV number formatting (decimal/field separator, number normalization)
  utils/string.h          # String utilities: new_c_str, trim
src/binary/                 # Binary C++ implementation
  binary_file.cpp             # BinaryFile class (Pimpl impl)
  csv_converter.cpp            # CSVConverter implementation
  iteration.cpp               # first_dimensions/next_dimensions impls + dimension_sizes_at_values helper
  binary_metadata.cpp       # BinaryMetadata factories, serialization, validation
  time_properties.cpp     # TimeFrequency string conversion
src/expression/             # Expression C++ implementation
  expression.cpp              # Expression class, operator overloads, save engine
  expression_helpers.h        # Shared inline helpers (validation, broadcast metadata, aggregation templates, percentile)
  expression_file.cpp         # ExpressionFile (leaf reading from .qvr)
  expression_scalar.cpp       # ExpressionScalar (constant broadcast)
  expression_binary.cpp       # ExpressionBinary (Add/Sub/Mul/Div)
  expression_unary.cpp        # ExpressionUnary (Negate/Abs/Sqrt/Log/Exp)
  expression_ternary.cpp      # ExpressionTernary (IfElse)
  expression_aggregate.cpp    # ExpressionAggregate (dimension-axis Sum/Mean/Min/Max/Percentile)
  expression_aggregate_agents.cpp  # ExpressionAggregateAgents (label-axis reduction)
  expression_select_agents.cpp     # ExpressionSelectAgents (label-axis projection)
  expression_rename_agents.cpp     # ExpressionRenameAgents (label-axis rename)
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
- **Error Messages**: All error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own. The only exception is pre-FFI type-marshalling errors (e.g., Python/Dart rejecting an unsupported value type before the FFI call), which the C API cannot diagnose because the value never reaches it; those messages are crafted locally and should name the offending column and type.
- **Homogeneity**: Binding interfaces must be consistent and intuitive. API surface should feel uniform across wrappers.
- **Ownership**: RAII used strictly. Ownership of pointers/resources must be explicit and unambiguous.
- **Constraint**: Be critical. If code is already optimal, state that clearly. Do not invent useless suggestions just to provide output.
- All public C++ methods should be bound to C API, then to Julia/Dart/Lua
- All *.sql test schemas in `tests/schemas/`, bindings reference from there
- **Self-Updating**: Always keep CLAUDE.md up to date with codebase changes

## Build & Test

### Running Python locally
Plain `python` / `python3` / `py` are not on PATH in this environment. Run Python through
`uv` instead — it provisions the interpreter and dependencies on the fly:
```bash
uv run python -c "..."                 # ad-hoc script
uv run --with pyyaml python -c "..."   # with a one-off dependency (e.g. validate a workflow YAML)
```
(CI runners invoke `python3` directly; this `uv` note is for local/agent use only.)

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

Binary subsystem: `BinaryFile` uses Pimpl (hides file I/O dependencies). `CSVConverter` is a plain class composing a `BinaryMetadata` and the CSV `iostream` (no Pimpl, no inheritance). `BinaryMetadata`, `Dimension`, `TimeProperties` are plain value types.

Expression subsystem: `Expression` is a plain value type wrapping `shared_ptr<ExpressionNode>` — no Pimpl. `ExpressionNode` is an abstract base with virtual `metadata()` / `compute_row()`; concrete subclasses `ExpressionFile`, `ExpressionScalar`, `ExpressionBinary`, `ExpressionUnary`, `ExpressionTernary`, `ExpressionAggregate`, `ExpressionAggregateAgents`, `ExpressionSelectAgents`, `ExpressionRenameAgents` are exposed via `QUIVER_API` and use Rule of Zero. Polymorphism is justified by the recursive tree shape (`ExpressionBinary`, `ExpressionUnary`, `ExpressionTernary`, and the aggregation / label-projection nodes own child `shared_ptr<ExpressionNode>` operands).

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
quiver_binary_file_open_file/close
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
- Time series: `read_time_series_group()`, `update_time_series_group()`, `add_time_series_row()` — both `read_time_series_group` and `update_time_series_group` use multi-column `vector<map<string, Value>>` rows with N typed value columns per time series group; `add_time_series_row` appends/upserts a single row using a `map<string, Value>` (one value per column)
- Time series files: `has_time_series_files()`, `list_time_series_files_columns()`, `read_time_series_files()`, `update_time_series_files()`
- Metadata: `get_scalar_metadata()`, `get_vector_metadata()`, `get_set_metadata()`, `get_time_series_metadata()` — all group metadata returns unified `GroupMetadata` with `dimension_column` (populated for time series, empty for vectors/sets)
- List groups: `list_scalar_attributes()`, `list_vector_groups()`, `list_set_groups()`, `list_time_series_groups()`
- Query: `query_string/integer/float(sql, parameters = {})` - parameterized SQL with positional `?` placeholders
- Schema inspection: `describe()` - prints schema info to stdout
- CSV: `export_csv()`, `import_csv()` -- CSV export/import with optional enum/date formatting via `CSVOptions`
  - **Locale-aware separators** (`utils/csv_format.h`): export matches the OS locale decimal separator — a `,` locale emits `,` decimals paired with a `;` field delimiter and a `sep=;` header (so Excel reads numbers correctly); a `.` locale emits the usual `,`-delimited `sep=,` file. Import resolves the delimiter from the `sep=` header (or infers it from the header line), parses with that delimiter directly (no `;`→`,` swap), auto-detects CRLF/LF/CR line endings, and normalizes locale numbers (thousands separators stripped, decimal comma → `.`) before parsing. Because export output is locale-dependent, byte-level CSV test assertions read through a `read_file_canonical`/`read_csv_canonical` helper that maps a `sep=;` file back to canonical comma/dot form.
  - **Label-identifier import preserves ids**: a scalar import omits the `id` column, so existing elements are matched by `label` and re-inserted with their original id (new labels get a fresh AUTOINCREMENT id above the preserved range). This keeps foreign keys in other tables pointing at the same element across a re-import.

### Element Class
Builder for element creation with fluent API:
```cpp
Element().set("label", "Item 1").set("value", 42).set("tags", {"a", "b"})
```

> **Python:** Element is internal-only (not part of the public API). Python users pass `**kwargs` to `create_element`/`update_element` instead.

### Binary Subsystem
Standalone binary file I/O layer for `.qvr` files with `.toml` metadata sidecars.

- `BinaryFile` class (Pimpl): `open_file(path, mode, metadata?)`, `read(dims)`, `write(dims, data)`, `get_metadata()`, `get_file_path()`
- `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)`
- `BinaryMetadata` struct: `dimensions`, `initial_datetime`, `unit`, `labels`, `version`; `number_of_time_dimensions()` is derived from `dimensions` (not stored)
  - Factories: `from_toml_content()`, `from_element()`
  - Serialization: `to_toml()`
  - Builders: `add_dimension()`, `add_time_dimension()` (chains `parent_dimension_index` to the previous time dimension)
  - Validation: `validate()`, `validate_time_dimension_metadata()`, `validate_time_dimension_sizes()`
- `Dimension` struct: `name`, `size`, optional `TimeProperties`
- `TimeProperties` struct: `frequency`, `initial_value`, `parent_dimension_index`
- `TimeFrequency` enum: `Yearly`, `Monthly`, `Weekly`, `Daily`, `Hourly`

#### Iteration Helpers
Free functions in `quiver::` for traversing the dimension space of a `BinaryMetadata` (declared in `quiver/binary/iteration.h`):

- `first_dimensions(meta)` — initial position; returns `initial_value` for time dims, `1` for non-time dims
- `next_dimensions(meta, current)` — next position via right-to-left cascade; returns `nullopt` at end. Uses `dimension_sizes_at_values` to handle variable-length time dims (Feb=28/29, Jan=31, etc.)
- `dimension_sizes_at_values(meta, values)` — per-dim actual sizes at a coordinate vector. Used by `next_dimensions()` and `ExpressionAggregate` to size variable-length time-dim iteration windows.

Used by `Expression::save()` and `CSVConverter::{bin_to_csv, csv_to_bin}` — single source of truth for `.qvr` traversal.

#### Write Registry
In-process path registry prevents reading files that are currently open for writing. A `static std::unordered_set<std::string>` in `binary.cpp` tracks canonical paths of files open in write mode. Opening a reader or a second writer on a registered path throws `std::runtime_error`. The registry entry is removed in the `Binary` destructor (before flush). Move semantics work correctly: moved-from objects have null `impl_` and skip unregistration. Not thread-safe or multi-process safe.

#### Binary Performance Bottlenecks
Profiled with 480×500×31 dimensions (~7.3M read/write calls). Main hot-path costs:

1. **`unordered_map<string, int64_t>` dims parameter (~40% of total time):** Every read/write constructs a hash map with string keys. Hashing, heap allocation for strings/buckets, and `find()` lookups dominate. Indexed `vector<int64_t>` overloads were prototyped (D-13/D-14) and dropped (`b9c9ad0`) in favor of API simplicity — the map-based form is the single supported entry point. Hot-path consumers (e.g., `FileNode::compute_row`, `Expression::save`) cache the `unordered_map` across calls to amortize the allocation. Do not reintroduce indexed overloads without revisiting that decision.
2. **`validate_dimension_values` (~19% of total time):** Called on every read/write. Checks dimension count, name existence, bounds, and time-dimension consistency (date arithmetic via `add_offset_from_int`/`datetime_to_int`). Could be made opt-in or skippable when callers guarantee correct values (e.g., when iterating via `next_dimensions()`).
3. **`vector<double>` allocation per read (~3%):** Each `read()` allocates a new vector. A `read_into(buffer, dims)` overload writing into caller-provided storage would eliminate this.

(Note: `FileNode::compute_row` was renamed to `ExpressionFile::compute_row`; the cache-amortization point still applies.)

### Expression Subsystem
Lazy expressions over `.qvr` binary files. Build a DAG using `+ - * /` operator overloads (binary and unary minus) and unary math free functions, materialize via `save()`.

```cpp
auto a = BinaryFile::open_file("a", 'r');
auto b = BinaryFile::open_file("b", 'r');
Expression result = abs((a + b) * 2.0 - sqrt(Expression(a)));
result.save("output");  // writes output.qvr + output.toml
```

- `Expression` value type (header `quiver/expression/expression.h`):
  - Constructors: `Expression(const BinaryFile&)` (implicit, enables `bf_a + bf_b`), `Expression(shared_ptr<ExpressionNode>)`
  - Accessors: `metadata()`
  - Materialize: `save(path)` — iterates via `first_dimensions`/`next_dimensions`, calls `compute_row()` per cell, writes to a new `.qvr`. Throws if `path` collides (after `weakly_canonical`) with any input file in the DAG.
  - Aggregation: `aggregate(dimension, op, [parameter])` collapses a dimension; `aggregate_agents(op, [parameter])` collapses the label axis. `op` is the nested enum `ExpressionAggregate::Operation` (for `aggregate`) or `ExpressionAggregateAgents::Operation` (for `aggregate_agents`), each with `Sum / Mean / Min / Max / Percentile`. `Percentile` requires a `parameter` fraction in `[0, 1]`; nullary ops reject `parameter`.
  - Label-axis projection: `select_agents(labels)` keeps (and may reorder) a chosen subset of operand labels; `rename_agents(mapping)` rewrites labels in place via a partial `{old: new}` map. Both validate eagerly: `select_agents` throws if any requested label is absent; `rename_agents` throws on duplicate keys or unknown keys, and `BinaryMetadata::validate()` rejects renames that produce duplicate output labels.
- Operator overloads (12 binary + 1 unary): `+ - * /` × {expr+expr, expr+double, double+expr}, plus unary `-expr`.
- Free functions in `quiver::` for unary math: `abs(expr)`, `sqrt(expr)`, `log(expr)`, `exp(expr)`.
- Free function `ifelse(cond, then_value, else_value)` selects per-element: NaN cond → NaN; `cond != 0` → `then_value`; else → `else_value`. `then` and `else` units must match; `cond`'s unit is ignored.
- `ExpressionNode` hierarchy (header `quiver/expression/expression_node.h`):
  - `ExpressionNode` (abstract): `metadata()`, `compute_row(dims, out)`
  - `ExpressionFile`: lazy reads from a `.qvr`. Caches an open `BinaryFile` and a reusable `unordered_map` across calls (mutable members; not thread-safe per instance).
  - `ExpressionScalar`: broadcasts a constant across the operand's label space.
  - `ExpressionBinary`: combines two operands with `ExpressionBinary::Operation::{Add,Subtract,Multiply,Divide}` (nested enum). Constructor pre-computes broadcast metadata (`build_broadcast_metadata`), reusable input/output buffers, and `lhs_to_out_`/`rhs_to_out_` index translation tables. The `apply(Operation, double, double)` operation-dispatch is a private static member.
  - `ExpressionUnary`: applies a single-operand math function with `ExpressionUnary::Operation::{Negate,Abs,Sqrt,Log,Exp}` (nested enum). `metadata()` returns the operand's metadata unchanged (no dimensional analysis — `sqrt(MW)` stays as `MW`). Constructor pre-allocates a reusable `operand_row_buf_`. Lets IEEE-754 NaN/inf propagate naturally (`sqrt(-1) → NaN`, `log(0) → -inf`); no NaN special-casing. The `apply(Operation, double)` operation-dispatch is a private static member.
  - `ExpressionTernary`: selects per-element across three operands. `Operation::{IfElse}` (nested enum). For `IfElse`: NaN in `condition` → NaN; `condition != 0` → `then_value`; else `else_value`. Constructor eagerly validates (`then` and `else` units must match; `condition`'s unit is ignored; shapes broadcast across all three pairs), pre-builds broadcast metadata via `build_ternary_broadcast_metadata`, pre-allocates per-operand dim/label translation tables and reusable buffers. The `apply(Operation, double, double, double)` operation-dispatch is a private static member.
  - `ExpressionAggregate`: collapses a named dimension. `Operation::{Sum,Mean,Min,Max,Percentile}` (nested enum). Constructor eagerly removes the dim from output metadata, rewires child time-dim `parent_dimension_index` transitively (a time dim whose parent was removed re-points to the removed dim's grandparent, or `-1`), and pre-allocates index translation + reusable buffers. Skips NaN inputs during accumulation; all-NaN range yields NaN.
  - `ExpressionAggregateAgents`: collapses the label axis to a single entry named after the operation (e.g., `"sum"`, `"mean"`, `"percentile"`). Dimensions, `initial_datetime`, `unit` unchanged. Same NaN policy as `ExpressionAggregate`.
  - `ExpressionSelectAgents`: projects the operand onto a caller-supplied label list. Constructor pre-computes a `selected_indices_` table from operand-label → output-position, copies operand metadata with `labels` replaced, and calls `output_meta_.validate()` (which rejects duplicate output labels). Missing labels throw `"Cannot select_agents: label not found: '<name>'"`. `compute_row` reads the operand row into a reusable buffer and gathers selected columns into `out`.
  - `ExpressionRenameAgents`: rewrites operand labels via a partial `{old: new}` mapping. Constructor builds a rename map (duplicate keys throw), walks operand labels swapping matched names, verifies every key was used (unmatched keys throw), and calls `output_meta_.validate()` (rejects collisions like `val1→val2` when `val2` already exists). `compute_row` forwards directly to the operand — count and order are unchanged so no per-row reshuffle is needed.
- Validation is **eager** at construction for `ExpressionBinary`, `ExpressionTernary`, `ExpressionAggregate`, `ExpressionAggregateAgents`, `ExpressionSelectAgents`, `ExpressionRenameAgents` (units/dim sizes/time-dim properties/label sizes/initial datetimes for binary and ternary; dim existence + op/parameter consistency + output metadata validity for aggregations; label existence + uniqueness for label-axis projections). `ExpressionUnary` has no inputs to cross-validate so its constructor just sizes the row buffer. Computation is **lazy**: no I/O until `save()`.
- All operation enums are nested in their owning class: `ExpressionBinary::Operation`, `ExpressionUnary::Operation`, `ExpressionTernary::Operation`, `ExpressionAggregate::Operation`, `ExpressionAggregateAgents::Operation`. The two aggregation enums are parallel types with identical values (`Sum / Mean / Min / Max / Percentile`). Label-axis projection nodes (`ExpressionSelectAgents`, `ExpressionRenameAgents`) have no operation enum — their behavior is fully specified by the label list / rename map. The C API mirrors this with five parallel enums: `quiver_expression_operation_t`, `quiver_expression_unary_operation_t`, `quiver_expression_ternary_operation_t`, `quiver_expression_aggregate_operation_t`, `quiver_expression_aggregate_agents_operation_t`.

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
| Time series add row | `add_time_series_row()` | `quiver_database_add_time_series_row()` | `add_time_series_row!()` | `addTimeSeriesRow()` | `add_time_series_row()` |
| Time series update | `update_time_series_group()` | `quiver_database_update_time_series_group()` | `update_time_series_group!()` | `updateTimeSeriesGroup()` | `update_time_series_group()` |
| Query | `query_string()` | `quiver_database_query_string()` | `query_string()` | `queryString()` | `query_string()` |
| CSV | `export_csv()` | `quiver_database_export_csv()` | `export_csv()` | `exportCSV()` | `export_csv()` |
| Describe | `describe()` | `quiver_database_describe()` | `describe()` | `describe()` | `describe()` |

**Binary cross-layer examples:**

| Category | C++ | C API | Julia |
|----------|-----|-------|-------|
| Open file | `BinaryFile::open_file(path, mode, md?)` | `quiver_binary_file_open_file()` | `open_file(path; mode, metadata=nothing)` |
| Close | (destructor) | `quiver_binary_file_close()` | `close!(file)` |
| Read | `binary_file.read(dims)` | `quiver_binary_file_read()` | `read(file; dims...)` |
| Write | `binary_file.write(dims, data)` | `quiver_binary_file_write()` | `write!(file; data=data, dims...)` |
| Get metadata | `binary_file.get_metadata()` | `quiver_binary_file_get_metadata()` | `get_metadata(file)` |
| Get file path | `binary_file.get_file_path()` | `quiver_binary_file_get_file_path()` | `get_file_path(file)` |
| Bin to CSV | `CSVConverter::bin_to_csv()` | `quiver_csv_converter_bin_to_csv()` | `bin_to_csv()` |
| CSV to bin | `CSVConverter::csv_to_bin()` | `quiver_csv_converter_csv_to_bin()` | `csv_to_bin()` |
| Metadata builder | `BinaryMetadata{}` | `quiver_binary_metadata_create()` | `Metadata(; kwargs...)` |
| Metadata from TOML | `BinaryMetadata::from_toml_content()` | `quiver_binary_metadata_from_toml()` | `from_toml_content()` |
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
- **Publishing**: `bindings/julia/` is the **canonical** Julia package and the *single source of
  truth*; the published `psrenergy/Quiver.jl` is a **full generated mirror** (never hand-edit it —
  develop only in `bindings/julia`). It is a plain S3-artifact package in General (no `Quiver_jll`,
  no Yggdrasil). The `publish-julia.yml` workflow (`workflow_dispatch`; runs standalone or
  dispatched by the `publish.yml` orchestrator) downloads the native libs from S3 (staged there
  by the `publish-s3.yml` native build for `linux-x86_64`, `macos-aarch64`, and
  `windows-x86_64`), runs
  `scripts/julia/generate_artifacts.jl` (tar → S3 upload → `Artifacts.toml`), then **wipes the mirror
  (keeping only its `.git/`) and copies the entire `bindings/julia` tree into it** — `src/`, `test/`,
  `.github/` (the mirror's `CI.yml`/`TagBot.yml` live here, dormant in the monorepo since nested
  workflows don't run), `README.md`, `.gitignore`, `.gitattributes`, `.JuliaFormatter.toml`, `LICENSE`,
  and the verbatim `Project.toml` (the binding shares Quiver.jl's UUID). It then overlays the real test
  schemas from repo-root `tests/schemas/` (the in-tree `bindings/julia/test/schemas` is an empty stub)
  and the generated `Artifacts.toml`, and opens a PR to `Quiver.jl`. Only `Manifest.toml` is excluded
  (gitignored). Because it's a full mirror, the PR surfaces a **delete** for anything in Quiver.jl not
  present in `bindings/julia`, so any file the mirror needs (extra workflows, docs) must live in
  `bindings/julia`. The job runs on a `[self-hosted, linux]` runner whose ambient IAM role grants S3
  write (no AWS key secrets); it requires the `QUIVER_JL_TOKEN` secret for the cross-repo PR.
- **Windows artifact gotcha:** `generate_artifacts.jl` `chmod 0o755`s the staged libs before tarring —
  Windows DLLs **must be executable in the artifact**, or Pkg's Windows extraction yields an NTFS ACL
  without execute and `LoadLibrary` fails with "Access is denied" (Linux `.so` load ignores the bit, so
  the symptom is Windows-only).
- **macOS artifact gotchas** (Apple-Silicon-only, `macos-aarch64`): (1) dyld resolves dependent
  dylibs **filesystem-first** — no Linux-style SONAME matching against already-loaded images — so
  the artifact ships libquiver ONLY under its install name `libquiver.0.dylib` (the `.0` tracks
  SOVERSION = major version); a second `libquiver.dylib` copy in the same dir could be loaded as a
  duplicate image (duplicated static state, e.g. the binary write registry). `libquiver_c.dylib`
  gets an `@loader_path` rpath via `install_name_tool` in `publish-s3.yml`. (2) `install_name_tool`
  invalidates the ad-hoc linker signature and arm64 macOS SIGKILLs `dlopen` of unsigned code, so
  the workflow re-signs with `codesign --force --sign -` — that step is load-bearing. (3) The
  mirror's `CI.yml` passes no `arch` to setup-julia (runner-native: x64 on ubuntu/windows, aarch64
  on macos-latest); x64 Julia on an arm64 mac runs under Rosetta and would not match the
  `arch = "aarch64"` Artifacts.toml entry.
- **Library loader** (`src/c_api.jl`, emitted from `generator/prologue.jl`) resolves `libquiver_c`
  in three tiers: `QUIVER_LIB_DIR` env → S3 artifact (when an `Artifacts.toml` is present, i.e. the
  published mirror) → in-tree `build/` (monorepo dev, the default here). Uses the runtime Artifacts
  functional API (`find_artifacts_toml`/`artifact_hash`/`artifact_path`), NOT `@artifact_str`, so the
  monorepo (which has no `Artifacts.toml`) still precompiles. Deps now include `Artifacts` + `Libdl`.

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

### JavaScript / Bun Notes
- Runs on **Bun** (`bun:ffi`), not Deno. Published to **npm** as `quiverdb` (unscoped); package
  manifest is `bindings/js/package.json` (no `deno.json`). `bun test test`, `bun run lint/format`
  (biome). No permission flags (Bun has none).
- **No generator** — FFI symbols are hand-written in `src/loader.ts` as `{ name: { args, returns } }`.
  Add new C API functions there manually.
- **Library loader** (`src/loader.ts`): lazy `getSymbols()` (init on first use — eager init would hit
  a `QuiverError` TDZ during the loader↔errors import cycle). Three tiers: bundled `libs/{os}-{arch}/`
  (shipped in the npm package) → dev `build/bin` walk-up → system PATH. On Windows, `ensureCoreOnPath`
  prepends the lib dir to `process.env.PATH` so the OS loader finds the sibling `libquiver.dll`
  (Bun's `dlopen` cannot preload the core lib — it rejects an empty symbol map).
- **Bun FFI gotchas (load-bearing):**
  - `FFIType.buffer` is rejected as an argument ABI type → buffer/string params use `"pointer"` and the
    call sites pass the `Uint8Array` (`alloc.buf`) directly; Bun pins a TypedArray for the call.
  - For out-params, **pass the TypedArray to the FFI call**, never a precomputed `ptr()` number, and read
    the `DataView` *after* the call. Creating a `DataView`/accessing `.buffer` between `ptr(buf)` and the
    call materializes/relocates a small JSC typed array, leaving the stored pointer stale (deterministic
    silent corruption — C writes land in abandoned memory).
  - No struct-by-value FFI return (oven-sh/bun#6139) → `quiver_database_options_default` is omitted;
    `ffi-helpers.makeDefaultOptions()` builds the options struct in JS.
- **Publishing:** `package.json` `files` allowlist ships `libs/**`; an empty `.npmignore` stops
  `npm pack` falling back to `.gitignore` (which excludes `*.dll`/`*.so`). The `publish-js.yml`
  workflow (`workflow_dispatch`; runs standalone or dispatched by the `publish.yml` orchestrator)
  downloads native libs from S3 (staged by the `publish-s3.yml` native build) into
  `libs/{linux-x86_64,macos-aarch64,windows-x86_64}/`, asserts every lib is in a throwaway
  `npm pack` tarball via
  `tar -tzf` (format-independent; npm roots entries under `package/`), then publishes with
  **`npm publish --loglevel verbose` via `actions/setup-node@v6`** using **npm Trusted Publishing
  (OIDC)** — `permissions: id-token: write`, no stored token; npm packs inline so the published
  artifact carries the deterministic, asserted file set. setup-node uses
  `package-manager-cache: false` (v6 caches by default; the Bun project has no `package-lock.json`).
  Verbose logging is load-bearing: npm logs the OIDC exchange result only at that level — a failed
  exchange silently falls back to token auth (setup-node's `NODE_AUTH_TOKEN` placeholder) and dies
  with a misleading E404 on the PUT. `publishConfig.provenance: true` emits a signed
  provenance attestation (repo is public). Requires a trusted publisher configured on npmjs.com
  (GitHub Actions · `psrenergy/quiver` · workflow `publish-js.yml` · environment `npm`) and
  npm ≥ 11.5.1 (ensured via `npm install -g npm@latest` on Node 24). Bun can't do OIDC trusted
  publishing yet (bun#24855→#15601), so the publish job uses npm; the binding + tests remain 100%
  Bun. A standalone re-dispatch of an already-published version is skipped via an `npm view` guard.

### Release Pipeline
- `publish.yml` (`workflow_dispatch`, optional `ref` input, no version input — the tag must equal
  what the source declares) orchestrates a full release: resolve version via
  `scripts/assert_version.py` + assert tag `v<version>` is absent or already at the release sha →
  dispatch `publish-s3.yml` and wait → create tag + GitHub release (`ncipollo/release-action`,
  `skipIfReleaseExists`) → dispatch `publish-julia.yml` / `publish-python.yml` / `publish-js.yml`
  **in parallel on the new tag** and wait for all three.
- Children are dispatched as top-level `workflow_dispatch` runs via `scripts/ci/dispatch_workflow.sh`
  (dispatch → correlate run id by workflow+ref+created-after → poll to completion), NOT as
  `workflow_call` reusable workflows: npm and PyPI trusted publishing validate the **top-level
  workflow filename** from the OIDC claims, so `publish-js.yml`/`publish-python.yml` must stay
  top-level. `gh workflow run` with `GITHUB_TOKEN` works (`workflow_dispatch` is an explicit
  exception to "GITHUB_TOKEN events don't trigger workflows"; needs `actions: write`).
- Dispatching the bindings on the tag pins them to the release commit and gives `publish-python`
  a per-release concurrency group (its `cancel-in-progress: true` can't be tripped by a master
  push mid-publish). The `npm`/`pypi` environments must keep deployment branch policy
  "No restriction" (or allow `v*` tags) or tag-dispatched publishes get rejected.
- A full re-run of a partially failed release is idempotent end-to-end: tag assert passes (same
  sha), S3 overwrites the same keys, release creation is skipped, the Julia PR branch is
  force-updated, PyPI uses `skip-existing: true`, npm hits the `npm view` guard.

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
