# Feature Research: CSV Export with Enum Resolution and Date Formatting

**Domain:** SQLite wrapper library -- CSV export for collection scalars and groups
**Researched:** 2026-02-22
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features that every database-to-CSV export function provides. Missing any of these makes the export feel broken or amateur.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Header row with column names | RFC 4180 section 2 rule 3: "There may be an optional header line appearing as the first line of the file with the same format as normal record lines." Every CSV tool (pandas `to_csv`, SQLite `.mode csv .headers on`, Apache Commons CSV) includes headers by default. Users paste CSVs into Excel, Google Sheets, R -- all expect a header row. | LOW | Use `list_scalar_attributes()` for scalar exports, `GroupMetadata::value_columns` for group exports. Column names come directly from the schema. The `label` column is always first (replaces `id`). |
| RFC 4180 value quoting and escaping | The universal CSV standard. Fields containing commas, double quotes, or newlines must be enclosed in double quotes. Double quotes within fields are escaped by doubling them (`""` not `\"`). Every credible CSV library implements this. Producing non-compliant CSV means broken imports everywhere. | LOW | Simple logic: if field contains `,`, `"`, or `\n`, wrap in double quotes and double any internal `"`. No external library needed -- this is ~15 lines of C++. |
| NULL handling as empty field | CSV has no native NULL representation. The universal convention (SQLite CLI, pandas `na_rep=''`, Excel) is to write an empty field. Writing the string "NULL" or "null" pollutes data and causes type-inference failures in downstream tools. | LOW | Check `std::holds_alternative<std::nullptr_t>(value)`. Write empty string. No quoting needed for empty fields per RFC 4180. |
| UTF-8 encoding | All modern CSV consumers expect UTF-8. SQLite stores TEXT as UTF-8. Quiver strings are `std::string` (byte sequences, de facto UTF-8). | LOW | No transformation needed. Write bytes as-is. The CSV file is UTF-8 because SQLite data is UTF-8. Do not add BOM (Excel handles UTF-8 without BOM since 2016; BOM breaks Unix tools). |
| Scalar export: label + all scalar attributes | The primary use case. Export a collection's scalar data as one row per element. Columns: `label`, then each scalar attribute from the schema (excluding `id`). This is what users mean when they say "export my data to CSV." | MEDIUM | Use `list_scalar_attributes()` to get column metadata. Use `read_element_ids()` to get all IDs. For each element, use `read_scalar_*_by_id()` for each attribute. Build rows in memory, write to file. Column ordering follows schema declaration order. |
| Group export: label + group columns | Export a vector, set, or time series group table. Replace the integer `id` column with the corresponding element `label` from the parent collection. Use actual column names from the schema (not generic "value"). | MEDIUM | Use `get_vector_metadata()` / `get_set_metadata()` / `get_time_series_metadata()` to get column info. Query the group table directly via SQL (JOIN with parent collection for label resolution). Column order: `label`, then dimension/index column if present, then value columns. |
| File path output parameter | Users specify where to write the CSV. Consistent with existing stub signature `export_csv(table, path)`. | LOW | Use `std::ofstream`. Throw Quiver-pattern error if file cannot be opened: "Failed to export_csv: cannot open file: {path}". |
| C API exposure | Quiver's architecture requires all public C++ methods to propagate through C API. The existing stubs already have `quiver_database_export_csv(db, table, path)` in the C API header. | LOW | Signature will change to accept an options struct. Follow existing C API patterns with `quiver_error_t` return. |
| Julia binding | Quiver's binding consistency requirement. Existing stub: `export_csv(db, table, path)`. | LOW | Add options parameter. Julia wrapper constructs the C struct from keyword arguments or a Julia-side options type. |
| Dart binding | Quiver's binding consistency requirement. Existing stub: `exportCSV(table, path)`. | LOW | Add options parameter. Dart wrapper uses a Dart class that marshals to C struct. |
| Lua binding | Quiver's binding consistency requirement. No existing stub in Lua, but all C++ methods must be bound. | LOW | `db:export_csv(table, path, options_table)`. Lua table for options is natural. |

### Differentiators (Competitive Advantage)

