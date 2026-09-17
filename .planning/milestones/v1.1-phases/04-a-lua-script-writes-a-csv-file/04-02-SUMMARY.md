---
phase: 04-a-lua-script-writes-a-csv-file
plan: 02
subsystem: database
tags: [lua, sol2, csv, sqlite-wrapper, quiver-lua-runner, error-handling]

# Dependency graph
requires:
  - phase: 04-a-lua-script-writes-a-csv-file
    provides: "quiver::csv_write::Writer, the CsvWriter sol2 usertype, db:write_csv/w:write_row/w:close, the cell-type dispatch and options decoder -- all from 04-01, extended here"
provides:
  - "The FMT-05 non-finite-number guard in the write_row cell formatter, naming write_row, the 1-based data-row ordinal, and the 1-based cell index"
  - "A reordered w:write_row that checks the closed state before formatting any cell, so a bad cell on a closed writer reports the closed-writer message, not a cell-formatting one (WRITE-05)"
  - "The pinned Pattern 1 message catalogue for db:write_csv/w:write_row/w:close, as a comment block atop src/csv_write.cpp (TEST-12's single source of truth)"
  - "34 tests in tests/test_lua_runner_write_csv.cpp: the plan's own behavior-list tests plus a dedicated LuaRunner_WriteCsvErrors group asserting Pattern 1 prefix + reason substring separately for every catalogued error, the LUA-10 precedence case, and the file-intact pcall/close/read-back proof"
  - "The executed, recorded std::to_chars non-finite spot-check on this toolchain (MSVC 19.51.36256), closing the milestone research's one MEDIUM-confidence claim"
affects: ["04-03 (FMT-07 header-as-width-authority, the DOC-05 worked-example execution test)", "Phase 5 (WRITE-06 unclosed-writer flush, TEST-11)"]

# Actuals (#2632)
actuals:
  tokens: 5900
  tasks: 3
  commits: 3

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Delegate-to-reuse-the-message: when a guard's message already exists on the callee (Writer::write_row's closed-writer throw), the caller checks the same condition first to control ORDERING (closed before cell-formatting) but re-invokes the callee with a dummy argument to raise, rather than duplicating the message text at a second call site"
    - "Ordinal-on-accept, not on-attempt: CsvWriter::next_row_index increments only after a row is successfully written, so a script that retries a rejected row inside pcall sees the same row number on the retry"
    - "expect_prefixed_error(prefix, reason) pair-assertion, adapted from test_lua_runner_read_csv.cpp's single-prefix version to take an explicit expected prefix plus a reason substring, checked as two separate EXPECT_* calls so neither can satisfy the other's failure mode"

key-files:
  created: []
  modified:
    - src/lua_runner.cpp
    - src/csv_write.cpp
    - src/csv_write.h
    - tests/test_lua_runner_write_csv.cpp

key-decisions:
  - "WRITE-05's closed-writer throw and WRITE-07's missing-parent-directory throw were already implemented in 04-01 (task 1's tracer built them ahead of schedule, per that plan's own task 1 action text) -- this plan's task 2 only needed to (a) add the FMT-05 finiteness guard, which was genuinely new, and (b) fix an ordering bug where w:write_row formatted a cell BEFORE checking whether the writer was closed, so a bad cell on a closed writer raised the wrong message."
  - "The finiteness guard's row ordinal increments only on a SUCCESSFUL write_row, not on every attempt -- confirmed against the acceptance criterion's exact scenario (two good rows, then a rejected third reports ordinal 3): incrementing on attempt would also satisfy that one scenario, but incrementing on accept is the more defensible semantic for a script that retries a rejected row inside pcall (the retry keeps the same ordinal instead of skipping one)."
  - "The message catalogue comment block lives in src/csv_write.cpp per the plan's explicit acceptance criterion, even though two of its entries (the FMT-05 finiteness message and the pre-existing cell-type/row-key messages) are actually thrown in src/lua_runner.cpp, not csv_write.cpp itself -- documented as such in the comment, since TEST-12 tests the whole db:write_csv/w:write_row/w:close feature as one unit regardless of which file raises which line."
  - "Task 1's probe intentionally used `volatile double` operands for the four divisions, not plain doubles, so the compiler cannot constant-fold 0.0/0.0 or 1.0/0.0 into a compile-time value before std::to_chars ever sees it -- the point of the probe is what to_chars does at runtime on this toolchain, not what the optimizer might precompute."

