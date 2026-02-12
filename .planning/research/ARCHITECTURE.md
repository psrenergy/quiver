# Architecture Patterns

**Domain:** Multi-column time series update interface redesign across C++/C API/FFI bindings
**Researched:** 2026-02-12
**Confidence:** HIGH (based on direct codebase analysis of all layers)

## Problem Statement

The C++ layer already supports multi-column time series via `vector<map<string, Value>>`, but the C API hardcodes a single-value-column assumption: `const char** date_times, const double* values, size_t row_count`. This means:

1. Time series tables with multiple value columns (e.g., `temperature REAL, humidity REAL`) cannot be written through the C API.
2. Time series tables with non-float value columns (e.g., `INTEGER`, `TEXT`) cannot be written through the C API.
3. Both Julia and Dart bindings inherit this limitation because they call the C API.
4. Lua bypasses the C API (calls C++ directly via sol2) and already supports multi-column writes.

The read side has the same limitation: `quiver_database_read_time_series_group` returns `char** date_times, double* values` -- single column, float-only. Both read and update need redesign.

## Current Architecture (What Exists)

### Layer-by-Layer Data Flow for Time Series Update

```
Julia/Dart binding
  |
  |  Binding takes List<Map<String, Object?>> (Dart) or Vector{Dict} (Julia)
  |  but extracts only "date_time" and "value" keys, discards everything else
  |
  v
C API: quiver_database_update_time_series_group(
    db, collection, group, id,
    const char** date_times,   // dimension column values
    const double* values,      // single value column (float only)
    size_t row_count
)
  |
  |  C API reconstructs vector<map<string, Value>> with exactly 2 columns
  |  by looking up metadata for dimension_column and first value_column
  |
  v
C++ Database::update_time_series_group(
    collection, group, id,
    const vector<map<string, Value>>& rows   // fully flexible, multi-column
)
  |
  |  Builds parameterized INSERT with all columns from first row
  |
  v
SQLite (supports any schema the user defined)
```

### Layer-by-Layer Data Flow for Time Series Read

```
C++ Database::read_time_series_group(collection, group, id)
  -> Returns vector<map<string, Value>>  (all columns, all types)
  |
  v
C API: quiver_database_read_time_series_group(...)
  -> Extracts dimension_column as char**, first value_column as double*
  -> Drops all other columns, coerces integers to double
  |
  v
Julia/Dart binding
  -> Reconstructs List<Map<String, Object?>> with only "date_time" and "value"
```

### Bottleneck

The C API is the bottleneck. It flattens a rich multi-column multi-type structure into a rigid 2-column float-only interface. The C++ layer above and SQLite below both support the full schema.

### Existing Precedent: Parameterized Query Pattern

The parameterized query C API already solves a similar problem -- passing heterogeneously-typed data through a C FFI boundary:

```c
// param_types[i]: QUIVER_DATA_TYPE_INTEGER(0), FLOAT(1), STRING(2), NULL(4)
// param_values[i]: pointer to int64_t, double, const char*, or NULL
quiver_database_query_string_params(db, sql,
    param_types, param_values, param_count,
    &out_value, &out_has_value);
```

This uses typed parallel arrays: `const int* param_types` + `const void* const* param_values` + `size_t param_count`. The C API implementation (`convert_params` in `database_query.cpp`) switches on type to reconstruct `vector<Value>`.

## Recommended Architecture

### Design Decision: Columnar Typed Arrays (Not Row-Oriented)

The parameterized query pattern passes one value per parameter. Time series passes N rows x M columns of data. Two approaches are possible:

**Option A: Row-oriented (follow query param pattern exactly)**
Pass `column_count * row_count` typed values in a flat array. Each "cell" is a (type, value) pair. The C API reconstructs the 2D structure.

**Option B: Columnar typed arrays**
Pass each column as a separate typed array. Column names + types describe the schema; column data arrays contain the values. Each column is homogeneous (all ints, all floats, or all strings), which matches SQLite's per-column type affinity and avoids `void*` pointer indirection for every cell.

**Recommendation: Option B (columnar).** Reasons:

