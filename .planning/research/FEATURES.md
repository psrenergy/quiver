# Feature Research: CSV Library Replacement

**Domain:** CSV library selection for C++ -- replacing a hand-rolled RFC 4180 writer in a SQLite wrapper
**Researched:** 2026-02-22
**Confidence:** HIGH

## Context

Quiver v0.4 shipped a hand-rolled CSV writer (~15 lines of C++ in `src/database_csv.cpp`). It covers the exact behaviors the existing tests exercise: RFC 4180 quoting of commas/double-quotes/newlines, NULL as empty field, LF line endings, header row, float formatting via `%g`, enum label resolution, and DateTime formatting. The v0.5 milestone replaces this with a proper CSV library that also enables future CSV import, and consolidates `include/quiver/c/csv.h` into `include/quiver/c/options.h`.

The research question is: what does a CSV library provide versus the hand-rolled writer, and what matters for selecting one?

---

## Feature Landscape

### Table Stakes for a CSV Library (Write Path)

Features a CSV library must provide to be a drop-in replacement for the existing hand-rolled writer. Missing any of these means the library cannot replace what Quiver already has.

| Feature | Why Expected | Complexity to Get Right | Notes |
|---------|--------------|------------------------|-------|
| RFC 4180 quoting: comma, double-quote, newline in fields | Every CSV parser downstream (Excel, pandas, R) requires this. The existing hand-rolled writer handles all three. A library that does not auto-quote on write produces broken output for any field with special characters. | LOW (conceptually), but easy to get subtly wrong | The existing writer wraps fields in `"..."` and doubles internal `"` characters. This must be preserved exactly. |
| Automatic quoting only when needed (minimal quoting) | Quoting every field is valid RFC 4180 but bloats files and makes diffs unreadable. The hand-rolled writer quotes only when necessary. Libraries that always-quote every field would change output format even if semantically equivalent. | LOW | vincentlaucsb/csv-parser's `make_csv_writer` defaults to minimal quoting, with opt-in to universal quoting. csv2 does not auto-quote at all. |
| LF line endings (not CRLF) | RFC 4180 specifies CRLF, but Quiver deliberately uses LF (binary mode `std::ios::binary`) because CRLF breaks Unix tools and the test suite asserts `\r\n` absence. A library that forces CRLF would break all existing tests. | LOW | Must be configurable or the library must write to a pre-opened `std::ofstream` (which Quiver controls in binary mode). |
| Write to `std::ofstream` (not just to named file) | Quiver opens the file itself (creates parent directories, opens binary mode). The CSV library should accept an already-open stream, not a path string. | LOW | All surveyed libraries (csv2, vincentlaucsb, rapidcsv) write to streams. |
| Header row control | The hand-rolled writer writes the header as an ordinary row (no distinction from data rows). A library that inserts a header automatically would duplicate it. | LOW | Most libraries treat the header as a regular row the caller writes first. |
| Write rows of `std::vector<std::string>` | Quiver pre-serializes all values to strings (applying enum maps, datetime formatting, float formatting) before writing. The CSV layer only needs to join fields with proper escaping. | LOW | This is the minimal write API. csv2's `write_row(vector<string>)` and vincentlaucsb's `operator<<(array)` both support this pattern. |
| No mandatory type inference on write | The library should not try to re-interpret field values as numbers or dates. Quiver has already converted every value to its final string form before the CSV library sees it. | LOW | Write-only or write-first libraries impose no type interpretation. |

### Table Stakes for a CSV Library (Read Path)

Features needed to support future CSV import. The v0.5 milestone does not implement import, but selecting a write-only library now would require replacing it again when import is added.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| RFC 4180 compliant reading | Any library that passes the RFC 4180 conformance test suite (quoted fields, escaped quotes, embedded newlines, optional trailing newline). Import of Quiver-generated CSV files must round-trip cleanly. | MEDIUM | vincentlaucsb/csv-parser explicitly targets RFC 4180. csv2 handles quoted fields. rapidcsv handles common cases but does not explicitly document RFC 4180 conformance. |
| Header row detection | Import needs to read the column names from the first row and map them to schema attributes. Library should surface the header row separately from data rows, or at minimum let the caller treat row 0 as headers. | LOW | All surveyed libraries support header row access by column name. |
| Per-row iteration without loading entire file into memory | Even moderate SQLite databases can produce CSV files with hundreds of thousands of rows. Iterating row-by-row avoids loading the whole file into a `vector`. | MEDIUM | vincentlaucsb/csv-parser is stream-based (input iterator, configurable 10MB chunk size). csv2 uses memory mapping. rapidcsv loads the entire document into memory -- not suitable for large imports. |
| Column access by name | Import maps CSV column names to schema attribute names. A library that only provides positional access requires the caller to maintain a `column_name -> index` map. | LOW | vincentlaucsb/csv-parser provides `row["column_name"]` as O(1). rapidcsv provides column-by-name access. csv2 provides only positional access via iterators. |
| Raw string access per field (no forced type conversion) | Quiver's import layer will perform its own type conversion and validation (string -> int64, string -> double, string -> datetime). The CSV library should give raw string values and not fail on conversion errors. | LOW | All surveyed libraries provide raw string access. vincentlaucsb/csv-parser uses lazy conversion. |
| Custom delimiter support | Future import might need to handle semicolon-separated files from European locales. Not a v0.5 requirement, but useful to have available. | LOW | All surveyed libraries support configurable delimiters. The writer should also support this even if Quiver only exposes comma-delimited output initially. |

