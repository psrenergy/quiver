# Stack Research

**Domain:** CSV export with enum resolution and date formatting for SQLite wrapper library
**Researched:** 2026-02-22
**Confidence:** HIGH

## Recommended Stack

### Core Technologies (No New Dependencies Required)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Hand-rolled CSV writer | N/A (C++20 stdlib only) | Write RFC 4180-compliant CSV output | CSV writing is trivially simple (3 rules). Adding a library for this is unjustifiable overhead -- see detailed rationale below. |
| `std::ofstream` | C++20 standard library | File output for CSV | Already available. No new headers needed beyond `<fstream>` and `<sstream>`. |
| `strftime` | C standard library (`<ctime>`) | Date/time reformatting from SQLite TEXT to user-specified format | SQLite stores dates as TEXT strings (e.g., `"2024-01-15 10:30:00"`). Parse with `std::get_time`, reformat with `strftime`. Simple, portable, zero-dependency. See rationale below. |
| `std::map<std::string, std::map<int64_t, std::string>>` | C++20 standard library | C++ representation of enum_labels (attribute -> {value -> label}) | STL map-of-maps is the natural C++ representation. Converted to flat parallel arrays at the C API boundary. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| None needed | -- | -- | This milestone requires zero new dependencies. All CSV writing, date formatting, and options passing use C++20 standard library facilities. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `bindings/julia/generator/generator.bat` | Regenerate Julia FFI after C API changes | Must run after adding new C API functions and structs for CSV export options |
| `bindings/dart/generator/generator.bat` | Regenerate Dart FFI after C API changes | Must run after adding new C API functions and structs for CSV export options |

## No Installation Required

This milestone requires **zero new dependencies**. Everything needed is already in the project or standard library:

```cmake
# Already available:
# - C++20 standard: <fstream>, <sstream>, <iomanip>, <ctime>, <string>, <map>
# - SQLite 3.50.2 (data source for CSV export)
# - GoogleTest 1.17.0 (test framework)
# - spdlog 1.17.0 (logging for export operations)

# No new FetchContent entries needed.
```

## Key Technical Findings

### 1. CSV Writing: Hand-Roll, Do Not Use a Library

**Recommendation: Write CSV output directly using `std::ofstream`.** Do NOT add a CSV library.

**Rationale:**

CSV *writing* per RFC 4180 has exactly 3 rules:
1. Fields containing commas, double quotes, or line breaks must be enclosed in double quotes.
2. Double quotes inside quoted fields are escaped by doubling them (`"` becomes `""`).
3. Each record is terminated by CRLF (though LF-only is widely accepted).

This is approximately 15 lines of C++:

```cpp
// Write a single CSV field with RFC 4180 quoting
void write_csv_field(std::ostream& out, const std::string& field) {
    if (field.find_first_of(",\"\r\n") != std::string::npos) {
        out << '"';
        for (char c : field) {
            if (c == '"') out << '"';
            out << c;
        }
        out << '"';
    } else {
        out << field;
    }
}
```

**Why NOT a library:**
- Every C++ CSV library found (fast-cpp-csv-parser, rapidcsv, csv2, csv-parser, lazycsv) is optimized for *reading* CSV, not writing. Writing support is either absent or trivial.
- Adding a FetchContent dependency for 15 lines of logic contradicts the project's "simple solutions over complex abstractions" principle.
- The project already has zero CSV-related dependencies. Adding one introduces build complexity, version tracking, and potential CMake conflicts for negligible value.
- Quiver's CSV output is structured data from SQLite queries -- no user-supplied freeform text, no multiline fields, no embedded commas in numeric data. The edge cases that make CSV *parsing* hard simply do not apply to CSV *writing* of database exports.

**Edge cases that DO apply (all handled by the 15-line function above):**
- Labels containing commas (e.g., `"Smith, John"`) -- quoted correctly.
- Labels containing double quotes (e.g., `He said "hello"`) -- escaped by doubling.
- Empty strings -- written as empty field (no quoting needed per RFC 4180).
- NULL values from SQLite -- write as empty field (conventional CSV representation).