Features that go beyond raw table dump and make Quiver's CSV export notably useful for the domain of schema-driven data management. These leverage Quiver's existing metadata introspection.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Enum/lookup value resolution via caller-provided map | Integer foreign key columns in CSV are meaningless to humans. A column showing `3` is useless; showing `"Active"` is useful. The caller passes a map of `{attribute_name -> {int_value -> string_label}}` and integer values are replaced with their human-readable labels. This is exactly what Datasette's `?_labels=on` does, except Quiver puts the caller in control of the mapping rather than auto-resolving from foreign tables. | MEDIUM | Caller-provided map avoids the fragility of auto-resolution (missing FK targets, ambiguous label columns). Implementation: during row serialization, check if the current attribute name exists in the enum map; if so, look up the integer value and substitute the string label. Unmapped values pass through as integers. This is simple dictionary lookup, no SQL JOINs required. |
| Date/time format string | Quiver stores date/time values as ISO 8601 TEXT strings in SQLite (`"2024-01-15T14:30:00"`). Users exporting to CSV often need a different format: `"2024-01-15"` (date only), `"01/15/2024"` (US locale), `"15 Jan 2024 14:30"` (human readable). A single format string applied to all `date_` prefixed columns transforms the output. Pandas exposes `date_format` for exactly this reason. | MEDIUM | Use C++ `<chrono>` to parse ISO 8601 input, then `std::format` or strftime-style formatting to produce output. Quiver already identifies date columns via `is_date_time_column()` (checks `date_` prefix). Apply format only to columns where `is_date_time_column(name)` is true. If parsing fails, pass through raw string unchanged (defensive). |
| Options struct pattern | Clean API: `CsvExportOptions` struct with `enum_labels` map and `date_time_format` string. Defaults to empty map and empty format (passthrough). Extensible for future options without breaking ABI. Matches Quiver's existing `DatabaseOptions` pattern. | LOW | C++ struct with default values. C API flat struct with arrays for the enum map (parallel arrays pattern from parameterized queries). Bindings construct naturally: Julia Dict/NamedTuple, Dart class, Lua table. |
| Two-function API: `export_scalars_csv` + `export_group_csv` | Clearer than the current generic `export_csv(table, path)`. Scalars and groups have fundamentally different structures (one row per element vs. many rows per element). Separate functions make the API self-documenting and allow type-appropriate column handling. The generic `export_csv` name is ambiguous about what "table" means in Quiver's schema-driven model. | LOW | `export_scalars_csv(collection, path, options)` handles the main collection table. `export_group_csv(collection, group, path, options)` handles any vector/set/time series table. Both share the same options struct and CSV writing logic. |
| Exclude `id` column, use `label` instead | Internal integer IDs are meaningless outside the database. Every row in a Quiver collection has a unique `label`. Exporting `label` instead of `id` makes the CSV self-documenting and human-readable. For group tables, JOIN with the parent collection to resolve `id` to `label`. | LOW | Scalar export: skip `id` column from `list_scalar_attributes()`, always include `label` first. Group export: SQL JOIN on parent table to get label. This is a design decision, not optional behavior -- always do it. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem useful but introduce complexity disproportionate to their value, or violate Quiver's design principles.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Auto-resolve foreign keys from database | Datasette does `?_labels=on` which automatically JOINs FK targets to resolve labels. Seems convenient -- no caller-provided map needed. | Fragile: requires every FK target table to have a predictable "label" column. Fails when FK targets are missing rows (Datasette issue #2214). Quiver schemas have arbitrary FK structures (see `relations.sql` -- `parent_id`, `sibling_id` point to different tables). Auto-resolution requires implicit assumptions about which column is the "display name." The caller knows their data model; the library does not. | Caller-provided enum map: `{"status_id" -> {1: "Active", 2: "Inactive"}}`. Explicit, no magic, no failure modes from missing FK targets. |
| Custom delimiter (tab, semicolon, pipe) | European locales use `;` as CSV delimiter because `,` is the decimal separator. Some tools prefer TSV. | Scope creep. RFC 4180 defines comma-separated values. Tab-separated is a different format (TSV). Supporting arbitrary delimiters means the quoting/escaping logic must be parameterized. Quiver is a data library, not a serialization framework. | Always produce RFC 4180 CSV (comma-delimited). If users need TSV, they can post-process or use a dedicated conversion tool. Quiver's job is data access, not format negotiation. |
| Column selection/filtering | Users may want to export only specific columns from a collection. Pandas `to_csv(columns=[...])` supports this. | Adds API complexity (column name validation, error handling for invalid names, interaction with enum maps). The primary use case is "export everything for this collection." Users who need column subsets can use `query_string()` with custom SQL and write their own CSV, or post-process with any CSV tool. | Export all columns. Users who need subsets use external tools or Quiver's `query_string()` for custom SQL. |
| Streaming/chunked export for large datasets | Pandas supports `chunksize` for memory-efficient export. Important for multi-gigabyte datasets. | Quiver targets embedded use cases with moderate data sizes (thousands to tens of thousands of rows). SQLite itself loads result sets into memory. Chunked export adds iterator complexity, partial-write error handling, and makes the API harder to reason about. | Write entire result to file in one pass. If memory becomes an issue in future, add a streaming variant as a separate function rather than complicating the primary API. |
| CSV import (round-trip) | Natural complement to export. Existing stub `import_csv` exists. | PROJECT.md explicitly lists CSV import as out of scope for v0.4. Import is significantly harder than export: type inference, schema validation, error handling for malformed rows, handling of enum reverse-mapping, duplicate detection. Conflating export and import in one milestone risks shipping neither well. | Defer to future milestone. Ship export first, validate the options struct pattern, then design import with lessons learned. |
| BOM (Byte Order Mark) for Excel compatibility | Older Excel versions (pre-2016) needed a UTF-8 BOM to detect encoding. Some users still request it. | Modern Excel (2016+) handles UTF-8 without BOM. BOM breaks many Unix tools (`cat`, `head`, `diff`), Python CSV readers, and causes `\xEF\xBB\xBF` artifacts in first-field parsing. Adding BOM as a default would be a net negative; as an option, it adds complexity for a shrinking edge case. | No BOM. UTF-8 is the universal default. Users with legacy Excel can add BOM via external tooling if needed. |
| Row filtering/WHERE clause | Users may want to export only rows matching certain criteria. | This turns CSV export into a query engine. Quiver already has `query_string()` for arbitrary SQL. Adding filtering to export duplicates existing functionality and complicates the API (filter syntax, parameter binding, interaction with enum resolution). | Export all rows. Users who need filtered export use `query_string()` for the data and write CSV themselves, or filter the output file. |
| Return CSV as string instead of writing to file | Some callers may want the CSV content in memory rather than written to disk. | Two output modes doubles the API surface. The file-based approach is more general (works for any size, standard pattern). In-memory CSV is trivially achievable by writing to a temp file and reading it back. | Write to file only. Bindings can add convenience wrappers that write to temp file and return contents if demand arises. |

