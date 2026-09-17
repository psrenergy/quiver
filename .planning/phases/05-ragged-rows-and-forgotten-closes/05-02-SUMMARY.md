---
phase: 05-ragged-rows-and-forgotten-closes
plan: 02
subsystem: database
tags: [lua, sol2, csv, write_csv, garbage-collection, quiver]

# Dependency graph
requires:
  - phase: 05-ragged-rows-and-forgotten-closes/05-01
    provides: "CsvWriter::header_width, the FMT-07 pad/throw block, and the LuaRunner_WriteCsv fixture conventions this plan's tests reuse"
provides:
  - "WRITE-06: GcGuard, a function-local RAII struct in LuaRunner::run whose destructor calls sol::state::collect_garbage() exactly once, flushing a writer the script never closed on both the normal-return and throw-unwinding paths"
  - "TEST-11: the two round-trip tests proving it -- UnclosedWriterIsFlushedWhenRunReturns and ScriptErrorMidWriteStillLeavesEarlierRowsReadable"
  - "The observed RED byte count (D-49): 0 bytes on both fixtures, matching the ROADMAP's claim exactly -- now verified, not merely asserted"
affects: ["05-03 (docs sync for FMT-07/WRITE-06, DOC-06)"]

actuals:
  tokens: 1546
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "One RAII scope guard drives a full sol2/Lua GC cycle at a C++ function boundary -- new to this codebase; TransactionGuard is a spirit-only analog (different resource, no prior code drives Lua GC from a scope guard)"
    - "Diagnostic-only byte-count evidence attached to a gtest FAIL() message via <<, never to the pass/fail assertion itself -- keeps the round-trip through db:read_csv as the sole correctness decision"

key-files:
  created: []
  modified:
    - src/lua_runner.cpp
    - tests/test_lua_runner_write_csv.cpp

key-decisions:
  - "GcGuard is declared immediately before `auto result = impl_->lua.safe_script(...)`, so C++'s reverse-declaration-order destruction runs the guard's collect_garbage() AFTER result's Lua stack reference is released (D-46)"
  - "Exactly one collect_garbage() call, unconditional, covering the throw path, the empty-return path, and the JSON-encode-return path uniformly -- no duplicated calls, no stabilization loop (D-48)"
  - "No warning, log line, or weak_ptr registry was added -- WRITE-06 is the flush alone, per the milestone's declined-diagnostic decision"

patterns-established:
  - "WRITE-06 flush: one RAII guard at run()'s top, one collect_garbage() call, covering every exit path including exception unwinding (D-47)"

requirements-completed: [WRITE-06, TEST-11]