### 2. Date/Time Formatting: Use strftime, Not std::chrono

**Recommendation: Use `std::get_time` to parse and `strftime` to format.** Do NOT use `std::chrono::parse` or `std::format` with chrono.

**Rationale:**

SQLite stores date/time values as TEXT strings (e.g., `"2024-01-15 10:30:00"`). The task is string-to-string reformatting: parse a known format, output in a user-specified format.

```cpp
// Reformat a SQLite date/time string using a strftime format
std::string reformat_date_time(const std::string& sqlite_value, const std::string& format) {
    std::tm tm = {};
    std::istringstream ss(sqlite_value);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return sqlite_value;  // Return original on parse failure

    char buf[128];
    std::strftime(buf, sizeof(buf), format.c_str(), &tm);
    return buf;
}
```

**Why `strftime` over `std::chrono::parse` + `std::format`:**

1. **Simpler for the task.** We are doing string -> tm -> string. `std::chrono::parse` converts to `time_point`, `std::format` converts back. Extra type conversions for no benefit.
2. **Format string compatibility.** The caller passes a format string. `strftime` format specifiers (`%Y-%m-%d`, `%d/%m/%Y %H:%M`) are universally understood. `std::format` chrono specifiers are identical but wrapped in `"{:%Y-%m-%d}"` syntax -- the outer `{}` would confuse users who expect plain `strftime` format strings.
3. **FFI simplicity.** The format string passes through C API as `const char*`. With `strftime` specifiers, the same format string works in Julia (`Dates.format`), Dart (`DateFormat`), and Lua (`os.date`) with minimal translation. `std::format` chrono syntax is C++-specific.
4. **No new includes.** `<ctime>` and `<iomanip>` are standard. `<format>` with chrono requires MSVC /std:c++20 (already set) but is a heavier include and untested in this codebase.
5. **Thread safety concern with `strftime` is irrelevant.** `strftime` itself is thread-safe (it writes to a caller-provided buffer). The thread-unsafe function is `localtime()`, which we do NOT use -- `std::get_time` fills a caller-owned `std::tm` directly.

**When to reconsider:** If a future milestone needs timezone-aware formatting or sub-second precision, `std::chrono` becomes worthwhile. For reformatting SQLite TEXT dates to user display formats, `strftime` is correct.

**Default format string:** `"%Y-%m-%d %H:%M:%S"` (identity transform -- output matches SQLite storage format). Caller can override with any `strftime`-compatible format like `"%d/%m/%Y"`, `"%B %d, %Y"`, etc.

### 3. C++ Options Struct Design

**Recommendation:** A plain value type `CsvExportOptions` in the C++ layer.

```cpp
// In include/quiver/csv_export_options.h (new file)
struct CsvExportOptions {
    // attribute_name -> { integer_value -> label_string }
    std::map<std::string, std::map<int64_t, std::string>> enum_labels;

    // strftime format for date_time columns. Empty = no reformatting.
    std::string date_time_format;
};
```

**Why this design:**
- Plain value type with Rule of Zero (no Pimpl needed -- no private dependencies to hide).
- `std::map` is natural for the lookup: when writing CSV row, check if `enum_labels.count(attribute_name)` and if so, look up `enum_labels[attribute_name][integer_value]`.
- `date_time_format` as empty string means "no reformatting" (write the raw SQLite TEXT value). This avoids needing `std::optional<std::string>` -- empty string is the natural sentinel.
- The C++ API signature becomes: `void export_csv(const std::string& collection, const std::string& path, const CsvExportOptions& options = {})`.

### 4. C API Options Struct: Flat Parallel Arrays

**Recommendation:** Use the parallel-arrays pattern already established by `quiver_database_update_time_series_group()` and `quiver_database_query_*_params()`.

