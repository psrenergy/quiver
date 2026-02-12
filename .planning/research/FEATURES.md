# Feature Landscape: Time Series Update Interface Redesign

**Domain:** Multi-column time series update API across C++/C/Julia/Dart/Lua
**Researched:** 2026-02-12
**Confidence:** HIGH (direct codebase analysis, established FFI patterns, existing internal precedents)

## Problem Statement

The current `update_time_series_group` C API is hardcoded to exactly 2 parallel arrays: one `const char* const*` for the dimension column (date_time strings) and one `const double*` for a single value column. The C++ layer already supports N columns via `vector<map<string, Value>>`, but the C API bottleneck prevents bindings from passing multi-column time series data.

**Current C API signature:**
```c
quiver_database_update_time_series_group(db, collection, group, id,
    const char* const* date_times,  // hardcoded: 1 string dimension column
    const double* values,            // hardcoded: 1 float value column
    size_t row_count);
```

**Target:** Support schemas like `(id, date_time, temperature REAL, humidity REAL, status TEXT)` -- N value columns of mixed types, matching the C++ layer's existing capability.

**Symmetry goal:** The read side (`read_time_series_group`) has the same limitation and must be redesigned in lockstep.

## Table Stakes

Features the redesigned API must have. Missing any of these means the redesign is incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **N typed value columns through C API** | The C++ layer already handles N columns. The C API must not be the bottleneck. Without this, multi-column time series schemas are unusable from any binding. | HIGH | Core challenge of this milestone. Requires new C API signature or new opaque type. |
| **Matching read-side redesign** | If update supports N columns, read must return N columns. Otherwise data written through the new API cannot be read back through the same API. | HIGH | Read and update must be redesigned together. Cannot ship one without the other. |
| **Type safety for each column** | Each value column has a schema-defined type (INTEGER, REAL, TEXT). The C API must accept typed data per column, not cast everything to double. | MEDIUM | Current API loses INTEGER precision (casts to double). Current API cannot handle TEXT value columns at all. |
| **Column name association** | Caller must specify which column each data array belongs to. Positional-only approaches break when schema column order changes. | MEDIUM | Names provide self-documenting calls and decouple from schema column ordering. |
| **Dimension column flexibility** | The dimension column name comes from metadata (e.g., `date_time`). The API should not hardcode the name "date_time". | LOW | C++ already uses metadata to find the dimension column. C API must pass it through. |
| **Empty row set (clear)** | Calling with 0 rows must delete all existing time series data for that element, matching current behavior. | LOW | Already works in C++ layer. New C API must preserve this. |
| **Consistent binding ergonomics** | Julia kwargs, Dart Map, Lua table -- each binding should feel natural in its language while using the same C API underneath. | MEDIUM | Target: `update_time_series_group!(db, "Sensor", "temperature", id; date_time=[...], temperature=[...])` in Julia. |
| **Backward-compatible C++ signature** | The C++ `update_time_series_group(collection, group, id, vector<map<string, Value>>)` already works for N columns. Do not change it. | LOW | Zero work needed at C++ layer. This is a C API + bindings change. |
| **Free function for read results** | New read signature returns new data structures. Must provide matching free function(s). | MEDIUM | Follow alloc/free co-location pattern from `database_time_series.cpp`. |
| **Test schema with multi-column time series** | Need a schema like `(id, date_time, temperature REAL, humidity REAL)` to actually test the multi-column path. | LOW | `multi_time_series.sql` exists but has single-column groups. Need a multi-value-column group schema. |

## Differentiators

