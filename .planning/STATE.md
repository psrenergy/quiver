---
gsd_state_version: 1.0
milestone: v0.12.9
milestone_name: milestone
status: Awaiting next milestone
stopped_at: Milestone v0.12.9 completed and archived
last_updated: "2026-10-03T19:13:44.635Z"
last_activity: 2026-10-03
last_activity_desc: Milestone v0.12.9 completed and archived
progress:
  total_phases: 5
  completed_phases: 5
  total_plans: 17
  completed_plans: 17
current_phase: 5
current_phase_name: Path-Policy Test and Docs
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-10-03)

**Core value:** Every file in the Lua scripting layer is small and single-purpose enough for an agent to change safely, and every existing script behaves exactly as before, apart from the deliberate, test-pinned fixes.
**Current focus:** Planning next milestone (run /gsd-new-milestone)

## Current Position

Phase: Milestone v0.12.9 complete
Plan: —
Status: Awaiting next milestone
Last activity: 2026-10-03 — Milestone v0.12.9 completed and archived

## Performance Metrics

**Velocity:**

- Total plans completed: 17
- Average duration: -
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 2 | - | - |
| 2 | 4 | - | - |
| 3 | 3 | - | - |
| 4 | 4 | - | - |
| 5 | 4 | - | - |

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
| Phase 04 P01 | 38min | 3 tasks | 15 files |
| Phase 04 P02 | 18min | 3 tasks | 8 files |
| Phase 04 P03 | 27min | 2 tasks | 10 files |
| Phase 04 P04 | 41min | 2 tasks | 9 files |
| Phase 05 P01 | 40 min | 2 tasks | 8 files |
| Phase 05 P02 | 25 min | 3 tasks | 13 files |
| Phase 05 P03 | 15 min | 3 tasks | 6 files |
| Phase 05 P04 | 35 min | 3 tasks | 11 files |

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
- [Phase 4]: 04-01: one type-error shape (lua_type_error) and one table check (require_table, get_type) in the decoder that first walks the argument; optional args via optional_from_lua (luaL_opt semantics)
- [Phase 4]: 04-01: file:write/bin_to_csv/file:read/aggregate* decode into ordered locals; open_file keeps mode -> containment -> metadata
- [Phase 4]: 04-01: Lua* = 460 tests / 12 suites (Debug and Release), C API 27, tidy 14; golden baseline build/fixes-check/baseline at 30a169f
- [Phase 4]: 04-02: db:transaction / db:dry_run check fn before begin; a callable table is refused (same rule as on_row)
- [Phase 4]: 04-02: A failed COMMIT at the end of db:transaction rolls back best-effort and rethrows; finish errors inside either block still surface
- [Phase 4]: 04-02: An empty Lua element array reaches the core: create_element skips it, update_element clears its group (BREAKING)
- [Phase 4]: 04-03: load is wrapped to always pass mode "t" (installed by lua.safe_script in the LuaRunner constructor); string.dump stays
- [Phase 4]: 04-03: Expression operand errors name Lua's metamethod event name or the quiver.* function name via binop<Op>(name) and to_expression(o, operation)
- [Phase 4]: 04-03: Lua* = 470 tests / 12 suites (Debug and Release), C API 27, tidy 14; golden baseline build/fixes-check/baseline at d81e2d8
- [Phase 4]: 04-04: SOL_ALL_SAFETIES_ON + SOL_PRINT_ERRORS=0 landed with the perf fallback SOL_SAFE_GETTER=0 / SOL_SAFE_STACK_CHECK=0 (full flags cost +16.4% on a 100k read_scalar_floats read; fallback -1.9% / file:read +0.5%)
- [Phase 4]: 04-04: the getter is now unchecked in Debug too (explicit =0 beats sol2's debug default); lua_cell_as and the key checks guard every .as<, Debug golden unchanged
- [Phase 4]: 04-04: Lua* = 472 / 12 suites (Windows Debug and Release), Linux 470 + 1 skip, C API 27 at 7e5eccd (superseded by the post-review baseline below: 477 at e6aa5c1)
- [Phase 05]: quiver_tests compiles its own copy of src/lua_runner/path_policy.cpp to reach the hidden resolve_sandboxed_path; nothing new exported; path_policy.cpp stays a one-function file
- [Phase 05]: 05-02: catalogue entries name their pinning test on a 'pinned by Suite.Test' line; the collect_garbage() note cites no test because close_open_handles() runs first
- [Phase 05]: 05-03: WRITE-06 is spelled 'the close-at-exit flush' and FMT-02 'the lone-empty-cell quoting' in test comments; DOC-01 closed (ids.sh IDS=0 FILES=0, RESIDUAL=44 = 19 diagnostic sites + 1 trailing comment)
- [Phase 05]: 05-04: the reference's sandbox-limits bullet gets no CHANGELOG entry (documents existing behaviour); pre-shape type errors documented in src/AGENTS.md, no message changed

### Pending Todos

None yet.

### Blockers/Concerns

Resolved concerns from v0.12.9 were cleared at milestone close (history: `milestones/v0.12.9-phases/`, `milestones/v0.12.9-MILESTONE-AUDIT.md`). Open items carried forward:

- [v0.12.9] PR notes, to copy into the PR body: (1) SAFE-06 perf, Release, median of 5 interleaved runs. `read_scalar_floats` 100k: 633 ms before, 737 ms (+16.4%) with all safeties, 626 vs 638 ms (−1.9%) with the fallback `SOL_SAFE_GETTER=0` + `SOL_SAFE_STACK_CHECK=0`. `file:read` 1M cells: 496 / 477 (−3.8%) / 424 vs 422 ms (+0.5%). (2) `SOL_SAFE_GETTER=0` also applies in Debug, so Debug CI no longer checks unguarded `.as<T>()`. Every current site is guarded, but new code must guard its own. (3) The Debug-only sol2 text changes from Phase 3 (12 probes) are in `milestones/v0.12.9-phases/03-dedupe/03-03-SUMMARY.md`. (4) Apple Clang and the off-Windows binding suites are proven only by PR CI.
- [v0.12.9] Final baseline: `quiver_tests` 1454 (`Lua*` 477 in 12 suites, `SandboxedPathTest` 11), `quiver_c_tests` 543 (`LuaRunnerCApiTest` 27). Linux GCC 13 and Clang 18/libc++: `Lua*` 475 (474 + 1 skip), sandbox 10.
- [v0.12.9] Tech debt (audit): positional string/number argument type errors still use sol2's raw text, not Pattern 1; `{}` skips on create but clears on update; 05-REVIEW IN-01 / IN-03 / IN-04; two pre-existing compiler warnings in the Linux logs.
- Backfilling the missing CHANGELOG `[0.12.9]` section is the maintainer's call. The memory note pointing at `[0.12.9]` is stale.
- The milestone was not git-tagged: the GSD id `v0.12.9` collides with the existing release tag `v0.12.9`. Give the next milestone an id that cannot collide.

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-10-03
Stopped at: Milestone v0.12.9 completed and archived; next: /gsd-cleanup, then /gsd-new-milestone
Resume file: None

## Operator Next Steps

- Start the next milestone with /gsd-new-milestone
