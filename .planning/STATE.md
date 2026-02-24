# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-23)

**Core value:** Consistent, type-safe database operations across multiple languages through a single C++ implementation
**Current focus:** Phase 6 - CSV and Convenience Helpers

## Current Position

Phase: 6 of 7 (CSV and Convenience Helpers)
Plan: 1 of 2 in current phase (06-01 complete)
Status: In Progress
Last activity: 2026-02-24 — Completed 06-01-PLAN.md (CSV export Python binding)

Progress: [████████░░] 80%

## Performance Metrics

**Velocity:**
- Total plans completed: 10
- Average duration: 4min
- Total execution time: 40min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-foundation | 2 | 6min | 3min |
| 02-reads-and-metadata | 2 | 10min | 5min |
| 03-writes-and-transactions | 2 | 11min | 6min |
| 04-queries-and-relations | 1 | 2min | 2min |
| 05-time-series | 2 | 5min | 3min |
| 06-csv-and-convenience-helpers | 1 | 6min | 6min |

**Recent Trend:**
- Last 5 plans: 03-02 (2min), 04-01 (2min), 05-01 (3min), 05-02 (2min), 06-01 (6min)
- Trend: Stable

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: CFFI ABI mode chosen for Python FFI (no compiler at install time)
- [Roadmap]: Package name changes from "margaux" to "quiver" — first action in Phase 1 (INFRA-01)
- [Roadmap]: CSV-02 (import_csv) flagged as likely unimplemented in C++ — verify at Phase 6 planning
- [Roadmap]: CONV-04 (DateTime helpers) included in scope; research recommends returning raw ISO strings — confirm at Phase 6
- [01-01]: Removed readme field from pyproject.toml (no README.md exists)
- [01-01]: Generator produces _declarations.py but _c_api.py uses hand-written cdef for Phase 1
- [01-02]: Database properties as methods (not @property) per user decision
- [01-02]: Element __del__ destroys silently (no warning); Database __del__ emits ResourceWarning
- [01-02]: Removed old tests/unit/ directory in favor of flat tests/ layout
- [02-01]: Bulk scalar reads skip NULL values (matches C++ behavior); by-id reads return None for NULLs
- [02-01]: read_scalar_relation maps both NULL and empty strings to None (matches Julia)
- [02-01]: Added update_scalar_relation early (Rule 3) to support relation test setup
- [02-02]: Bulk vector/set reads skip elements with no data (matches C++ NULL skipping behavior)
- [02-02]: list_* methods return full metadata objects (not just names) -- enables Phase 6 convenience methods
- [02-02]: _parse_scalar_metadata/_parse_group_metadata are module-level helpers
- [03-01]: C API update_scalar_string now accepts NULL to set column to NULL (behavioral change, test updated)
- [03-02]: Transaction context manager uses except BaseException for rollback (catches KeyboardInterrupt/SystemExit)
- [03-02]: _Bool* CFFI type for in_transaction output parameter (matches C API bool* exactly)
- [04-01]: Used void** instead of const void* const* in CFFI cdef (ABI-compatible; CFFI ignores const qualifiers)
- [04-01]: _marshal_params is module-level function consistent with _parse_scalar_metadata pattern
- [05-01]: DATE_TIME columns return plain str (no auto-parsing) -- consistent with Phase 4 query_string pattern
- [05-01]: Parameter order (collection, group, id) follows C++/C API order, matching Julia/Dart
- [05-01]: type(v) is int used for strict INTEGER validation (rejects bool subclass)
- [05-01]: Dimension column type hardcoded to STRING (2) in _marshal_time_series_columns
- [05-02]: Symmetric dict API for read/update files -- same {column: path_or_None} shape in both directions
- [06-01]: import_csv raises NotImplementedError in Python (C++ implementation is no-op stub)
- [06-01]: Group CSV export format uses label + value columns (no id/vector_index, no sep prefix) per actual DLL behavior

### Pending Todos

None yet.

### Blockers/Concerns

- ~~[Phase 6]: CSV-02 (import_csv) resolved — C++ has no-op stub, Python raises NotImplementedError~~
- [Phase 6]: CONV-04 (DateTime helpers) — research recommends deferring. Confirm scope decision at Phase 6 planning.

## Session Continuity

Last session: 2026-02-24
Stopped at: Completed 06-01-PLAN.md (CSV export Python binding)
Resume file: Phase 6, Plan 2 next (convenience helpers)
