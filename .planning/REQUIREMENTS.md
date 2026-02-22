# Requirements: Quiver

**Defined:** 2026-02-22
**Core Value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.

## v0.4 Requirements

Requirements for CSV Export milestone. Each maps to roadmap phases.

### CSV Export

- [ ] **CSV-01**: `export_csv(collection, group, path, options)` -- group="" exports scalars (label + all scalar attributes, no id), group="name" exports that vector/set/time series group (label replaces id, schema column names)
- [ ] **CSV-02**: CSV output is RFC 4180 compliant (header row, comma delimiter, double-quote escaping for commas/quotes/newlines)
- [ ] **CSV-03**: Empty collection writes header-only CSV (not an error)
- [ ] **CSV-04**: NULL values written as empty fields

### Options

- [ ] **OPT-01**: `CSVExportOptions` struct with `enum_labels` map (`attribute -> {value -> label}`) and `date_time_format` string
- [ ] **OPT-02**: Enum resolution replaces integer values with labels from caller-provided map; unmapped values fall back to raw integer
- [ ] **OPT-03**: Date/time formatting via `strftime` format string applied only to DateTime columns (identified by metadata, not value inspection)
- [ ] **OPT-04**: `default_csv_export_options()` / `quiver_csv_export_options_default()` following DatabaseOptions pattern

### C API

- [ ] **CAPI-01**: `quiver_csv_export_options_t` flat struct with parallel arrays for enum_labels, passable through FFI
- [ ] **CAPI-02**: `quiver_database_export_csv(db, collection, group, path, opts)` returning `quiver_error_t`; group="" or NULL for scalars

### Bindings

- [ ] **BIND-01**: Julia `export_csv(db, collection, group, path, options)` with default options
- [ ] **BIND-02**: Dart `exportCSV(collection, group, path, options)` with default options
- [ ] **BIND-03**: Lua `export_csv(collection, group, path, options)` with default options

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

- CSV import (reverse of export, with type inference and schema validation)
- Column selection/filtering for CSV export
- Custom delimiters (TSV, semicolon-separated)
- Streaming/chunked export for large datasets

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| CSV import | Significantly harder (type inference, validation); defer to future milestone |
| Multi-language columns in CSV | Caller picks one language per call; multi-column adds complexity with little benefit |
| Custom delimiters | RFC 4180 comma-only is correct for this use case |
| Streaming export | Not needed at Quiver's target data sizes |
| Auto-resolving enum values from FK relationships | Fragile; caller-provided map is explicit and safe |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CSV-01 | - | Pending |
| CSV-02 | - | Pending |
| CSV-03 | - | Pending |
| CSV-04 | - | Pending |
| OPT-01 | - | Pending |
| OPT-02 | - | Pending |
| OPT-03 | - | Pending |
| OPT-04 | - | Pending |
| CAPI-01 | - | Pending |
| CAPI-02 | - | Pending |
| BIND-01 | - | Pending |
| BIND-02 | - | Pending |
| BIND-03 | - | Pending |

**Coverage:**
- v0.4 requirements: 13 total
- Mapped to phases: 0
- Unmapped: 13

---
*Requirements defined: 2026-02-22*
*Last updated: 2026-02-22 after initial definition*