Features that make the redesign feel polished rather than just functional.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Columnar input (arrays of columns, not arrays of rows)** | Columnar layout matches how scientific/time-series data is typically stored in memory (separate arrays per measurement). Row-of-maps is the C++ internal representation; columnar is the natural FFI representation. | LOW | This is actually simpler for FFI than row-based. Each column is one contiguous array. Column names + types + arrays = complete. |
| **Metadata-driven validation in C API** | The C API layer can call `get_time_series_metadata` to validate column names and types before passing to C++. Catches errors at the FFI boundary with clear messages. | LOW | Metadata system already exists. Just wire validation into the new update function. |
| **Consistent pattern with `quiver_element_t` builder** | The Element builder (create/set/set/set/use/destroy) is an established pattern for multi-typed data through FFI. Could reuse or extend this for time series. | MEDIUM | Two viable approaches: (A) new opaque builder type, (B) parallel arrays with type descriptors. See Architecture Decisions below. |
| **Integer value column support** | Current API casts all values to double. New API should natively support INTEGER columns, preserving int64_t precision. | LOW | `Value` variant already supports `int64_t`. Just need typed column arrays in C API. |
| **String value column support** | Time series tables can have TEXT value columns (e.g., status codes, categories). Current API cannot represent these at all. | LOW | Same approach as string arrays elsewhere in C API. |
| **Binding-level kwargs decomposition** | Julia/Dart bindings decompose kwargs/Map into the columnar C API calls. The binding does the type dispatch based on Julia/Dart native types. Caller never thinks about C types. | MEDIUM | Follows existing `create_element!` pattern where Julia kwargs -> Element builder -> C API. |

## Anti-Features

Features to explicitly NOT build during this redesign.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Row-based C API (array of structs)** | Requires a new struct type with variant fields, complex memory management, and awkward FFI marshaling. Columnar is simpler for both C API and bindings. | Use columnar approach: parallel arrays per column with type tags. |
| **Append semantics** | "Add rows to existing time series without replacing." Complicates the API (upsert? merge? conflict resolution?) and the SQL generation. | Keep replace-all semantics. Caller reads, modifies, writes back. Simple and predictable. If append is needed later, add `append_time_series_group` as a separate function. |
| **Partial column update** | "Update only the temperature column, leave humidity unchanged." Requires per-column UPDATE instead of DELETE+INSERT, dramatically complicates SQL generation. | Keep full-group replacement. All columns must be provided for all rows. Partial update is a different operation. |
| **Time range filtering on update** | "Replace only rows in date range X-Y." Makes the DELETE clause conditional, adds complexity. | Caller reads full data, modifies desired range, writes back full data. Or use raw `query_*` for surgical updates. |
| **Automatic type coercion** | "Accept int array for REAL column and auto-convert." Type coercion hides bugs. If schema says REAL and caller passes INTEGER, that should either error or the binding should handle the conversion explicitly. | Bindings handle language-level coercion (Julia `Real` -> `Float64`). C API expects exact types. C++ layer already handles Value variant correctly. |
| **Streaming/chunked update** | "Send rows in batches for large time series." Adds transaction management complexity across multiple C API calls. | Single-call replace with all rows. For truly large datasets, use `import_csv` or raw SQL through `query_*`. |
| **Combined read+update (patch)** | "Read-modify-write in a single atomic operation." Requires exposing transactions through C API. | Keep read and update as separate operations. SQLite WAL mode provides isolation. |

## Architecture Decisions

### Critical Decision: C API Design for N Typed Columns

Three approaches were analyzed. The recommended approach is **Option B: Columnar Parallel Arrays**.

#### Option A: Opaque Builder Type (like `quiver_element_t`)

```c
// New opaque type
quiver_time_series_data_t* data = nullptr;
quiver_time_series_data_create(&data, row_count);
quiver_time_series_data_set_string_column(data, "date_time", date_times, row_count);
quiver_time_series_data_set_float_column(data, "temperature", temperatures, row_count);
quiver_time_series_data_set_float_column(data, "humidity", humidities, row_count);
quiver_database_update_time_series_group(db, collection, group, id, data);
quiver_time_series_data_destroy(data);
```

