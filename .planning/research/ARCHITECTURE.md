# Architecture Research: CSV Export with Enum Resolution and Date Formatting

**Domain:** CSV export for SQLite wrapper library with cross-layer propagation
**Researched:** 2026-02-22
**Confidence:** HIGH

## System Overview

Current architecture with CSV export integration points:

```
Bindings Layer (Julia, Dart, Lua)
  |  Construct native map/dict -> flatten to parallel arrays
  |  Call C API with flat options struct
  |
C API Layer (src/c/)
  |  Flat functions, quiver_error_t returns
  |  NEW: quiver_csv_export_options_t struct (flat parallel arrays for enum map)
  |  NEW: quiver_database_export_scalars_csv / quiver_database_export_group_csv
  |  NEW: quiver_csv_export_options_free for cleanup
  |
C++ Public API (include/quiver/database.h)
  |  Database class with Pimpl
  |  NEW: CsvExportOptions struct (nested map + format string)
  |  CHANGE: export_csv signature replaced with export_scalars_csv / export_group_csv
  |
C++ Implementation (src/database_csv.cpp)
  |  NEW file: Direct sqlite3 queries, CSV formatting, enum resolution, date formatting
  |  Uses: Schema for column discovery, impl_->db for queries
  |
SQLite (sqlite3*)
  |  Direct SELECT queries for CSV data extraction
```

## New API Methods

### C++ Public API

Two distinct export operations replace the current stub:

```cpp
// include/quiver/csv_export_options.h -- new header
struct CsvExportOptions {
    // attribute_name -> { integer_value -> label_string }
    std::map<std::string, std::map<int64_t, std::string>> enum_labels;

    // strftime-compatible format string for date_time columns (empty = no formatting)
    std::string date_time_format;
};

// include/quiver/database.h -- modified signatures
void export_scalars_csv(const std::string& collection,
                        const std::string& path,
                        const CsvExportOptions& options = {});

void export_group_csv(const std::string& collection,
                      const std::string& group,
                      const std::string& path,
                      const CsvExportOptions& options = {});
```

**Rationale for two methods instead of one:**

The current stub `export_csv(table, path)` takes a raw table name, which is wrong for two reasons: (1) callers should not need to know internal table naming conventions (`Collection_vector_values`), and (2) scalar export requires joining id->label while group export has different column semantics (id column replaced with label, structural columns like `vector_index` omitted from output). Two methods with distinct parameters are clearer than a polymorphic single method.

**CsvExportOptions as a separate header** because it is a plain value type (no private dependencies, Rule of Zero) and will be included by both `database.h` and the C API headers. This follows the pattern of `attribute_metadata.h` and `data_type.h`.

### C API Structs

The core challenge: representing `map<string, map<int64_t, string>>` (enum_labels) through a flat C API.

**Design: Flattened parallel arrays with entry counts per attribute.**

```c
// include/quiver/c/csv_export_options.h -- new header

typedef struct {
    // Enum labels: flattened representation of attribute -> {value -> label}
    //
    // attribute_names[i] identifies which attribute the entries belong to.
    // attribute_entry_counts[i] says how many (value, label) pairs this attribute has.
    // enum_values[] and enum_label_strings[] are the flattened pairs, contiguous.
    //
    // Example: attribute "status" with {0->"Active", 1->"Inactive"}, "priority" with {0->"Low"}
    //   attribute_names = ["status", "priority"]
    //   attribute_entry_counts = [2, 1]
    //   attribute_count = 2
    //   enum_values = [0, 1, 0]          -- status entries, then priority entries
    //   enum_label_strings = ["Active", "Inactive", "Low"]
    //   total_enum_entries = 3
    //
    const char* const* attribute_names;
    const size_t* attribute_entry_counts;
    size_t attribute_count;

    const int64_t* enum_values;
    const char* const* enum_label_strings;
    size_t total_enum_entries;

    // Date/time format (NULL = no formatting, pass through raw string)
    const char* date_time_format;
} quiver_csv_export_options_t;
```