## Feature Dependencies

```
CsvExportOptions struct (C++)
    |
    +--used by--> export_scalars_csv(collection, path, options)
    |                 |
    |                 +--uses--> list_scalar_attributes(collection)    [existing]
    |                 +--uses--> read_element_ids(collection)          [existing]
    |                 +--uses--> read_scalar_*_by_id(collection, ...) [existing]
    |                 +--uses--> is_date_time_column(name)             [existing]
    |                 +--uses--> RFC 4180 CSV writer (new internal)
    |                 +--uses--> Enum map lookup (new internal)
    |                 +--uses--> Date format transform (new internal)
    |
    +--used by--> export_group_csv(collection, group, path, options)
    |                 |
    |                 +--uses--> get_vector_metadata / get_set_metadata / get_time_series_metadata [existing]
    |                 +--uses--> Direct SQL query with JOIN for label resolution (new internal)
    |                 +--uses--> RFC 4180 CSV writer (shared with scalars)
    |                 +--uses--> Enum map lookup (shared with scalars)
    |                 +--uses--> Date format transform (shared with scalars)
    |
    +--propagates to--> C API: quiver_csv_export_options_t struct
    |                       |
    |                       +--used by--> quiver_database_export_scalars_csv()
    |                       +--used by--> quiver_database_export_group_csv()
    |
    +--propagates to--> Julia binding: export_scalars_csv(), export_group_csv!()
    +--propagates to--> Dart binding: exportScalarsCsv(), exportGroupCsv()
    +--propagates to--> Lua binding: export_scalars_csv(), export_group_csv()

RFC 4180 CSV writer (internal helper)
    +--used by--> export_scalars_csv
    +--used by--> export_group_csv
    +--handles--> Header row generation
    +--handles--> Field quoting/escaping (comma, double-quote, newline)
    +--handles--> NULL -> empty field
    +--handles--> Line termination (CRLF per RFC 4180)

Enum map lookup (internal helper)
    +--requires--> CsvExportOptions::enum_labels map
    +--applied during--> Row serialization (before CSV quoting)

Date format transform (internal helper)
    +--requires--> CsvExportOptions::date_time_format string
    +--uses--> is_date_time_column() to identify target columns [existing]
    +--uses--> C++ <chrono> or strftime for format conversion
```

