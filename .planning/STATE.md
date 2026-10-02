---
gsd_state_version: 1.0
milestone: v0.12.9
milestone_name: milestone
current_phase: 1
current_phase_name: Behaviour Pins
status: planning
stopped_at: Phase 1 context gathered
last_updated: "2026-10-02T19:20:53.817Z"
last_activity: 2026-10-02
last_activity_desc: Roadmap revised after checker feedback (5 phases, 40/40 v1 requirements mapped)
progress:
  total_phases: 1
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-10-02)

**Core value:** Every file in the Lua scripting layer is small and single-purpose enough for an agent to change safely, and every existing script behaves exactly as before, apart from the deliberate, test-pinned fixes.
**Current focus:** Phase 1 - Behaviour Pins

## Current Position

Phase: 1 of 5 (Behaviour Pins)
Plan: 0 of TBD in current phase
Status: Ready to plan
Last activity: 2026-10-02 — Roadmap revised after checker feedback (5 phases, 40/40 v1 requirements mapped)

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**

- Total plans completed: 0
- Average duration: -
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: strict order pins -> split -> dedupe -> fixes + Release type safety -> rename. One PR per phase into master, each green on its own.
- Roadmap: `SOL_ALL_SAFETIES_ON` + `SOL_PRINT_ERRORS=0` is the last commit of Phase 4, after the explicit `require_table`/`lua_string_key`/`optional_from_lua` checks. Perf budget 5% on a bulk read and `file:read`; if over, add `SOL_SAFE_GETTER=0`/`SOL_SAFE_STACK_CHECK=0`, never disable `SOL_SAFE_FUNCTION_CALLS`/`SOL_SAFE_USERTYPE`.
- Roadmap: the no-op `SOL_SAFE_FUNCTION=1` is deleted in Phase 4 (SAFE-06); PROJECT.md Constraints and Key Decisions now record it.
- Roadmap: Phase 2 converts the 17 variadic `Database` pairs to `bind.set_function` (the split forces it). Phase 3 (DEDUP-02) adds the member-pointer forwarders.
- Roadmap: Phase 2's "no `lua_runner.cpp` citation" gate covers the core file only. The C API TU `src/c/lua_runner.cpp` and `test_c_api_lua_runner.cpp` keep their names until Phase 5.
- Roadmap: DOC-01 (planning-ID comments) completes in Phase 5. Phase 2 already strips them from the code it moves into `src/sandbox/`, Phases 1-4 add none, and Phase 5's grep also covers this milestone's IDs.
- Roadmap: TEST-01 rides with the rename (Phase 5), so the unit test is written once against `resolve_contained_path`. Its tests use a `ContainedPathTest` prefix, outside the `Sandbox*` count. REN-07 (`tests/scratch`) is also in Phase 5.
- Roadmap: Phase 5 renames the binding source/test files and headers with the class; Python's module path becomes `quiverdb.sandbox`.
- Roadmap: the CHANGELOG `[0.13.0] — unreleased` section and its compare link are opened in Phase 4 (C1/C5/C7/`load` BREAKING, the Release dot-call and raw sol2 self-check text, C2/C4/C6/C8 Fixed) and completed in Phase 5 (rename). No version bump.

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 4 needs research: the perf measurement protocol (the threshold and fallback are set), roughly 20 `require_table` placements that must keep the existing check order, and the C7 fan-out wording.
- Phase 5 needs research: FFI mechanics differ per binding. Dart is hand-edited (never run `scripts/generator.bat`), Python resolves symbols lazily, JS loads eagerly, and the Dart hook cache must be cleared. The blast radius is about 50 files; re-derive it with `git grep`. TEST-01 also needs a link strategy: `quiver` is shared with hidden visibility, so `resolve_contained_path` must be header-inline, exported, or `path_policy.cpp` compiled into `quiver_tests`.
- Gtest counts: 428 Lua / 27 C API as of `bdf9087`. Phase 1 records the baseline Phases 2 and 3 must reproduce; Phase 4 records the counts Phase 5's `Sandbox*` filter and C API run must reproduce.
- Backfilling the missing CHANGELOG `[0.12.9]` section is the maintainer's call and outside this milestone. The memory note pointing at `[0.12.9]` is stale.

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-10-02T19:20:53.786Z
Stopped at: Phase 1 context gathered
Resume file: .planning/phases/01-behaviour-pins/01-CONTEXT.md
