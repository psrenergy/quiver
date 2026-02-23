# Phase 9: Header Consolidation - Context

**Gathered:** 2026-02-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Consolidate C API and C++ option/config types into unified `options.h` headers, delete standalone `csv.h` files in both layers, update all include paths, and regenerate FFI bindings. No new functionality -- purely structural cleanup.

</domain>

<decisions>
## Implementation Decisions

### C++ csv.h scope (expanded beyond original requirements)
- Create new `include/quiver/options.h` as the central C++ options header
- Move `CSVExportOptions` struct and `default_csv_export_options()` from `include/quiver/csv.h` into the new `options.h`
- Move `DatabaseOptions` alias (`using DatabaseOptions = quiver_database_options_t`) and `default_database_options()` from `include/quiver/database.h` into the new `options.h`
- `DatabaseOptions` stays as a `using` alias for the C API type -- no new C++ struct
- `LogLevel` stays in the C API `options.h` -- no C++ alias needed
- Delete `include/quiver/csv.h` after migration
- `database.h` includes the new `options.h` so callers get all types from one include

### Type ordering in C API options.h
- CSV types go after existing database options: `quiver_log_level_t` -> `quiver_database_options_t` -> `quiver_csv_export_options_t` -> `quiver_csv_export_options_default()`
- Preserve the full 10-line parallel-arrays documentation comment block from `csv.h` when moving

### Type ordering in C++ options.h
- `DatabaseOptions` alias + `default_database_options()` first
- `CSVExportOptions` struct + `default_csv_export_options()` second
- Matches C API ordering

### Include path cleanup
- Fix all 8 files that directly include either `csv.h` -- replace with `options.h` includes
- Minimal audit beyond breakages: check that no file includes both C and C++ options.h unnecessarily
- C API `database.h` includes `options.h` (replaces current `csv.h` include)
- C++ `database.h` includes new `options.h` (replaces current `csv.h` include)
- Individual `.cpp` files include `options.h` directly -- no reliance on transitive includes through `database.h`

### Claude's Discretion
- CLAUDE.md update scope: update file tree entries, plus any prose references to csv.h -- use judgment on what needs changing
- CLAUDE.md description for new `include/quiver/options.h` -- pick wording that fits existing style

</decisions>

<specifics>
## Specific Ideas

No specific requirements -- open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 09-header-consolidation*
*Context gathered: 2026-02-22*