### Dependency Notes

- **RFC 4180 writer is shared infrastructure:** Both `export_scalars_csv` and `export_group_csv` use the same CSV writing logic. Implement as an internal helper (not public API). This could be a simple `write_csv_row(ofstream, vector<string>)` function.
- **Enum map and date format are applied during serialization:** They transform values before the CSV writer sees them. The pipeline is: read raw value -> apply enum map if applicable -> apply date format if applicable -> CSV-quote the result -> write to file.
- **Group export requires SQL JOIN:** Unlike scalar export (which uses existing read-by-id methods), group export needs a direct SQL query joining the group table with the parent collection for label resolution. This uses the existing `execute()` internal method.
- **C API options struct is the FFI bottleneck:** The `enum_labels` map (a nested `map<string, map<int, string>>`) must be flattened for C API (parallel arrays). This is the most complex marshaling task, but Quiver has precedent in the parameterized query and time series columnar patterns.

## MVP Definition

### Launch With (v0.4 Core)

Minimum viable CSV export -- what must ship for the milestone to be complete.

- [ ] `CsvExportOptions` struct in C++ with `enum_labels` and `date_time_format` fields
- [ ] `export_scalars_csv(collection, path, options)` -- export scalar attributes with label column
- [ ] `export_group_csv(collection, group, path, options)` -- export vector/set/time series group with label resolution
- [ ] RFC 4180 compliant CSV writing (header row, field quoting/escaping, CRLF line endings)
- [ ] Enum value resolution via caller-provided map
- [ ] Date/time column formatting via format string
- [ ] NULL values written as empty fields
- [ ] C API: `quiver_csv_export_options_t` struct + two export functions
- [ ] Julia binding: `export_scalars_csv()` + `export_group_csv!()`
- [ ] Dart binding: `exportScalarsCsv()` + `exportGroupCsv()`
- [ ] Lua binding: `export_scalars_csv()` + `export_group_csv()`
- [ ] C++ tests covering: basic scalar export, group export, enum resolution, date formatting, quoting edge cases, NULL handling
- [ ] C API tests mirroring C++ coverage
- [ ] Binding tests in Julia, Dart, Lua

### Add After Validation (v0.4 Polish)

Features to add once core export is working and tested.

- [ ] Edge case hardening: very long strings, Unicode in fields, empty collections
- [ ] Error messages following Quiver's three patterns for all failure modes
- [ ] Remove or deprecate old `export_csv` / `import_csv` stubs

### Future Consideration (v0.5+)

Features to defer until CSV export has real-world usage feedback.

- [ ] CSV import (reverse of export, with type inference and validation)
- [ ] Streaming export for large datasets (if memory becomes a concern)
- [ ] Column selection/filtering (if users request it)

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| `export_scalars_csv` (C++) | HIGH | MEDIUM | P1 |
| `export_group_csv` (C++) | HIGH | MEDIUM | P1 |
| RFC 4180 CSV writer (internal) | HIGH | LOW | P1 |
| `CsvExportOptions` struct (C++) | HIGH | LOW | P1 |
| Enum value resolution | HIGH | LOW | P1 |
| Date/time formatting | MEDIUM | MEDIUM | P1 |
| NULL as empty field | HIGH | LOW | P1 |
| Label instead of ID | HIGH | LOW | P1 |
| C API options struct + functions | HIGH | MEDIUM | P1 |
| Julia binding | HIGH | LOW | P1 |
| Dart binding | HIGH | LOW | P1 |
| Lua binding | HIGH | LOW | P1 |
| C++/C API/binding tests | HIGH | MEDIUM | P1 |
| Remove old stubs | LOW | LOW | P2 |
| CSV import | MEDIUM | HIGH | P3 |

