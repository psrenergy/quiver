# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-22)

**Core value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.
**Current focus:** v0.4 CSV Export -- Phase 5 (C++ Core)

## Current Position

Phase: 5 of 7 (C++ Core)
Plan: 1 of 2 in current phase
Status: Executing
Last activity: 2026-02-22 -- Completed 05-01-PLAN.md (C++ CSV Export)

Progress: [::::::::::::::::::::] v0.3 complete | [#####.....] v0.4 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 5 (4 v0.3 + 1 v0.4)
- Average duration: ~10 min
- Total execution time: ~50 min

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
| 5. C++ Core | 1 | ~10 min | ~10 min |

*Updated after each plan completion*
| Phase 05 P01 | 10min | 2 tasks | 7 files |

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

### Key Technical Context

- export_csv implemented in src/database_csv.cpp with full scalar/group export logic
- CSVExportOptions struct in include/quiver/csv.h (plain value type, Rule of Zero)
- C API stub updated to 4-param signature (collection, group, path); full options struct deferred to Phase 6
- Julia/Dart bindings still reference old 2-param signature (will need updates in Phase 7)
- import_csv remains an empty stub in src/database_describe.cpp

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-22
Stopped at: Completed 05-01-PLAN.md
Resume file: .planning/phases/05-c-core/05-01-SUMMARY.md