**Why this design over alternatives:**

| Alternative | Problem |
|-------------|---------|
| JSON string | Requires JSON parser in C++ core; violates "no external deps for options" |
| Opaque builder handle | Adds lifecycle complexity (create/add_entry/free); more C API surface |
| Array of per-attribute structs with inner arrays | Nested heap allocation, complex ownership |
| **Flattened parallel arrays (chosen)** | Caller builds contiguous arrays; no allocation in C layer; matches existing parallel-array patterns (parameterized queries, time series update) |

The parallel-array pattern is already established in the codebase. `quiver_database_update_time_series_group` uses `column_names[]`, `column_types[]`, `column_data[]`, `column_count`, `row_count`. The enum map flattening follows the same principle: parallel arrays plus counts.

**Key invariant:** `sum(attribute_entry_counts[0..attribute_count-1]) == total_enum_entries`. The C++ conversion function validates this.

### C API Functions

```c
// include/quiver/c/database.h -- new/modified declarations

// Replace existing export_csv stub
QUIVER_C_API quiver_error_t quiver_database_export_scalars_csv(
    quiver_database_t* db,
    const char* collection,
    const char* path,
    const quiver_csv_export_options_t* options);  // NULL = default options

QUIVER_C_API quiver_error_t quiver_database_export_group_csv(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    const char* path,
    const quiver_csv_export_options_t* options);  // NULL = default options
```

**No free function needed** for `quiver_csv_export_options_t` because the caller owns all the data. The struct contains only `const` pointers into caller-owned memory. The C API reads the data during the export call and does not retain any references. This matches `quiver_database_options_t` which also has no free function.

### C API to C++ Conversion

A helper in `src/c/database_csv.cpp` converts the flat struct to the nested C++ map:

```cpp
static CsvExportOptions convert_csv_options(const quiver_csv_export_options_t* opts) {
    CsvExportOptions result;
    if (!opts) return result;

    if (opts->date_time_format) {
        result.date_time_format = opts->date_time_format;
    }

    // Rebuild nested map from flattened arrays
    size_t offset = 0;
    for (size_t i = 0; i < opts->attribute_count; ++i) {
        std::string attr_name(opts->attribute_names[i]);
        std::map<int64_t, std::string> entries;
        for (size_t j = 0; j < opts->attribute_entry_counts[i]; ++j) {
            entries[opts->enum_values[offset + j]] =
                std::string(opts->enum_label_strings[offset + j]);
        }
        result.enum_labels[attr_name] = std::move(entries);
        offset += opts->attribute_entry_counts[i];
    }

    // Validate invariant
    if (offset != opts->total_enum_entries) {
        throw std::runtime_error(
            "Cannot export_csv: enum entry count mismatch "
            "(sum of attribute_entry_counts != total_enum_entries)");
    }

    return result;
}
```

## Data Flow

### Export Scalars CSV

```
caller: db.export_scalars_csv("Items", "output.csv", options)
    |
    v
Database::export_scalars_csv()
    |-- impl_->require_collection("Items", "export_scalars_csv")
    |-- Retrieve scalar metadata: list_scalar_attributes("Items")
    |-- Build column list: [label, attr1, attr2, ...] (skip "id")
    |-- Execute: SELECT label, attr1, attr2, ... FROM Items
    |
    v
For each row:
    |-- For each column value:
    |   |-- Is this column in enum_labels map?
    |   |   YES: lookup value -> label, write label
    |   |   NO: Is this a date_time column with format string?
    |   |       YES: format date string
    |   |       NO: write raw value
    |
    v
Write CSV rows to file (std::ofstream)
```

### Export Group CSV

