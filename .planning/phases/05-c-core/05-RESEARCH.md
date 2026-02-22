# Phase 5: C++ Core - Research

**Researched:** 2026-02-22
**Domain:** C++ CSV export implementation (RFC 4180 writer, enum resolution, date formatting)
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Scalar exports: columns appear in schema definition order (CREATE TABLE column order), with `label` as the first column header (literal "label", not collection name), `id` column excluded
- Group exports: column order is Claude's discretion based on schema structure; `label` replaces `id`
- All column headers use raw schema column names exactly as defined in CREATE TABLE (no renaming, no cleanup)
- Overwrite existing files silently -- no error if file exists
- LF line endings (`\n`), not CRLF -- deviation from strict RFC 4180, but more practical for modern tooling
- Create missing parent directories automatically (like `mkdir -p`)
- Return type is `void` -- success means file written, failure throws
- Empty `date_time_format` (default): pass through raw string values as stored in SQLite, no formatting applied
- Empty `enum_labels` (default): output raw integer values, no resolution
- Default options factory follows the same pattern as `DatabaseOptions`: free function `default_csv_export_options()` returning a `CSVExportOptions` struct
- Single method signature with default parameter: `export_csv(collection, group, path, options = default_csv_export_options())`
- `enum_labels` type: `std::unordered_map<std::string, std::unordered_map<int64_t, std::string>>` (attribute name -> {integer value -> label string})
- `date_time_format`: `std::string` (strftime format string, empty = no formatting)
- Header location: `include/quiver/csv.h` -- houses both export options/declarations now, and import in the future
- `export_csv()` is a method on the `Database` class, consistent with other operations
- Header file named `csv.h` (not `csv_export.h`) to accommodate future import functionality in the same header

### Claude's Discretion
- Whether to use a third-party CSV library (user approved adding one if beneficial, noting CSV import is a future requirement)
- UTF-8 BOM inclusion
- File open mode (binary vs text)
- Atomic write strategy (temp file + rename vs direct write)
- Group export column ordering logic
- CSVExportOptions as plain value type vs Pimpl

### Deferred Ideas (OUT OF SCOPE)
- CSV import (reading CSV into database) -- tracked in REQUIREMENTS.md as future requirement
- Column selection/filtering for CSV export -- tracked in REQUIREMENTS.md as future requirement
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CSV-01 | `export_csv(collection, group, path, options)` -- group="" exports scalars, group="name" exports that group with label replacing id | Schema introspection via `list_scalar_attributes()`, `get_vector_metadata()`, `get_set_metadata()`, `get_time_series_metadata()` provides column info; `execute()` method enables flexible SQL queries for data extraction |
| CSV-02 | CSV output is RFC 4180 compliant (header row, comma delimiter, double-quote escaping) | RFC 4180 escaping is simple enough to hand-roll; vincentlaucsb/csv-parser available if library preferred |
| CSV-03 | Empty collection writes header-only CSV (not an error) | Query returning zero rows naturally produces header-only output |
| CSV-04 | NULL values written as empty fields | `Row::is_null()` / `Value` variant with `nullptr_t` check enables NULL detection |
| OPT-01 | `CSVExportOptions` struct with `enum_labels` map and `date_time_format` string | Plain value type (Rule of Zero), no private dependencies to hide |
| OPT-02 | Enum resolution replaces integer values with labels; unmapped values fall back to raw integer | Lookup in `enum_labels[attribute_name][int_value]` at write time |
| OPT-03 | Date/time formatting via `strftime` on DateTime columns identified by metadata | Metadata `DataType::DateTime` identifies columns; manual ISO 8601 parse + `strftime` format (no `strptime` on Windows) |
| OPT-04 | `default_csv_export_options()` following DatabaseOptions pattern | Matches existing `default_database_options()` pattern exactly |
</phase_requirements>

## Summary

This phase implements CSV export in the C++ core library. The implementation is straightforward: query data from SQLite, format it as RFC 4180 CSV with optional enum resolution and date formatting, and write to a file. The primary complexity lies in three areas: (1) getting columns in schema definition order despite `TableDefinition::columns` being a `std::map` (alphabetically sorted), (2) DateTime formatting on Windows where `strptime` is unavailable, and (3) correctly routing scalar vs group exports through different SQL query patterns.

