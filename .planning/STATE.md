# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-22)

**Core value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.
**Current focus:** v0.4 CSV Export -- Phase 5 (C++ Core)

## Current Position

Phase: 5 of 7 (C++ Core)
Plan: 0 of TBD in current phase
Status: Ready to plan
Last activity: 2026-02-22 -- Roadmap created for v0.4 CSV Export

Progress: [::::::::::::::::::::] v0.3 complete | [..........] v0.4 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 4 (v0.3)
- Average duration: ~10 min
- Total execution time: ~40 min

**By Phase (v0.3):**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. C++ Transaction Core | 1 | ~10 min | ~10 min |
| 2. C API Transaction Surface | 1 | ~10 min | ~10 min |
| 3. Language Bindings | 1 | ~10 min | ~10 min |
| 4. Performance Benchmark | 1 | ~10 min | ~10 min |

*Updated after each plan completion*

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

### Key Technical Context

- Existing export_csv/import_csv are empty stubs in src/database_describe.cpp
- C API wrappers exist in src/c/database.cpp but call empty C++ functions
- Julia/Dart bindings already wrap the stubs (will need signature changes)
- Lua binds export_csv/import_csv in lua_runner.cpp (also wraps empty stubs)

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-22
Stopped at: Phase 5 context gathered
Resume file: .planning/phases/05-c-core/05-CONTEXT.md
