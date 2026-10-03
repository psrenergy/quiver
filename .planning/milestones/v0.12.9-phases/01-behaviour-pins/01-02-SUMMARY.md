---
phase: 01-behaviour-pins
plan: 02
subsystem: testing
tags: [lua-runner, js-sync-test, phase-gate, baseline]
status: complete
requires:
  - "01-01: LuaRunner_Lifecycle suite and the check-order / key-width pins"
provides:
  - "lua-api-sync.test.ts per-usertype non-empty floors and a single open_libraries( guard"
  - "Phase 1 gtest baseline: Lua* = 444 tests / 12 suites, LuaRunnerCApiTest = 27"
affects: [02-split, 03-dedupe, 04, 05]
tech-stack:
  added: []
  patterns:
    - "Meta-guard per parsed unit: list the units that parsed to nothing, expect []"
key-files:
  created: []
  modified:
    - bindings/js/test/lua-api-sync.test.ts
    - tests/AGENTS.md
    - bindings/js/AGENTS.md
    - .planning/STATE.md
decisions:
  - "Phase 1 gtest baseline is 444 Lua* tests across 12 suites and 27 LuaRunnerCApiTest tests, observed at 570c2c1; Phases 2 and 3 must reproduce both exactly"
requirements-completed: [PIN-03, PIN-05]
metrics:
  duration: "~35 min"
  completed: 2026-10-02
actuals:
  tokens: 1500
  tasks: 3
  commits: 3
---

# Phase 1 Plan 02: Sync-test guards and phase gate Summary

The JS Lua sync test now fails, naming the type, when any of BinaryFile/BinaryMetadata/Expression/CsvWriter parses
to zero methods, or when `open_libraries(` appears anything other than once. Both hand mutations proved this and
were reverted. The phase gate is green, and the baseline is 444 `Lua*` tests across 12 suites plus 27 C API tests.

## Tasks

| Task | Name | Commit | Files |
| ---- | ---- | ------ | ----- |
| 1 | Tracer: sync-test usertype and open_libraries guards | f8ae475 | bindings/js/test/lua-api-sync.test.ts |
| 2 | AGENTS.md: lifecycle file, sync-test guards, Lua* Release filter | 570c2c1 | tests/AGENTS.md, bindings/js/AGENTS.md |
| 3 | Phase gate and baseline | 9f44881 | .planning/STATE.md |

## Mutation checks (src/lua_runner.cpp, never built, reverted)

1. Deleted CsvWriter's `"write_row",` (l.898) and `"close",` (l.941) name lines. `parse found the binding surface`
   failed with received `["CsvWriter"]` (expected `[]`). BinaryFile's own `close` (l.997) did not mask it.
   `git checkout -- src/lua_runner.cpp`; `git diff --exit-code -- src/` exit 0.
2. Duplicated the `lua.open_libraries(` line (l.243): `expect(received).toBe(expected)`, `Expected: 1`,
   `Received: 2`. Reverted the same way, and `git diff --exit-code -- src/` exited 0.
3. Clean tree: 6 pass, 0 fail.

## Phase 1 gtest baseline

Counts are copied from `--gtest_list_tests` at HEAD `570c2c1` on 2026-10-02:

| Build | `Lua*` tests | `Lua*` suites | Run |
| ----- | ------------ | ------------- | --- |
| build/dev (Debug) | 444 | 12 | 444 passed |
| build/release (Release) | 444 | 12 | 444 passed |
| build/ (Debug, test-all) | 444 | 12 | in full run |

C API `LuaRunnerCApiTest.*` has 27 tests, all passing. The count is 444, not the 441 the plan predicted. Plan 01
added two freed-source move pins (`MoveConstructorOutlivesSource`, `MoveAssignmentOutlivesSource`) and
`LuaRunner_WriteCsv.NonTableOptionsThrows` (see 01-01-SUMMARY). The full `quiver_tests` run has 1410 tests across
43 suites, and `quiver_c_tests` has 543 across 11.

## test-all.bat

```
  C++ tests:        PASS
  C API tests:      PASS
  Julia tests:      PASS
  Dart tests:       PASS
  JavaScript tests: PASS
  Python tests:     PASS
All tests PASSED
```

Exit code 0, and `grep -Ec 'tests: +PASS'` prints 6. The banner prints without the `!` the plan's acceptance
string expects. cmd's delayed expansion most likely strips it, so that one exact-string grep fails on punctuation
alone. Every suite passed.

## Other gates

- The clang-format 22.1.8 `--dry-run --Werror` check over include/src/tests exits 0, and `biome check` on the sync
  test is clean.
- `git diff --name-only 5b57e7c -- . ':(exclude).planning'` lists exactly the ten allowed paths.
- The planning-ID grep over added lines in tests and bindings/js prints nothing.
- The removed-line grep (`git diff -U0 5b57e7c -- tests bindings/js/test | grep '^-[^-]'`) prints five lines, and
  this plan's own edits account for every one:
  - The `Expression`-only floor that Task 1 was told to replace. The new four-type floor still covers
    `Expression`.
  - Four rewrapped prose lines in tests/AGENTS.md, which Task 2 required. The criterion's path `tests` includes
    that file.
  - No `.cpp`/`.h`/CMake test line was removed or edited: the same grep restricted to those files prints nothing.
- `src/`, `include/` and `cmake/` are clean.

## Deviations from Plan

- **Tracer checkpoint not taken.** Auto mode is off, but the tracer's automated checks covered the whole slice
  (clean pass, both mutations failing, src/ clean), the orchestrator made the stop optional, and the remaining
  tasks were docs and the gate. Execution therefore continued without stopping.
- **Baseline 444 instead of 441**, as explained above. The observed count was recorded, not the predicted one.
- **tests/AGENTS.md `_lifecycle` gloss** describes all four move pins (live and freed source) and the
  `sizeof(LuaRunner)` static_assert, as the prior-wave context asked. This is more than the plan's one-line gloss.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: bindings/js/test/lua-api-sync.test.ts, tests/AGENTS.md, bindings/js/AGENTS.md
- FOUND: f8ae475, 570c2c1, 9f44881 on rs/runner