coverage:
  - id: D1
    description: "A script that returns without calling w:close() leaves a complete, re-readable file, asserted via a second lua.run() on the same, never-destroyed LuaRunner (ROADMAP criterion 4)"
    requirement: "WRITE-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.UnclosedWriterIsFlushedWhenRunReturns"
        status: pass
    human_judgment: false
  - id: D2
    description: "A script that errors mid-write with the writer still open still leaves the rows written before the error on disk and readable -- the flush fires during stack unwinding too (D-47)"
    requirement: "WRITE-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.ScriptErrorMidWriteStillLeavesEarlierRowsReadable"
        status: pass
    human_judgment: false
  - id: D3
    description: "TEST-11 was observed genuinely RED before the fix, with the actual on-disk byte count (0 bytes, both fixtures) recorded rather than the ROADMAP's unverified claim repeated (D-49)"
    requirement: "TEST-11"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.UnclosedWriterIsFlushedWhenRunReturns (RED run captured in this SUMMARY's Performance/Deviations notes)"
        status: pass
    human_judgment: false

duration: 12min
completed: 2026-09-17
status: complete
---

# Phase 5 Plan 02: The unclosed-writer flush (WRITE-06) Summary

**One `GcGuard` RAII struct in `LuaRunner::run` calls `sol::state::collect_garbage()` exactly once, flushing a CSV writer the script never explicitly `close()`d — proven by two TEST-11 round-trip tests that were observed genuinely RED (0 bytes on disk, not the ROADMAP's assumed claim) before the fix landed.**

## Performance

- **Duration:** 12 min
- **Started:** 2026-09-17T13:13:00Z (approx)
- **Completed:** 2026-09-17T13:21:12Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- `GcGuard` — a function-local RAII struct declared before `safe_script` in `LuaRunner::run`, whose one-line destructor calls `impl_->lua.collect_garbage()`. Declaration order is load-bearing and documented inline: stack locals destroy in reverse declaration order, so the guard (declared first) runs *after* `result` (declared second) — releasing the Lua stack reference before the collection fires.
- Two new gtests in the `LuaRunner_WriteCsv` fixture, both the two-separate-`lua.run()`-calls shape this suite had no precedent for (RESEARCH.md Q4): `UnclosedWriterIsFlushedWhenRunReturns` (normal-return path) and `ScriptErrorMidWriteStillLeavesEarlierRowsReadable` (D-47's throw-unwinding path). Neither destroys, moves from, or resets the `LuaRunner` between the two calls — the ROADMAP criterion 4 trap this plan's `<phase_critical_rules>` warned against.
- **RED observed and recorded (D-49):** both fixtures produced a **0-byte file** on disk when read back against unmodified `LuaRunner::run` — the ROADMAP's "zero bytes" claim, previously unverified (RESEARCH.md rated it MEDIUM confidence), is now confirmed exactly, not merely repeated. The observed byte count is attached to each test's `FAIL()` message via `<<` (diagnostic only); the pass/fail decision stayed the `db:read_csv` round trip both before and after the fix.
- After the fix: both tests pass unmodified (`git diff` on the test file was empty for Task 2), the full `quiver_tests.exe` suite is 1234/1234 (1232 pre-existing + 2 new), and `quiver_c_tests.exe` is 557/557 — no regression anywhere in the suite.

## Task Commits

Each task was committed atomically:

1. **Task 1: TEST-11 RED — write both cases, observe the failure, record the actual byte count** - `3f06f41` (test)
2. **Task 2: The GcGuard — one collect_garbage() covering both returns and the throw path** - `8ec252d` (feat)

_Task 1 left the suite deliberately RED (2 failed, non-zero exit) against unmodified `LuaRunner::run`, confirmed by acceptance-criteria greps (`git diff --name-only` listed only the test file; `grep -c 'collect_garbage'` was 0). Task 2 turned both tests GREEN with zero changes to the test file._

## Files Created/Modified
- `src/lua_runner.cpp` — `GcGuard` struct + named local `gc_guard` at the top of `LuaRunner::run`, one `collect_garbage()` call in its destructor
- `tests/test_lua_runner_write_csv.cpp` — two new `TEST_F`s in `LuaRunner_WriteCsv`: `UnclosedWriterIsFlushedWhenRunReturns` and `ScriptErrorMidWriteStillLeavesEarlierRowsReadable`

## Decisions Made
- Matched the plan's exact declaration-order requirement: `GcGuard gc_guard{impl_->lua};` is the first statement in `run()`, before `auto result = impl_->lua.safe_script(...)`.
- Kept both fixtures to the plan's minimum shape (a 1-name header, one short row) per RESEARCH.md Pitfall 2, so a buffer-spill false-negative was never a risk.
- Wrote the byte-count diagnostic as a C++-level `FAIL() << ... << observed_bytes << " bytes"` inside a `try`/`catch` around the second `lua.run()` call, rather than embedding it in the Lua script's own `assert()` — this keeps the pass/fail decision purely the `db:read_csv` round trip (rule 6) while still surfacing the exact number on a failing run and printing nothing on a passing one.
- Trimmed the `GcGuard` doc comment's prose so the literal token `collect_garbage` appears exactly once in the file (the actual call site) — the plan's acceptance criterion (`grep -c 'collect_garbage'` == 1) counts matching lines, and an early draft's comment referenced the token twice more, which would have failed that check without changing behavior. No functional change, purely a wording fix caught before commit.

## Deviations from Plan

None — plan executed exactly as written. Task 1 left the suite red with `GcGuard`/`collect_garbage` absent from `src/lua_runner.cpp` (confirmed via `git diff --name-only`); Task 2 introduced exactly one `collect_garbage()` call and zero `log_warning`/`weak_ptr` tokens, matching every prohibition in `must_haves.prohibitions`.

## Issues Encountered
None. The one wording adjustment described above (trimming duplicate `collect_garbage` mentions in the comment) was caught and fixed before the commit, so it never landed as a separate deviation.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- WRITE-06 and TEST-11 are both shipped; ROADMAP Phase 5 criterion 4 is now true and asserted by an automated test.
- 05-03 (DOC-06: `src/CLAUDE.md`, root `CLAUDE.md`, `CHANGELOG.md`, and `bindings/js/src/lua-api.ts` per D-50) can now document FMT-07's pad/throw rule and WRITE-06's flush-without-close guarantee against fully-shipped, fully-tested behavior.
- Whole-suite regressions confirmed clean: `quiver_tests.exe` 1234/1234 passed, `quiver_c_tests.exe` 557/557 passed.

## Self-Check: PASSED

- `tests/test_lua_runner_write_csv.cpp` — FOUND
- `src/lua_runner.cpp` — FOUND
- `.planning/phases/05-ragged-rows-and-forgotten-closes/05-02-SUMMARY.md` — FOUND
- Commit `3f06f41` (test: TEST-11 RED) — FOUND
- Commit `8ec252d` (feat: GcGuard) — FOUND
- Commit `5fab64e` (docs: plan summary) — FOUND

---
*Phase: 05-ragged-rows-and-forgotten-closes*
*Completed: 2026-09-17*