**Pros:** Clean, type-safe, extensible. Follows existing Element pattern.
**Cons:** New opaque type = new header, new internal struct, new create/destroy lifecycle, new generator output, new binding wrappers. Significant surface area increase. Every binding needs a builder wrapper class.

#### Option B: Columnar Parallel Arrays (RECOMMENDED)

```c
// column_names[i], column_types[i], column_values[i] describe each column
const char* column_names[] = {"date_time", "temperature", "humidity"};
const int column_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_FLOAT};
const void* const column_values[] = {date_times_array, temperatures_array, humidities_array};
size_t column_count = 3;
size_t row_count = 100;

quiver_database_update_time_series_group(db, collection, group, id,
    column_names, column_types, column_values, column_count, row_count);
```

**Pros:** Single function call. No new opaque types. Follows the exact pattern already used by `quiver_database_query_*_params` for typed parallel arrays. Bindings already know how to marshal `int* types` + `void** values`. Minimal new C API surface. Mechanically translatable in Julia/Dart/Lua.
**Cons:** `void*` arrays require careful casting. Caller must ensure array lengths match `row_count`. Less type-safe than builder pattern.

**Why recommended:** This project has an established precedent in `convert_params()` from `database_query.cpp` -- it already converts `int* param_types` + `void* const* param_values` into `vector<Value>`. The time series update is the same pattern applied to columnar data instead of row parameters. No new concepts, no new types, no new lifecycle management. The complexity is contained in one C API function and its binding-layer marshaling.

#### Option C: JSON/String Serialization

```c
// Serialize to JSON string, parse in C API
const char* json = "[{\"date_time\":\"2024-01-01\",\"temperature\":1.5}]";
quiver_database_update_time_series_group_json(db, collection, group, id, json);
```

**Pros:** Trivial C API signature. Any language can produce JSON.
**Cons:** Requires JSON parser dependency (or hand-rolled parsing). Serialization/deserialization overhead. Loses type safety entirely. Violates project principle of "no unnecessary dependencies."

### Read Side Redesign

The read side must return N typed columns. Two sub-options:

**Option B-Read (matches Option B): Columnar parallel arrays as output**

```c
char** out_column_names = nullptr;
int* out_column_types = nullptr;
void** out_column_values = nullptr;  // Each void* points to typed array
size_t out_column_count = 0;
size_t out_row_count = 0;

quiver_database_read_time_series_group(db, collection, group, id,
    &out_column_names, &out_column_types, &out_column_values,
    &out_column_count, &out_row_count);

// Free
quiver_database_free_time_series_data(
    out_column_names, out_column_types, out_column_values,
    out_column_count, out_row_count);
```

This is complex but follows naturally from Option B. The free function must know column types to free string arrays (delete[] each string) vs numeric arrays (delete[] the array).

**Simplification: Metadata-aware free.** The free function receives `column_types` and `column_count`, so it knows which `void*` entries are `char**` (need per-string cleanup) vs `int64_t*`/`double*` (single delete[]).

## Feature Dependencies

```
[Multi-column test schema]
    |
    +--required by--> [C++ layer testing] (already works, just needs test)
    |
    +--required by--> [C API redesign testing]

[C API update_time_series_group redesign]
    |
    +--depends on--> [Existing metadata system] (get_time_series_metadata)
    +--depends on--> [Existing quiver_data_type_t enum]
    +--depends on--> [Existing convert_params pattern from database_query.cpp]
    |
    +--must ship with--> [C API read_time_series_group redesign]
    +--must ship with--> [C API free_time_series_data redesign]
    |
    +--enables--> [Julia binding update]
    +--enables--> [Dart binding update]
    +--enables--> [Lua binding update] (Lua bypasses C API, but consistency matters)
    +--enables--> [Generator re-run for new C API signature]

[Julia kwargs interface]
    |
    +--depends on--> [C API redesign]
    +--depends on--> [Julia c_api.jl regeneration]
    +--uses--> [Existing create_element! kwargs pattern as template]

[Dart Map interface]
    |
    +--depends on--> [C API redesign]
    +--depends on--> [Dart bindings.dart regeneration]
    +--uses--> [Existing createElement Map pattern as template]

[Lua table interface]
    |
    +--depends on--> [C++ layer] (Lua calls C++ directly, not C API)
    +--note--> Already works for N columns. Just needs kwargs-style sugar.
```

