# Technology Stack: Time Series Update Interface Redesign

**Project:** Quiver -- multi-column time series C API interface
**Researched:** 2026-02-12
**Confidence:** HIGH (based on existing codebase patterns and established C FFI precedent)

## Problem Statement

The C API's `quiver_database_update_time_series_group` currently accepts only 2 parallel arrays (`const char* const* date_times` + `const double* values`), but the C++ layer already supports N columns of mixed types via `vector<map<string, Value>>` where `Value = variant<nullptr_t, int64_t, double, string>`. The same gap exists for `quiver_database_read_time_series_group`. The redesigned C API must pass variable-schema, multi-column, mixed-type tabular data through the FFI boundary.

## Recommended Approach: Columnar Parallel Arrays with Type Tags

**Use the columnar typed-arrays pattern -- one flat array per column, with parallel name/type metadata arrays.**

This is the same pattern the codebase already uses for parameterized queries (`database_query.cpp`), extended to tabular data.

### Core New Types (C API)

No new structs are needed. Use flat arrays and the existing `quiver_data_type_t` enum.

| Component | Type | Purpose |
|-----------|------|---------|
| Column names | `const char* const* column_names` | Parallel array of column name strings |
| Column types | `const int* column_types` | Parallel array of `quiver_data_type_t` values |
| Column data | `const void* const* column_data` | Parallel array of typed data pointers |
| Column count | `size_t column_count` | Number of columns |
| Row count | `size_t row_count` | Number of rows (all columns must have this many) |

Each `column_data[i]` points to a flat array of `row_count` elements whose type is determined by `column_types[i]`:

| `column_types[i]` | `column_data[i]` type | Element access |
|--------------------|-----------------------|----------------|
| `QUIVER_DATA_TYPE_INTEGER` (0) | `const int64_t*` | `((int64_t*)column_data[i])[row]` |
| `QUIVER_DATA_TYPE_FLOAT` (1) | `const double*` | `((double*)column_data[i])[row]` |
| `QUIVER_DATA_TYPE_STRING` (2) | `const char* const*` | `((char**)column_data[i])[row]` |

### Why This Approach

**1. Already proven in this codebase.** The parameterized query pattern (`database_query.cpp` lines 10-36) uses exactly `const int* param_types` + `const void* const* param_values` to pass mixed-type data through the C boundary. Both Julia (`marshal_params`) and Dart (`_marshalParams`) already have working marshaling code for this pattern. Extending from "one value per param" to "one column array per slot" is a mechanical change.

**2. Zero new dependencies.** No new structs, no new allocation patterns, no serialization libraries. Uses only existing `quiver_data_type_t`, existing `strdup_safe`, existing `new[]`/`delete[]` pairs.

**3. Columnar layout matches SQL.** Each column maps directly to a SQL column. The C++ conversion function (`convert_columns_to_rows`) assembles `vector<map<string, Value>>` from the parallel column arrays in a single pass. This avoids the row-oriented marshaling overhead of converting N maps to C and back.

**4. FFI-friendly across all binding languages.** Julia `ccall` handles `Ptr{Ptr{Cvoid}}` natively. Dart FFI handles `Pointer<Pointer<Void>>` natively. Both already do this for parameterized queries. No special FFI features needed.

### Concrete C API Signatures

#### Update (write direction: caller provides data)

```c
quiver_error_t quiver_database_update_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    const char* const* column_names,   // [column_count] column name strings
    const int* column_types,           // [column_count] quiver_data_type_t values
    const void* const* column_data,    // [column_count] typed data arrays
    size_t column_count,               // number of columns
    size_t row_count                   // number of rows (all columns same length)
);
```

#### Read (read direction: library allocates, caller frees)

```c
quiver_error_t quiver_database_read_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    char*** out_column_names,          // [*out_column_count] allocated column names
    int** out_column_types,            // [*out_column_count] allocated type tags
    void*** out_column_data,           // [*out_column_count] allocated typed arrays
    size_t* out_column_count,          // number of columns
    size_t* out_row_count              // number of rows
);

quiver_error_t quiver_database_free_time_series_data(
    char** column_names,
    int* column_types,
    void** column_data,
    size_t column_count,
    size_t row_count
);
```

