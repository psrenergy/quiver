# C API (`src/c/` + `include/quiver/c/`)

The FFI surface every binding sits on. Thin marshaling over the C++ core — the C API never
re-implements validation or error messages that exist in C++ (one layer owns each message).
Cross-layer naming rules live in the root `CLAUDE.md`; C++ internals in `src/CLAUDE.md`.

## File Map

```
include/quiver/c/         # C API headers (for FFI)
  common.h                # quiver_error_t, quiver_get_last_error, quiver_version
  options.h               # All option types and defaults: LogLevel, DatabaseOptions, CSVOptions
  database.h / element.h / lua_runner.h
include/quiver/c/binary/    # Binary C API headers
  binary_file.h               # quiver_binary_file_t opaque handle, open/close/read/write
  csv_converter.h             # bin_to_csv, csv_to_bin functions
  binary_metadata.h           # quiver_binary_metadata_t + flat structs (quiver_dimension_t, quiver_time_properties_t)
include/quiver/c/expression/  # Expression C API header
  expression.h                # quiver_expression_t handle + node constructors + five operation enums
src/c/
  common.cpp              # quiver_get_last_error / quiver_set_last_error / quiver_version
  internal.h              # Shared structs (quiver_database, quiver_element, quiver_binary_file, quiver_binary_metadata, quiver_expression), QUIVER_REQUIRE macro
  database_helpers.h      # Marshaling templates, string array copiers, metadata converters
  options.cpp             # Option defaults: quiver_database_options_default, quiver_csv_options_default
  database.cpp            # Lifecycle: open, close, factory methods, describe
  database_options.h      # Option converters: convert_database_options, convert_csv_options
  database_create.cpp     # quiver_database_create_element
  database_update.cpp     # quiver_database_update_element
  database_delete.cpp     # quiver_database_delete_element
  database_read.cpp       # All read operations + co-located free functions
  database_metadata.cpp   # Metadata get/list + co-located free functions
  database_query.cpp      # Query operations (plain and parameterized)
  database_time_series.cpp # Time series operations + co-located free functions
  database_transaction.cpp # Transaction control (begin, commit, rollback, in_transaction)
  database_csv_export.cpp / database_csv_import.cpp
  element.cpp             # Element builder C API
  lua_runner.cpp          # LuaRunner C API (errors via quiver_get_last_error)
src/c/binary/               # BinaryFile / CSVConverter / BinaryMetadata wrappers
src/c/expression/           # Expression node constructors, save, free
```

## Return Codes

All C API functions return binary `quiver_error_t` (`QUIVER_OK = 0` or `QUIVER_ERROR = 1`). Values are returned via output parameters.
Exceptions: `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`, `quiver_database_options_default`, `quiver_csv_options_default` (utility functions with direct return).

## Error Handling

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

Every entry point that executes C++ logic wears the try/catch: nothing may throw across the FFI
boundary. Trivial functions that cannot throw (plain `delete[]` frees, pointer-read getters like
`is_healthy`/`in_transaction`) skip the wrapper; `quiver_database_free_time_series_data` keeps it
because its typed-dispatch deallocation can. All components (LuaRunner included) report through
the single `quiver_get_last_error` channel; there are no per-handle error channels.

## Factory Functions

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

A `NULL` options pointer means defaults — in **every** function that takes options (lifecycle
and CSV alike).

## Memory Management