requirements-completed: [WRITE-05, WRITE-07, FMT-05, TEST-12]

coverage:
  - id: D1
    description: "A non-finite double cell (NaN or +/-infinity) raises a Pattern 1 error naming write_row, the row ordinal, and the cell index, before std::to_chars ever sees the value"
    requirement: "FMT-05"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.NonFiniteNumberCellThrowsNamingWriteRowAndRowOrdinal"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsvErrors.NonFiniteNumberCellIsPrefixedWriteRowError"
        status: pass
    human_judgment: false
  - id: D2
    description: "A rejected non-finite row leaves the file exactly as it was before the failing call -- proven inside one script via pcall + w:close() + a db:read_csv read-back, asserting both the failed pcall message and the row count"
    requirement: "FMT-05"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.RejectedNonFiniteRowLeavesFileIntactAfterPcallAndClose"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsvErrors.RejectedRowLeavesFileIntactProvenBothHalves"
        status: pass
    human_judgment: false
  - id: D3
    description: "w:write_row after w:close() raises Pattern 1 naming write_row (checked BEFORE any cell is formatted); w:close() called twice raises nothing"
    requirement: "WRITE-05"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.WriteRowAfterCloseThrowsNamingWriteRow"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsvErrors.WriteAfterCloseIsPrefixedWriteRowError"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsvErrors.DoubleCloseIsIdempotentNotAnError"
        status: pass
    human_judgment: false
  - id: D4
    description: "db:write_csv into a non-existent parent directory fails at open with Pattern 1 naming write_csv and the caller's own path spelling; the directory is not created"
    requirement: "WRITE-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.MissingParentDirectoryThrowsAndDoesNotCreateIt"
        status: pass
    human_judgment: false
  - id: D5
    description: "TEST-12's five catalogued errors, idempotent close, and the LUA-10 precedence rule are each asserted by Pattern 1 prefix PLUS a reason substring, checked separately so a write_csv message can never satisfy a write_row assertion"
    requirement: "TEST-12"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsvErrors (8 tests)"
        status: pass
    human_judgment: false
  - id: D6
    description: "The one MEDIUM-confidence research claim (std::to_chars on a non-finite double) is executed and recorded, not merely inferred from standards text"
    verification:
      - kind: other
        ref: "scratchpad to_chars_probe.cpp/.out (not committed) -- output quoted verbatim below"
        status: pass
    human_judgment: false

duration: 55min
completed: 2026-09-16
status: complete
---

# Phase 4 Plan 2: The Error Catalogue Summary

**The four write-side guards -- a std::isfinite check before every to_chars cell, a closed-writer check before every cell format, the missing-parent-directory precondition, and a pinned message catalogue -- proven by 8 prefix-plus-reason TEST-12 assertions, plus the executed std::to_chars non-finite spot-check the milestone research left unverified.**

## Performance

- **Duration:** ~55 min
- **Started:** 2026-09-16 (session start; exact epoch not captured)
- **Completed:** 2026-09-16T19:07:00Z
- **Tasks:** 3
- **Files modified:** 4 (0 created, 4 modified)

## Accomplishments

- Executed the milestone's one MEDIUM-confidence research claim: compiled and ran a throwaway
  probe calling `quiver::utils::append_number`'s exact `std::to_chars` form (32-byte
  `std::array<char, 32>`, no format/precision argument) on four runtime-produced non-finite
  doubles, using this repo's own toolchain (MSVC 19.51.36256 for x64). Recorded verbatim below.