```c
// In include/quiver/c/database.h (added to existing file)
typedef struct {
    // Enum label mappings (parallel arrays, flattened from nested map)
    // enum_attribute_names[i] = attribute name
    // enum_values[i]          = integer value
    // enum_labels[i]          = label string
    // All three arrays have enum_entry_count elements.
    const char* const* enum_attribute_names;
    const int64_t* enum_values;
    const char* const* enum_labels;
    size_t enum_entry_count;

    // strftime format for date_time columns. NULL = no reformatting.
    const char* date_time_format;
} quiver_csv_export_options_t;

// Default constructor function
QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void);
```

**Why parallel arrays (not a builder pattern or nested structs):**

1. **Established precedent.** The C API already uses parallel arrays in multiple places:
   - `quiver_database_update_time_series_group()` uses `column_names[]`, `column_types[]`, `column_data[]`, `column_count`.
   - `quiver_database_query_*_params()` uses `param_types[]`, `param_values[]`, `param_count`.
   - `quiver_database_update_time_series_files()` uses `columns[]`, `paths[]`, `count`.
   This is the established Quiver C API idiom for key-value-like data.

2. **FFI-generator friendly.** Both `ffigen` (Dart) and `Clang.jl` (Julia) can auto-generate bindings for this struct because it contains only primitive types and pointer-to-primitive arrays. No opaque handles, no nested structs, no function pointers.

3. **Flat representation of nested map.** The C++ `map<string, map<int64_t, string>>` flattens to three parallel arrays:

   ```
   C++: { "status": {0: "Inactive", 1: "Active"}, "priority": {1: "Low", 2: "High"} }

   C API:
     enum_attribute_names = ["status",   "status", "priority", "priority"]
     enum_values          = [0,          1,        1,          2         ]
     enum_labels          = ["Inactive", "Active", "Low",      "High"   ]
     enum_entry_count     = 4
   ```

4. **Caller-owned, no lifecycle.** All pointers in the struct are `const` -- the C API reads them during the `export_csv` call and does not retain references. No alloc/free pair needed for the options struct itself.

5. **Null for defaults.** `enum_attribute_names = NULL` with `enum_entry_count = 0` means no enum mappings. `date_time_format = NULL` means no date reformatting. This matches the `NULL`-as-default pattern used elsewhere in the C API.

**C API function signature:**

```c
QUIVER_C_API quiver_error_t quiver_database_export_scalars_csv(
    quiver_database_t* db,
    const char* collection,
    const char* path,
    const quiver_csv_export_options_t* options);

QUIVER_C_API quiver_error_t quiver_database_export_group_csv(
    quiver_database_t* db,
    const char* collection,
    const char* group,
    const char* path,
    const quiver_csv_export_options_t* options);
```

**Conversion in C API implementation (conceptual):**

```cpp
// In src/c/database_csv.cpp
static quiver::CsvExportOptions convert_options(const quiver_csv_export_options_t* opts) {
    quiver::CsvExportOptions result;
    if (opts && opts->enum_attribute_names && opts->enum_entry_count > 0) {
        for (size_t i = 0; i < opts->enum_entry_count; ++i) {
            result.enum_labels[opts->enum_attribute_names[i]][opts->enum_values[i]] =
                opts->enum_labels[i];
        }
    }
    if (opts && opts->date_time_format) {
        result.date_time_format = opts->date_time_format;
    }
    return result;
}
```

### 5. Binding-Level Options Patterns

Each binding constructs the flat C struct from its native map type:

**Julia:**
```julia
function export_scalars_csv(db::Database, collection::String, path::String;
                            enum_labels::Dict{String, Dict{Int64, String}} = Dict(),
                            date_time_format::String = "")
    # Convert Dict to parallel arrays, call C API
end
```

**Dart:**
```dart
void exportScalarsCsv(String collection, String path, {
  Map<String, Map<int, String>> enumLabels = const {},
  String dateTimeFormat = '',
})
```

**Lua:**
```lua
db:export_scalars_csv("Collection", "output.csv", {
    enum_labels = { status = { [0] = "Inactive", [1] = "Active" } },
    date_time_format = "%d/%m/%Y"
})
```