The existing codebase provides strong foundations: `Schema` for table introspection, `execute()` for parameterized queries returning typed `Result`/`Row` objects, metadata APIs for identifying DateTime columns and group structures, and `std::filesystem` for directory creation. The `export_csv` stub already exists with a 2-parameter signature that must be replaced with the 4-parameter design.

**Primary recommendation:** Hand-roll the CSV writer (RFC 4180 escaping is ~15 lines of code) rather than adding a third-party dependency. Implement a simple ISO 8601 parser using `sscanf` for the DateTime formatting requirement, since `strptime` is unavailable on MSVC and `std::chrono::parse` requires `/std:c++latest`. Open files in binary mode to control line endings precisely.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ Standard Library | C++20 | `<filesystem>`, `<fstream>`, `<ctime>`, `<string>`, `<unordered_map>` | Already used throughout project; no new dependencies needed |
| SQLite3 | 3.50.2 | Data source; `PRAGMA table_info` for column ordering | Already a project dependency |
| spdlog | 1.17.0 | Logging within `export_csv` | Already a project dependency |

### Supporting
No new libraries needed for this phase.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-rolled CSV writer | [vincentlaucsb/csv-parser](https://github.com/vincentlaucsb/csv-parser) | Library has RFC 4180 writer built-in and would benefit future CSV import; however adds a dependency for ~15 lines of escaping logic. The library is large (full parser + writer). If the user approves, it could be added via FetchContent for future import benefit. |
| Manual ISO 8601 parse | [HowardHinnant/date](https://github.com/HowardHinnant/date) | Mature date library but heavy dependency for a simple parse operation. C++20 `<chrono>` has this built in but requires `/std:c++latest` on MSVC. |
| `sscanf`-based datetime parser | `std::get_time` / `std::istringstream` | `std::get_time` is portable C++11 and works on MSVC; slightly more verbose than `sscanf` but safer. Either works. |

**Recommendation: No new dependencies.** The CSV writer is trivial. DateTime parsing needs only a simple helper function. Adding vincentlaucsb/csv-parser could be revisited when CSV import is implemented.

## Architecture Patterns

### Recommended File Structure
```
include/quiver/
  csv.h                    # CSVExportOptions struct + default_csv_export_options() free function
  database.h               # Updated export_csv() signature (4 params, not 2)

src/
  database_csv.cpp         # New file: export_csv() implementation + CSV writing helpers
  database_describe.cpp    # Remove empty export_csv/import_csv stubs (keep describe + import_csv stub)
```

### Pattern 1: Schema-Order Column Retrieval
**What:** `TableDefinition::columns` is `std::map<std::string, ColumnDefinition>` -- columns are stored in alphabetical order, NOT schema definition order. The user explicitly requires schema definition order for scalar exports.
**When to use:** Every scalar export.
**Solution:** Use `PRAGMA table_info(table_name)` via `execute()` which returns columns in CREATE TABLE definition order (by `cid`). The `execute()` method returns a `Result` whose `columns()` gives names in query order. Alternatively, run `SELECT * FROM collection LIMIT 0` and use `result.columns()` to get schema-ordered column names.
**Confidence:** HIGH -- verified by reading `schema.cpp` line 317-319 where `std::map` insertion loses ordering.

```cpp
// Get columns in schema definition order (not alphabetical)
auto result = execute("SELECT * FROM " + collection + " LIMIT 0");
auto ordered_columns = result.columns(); // schema definition order
// Filter out "id" column, keep "label" first
```

### Pattern 2: Existing Signature Change
**What:** The current stub is `export_csv(const std::string& table, const std::string& path)` (2 params). The new signature is `export_csv(collection, group, path, options)` (4 params).
**When to use:** Method declaration in `database.h` and C API wrapper in `src/c/database.cpp`.
**Impact:** This is a BREAKING CHANGE to the existing (unused) signature. Must update:
- `include/quiver/database.h` -- method declaration
- `src/database_describe.cpp` -- remove old stub (or move to new file)
- `src/c/database.cpp` -- C API wrapper signature
- Lua bindings in `src/lua_runner.cpp` -- binding registration
- Julia/Dart bindings are separate phases (6, 7)

```cpp
// Old (remove):
void export_csv(const std::string& table, const std::string& path);

// New:
void export_csv(const std::string& collection,
                const std::string& group,
                const std::string& path,
                const CSVExportOptions& options = default_csv_export_options());
```

### Pattern 3: Scalar Export SQL
**What:** For `group=""`, query all scalars from the collection table.
**SQL pattern:**
```sql
-- Get labels and all scalar columns (excluding id) in definition order
SELECT label, col1, col2, ... FROM Collection ORDER BY rowid
```
**Column list:** Derived from `PRAGMA table_info` or `SELECT * ... LIMIT 0`, filtered to exclude `id`.

### Pattern 4: Group Export SQL
**What:** For `group="name"`, query the group table joined with the collection table to get labels.
**SQL pattern:**
```sql
-- Vector group: join to get label, use schema column names
SELECT C.label, G.col1, G.col2, ...
FROM Collection_vector_values G
JOIN Collection C ON C.id = G.id
ORDER BY G.id, G.vector_index

-- Set group: similar but no vector_index
SELECT C.label, G.col1, G.col2, ...
FROM Collection_set_tags G
JOIN Collection C ON C.id = G.id
ORDER BY G.id

-- Time series group: similar, ordered by dimension
SELECT C.label, G.date_time, G.value, ...
FROM Collection_time_series_data G
JOIN Collection C ON C.id = G.id
ORDER BY G.id, G.date_time
```
**Column headers:** Use raw schema column names from the group table (skip `id`, skip `vector_index` for vectors), prepend `label`.

### Pattern 5: CSVExportOptions as Plain Value Type
**What:** No private dependencies to hide, so no Pimpl needed. Rule of Zero.
**Placement:** `include/quiver/csv.h`

```cpp
#ifndef QUIVER_CSV_H
#define QUIVER_CSV_H

#include "export.h"
#include <string>
#include <unordered_map>

namespace quiver {

struct QUIVER_API CSVExportOptions {
    std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> enum_labels;
    std::string date_time_format;
};

inline CSVExportOptions default_csv_export_options() {
    return {};
}

}  // namespace quiver
#endif
```

### Anti-Patterns to Avoid
- **Value inspection for DateTime:** Do NOT check if a string value "looks like" a date. Use `DataType::DateTime` from schema metadata to identify DateTime columns. The `is_date_time_column()` function and column metadata already handle this.
- **Alphabetical column ordering:** Do NOT iterate `TableDefinition::columns` (the `std::map`) for column ordering in scalar exports. It returns alphabetical order, not schema order. Use `PRAGMA table_info` or `SELECT * LIMIT 0` instead.
- **CRLF line endings:** Do NOT open file in text mode on Windows. Text mode auto-converts `\n` to `\r\n` on Windows. Open in binary mode (`std::ios::binary`) to ensure LF-only output per the user's decision.
- **Separate import_csv changes:** Do NOT modify `import_csv` in this phase. Leave the stub as-is. Import is deferred.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Directory creation | Manual recursive mkdir | `std::filesystem::create_directories()` | Already available in C++17/20; handles all edge cases |
| Column order discovery | Custom PRAGMA parser | `execute("SELECT * FROM table LIMIT 0").columns()` | Reuses existing `execute()` infrastructure; returns column names in schema order |
| DateTime column detection | String pattern matching on values | `ScalarMetadata::data_type == DataType::DateTime` or `GroupMetadata::value_columns[i].data_type` | Schema metadata is authoritative; value inspection is fragile |
| File path handling | Manual string manipulation | `std::filesystem::path` | Cross-platform, handles separators correctly |

**Key insight:** The existing Schema/Metadata/Result infrastructure provides everything needed. The only truly new logic is the CSV escaping function and the ISO 8601 datetime parser.

## Common Pitfalls

### Pitfall 1: Column Ordering from std::map
**What goes wrong:** Iterating `TableDefinition::columns` gives alphabetical order, not CREATE TABLE order. A table with columns `(id, label, zebra, alpha)` would output `alpha, id, label, zebra`.
**Why it happens:** `std::map<std::string, ColumnDefinition>` sorts by key.
**How to avoid:** Query column names via `execute("SELECT * FROM table LIMIT 0").columns()` or `execute("PRAGMA table_info(table)")` which preserves definition order.
**Warning signs:** Test CSV output has columns in wrong order despite schema being correct.

### Pitfall 2: Windows Text Mode Adds CRLF
**What goes wrong:** Opening file with `std::ofstream` (default text mode) on Windows converts `\n` to `\r\n`.
**Why it happens:** Windows text mode translation is default behavior.
**How to avoid:** Open file with `std::ios::binary` flag.
**Warning signs:** CSV files have `\r\n` line endings on Windows but `\n` on Linux.

### Pitfall 3: strptime Unavailable on Windows/MSVC
**What goes wrong:** Code compiles on Linux but fails on MSVC because `strptime` is a POSIX function not available in the Microsoft C runtime.
**Why it happens:** MSVC does not provide POSIX `strptime`. `std::chrono::parse` requires `/std:c++latest` which the project does not use (it uses C++20).
**How to avoid:** Use `std::get_time` with `std::istringstream` (portable C++ approach) or a manual `sscanf`-based parser for ISO 8601 format.
**Warning signs:** Compilation errors on Windows with `strptime` undefined.

### Pitfall 4: Enum Resolution on Non-Integer Columns
**What goes wrong:** Attempting enum lookup on a TEXT or REAL column where the value is not an integer.
**How to avoid:** Only apply enum resolution when the column's `DataType` is `Integer`. Check metadata before attempting resolution.
**Warning signs:** Crash or incorrect output when enum_labels contains an attribute name that matches a non-integer column.

### Pitfall 5: NULL Value Handling in Variants
**What goes wrong:** Calling `std::get<int64_t>` on a `Value` that holds `nullptr_t` throws `std::bad_variant_access`.
**How to avoid:** Always check `std::holds_alternative<std::nullptr_t>(value)` or use `Row::is_null()` before extracting typed values.
**Warning signs:** Runtime crash when exporting rows with NULL values.

### Pitfall 6: Quoting Empty Fields vs NULL Fields
**What goes wrong:** Empty string `""` and NULL both produce empty output in CSV, but they are semantically different. Per requirements, NULL should appear as empty field (no quotes needed).
**How to avoid:** NULL produces literally nothing between commas (`,`). Empty string produces `""` (quoted empty). Both are valid RFC 4180 but differ in meaning. For this implementation, NULL -> empty (unquoted), empty string -> `""`.
**Warning signs:** Round-trip fidelity issues if import is later added.

## Code Examples

### RFC 4180 CSV Escaping
```cpp
// Minimal RFC 4180 field escaping
static std::string csv_escape(const std::string& field) {
    bool needs_quoting = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quoting = true;
            break;
        }
    }
    if (!needs_quoting) {
        return field;
    }
    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";  // Double the quote
        } else {
            escaped += c;
        }
    }
    escaped += "\"";
    return escaped;
}
```

### ISO 8601 Parse + strftime Format
```cpp
// Parse ISO 8601 datetime string to struct tm (cross-platform, no strptime needed)
static bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream ss(datetime_str);
    // Try "YYYY-MM-DDTHH:MM:SS" format
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        // Try "YYYY-MM-DD HH:MM:SS" format (space separator)
        ss.clear();
        ss.str(datetime_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }
    return !ss.fail();
}

// Format datetime value using strftime
static std::string format_datetime(const std::string& raw_value, const std::string& format) {
    std::tm tm{};
    if (!parse_iso8601(raw_value, tm)) {
        return raw_value;  // Unparseable: return raw
    }
    char buffer[256];
    if (std::strftime(buffer, sizeof(buffer), format.c_str(), &tm) == 0) {
        return raw_value;  // Format error: return raw
    }
    return std::string(buffer);
}
```

### Value-to-String Conversion for CSV Cell
```cpp
static std::string value_to_csv_string(
    const Value& value,
    const std::string& column_name,
    DataType data_type,
    const CSVExportOptions& options)
{
    // NULL -> empty field
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return "";
    }

    // Integer with possible enum resolution
    if (std::holds_alternative<int64_t>(value)) {
        auto int_val = std::get<int64_t>(value);
        // Check enum_labels
        if (auto attr_it = options.enum_labels.find(column_name);
            attr_it != options.enum_labels.end()) {
            if (auto val_it = attr_it->second.find(int_val);
                val_it != attr_it->second.end()) {
                return csv_escape(val_it->second);
            }
        }
        return std::to_string(int_val);
    }

    // Float
    if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
        // Note: std::to_string(double) may produce trailing zeros
        // Consider snprintf for cleaner output
    }

    // String (may be DateTime)
    if (std::holds_alternative<std::string>(value)) {
        auto& str_val = std::get<std::string>(value);
        // DateTime formatting (by metadata, not value inspection)
        if (data_type == DataType::DateTime && !options.date_time_format.empty()) {
            return csv_escape(format_datetime(str_val, options.date_time_format));
        }
        return csv_escape(str_val);
    }

    return "";
}
```

### Writing CSV File
```cpp
void Database::export_csv(const std::string& collection,
                          const std::string& group,
                          const std::string& path,
                          const CSVExportOptions& options) {
    namespace fs = std::filesystem;

    // Create parent directories
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }

    // Open in binary mode to control line endings (LF only)
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to export_csv: could not open file: " + path);
    }

    if (group.empty()) {
        export_csv_scalars(collection, file, options);
    } else {
        export_csv_group(collection, group, file, options);
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `strptime` for datetime parsing | `std::get_time` (C++11) or `std::chrono::parse` (C++20/latest) | C++11/C++20 | `strptime` unavailable on MSVC; `std::get_time` is the portable C++ equivalent |
| `fopen`/`fprintf` for file writing | `std::ofstream` with `std::ios::binary` | C++98 | Modern C++ standard; consistent with rest of codebase |
| Custom directory creation | `std::filesystem::create_directories` | C++17 | Standard library handles all edge cases |

**Deprecated/outdated:**
- `strptime`: POSIX-only, not available on MSVC. Use `std::get_time` instead.
- Opening files in text mode for CSV: Produces platform-dependent line endings. Use binary mode for consistent LF output.

## Open Questions

1. **Float formatting precision**
   - What we know: `std::to_string(double)` uses `%f` format which produces trailing zeros (e.g., `3.140000`). This is technically correct but aesthetically unpleasant.
   - What's unclear: Whether the user prefers minimal float representation (e.g., `3.14`) or full precision.
   - Recommendation: Use `snprintf(buf, sizeof(buf), "%g", value)` which strips trailing zeros while preserving precision. Or use `std::format("{}", value)` if available. This is Claude's discretion.

2. **Group export column ordering for vectors**
   - What we know: Vector tables have `id`, `vector_index`, and value columns. User decided group column ordering is Claude's discretion.
   - Recommendation: For vectors: `label, [value columns in schema order]` (skip `id` and `vector_index`). For sets: `label, [value columns in schema order]` (skip `id`). For time series: `label, dimension_column, [value columns in schema order]` (skip `id`).

3. **UTF-8 BOM**
   - What we know: User left this as Claude's discretion.
   - Recommendation: No BOM. BOM is unnecessary for UTF-8 and can cause issues with many CSV parsers and Unix tools. Only Excel on Windows historically needed BOM, and modern Excel handles UTF-8 without it.

## Sources

### Primary (HIGH confidence)
- Codebase analysis: `include/quiver/schema.h`, `src/schema.cpp` -- verified `std::map` column ordering issue
- Codebase analysis: `src/database.cpp` -- verified `execute()` returns columns in query order
- Codebase analysis: `include/quiver/data_type.h` -- verified `DataType::DateTime` and `is_date_time_column()`
- Codebase analysis: `include/quiver/attribute_metadata.h` -- verified `ScalarMetadata` and `GroupMetadata` structures
- Codebase analysis: `src/database_describe.cpp` -- verified existing empty `export_csv` stub
- Codebase analysis: `src/c/database.cpp` lines 133-155 -- verified existing C API wrapper stubs
- Codebase analysis: `cmake/Dependencies.cmake` -- verified FetchContent pattern for adding dependencies
- [cppreference std::strftime](https://en.cppreference.com/w/cpp/chrono/c/strftime) -- strftime format specifiers
- [Microsoft strftime docs](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strftime-wcsftime-strftime-l-wcsftime-l?view=msvc-170) -- MSVC-specific strftime behavior

### Secondary (MEDIUM confidence)
- [vincentlaucsb/csv-parser](https://github.com/vincentlaucsb/csv-parser) -- CSV library option with read+write support
- [C++20 chrono on MSVC](https://devblogs.microsoft.com/cppblog/cpp20s-extensions-to-chrono-available-in-visual-studio-2019-version-16-10/) -- `std::chrono::parse` requires `/std:c++latest` on MSVC

### Tertiary (LOW confidence)
- None. All findings verified against codebase or official documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies needed; all infrastructure exists in codebase
- Architecture: HIGH -- patterns derived directly from existing codebase (execute, metadata, schema)
- Pitfalls: HIGH -- column ordering issue verified by reading `schema.cpp`; strptime issue verified by MSVC documentation; binary mode requirement verified by Windows text mode behavior

**Research date:** 2026-02-22
**Valid until:** 2026-03-22 (stable domain; no rapidly changing dependencies)