- Added the FMT-05 finiteness guard to `csv_cell_to_string`'s `double` branch in
  `src/lua_runner.cpp`: `std::isfinite` is checked before `append_number`/`to_chars` is ever
  reached, so no platform-specific non-finite spelling can land in a cell. The message names
  `write_row`, the 1-based data-row ordinal (a new `CsvWriter::next_row_index` member,
  incremented only after a row is accepted), and the 1-based cell index.
- Fixed an evaluation-order bug in the `w:write_row` sol2 binding: cells were previously formatted
  as an argument to `csv_write::Writer::write_row`, evaluated by C++ before the call, so a bad cell
  on an already-closed writer raised the cell error instead of the closed-writer error. The
  binding now checks `self.writer.is_closed()` first and, if true, delegates to
  `Writer::write_row({}, "write_row")` to reuse Writer's own pinned message rather than
  duplicating the text.
- Confirmed WRITE-05's closed-writer throw and WRITE-07's missing-parent-directory throw were
  already correct from 04-01's task 1 (built ahead of schedule per that plan's own instructions) --
  no change needed to `csv_write.cpp`'s constructor or `write_row` logic beyond the ordering fix
  above.
- Pinned the full Pattern 1 message catalogue for `db:write_csv`/`w:write_row`/`w:close` as a
  comment block atop `src/csv_write.cpp`, covering every throw the feature can raise across both
  `csv_write.cpp` and `src/lua_runner.cpp`'s cell/row/option decoders.
- Added 14 new tests to `tests/test_lua_runner_write_csv.cpp` (6 in the existing `LuaRunner_WriteCsv`
  fixture for task 2's behavior list, 8 in a new `LuaRunner_WriteCsvErrors` fixture for task 3's
  TEST-12 catalogue), including an `expect_prefixed_error(prefix, reason)` helper adapted from
  `test_lua_runner_read_csv.cpp`'s single-prefix version, asserting the Pattern 1 prefix and a
  reason substring as two separate `EXPECT_*` calls.
- Full suite: 1213 tests pass (up from 1199 at the start of this plan); `clang-format --dry-run
  -Werror` clean on every touched file.

## Executed Research Claim: `std::to_chars` on a Non-Finite Double

**Toolchain:** Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36256 for x64 (MSVC 14.51.36231
toolset, the same one CMake selected for `build/`), compiled with `/std:c++20 /EHsc`.

**Probe:** four `volatile double` operands (`zero = 0.0`, `one = 1.0`) divided at runtime so the
compiler cannot constant-fold the result, each passed through the exact
`quiver::utils::append_number` `std::to_chars` form (32-byte `std::array<char, 32>` buffer, no
format or precision argument).

**Verbatim output:**

```
quiet_nan (0.0/0.0)      errc=0 bytes=9 text="-nan(ind)"
neg_nan (-(0.0/0.0))     errc=0 bytes=3 text="nan"
pos_inf (1.0/0.0)        errc=0 bytes=3 text="inf"
neg_inf (-1.0/0.0)       errc=0 bytes=4 text="-inf"
```

All four calls returned `std::errc{}` (success, `errc=0`) -- `to_chars` never signals an error for
a non-finite input on this toolchain; it always writes a token. The unexpected finding: MSVC's
`0.0/0.0` produces an indeterminate NaN with the sign bit SET, spelled `"-nan(ind)"` (9 bytes) --
not the plain `"nan"` a naive reading of the standard might suggest -- while its unary negation
`-(0.0/0.0)` clears that sign bit and spells as plain `"nan"` (3 bytes). This is exactly the
platform-specific spelling FMT-05's guard exists to keep out of a cell: a glibc build is known to
spell the same two values differently (typically `"nan"` and `"-nan"`), so a written non-finite
token would disagree between platforms on IDENTICAL Lua source. The `std::isfinite` guard added in
task 2 throws before any of these four strings can reach `append_number`, regardless of what this
spot-check found -- the guard's necessity does not depend on the spot-check's outcome, but the
outcome is now a recorded fact instead of a standards-text inference, closing
`.planning/STATE.md`'s third Blocker.

