# Phase 5: C++ Core - Context

**Gathered:** 2026-02-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Implement CSV export in the C++ core: `CSVExportOptions` struct, `export_csv()` method on Database, RFC 4180 writer, enum resolution, date formatting, and tests. The C API surface and language bindings are separate phases (6 and 7).

</domain>

<decisions>
## Implementation Decisions

### Column ordering
- Scalar exports: columns appear in schema definition order (CREATE TABLE column order), with `label` as the first column header (literal "label", not collection name), `id` column excluded
- Group exports: column order is Claude's discretion based on schema structure; `label` replaces `id`
- All column headers use raw schema column names exactly as defined in CREATE TABLE (no renaming, no cleanup)

### File handling
- Overwrite existing files silently — no error if file exists
- LF line endings (`\n`), not CRLF — deviation from strict RFC 4180, but more practical for modern tooling
- UTF-8 BOM: Claude's discretion based on target usage
- Create missing parent directories automatically (like `mkdir -p`)
- Return type is `void` — success means file written, failure throws
- File mode (text vs binary) and atomic write strategy: Claude's discretion

### Default options behavior
- Empty `date_time_format` (default): pass through raw string values as stored in SQLite, no formatting applied
- Empty `enum_labels` (default): output raw integer values, no resolution
- Default options factory follows the same pattern as `DatabaseOptions`: free function `default_csv_export_options()` returning a `CSVExportOptions` struct
- Single method signature with default parameter: `export_csv(collection, group, path, options = default_csv_export_options())`

### Options struct design
- `enum_labels` type: `std::unordered_map<std::string, std::unordered_map<int64_t, std::string>>`  (attribute name -> {integer value -> label string})
- `date_time_format`: `std::string` (strftime format string, empty = no formatting)
- Struct type (Pimpl vs value): Claude's discretion (no private dependencies to hide, so likely plain value type)
- Header location: `include/quiver/csv.h` — houses both export options/declarations now, and import in the future
- `export_csv()` is a method on the `Database` class, consistent with other operations

### Claude's Discretion
- Whether to use a third-party CSV library (user approved adding one if beneficial, noting CSV import is a future requirement)
- UTF-8 BOM inclusion
- File open mode (binary vs text)
- Atomic write strategy (temp file + rename vs direct write)
- Group export column ordering logic
- CSVExportOptions as plain value type vs Pimpl

</decisions>

<specifics>
## Specific Ideas

- User explicitly noted: "you can add to the project some CSV library — remember that in the future we will read a CSV to import it." Library choice should consider both read and write capabilities.
- Header file named `csv.h` (not `csv_export.h`) to accommodate future import functionality in the same header.

</specifics>

<deferred>
## Deferred Ideas

- CSV import (reading CSV into database) — tracked in REQUIREMENTS.md as future requirement
- Column selection/filtering for CSV export — tracked in REQUIREMENTS.md as future requirement

</deferred>

---

*Phase: 05-c-core*
*Context gathered: 2026-02-22*