`new`/`delete`, provide matching `quiver_{entity}_free_*` functions:
```cpp
// Factory functions return error code, use out-parameter for handle
quiver_error_t quiver_database_from_schema(..., quiver_database_t** out_db);
quiver_error_t quiver_database_close(quiver_database_t* db);

// Database entity free functions (arrays, vectors, metadata, time series)
quiver_database_free_integer_array(int64_t*)
quiver_database_free_float_array(double*)
quiver_database_free_string_array(char**, size_t)

// Single string cleanup (strings returned by query/read-by-id/element operations)
quiver_database_free_string(char*)

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

Conventions that keep the error paths safe:
- In the **multi-column time series read path** (where marshaling can fail mid-column), out-arrays
  are zero-initialized (`new T*[n]()`) and out-parameters assigned as soon as each array is
  allocated, so error-path cleanup never sees uninitialized pointers. Single-shot allocations
  elsewhere use plain `new` (nothing can fail between alloc and return).
- **Array/string free functions are NULL-tolerant** (freeing NULL, or an array slot left NULL, is
  a no-op). Struct free functions (`free_scalar_metadata`, `free_group_metadata`,
  `free_dimension`, `free_time_series_files`) `QUIVER_REQUIRE` a non-NULL handle.

### Alloc/Free Co-location
Allocation functions and their corresponding free functions live in the same translation unit, organized per area: read alloc/free pairs in `database_read.cpp`, metadata alloc/free pairs in `database_metadata.cpp`, time series alloc/free pairs in `database_time_series.cpp`. Cross-area sharing exists where types overlap (query strings are freed by `free_string` in `database_read.cpp`; `list_time_series_groups` metadata by the array free in `database_metadata.cpp`).

### String Handling
Always null-terminate, use `quiver::string::new_c_str()` from `src/utils/string.h` (called with
full qualification):
```cpp
inline char* new_c_str(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}
```

## Metadata Types

Unified `quiver_group_metadata_t` for vector, set, and time series groups. `dimension_column` is `NULL` for vectors/sets, populated for time series. Single free functions:
```cpp
quiver_database_free_scalar_metadata(quiver_scalar_metadata_t*)
quiver_database_free_group_metadata(quiver_group_metadata_t*)
quiver_database_free_scalar_metadata_array(quiver_scalar_metadata_t*, size_t)
quiver_database_free_group_metadata_array(quiver_group_metadata_t*, size_t)
```
Internal helpers `convert_scalar_to_c`, `convert_group_to_c`, `free_scalar_fields`, `free_group_fields` in `database_helpers.h` avoid duplication.

## Database Description / Statistics

`quiver_database_describe` / `_describe_collection` / `_summarize_collection` each return a
human-readable **text report** via a `char** out_report` out-param (freed by the existing
`quiver_database_free_string`) — no structs. All three live in `database.cpp` as trivial
`new_c_str(db->db.<fn>(...))` wrappers.

## Multi-Column Time Series

The C API uses a columnar typed-arrays pattern for time series read and update:
- `quiver_database_update_time_series_group()` accepts parallel arrays: `column_names[]`, `column_types[]`, `column_data[]`, `column_count`, `row_count`. Pass `column_count == 0` and `row_count == 0` with NULL arrays to clear all rows.
- `quiver_database_read_time_series_group()` returns columnar typed arrays matching the schema. Column data arrays are typed: `INTEGER` -> `int64_t*`, `FLOAT` -> `double*`, `STRING`/`DATE_TIME` -> `char**`.
- `quiver_database_free_time_series_data()` deallocates read results. String columns require per-element cleanup; numeric columns use a single `delete[]`.
- `quiver_database_read_time_series_row()` returns a single `void*` array whose element type the
  caller dispatches on via `out_data_type`. Null entries are encoded per type: `FLOAT` → NaN,
  `STRING`/`DATE_TIME` → NULL `char*`, `INTEGER` → 0.

This pattern mirrors the `convert_params()` approach from `database_query.cpp` for type-safe FFI marshaling across N typed columns.

## Parameterized Queries

`_params` variants use parallel arrays for typed parameters:
```c
// param_types[i]: QUIVER_DATA_TYPE_INTEGER(0), FLOAT(1), STRING(2), NULL(4)
// param_values[i]: pointer to int64_t, double, const char*, or NULL
quiver_database_query_string_params(db, sql, param_types, param_values, param_count, &out, &has);
```