The probe source and its captured stdout live in the session scratchpad
(`to_chars_probe.cpp`/`to_chars_probe.out`) and were not committed, per the task's own instruction.
`git status --porcelain src tests bindings` was empty at the end of task 1.

## Task Commits

Each task was committed atomically:

1. **Task 1: Execute the `std::to_chars` non-finite spot-check** - no commit (investigation only;
   no file under `src/`, `tests/`, or `bindings/` was modified, confirmed by `git status
   --porcelain`). Result recorded above and quoted verbatim.
2. **Task 2: The four guards** - `ba668a2` (test, RED) then `cc26c75` (feat, GREEN)
3. **Task 3: TEST-12 catalogue suite** - `8378ecb` (test)

_Task 2 carried `tdd="true"`; the RED commit added 6 tests, 2 of which genuinely failed against the
pre-guard code (the two non-finite-number cases) -- confirmed by running the suite before
implementing. The other 4 already passed because 04-01's task 1 built the closed-writer and
missing-parent-directory checks ahead of schedule; that is a correct RED state for the tests that
exercise genuinely new behavior, not a vacuous pass across the board._

## Files Created/Modified

- `src/lua_runner.cpp` - FMT-05 finiteness guard in `csv_cell_to_string`; `CsvWriter::next_row_index`;
  reordered `w:write_row` closed-check-before-formatting; `csv_row_cells_from_lua`'s new `row_index`
  parameter threaded through
- `src/csv_write.cpp` - Pinned Pattern 1 message catalogue comment block (no behavior change --
  WRITE-05/WRITE-07 were already correct)
- `src/csv_write.h` - Pointer comment to the catalogue's new location
- `tests/test_lua_runner_write_csv.cpp` - 14 new tests: 6 in `LuaRunner_WriteCsv` (task 2's
  behavior list), 8 in the new `LuaRunner_WriteCsvErrors` fixture (task 3's TEST-12 catalogue),
  plus the shared `expect_prefixed_error(prefix, reason)` helper

## Decisions Made

See `key-decisions` in the frontmatter above -- all four are implementation-detail calls made
within the plan's own instructions (ordinal-on-accept vs. ordinal-on-attempt, where the catalogue
comment lives, the delegate-to-reuse-the-message pattern, and the `volatile` probe operands), none
of them architectural.

## Deviations from Plan

None - plan executed exactly as written. The observation that WRITE-05's closed-writer throw and
WRITE-07's missing-parent-directory throw were already implemented (by 04-01, ahead of that plan's
own stated schedule) is not a deviation from THIS plan -- it meant task 2's actual new-code surface
was narrower than the plan's action text implied (the finiteness guard and the evaluation-order
fix), which is exactly what task 2's own read-first instructions asked to confirm before writing
any test.

## Issues Encountered

None. The RED phase's mixed pass/fail result (4 of 6 new tests already green) was expected once
the pre-existing code was read, not a surprise discovered mid-implementation.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- **Ready for plan 04-03**: FMT-07 (header-as-width-authority) and the DOC-05 worked-example
  execution test are both explicitly deferred to that plan; nothing in this plan's changes affects
  header handling or the reference text.
- **Ready for Phase 5**: WRITE-06 (flush-on-`run()`-exit for an unclosed writer) and TEST-11 remain
  untouched by design -- this plan's file-intact tests explicitly call `w:close()` and comment why
  it is load-bearing rather than redundant, so a future WRITE-06 implementation has a clear
  before/after test to compare against.
- No blockers. Full C++ suite (1213 tests) passes; `clang-format --dry-run -Werror` is clean on
  every file this plan touched.

---
*Phase: 04-a-lua-script-writes-a-csv-file*
*Completed: 2026-09-16*

## Self-Check: PASSED
All modified files verified present; all task commit hashes verified in git log.