1. **Type safety per column.** A column is either all `int64_t*`, all `double*`, or all `const char**`. No `void*` casting per cell. The type array has `column_count` entries (not `row_count * column_count`).
2. **FFI friendliness.** Julia and Dart FFI can pass native arrays directly (`Ptr{Cdouble}`, `Pointer<Double>`). Row-oriented `void*` arrays require per-cell pointer boxing in bindings, which is awkward and error-prone.
3. **Matches the read direction.** Reads naturally produce columnar data (a SQL column maps to one C array). Symmetry between read and write simplifies the mental model.
4. **SQLite affinity.** SQLite columns have a single declared type. Columnar layout matches this -- one type per column, not one type per cell.
5. **Performance.** Columnar arrays are cache-friendly for the common case of iterating all values in a column. Row-oriented `void*` indirection is a pointer chase per cell.

### New C API Signatures

#### Update

```c
// Replaces: quiver_database_update_time_series_group
quiver_error_t quiver_database_update_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    const char* const* column_names,     // [column_count] column names
    const int* column_types,             // [column_count] QUIVER_DATA_TYPE_* enum
    const void* const* column_data,      // [column_count] typed array pointers
    size_t column_count,
    size_t row_count
);
```

Where `column_data[i]` points to:
- `const int64_t*` array of `row_count` elements if `column_types[i] == QUIVER_DATA_TYPE_INTEGER`
- `const double*` array of `row_count` elements if `column_types[i] == QUIVER_DATA_TYPE_FLOAT`
- `const char* const*` array of `row_count` elements if `column_types[i] == QUIVER_DATA_TYPE_STRING`

The dimension column (e.g., `date_time`) is included as one of the named columns. The C API no longer needs to know which column is the dimension -- it passes all columns uniformly and the C++ layer handles ordering.

#### Read

```c
// Replaces: quiver_database_read_time_series_group
quiver_error_t quiver_database_read_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    char*** out_column_names,            // [*out_column_count] allocated column names
    int** out_column_types,              // [*out_column_count] QUIVER_DATA_TYPE_* values
    void*** out_column_data,             // [*out_column_count] typed array pointers
    size_t* out_column_count,
    size_t* out_row_count
);
```

Where `out_column_data[i]` is:
- `int64_t*` array of `*out_row_count` elements if `out_column_types[i] == QUIVER_DATA_TYPE_INTEGER`
- `double*` array of `*out_row_count` elements if `out_column_types[i] == QUIVER_DATA_TYPE_FLOAT`
- `char**` array of `*out_row_count` elements if `out_column_types[i] == QUIVER_DATA_TYPE_STRING`

#### Free

```c
// Replaces: quiver_database_free_time_series_data
quiver_error_t quiver_database_free_time_series_data(
    char** column_names,
    int* column_types,
    void** column_data,
    size_t column_count,
    size_t row_count
);
```

The free function uses `column_types` to determine how to free each `column_data[i]`: `delete[] (int64_t*)`, `delete[] (double*)`, or `delete[] each string then delete[] the char** array`.

### C API Implementation (database_time_series.cpp)

The C API marshaling layer converts between columnar C arrays and the C++ row-oriented `vector<map<string, Value>>`:

**Update direction (columnar -> row-oriented):**
```
For each row r in 0..row_count:
    map<string, Value> row;
    For each column c in 0..column_count:
        row[column_names[c]] = extract_typed_value(column_types[c], column_data[c], r);
    rows.push_back(row);
Call db->db.update_time_series_group(collection, group, id, rows);
```

**Read direction (row-oriented -> columnar):**
```
auto rows = db->db.read_time_series_group(collection, group, id);
auto metadata = db->db.get_time_series_metadata(collection, group);
// Use metadata to determine column names and types (not row content)
Allocate column arrays based on metadata types and row count.
For each row r:
    For each column c:
        Store rows[r][column_names[c]] into typed column_data[c][r];
```

### C++ Layer

**No changes needed.** `Database::update_time_series_group` already takes `vector<map<string, Value>>` and `Database::read_time_series_group` already returns `vector<map<string, Value>>`. The C++ interface is already fully general.

