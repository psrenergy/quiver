---
phase: 02-the-dirty-files-parse-correctly
plan: 04
subsystem: testing
tags: [lua, csv-parser, sol2, release-build, SOL_SAFE_GETTER, TEST-05, DOC-04]

requires:
  - phase: 02-the-dirty-files-parse-correctly (plan 01)
    provides: "csv_read::Options.header_row and read_csv_options_from_lua's header_row decoder (the sol::object type check verified here)"
  - phase: 02-the-dirty-files-parse-correctly (plan 03)
    provides: "tests/fixtures/ (the two committed real-file CSVs, now documented in tests/CLAUDE.md)"
provides:
  - "A recorded Release-build run of the full LuaRunner* suite (291/291), proving the header_row sol::object decoder is safe with SOL_SAFE_GETTER off"
  - "tests/CLAUDE.md documentation of tests/fixtures/ and the release-preset test-binary trap"
affects:
  - "tests/CLAUDE.md"

actuals:
  tokens: 500
  tasks: 2
  commits: 1

tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - tests/CLAUDE.md

key-decisions:
  - "src/CLAUDE.md and CHANGELOG.md needed no edits: plan 02-01 already wrote the make_format ordering/past-EOF paragraph and the header_row changelog entry this plan's Task 2 asked for. Verified by reading both files and their git history before touching anything, rather than duplicating content the phase already shipped."
  - "Cleaned up build-release/ after the test run rather than adding it to .gitignore -- it is a one-shot verification tree, not a directory this repo's other tooling (scripts/build-all.bat, CI) ever creates, so a permanent gitignore entry would document a workflow that doesn't otherwise exist."

requirements-completed: [TEST-05]

coverage:
  - id: D1
    description: "The full LuaRunner test suite passes in a Release build (SOL_SAFE_GETTER off), matching the Debug run exactly, proving Phase 2's new header_row sol::object decoder has no release-only marshalling bug"
    requirement: "TEST-05"
    verification:
      - kind: integration
        ref: "build-release/bin/quiver_tests.exe --gtest_filter='LuaRunner*' (291/291 pass, Release, CMAKE_BUILD_TYPE=Release)"
        status: pass
      - kind: integration
        ref: "build/bin/quiver_tests.exe --gtest_filter='LuaRunner*' (291/291 pass, Debug -- parity baseline)"
        status: pass
    human_judgment: false
  - id: D2
    description: "tests/CLAUDE.md documents the new tests/fixtures/ directory and the release-preset test-binary trap; src/CLAUDE.md and CHANGELOG.md already carried the header_row documentation from plan 02-01"
    requirement: "DOC-04"
    verification:
      - kind: other
        ref: "grep -q 'header_row' src/CLAUDE.md && grep -q 'fixtures' tests/CLAUDE.md && grep -q 'header_row' CHANGELOG.md (plan's own verify command)"
        status: pass
    human_judgment: false

duration: ~20min
completed: 2026-09-16
status: complete
---

# Phase 2 Plan 4: Release-build parity and closing paperwork Summary

Proves the phase's new `header_row` `sol::object` decoder marshals identically with sol2's safety
getters off (a from-scratch Release tree, tests enabled, 291/291 `LuaRunner*` tests passing —
matching the Debug run exactly) and closes the phase's documentation debt in `tests/CLAUDE.md`,
finding that `src/CLAUDE.md` and `CHANGELOG.md` were already complete from plan 02-01.

## Performance

- **Duration:** ~20 min
- **Tasks:** 2
- **Files modified:** 1 (`tests/CLAUDE.md`)

## Accomplishments

- **Task 1 — Release build, TEST-05:** Configured a from-scratch tree (`cmake -S . -B
  build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON
  -DQUIVER_BUILD_C_API=ON`, deliberately not the `release` preset, which sets
  `QUIVER_BUILD_TESTS=OFF` and would have verified nothing), built it (`cmake --build build-release
  --config Release`), and ran `build-release/bin/quiver_tests.exe --gtest_filter='LuaRunner*'`:
  **291/291 pass**. Ran the identical filter against the existing Debug tree
  (`build/bin/quiver_tests.exe`) for a same-session parity baseline: also **291/291 pass**, no
  divergence. This directly discharges the phase's stated concern — `SOL_SAFE_GETTER` is on in
  Debug and off in Release, and the `header_row` decoder's `sol::object` type check
  (`read_csv_options_from_lua`, plan 02-01) is exactly the shape that degrades silently under that
  difference. Removed `build-release/` afterward (not gitignored, and a one-shot verification
  tree, not a persistent build directory any other tooling in this repo creates).
