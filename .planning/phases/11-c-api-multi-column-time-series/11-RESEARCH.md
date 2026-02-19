# Phase 11: C API Multi-Column Time Series - Research

**Researched:** 2026-02-19
**Domain:** C API FFI design for typed columnar time series data
**Confidence:** HIGH

## Summary

Phase 11 replaces the existing single-column C API time series functions (`update_time_series_group`, `read_time_series_group`, `free_time_series_data`) with new multi-column signatures that support N typed value columns. The C++ layer already supports multi-column time series via `vector<map<string, Value>>`, so this phase is purely a C API marshaling redesign. All existing C API time series tests must be rewritten to the new signatures. Julia and Dart bindings will break (updated in Phases 12/13).

The key challenge is designing a clean columnar FFI interface that handles mixed types (INTEGER, REAL, TEXT) per column, with correct memory management for the read side. The established `convert_params()` pattern from `database_query.cpp` provides the internal precedent for typed parallel arrays (`param_types[]`, `param_values[]`) through C FFI.

**Primary recommendation:** Use parallel arrays (column_names[], column_types[], column_data[]) for both update and read functions. For the read result, return a struct-of-arrays via out-parameters. The free function takes column_types[] so it knows how to deallocate each column (delete[] for numerics, per-element delete[] for strings).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Dimension column (date_time) is unified -- treated as just another column in `column_names[]`/`column_data[]` arrays, not a separate parameter
- C API discovers the dimension column from metadata internally (calls `get_time_series_metadata()`) -- caller passes columns in any order
- Dimension column is required: if caller omits it, return `QUIVER_ERROR` with message like `"Cannot update_time_series_group: dimension column 'date_time' missing from column_names"`
- Partial column updates allowed: caller can pass a subset of value columns (only dimension is mandatory)
- Replace existing functions in-place with new signatures (no `_multi`/`_v2` suffixes, no coexistence with old signatures)
- `quiver_database_update_time_series_group` -- new multi-column signature replaces old
- `quiver_database_read_time_series_group` -- new multi-column signature replaces old
- `quiver_database_free_time_series_data` -- new signature with `column_types` for type-aware deallocation
- All existing callers (C API tests, Julia bindings, Dart bindings) updated in their respective phases
- Binding names unchanged: Julia `read_time_series_group` / Dart `readTimeSeriesGroup` keep their names with new multi-column behavior
- Read returns column names alongside data (self-describing result, no extra metadata call needed by caller)
- Dimension column included as a regular column in output arrays (mirrors unified update approach)
- Column order matches schema definition order (as returned by metadata)
- Empty results: NULL pointers with count 0 (matches existing empty-result convention)
- Fail-fast: error on first bad column found
- Unknown column name: `"Cannot update_time_series_group: column 'temperature' not found in group 'data' for collection 'Items'"`
- Type mismatch: `"Cannot update_time_series_group: column 'status' has type TEXT but received INTEGER"`
- Follows existing error patterns (Pattern 1: `"Cannot {operation}: {reason}"`)

### Claude's Discretion
- Read result format: parallel out-arrays vs result struct (user said "you decide")
- Exact parameter order in new function signatures
- Columnar-to-row conversion implementation details
- Atomic allocation strategy for read-side leak prevention
- Helper function placement in `database_helpers.h`

### Deferred Ideas (OUT OF SCOPE)
- Transaction batching / combined create+time-series API -- the current two-step workflow (create_element then update_time_series_group) uses two separate SQLite transactions. A batch transaction API or combined operation could improve performance for bulk inserts. Defer to a future phase.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CAPI-01 | C API supports N typed value columns for time series update (columnar parallel arrays: column_names[], column_types[], column_data[], column_count, row_count) | Update signature design in Architecture Patterns section; convert_params() precedent from database_query.cpp; columnar-to-row conversion pattern |
| CAPI-02 | C API supports N typed value columns for time series read (returns columnar typed arrays matching schema) | Read signature design in Architecture Patterns section; row-to-columnar conversion pattern; self-describing result layout |
| CAPI-03 | Free function correctly deallocates variable-column read results (string columns: per-element cleanup; numeric columns: single delete[]) | Free function design with column_types[] parameter; type-aware deallocation strategy in Architecture Patterns |
| CAPI-04 | Column types use existing quiver_data_type_t enum (INTEGER, FLOAT, STRING) with no new type definitions | Verified: quiver_data_type_t already defines INTEGER=0, FLOAT=1, STRING=2, DATE_TIME=3, NULL=4; no changes needed to the enum |
| CAPI-05 | C API validates column names and types against schema metadata before processing, returning clear error messages on mismatch | Validation pattern using get_time_series_metadata(); error message formats specified in Locked Decisions |
| MIGR-01 | Multi-column test schema with mixed types (INTEGER + REAL + TEXT value columns) in tests/schemas/valid/ | New schema design documented; existing multi_time_series.sql only has single REAL columns per group |
</phase_requirements>

