# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-22)

**Core value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.
**Current focus:** v0.5 CSV Refactor -- Defining requirements

## Current Position

Phase: Not started (defining requirements)
Plan: —
Status: Defining requirements
Last activity: 2026-02-22 — Milestone v0.5 started

## Performance Metrics

**Velocity:**
- Total plans completed: 10 (4 v0.3 + 6 v0.4)
- Average duration: ~7 min
- Total execution time: ~70 min

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
| 6. C API | 2 | ~8 min | ~4 min |
| 7. Bindings | 2 | ~8 min | ~4 min |

*Updated after each plan completion*
| Phase 05 P01 | 10min | 2 tasks | 7 files |
| Phase 05 P02 | 4min | 2 tasks | 4 files |
| Phase 06 P01 | 3min | 2 tasks | 5 files |
| Phase 06 P02 | 5min | 2 tasks | 5 files |
| Phase 07 P01 | 13min | 2 tasks | 5 files |
| Phase 07 P02 | 8min | 2 tasks | 3 files |

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
- [Phase 06]: Mirror all 19 C++ CSV tests in C API for output parity verification
- [Phase 07]: Lua bypasses C API, calls Database::export_csv() directly via sol2 lambda
- [Phase 07]: sol::optional<sol::table> for truly optional Lua options table argument
- [Phase 07]: Nested for_each traversal for enum_labels Lua table to C++ unordered_map
- [Phase 07]: Julia build_csv_options constructs new immutable struct with all 6 positional fields
- [Phase 07]: Dart allocates quiver_csv_export_options_t via Arena, sets fields directly (no default factory)
- [Phase 07]: Fixed Julia build_options to use keyword splatting (;kwargs...) for Julia 1.12 compatibility

### Key Technical Context

- export_csv implemented in src/database_csv.cpp with full scalar/group export logic
- CSVExportOptions struct in include/quiver/csv.h (plain value type, Rule of Zero)
- C API export_csv now 5-param with quiver_csv_export_options_t* opts (replaces old 4-param stub)
- quiver_csv_export_options_t flat struct in include/quiver/c/csv.h with grouped-by-attribute parallel arrays
- convert_options helper in src/c/database_csv.cpp reconstructs C++ CSVExportOptions from flat struct
- Julia export_csv with build_csv_options marshaling Dict kwargs to C API flat struct (19 tests)
- Dart exportCSV with Arena-based options marshaling Map to C API struct (5 tests)
- import_csv remains an empty stub in src/database_describe.cpp
- 19 C++ CSV export tests in test_database_csv.cpp covering all 8 requirements (CSV-01 through OPT-04)
- 19 C API CSV export tests in test_c_api_database_csv.cpp mirroring C++ tests
- csv_export.sql test schema uses 'measurement' (vector) and 'tag' (set) to avoid duplicate attribute validation
- Lua export_csv binding in src/lua_runner.cpp via sol2 lambda with optional table argument
- 5 Lua CSV export tests in test_lua_runner.cpp (ScalarDefaults, GroupExport, EnumLabels, DateTimeFormat, CombinedOptions)
- 282 C API tests pass (263 + 19 CSV), 442 C++ tests pass (437 + 5 Lua CSV)
- 437 Julia tests pass (418 + 19 CSV), 252 Dart tests pass (247 + 5 CSV)

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-02-22
Stopped at: Completed 07-01-PLAN.md (Julia/Dart CSV bindings complete)
Resume file: .planning/phases/07-bindings/07-01-SUMMARY.md