### Lua Layer

**No changes needed.** `update_time_series_group_from_lua` calls C++ directly via sol2, converting Lua tables to `vector<map<string, Value>>`. `read_time_series_group_to_lua` returns the full multi-column data as Lua tables. Lua already supports the full interface.

### Julia Binding Changes

**Current (hardcoded two columns):**
```julia
function update_time_series_group!(db, collection, group, id, rows::Vector{Dict{String, Any}})
    date_time_ptrs = [String(row["date_time"]) for row in rows]
    values = Float64[Float64(row["value"]) for row in rows]
    C.quiver_database_update_time_series_group(db.ptr, collection, group, id,
        date_time_ptrs, values, row_count)
end
```

**New (columnar, schema-driven):**
```julia
function update_time_series_group!(db, collection, group, id, rows::Vector{Dict{String, Any}})
    if isempty(rows)
        # Call C API with column_count=0, row_count=0
        return nothing
    end

    metadata = get_time_series_metadata(db, collection, group)
    # Build column lists from metadata (dimension_column + value_columns)
    column_names = [metadata.dimension_column; [vc.name for vc in metadata.value_columns]]
    column_types = [metadata_type_to_quiver_type(dim_type); [vc.data_type for vc ...]]

    # For each column, extract typed array from rows
    column_data = [extract_column(rows, name, type) for (name, type) in zip(column_names, column_types)]

    # Call new C API
    C.quiver_database_update_time_series_group(db.ptr, collection, group, id,
        column_names_ptrs, column_types_arr, column_data_ptrs,
        length(column_names), length(rows))
end
```

The key change: instead of hardcoding `"date_time"` and `"value"`, the binding queries metadata to discover column names and types, then marshals each column as a typed array.

**Alternative (simpler, kwargs-style):**
```julia
function update_time_series_group!(db, collection, group, id; kwargs...)
    # kwargs like: date_time=["2024-01-01", "2024-01-02"], temperature=[20.5, 21.0]
    # Each kwarg is a column name -> array of values
end
```

This kwargs convenience method can wrap the Dict-based method.

### Dart Binding Changes

**Current (hardcoded two columns):**
```dart
void updateTimeSeriesGroup(String collection, String group, int id,
    List<Map<String, Object?>> rows) {
    // Extracts 'date_time' and 'value' only
    dateTimes[i] = dateTime.toNativeUtf8();
    values[i] = value.toDouble();
    bindings.quiver_database_update_time_series_group(...);
}
```

**New (columnar, schema-driven):**
```dart
void updateTimeSeriesGroup(String collection, String group, int id,
    List<Map<String, Object?>> rows) {
    // Query metadata for column names and types
    final metadata = getTimeSeriesMetadata(collection, group);
    // Marshal each column as typed native array
    // Call new C API with column_names, column_types, column_data arrays
}
```

Same pattern as Julia: metadata-driven column discovery, typed marshaling per column.

## Component Boundaries

### Components Changed

| Component | File(s) | Change Type | Description |
|-----------|---------|-------------|-------------|
| C API time series header | `include/quiver/c/database.h` | **MODIFIED** | New signatures for read/update/free |
| C API time series impl | `src/c/database_time_series.cpp` | **MODIFIED** | New columnar marshaling logic |
| C API helpers | `src/c/database_helpers.h` | **MODIFIED** | Add columnar marshaling helper functions |
| Julia FFI declarations | `bindings/julia/src/c_api.jl` | **REGENERATED** | New ccall signatures |
| Julia update | `bindings/julia/src/database_update.jl` | **MODIFIED** | Columnar marshaling, metadata-driven |
| Julia read | `bindings/julia/src/database_read.jl` | **MODIFIED** | Columnar unmarshaling |
| Dart FFI bindings | `bindings/dart/lib/src/ffi/bindings.dart` | **REGENERATED** | New FFI signatures |
| Dart update | `bindings/dart/lib/src/database_update.dart` | **MODIFIED** | Columnar marshaling, metadata-driven |
| Dart read | `bindings/dart/lib/src/database_read.dart` | **MODIFIED** | Columnar unmarshaling |