## Standard Stack

### Core
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| quiver_data_type_t | include/quiver/c/database.h | Type enum for columns | Already exists: INTEGER=0, FLOAT=1, STRING=2, DATE_TIME=3, NULL=4 |
| convert_params() | src/c/database_query.cpp | Type-dispatch on parallel arrays | Established pattern for typed void* arrays through C FFI |
| strdup_safe() | src/c/database_helpers.h | Safe string duplication | Standard helper for C API string allocation |
| QUIVER_REQUIRE | src/c/internal.h | Null argument validation macro | Max 6 args currently -- new signatures may need 7+ |
| GroupMetadata | include/quiver/attribute_metadata.h | Time series metadata with dimension_column | Used internally to discover dimension column and validate column names/types |

### Supporting
| Component | Location | Purpose | When to Use |
|-----------|----------|---------|-------------|
| convert_group_to_c() | src/c/database_helpers.h | C++ GroupMetadata to C struct | For internal metadata lookups during validation |
| to_c_data_type() | src/c/database_helpers.h | C++ DataType to quiver_data_type_t | For type comparison during validation |
| data_type_to_string() | include/quiver/data_type.h | DataType to string for error messages | For type mismatch error messages |

### Not Needed
| Instead of | Why Not |
|------------|---------|
| New type definitions | CAPI-04 requires reusing existing quiver_data_type_t |
| New C++ methods | C++ layer already handles multi-column via vector<map<string, Value>> |
| Result struct type | Parallel out-arrays keep the API consistent with existing read patterns |

## Architecture Patterns

### Pattern 1: New Update Signature
**What:** Replace the old `(date_times, values, row_count)` signature with columnar typed arrays.
**When to use:** `quiver_database_update_time_series_group`

Current signature (to be replaced):
```c
quiver_error_t quiver_database_update_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    const char* const* date_times,  // OLD: separate dimension param
    const double* values,           // OLD: single value column
    size_t row_count);
```

New signature:
```c
quiver_error_t quiver_database_update_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    const char* const* column_names,    // N column names (including dimension)
    const int* column_types,            // quiver_data_type_t per column
    const void* const* column_data,     // typed array per column
    size_t column_count,                // number of columns
    size_t row_count);                  // number of rows per column
```

**Caller example (3 columns: date_time TEXT, temperature REAL, status TEXT):**
```c
const char* col_names[] = {"date_time", "temperature", "status"};
int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_STRING};

const char* date_times[] = {"2024-01-01", "2024-01-02"};
double temps[] = {20.0, 21.5};
const char* statuses[] = {"ok", "warn"};

const void* col_data[] = {date_times, temps, statuses};

quiver_database_update_time_series_group(db, "Sensor", "readings", id,
    col_names, col_types, col_data, 3, 2);
```

**Implementation flow:**
1. QUIVER_REQUIRE null checks on db, collection, group
2. Early return if row_count == 0 and column_count == 0 (clear operation)
3. Validate column_count > 0 when row_count > 0
4. Get metadata via `db->db.get_time_series_metadata(collection, group)`
5. Validate dimension column present in column_names[]
6. Validate each column_name exists in metadata (dimension + value_columns)
7. Validate each column_type matches schema type (using to_c_data_type)
8. Convert columnar C arrays to `vector<map<string, Value>>` rows
9. Call `db->db.update_time_series_group(collection, group, id, rows)`