### Differentiators (Nice to Have, Not Blocking)

Features that go beyond the minimum requirements and could be leveraged in future milestones.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Type inference on read | Automatically detect column types (integer, float, string, boolean) from the CSV content. pandas does this via `infer_dtypes`. Useful for import when the schema provides expected types and the library can validate against them. | MEDIUM | vincentlaucsb/csv-parser provides `CSVField` with `.get<int>()`, `.get<double>()`, and automatic numeric detection. Not needed for v0.5 but would reduce import boilerplate. |
| Automatic delimiter detection | Library sniffs whether a file uses `,`, `;`, `\t`, or `|` as the delimiter. Reduces friction for import from external sources. | MEDIUM | vincentlaucsb/csv-parser has automatic delimiter detection as an explicit feature. None of the others do. |
| Quote-all mode | Write every field in double quotes regardless of content. Useful for tools that expect universal quoting. Not Quiver's current behavior but cheap to add if the library supports it. | LOW | vincentlaucsb `make_csv_writer(stream, false)` enables universal quoting. csv2 does not support this. |
| Memory-mapped reading | For large files, memory mapping avoids repeated `read()` syscalls. csv2 uses memory mapping for its reader. | LOW | Relevant only for very large import files. Quiver's SQLite single-connection model means imports are unlikely to involve multi-gigabyte files. |
| Row/column count without full parse | Some reporting scenarios need to know how many rows a CSV has before parsing it. | LOW | Not relevant for Quiver's use case. Import iterates all rows anyway. |

### Anti-Features (Features That Seem Good But Aren't for Quiver)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Forced CRLF line endings | RFC 4180 specifies CRLF. Libraries that hard-code CRLF "for compliance" would break Quiver's tests, which assert LF-only output (binary mode write was deliberately chosen). | Changes existing behavior, breaks test assertions on `\r\n` absence, breaks Unix tooling. | Open `std::ofstream` in binary mode before passing to the library. OR pick a library that writes to a caller-provided stream and does not inject line endings internally. |
| Library-managed file open (path parameter to writer) | csv2 `Reader` and some library writers take a file path and manage the `fstream` internally. | Quiver needs control of `std::ios::binary` and `fs::create_directories()`. A library that opens the file internally removes that control. | Require a library whose writer constructor takes `std::ofstream&`, not a path string. |
| Mandatory header row injection | Libraries that auto-write a header from column metadata would conflict with Quiver's approach of writing the header as a normal row (already pre-formatted). | Produces duplicate header or incorrect header format. | Treat header as just another row the caller writes first. |
| Type-annotated cell output | Some libraries encode the type in the written output (e.g., quoting all strings, not quoting numbers). CSV has no type annotations -- everything is text. Over-engineering the write side. | Forces the library to know whether a value is a "string" or "number," adding an unnecessary API layer above Quiver's already-converted strings. | The CSV layer only sees `vector<string>`. Quiver handles all type decisions before the CSV library is invoked. |
| Built-in enum/FK resolution | Auto-resolving foreign key integer values to string labels from database metadata. Seems convenient but requires the CSV library to understand database schemas. | Out of scope for a CSV library. Quiver already handles enum resolution before the CSV layer. A library with this feature would be solving the wrong problem. | Quiver resolves enums, formats datetimes, and formats floats before passing strings to the CSV writer. The CSV writer sees only final string values. |
| JSON or XML output support | Some "CSV libraries" are actually general serialization libraries. | Adds compile-time and link-time overhead for features Quiver will never use. | Pick a library focused on CSV only. |

---

## Feature Dependencies