### C++ Conversion (Inside `database_time_series.cpp`)

The conversion from columnar C arrays to `vector<map<string, Value>>` is a straightforward nested loop, following the exact same switch/cast pattern as `convert_params` in `database_query.cpp`:

```cpp
static std::vector<std::map<std::string, quiver::Value>>
columns_to_rows(const char* const* col_names, const int* col_types,
                const void* const* col_data, size_t col_count, size_t row_count) {
    std::vector<std::map<std::string, quiver::Value>> rows;
    rows.resize(row_count);
    for (size_t c = 0; c < col_count; ++c) {
        std::string name(col_names[c]);
        for (size_t r = 0; r < row_count; ++r) {
            switch (col_types[c]) {
            case QUIVER_DATA_TYPE_INTEGER:
                rows[r][name] = static_cast<const int64_t*>(col_data[c])[r];
                break;
            case QUIVER_DATA_TYPE_FLOAT:
                rows[r][name] = static_cast<const double*>(col_data[c])[r];
                break;
            case QUIVER_DATA_TYPE_STRING:
                rows[r][name] = std::string(static_cast<const char* const*>(col_data[c])[r]);
                break;
            default:
                throw std::runtime_error("Unknown column type: " + std::to_string(col_types[c]));
            }
        }
    }
    return rows;
}
```

### Binding Marshaling Patterns

#### Julia (extends existing `marshal_params` pattern)

```julia
function update_time_series_group!(db, collection, group, id; kwargs...)
    # kwargs: date_time=["2024-01-01",...], value=[1.5, 2.5,...], temp=[20, 21,...]
    col_names = String.(collect(keys(kwargs)))
    col_count = length(col_names)
    row_count = length(first(values(kwargs)))

    # Build parallel arrays exactly like marshal_params does
    name_ptrs = [Base.unsafe_convert(Cstring, Base.cconvert(Cstring, n)) for n in col_names]
    type_tags = Cint[]
    data_ptrs = Ptr{Cvoid}[]
    refs = []  # GC roots

    for (name, vals) in kwargs
        if eltype(vals) <: Integer
            push!(type_tags, PARAM_TYPE_INTEGER)
            arr = Int64.(vals)
            push!(refs, arr)
            push!(data_ptrs, pointer(arr))
        elseif eltype(vals) <: Real
            # ... same pattern for Float64
        elseif eltype(vals) <: AbstractString
            # ... same pattern as existing string array marshaling
        end
    end

    GC.@preserve refs name_cstrings begin
        check(C.quiver_database_update_time_series_group(...))
    end
end
```

#### Dart (extends existing `_marshalParams` pattern)

```dart
void updateTimeSeriesGroup(String collection, String group, int id,
    Map<String, List<Object>> columns) {
  final arena = Arena();
  try {
    final colCount = columns.length;
    final rowCount = columns.values.first.length;
    final names = arena<Pointer<Char>>(colCount);
    final types = arena<Int>(colCount);
    final data = arena<Pointer<Void>>(colCount);

    var i = 0;
    for (final entry in columns.entries) {
      names[i] = entry.key.toNativeUtf8(allocator: arena).cast();
      final values = entry.value;
      if (values.first is int) {
        types[i] = QUIVER_DATA_TYPE_INTEGER;
        final arr = arena<Int64>(rowCount);
        for (var r = 0; r < rowCount; r++) arr[r] = values[r] as int;
        data[i] = arr.cast();
      } else if (values.first is double) {
        // ... same pattern
      }
      i++;
    }
    check(bindings.quiver_database_update_time_series_group(...));
  } finally {
    arena.releaseAll();
  }
}
```

#### Lua (direct -- sol2 handles everything)

Lua already passes `sol::table` of tables directly to C++. The Lua binding calls `db.update_time_series_group()` which accepts `vector<map<string, Value>>`. **No change needed in the Lua layer** -- the existing `update_time_series_group_from_lua` function converts `sol::table` rows to `map<string, Value>` rows directly without going through the C API. Lua bindings bypass the C API entirely via sol2.

## Alternatives Considered and Rejected

### Alternative 1: Opaque Row Builder (like `quiver_element_t`)