### Pattern 2: New Read Signature
**What:** Replace the old `(out_date_times, out_values, out_row_count)` with multi-column out-arrays.
**When to use:** `quiver_database_read_time_series_group`

**Recommendation for Claude's Discretion (read result format):** Use parallel out-arrays (not a struct), because:
- Consistent with existing read patterns (all other read functions use out-params)
- No new type definitions needed
- Self-describing: column names and types returned alongside data

New signature:
```c
quiver_error_t quiver_database_read_time_series_group(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    int64_t id,
    char*** out_column_names,       // [out] array of column name strings
    int** out_column_types,         // [out] array of quiver_data_type_t
    void*** out_column_data,        // [out] array of typed data arrays
    size_t* out_column_count,       // [out] number of columns
    size_t* out_row_count);         // [out] number of rows
```

**Implementation flow:**
1. QUIVER_REQUIRE null checks (note: 6+ args -- may need to split or extend macro)
2. Get metadata for column order (schema definition order per decision)
3. Call `db->db.read_time_series_group(collection, group, id)`
4. If empty: set all out-params to NULL/0, return QUIVER_OK
5. Allocate column_names[], column_types[], column_data[] arrays (column_count entries each)
6. For each column, allocate typed array:
   - INTEGER columns: `new int64_t[row_count]`
   - FLOAT columns: `new double[row_count]`
   - STRING/DATE_TIME columns: `new char*[row_count]` with `strdup_safe` per element
7. Fill data from rows into columnar arrays
8. Return QUIVER_OK

**Caller example:**
```c
char** col_names = NULL;
int* col_types = NULL;
void** col_data = NULL;
size_t col_count = 0, row_count = 0;

quiver_database_read_time_series_group(db, "Sensor", "readings", id,
    &col_names, &col_types, &col_data, &col_count, &row_count);

// Access temperature column (index 1, type FLOAT)
double* temps = (double*)col_data[1];
printf("First temp: %f\n", temps[0]);

// Access status column (index 2, type STRING)
char** statuses = (char**)col_data[2];
printf("First status: %s\n", statuses[0]);

quiver_database_free_time_series_data(col_names, col_types, col_data, col_count, row_count);
```

### Pattern 3: New Free Signature
**What:** Type-aware deallocation of read results.
**When to use:** `quiver_database_free_time_series_data`

New signature:
```c
quiver_error_t quiver_database_free_time_series_data(
    char** column_names,
    int* column_types,
    void** column_data,
    size_t column_count,
    size_t row_count);
```

**Implementation flow:**
```c
// Free column names
for (i = 0; i < column_count; i++) delete[] column_names[i];
delete[] column_names;

// Free column data based on type
for (i = 0; i < column_count; i++) {
    switch (column_types[i]) {
    case QUIVER_DATA_TYPE_INTEGER:
        delete[] (int64_t*)column_data[i];
        break;
    case QUIVER_DATA_TYPE_FLOAT:
        delete[] (double*)column_data[i];
        break;
    case QUIVER_DATA_TYPE_STRING:
    case QUIVER_DATA_TYPE_DATE_TIME:
        char** strings = (char**)column_data[i];
        for (j = 0; j < row_count; j++) delete[] strings[j];
        delete[] strings;
        break;
    }
}
delete[] column_data;

// Free types array
delete[] column_types;
```

### Pattern 4: Columnar-to-Row Conversion (Update Side)
**What:** Convert caller's columnar C arrays to C++ `vector<map<string, Value>>`.
**When to use:** Inside `quiver_database_update_time_series_group` implementation.

```cpp
// Build rows from columnar data
std::vector<std::map<std::string, Value>> rows;
rows.reserve(row_count);

for (size_t r = 0; r < row_count; ++r) {
    std::map<std::string, Value> row;
    for (size_t c = 0; c < column_count; ++c) {
        const char* col_name = column_names[c];
        switch (column_types[c]) {
        case QUIVER_DATA_TYPE_INTEGER:
            row[col_name] = static_cast<const int64_t*>(column_data[c])[r];
            break;
        case QUIVER_DATA_TYPE_FLOAT:
            row[col_name] = static_cast<const double*>(column_data[c])[r];
            break;
        case QUIVER_DATA_TYPE_STRING:
        case QUIVER_DATA_TYPE_DATE_TIME:
            row[col_name] = std::string(static_cast<const char* const*>(column_data[c])[r]);
            break;
        }
    }
    rows.push_back(std::move(row));
}
```