```
CSV Library (Write)
    |
    +--required for--> Replace csv_escape() + write_csv_row() in src/database_csv.cpp
    |                       |
    |                       +--must preserve--> RFC 4180 quoting (comma/quote/newline)
    |                       +--must preserve--> LF line endings (not CRLF)
    |                       +--must preserve--> Minimal quoting (quote only when needed)
    |                       +--must preserve--> NULL -> empty unquoted field
    |                       +--must support-->  Write to pre-opened std::ofstream
    |
    +--enables (future)--> CSV import (read path)
                                |
                                +--requires--> RFC 4180 compliant read
                                +--requires--> Header row detection
                                +--requires--> Per-row streaming iteration
                                +--requires--> Column access by name
                                +--requires--> Raw string field access

Existing Quiver write pipeline (must stay unchanged):
    SQLite query result
        -> value_to_csv_string() [enum map, datetime format, float format]
            -> CSV library write_row(vector<string>)
                -> std::ofstream (binary mode, opened by Quiver, not by library)
```

### Dependency Notes

- **The CSV library is a leaf node:** It receives pre-processed strings and writes them to a file. It has no upstream dependency on Quiver's schema or type system.
- **The existing write pipeline does not change:** enum map lookup, datetime formatting, `%g` float formatting, and NULL handling all remain in `value_to_csv_string()`. The library only replaces `csv_escape()` + `write_csv_row()`.
- **Write path is ~15 lines; read path is the real investment:** Choosing a library purely for write replacement is defensible but wasteful if the library cannot handle import later. Select a library with a credible read path even if it is not used in v0.5.
- **LF vs CRLF is a hard constraint:** Existing tests assert `\r\n` absence. Any library that injects CRLF unconditionally is disqualifying.

---

## MVP Definition

### v0.5 Scope (Replace Writer, Enable Future Import)

What must be true for the library integration to be complete.

- [x] Existing CSV export behavior is preserved exactly (all 22 C++ CSV tests pass)
- [x] `csv_escape()` and `write_csv_row()` are deleted from `src/database_csv.cpp`
- [x] Library handles RFC 4180 quoting automatically (no manual quoting logic in Quiver)
- [x] Library writes LF line endings (or Quiver controls this via binary-mode ofstream)
- [x] Library is integrated via CMake FetchContent (consistent with SQLite, spdlog, sol2)
- [x] Library is header-only or has a minimal build footprint (no heavy runtime dependency)
- [x] Library supports future read path (not write-only)

### Not in v0.5 Scope

- [ ] CSV import implementation (explicit out of scope in PROJECT.md)
- [ ] Custom delimiter support (comma-only is sufficient)
- [ ] Streaming/chunked export
- [ ] Type inference on read (needed for import, not for refactor)

### Future (Import Milestone)

- [ ] `import_csv(collection, path, options)` using the library's read path
- [ ] Header-to-attribute-name mapping with schema validation
- [ ] Type conversion with error reporting for malformed fields
- [ ] Enum reverse-mapping (string label -> integer value)

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| RFC 4180 write (auto-quoting) | HIGH | LOW (library provides) | P1 |
| LF line endings (not CRLF) | HIGH | LOW (binary mode ofstream) | P1 |
| Write to pre-opened ofstream | HIGH | LOW (verify library supports this) | P1 |
| All 22 existing CSV tests pass | HIGH | LOW (regression, not new work) | P1 |
| Header-only or minimal build | MEDIUM | LOW (selection criterion) | P1 |
| FetchContent integration | MEDIUM | LOW (library provides CMakeLists) | P1 |
| Read path (for future import) | MEDIUM | LOW (selection criterion now, cost paid in import milestone) | P1 |
| Per-row streaming read | MEDIUM | LOW (selection criterion, not implemented in v0.5) | P2 |
| Column-by-name access on read | MEDIUM | LOW (selection criterion, not implemented in v0.5) | P2 |
| Type inference on read | LOW | MEDIUM (not needed until import milestone) | P3 |
| Custom delimiter | LOW | LOW (not exposed in v0.5) | P3 |

**Priority key:**
- P1: Required for v0.5
- P2: Required by import milestone, must be available in selected library
- P3: Nice to have, defer

---

## Library Comparison (Write + Read Capable)