```
caller: db.export_group_csv("Items", "values", "output.csv", options)
    |
    v
Database::export_group_csv()
    |-- impl_->require_collection("Items", "export_group_csv")
    |-- Determine group type (vector/set/time_series) via Schema
    |-- Get group table name: Items_vector_values / Items_set_tags / Items_time_series_data
    |-- Get group metadata for column info
    |-- Build SELECT with JOIN to parent for label:
    |   SELECT Items.label, col1, col2, ...
    |   FROM Items_vector_values
    |   JOIN Items ON Items_vector_values.id = Items.id
    |   ORDER BY Items.label, vector_index  (or date_time for time series)
    |-- Skip structural columns in output: id, vector_index (for vectors)
    |
    v
For each row:
    |-- Same enum resolution and date formatting as scalars
    |
    v
Write CSV rows to file (std::ofstream)
```

### SQL Query Strategy: Direct sqlite3

The export uses direct SQL via the existing `Database::execute()` internal method, NOT the typed read methods (`read_scalar_integers`, etc.). Reasons:

1. **Performance:** One query fetches all columns at once. Using typed read methods would require N queries (one per attribute) plus manual row alignment.
2. **Column order:** A single SELECT preserves column ordering naturally. Composing from individual reads requires reconstructing row order from parallel vectors.
3. **Consistency:** `describe()` in the same file (`database_describe.cpp`) already uses `impl_->schema` directly. Export follows the same pattern.
4. **Simplicity:** One query, iterate rows, format output. No intermediate data structures.

The query returns a `Result` with `Row` objects. Each `Row` value is accessed positionally via `get_integer/get_float/get_string`, then formatted.

## Component Boundaries

### New Components

| Component | File | Responsibility |
|-----------|------|---------------|
| `CsvExportOptions` | `include/quiver/csv_export_options.h` | C++ options struct (plain value type, Rule of Zero) |
| `quiver_csv_export_options_t` | `include/quiver/c/csv_export_options.h` | C API flat struct for FFI |
| CSV export implementation | `src/database_csv.cpp` | SQL queries, CSV writing, enum resolution, date formatting |
| C API CSV functions | `src/c/database_csv.cpp` | C API wrappers, flat-to-nested conversion |

### Modified Components

| Component | File | Change |
|-----------|------|--------|
| Database class | `include/quiver/database.h` | Replace `export_csv` stub with `export_scalars_csv` + `export_group_csv` |
| Database describe | `src/database_describe.cpp` | Remove `export_csv` and `import_csv` stubs |
| C API database header | `include/quiver/c/database.h` | Replace `quiver_database_export_csv` with two new functions |
| C API database impl | `src/c/database.cpp` | Remove `quiver_database_export_csv` and `quiver_database_import_csv` stubs |
| CMakeLists.txt | `src/CMakeLists.txt` | Add `database_csv.cpp` |
| C API CMakeLists.txt | `src/c/CMakeLists.txt` | Add `database_csv.cpp` |
| Julia CSV binding | `bindings/julia/src/database_csv.jl` | Replace stub with real implementation |
| Julia C API | `bindings/julia/src/c_api.jl` | Regenerate FFI declarations |
| Dart CSV binding | `bindings/dart/lib/src/database_csv.dart` | Replace stub with real implementation |
| Dart FFI bindings | `bindings/dart/lib/src/ffi/bindings.dart` | Regenerate FFI declarations |
| Lua runner | `src/lua_runner.cpp` | Add `export_scalars_csv` and `export_group_csv` bindings |

### Unchanged Components

Schema, metadata, TypeValidator, TransactionGuard, Element, existing read/write methods, existing query methods -- none of these change.

## CSV Writing Implementation

### No External CSV Library

Use `std::ofstream` with manual CSV escaping. The output is simple tabular data. A CSV library adds a dependency for what amounts to:

```cpp
static void write_csv_field(std::ofstream& out, const std::string& value, bool first) {
    if (!first) out << ',';
    // Quote if contains comma, quote, or newline
    if (value.find_first_of(",\"\n") != std::string::npos) {
        out << '"';
        for (char c : value) {
            if (c == '"') out << '"';
            out << c;
        }
        out << '"';
    } else {
        out << value;
    }
}
```

This is ~15 lines. A library dependency is not justified.

### Date/Time Formatting