### Components Unchanged

| Component | Reason |
|-----------|--------|
| C++ Database class (`include/quiver/database.h`) | Already supports multi-column via `vector<map<string, Value>>` |
| C++ time series impl (`src/database_time_series.cpp`) | Already handles arbitrary columns |
| Lua bindings (`src/lua_runner.cpp`) | Calls C++ directly, already multi-column |
| C++ tests (`test_database_time_series.cpp`) | Test C++ layer which is unchanged |
| All schema files | Schema definitions are not affected |
| All non-time-series C API functions | Unrelated |

## Data Flow (New Design)

### Update Path

```
Julia: update_time_series_group!(db, "Sensor", "readings", 1,
    [Dict("date_time" => "2024-01-01", "temperature" => 20.5, "humidity" => 65.0), ...])

Dart: updateTimeSeriesGroup(db, "Sensor", "readings", 1,
    [{"date_time": "2024-01-01", "temperature": 20.5, "humidity": 65.0}, ...])

  |  Binding queries metadata to discover columns:
  |    dimension_column = "date_time" (TEXT)
  |    value_columns = [{name: "temperature", type: REAL}, {name: "humidity", type: REAL}]
  |
  |  Marshals to columnar arrays:
  |    column_names  = ["date_time", "temperature", "humidity"]
  |    column_types  = [STRING, FLOAT, FLOAT]
  |    column_data   = [char**{"2024-01-01",...}, double*{20.5,...}, double*{65.0,...}]
  |    column_count  = 3
  |    row_count     = N
  |
  v
C API: quiver_database_update_time_series_group(
    db, "Sensor", "readings", 1,
    column_names, column_types, column_data,
    column_count=3, row_count=N)

  |  Converts columnar -> row-oriented:
  |    for each row: map<string, Value> with all columns
  |
  v
C++ Database::update_time_series_group(
    "Sensor", "readings", 1,
    [{date_time: "2024-01-01", temperature: 20.5, humidity: 65.0}, ...])

  |
  v
SQLite: INSERT INTO Sensor_time_series_readings (id, date_time, temperature, humidity)
        VALUES (1, ?, ?, ?)
```

### Read Path

```
C++ Database::read_time_series_group("Sensor", "readings", 1)
  -> Returns [{date_time: "2024-01-01", temperature: 20.5, humidity: 65.0}, ...]
  |
  v
C API: Uses metadata to build typed columnar output:
  out_column_names  = ["date_time", "temperature", "humidity"]
  out_column_types  = [STRING, FLOAT, FLOAT]
  out_column_data   = [char**{...}, double*{...}, double*{...}]
  out_column_count  = 3
  out_row_count     = N
  |
  v
Julia/Dart: Unmarshal columnar arrays back to List<Map<String, Object?>>
  -> [{date_time: "2024-01-01", temperature: 20.5, humidity: 65.0}, ...]
```

## Patterns to Follow

### Pattern 1: Metadata-Driven Marshaling

**What:** Use `get_time_series_metadata` to discover column names and types at runtime, then marshal/unmarshal accordingly. Never hardcode column names in bindings.

**When:** Any time data crosses the C API boundary for variable-schema tables.

**Why:** The schema is defined by the user's SQL file, not by the library code. Column names and types are only known at runtime. Metadata provides the contract.