### Pattern 5: Row-to-Columnar Conversion (Read Side)
**What:** Convert C++ `vector<map<string, Value>>` to columnar C arrays.
**When to use:** Inside `quiver_database_read_time_series_group` implementation.

**Recommendation for Claude's Discretion (atomic allocation strategy):**
Use a try/catch block around all allocations. On failure, clean up everything already allocated before rethrowing. This is simpler than RAII wrappers for a C API function and matches the existing code style (no custom RAII in C API layer).

```cpp
// Get column info from metadata (determines output order)
auto metadata = db->db.get_time_series_metadata(collection, group);
// Build column list: dimension first, then value columns (schema order)
std::vector<std::pair<std::string, quiver_data_type_t>> columns;
columns.push_back({metadata.dimension_column, QUIVER_DATA_TYPE_STRING}); // dimension is always TEXT/DATE_TIME
for (const auto& vc : metadata.value_columns) {
    columns.push_back({vc.name, to_c_data_type(vc.data_type)});
}

size_t col_count = columns.size();
*out_column_count = col_count;
*out_row_count = rows.size();

// Allocate outer arrays
*out_column_names = new char*[col_count];
*out_column_types = new int[col_count];
*out_column_data = new void*[col_count];

// Allocate per-column data arrays and fill
for (size_t c = 0; c < col_count; ++c) {
    (*out_column_names)[c] = strdup_safe(columns[c].first);
    (*out_column_types)[c] = columns[c].second;

    switch (columns[c].second) {
    case QUIVER_DATA_TYPE_INTEGER: {
        auto* arr = new int64_t[rows.size()];
        for (size_t r = 0; r < rows.size(); ++r) {
            arr[r] = std::get<int64_t>(rows[r].at(columns[c].first));
        }
        (*out_column_data)[c] = arr;
        break;
    }
    case QUIVER_DATA_TYPE_FLOAT: {
        auto* arr = new double[rows.size()];
        for (size_t r = 0; r < rows.size(); ++r) {
            // Handle both int64_t and double from C++ layer
            auto& val = rows[r].at(columns[c].first);
            if (std::holds_alternative<double>(val))
                arr[r] = std::get<double>(val);
            else if (std::holds_alternative<int64_t>(val))
                arr[r] = static_cast<double>(std::get<int64_t>(val));
        }
        (*out_column_data)[c] = arr;
        break;
    }
    case QUIVER_DATA_TYPE_STRING:
    case QUIVER_DATA_TYPE_DATE_TIME: {
        auto** arr = new char*[rows.size()];
        for (size_t r = 0; r < rows.size(); ++r) {
            arr[r] = strdup_safe(std::get<std::string>(rows[r].at(columns[c].first)));
        }
        (*out_column_data)[c] = arr;
        break;
    }
    }
}
```

### Pattern 6: Schema Validation (Update Side)
**What:** Validate caller's column names and types against metadata before processing.
**When to use:** At the start of `quiver_database_update_time_series_group`, before conversion.

