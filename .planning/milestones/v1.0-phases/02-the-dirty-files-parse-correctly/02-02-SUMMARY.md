---
phase: 02-the-dirty-files-parse-correctly
plan: 02
subsystem: testing
tags: [lua, csv-parser, csv-parser-tests, header_row, PARSE-02..07, LUA-06, TEST-01, TEST-04]

requires:
  - phase: 02-the-dirty-files-parse-correctly (plan 01)
    provides: "csv_read::Options.header_row and read_csv_options_from_lua's header_row decoder"
provides:
  - "The dirty-file fixture matrix discharging PARSE-02 through PARSE-07 through db:read_csv"
  - "The LUA-06 test proving repeated/blank header names remain fully reachable (D-21)"
  - "The four TEST-04 decoder-side header_row negatives (wrong type, fraction, negative, stream)"
affects:
  - "tests/test_lua_runner_read_csv.cpp"

actuals:
  tokens: 3187
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "BOM built as its own std::string, concatenated with +, rather than embedded in the same string literal as the following text -- \\xEF\\xBB\\xBF directly followed by an alphanumeric inside one literal has the hex escape swallow that character too."

key-files:
  created: []
  modified:
    - tests/test_lua_runner_read_csv.cpp

key-decisions:
  - "Task 2's 'header row on the last line' test was not duplicated: 02-01 already added HeaderRowOnLastLineSucceedsWithEmptyRows covering the identical scenario verbatim. Added a one-line cross-reference comment there instead of a redundant test."
  - "Task 3's 'correct the stale FutureHeaderKeyIsAnUnknownOptionToday comment' item required no action: 02-01 already corrected it (verified by reading the current comment before touching anything)."

requirements-completed: [PARSE-02, PARSE-03, PARSE-04, PARSE-05, PARSE-06, PARSE-07, LUA-06, TEST-01, TEST-04]

coverage:
  - id: D1
    description: "Composite fixture (BOM + CRLF + quoted comma + doubled quote + embedded newline + ragged row) round-trips through db:read_csv to the exact expected JSON, with per-requirement Lua asserts naming PARSE-02..07"
    requirement: "PARSE-02"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.DirtyFileParsesEveryParserRequirement"
        status: pass
    human_judgment: false
  - id: D2
    description: "LF and CRLF variants of the same dirty content parse to identical JSON (PARSE-06's second half)"
    requirement: "PARSE-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.LfAndCrlfEndingsParseIdentically"
        status: pass
    human_judgment: false
  - id: D3
    description: "BOM is stripped under an explicit header_row (2) and under no-header (0) alike, asserted by value and by string length"
    requirement: "PARSE-05"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.BomStrippedUnderExplicitHeaderRowAndNoHeader"
        status: pass
    human_judgment: false
  - id: D4
    description: "The real Maranhao header's duplicate ANO/Residencial names and five blank names all remain reachable by index, verbatim, with no column shadowed or missing"
    requirement: "LUA-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.RepeatedAndBlankHeaderNamesAllReachable"
        status: pass
    human_judgment: false
  - id: D5
    description: "header_row as a quoted string, a fractional number, and a negative integer (both entry points) each throw the exact Pattern 1 message rather than silently falling back to a default; all four appended to the blanket unwrapped-message guard"
    requirement: "TEST-04"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.HeaderRowAsStringThrowsMustBeAnInteger"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.HeaderRowAsFractionThrowsMustBeAnInteger"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.NegativeHeaderRowThrowsMustNotBeNegative"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.StreamNegativeHeaderRowNamesTheStreamEntryPoint"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EveryNegativeCaseStartsWithItsOwnEntryPointPrefix"
        status: pass
    human_judgment: false

duration: ~25min
completed: 2026-09-16
status: complete
---

# Phase 2 Plan 2: The dirty-file test matrix (PARSE-02..07, LUA-06, TEST-04) Summary

Test-only plan (zero production code) adding 8 new tests to `test_lua_runner_read_csv.cpp` that
pin PARSE-02 through PARSE-07, LUA-06, and the `header_row` decoder's TEST-04 negatives — 56 to 64
tests in the suite, 1167 to 1175 in the full C++ binary.

## Performance

- **Duration:** ~25 min
- **Tasks:** 3
- **Files modified:** 1 (`tests/test_lua_runner_read_csv.cpp`)

## Accomplishments