Lua's table syntax maps naturally to the nested map structure. The Lua binding (via sol2) reads the table and constructs the C++ `CsvExportOptions` directly, bypassing the C API flat struct entirely -- consistent with how `LuaRunner` already calls C++ methods directly.

### 6. API Surface: Two Functions, Not One

**Recommendation:** Split `export_csv` into `export_scalars_csv` and `export_group_csv`.

```cpp
// Exports label + all scalar attributes for every element in collection
void export_scalars_csv(const std::string& collection,
                        const std::string& path,
                        const CsvExportOptions& options = {});

// Exports label + group columns for every element (vector, set, or time series)
void export_group_csv(const std::string& collection,
                      const std::string& group,
                      const std::string& path,
                      const CsvExportOptions& options = {});
```

**Why two functions:**
- Scalars export produces one row per element (label + scalar attributes as columns).
- Group export produces N rows per element (label + group value columns). The structure is fundamentally different.
- The existing `export_csv(table, path)` stub conflates these two cases into one function parameterized by "table" -- but "table" is ambiguous (is it the collection name or the full vector table name like `Items_vector_values`?).
- Two explicit functions follow the project's naming convention: verb + category + type. `export_scalars_csv` and `export_group_csv` are unambiguous.

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Hand-rolled CSV writer (~15 lines) | csv2 or rapidcsv library | Only if CSV *import* is also needed in the same milestone. Since import is deferred (out of scope), a library adds nothing. |
| `strftime` for date formatting | `std::chrono::parse` + `std::format` | If sub-second precision or timezone conversion is needed. For reformatting SQLite TEXT dates, `strftime` is simpler and produces identical output. |
| `strftime` format specifiers for user API | `std::format` chrono specifiers (`{:%Y-%m-%d}`) | Never for Quiver -- `strftime` specifiers are language-agnostic (Julia, Dart, Lua all have analogues), while `std::format` syntax is C++-specific. |
| Flat parallel arrays for C API enum_labels | Opaque builder pattern (`quiver_csv_options_create()`, `quiver_csv_options_add_enum()`, `quiver_csv_options_free()`) | If the options struct becomes much more complex (10+ fields, deeply nested). For 2 fields (enum_labels + date_time_format), a flat struct is simpler and avoids lifecycle management. |
| Flat parallel arrays for C API enum_labels | Nested C struct (`quiver_enum_mapping_t` with inner array of key-value pairs) | Never -- nested structs with inner dynamic arrays are painful for FFI generators. Dart ffigen and Julia Clang.jl handle flat structs with pointers-to-arrays well; nested dynamic allocations cause binding bugs. |
| Two export functions (scalars + group) | One polymorphic `export_csv(table_or_collection, path, options)` | Never -- the output structure differs fundamentally (1 row/element vs N rows/element). A single function would need a flag or heuristic to determine which mode, violating the project's "clean code over defensive code" principle. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Any CSV library (csv2, rapidcsv, fast-cpp-csv-parser, etc.) | CSV writing is ~15 lines. These libraries focus on parsing, not writing. Adding a dependency for trivial logic violates "simple solutions over complex abstractions." | `std::ofstream` with a hand-rolled `write_csv_field()` function |
| `std::chrono::parse` + `std::format` for date reformatting | Over-engineered for string-to-string reformatting. Introduces C++-specific format syntax that doesn't translate to binding languages. `<format>` header is heavy and untested in this codebase. | `std::get_time` + `strftime` -- universally understood format specifiers, zero new includes |
| `localtime()` for date parsing | Thread-unsafe (shared static buffer). Not needed -- `std::get_time` fills a caller-owned `std::tm`. | `std::get_time` with `std::istringstream` |
| Builder pattern for C API options | Introduces lifecycle management (create/free), makes FFI generation harder, adds 4-6 C API functions for what can be a single struct. | Flat `quiver_csv_export_options_t` struct with parallel arrays (caller-owned, no alloc/free) |
| JSON or TOML config for CSV options | Absurd indirection for 2 configuration fields. Would require serialization/deserialization in every binding. | Direct struct/map in each language |
| `fprintf` for CSV output | No proper quoting. Would require manual buffer management for field escaping. | `std::ofstream` with C++ string operations |
| UTF-8 BOM in CSV output | Not needed. BOM is only required for Excel compatibility on Windows, and Quiver's CSV is for programmatic consumption. Adding BOM would corrupt the first field for non-Excel parsers. | Plain UTF-8 without BOM |