**Pattern:** Create a `quiver_time_series_row_t` opaque builder, add columns via setter calls, then pass an array of rows.

```c
// REJECTED
quiver_time_series_row_t* row;
quiver_time_series_row_create(&row);
quiver_time_series_row_set_string(row, "date_time", "2024-01-01");
quiver_time_series_row_set_float(row, "value", 1.5);
// ... repeat for every row
quiver_database_update_time_series_group(db, col, grp, id, rows, row_count);
quiver_time_series_row_destroy(row);
```

**Why rejected:**
- **O(rows * columns) FFI crossings** for the setter calls vs O(columns) for columnar approach. Time series can have thousands of rows -- this matters.
- **New opaque type, new create/destroy, new free functions** -- adds API surface for no benefit.
- **Row-oriented is the wrong shape.** The C++ layer already has `vector<map<string, Value>>` (row-oriented). Converting from row-oriented C structs to row-oriented C++ maps is fine, but the callers (Julia, Dart) naturally have columnar data (arrays/vectors). Converting columnar -> row -> row is wasteful. Columnar -> columnar -> row (at C++ boundary only) is one conversion.
- **The element builder pattern exists because elements have mixed scalar+array attributes.** Time series rows are uniform tabular data. Different problem, different solution.

### Alternative 2: JSON Serialization

**Pattern:** Serialize rows as JSON string, pass single `const char*` through C boundary.

```c
// REJECTED
quiver_database_update_time_series_group_json(db, col, grp, id,
    "[{\"date_time\":\"2024-01-01\",\"value\":1.5}]");
```

**Why rejected:**
- **Serialization/deserialization overhead** for every call. JSON parsing is not free.
- **New dependency** (JSON parser in C++ layer) or hand-rolled parser.
- **Type information lost** -- JSON numbers are ambiguous (integer vs float). Would need type coercion logic that does not exist today.
- **Violates the project's "Intelligence in C++ layer" principle.** The C API should pass structured data, not strings-to-be-parsed.
- **No other quiver C API function uses serialization.** This would be an anomalous pattern.

### Alternative 3: Row-of-Unions (Struct Array)

**Pattern:** Define a `quiver_value_t` tagged union struct, pass `quiver_value_t**` (array of rows, each row is array of values).

```c
// REJECTED
typedef struct {
    quiver_data_type_t type;
    union { int64_t i; double f; const char* s; };
} quiver_value_t;

quiver_database_update_time_series_group(db, col, grp, id,
    column_names, values_2d, col_count, row_count);
```

**Why rejected:**
- **Tagged unions are painful in FFI.** Julia cannot directly map C tagged unions -- requires manual `Ref{quiver_value_t}` construction with byte-level layout awareness. Dart FFI `Union` support works but is verbose and error-prone (must match exact padding/alignment).
- **Row-of-unions requires O(rows * cols) individual struct constructions** on the binding side. Columnar arrays require O(cols) array allocations.
- **New struct type** that does not exist in the codebase. The existing `quiver_data_type_t` enum + `void*` pattern is already proven.

### Alternative 4: `void**` Array of Pointers (Untyped)

**Pattern:** Pass column data as `void**` without type tags, rely on metadata query to determine types.

```c
// REJECTED -- caller queries metadata first, then passes void** trusting correct types
quiver_database_update_time_series_group(db, col, grp, id,
    column_names, column_data, col_count, row_count);  // no type tags!
```

**Why rejected:**
- **No type safety at the C boundary.** A caller passing `int64_t*` where `double*` was expected would silently corrupt data.
- **Requires callers to pre-query metadata.** Adds an extra round-trip and makes the API stateful (depends on previous call result).
- **The parameterized query precedent explicitly includes type tags.** This is a solved problem in the codebase.

## Integration Points with Existing Types

