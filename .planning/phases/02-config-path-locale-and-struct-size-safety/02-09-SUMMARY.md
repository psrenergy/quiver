---
phase: 02-config-path-locale-and-struct-size-safety
plan: 09
subsystem: testing
tags: [ui-config, locale, c-api, lua, julia, dart, python, js, cross-layer-coverage]

# Dependency graph
requires:
  - phase: 02-config-path-locale-and-struct-size-safety (plans 02-01..02-07)
    provides: ui_config_dir/ui_locale threaded through open/from_schema/from_migrations and has_ui_config() in every layer
provides:
  - "Committed regression tests (not just probe-proven behaviour) for malformed-sidecar polarity and the D-01 :memory: distinction, in the C API, Lua, Julia, Dart, Python and JS"
  - "Committed open()/from_migrations() coverage for ui_config_dir/ui_locale in Julia, Dart, Python and JS (previously proven only through from_schema())"
  - "Committed whole-database describe() locale assertion in Julia, Dart, Python and JS (previously proven only through describe_collection())"
  - "Committed empty-string ui_locale edge case (D-03 NULL mapping) in Julia, Dart, Python and JS"
  - "Fixed a merge-dropped regression in bindings/julia/src/c_api.jl (ui_config_dir/ui_locale fields missing from quiver_database_options_t) that was silently blocking the entire Julia test suite"
affects: [02-10, 02-11, 02-12, 02-13]