## Stack Patterns by Variant

**If CSV import is added in a future milestone:**
- Consider adding csv2 (header-only, supports both reading and writing) at that point.
- CSV parsing has genuine complexity (quoted fields with embedded newlines, encoding detection, type inference) that justifies a library.
- For now, export-only means hand-rolling is correct.

**If more complex options are needed later (e.g., column selection, filtering, custom delimiters):**
- Upgrade the flat `quiver_csv_export_options_t` struct with additional fields. Parallel arrays scale to any number of fields without changing the FFI pattern.
- If options exceed ~5-6 fields, consider an opaque builder pattern at that point. But not now.

**If sub-second or timezone-aware date formatting is needed:**
- Switch from `strftime` to `std::chrono::parse` + `std::format` in the C++ implementation.
- The C API format string stays the same (strftime specifiers are a subset of chrono format specifiers).
- This is a C++ internal change only; bindings are unaffected.

## Version Compatibility

| Component | Version | Compatible With | Notes |
|-----------|---------|-----------------|-------|
| `std::get_time` / `strftime` | C++11 standard library | All C++11+ compilers | Available in MSVC, GCC, Clang. Project uses C++20. |
| `std::ofstream` | C++ standard library | All compilers | Universally available. |
| `std::map` | C++ standard library | All compilers | Nested maps work in all C++11+ compilers. |
| `quiver_csv_export_options_t` (new struct) | N/A | Dart ffigen, Julia Clang.jl | Flat struct with `const char*`, `const int64_t*`, `size_t` -- all types already used in existing C API structs. FFI generators handle these correctly. |
| SQLite 3.50.2 | Already in project | All reads for CSV data | No SQLite API changes needed. CSV export reads data using existing `read_scalar_*`, `read_vector_*`, etc. |

## Sources

- [RFC 4180: Common Format and MIME Type for CSV Files](https://www.rfc-editor.org/rfc/rfc4180) -- HIGH confidence, official standard. Confirmed 3 quoting rules.
- [std::strftime cppreference](https://en.cppreference.com/w/cpp/chrono/c/strftime) -- HIGH confidence, standard library reference.
- [C++20 Chrono Extensions (MSVC Blog)](https://devblogs.microsoft.com/cppblog/cpp20s-extensions-to-chrono-available-in-visual-studio-2019-version-16-10/) -- HIGH confidence, confirms `std::chrono::parse` available in MSVC since VS2019 16.10.
- [C++ Chrono Calendar Types (C++ Stories)](https://www.cppstories.com/2025/chrono-calendar-types/) -- MEDIUM confidence, community source confirming C++20 chrono patterns.
- [fast-cpp-csv-parser GitHub](https://github.com/ben-strasser/fast-cpp-csv-parser) -- HIGH confidence, confirmed read-only (no write support).
- [csv2 GitHub](https://github.com/p-ranav/csv2) -- HIGH confidence, confirmed read+write but overkill for write-only use.
- [RFC 4180 edge cases (Hacker News)](https://news.ycombinator.com/item?id=31254151) -- MEDIUM confidence, community discussion confirming hand-rolling CSV writing is fine when you follow RFC 4180.
- [Dart FFI interop documentation](https://dart.dev/interop/c-interop) -- HIGH confidence, official docs. Confirms flat structs with primitive pointers work well for FFI.
- Quiver codebase analysis (existing C API patterns: `database_time_series.cpp`, `database_query.cpp`) -- HIGH confidence, primary source for parallel-arrays precedent.

---
*Stack research for: Quiver v0.4 -- CSV Export with Enum Resolution and Date Formatting*
*Researched: 2026-02-22*