```cpp
auto metadata = db->db.get_time_series_metadata(collection, group);

// Build a lookup: column_name -> expected quiver_data_type_t
std::map<std::string, quiver_data_type_t> schema_columns;
// Dimension column is STRING/DATE_TIME type
schema_columns[metadata.dimension_column] = QUIVER_DATA_TYPE_STRING;
for (const auto& vc : metadata.value_columns) {
    schema_columns[vc.name] = to_c_data_type(vc.data_type);
}

// Check dimension column is present
bool has_dimension = false;
for (size_t i = 0; i < column_count; ++i) {
    if (column_names[i] == metadata.dimension_column) {
        has_dimension = true;
        break;
    }
}
if (!has_dimension) {
    throw std::runtime_error(
        "Cannot update_time_series_group: dimension column '" +
        metadata.dimension_column + "' missing from column_names");
}

// Validate each column
for (size_t i = 0; i < column_count; ++i) {
    std::string name(column_names[i]);
    auto it = schema_columns.find(name);
    if (it == schema_columns.end()) {
        throw std::runtime_error(
            "Cannot update_time_series_group: column '" + name +
            "' not found in group '" + group + "' for collection '" + collection + "'");
    }
    // Type check (DATE_TIME accepts STRING)
    auto expected = it->second;
    auto actual = static_cast<quiver_data_type_t>(column_types[i]);
    bool type_ok = (expected == actual)
        || (expected == QUIVER_DATA_TYPE_DATE_TIME && actual == QUIVER_DATA_TYPE_STRING)
        || (expected == QUIVER_DATA_TYPE_STRING && actual == QUIVER_DATA_TYPE_DATE_TIME);
    if (!type_ok) {
        throw std::runtime_error(
            "Cannot update_time_series_group: column '" + name + "' has type " +
            quiver::data_type_to_string(/* reverse map */) + " but received " + /* ... */);
    }
}
```

### Pattern 7: Multi-Column Test Schema (MIGR-01)
**What:** New test schema with mixed INTEGER + REAL + TEXT value columns.
**Where:** `tests/schemas/valid/mixed_time_series.sql`

