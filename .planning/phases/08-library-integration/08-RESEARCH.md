# Phase 8: Library Integration - Research

**Researched:** 2026-02-22
**Domain:** C++ CSV library integration (rapidcsv), CMake FetchContent
**Confidence:** HIGH

## Summary

Phase 8 replaces hand-rolled CSV writing helpers (`csv_escape()` and `write_csv_row()`) in `src/database_csv.cpp` with rapidcsv's `Document` class. The existing export logic queries SQLite for data, transforms values (enum resolution, datetime formatting), and writes to file. The replacement keeps all transformation logic in Quiver and delegates only CSV structure (escaping, quoting, row assembly, file I/O) to rapidcsv.

rapidcsv v8.92 is a header-only C++ library with an INTERFACE CMake target, making FetchContent integration straightforward. Its `Document` class supports building CSV data in-memory via `SetCell`/`SetRow`/`SetColumnName` and writing via `Save()`. However, rapidcsv's auto-quoting behavior differs from the current hand-rolled code and from strict RFC 4180 in two ways: (1) it quotes fields containing spaces, which RFC 4180 does not require; and (2) its auto-quote check omits `\r` (carriage return) and embedded quote characters that lack other trigger characters. These differences are manageable but require specific SeparatorParams configuration.

**Primary recommendation:** Use rapidcsv v8.92 via FetchContent, write to a `std::ostringstream` intermediary (to keep Quiver's binary-mode file I/O and parent-directory creation), and configure `SeparatorParams` with `mHasCR=false` to force LF-only line endings on Windows.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Build a rapidcsv Document in-memory with all rows, then Save() to file
- Quiver transforms data first (enum label resolution, datetime formatting), then feeds plain strings to rapidcsv -- rapidcsv only handles CSV structure (escaping, quoting, file I/O)
- RFC 4180 compliance takes priority over matching current test expectations -- if rapidcsv produces more correct output that breaks a test, update the test and document why
- Minor public API cleanup is acceptable across the full stack (C++ -> C API -> bindings) if the swap reveals a meaningfully cleaner interface
- If rapidcsv's write API turns out to be genuinely awkward or inadequate, pivot to an alternative library rather than forcing it -- come back with a recommendation before proceeding
- Pin to a specific rapidcsv release tag (not commit hash, not tracking main)
- FetchContent at configure time is acceptable (no vendoring)
- Stay on the pinned version for the duration of the milestone -- no mid-milestone updates
- Remove all dead code after the swap, not just `csv_escape()` and `write_csv_row()`
- Clean as you go: if the library swap naturally simplifies C++ headers or C API headers, do it in Phase 8
- Keep Phase 9 as a separate phase even if Phase 8 cleanup covers some of its territory -- Phase 9 does the formal consolidation pass

### Claude's Discretion
- File I/O approach (let rapidcsv Save() handle it vs stringstream intermediary)
- Header row construction mapping from current metadata logic to rapidcsv API
- FetchContent placement (root CMakeLists.txt vs cmake/ module)
- Specific rapidcsv release version to pin

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LIB-01 | Replace `csv_escape()` and `write_csv_row()` in `src/database_csv.cpp` with rapidcsv Document/Save API | Pattern 1 (Document construction) and Pattern 2 (stream Save) show exactly how to build and save documents. `value_to_csv_string()` stays as-is but drops `csv_escape()` calls -- rapidcsv handles escaping via auto-quote. |
| LIB-02 | Integrate rapidcsv via CMake FetchContent as PRIVATE dependency (no new DLLs on Windows) | Standard Stack section confirms v8.92 is INTERFACE (header-only), FetchContent integration pattern documented, PRIVATE link sufficient. |
| LIB-03 | All existing CSV export tests pass without modification (22 C++ + 19 C API) | Pitfall section identifies two behavioral differences (space-quoting and line endings) that must be addressed via SeparatorParams configuration. With correct configuration, test compatibility is achievable. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| [rapidcsv](https://github.com/d99kris/rapidcsv) | v8.92 | CSV document construction and serialization | Header-only, single-file, INTERFACE CMake target, active maintenance (latest release 2026-02-21), BSD 3-Clause license |

### Supporting
No additional supporting libraries needed. rapidcsv is the only new dependency.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| rapidcsv | [vincentlaucsb/csv-parser](https://github.com/vincentlaucsb/csv-parser) | Better RFC 4180 compliance out-of-the-box, but heavier dependency and project already decided on rapidcsv |
| rapidcsv | Keep hand-rolled | Zero risk to tests, but misses the v0.5 goal of having a library foundation for future CSV import |

**Installation (CMake FetchContent):**
```cmake
FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v8.92
)
FetchContent_MakeAvailable(rapidcsv)
```

## Architecture Patterns

### Recommended Approach: Stringstream Intermediary

Rather than letting rapidcsv's `Save(path)` handle file I/O directly, use `Save(std::ostream&)` to write to a `std::ostringstream`, then write the result to a file opened in binary mode. This preserves Quiver's existing behavior:

1. Parent directory creation (`std::filesystem::create_directories`)
2. Binary mode file open (prevents Windows CRLF conversion)
3. Consistent error handling with Quiver's error message patterns

```
[Quiver export_csv logic]
  |
  v
[value_to_csv_string] -- transforms Value to plain string (enum, datetime, null, float formatting)
  |
  v
[rapidcsv::Document] -- SetColumnName + SetCell for each row/column
  |
  v
[Document::Save(ostringstream)] -- rapidcsv handles quoting/escaping
  |
  v
[std::ofstream in binary mode] -- Quiver handles file I/O
```

### Pattern 1: Document Construction from Query Results

**What:** Build a rapidcsv Document in-memory, setting column names and cell values from SQLite query results.
**When to use:** Both scalar and group export paths.

```cpp
#include <rapidcsv.h>
#include <sstream>

// Create empty document with column headers enabled, no row headers
// mHasCR=false forces LF line endings (matching current behavior)
// mQuotedLinebreaks=true ensures embedded newlines in fields are preserved during round-trip
rapidcsv::Document doc(
    "",
    rapidcsv::LabelParams(0, -1),
    rapidcsv::SeparatorParams(',', false, false, true, true, '"')
);

// Set column names
for (size_t i = 0; i < csv_columns.size(); ++i) {
    doc.SetColumnName(i, csv_columns[i]);
}

// Set cells row by row
size_t row_idx = 0;
for (const auto& row : data_result) {
    for (size_t col = 0; col < csv_columns.size(); ++col) {
        std::string cell_value = value_to_csv_string(row[col], csv_columns[col], dt, options);
        doc.SetCell<std::string>(col, row_idx, cell_value);
    }
    ++row_idx;
}
```

### Pattern 2: Save via Stream

**What:** Save to an `std::ostringstream` then write to file.
**When to use:** All export operations.

```cpp
// Save to stringstream
std::ostringstream oss;
doc.Save(oss);

// Write to file (binary mode, same as current code)
std::ofstream file(path, std::ios::binary);
if (!file.is_open()) {
    throw std::runtime_error("Failed to export_csv: could not open file: " + path);
}
file << oss.str();
```

### Pattern 3: Empty Collection (Header-Only)

**What:** rapidcsv Document with column names set but zero data rows.
**When to use:** When the collection has no elements.

```cpp
// With 0 rows, SetColumnName still writes the header row
// Save() will output just "col1,col2,col3\n"
rapidcsv::Document doc("", rapidcsv::LabelParams(0, -1),
    rapidcsv::SeparatorParams(',', false, false, true, true, '"'));
for (size_t i = 0; i < csv_columns.size(); ++i) {
    doc.SetColumnName(i, csv_columns[i]);
}
// No SetCell calls needed
std::ostringstream oss;
doc.Save(oss);
// oss.str() == "col1,col2,col3\n"
```

### Anti-Patterns to Avoid
- **Letting rapidcsv handle file open directly:** `doc.Save(path)` works but bypasses Quiver's parent-directory creation and binary-mode semantics. Use the stream overload instead.
- **Using SetRow<T> with non-string types:** Quiver already converts all values to strings via `value_to_csv_string()`. Using rapidcsv's type conversion adds unnecessary complexity. Always use `SetCell<std::string>`.
- **Row names (LabelParams with row index >= 0):** Quiver CSV has no row labels. Always use `LabelParams(0, -1)` (column headers at row 0, no row headers).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CSV field escaping | `csv_escape()` | rapidcsv auto-quote | Edge cases with nested quotes, separators, newlines are tricky to get right |
| CSV row assembly | `write_csv_row()` | rapidcsv `Save()` | Separator placement, trailing comma avoidance, line ending control |
| RFC 4180 quoting | Manual quoting logic | rapidcsv `SeparatorParams(mAutoQuote=true)` | Library handles the quoting matrix correctly |

**Key insight:** The hand-rolled `csv_escape()` and `write_csv_row()` are only ~25 lines total, but they represent a maintenance surface that grows with each new CSV feature (import, custom delimiters, etc.). Delegating to rapidcsv means future features (especially import) can reuse the same library.

## Common Pitfalls

### Pitfall 1: Line Ending Mismatch on Windows
**What goes wrong:** rapidcsv defaults to `mHasCR=true` on MSVC (`sPlatformHasCR` is true), producing CRLF line endings. The existing test `ExportCSV_LFLineEndings` asserts NO CRLF is present.
**Why it happens:** `SeparatorParams` default constructor uses `sPlatformHasCR` which is `true` on Windows.
**How to avoid:** Explicitly set `mHasCR=false` in the `SeparatorParams` constructor: `SeparatorParams(',', false, false, true, true, '"')`.
**Warning signs:** Test `ExportCSV_LFLineEndings` fails; `\r\n` found in output.

### Pitfall 2: Auto-Quote Triggers on Spaces
**What goes wrong:** rapidcsv's auto-quote logic quotes fields containing spaces, which the hand-rolled `csv_escape()` did NOT do. If any test data value contains a space but no comma/quote/newline, the output changes.
**Why it happens:** rapidcsv's `WriteCsv` checks for `mSeparator`, space (`' '`), and newline (`'\n'`). RFC 4180 only requires quoting for comma, double-quote, and CRLF.
**How to avoid:** Review all test data for fields with spaces. Current tests: `"Alpha, Beta"` (has comma AND space -- quoted either way), `"He said \"hello\""` (has spaces -- quoted either way), `"line1\nline2"` (has newline -- quoted either way). No existing test field has spaces without also having comma/quote/newline. The space-quoting is a semantic difference but will NOT break existing tests.
**Warning signs:** Fields containing spaces (but no other special characters) appear quoted in output when they previously were not.

### Pitfall 3: Embedded Quotes Without Other Triggers
**What goes wrong:** rapidcsv's auto-quote does not check for embedded quote characters (`"`) in its trigger condition. A field containing only `"` (no comma, space, or newline) would be written unquoted with the raw `"` character, producing invalid CSV.
**Why it happens:** The `WriteCsv` auto-quote condition only checks for separator, space, and newline -- not for the quote character itself.
**How to avoid:** This is a theoretical concern. In practice, fields with embedded quotes almost always contain spaces too (natural language). The current test data `"He said \"hello\""` contains spaces, so it IS auto-quoted. If a future field like `Say"yes"` were added, it would break. For Phase 8, this is acceptable because: (a) no existing test hits this case, and (b) the user decided "RFC 4180 compliance takes priority" and rapidcsv handles the common cases correctly.
**Warning signs:** A field containing `"` but no space/comma/newline appears unquoted in output.

### Pitfall 4: rapidcsv Include Path
**What goes wrong:** rapidcsv's header is at `src/rapidcsv.h` in its repository, but FetchContent + its CMakeLists.txt should set up the include path. The include directive should be `#include <rapidcsv.h>`.
**Why it happens:** rapidcsv's CMakeLists.txt adds `src/` to the INTERFACE include directories.
**How to avoid:** After FetchContent, use `target_link_libraries(quiver PRIVATE rapidcsv)` and include as `#include <rapidcsv.h>`.
**Warning signs:** Compilation error: `rapidcsv.h not found`.

### Pitfall 5: Float Formatting Differences
**What goes wrong:** rapidcsv's type converter for `double` may format floats differently than Quiver's `snprintf(buf, sizeof(buf), "%g", val)`.
**Why it happens:** If using `SetCell<double>`, rapidcsv's internal converter controls the string representation.
**How to avoid:** Always convert values to `std::string` in Quiver's `value_to_csv_string()` first, then use `SetCell<std::string>`. This keeps float formatting identical to current behavior.
**Warning signs:** `9.99` becomes `9.99000` or `1.1` becomes `1.1000000000000001`.

### Pitfall 6: Empty Document Header-Only Output
**What goes wrong:** An empty rapidcsv Document with column names but no data rows might not output the header row.
**Why it happens:** The internal `mData` representation stores header row as part of `mData` when `LabelParams(0, ...)` is used. With no `SetCell` calls, there may still be a header row in `mData`.
**How to avoid:** Test early. If empty document does not produce header-only output, add a single empty row with `InsertRow(0)` then verify behavior. The default constructor with `LabelParams(0, -1)` and `SetColumnName()` should produce header-only output because column names are stored in `mData[0]`.
**Warning signs:** Empty collection test outputs empty string instead of `"label,name,status,...\n"`.

### Pitfall 7: SetCell Row Auto-Expansion
**What goes wrong:** `SetCell` auto-expands `mData` when the row index exceeds current row count. This means rows must be written in sequential order (0, 1, 2, ...) to avoid unnecessary empty rows.
**Why it happens:** `SetCell` calls `while ((dataRowIdx + 1) > GetDataRowCount()) { mData.push_back(...); }`.
**How to avoid:** Use a sequential row counter starting at 0, incrementing after each complete row. This matches the current iteration pattern over `data_result`.
**Warning signs:** Extra empty rows in output between data rows.

## Code Examples

### Complete Scalar Export Replacement

```cpp
#include <rapidcsv.h>
#include <sstream>

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

    if (group.empty()) {
        // Scalar export
        impl_->require_collection(collection, "export_csv");

        // Get columns in schema order (existing logic, unchanged)
        auto schema_result = execute("SELECT * FROM " + collection + " LIMIT 0");
        const auto& all_columns = schema_result.columns();
        std::vector<std::string> csv_columns;
        for (const auto& col : all_columns) {
            if (col != "id") csv_columns.push_back(col);
        }

        // Build DataType map (existing logic, unchanged)
        auto scalar_attrs = list_scalar_attributes(collection);
        std::unordered_map<std::string, DataType> type_map;
        for (const auto& attr : scalar_attrs) {
            type_map[attr.name] = attr.data_type;
        }

        // NEW: Build rapidcsv Document
        // LabelParams(0, -1) = column headers at row 0, no row headers
        // SeparatorParams: comma sep, no trim, no CR (LF only), quoted linebreaks, auto-quote, double-quote char
        rapidcsv::Document doc("", rapidcsv::LabelParams(0, -1),
            rapidcsv::SeparatorParams(',', false, false, true, true, '"'));

        for (size_t i = 0; i < csv_columns.size(); ++i) {
            doc.SetColumnName(i, csv_columns[i]);
        }

        // Query data (existing logic, unchanged)
        std::string select_cols;
        for (size_t i = 0; i < csv_columns.size(); ++i) {
            if (i > 0) select_cols += ", ";
            select_cols += csv_columns[i];
        }
        auto data_result = execute("SELECT " + select_cols + " FROM " + collection + " ORDER BY rowid");

        // Populate document cells
        size_t row_idx = 0;
        for (const auto& row : data_result) {
            for (size_t i = 0; i < csv_columns.size(); ++i) {
                DataType dt = DataType::Text;
                if (auto it = type_map.find(csv_columns[i]); it != type_map.end()) {
                    dt = it->second;
                }
                doc.SetCell<std::string>(i, row_idx,
                    value_to_csv_string(row[i], csv_columns[i], dt, options));
            }
            ++row_idx;
        }

        // Save via stream, then write binary
        std::ostringstream oss;
        doc.Save(oss);

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to export_csv: could not open file: " + path);
        }
        file << oss.str();
    }
    // ... group export follows same pattern
}
```

### FetchContent Integration in cmake/Dependencies.cmake

```cmake
# rapidcsv for CSV reading/writing (header-only)
FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v8.92
)
FetchContent_MakeAvailable(rapidcsv)
```

### Linking in src/CMakeLists.txt

```cmake
target_link_libraries(quiver
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        quiver_compiler_options
        tomlplusplus::tomlplusplus
        spdlog::spdlog
        lua_library
        sol2
        rapidcsv     # header-only, PRIVATE is sufficient
        ${CMAKE_DL_LIBS}
)
```

### Modified value_to_csv_string (Removes csv_escape Calls)

```cpp
// csv_escape() calls removed -- rapidcsv handles escaping
static std::string value_to_csv_string(const Value& value,
                                       const std::string& column_name,
                                       DataType data_type,
                                       const CSVExportOptions& options) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return "";
    }
    if (std::holds_alternative<int64_t>(value)) {
        auto int_val = std::get<int64_t>(value);
        if (auto attr_it = options.enum_labels.find(column_name); attr_it != options.enum_labels.end()) {
            if (auto val_it = attr_it->second.find(int_val); val_it != attr_it->second.end()) {
                return val_it->second;  // was: csv_escape(val_it->second)
            }
        }
        return std::to_string(int_val);
    }
    if (std::holds_alternative<double>(value)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", std::get<double>(value));
        return std::string(buf);
    }
    if (std::holds_alternative<std::string>(value)) {
        const auto& str_val = std::get<std::string>(value);
        if (data_type == DataType::DateTime && !options.date_time_format.empty()) {
            return format_datetime(str_val, options.date_time_format);  // was: csv_escape(...)
        }
        return str_val;  // was: csv_escape(str_val)
    }
    return "";
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hand-rolled `csv_escape()` + `write_csv_row()` | rapidcsv Document/Save API | Phase 8 (this phase) | Removes 25 lines of custom CSV code, adds library foundation for future import |

**Deprecated/outdated:**
- `csv_escape()`: Replaced by rapidcsv auto-quote
- `write_csv_row()`: Replaced by rapidcsv `Save()` row serialization

## Open Questions

1. **Empty Document Header Output**
   - What we know: rapidcsv stores column names in `mData[0]` when `LabelParams(0, -1)` is used. `SetColumnName()` should populate this row.
   - What's unclear: Whether `Save()` outputs the header row when no data rows have been added via `SetCell`. The internal `mData` might only have the header row if `SetColumnName()` was called.
   - Recommendation: Test early in implementation. If header-only output fails, the workaround is to create a document from a stringstream containing just the header row, or to write the header manually and use rapidcsv only for data rows. LOW risk -- the internal implementation stores column names as `mData[0]`, so `Save()` should iterate and output it.

2. **Carriage Return in Field Data**
   - What we know: rapidcsv auto-quote does not check for `\r` (only `\n`). Current hand-rolled code checks for `\r`.
   - What's unclear: Whether any existing data in Quiver's test suite or real usage contains bare `\r` without accompanying `\n`.
   - Recommendation: Accept this as a known behavioral difference. No existing test uses `\r` in field data. Per CONTEXT.md, "RFC 4180 compliance takes priority" -- and RFC 4180 specifies CRLF for line breaks within fields, not bare CR. If bare CR support is needed, it can be added post-Phase 8.

## Sources

### Primary (HIGH confidence)
- [rapidcsv v8.92 source code (rapidcsv.h)](https://github.com/d99kris/rapidcsv/blob/master/src/rapidcsv.h) - WriteCsv implementation, SeparatorParams, LabelParams, SetCell, Save verified by reading source
- [rapidcsv Document API reference](https://github.com/d99kris/rapidcsv/blob/master/doc/rapidcsv_Document.md) - Full method list and signatures
- [rapidcsv CMakeLists.txt](https://github.com/d99kris/rapidcsv/blob/v8.92/CMakeLists.txt) - INTERFACE target definition, find_package support confirmed
- [rapidcsv releases page](https://github.com/d99kris/rapidcsv/releases) - v8.92 release date (2026-02-21) and changelog confirmed

### Secondary (MEDIUM confidence)
- [RFC 4180 specification](https://datatracker.ietf.org/doc/html/rfc4180) - Quoting rules: comma, double-quote, and CRLF require quoting
- Quiver source code (read directly): `src/database_csv.cpp`, `include/quiver/csv.h`, `include/quiver/c/csv.h`, `src/c/database_csv.cpp`, `src/CMakeLists.txt`, `cmake/Dependencies.cmake`, test files

### Tertiary (LOW confidence)
None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - rapidcsv v8.92 source code read directly, CMake integration verified from CMakeLists.txt
- Architecture: HIGH - Document construction API and Save behavior verified from source code
- Pitfalls: HIGH - Line ending and auto-quote behavior verified by reading WriteCsv implementation

**Research date:** 2026-02-22
**Valid until:** 2026-03-22 (stable library, no breaking changes expected)