### Dependency Notes

- **C++ layer needs zero changes.** `update_time_series_group(collection, group, id, vector<map<string, Value>>)` already accepts N columns of any type. The redesign is entirely C API + bindings.
- **Read and update must ship together.** A multi-column update with a single-column read would leave data inaccessible through the API.
- **Generators must re-run** after C API header changes. Julia `generator.bat` and Dart `generator.bat` produce the FFI layer. High-level wrappers are then updated manually.
- **Lua is special:** `lua_runner.cpp` calls C++ directly, not the C API. The `update_time_series_group_from_lua` function already converts Lua tables to `vector<map<string, Value>>`. It already works for N columns. Only the ergonomic sugar (kwargs-style) needs adding.
- **Multi-column test schema is the prerequisite.** Without a schema like `(id, date_time TEXT, temperature REAL, humidity REAL, status TEXT)`, there is nothing to test the multi-column path against.

## MVP Recommendation

### Phase 1: Foundation (must complete first)

1. **Multi-column test schema** -- Add `tests/schemas/valid/multi_column_time_series.sql` with `(id, date_time TEXT, temperature REAL, humidity REAL)`. Test C++ layer with it to confirm it already works.
2. **C API `update_time_series_group` redesign** -- Replace 2-array signature with columnar parallel arrays (Option B). Implement conversion from columnar arrays to `vector<map<string, Value>>` in the C API layer.
3. **C API `read_time_series_group` redesign** -- Replace 2-array output with columnar parallel array output. Implement conversion from `vector<map<string, Value>>` to columnar arrays.
4. **C API `free_time_series_data` redesign** -- New free function that handles N typed columns.
5. **C++ and C API tests** -- Test both single-column (backward compat behavior) and multi-column scenarios.

### Phase 2: Bindings (depends on Phase 1)

6. **Regenerate FFI layers** -- Run `generator.bat` for Julia and Dart.
7. **Julia high-level wrapper** -- Add kwargs-style `update_time_series_group!(db, col, group, id; date_time=[...], temp=[...])` and update `read_time_series_group` to return multi-column Dicts.
8. **Dart high-level wrapper** -- Add Map-based `updateTimeSeriesGroup(col, group, id, {'date_time': [...], 'temperature': [...]})` and update `readTimeSeriesGroup`.
9. **Lua ergonomics** -- Already works for N columns. Add documentation/test coverage for multi-column usage.
10. **Binding tests** -- Julia, Dart, Lua tests using multi-column schema.

### Defer

- **Append semantics** -- Different operation, different milestone.
- **Partial column update** -- Different operation, different milestone.
- **Integer dimension columns** -- All current schemas use TEXT dimension columns (date_time). Integer dimensions (e.g., sequence numbers) could be added later but are not the motivating use case.

## Complexity Assessment

| Component | Complexity | Rationale |
|-----------|------------|-----------|
| C++ layer changes | NONE | Already handles N typed columns via `vector<map<string, Value>>`. |
| Multi-column test schema | LOW | One SQL file. |
| C API update signature | MEDIUM | New parallel-array marshaling. Pattern exists in `database_query.cpp`. ~100 lines of new conversion code. |
| C API read signature | HIGH | Must allocate N typed arrays, track types for freeing. More complex than update because of output allocation. ~150 lines. |
| C API free function | MEDIUM | Must iterate columns, switch on type to free correctly. ~50 lines. |
| Julia binding | MEDIUM | kwargs decomposition to parallel arrays. Follows `create_element!` pattern but with array values instead of scalars. ~80 lines. |
| Dart binding | MEDIUM | Map decomposition to native memory. Follows `createElement` pattern. Arena-based allocation simplifies cleanup. ~100 lines. |
| Lua binding | LOW | Already works. Just verify multi-column test passes. ~10 lines of test. |
| Generator re-runs | LOW | Mechanical. Run bat files, verify output. |