**Example (C API update implementation):**
```cpp
quiver_error_t quiver_database_update_time_series_group(
    quiver_database_t* db, const char* collection, const char* group, int64_t id,
    const char* const* column_names, const int* column_types,
    const void* const* column_data, size_t column_count, size_t row_count) {
    QUIVER_REQUIRE(db, collection, group);
    if (row_count > 0 && (!column_names || !column_types || !column_data)) {
        quiver_set_last_error("Null column arrays with non-zero row_count");
        return QUIVER_ERROR;
    }
    try {
        std::vector<std::map<std::string, quiver::Value>> rows;
        rows.reserve(row_count);
        for (size_t r = 0; r < row_count; ++r) {
            std::map<std::string, quiver::Value> row;
            for (size_t c = 0; c < column_count; ++c) {
                switch (column_types[c]) {
                case QUIVER_DATA_TYPE_INTEGER:
                    row[column_names[c]] = static_cast<const int64_t*>(column_data[c])[r];
                    break;
                case QUIVER_DATA_TYPE_FLOAT:
                    row[column_names[c]] = static_cast<const double*>(column_data[c])[r];
                    break;
                case QUIVER_DATA_TYPE_STRING:
                    row[column_names[c]] = std::string(
                        static_cast<const char* const*>(column_data[c])[r]);
                    break;
                }
            }
            rows.push_back(std::move(row));
        }
        db->db.update_time_series_group(collection, group, id, rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

### Pattern 2: Co-located Alloc/Free for Columnar Data

**What:** The read function allocates columnar arrays; the free function deallocates them. Both live in `src/c/database_time_series.cpp`, following the existing alloc/free co-location principle.

**When:** Always for C API functions that return allocated memory.

**Why:** The free function needs to know the column types to correctly deallocate each column's data array (string columns need per-element deletion; numeric columns are a single `delete[]`).

### Pattern 3: Reuse Existing Type Enum

**What:** Use the existing `quiver_data_type_t` enum (`QUIVER_DATA_TYPE_INTEGER=0, FLOAT=1, STRING=2`) for column types. Do not introduce a new enum.

**When:** Any new C API function that needs to express data types.

**Why:** Consistency with the parameterized query API. The enum is already defined in `include/quiver/c/database.h` and understood by all bindings.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Hardcoding Column Names in Bindings

**What:** Current Julia/Dart hardcode `"date_time"` and `"value"` as column names.

**Why bad:** Fails for schemas with different dimension column names (e.g., `date_recorded`) or value column names (e.g., `temperature`, `humidity`). The dimension column is discovered from metadata (`dimension_column` field), not assumed.

**Instead:** Query metadata to discover column names. The binding should work with any schema.

### Anti-Pattern 2: Row-Oriented void* Arrays Through FFI

**What:** Passing each cell as a `(type, void*)` pair -- `row_count * column_count` type+pointer pairs.

**Why bad:** FFI bindings (Julia, Dart) would need to construct per-cell `Ptr{Cvoid}` arrays. For 1000 rows x 5 columns, that is 5000 `void*` pointer allocations in the binding layer, each requiring boxing. Columnar approach: 5 typed arrays, each allocated once.

**Instead:** Columnar typed arrays. Each column is one contiguous native array.

### Anti-Pattern 3: Changing the C++ Interface

**What:** Modifying `Database::update_time_series_group` or `Database::read_time_series_group` to take/return columnar data.

**Why bad:** The C++ interface (`vector<map<string, Value>>`) is already correct and general. Lua depends on it working row-oriented. Changing it creates unnecessary churn in the C++ layer and Lua bindings.

**Instead:** Only change the C API marshaling. The C API is the translation layer between the columnar FFI-friendly format and the row-oriented C++ format.

### Anti-Pattern 4: Separate Functions Per Column Type Combination

**What:** Creating `quiver_database_update_time_series_group_floats`, `quiver_database_update_time_series_group_integers`, `quiver_database_update_time_series_group_mixed`, etc.

**Why bad:** The number of type combinations is combinatorial. A table with 3 columns could be (STRING, FLOAT, FLOAT), (STRING, INTEGER, FLOAT), (STRING, STRING, INTEGER), etc. The typed parallel array pattern handles all combinations uniformly.

**Instead:** One function with `column_types[]` that describes each column.

## Build Order

The implementation should follow this dependency order to maintain a working test suite at each step.

### Step 1: New C API Signatures (Header)

Modify `include/quiver/c/database.h` to declare the new function signatures. Keep the old signatures temporarily (with a comment marking them for removal) so existing code compiles.

**Files changed:** `include/quiver/c/database.h`
**Tests:** All existing tests still compile and pass (old signatures still present).

### Step 2: New C API Implementation

Implement the new columnar marshaling in `src/c/database_time_series.cpp`. Add helper functions to `src/c/database_helpers.h` if needed (e.g., `convert_columnar_to_rows`, `convert_rows_to_columnar`). Update the free function.

**Files changed:** `src/c/database_time_series.cpp`, `src/c/database_helpers.h`
**Tests:** Write new C API tests for multi-column schemas (`multi_time_series.sql`). Old single-column tests still pass via old signatures.

### Step 3: Migrate C API Tests to New Signatures

Update `tests/test_c_api_database_time_series.cpp` to use the new signatures. Remove old signatures from the header.

**Files changed:** `tests/test_c_api_database_time_series.cpp`, `include/quiver/c/database.h`
**Tests:** All C++ and C API tests pass. Bindings temporarily broken (expected).

### Step 4: Regenerate FFI Bindings

Run `bindings/julia/generator/generator.bat` and `bindings/dart/generator/generator.bat` to pick up the new C API signatures.

**Files changed:** `bindings/julia/src/c_api.jl`, `bindings/dart/lib/src/ffi/bindings.dart`
**Tests:** FFI layer updated, but binding logic still uses old calling convention (will fail).

### Step 5: Update Julia Bindings

Rewrite `update_time_series_group!` and `read_time_series_group` in Julia to use columnar marshaling via metadata.

**Files changed:** `bindings/julia/src/database_update.jl`, `bindings/julia/src/database_read.jl`
**Tests:** Julia tests pass.

### Step 6: Update Dart Bindings

Rewrite `updateTimeSeriesGroup` and `readTimeSeriesGroup` in Dart to use columnar marshaling via metadata.

**Files changed:** `bindings/dart/lib/src/database_update.dart`, `bindings/dart/lib/src/database_read.dart`
**Tests:** Dart tests pass.

### Step 7: Full Stack Verification

Run `scripts/build-all.bat` to confirm all layers pass.

## Integration Points

| Boundary | Before | After |
|----------|--------|-------|
| C API <-> C++ | C API marshals 2 fixed columns to `vector<map>` | C API marshals N typed columns to `vector<map>` |
| Julia <-> C API | Julia passes `Ptr{Ptr{Cchar}}` + `Ptr{Cdouble}` | Julia passes `Ptr{Ptr{Cchar}}` (names) + `Ptr{Cint}` (types) + `Ptr{Ptr{Cvoid}}` (data) |
| Dart <-> C API | Dart passes `Pointer<Pointer<Char>>` + `Pointer<Double>` | Dart passes names + types + `Pointer<Pointer<Void>>` (data) |
| Lua <-> C++ | Direct sol2 binding (unchanged) | Direct sol2 binding (unchanged) |
| Bindings <-> Metadata API | Bindings ignore metadata for writes | Bindings query metadata to determine column names/types before marshaling |

## Scalability Considerations

| Concern | Current | After Redesign |
|---------|---------|----------------|
| Column count | Fixed 2 (dimension + 1 value) | Arbitrary N columns |
| Column types | Float-only values | Integer, Float, String per column |
| Schemas supported | Only `(date_time TEXT, value REAL)` pattern | Any `{Collection}_time_series_{name}` schema |
| Row count | No limit (good) | No limit (unchanged) |
| Memory overhead | Minimal (2 flat arrays) | Proportional to column count (N flat arrays) |
| Binding complexity | Simple (hardcoded 2 arrays) | Moderate (metadata query + typed array construction) |

## Sources

- Direct codebase analysis: all source files listed in Component Boundaries table (HIGH confidence)
- Existing C API patterns: `database_query.cpp` typed parallel arrays (`convert_params`) (HIGH confidence)
- Existing metadata API: `GroupMetadata.dimension_column`, `GroupMetadata.value_columns` already provide all needed schema info (HIGH confidence)
- `multi_time_series.sql` schema: proves multi-column time series tables are already supported at schema and C++ level (HIGH confidence)

---
*Architecture research for: Quiver multi-column time series update interface redesign*
*Researched: 2026-02-12*