**Priority key:**
- P1: Must have for v0.4 launch
- P2: Should have, add during v0.4 polish
- P3: Nice to have, defer to future milestone

## Competitor Feature Analysis

| Feature | SQLite CLI (`.mode csv`) | pandas (`to_csv`) | Datasette (CSV export) | DuckDB (`COPY TO`) | Quiver (Our Approach) |
|---------|-------------------------|--------------------|-----------------------|--------------------|-----------------------|
| Header row | `.headers on` (opt-in) | `header=True` (default) | Always included | `HEADER true` option | Always included (no option to disable) |
| Value quoting | RFC 4180 compliant | `quoting` param (4 modes) | RFC 4180 compliant | RFC 4180 compliant | RFC 4180 compliant (always quote-when-needed) |
| NULL handling | Empty field (or `.nullvalue X`) | `na_rep=''` (configurable) | Empty field | Empty field (or configurable) | Empty field (not configurable) |
| Enum/FK resolution | Not supported (raw SQL JOIN) | Not built-in (manual `map`) | `?_labels=on` auto-resolves FKs | Not built-in | Caller-provided enum map (explicit, no auto-resolution) |
| Date formatting | Not supported (raw TEXT) | `date_format` param | Not supported | `TIMESTAMPFORMAT` option | `date_time_format` in options struct |
| Column selection | `SELECT col1, col2 ...` | `columns=[...]` param | Column checkboxes in UI | `SELECT` clause | Not supported (export all columns) |
| ID-to-label | Not applicable | Not applicable | Not applicable | Not applicable | Always replaces `id` with `label` (schema-aware) |
| Delimiter choice | `.separator ","` (configurable) | `sep` param | Not configurable | `DELIMITER` option | Comma only (RFC 4180) |
| Encoding | UTF-8 | `encoding` param | UTF-8 | UTF-8 | UTF-8 only |
| BOM support | Not supported | Not supported | Not supported | Not supported | Not supported (correct) |

### Key Ecosystem Insight

The ecosystem splits into two categories:

1. **Generic CSV tools** (pandas, DuckDB COPY, SQLite CLI): Maximum flexibility -- configurable delimiters, quoting modes, NULL representations, column selection. Appropriate for general-purpose data tooling where the caller controls everything.

2. **Schema-aware exporters** (Datasette, ORMs with serialization): Leverage schema knowledge to produce human-readable output -- FK resolution, meaningful column names, type-appropriate formatting. Less configurable, but the output is immediately useful.

Quiver falls squarely in category 2. The library knows the schema (via `describe()`, metadata introspection, `is_date_time_column()`). It should use that knowledge to produce CSV that is human-readable by default: labels instead of IDs, resolved enums, formatted dates. The options struct provides the two transformations that require caller knowledge (which values are enums, what date format to use) while the library handles everything else automatically.

## Sources

- [RFC 4180 -- Common Format and MIME Type for CSV](https://www.rfc-editor.org/rfc/rfc4180.html) -- canonical CSV specification, quoting and escaping rules
- [pandas DataFrame.to_csv()](https://pandas.pydata.org/docs/reference/api/pandas.DataFrame.to_csv.html) -- reference API for CSV export options (date_format, quoting, na_rep, header, columns)
- [Datasette CSV Export](https://docs.datasette.io/en/stable/csv_export.html) -- `_labels=on` FK resolution feature
- [Datasette Issue #2214](https://github.com/simonw/datasette/issues/2214) -- FK label resolution failure when references are missing (justifies caller-provided map over auto-resolution)
- [Datasette Issue #406](https://github.com/simonw/datasette/issues/406) -- nullable foreign keys in labels mode
- [SQLite CLI CSV Export](https://sqlite.org/cli.html) -- `.mode csv`, `.headers on`, `.nullvalue` behavior
- [SQLite Tutorial: Export CSV](https://www.sqlitetutorial.net/sqlite-export-csv/) -- practical SQLite CSV export patterns
- [Apache Arrow CSV Writing](https://arrow.apache.org/docs/cpp/csv.html) -- C++ CSV writer patterns
- [CSV Special Characters Guide](https://inventivehq.com/blog/handling-special-characters-in-csv-files) -- RFC 4180 escaping edge cases

---
*Feature research for: CSV Export with Enum Resolution and Date Formatting in SQLite Wrapper Library*
*Researched: 2026-02-22*
