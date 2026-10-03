---
phase: 01-behaviour-pins
plan: 01
subsystem: testing
tags: [lua-runner, gtest, behaviour-pins, move-semantics, csv, sandbox]
status: complete
requires: []
provides:
  - "LuaRunner_Lifecycle suite: 4 move pins (source alive and source freed) + sizeof(LuaRunner) static_assert"
  - "Check-order pins for open_file, read_csv, read_csv_stream, export_csv, import_csv, write_csv, closed write_row"
  - "1,000,000 key-width cap pins for write_row and write_csv header"
affects: [02-split, 03-dedupe]
tech-stack:
  added: []
  patterns:
    - "Freed-source move pin: std::make_unique source, move out, reset, then run on the moved-to runner"
    - "File-scope static_assert on sizeof(LuaRunner) as a Release-deterministic guard against run state outside Impl"
key-files:
  created:
    - tests/test_lua_runner_lifecycle.cpp
  modified:
    - tests/CMakeLists.txt
    - tests/test_lua_binary.cpp
    - tests/test_lua_runner_read_csv.cpp
    - tests/test_lua_runner_csv_export.cpp
    - tests/test_lua_runner_csv_import.cpp
    - tests/test_lua_runner_write_csv.cpp
decisions:
  - "Move pins come in two flavours: source kept alive (kills a run() that closes through a different object than bindings register into) and source freed (kills run state held by value on LuaRunner); plus static_assert(sizeof(LuaRunner) == sizeof(void*)) so the by-value case fails in Release too"
  - "The expr:save step in the move pins is reach-only coverage; the CSV and binary close checks are the deterministic move signals"
requirements-completed: [PIN-01, PIN-02, PIN-04, PIN-05]
metrics:
  duration: "~134 min wall clock (incl. tracer checkpoint review)"
  completed: 2026-10-02
actuals:
  tokens: 2600
  tasks: 3
  commits: 4
---

# Phase 1 Plan 01: Behaviour Pins Summary

Fifteen new LuaRunner pins (4 move, 6 check-order in four area files, 5 write_csv cap/order/closed-writer) plus a
`sizeof(LuaRunner)` static_assert, all green in Debug and Release. No production file touched.

## Tasks

| Task | Name | Commit | Files |
| ---- | ---- | ------ | ----- |
| 1 | Tracer: runner move pins in a new lifecycle test file | 01830f7 | tests/test_lua_runner_lifecycle.cpp, tests/CMakeLists.txt |
| 1a | Checkpoint change: freed-source move pins + static_assert | acfbf92 | tests/test_lua_runner_lifecycle.cpp |
| 2 | Check-order pins for open_file, read_csv, read_csv_stream, export_csv, import_csv | 773878f | tests/test_lua_binary.cpp, tests/test_lua_runner_read_csv.cpp, tests/test_lua_runner_csv_export.cpp, tests/test_lua_runner_csv_import.cpp |
| 3 | Key-width cap pins, write_csv order pin, closed-writer order pins | cbbc710 | tests/test_lua_runner_write_csv.cpp |

## Observed counts (after Task 3)

| Build | `Lua*` tests | `Lua*` suites | Result |
| ----- | ------------ | ------------- | ------ |
| build/dev (Debug) | 443 | 12 | all passed |
| build/release (Release) | 443 | 12 | all passed |

428 baseline + 15 new = 443. The plan said 441; the +2 are the freed-source move pins (see Deviations).
`LuaRunner_Lifecycle.*` lists 4 tests in both builds. The existing `RowWidthComesFromMaxIntegerKeyNotKeyCount`,
`RowWithZeroIntegerKeysWritesOneQuotedEmptyCell`, `NonIntegerRowKeyThrows` and `SubOneIntegerRowKeyThrows` pass in
both builds.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical coverage] The move pins could not catch run state held by value on LuaRunner**
- **Found during:** Task 1 tracer gate (orchestrator mutation testing)
- **Issue:** Surviving mutant M1a: the handle registries moved out of the heap Impl into a by-value
  `LuaRunHandles handles_` member on `LuaRunner`, with Impl holding a reference to it and the move ops still
  `= default`. Both `MoveConstructor` and `MoveAssignment` passed (and all 430 Lua* tests), because registration and
  close both go through the same reference into `source`, which the tests keep alive. That is the "member outside
  Impl" case the split must not introduce.