The `date_time_format` string in `CsvExportOptions` is a C++ `std::chrono`-style format or a simple custom format. However, since SQLite stores date/time as TEXT strings (ISO 8601 format like "2024-01-15T10:30:00"), and the caller wants output formatting, the simplest approach is:

1. Parse the stored ISO string to `std::tm` via `strptime` (POSIX) or manual parsing
2. Format via `strftime` with the caller's format string
3. Write the formatted string to CSV

Since the project targets C++20 on Windows (MSVC), `strptime` is not available. Use manual ISO 8601 parsing (the format is fixed and known) and `std::put_time` / `strftime` for output. This avoids platform-specific dependencies.

If `date_time_format` is empty, date/time columns are written as-is (raw SQLite TEXT value). This is the default behavior.

### Enum Resolution

For each row, for each column that appears in `enum_labels`:

1. Read the integer value from the `Row`
2. Look up in the attribute's `map<int64_t, string>`
3. If found, write the label string
4. If not found, write the raw integer (graceful degradation, log a warning)

This happens at the C++ layer. Bindings pass the map through; they do not perform resolution themselves. This follows the project principle: "Logic resides in C++ layer. Bindings remain thin."

## Binding Integration

### Julia

Julia builds the options struct most naturally using a Dict:

```julia
# Caller API:
export_scalars_csv!(db, "Items", "output.csv";
    enum_labels = Dict(
        "status" => Dict(0 => "Active", 1 => "Inactive"),
        "priority" => Dict(0 => "Low", 1 => "Medium", 2 => "High")
    ),
    date_time_format = "%Y-%m-%d"
)
```

The Julia binding flattens the `Dict{String, Dict{Int64, String}}` into parallel arrays and fills a `quiver_csv_export_options_t` struct via `ccall`. The flattening logic lives in Julia (binding-side convenience):

```julia
function export_scalars_csv!(db::Database, collection::String, path::String;
                             enum_labels::Dict{String,Dict{Int64,String}} = Dict{String,Dict{Int64,String}}(),
                             date_time_format::String = "")
    # Flatten enum_labels to parallel arrays
    attr_names = collect(keys(enum_labels))
    attr_entry_counts = [length(enum_labels[a]) for a in attr_names]
    all_values = Int64[]
    all_labels = String[]
    for attr in attr_names
        for (v, l) in enum_labels[attr]
            push!(all_values, v)
            push!(all_labels, l)
        end
    end
    # Build C struct and call
    ...
end
```

### Dart

Dart uses a `Map<String, Map<int, String>>` and a helper class:

```dart
class CsvExportOptions {
  final Map<String, Map<int, String>> enumLabels;
  final String dateTimeFormat;

  const CsvExportOptions({
    this.enumLabels = const {},
    this.dateTimeFormat = '',
  });
}

// Caller API:
db.exportScalarsCSV('Items', 'output.csv',
  options: CsvExportOptions(
    enumLabels: {
      'status': {0: 'Active', 1: 'Inactive'},
    },
    dateTimeFormat: '%Y-%m-%d',
  ),
);
```

The Dart binding allocates native memory in an `Arena`, flattens the map, fills the C struct, calls the C API, and releases the arena. This follows the existing pattern in `database_csv.dart`.

### Lua

Lua receives native tables. The C++ sol2 binding extracts the nested table structure:

```lua
-- Caller API:
db:export_scalars_csv("Items", "output.csv", {
    enum_labels = {
        status = { [0] = "Active", [1] = "Inactive" },
        priority = { [0] = "Low" }
    },
    date_time_format = "%Y-%m-%d"
})
```

The sol2 binding converts the Lua table directly to `CsvExportOptions` in C++:

```cpp
"export_scalars_csv",
[](Database& self, const std::string& collection, const std::string& path,
   sol::optional<sol::table> opts_table) {
    CsvExportOptions options;
    if (opts_table) {
        options = lua_table_to_csv_options(*opts_table);
    }
    self.export_scalars_csv(collection, path, options);
},
```

Lua skips the C API entirely (sol2 binds to C++ directly), so no flattening needed. The nested Lua table maps directly to the nested C++ map. This is consistent with how time series and query parameters work in Lua.