- **Task 2 — paperwork (DOC-04):** Read `src/CLAUDE.md`'s `make_format` paragraph and
  `CHANGELOG.md`'s unreleased section before editing either, and found plan 02-01 (commits
  693c417, 570f314) had already written exactly the content this task's `src/CLAUDE.md` and
  `CHANGELOG.md` instructions asked for — the header-mode call-order constraint, its consequence,
  the synthesized past-EOF error, and the caller-facing `header_row` changelog entry. Extended only
  `tests/CLAUDE.md`: added a paragraph describing the new `tests/fixtures/` directory (the two real
  Maranhão CSVs from plan 02-03, committed byte-exact because their BOM/CRLF bytes are themselves
  what two parser requirements assert, needing no CMake registration like `tests/schemas/`), and
  added the release-preset test-binary trap next to the existing `SOL_SAFE_GETTER` note, recording
  this plan's 291/291 Debug/Release parity result as the concrete example.

## Task Commits

1. **Task 1: Release build and LuaRunner* run** — no repository files modified (a build and a test
   run only, per the plan's own `<files>` declaration); results recorded above and in this
   SUMMARY.
2. **Task 2: tests/CLAUDE.md documentation** — `5fba037` (docs)

**Plan metadata:** committed alongside this SUMMARY (see below)

## Files Created/Modified

- `tests/CLAUDE.md` — added the `tests/fixtures/` paragraph to the `test_lua_runner_read_csv.cpp`
  entry, and the release-preset trap + TEST-05 result next to the existing `SOL_SAFE_GETTER` note.

## Decisions Made

- Did not touch `src/CLAUDE.md` or `CHANGELOG.md`: verified via `git log`/`git show` that plan
  02-01 (commits 693c417, 570f314) already wrote the exact content Task 2 asked for. Writing it a
  second time would have duplicated prose the phase already shipped.
- Cleaned up `build-release/` rather than adding a `.gitignore` entry: it's a one-shot verification
  tree the plan itself instructs building fresh, not a directory any script or CI job in this repo
  creates as a matter of course.

## Deviations from Plan

### Noted, Not Duplicated

**1. Task 2's src/CLAUDE.md and CHANGELOG.md instructions were already satisfied (done in 02-01)**
- **Found during:** Task 2, before editing
- **Issue:** The plan's Task 2 asked for the `make_format` header-mode ordering/past-EOF paragraph
  in `src/CLAUDE.md` and a caller-facing `header_row` entry in `CHANGELOG.md`. Reading both files
  showed plan 02-01 had already written this content verbatim (confirmed via `git show
  693c417`/`570f314`), because 02-01's own deviation-handling (Rule 2 — doc correctness) caught
  that its `header_row` change made the pre-existing prose in both files false and fixed it in the
  same plan.
- **Resolution:** No changes made to either file; verified the existing text already satisfies the
  plan's `must_haves` artifacts and the verify command (`grep -q 'header_row' src/CLAUDE.md`,
  `grep -q 'header_row' CHANGELOG.md`).
- **Files modified:** none

No auto-fixed issues (Rules 1-3) — this plan touched no production code, so there was nothing to
break or hard-code around. The only "deviation" is the above already-done finding, consistent with
plan 02-02's identical pattern earlier in this phase.

## Issues Encountered

None. The Release configure/build/test cycle succeeded on the first attempt (MSVC environment was
already sourced in the shell); no test failed in Release that passed in Debug.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- TEST-05 and DOC-04 are both discharged. Phase 2's full requirement set (LUA-05, LUA-06, LUA-08,
  PARSE-02 through PARSE-07, TEST-01, TEST-02, TEST-04, TEST-05) is now complete across its four
  plans.
- DOC-02/DOC-03 (the agent-reference rewrite and worked dirty-file example) remain out of scope
  for this phase by design — deferred to Phase 3, per this plan's `<documentation_scope>`.
- `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner*'` — 291/291 pass (Debug).
- Release parity: `build-release/bin/quiver_tests.exe --gtest_filter='LuaRunner*'` — 291/291 pass
  (tree removed after verification, per plan instruction not to commit it).

## Known Stubs

None. No stub patterns, placeholder values, or unwired data paths were introduced.

## Threat Flags

None. This plan's only registered threat (T-02-08, a vacuous Release verification via the wrong
CMake preset) was mitigated exactly as planned: an explicit configure with `QUIVER_BUILD_TESTS=ON`
into a separate tree, never the `release` preset.

---
*Phase: 02-the-dirty-files-parse-correctly*
*Completed: 2026-09-16*