```sql
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Sensor (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Sensor_time_series_readings (
    id INTEGER NOT NULL REFERENCES Sensor(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity INTEGER NOT NULL,
    status TEXT NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

This schema enables testing:
- REAL column (temperature)
- INTEGER column (humidity)
- TEXT column (status)
- All three types in a single time series group
- Validates CAPI-04 (uses existing quiver_data_type_t for all three)

### Anti-Patterns to Avoid
- **Separate dimension parameter:** Decision is locked -- dimension is unified in column_names[]. Do NOT add a separate `date_times` parameter alongside `column_data`.
- **New type definitions:** CAPI-04 prohibits adding new types. Reuse quiver_data_type_t.
- **Backward compatibility shims:** Decision is to replace in-place. No old signature coexistence.
- **Struct-based result type:** Would require a new typedef and break consistency with existing out-parameter patterns. Use parallel out-arrays.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Type dispatch on void* columns | Custom dispatch code per function | Reuse convert_params() switch pattern | Already proven in database_query.cpp |
| Metadata lookup | Inline SQL queries | db->db.get_time_series_metadata() | C++ layer already provides this |
| Column type to string | Manual if/else | data_type_to_string() from data_type.h | Existing utility function |
| C++ to C type mapping | Manual switch | to_c_data_type() from database_helpers.h | Already handles all DataType variants |

**Key insight:** The C++ layer already handles multi-column time series natively. This phase is purely marshaling work at the C API boundary.

## Common Pitfalls

### Pitfall 1: QUIVER_REQUIRE Macro Arg Limit
**What goes wrong:** The new read signature has 9 parameters. QUIVER_REQUIRE only supports up to 6 arguments.
**Why it happens:** The macro uses a counting trick that tops out at 6.
**How to avoid:** Either extend the macro to support more args (add QUIVER_REQUIRE_7, _8, _9 etc.), or split the null checks into two QUIVER_REQUIRE calls (e.g., first 5 args, then remaining args). The split approach is simpler and non-breaking.
**Warning signs:** Compilation errors in the QUIVER_REQUIRE macro expansion.

### Pitfall 2: Memory Leak on Partial Read Allocation Failure
**What goes wrong:** If allocation fails mid-way through column data arrays, previously allocated arrays leak.
**Why it happens:** The read function allocates N column arrays sequentially. If the 3rd allocation throws, columns 1 and 2 are leaked.
**How to avoid:** Wrap the entire allocation block in try/catch. On exception, iterate through already-allocated columns and free them using the same type-dispatch logic as the free function. Alternatively, allocate all outer arrays first, initialize data pointers to nullptr, then fill -- on exception, free everything that's non-null.
**Warning signs:** Valgrind/ASAN reports on error paths.

### Pitfall 3: Type Mismatch Between SQLite Storage and C API Type
**What goes wrong:** C++ reads an INTEGER from SQLite but the schema says REAL, or vice versa. The variant holds the wrong alternative, causing std::get to throw.
**Why it happens:** SQLite uses dynamic typing. A REAL column might store an exact integer value that SQLite returns as INTEGER type.
**How to avoid:** In the row-to-columnar conversion (read side), handle both int64_t and double variants for REAL columns (already done in existing code with static_cast fallback). Similarly, handle potential int64_t values in INTEGER columns that SQLite might return as doubles for large values.
**Warning signs:** std::bad_variant_access exceptions during read.

### Pitfall 4: DATE_TIME vs STRING Type Equivalence
**What goes wrong:** The schema metadata returns DATE_TIME for dimension columns (date_ prefix inference), but callers pass QUIVER_DATA_TYPE_STRING in column_types[].
**Why it happens:** `is_date_time_column()` in data_type.h infers DATE_TIME for columns starting with "date_". But from the caller's perspective, these are just strings.
**How to avoid:** In validation, treat DATE_TIME and STRING as compatible types. Both map to `const char*` in the C API. The type check should accept STRING where DATE_TIME is expected and vice versa.
**Warning signs:** False validation errors on dimension column type mismatch.

### Pitfall 5: Empty Column List with Non-Zero Row Count
**What goes wrong:** Caller passes row_count > 0 but column_count == 0 (or vice versa).
**Why it happens:** Inconsistent caller state.
**How to avoid:** Add explicit validation: if row_count > 0, column_count must be > 0. If column_count > 0, column_names/column_types/column_data must not be NULL.
**Warning signs:** Segfault on null dereference.

### Pitfall 6: Binding Generators Must Be Re-Run
**What goes wrong:** Julia/Dart FFI bindings reference old function signatures and fail to compile/link.
**Why it happens:** The C API header changes but generated bindings are stale.
**How to avoid:** After changing `include/quiver/c/database.h`, re-run both binding generators (`bindings/julia/generator/generator.bat` and `bindings/dart/generator/generator.bat`). However, note that Phase 11 only updates C API + C API tests. Phases 12/13 handle binding updates. The generators need to run in those phases, not Phase 11.
**Warning signs:** Link errors in binding test suites.

### Pitfall 7: Column Order Assumption
**What goes wrong:** Read output column order doesn't match schema definition order.
**Why it happens:** Using std::map iteration order (alphabetical by column name) instead of schema definition order.
**How to avoid:** Build the column list from metadata explicitly: dimension column first, then value_columns in metadata order. The C++ `get_time_series_metadata()` returns value_columns in schema definition order (it iterates `table_def->columns` which is an ordered map from SQLite column parsing).
**Warning signs:** Test assertions fail because column indices don't match expected order.

## Code Examples

### Example 1: Existing convert_params() Pattern (Established Precedent)
```cpp
// Source: src/c/database_query.cpp
static std::vector<quiver::Value>
convert_params(const int* param_types, const void* const* param_values, size_t param_count) {
    std::vector<quiver::Value> params;
    params.reserve(param_count);
    for (size_t i = 0; i < param_count; ++i) {
        switch (param_types[i]) {
        case QUIVER_DATA_TYPE_INTEGER:
            params.emplace_back(*static_cast<const int64_t*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_FLOAT:
            params.emplace_back(*static_cast<const double*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_STRING:
            params.emplace_back(std::string(static_cast<const char*>(param_values[i])));
            break;
        case QUIVER_DATA_TYPE_NULL:
            params.emplace_back(nullptr);
            break;
        }
    }
    return params;
}
```

### Example 2: Existing Read Result Handling (Empty Convention)
```cpp
// Source: src/c/database_time_series.cpp (current -- to be replaced)
if (rows.empty()) {
    *out_date_times = nullptr;
    *out_values = nullptr;
    *out_row_count = 0;
    return QUIVER_OK;
}
```

New pattern for multi-column empty result:
```cpp
if (rows.empty()) {
    *out_column_names = nullptr;
    *out_column_types = nullptr;
    *out_column_data = nullptr;
    *out_column_count = 0;
    *out_row_count = 0;
    return QUIVER_OK;
}
```

### Example 3: Type Validation Helper
```cpp
// New helper for database_time_series.cpp
static const char* c_type_name(int type) {
    switch (type) {
    case QUIVER_DATA_TYPE_INTEGER: return "INTEGER";
    case QUIVER_DATA_TYPE_FLOAT: return "FLOAT";
    case QUIVER_DATA_TYPE_STRING: return "STRING";
    case QUIVER_DATA_TYPE_DATE_TIME: return "DATE_TIME";
    default: return "UNKNOWN";
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hardcoded (date_times, values) pair | N typed columnar arrays | Phase 11 (this phase) | Enables multi-column time series through C API |
| Separate dimension parameter | Unified column_names[] with dimension discovered from metadata | Phase 11 (this phase) | Simpler caller API, callers don't need to know which column is the dimension |

**What stays unchanged:**
- C++ layer: `vector<map<string, Value>>` interface is not modified
- quiver_data_type_t enum: no changes
- Time series files functions: unchanged (they already use flexible column patterns)
- Metadata functions: unchanged (already return multi-column metadata)

## Open Questions

1. **Column order in update: does schema order matter or can caller pass any order?**
   - What we know: The decision says "column order matches schema definition order" for read output. For update, the conversion to `map<string, Value>` is order-independent (map keys).
   - What's unclear: Whether we should validate that caller's column order matches schema order on update.
   - Recommendation: Don't validate order on update. The map conversion is order-agnostic. The "schema definition order" constraint applies only to read output. This matches the decision: "caller passes columns in any order."

2. **DATE_TIME type in column_types[]: should callers use STRING or DATE_TIME?**
   - What we know: Dimension columns start with "date_" which causes schema to report DATE_TIME type. But DATE_TIME maps to TEXT in SQLite and `const char*` in C API.
   - What's unclear: Whether callers should pass QUIVER_DATA_TYPE_DATE_TIME or QUIVER_DATA_TYPE_STRING for the dimension column.
   - Recommendation: Accept both. In validation, treat STRING and DATE_TIME as interchangeable for string-typed columns. Document that either is acceptable.

3. **Clear operation: what signature for clearing all rows?**
   - What we know: Current API allows `(nullptr, nullptr, 0)` to clear rows. New API has more parameters.
   - What's unclear: Whether clearing requires `(NULL, NULL, NULL, 0, 0)` or some other convention.
   - Recommendation: Allow column_count == 0 and row_count == 0 with all column arrays NULL. This mirrors the existing empty-update convention and the locked decision.

## Sources

### Primary (HIGH confidence)
- `src/c/database_time_series.cpp` -- Current implementation being replaced (lines 1-279)
- `src/c/database_query.cpp` -- convert_params() pattern for typed parallel arrays (lines 10-36)
- `src/c/database_helpers.h` -- Existing helpers (strdup_safe, to_c_data_type, convert_group_to_c)
- `include/quiver/c/database.h` -- Current C API header with quiver_data_type_t enum
- `src/database_time_series.cpp` -- C++ implementation of multi-column read/update
- `include/quiver/attribute_metadata.h` -- GroupMetadata struct with dimension_column
- `include/quiver/data_type.h` -- DataType enum, is_date_time_column(), data_type_to_string()
- `include/quiver/value.h` -- Value = variant<nullptr_t, int64_t, double, string>

### Test Files (HIGH confidence)
- `tests/test_c_api_database_time_series.cpp` -- 14 existing C API tests to be rewritten
- `tests/test_database_time_series.cpp` -- C++ tests (unchanged, serve as reference)
- `tests/schemas/valid/collections.sql` -- Current single-column time series schema
- `tests/schemas/valid/multi_time_series.sql` -- Multi-group schema (single column per group)

### Architecture References (HIGH confidence)
- `src/c/internal.h` -- QUIVER_REQUIRE macro (max 6 args) and quiver_database struct
- CLAUDE.md project instructions -- Error patterns, naming conventions, co-location rules

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- All components already exist in the codebase
- Architecture: HIGH -- Patterns derived directly from existing code (convert_params, read helpers)
- Pitfalls: HIGH -- Identified from concrete code analysis (macro limit, type mapping, memory management)

**Research date:** 2026-02-19
**Valid until:** Indefinite -- this is internal codebase architecture, not external dependency