## Patterns to Follow

### Pattern 1: Options Struct with Defaults

Follow the `DatabaseOptions` pattern: a simple struct with a default constructor / initializer.

```cpp
struct CsvExportOptions {
    std::map<std::string, std::map<int64_t, std::string>> enum_labels;
    std::string date_time_format;
};
// Default construction gives empty map and empty format string = no transformation
```

C++ callers can use `{}` for defaults. C API callers pass `NULL` for the options pointer. This mirrors `quiver_database_options_t` being optional in factory functions.

### Pattern 2: Separate Implementation File

Create `src/database_csv.cpp` (new file) rather than extending `database_describe.cpp`. Rationale:

- `describe()` prints to stdout (diagnostic). CSV export writes to files (data operation). Different concerns.
- The existing file organization (`database_read.cpp`, `database_create.cpp`, etc.) separates by operation type.
- CSV export will have meaningful implementation (~200-300 lines). It deserves its own translation unit.

The stub `export_csv` and `import_csv` move out of `database_describe.cpp`. The `import_csv` stub can remain as-is (empty body) or be removed entirely since it is out of scope for v0.4.

### Pattern 3: Schema-Driven Column Discovery

Use `list_scalar_attributes()` and group metadata to discover columns dynamically, not hardcoded SQL. This ensures export works with any schema.

```cpp
void Database::export_scalars_csv(const std::string& collection,
                                  const std::string& path,
                                  const CsvExportOptions& options) {
    impl_->require_collection(collection, "export_scalars_csv");
    auto attributes = list_scalar_attributes(collection);

    // Build SELECT from discovered attributes (skip id, include label)
    std::string sql = "SELECT label";
    std::vector<std::string> column_names = {"label"};
    for (const auto& attr : attributes) {
        if (attr.name == "id" || attr.name == "label") continue;
        sql += ", " + attr.name;
        column_names.push_back(attr.name);
    }
    sql += " FROM " + collection;

    auto result = execute(sql);
    // ... write CSV ...
}
```

### Pattern 4: C API Co-location

Following the alloc/free co-location rule: `quiver_database_export_scalars_csv` and `quiver_database_export_group_csv` plus the `convert_csv_options` helper all live in `src/c/database_csv.cpp`. No free function is needed (caller-owned data), but if one were needed later, it would go in the same file.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Passing the Enum Map as JSON

**What:** Serialize the map to a JSON string, pass as `const char*`, parse in C++.
**Why bad:** Introduces a JSON parsing dependency. The C API should handle structured data natively, not through serialization formats. The existing codebase has zero JSON usage.
**Instead:** Flattened parallel arrays as described above.

### Anti-Pattern 2: Using Existing Read Methods for Export

**What:** Call `read_scalar_integers()`, `read_scalar_strings()`, etc. individually, then align the parallel vectors into rows.
**Why bad:** N+1 queries instead of one. Row alignment is error-prone (what if some attributes have NULL gaps?). Significantly slower for large collections.
**Instead:** Single `SELECT` query via `execute()`, iterate rows.

### Anti-Pattern 3: Enum Resolution in Bindings

**What:** Have Julia/Dart/Lua do the value->label replacement after reading raw data.
**Why bad:** Violates "Logic resides in C++ layer." Duplicates logic in 3+ places. The C++ layer has all the information needed.
**Instead:** Pass the map to C++, resolve there, write resolved values to CSV.

### Anti-Pattern 4: Re-using the DatabaseOptions Struct

**What:** Add CSV fields to `quiver_database_options_t`.
**Why bad:** `DatabaseOptions` is a database-wide configuration (log level, read-only). CSV export options are per-call parameters. Mixing them conflates lifecycle configuration with operation parameters.
**Instead:** Separate `CsvExportOptions` / `quiver_csv_export_options_t`.

### Anti-Pattern 5: One Generic Export Function