- **Task 1 — the dirty-file matrix (PARSE-02..07):** `DirtyFileParsesEveryParserRequirement` uses
  the exact composite fixture from 02-CONTEXT.md's verified probe (BOM + CRLF + a quoted comma +
  a doubled quote + an embedded newline + a short final row), asserting one exact JSON string then
  six per-requirement Lua asserts naming PARSE-02 through PARSE-07 each. `LfAndCrlfEndingsParseIdentically`
  proves the same content under LF endings produces byte-identical JSON (PARSE-06's second half).
  `BomStrippedUnderExplicitHeaderRowAndNoHeader` proves PARSE-05 holds under `header_row = 2` and
  `header_row = 0` alike, asserted by value and by string length so an invisible 3-byte prefix
  cannot pass a visual-only check. No production code touched — all six properties already hold
  against the Phase 1 reader.
- **Task 2 — LUA-06 (D-21):** `RepeatedAndBlankHeaderNamesAllReachable` reads a fixture using the
  real Maranhao Energia header line (`ANO,Residencial,,ANO,MÊS, Residencial ,,,,,`, **11** fields —
  counted from the fixture itself, not the "twelve" an earlier planning draft said) and asserts
  both `ANO` positions and both `Residencial` positions (one space-padded) hold their verbatim
  names, every blank name is `""` (not absent), and all 11 data columns are reachable at their own
  index. No code discharges this requirement — Phase 1's positional `header`/`rows` shape has
  nothing for a duplicate name to shadow.
- **Task 3 — TEST-04 header_row negatives:** Four new tests (`HeaderRowAsStringThrowsMustBeAnInteger`,
  `HeaderRowAsFractionThrowsMustBeAnInteger`, `NegativeHeaderRowThrowsMustNotBeNegative`,
  `StreamNegativeHeaderRowNamesTheStreamEntryPoint`) each assert the exact Pattern 1 message the
  02-01 decoder already raises. All four scripts appended to
  `EveryNegativeCaseStartsWithItsOwnEntryPointPrefix`'s vector.

## Task Commits

Each task was committed atomically:

1. **Task 1: The dirty-file matrix** — `98d71e9` (test)
2. **Task 2: Repeated/blank header names + last-line cross-reference** — `7c39282` (test)
3. **Task 3: header_row decoder negatives** — `bca844f` (test)

**Plan metadata:** committed alongside this SUMMARY (see below)

_No TDD RED/GREEN split: every test in this plan asserts pre-existing behavior (per the phase's
own scope_warning), so there is no failing-first phase to separate from a passing-after phase._

## Files Created/Modified

- `tests/test_lua_runner_read_csv.cpp` — 8 new `TEST_F` cases plus one cross-reference comment on
  a pre-existing test; no other file touched.

## Decisions Made

- Did not duplicate Task 2's "header row on the last line" test: `HeaderRowOnLastLineSucceedsWithEmptyRows`
  (added in plan 02-01) already covers this exact scenario (`header_row = 3` naming a file's final
  line, non-empty header, zero rows) verbatim. Added a one-line comment there cross-referencing this
  plan instead, so a future reader can see the requirement is satisfied without hunting for a
  second, identical test.
- Did not touch the `FutureHeaderKeyIsAnUnknownOptionToday` comment: plan 02-01 already corrected
  it (verified by reading the current text before acting) — Task 3's instruction to fix it was
  already satisfied.

## Deviations from Plan

### Noted, Not Duplicated

**1. Task 2's second test already exists (added in 02-01)**
- **Found during:** Task 2
- **Issue:** The plan's Task 2 asked for `HeaderRowOnLastLineIsAHeaderOnlyFile`, pinning the
  boundary between "header found, no data after it" and "header not found" via an explicit
  `header_row` naming the file's final line. Plan 02-01 already added
  `HeaderRowOnLastLineSucceedsWithEmptyRows`, which is the identical scenario (same fixture shape,
  same assertion) — confirmed by reading 02-01-SUMMARY.md and the existing test before writing
  anything new.
- **Resolution:** No new test added (would have been a byte-for-byte duplicate in spirit). Added a
  cross-reference comment on the existing test instead.
- **Files modified:** `tests/test_lua_runner_read_csv.cpp`
- **Commit:** 7c39282

**2. Task 3's stale-comment fix already applied (in 02-01)**
- **Found during:** Task 3
- **Issue:** The plan's Task 3 instructed correcting `FutureHeaderKeyIsAnUnknownOptionToday`'s
  comment (it anticipated a future key named `header`, but D-20 settled on `header_row`). Reading
  the current file showed 02-01 had already made exactly this correction.
- **Resolution:** No change made; verified the existing comment text is already correct.
- **Files modified:** none
- **Commit:** n/a

No auto-fixed issues (Rules 1-3) — this plan added zero production code, so there was nothing to
break, fix, or harden. Both items above are "found already done", not deviations from correctness.

## Issues Encountered

None. All 8 new tests passed on first build/run (expected: PARSE-02..07 were pre-verified against
the Phase 1 reader with zero code, LUA-06 needs no code by construction, and the `header_row`
decoder's validation was already implemented and tested for its happy paths in plan 02-01).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- PARSE-02..07, LUA-06, TEST-01 (for this scope), and TEST-04 are now fully discharged by
  hand-written fixtures through the Lua boundary.
- TEST-02 (the two real Maranhão CSV regression tests) and TEST-05 (Release-build parity) remain
  for later plans in this phase — this plan's frontmatter requirements list did not include them.
- `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv.*'` — 64/64 pass.
- `./build/bin/quiver_tests.exe` (full suite) — 1175/1175 pass.
- `clang-format --dry-run --Werror` on the touched file — clean.

## Known Stubs

None. No stub patterns, placeholder values, or unwired data paths were introduced — this plan
added only test code.

## Threat Flags

None. No new file-path input surface, network surface, or trust-boundary change; `header_row`'s
own threat register entries (T-02-04, T-02-05) are discharged by this plan's Task 3 tests
(the throw-not-fallback assertions and the blanket-prefix list additions).

## Self-Check: PASSED

- `tests/test_lua_runner_read_csv.cpp` — FOUND, contains all 8 new `TEST_F` names.
- Commit 98d71e9 — FOUND in `git log`.
- Commit 7c39282 — FOUND in `git log`.
- Commit bca844f — FOUND in `git log`.