| Criterion | csv2 (p-ranav) | vincentlaucsb/csv-parser | rapidcsv (d99kris) |
|-----------|---------------|--------------------------|-------------------|
| Write support | YES -- `Writer<delimiter<','>>` | YES -- `make_csv_writer(stream)` | YES -- via `Document` save |
| RFC 4180 auto-quoting on write | NO -- does not quote fields automatically | YES -- quotes only when needed, opt-in universal quoting | Partial -- basic escaping |
| Read support | YES -- memory-mapped reader | YES -- streaming iterator reader | YES -- full document load |
| RFC 4180 read compliance | Partial -- documented | YES -- explicit RFC 4180 claim | Partial |
| Header-only | YES -- single_include/csv2/csv2.hpp | YES -- single_include/csv.hpp | YES -- src/rapidcsv.h |
| FetchContent ready | YES -- has CMakeLists | YES -- has CMakeLists + wiki example | YES -- has CMakeLists |
| CMake target name | `csv2` | `csv` | `rapidcsv` |
| License | MIT | MIT | BSD-2 |
| Write to pre-opened stream | YES -- Writer(ofstream) | YES -- make_csv_writer(stream) | Indirect via stringstream |
| Streaming read (no full load) | YES -- memory map | YES -- 10MB chunks, iterator | NO -- loads entire file |
| Column access by name on read | NO -- positional only | YES -- row["name"] O(1) | YES -- GetColumn<T>("name") |
| C++ standard | C++17 | C++11 (C++17 recommended) | C++11 |
| Automatic delimiter detection | NO | YES | NO |
| Type inference on read | NO | YES (lazy, opt-in) | YES (templated conversion) |
| Last release (approx) | 2021 | June 2024 (v2.3.0) | Actively maintained |
| Stars (approx) | 700 | 2100 | 1100 |

**Verdict:** vincentlaucsb/csv-parser is the correct choice.

- It is the only library that provides RFC 4180 auto-quoting on write (the primary requirement).
- It writes to a caller-provided `std::ostream&`, allowing Quiver to control binary mode and LF endings.
- Its streaming reader with column-by-name access is exactly what CSV import will need.
- It has official FetchContent documentation, MIT license, and active maintenance through 2024.
- csv2 fails on the most critical requirement: its writer does not auto-quote fields.
- rapidcsv loads the entire file into memory -- incompatible with large CSV import.

---

## What Changes vs What Stays the Same

### Stays the Same (No Code Changes)

- `CSVExportOptions` struct (C++ and C API) -- unchanged
- `value_to_csv_string()` -- enum map lookup, datetime formatting, `%g` float formatting, NULL handling all remain in Quiver
- `Database::export_csv()` -- function signature and behavior unchanged
- All C API functions and their signatures
- All binding code (Julia, Dart, Lua)
- All test assertions (the output format does not change)
- Parent directory creation (`fs::create_directories`)
- Binary mode file open (`std::ios::binary`) -- Quiver keeps control of this

### Changes (Code Deleted and Replaced)

- `static std::string csv_escape(const std::string& field)` -- deleted, replaced by library
- `static void write_csv_row(std::ofstream& file, const std::vector<std::string>& fields)` -- deleted, replaced by library writer call
- `#include <fstream>` may become unnecessary if the library handles the stream write internally (but Quiver still needs it for `std::ofstream`)
- `CMakeLists.txt` -- add `FetchContent_Declare` for csv library, add to link targets

### Regression Risk

LOW. The library replaces exactly two internal static functions. The public API, options struct, C API, and all bindings are untouched. Test coverage is comprehensive (22 C++ tests, 31 C API tests covering all edge cases). If any test fails after the swap, the cause is localized to the two deleted functions.

---

## Sources

- [vincentlaucsb/csv-parser GitHub](https://github.com/vincentlaucsb/csv-parser) -- HIGH confidence (official repo, verified write behavior via test file)
- [csv-parser write test suite](https://github.com/vincentlaucsb/csv-parser/blob/master/tests/test_write_csv.cpp) -- HIGH confidence (RFC 4180 quoting tests verified directly)
- [csv-parser CMake FetchContent wiki](https://github.com/vincentlaucsb/csv-parser/wiki/Example:-Using-csv%E2%80%90parser-with-CMake-and-FetchContent) -- HIGH confidence (official wiki)
- [p-ranav/csv2 GitHub](https://github.com/p-ranav/csv2) -- HIGH confidence (official repo, writer implementation reviewed directly)
- [csv2 single include](https://github.com/p-ranav/csv2/blob/master/single_include/csv2/csv2.hpp) -- HIGH confidence (source confirms no auto-quoting in write_row)
- [d99kris/rapidcsv GitHub](https://github.com/d99kris/rapidcsv) -- MEDIUM confidence (feature claims from README, full document load confirmed)
- [RFC 4180](https://www.rfc-editor.org/rfc/rfc4180) -- HIGH confidence (canonical standard)

---
*Feature research for: CSV library replacement in Quiver SQLite wrapper (v0.5 milestone)*
*Researched: 2026-02-22*