**What:** `export_csv(collection, group_or_empty, path, options)` where empty group means scalars.
**Why bad:** Overloaded semantics through empty string detection. The two operations have different SQL, different column handling, and different skip-lists. Separate functions are clearer.
**Instead:** `export_scalars_csv` and `export_group_csv`.

## Files New vs Modified

### New Files

| File | Purpose |
|------|---------|
| `include/quiver/csv_export_options.h` | CsvExportOptions C++ struct |
| `include/quiver/c/csv_export_options.h` | quiver_csv_export_options_t C struct |
| `src/database_csv.cpp` | C++ export implementation |
| `src/c/database_csv.cpp` | C API export wrappers + option conversion |
| `tests/test_database_csv.cpp` | C++ CSV export tests |
| `tests/test_c_api_database_csv.cpp` | C API CSV export tests |
| `tests/schemas/valid/csv_export.sql` | Test schema with enum-like integer attributes |

### Modified Files

| File | Change |
|------|--------|
| `include/quiver/database.h` | Replace `export_csv`/`import_csv` with new signatures, add `#include "csv_export_options.h"` |
| `include/quiver/c/database.h` | Replace `quiver_database_export_csv`/`import_csv` with new function declarations |
| `src/database_describe.cpp` | Remove `export_csv`/`import_csv` stubs |
| `src/c/database.cpp` | Remove `quiver_database_export_csv`/`import_csv` stubs |
| `src/CMakeLists.txt` | Add `database_csv.cpp` |
| `src/c/CMakeLists.txt` | Add `database_csv.cpp` |
| `bindings/julia/src/database_csv.jl` | Replace stubs with real export functions |
| `bindings/julia/src/c_api.jl` | Regenerate (via generator) |
| `bindings/dart/lib/src/database_csv.dart` | Replace stubs with real export methods |
| `bindings/dart/lib/src/ffi/bindings.dart` | Regenerate (via generator) |
| `src/lua_runner.cpp` | Add `export_scalars_csv` and `export_group_csv` bindings |

### Removed Stubs

| Current Stub | Replacement |
|-------------|-------------|
| `Database::export_csv(table, path)` | `Database::export_scalars_csv(collection, path, options)` + `Database::export_group_csv(collection, group, path, options)` |
| `Database::import_csv(table, path)` | Keep empty stub (out of scope for v0.4) or remove declaration |
| `quiver_database_export_csv(db, table, path)` | `quiver_database_export_scalars_csv(db, collection, path, options)` + `quiver_database_export_group_csv(db, collection, group, path, options)` |
| `quiver_database_import_csv(db, table, path)` | Keep empty stub or remove |

## Build Order

### Phase 1: C++ Core (no downstream dependencies)

**Step 1.1:** Create `include/quiver/csv_export_options.h`
- Define `CsvExportOptions` struct

**Step 1.2:** Modify `include/quiver/database.h`
- Add `#include "csv_export_options.h"`
- Replace `export_csv` with `export_scalars_csv` and `export_group_csv`
- Decision: keep `import_csv` stub or remove declaration

**Step 1.3:** Create `src/database_csv.cpp`
- Implement `export_scalars_csv`: schema discovery, SQL query, CSV writing, enum resolution, date formatting
- Implement `export_group_csv`: group type detection, JOIN query, column filtering, CSV writing
- Implement CSV escaping helper, date parsing/formatting helper

**Step 1.4:** Remove stubs from `src/database_describe.cpp`

**Step 1.5:** Update `src/CMakeLists.txt`

**Step 1.6:** Create test schema `tests/schemas/valid/csv_export.sql`
- Include integer attributes that simulate enums
- Include date_time columns
- Include vector, set, and time series groups

**Step 1.7:** Create `tests/test_database_csv.cpp`
- Test scalars export (basic, with enum labels, with date formatting)
- Test group export (vector, set, time series)
- Test empty collection export
- Test enum value not in map (graceful fallback)
- Test file creation and CSV format correctness

### Phase 2: C API (depends on Phase 1)

**Step 2.1:** Create `include/quiver/c/csv_export_options.h`
- Define `quiver_csv_export_options_t` flat struct