| Existing Type/Pattern | How It Integrates |
|-----------------------|-------------------|
| `quiver_data_type_t` enum | Reused directly as `column_types[i]` values. INTEGER=0, FLOAT=1, STRING=2 cover all time series column types. NULL=4 is not needed (time series columns are NOT NULL in schema). |
| `convert_params()` in `database_query.cpp` | Direct model for the column-to-row conversion function. Same switch/cast pattern, extended from 1D to 2D. |
| `strdup_safe()` in `database_helpers.h` | Used for allocating output string columns in the read direction. |
| `copy_strings_to_c()` in `database_helpers.h` | Used for allocating `out_column_names` string array in read direction. |
| `QUIVER_REQUIRE` macro | Used for null-checking all input parameters. |
| `quiver_group_metadata_t` | Already exposes column names and types via `get_time_series_metadata`. Bindings can use this to validate column types before marshaling (optional optimization, not required). |
| Alloc/free co-location pattern | The new `free_time_series_data` replaces the old one in the same `database_time_series.cpp` translation unit. |
| `marshal_params` (Julia) / `_marshalParams` (Dart) | Direct templates for the columnar marshaling code in bindings. |

## What Does NOT Change

| Component | Reason |
|-----------|--------|
| C++ `Database` class API | `update_time_series_group(collection, group, id, rows)` signature stays the same. It already accepts `vector<map<string, Value>>`. |
| C++ `Value` type | `variant<nullptr_t, int64_t, double, string>` is unchanged. |
| Lua bindings | Lua passes `sol::table` directly to C++, never goes through C API. No change. |
| `quiver_data_type_t` enum | Reused as-is. No new enum values needed. |
| `quiver_element_t` and element operations | Completely separate concern. Element builder is for create_element, not time series update. |
| CMake build configuration | No new source files needed -- `database_time_series.cpp` already exists, just update its content. |
| FFI generators | Run `scripts/generator.bat` after C header changes. Existing process. |

## Migration Path

The old 2-parameter signature and the new N-column signature cannot coexist (same function name). This is a **breaking change** to the C API. Per project principles ("WIP project -- breaking changes acceptable, no backwards compatibility required"), this is acceptable.

### Rollout Order

1. **C API header** -- change signature in `include/quiver/c/database.h` (both update and read)
2. **C API implementation** -- update `src/c/database_time_series.cpp` with columnar conversion
3. **C API tests** -- update `test_c_api_database_time_series.cpp`
4. **Regenerate FFI** -- run `scripts/generator.bat`
5. **Julia binding** -- update `database_update.jl` and `database_read.jl`
6. **Dart binding** -- update `database_update.dart` and `database_read.dart`
7. **Julia/Dart tests** -- update binding test files
8. **Multi-column test schema** -- add test using `multi_time_series.sql` (already exists with temperature+humidity columns)

## No New Dependencies Required

The recommended approach uses only existing language features and patterns:

| Language | What's Used | Already Available |
|----------|-------------|-------------------|
| C | `void*`, `int*`, `const char* const*`, `size_t` | Standard C |
| C++ | `static_cast`, `std::string`, `std::map`, `quiver::Value` | Already in codebase |
| Julia | `Ptr{Cvoid}`, `Cint`, `Cstring`, `ccall`, `GC.@preserve` | Standard Julia FFI |
| Dart | `Pointer<Void>`, `Pointer<Int>`, `Arena`, `Int64`, `Double` | dart:ffi (already used) |

**No new libraries. No new build dependencies. No new CMake targets.**

## Sources

- Quiver codebase `src/c/database_query.cpp` -- existing `convert_params()` pattern for mixed-type C FFI (lines 10-36)
- Quiver codebase `bindings/julia/src/database_query.jl` -- existing `marshal_params()` Julia FFI marshaling
- Quiver codebase `bindings/dart/lib/src/database.dart` -- existing `_marshalParams()` Dart FFI marshaling
- Quiver codebase `src/c/database_time_series.cpp` -- current single-column implementation to be replaced
- SQLite C API (`sqlite3_bind_*` family) -- industry precedent for type-tagged value passing in C APIs
- Apache Arrow C Data Interface -- industry precedent for columnar typed data passing through C FFI (columnar layout with type metadata, though Arrow's full schema is overkill for this use case)
- libpq (PostgreSQL C client) -- uses `const char* const*` parallel arrays with type OID arrays for parameterized queries, same conceptual pattern

---
*Stack research for: Quiver time series update interface redesign*
*Researched: 2026-02-12*