- **Correction to the plan's rationale:** Task 1's "keep `source` alive, a short-lived moved-from object would turn
  a future misplaced registry into UB instead of a deterministic failure" and RESEARCH assumption A1 hold only when
  `run()` closes through a different object than the bindings register into (mutant M1b, which the alive-source
  pins do kill). For M1a the live source makes the pins pass silently.
- **Fix:** Kept `MoveConstructor` / `MoveAssignment` unchanged (they kill M1b/M2/M3/M5 deterministically) and added
  `MoveConstructorOutlivesSource` / `MoveAssignmentOutlivesSource`, which free the source (`std::make_unique` +
  `reset()`) before the moved-to runner runs again. Under M1a they crash (SEH 0xc0000005) in Debug, per the
  orchestrator's verifier. Because that kill relies on Debug heap fill and may pass in Release, added a file-scope
  `static_assert(sizeof(quiver::LuaRunner) == sizeof(void*), ...)`; it compiles on the baseline (MSVC `unique_ptr`
  with default deleter is pointer-sized), so it was kept.
- **Count consequences:** Task 1 greps for `TEST_F(LuaRunner_Lifecycle, Move`, `origin == 'first'` and
  `second_doubled.qvr` each print 4 (not 2); `Lua*` end state is 443 tests (not 441) across 12 suites.
- **Files modified:** tests/test_lua_runner_lifecycle.cpp
- **Commit:** acfbf92

### Refuted (no action)

- The `expr:save` step in `expect_handles_closed` is reach-only coverage: with one shared `Database`, a misplaced
  `db` capture cannot be told apart. The CSV and binary close checks are the deterministic move signals.

## Verification

- Six Task 2 pins and five Task 3 pins each list and pass in dev and release.
- clang-format 22.1.8 `--dry-run --Werror` clean on all seven touched files.
- `git diff -U0 5b57e7c -- tests | grep '^-[^-]'` prints nothing (no existing line edited or removed).
- No planning IDs in any added test line (plan's ID grep prints nothing).
- `git diff --name-only 5b57e7c -- . ':(exclude).planning'` lists exactly the seven `files_modified`; nothing under
  src/, include/, cmake/ or bindings/.

## Post-execution verification

The orchestrator mutation-tested Tasks 2 and 3 and the move fix in throwaway worktrees (45 mutant runs against
`src/lua_runner.cpp`, Debug, plus M1a in Release), then had each surviving mutant independently refuted or confirmed.

- **Move fix holds.** M1a now fails to compile in Debug and Release (the `static_assert`, C2338). Without the
  assert, both OutlivesSource pins crash under M1a in Debug; in Release only `MoveConstructorOutlivesSource` does, so
  the `static_assert` is the only Release guard for the assignment case.
- **Every order pin fails when its named order is reversed**, and each asserts the full Pattern 1 message, so an
  unrelated error cannot satisfy it.
- **Two order pins had an unpinned control.** The import_csv and write_csv escape-before-options pins raise the
  escape error first, so they could not tell whether the "options must be a table" check still existed. A
  non-table options value silently meaning "defaults" survived the whole suite (for import_csv that runs a real,
  deleting import). Fixed by adding the controls the other entry points already have: an
  `"options must be a table"` assertion in `LuaRunner_ImportCSV.OptionsAreStrict` and the new
  `LuaRunner_WriteCsv.NonTableOptionsThrows`. `Lua*` is now **444 tests across 12 suites** in build/dev and build/release.
- **Also fixed:** the MoveConstructor comment no longer claims a live source makes every misplaced registry fail
  (M1a disproved it), and the five Task 2 pins now follow their files' `csv_schema` naming and blank-line habits.
- **Refuted, no change:** `:memory:` variants of the open_file mode / read_csv_stream on_row orders (excluded by
  CONTEXT D-04), the width cap's `>` vs `>=` boundary and a closed check moved after the key walk (orders the
  plan does not pin), and open_file accepting a multi-character mode (validation, not order).

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: tests/test_lua_runner_lifecycle.cpp
- FOUND: 01830f7, acfbf92, 773878f, cbbc710 on rs/runner