**Step 2.2:** Modify `include/quiver/c/database.h`
- Add `#include "csv_export_options.h"`
- Replace `quiver_database_export_csv` with two new function declarations

**Step 2.3:** Create `src/c/database_csv.cpp`
- Implement `convert_csv_options` helper (flat -> nested)
- Implement `quiver_database_export_scalars_csv`
- Implement `quiver_database_export_group_csv`

**Step 2.4:** Remove stubs from `src/c/database.cpp`

**Step 2.5:** Update `src/c/CMakeLists.txt`

**Step 2.6:** Create `tests/test_c_api_database_csv.cpp`
- Same scenarios as C++ tests but via C API
- Test NULL options (defaults)
- Test enum flattening correctness

### Phase 3: Bindings (depends on Phase 2, parallelizable)

**Step 3.1 (Julia):**
- Regenerate `c_api.jl` via generator
- Rewrite `database_csv.jl` with Dict flattening and real ccall
- Add Julia CSV export tests

**Step 3.2 (Dart):**
- Regenerate FFI bindings via generator
- Rewrite `database_csv.dart` with Map flattening and Arena allocation
- Add Dart CSV export tests

**Step 3.3 (Lua):**
- Add `export_scalars_csv` and `export_group_csv` to `bind_database()` in `lua_runner.cpp`
- Add `lua_table_to_csv_options` helper
- Add Lua CSV export tests (via LuaRunner in C++ test suite)

### Dependency Graph

```
Phase 1: C++ Core + Options Struct + Tests
    |
    v
Phase 2: C API + Flat Options Struct + Tests
    |
    +---> Phase 3.1: Julia Binding + Tests
    |
    +---> Phase 3.2: Dart Binding + Tests
    |
    +---> Phase 3.3: Lua Binding + Tests
```

## Cross-Layer Naming Summary

| C++ | C API | Julia | Dart | Lua |
|-----|-------|-------|------|-----|
| `export_scalars_csv()` | `quiver_database_export_scalars_csv()` | `export_scalars_csv!()` | `exportScalarsCSV()` | `export_scalars_csv()` |
| `export_group_csv()` | `quiver_database_export_group_csv()` | `export_group_csv!()` | `exportGroupCSV()` | `export_group_csv()` |
| `CsvExportOptions` | `quiver_csv_export_options_t` | keyword args on export functions | `CsvExportOptions` class | Lua table argument |

## Scalability Considerations

| Concern | Small (100 rows) | Medium (10K rows) | Large (1M rows) |
|---------|-------------------|--------------------|--------------------|
| Memory | Single Result in memory, write as you go | Same; Result holds all rows | Consider streaming: execute query, iterate sqlite3_step, write per-row without buffering all in Result |
| File I/O | Negligible | Negligible | Buffered ofstream handles this well |
| Enum lookup | Map lookup per cell, negligible | Same | Same; map is small (enums have few values) |

For v0.4, loading the full `Result` into memory is acceptable. If million-row collections become common, a future optimization can iterate `sqlite3_step` directly without materializing the full `Result`. This is an optimization, not an architectural change.

## Sources

- Existing codebase analysis: `include/quiver/database.h`, `include/quiver/c/database.h`, `include/quiver/c/options.h`, `src/database_describe.cpp`, `src/c/database.cpp`, `src/c/database_query.cpp`, `src/c/database_time_series.cpp`, `src/c/database_helpers.h`, `src/database_impl.h`, `src/database_read.cpp`, `src/lua_runner.cpp`
- Existing binding stubs: `bindings/julia/src/database_csv.jl`, `bindings/dart/lib/src/database_csv.dart`
- Schema patterns: `tests/schemas/valid/collections.sql`, `tests/schemas/valid/relations.sql`
- Project requirements: `.planning/PROJECT.md` (v0.4 requirements)

---
*Architecture research for: CSV Export with Enum Resolution and Date Formatting in Quiver SQLite wrapper*
*Researched: 2026-02-22*
