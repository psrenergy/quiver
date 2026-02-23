# Requirements: Quiver

**Defined:** 2026-02-22
**Core Value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.

## v0.5 Requirements

Requirements for CSV Refactor milestone. Each maps to roadmap phases.

### CSV Library

- [ ] **LIB-01**: Replace `csv_escape()` and `write_csv_row()` in `src/database_csv.cpp` with rapidcsv Document/Save API
- [ ] **LIB-02**: Integrate rapidcsv via CMake FetchContent as PRIVATE dependency (no new DLLs on Windows)
- [ ] **LIB-03**: All existing CSV export tests pass without modification (22 C++ + 19 C API)

### Header Consolidation

- [ ] **HDR-01**: Merge `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` declaration from `csv.h` into `options.h`
- [ ] **HDR-02**: Delete `include/quiver/c/csv.h` with no compatibility stub
- [ ] **HDR-03**: Regenerate Julia and Dart FFI bindings; generated struct must be identical

## v0.4 Requirements (Complete)

### CSV Export

- [x] **CSV-01**: `export_csv(collection, group, path, options)` -- group="" exports scalars, group="name" exports groups
- [x] **CSV-02**: CSV output is RFC 4180 compliant
- [x] **CSV-03**: Empty collection writes header-only CSV
- [x] **CSV-04**: NULL values written as empty fields

### Options

- [x] **OPT-01**: `CSVExportOptions` struct with `enum_labels` map and `date_time_format` string
- [x] **OPT-02**: Enum resolution with unmapped fallback to raw integer
- [x] **OPT-03**: DateTime formatting via strftime on metadata-identified columns
- [x] **OPT-04**: `default_csv_export_options()` / `quiver_csv_export_options_default()` factory

### C API

- [x] **CAPI-01**: `quiver_csv_export_options_t` flat struct with parallel arrays for enum_labels
- [x] **CAPI-02**: `quiver_database_export_csv(db, collection, group, path, opts)` returning `quiver_error_t`

### Bindings

- [x] **BIND-01**: Julia `export_csv(db, collection, group, path, options)` with default options
- [x] **BIND-02**: Dart `exportCSV(collection, group, path, options)` with default options
- [x] **BIND-03**: Lua `export_csv(collection, group, path, options)` with default options

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
| CSV import | Defer to future milestone; v0.5 is refactor only |
| Multi-language columns in CSV | Caller picks one language per call |
| Custom delimiters | RFC 4180 comma-only is correct for this use case |
| Streaming export | Not needed at Quiver's target data sizes |
| vincentlaucsb/csv-parser | rapidcsv chosen for v0.5 (header-only, simpler integration); revisit for import milestone |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| LIB-01 | Phase 8 | Pending |
| LIB-02 | Phase 8 | Pending |
| LIB-03 | Phase 8 | Pending |
| HDR-01 | Phase 9 | Pending |
| HDR-02 | Phase 9 | Pending |
| HDR-03 | Phase 9 | Pending |

**Coverage:**
- v0.5 requirements: 6 total
- Mapped to phases: 6
- Unmapped: 0

---
*Requirements defined: 2026-02-22*
*Last updated: 2026-02-22 after roadmap creation*