# Actuals (#2632)
actuals:
  tokens: 7017
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Runtime-generated scratch migrations directory (mktempdir + up.sql/down.sql written from an existing schema.sql's own DDL) as the way to exercise from_migrations() against a fixture schema with no committed migrations fixture of its own -- used identically in Julia, Dart, Python and JS in this plan"

key-files:
  created: []
  modified:
    - tests/test_c_api_database_options.cpp
    - tests/test_lua_runner_ui_options.cpp
    - bindings/julia/src/c_api.jl
    - bindings/julia/test/test_database_ui_options.jl
    - bindings/dart/test/database_ui_options_test.dart
    - bindings/python/tests/test_database_ui_options.py
    - bindings/js/test/database-ui-options.test.ts

key-decisions:
  - "Restored two Ptr{Cchar} struct fields silently dropped from bindings/julia/src/c_api.jl by a prior merge commit (e8d35b9) -- Rule 1 auto-fix, since it blocked the entire Julia test suite (module init failed the struct-size gate: Julia expected 8 bytes, native reported 24) not just this plan's new tests. Committed separately from the test additions."
  - "For the C API and Lua :memory:-without-override case, reused the pre-existing HasUiConfigFalseWhenDirectoryAbsent test instead of duplicating it, since it already asserted exactly what the plan's third malformed/:memory: case required."
  - "from_migrations() coverage needed a migrations directory producing the EconomicDriver schema the foresight_like sidecar labels, and no such fixture exists (files_modified is the six test files only) -- each of the four bindings writes one at runtime from foresight_like/schema.sql's own DDL into a scratch directory; nothing is committed to tests/schemas/."
  - "Lua's new :memory:/malformed assertions check the exact JSON-encoded return string (\"true\"/\"false\" from `return db:has_ui_config()`), per the plan's instruction to prove the value that actually crosses the LuaRunner boundary, not truthiness inside the script."

patterns-established:
  - "Runtime-generated migrations fixture pattern (see tech-stack.patterns) for exercising from_migrations() against a shared schema fixture that has no migrations directory of its own."

requirements-completed: [OPT-01, OPT-02, OPT-03, OPT-04]

coverage:
  - id: D1
    description: "C API and Lua assert has_ui_config()==false for the shared tests/schemas/ui/malformed fixture and the D-01 :memory: distinction (explicit dir loads even for :memory:, convention path never fires for :memory:)"
    requirement: OPT-04
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_options.cpp#DatabaseCApiOptions.HasUiConfigFalseWhenSidecarMalformed"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_options.cpp#DatabaseCApiOptions.HasUiConfigTrueForMemoryDatabaseWithExplicitDir"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_ui_options.cpp#LuaRunnerUiOptions.LuaHasUiConfigFalseWhenSidecarMalformed"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_ui_options.cpp#LuaRunnerUiOptions.LuaHasUiConfigTrueForMemoryDatabaseWithExplicitDir"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_ui_options.cpp#LuaRunnerUiOptions.LuaHasUiConfigFalseForMemoryDatabaseWithoutDir"
        status: pass
    human_judgment: false
  - id: D2
    description: "Julia, Dart, Python and JS each assert malformed polarity and the :memory: distinction"
    requirement: OPT-04
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_ui_options.jl#malformed sidecar reports false without throwing / :memory: distinction"
        status: pass
      - kind: unit
        ref: "bindings/dart/test/database_ui_options_test.dart#malformed sidecar / :memory: distinction"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_malformed_sidecar_reports_false_without_raising / test_memory_database_distinction"
        status: pass
      - kind: unit
        ref: "bindings/js/test/database-ui-options.test.ts#malformedSidecarReportsFalseWithoutThrowing / memoryDatabaseDistinction"
        status: pass
    human_judgment: false
  - id: D3
    description: "Julia, Dart, Python and JS each exercise open() and from_migrations() with ui_config_dir + ui_locale, asserting the Spanish label reads back"
    requirement: OPT-03
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_ui_options.jl#open() threads ui_config_dir and ui_locale / from_migrations() threads ui_config_dir and ui_locale"
        status: pass
      - kind: unit
        ref: "bindings/dart/test/database_ui_options_test.dart#open() threads uiConfigDir and uiLocale / fromMigrations() threads uiConfigDir and uiLocale"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_open_threads_ui_config_dir_and_locale / test_from_migrations_threads_ui_config_dir_and_locale"
        status: pass
      - kind: unit
        ref: "bindings/js/test/database-ui-options.test.ts#openThreadsUiConfigDirAndLocale / fromMigrationsThreadsUiConfigDirAndLocale"
        status: pass
    human_judgment: false
  - id: D4
    description: "Julia, Dart, Python and JS each assert a locale-specific label through whole-database describe(), and an empty-string ui_locale matches the unset/en output"
    requirement: OPT-02
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_ui_options.jl#describe() carries the locale-specific label / empty-string ui_locale matches the unset/en output"
        status: pass
      - kind: unit
        ref: "bindings/dart/test/database_ui_options_test.dart#describe() carries the locale-specific label / empty-string uiLocale"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_describe_carries_locale_specific_label / test_empty_string_ui_locale_matches_unset_output"
        status: pass
      - kind: unit
        ref: "bindings/js/test/database-ui-options.test.ts#describeCarriesLocaleSpecificLabel / emptyStringUiLocaleMatchesUnsetOutput"
        status: pass
    human_judgment: false

duration: 55min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 09: Cross-Layer UI-Options Coverage Gap Closure Summary

**Turned six probe-proven-but-uncommitted behaviours (malformed-sidecar polarity, the `:memory:` distinction, `open`/`from_migrations` parameter threading, and whole-database `describe()` locale rendering) into committed test cases across the C API, Lua, Julia, Dart, Python and JS suites, closing Gap 5 and Gap 7 from 02-VERIFICATION.md.**

## Performance

- **Duration:** ~55 min
- **Tasks:** 3 (all `type="auto"`)
- **Files modified:** 7 (6 planned test files + 1 unplanned regression fix)

## Accomplishments

- **C API** (`tests/test_c_api_database_options.cpp`): added `HasUiConfigFalseWhenSidecarMalformed` and `HasUiConfigTrueForMemoryDatabaseWithExplicitDir`; the pre-existing `HasUiConfigFalseWhenDirectoryAbsent` already covered the `:memory:` + no-override case, so it was reused rather than duplicated.
- **Lua** (`tests/test_lua_runner_ui_options.cpp`): added three new `TEST`s driving `LuaRunner::run("return db:has_ui_config()")` directly against a `Database` built with the matching `DatabaseOptions` (malformed → `"false"`, `:memory:` + explicit dir → `"true"`, `:memory:` + no dir → `"false"`), asserting on the exact JSON-encoded return string per the plan's instruction.
- **Julia, Dart, Python, JS**: each of the four binding suites gained the same six new cases: malformed polarity, the `:memory:` distinction, `open()` threading `ui_config_dir`/`ui_locale`, `from_migrations()` threading the same, a whole-database `describe()` locale assertion, and the empty-string `ui_locale` edge (D-03 NULL mapping).
- **Unplanned regression fix**: discovered and fixed that `bindings/julia/src/c_api.jl`'s `quiver_database_options_t` definition was silently missing the `ui_config_dir`/`ui_locale` fields that plan 02-05 (commit `36851bf`) had added — a prior merge (`e8d35b9`, "Merge branch 'master' into rs/enums") dropped the two lines without a recorded conflict. This made `Quiver.C.__init__()`'s own load-time struct-size gate refuse to initialize the module at all ("Julia expects 8 bytes, the loaded native library reports 24 bytes"), blocking the *entire* Julia test suite, not just this plan's new cases. Fixed by restoring the two fields verbatim (confirmed via `git show 36851bf:...` that nothing else in `bindings/julia/src/` diverged).

## Task Commits

Each task was committed atomically:

1. **Task 1: C API and Lua assert malformed polarity and the :memory: distinction** — `60cda47` (test)
2. **Unplanned regression fix** (Rule 1, blocking Task 2) — `bc1fe6e` (fix)
3. **Task 2: Julia and Dart cover malformed, :memory:, open/from_migrations, and describe()** — `303fbd2` (test)
4. **Task 3: Python and JS cover malformed, :memory:, open/from_migrations, and describe()** — `222141e` (test)

_No separate plan-metadata commit was made for this file; per `<sequential_execution>` this SUMMARY is committed as part of the standard non-worktree flow below._

## Files Created/Modified

- `tests/test_c_api_database_options.cpp` — two new `TEST`s for malformed polarity and the `:memory:` + explicit-dir case.
- `tests/test_lua_runner_ui_options.cpp` — three new `TEST`s driving `has_ui_config()` through `LuaRunner::run`.
- `bindings/julia/src/c_api.jl` — restored `ui_config_dir`/`ui_locale` `Ptr{Cchar}` fields (regression fix).
- `bindings/julia/test/test_database_ui_options.jl` — six new `@testset`s + a `scratch_migrations_dir()` helper.
- `bindings/dart/test/database_ui_options_test.dart` — six new `group`s + a `scratchMigrationsDir()` helper.
- `bindings/python/tests/test_database_ui_options.py` — six new `test_*` functions + `_write_migrations_dir()` helper.
- `bindings/js/test/database-ui-options.test.ts` — six new `test`s + `makeScratchMigrationsDir()` helper.

## Decisions Made

- Restored the merge-dropped Julia struct fields as a standalone commit (`bc1fe6e`) separate from the test-addition commits, since it is a production-code fix (Rule 1), not a test change, even though it was required to make Task 2's verification possible.
- Reused the existing C API `HasUiConfigFalseWhenDirectoryAbsent` test for the `:memory:` + no-override polarity rather than adding a duplicate, since it already exactly matched the plan's third required case.
- `from_migrations()` coverage in all four bindings uses a runtime-generated scratch migrations directory (one version, `up.sql` = `foresight_like/schema.sql`'s own DDL, trivial `down.sql`) rather than a new committed fixture — the plan's `files_modified` list is the six test files only, and no existing migrations fixture creates the `EconomicDriver` schema the sidecar labels.
- Lua's new assertions check the exact JSON-encoded return string (`"true"`/`"false"`) rather than an in-script Lua `assert`, per the plan's explicit instruction to prove what crosses the `LuaRunner` boundary.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Restored ui_config_dir/ui_locale fields dropped from Julia c_api.jl by a merge**
- **Found during:** Task 2 (running `bindings/julia/test/test.bat` for the first time in this plan)
- **Issue:** `bindings/julia/src/c_api.jl`'s `quiver_database_options_t` struct was missing the two `Ptr{Cchar}` fields (`ui_config_dir`, `ui_locale`) that plan 02-05 had added in commit `36851bf`. The merge commit `e8d35b9` dropped these two lines silently (no recorded conflict on this file). This made `Quiver.C.__init__()`'s load-time struct-size gate fail immediately on module load ("Julia expects 8 bytes, the loaded native library reports 24 bytes"), so `bindings/julia/test/test.bat` could not run *any* test — not just this plan's additions.
- **Fix:** Restored the two fields verbatim in the struct definition, confirmed via `git show 36851bf:bindings/julia/src/c_api.jl` that they were present pre-merge and that nothing else in `bindings/julia/src/` differs from that commit.
- **Files modified:** `bindings/julia/src/c_api.jl`
- **Verification:** `bindings/julia/test/test.bat` — 1478/1478 passed (was failing to even load the module before the fix).
- **Committed in:** `bc1fe6e` (separate commit, before Task 2's test additions)

---

**Total deviations:** 1 auto-fixed (1 Rule 1 bug fix — a merge regression, not caused by this plan's own changes but blocking Task 2's ability to run at all)
**Impact on plan:** Necessary for Task 2 (and the rest of the Julia suite) to run at all. No scope creep — the fix is a two-line restoration of previously-committed, previously-reviewed code.

## Issues Encountered

- JuliaFormatter (`bindings/julia/format/format.bat`) could not run locally in this environment (`Style` package not resolvable — a pre-existing local Julia depot/environment issue, unrelated to this plan's changes). The Julia test file additions were hand-formatted to match the surrounding file's existing style (4-space indent, `@testset` nesting) since the automated formatter was unavailable; not re-attempted per the scope-boundary rule (pre-existing environment issue, out of scope).

## User Setup Required

None — no external service configuration required.

## Verification

- `cmake --build build --config Debug` — succeeded.
- `./build/bin/quiver_c_tests.exe` — **569/569 passed** (baseline 567; +2 new).
- `./build/bin/quiver_tests.exe` — **1304/1304 passed** (well above the 1175 02-VERIFICATION.md baseline; +8 new across `DatabaseCApiOptions`/`LuaRunnerUiOptions` combined, plus growth from other work already in this checkout).
- `./build/bin/quiver_tests.exe --gtest_filter=*FixturesAreNeverCopiedIntoABinding*` — passed (guards that no fixture was copied into any binding).
- `bindings/julia/test/test.bat` — **1478/1478 passed** (baseline 1465; +13, including the "UI Options" testset at 27/27). Required the regression fix above to run at all.
- `bindings/dart/test/test.bat` (full suite) and a direct re-run of `dart test test/database_ui_options_test.dart` — **440/440** full suite (baseline 434; +6) and **11/11** for the UI options file alone (was 5).
- `uv run pytest bindings/python/tests/` (via `build/bin` on PATH) — **327/327 passed** (baseline 318, prior plan 02-08 raised it to 321; +6 new here).
- `bun test test` (JS, full suite) — **237/237 passed** (baseline 231; +6), and `bun test test/database-ui-options.test.ts` alone — **10/10 passed** (was 4).
- `git diff --stat` across all four task commits confines changes to exactly the six planned test files plus the one-line-pair regression fix in `bindings/julia/src/c_api.jl` — no production behaviour changed, no fixture added or modified.
- `grep -rl 'main.toml' bindings/ --include=*.toml` — no matches (no fixture copied into any binding).
- Ran `dart format`, `bunx biome check --write`, and `uv run ruff format` on the touched Dart/JS/Python files (cosmetic reformatting only, re-verified green after formatting).

## Next Phase Readiness

- Gap 5 (malformed polarity + `:memory:` distinction, previously C++-only) is closed across all six layers.
- Gap 7 (self-reported `open`/`from_migrations` and whole-database `describe()` coverage holes from SC1) is closed across all four FFI bindings.
- SC2 in 02-VERIFICATION.md should move from PARTIAL to MET on re-verification of this plan's scope; the remaining SC3/SC4/SAFE-01(JS) gaps and the stale-clone/CHANGELOG blocker are out of this plan's scope and unaffected by it.
- The runtime-generated scratch-migrations-directory pattern established here is reusable by any future plan that needs to exercise `from_migrations()` against a fixture schema with no committed migrations directory.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