**Total estimated effort:** Medium. The core challenge is the C API read-side output allocation (returning N typed arrays through out-parameters). Everything else follows established patterns.

## Existing Internal Precedents

These patterns within the Quiver codebase directly inform the redesign:

| Precedent | Where | What It Teaches |
|-----------|-------|-----------------|
| `convert_params()` | `src/c/database_query.cpp` | Typed parallel arrays (`int* types`, `void** values`) through C API. Exact pattern for columnar input. |
| `quiver_element_t` builder | `include/quiver/c/element.h` | Opaque type with typed setters. Alternative approach (not recommended for this use case due to surface area cost). |
| `update_time_series_files` | `src/c/database_time_series.cpp` | Parallel string arrays (`const char** columns`, `const char** paths`) through C API. Column-name-associated data. |
| `read_time_series_files` | `src/c/database_time_series.cpp` | Parallel string array output (`char*** out_columns`, `char*** out_paths`). Output allocation pattern. |
| `copy_strings_to_c()` | `src/c/database_helpers.h` | Helper for allocating string arrays. Reusable for string column output. |
| `read_scalars_impl<T>()` | `src/c/database_helpers.h` | Template for copying typed vectors to C arrays. Reusable per-column. |
| `GroupMetadata.value_columns` | `include/quiver/attribute_metadata.h` | Already provides column names and types. Read-side can use this to allocate correctly typed output arrays. |
| `quiver_data_type_t` enum | `include/quiver/c/database.h` | Already defines INTEGER=0, FLOAT=1, STRING=2, DATE_TIME=3, NULL=4. Reuse as column type tag. |
| Julia `create_element!(db, col; kwargs...)` | `bindings/julia/src/database_create.jl` | kwargs -> Element builder pattern. Template for kwargs -> columnar arrays. |
| Dart `createElement(col, Map)` | `bindings/dart/lib/src/database_create.dart` | Map -> Element builder pattern. Template for Map -> columnar arrays. |

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| C API multi-column update | HIGH | MEDIUM | P0 |
| C API multi-column read | HIGH | HIGH | P0 |
| C API free function redesign | HIGH | MEDIUM | P0 |
| Multi-column test schema | HIGH | LOW | P0 |
| Julia kwargs interface | HIGH | MEDIUM | P1 |
| Dart Map interface | HIGH | MEDIUM | P1 |
| Lua multi-column tests | MEDIUM | LOW | P1 |
| Metadata-driven validation | MEDIUM | LOW | P2 |
| Integer value column support | MEDIUM | LOW | P0 (comes free with typed design) |
| String value column support | MEDIUM | LOW | P0 (comes free with typed design) |

**Priority key:**
- P0: Core C API redesign (must complete first, enables everything else)
- P1: Binding updates (immediately follows C API)
- P2: Polish (after bindings work)

## Sources

- Quiver codebase analysis (direct inspection of all layers) -- HIGH confidence
- Existing `convert_params()` pattern in `src/c/database_query.cpp` -- HIGH confidence (proven internal pattern)
- Existing `quiver_element_t` builder pattern -- HIGH confidence (proven internal pattern)
- Existing `update_time_series_files` parallel array pattern -- HIGH confidence (proven internal pattern)
- Current `GroupMetadata` with `value_columns` and `dimension_column` -- HIGH confidence (metadata system already provides type info needed for validation)

---
*Feature research for: Time Series Update Interface Redesign*
*Researched: 2026-02-12*
