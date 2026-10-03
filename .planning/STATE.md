---
gsd_state_version: 1.0
milestone: v0.12.9
milestone_name: milestone
current_phase: 4
current_phase_name: Fixes and Release Type Safety
status: planning
stopped_at: Completed 03-03-PLAN.md
last_updated: "2026-10-03T07:45:50.727Z"
last_activity: 2026-10-03
last_activity_desc: Phase 2 complete, transitioned to Phase 3
progress:
  total_phases: 3
  completed_phases: 3
  total_plans: 9
  completed_plans: 9
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-10-03)

**Core value:** Every file in the Lua scripting layer is small and single-purpose enough for an agent to change safely, and every existing script behaves exactly as before, apart from the deliberate, test-pinned fixes.
**Current focus:** Phase 3 — Dedupe

## Current Position

Phase: 4 — Fixes and Release Type Safety
Plan: Not started
Status: Ready to plan
Last activity: 2026-10-03 — Phase 3 complete, transitioned to Phase 4

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 9
- Average duration: -
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 2 | - | - |
| 2 | 4 | - | - |
| 3 | 3 | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 134min | 3 tasks | 7 files |
| Phase 01 P02 | 35 min | 3 tasks | 4 files |
| Phase 02 P01 | 20min | 2 tasks | 4 files |
| Phase 02 P02 | 20min | 2 tasks | 5 files |
| Phase 02 P03 | 10min | 3 tasks | 9 files |
| Phase 02 P04 | 45min | 3 tasks | 15 files |
| Phase 03 P01 | 35min | 3 tasks | 7 files |
| Phase 03 P02 | 20min | 2 tasks | 6 files |
| Phase 03 P03 | 22min | 3 tasks | 5 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: strict order pins -> split -> dedupe -> fixes + Release type safety -> docs/path-policy test. One PR per phase into master, each green on its own. (The rename was dropped 2026-10-02.)
- Roadmap: `SOL_ALL_SAFETIES_ON` + `SOL_PRINT_ERRORS=0` is the last commit of Phase 4, after the explicit `require_table`/`lua_string_key`/`optional_from_lua` checks. Perf budget 5% on a bulk read and `file:read`; if over, add `SOL_SAFE_GETTER=0`/`SOL_SAFE_STACK_CHECK=0`, never disable `SOL_SAFE_FUNCTION_CALLS`/`SOL_SAFE_USERTYPE`.
- Roadmap: the no-op `SOL_SAFE_FUNCTION=1` is deleted in Phase 4 (SAFE-06); PROJECT.md Constraints and Key Decisions now record it.
- Roadmap: Phase 2 converts the 17 variadic `Database` pairs to `bind.set_function` (the split forces it). Phase 3 (DEDUP-02) adds the member-pointer forwarders.
- Roadmap: Phase 2's "no `lua_runner.cpp` citation" gate covers the core file only. The C API TU `src/c/lua_runner.cpp` and `test_c_api_lua_runner.cpp` keep their names until Phase 5.
- Roadmap: DOC-01 (planning-ID comments) completes in Phase 5. Phase 2 already strips them from the code it moves into `src/lua_runner/`, Phases 1-4 add none, and Phase 5's grep also covers this milestone's IDs.
- Roadmap: TEST-01 is in Phase 5: `SandboxedPathTest` unit tests against `resolve_sandboxed_path`, outside the `Lua*` count. (The rename-era `resolve_contained_path`/`ContainedPathTest`, REN-07 and the binding-file renames were dropped with the rename.)
- [Phase 01]: Pins must be mutation-tested, not just green: a move pin that keeps the source alive passed a run-state-outside-Impl mutant; two order pins could not see their "options must be a table" check vanish. Phase 2's split should rerun the lifecycle pins and keep `LuaRunner` pointer-sized.
- Roadmap: the CHANGELOG `[0.13.0] — unreleased` section and its compare link are opened in Phase 4 (C1/C5/C7/`load` BREAKING, the Release dot-call and raw sol2 self-check text, C2/C4/C6/C8 Fixed) and completed in Phase 5 (rename). No version bump.
- [Phase 01]: Move pins pair a kept-alive source with a freed source, plus static_assert(sizeof(LuaRunner)==sizeof(void*)) so run state held outside Impl fails in Release too
- [Phase 01]: Phase 1 gtest baseline: Lua* = 444 tests / 12 suites, LuaRunnerCApiTest = 27 (at 570c2c1)
- [Phase 02]: /bigobj (-Wa,-mbig-obj) is target-wide on quiver so no new sol2 TU can miss it
- [Phase 02]: NOLINT pairs now name performance-unnecessary-value-param (old name suppressed nothing)
- [Phase 02]: lua_runner.cpp was de-classed into quiver::lua_internal and laid out as the future files end to end (return_json, shared, path_policy, db_metadata, db_read, db_write, db_time_series, db_core, binary, csv, root), so plan 03's extractions are pure range cuts
- [Phase 02]: Each lua_runner/ TU includes only the headers it uses; lua_runner.cpp trimmed to lua_runner.h, csv_write.h, internal.h, binary_file.h, database.h, sol, memory/stdexcept/string
- [Phase 02]: csv.cpp is 446 lines, so the line-budget fallback was not applied and CsvWriter stays in csv.cpp
- [Phase 02]: C2 citation check keeps one hit by construction (src/AGENTS.md file-map root TU line under lua_runner/); every hit names a src/lua_runner/ file or the C API TU
- [Phase 02]: tidy baseline on src/lua_runner/ is the 15 pre-existing warnings; header warnings from include/quiver and src/utils/datetime.h are pre-existing, reproduced from untouched TUs
- [Phase 3]: 03-01: db:transaction/db:dry_run stay two lambdas over run_in_scope so Debug bad-argument text is unchanged
- [Phase 3]: 03-01: Debug-only sol2 text changes accepted for the 17 member-pointer forwarders and the three query_* methods; Release unchanged
- [Phase 3]: 03-01: query_*_lua stay three named functions so Phase 4 optional_from_lua lands at three sites
- [Phase 3]: 03-02: option_entries owns the options-must-be-a-table check; nil handling stays per caller (CSV decoders return defaults, quiver.metadata lets nil reach the check)
- [Phase 3]: 03-02: Debug-only sol2 dot-call text change accepted for w.write_row / w.close (CsvWriter members); Release unchanged
- [Phase 3]: 03-03: run-handle registries prune only expired() entries on insert (add_writer/add_binary_file); the close-at-exit function is close_open_handles
- [Phase 3]: 03-03: binop<Op> with transparent functors backs all twelve binary Expression operators; gt..neq stay six literal ns.set_function calls for the sync test

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 4 needs research: the perf measurement protocol (the threshold and fallback are set), roughly 20 `require_table` placements that must keep the existing check order, and the C7 fan-out wording.
- Phase 5 needs research: TEST-01 also needs a link strategy: `quiver` is shared with hidden visibility, so `resolve_sandboxed_path` must be header-inline, exported, or `path_policy.cpp` compiled into `quiver_tests`.
- Phase 1 gtest baseline (observed 2026-10-02 at `570c2c1`): `quiver_tests --gtest_filter=Lua*` = 444 tests across 12 suites (identical in Debug and Release, and in `build/`), and C API `LuaRunnerCApiTest` = 27. Phases 2 and 3 must reproduce both exactly; Phase 4 records the counts Phase 5's `Lua*` and C API runs must reproduce. (444, not the planned 441: plan 01 added two freed-source move pins and `LuaRunner_WriteCsv.NonTableOptionsThrows`. The old `Sandbox*` filter no longer applies: the rename was dropped and the filter stays `Lua*`.)
- [Phase 2] Linux baseline (GCC 13 and Clang 18/libc++, Docker, at `8fbb066`): `Lua*` = 442 run, 441 pass + 1 skip (the chmod-000 test as root); the 2 fewer than Windows are `DeviceNamePathIsReportedWithPrefix` in `LuaRunner_ReadCsv` and `LuaBinaryTest`, `#ifdef _WIN32` by design. Apple Clang and off-Windows binding suites are still only proven by PR CI.
- [Phase 2] sol2 is now parsed once per TU: CPU time across the Lua files rose from 61 s to 223 s (wall time fell from 70 s to 46 s). Phase 3 dedupe should not add new sol2 TUs without reason.
- [Phase 2] Review info items carried to later phases: IN-01 (`db_core.cpp` NOLINT region starts below the `query_*_lua` helpers), IN-02 (lambdas in `bind_csv`/`bind_binary` shadow the `lua` parameter), IN-03 (four decoders convert table keys without a type check; Phase 4 SAFE work). See `02-REVIEW.md`.
- Backfilling the missing CHANGELOG `[0.12.9]` section is the maintainer's call and outside this milestone. The memory note pointing at `[0.12.9]` is stale.

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-10-03T07:23:27.332Z
Stopped at: Completed 03-03-PLAN.md
Resume file: None
