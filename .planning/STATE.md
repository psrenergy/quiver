---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 2
current_phase_name: The dirty files parse correctly
status: planning
stopped_at: Completed 01-03-PLAN.md
last_updated: "2026-09-16T03:16:00.803Z"
last_activity: 2026-09-16
last_activity_desc: Phase 01 complete, transitioned to Phase 2
progress:
  total_phases: 1
  completed_phases: 1
  total_plans: 3
  completed_plans: 3
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-09-14)

**Core value:** Every layer — C++, C, Julia, Dart, Python, JS and Lua — sees the same data under the same rules, because all the logic lives in the C++ core and the bindings stay thin.
**Current focus:** Phase 01 — a-lua-script-reads-a-csv-file

## Current Position

Phase: 2 — The dirty files parse correctly
Plan: Not started
Status: Ready to plan
Last activity: 2026-09-16 — Phase 01 complete, transitioned to Phase 2

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 3
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 3 | - | - |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 20min | 2 tasks | 8 files |
| Phase 01 P02 | 25min | 2 tasks | 3 files |
| Phase 01 P03 | 45min | 2 tasks | 5 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table. Affecting current work:

- Parser is `vincentlaucsb/csv-parser` 5.3.0 via FetchContent, speculative-parallel and SIMD off
  (its default window is `chunk_size × worker_count` and parallel parsing auto-enables above 50 MB —
  PARSE-09 exists because of that)

- Two Lua entry points over one parser, so whole-file and streaming cannot diverge
- Read only; CSV writing deferred to v2
- Writing parsed rows into the database stays the script's job
- Core CSV handling (`import_csv`/`export_csv`, still on rapidcsv) is not unified in this milestone
- [Phase ?]: csv-parser 5.3.0 wired in (threads/SIMD forced off); db:read_csv and db:read_csv_stream share one internal csv_read::Reader (no public header)
- [Phase ?]: D-22 evaluation order fix: db:read_csv/db:read_csv_stream now resolve the sandboxed path before decoding the options table, matching the locked catalogue order
- [Phase ?]: D-22 entry 10 (csv-parser wrapper) has no portable runtime trigger on Windows; proven via a source-level assertion instead, flagged for human review
- [Phase ?]: Phase 01 (a-lua-script-reads-a-csv-file) fully executed: all three plans complete, TEST-03/LUA-04/LUA-08 sandbox and error-catalogue coverage proven end to end

### Pending Todos

None yet.

### Blockers/Concerns

- `bindings/js/test/lua-api-sync.test.ts` is a hard build gate: it parses `src/lua_runner.cpp` and
  fails until every newly bound `db:` name is a literal token in `bindings/js/src/lua-api.ts`.
  Phase 1 must land DOC-01 alongside the binding, not after it.

- `lua_runner.cpp` already needs `/bigobj` on MSVC for sol2's template depth; a header-only parser
  landing in that translation unit is a concrete build-size risk.

- TEST-02's two real Maranhão CSVs live outside the repo and must be copied into `tests/` as
  fixtures during Phase 2.

- Pre-existing and out of scope, but noted: `import_csv`'s global `;`→`,` replace and
  quote-unaware trailing-comma stripper are live data-corruption paths; `CHANGELOG.md`'s unreleased
  heading says `0.10.4` while all five manifests say `0.10.6`.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-09-15T15:52:28.890Z
Stopped at: Completed 01-03-PLAN.md
Resume file: None
