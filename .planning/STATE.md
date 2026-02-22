# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-22)

**Core value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.
**Current focus:** v0.4 CSV Export -- Phase 6 (C API)

## Current Position

Phase: 6 of 7 (C API)
Plan: 1 of 2 in current phase
Status: Executing
Last activity: 2026-02-22 -- Completed 06-01-PLAN.md (C API CSV header + implementation)

Progress: [::::::::::::::::::::] v0.3 complete | [############] v0.4 Phase 6 in progress (1/2 plans)

## Performance Metrics

**Velocity:**
- Total plans completed: 7 (4 v0.3 + 3 v0.4)
- Average duration: ~8 min
- Total execution time: ~57 min

**By Phase (v0.3):**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. C++ Transaction Core | 1 | ~10 min | ~10 min |
| 2. C API Transaction Surface | 1 | ~10 min | ~10 min |
| 3. Language Bindings | 1 | ~10 min | ~10 min |
| 4. Performance Benchmark | 1 | ~10 min | ~10 min |

**By Phase (v0.4):**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 5. C++ Core | 2 | ~14 min | ~7 min |
| 6. C API | 1 | ~3 min | ~3 min |

*Updated after each plan completion*
| Phase 05 P01 | 10min | 2 tasks | 7 files |
| Phase 05 P02 | 4min | 2 tasks | 4 files |
| Phase 06 P01 | 3min | 2 tasks | 5 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
All v0.3 decisions archived -- see milestones/v0.3-ROADMAP.md for details.

v0.4 design decisions (from research):
- Single public function: `export_csv(collection, group, path, options)` -- group="" for scalars
- `CSVExportOptions` naming (not CsvExportOptions), following existing `DatabaseOptions` pattern
- C API: flat `quiver_csv_export_options_t` struct with parallel arrays for enum_labels
- Enum fallback: unmapped values written as raw integer
- Date formatting: strftime, only on DateTime columns by metadata
- Lua bypasses C API for options (sol2 reads Lua table directly to C++ struct)
- [Phase 05]: Hand-rolled RFC 4180 CSV writer instead of third-party library
- [Phase 05]: std::get_time for cross-platform ISO 8601 parsing (no strptime on MSVC)
- [Phase 05]: snprintf %g for float formatting (no trailing zeros)
- [Phase 05]: Unique attribute names across group tables to avoid schema validator collision
- [Phase 06]: Grouped-by-attribute parallel arrays layout for enum_labels flat struct
- [Phase 06]: No free function for options struct -- caller-owned, borrowing semantics
- [Phase 06]: NULL date_time_format treated as empty string in convert_options

### Key Technical Context

- export_csv implemented in src/database_csv.cpp with full scalar/group export logic
- CSVExportOptions struct in include/quiver/csv.h (plain value type, Rule of Zero)
- C API export_csv now 5-param with quiver_csv_export_options_t* opts (replaces old 4-param stub)
- quiver_csv_export_options_t flat struct in include/quiver/c/csv.h with grouped-by-attribute parallel arrays
- convert_options helper in src/c/database_csv.cpp reconstructs C++ CSVExportOptions from flat struct
- Julia/Dart bindings still reference old signature (will need regeneration + updates in Phase 7)
- import_csv remains an empty stub in src/database_describe.cpp
- 19 CSV export tests in test_database_csv.cpp covering all 8 requirements (CSV-01 through OPT-04)
- csv_export.sql test schema uses 'measurement' (vector) and 'tag' (set) to avoid duplicate attribute validation

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-22
Stopped at: Completed 06-01-PLAN.md
Resume file: .planning/phases/06-c-api/06-01-SUMMARY.md
