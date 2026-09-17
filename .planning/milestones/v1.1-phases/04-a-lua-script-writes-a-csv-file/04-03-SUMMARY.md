---
phase: 04-a-lua-script-writes-a-csv-file
plan: 03
subsystem: database
tags: [lua, sol2, csv, sqlite-wrapper, quiver-lua-runner, testing]

# Dependency graph
requires:
  - phase: 04-a-lua-script-writes-a-csv-file
    provides: "quiver::csv_write::Writer, db:write_csv/w:write_row/w:close, the cell-type dispatch, the FMT-05 finiteness guard, the TEST-12 message catalogue, and the DOC-05 LUA_DB_API_REFERENCE section -- all from 04-01/04-02, proved here rather than extended"
provides:
  - "TEST-06 dirty-cell suite: combined separator+quote+CR+LF cell (comma and semicolon separators), leading/trailing/separator-only cells, an embedded CR-then-LF cell proven not to split rows, an empty cell beside a quoted neighbor, and a multi-byte UTF-8 cell with no quote byte -- every assertion round-tripped through db:read_csv"
  - "TEST-08 quote-doubling suite: a lone quote character and two quote characters, both asserted by exact read-back length"
  - "TEST-07 numeric suite: INT64_MIN/INT64_MAX via math.mininteger/math.maxinteger, and a float re-write identity check (write, read as string, re-write that string, compare) for 0.1, 1e-7, and DBL_MAX"
  - "The DOC-05 worked-example extraction test: reads bindings/js/src/lua-api.ts at run time, extracts and un-escapes the fenced Lua block, executes it, and asserts the produced file's cells positionally against the example's own data table -- including the interior nil at full row width"
  - "A green, explicitly-configured Release build (-DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON): 1224/1224 quiver_tests, 557/557 quiver_c_tests"
affects: ["Phase 5 (FMT-07 header-as-width-authority, WRITE-06 unclosed-writer flush, TEST-10/TEST-11, DOC-06 CHANGELOG)"]

# Actuals (#2632)
actuals:
  tokens: 5500
  tasks: 3
  commits: 3

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Reference-extraction test: read a shipped doc file at test run time, locate a `## heading`, extract the fenced code block between escaped ```lang markers, un-escape the TypeScript template-literal backslash-backtick, and execute the result -- mirrors bindings/js/test/lua-api-sync.test.ts's parse-the-binding technique, applied here in the opposite direction (parse the reference, not the binding)"
    - "Float re-write identity assertion: write a float, read it back as a string, write that string as a second file's cell, read it back again, and compare the two read-back strings -- proves no truncation/rounding without the test needing to know append_number's exact output text"

key-files:
  created: []
  modified:
    - tests/test_lua_runner_write_csv.cpp
    - .gitignore

key-decisions:
  - "The empty-cell-beside-a-quoted-cell test asserts round-trip correctness through db:read_csv rather than literally inspecting whether the emitted byte was quoted -- consistent with the plan's own round-trip-only rule (no ifstream/string-search for a correctness assertion). A regression that over-quoted the empty cell would still read back as \"\" either way, so the practical signal this suite can give is the neighbor-intact positional round trip, which is what FMT-02's narrowness would break if the quoting predicate mis-scoped its trigger to the whole row instead of the cell."
  - "Task 2's INT64_MIN/INT64_MAX and float-identity tests were the only genuinely new numeric coverage this plan needed to add -- the 9007199254740993 exact-digit-string test and the whole-float-equals-integer test were already built ahead of schedule in 04-01's task 1 tracer. Enhanced the existing 9007199254740993 test's comment with the regression-value detail the plan's action text asked for (the value one lower is what a routing-through-double regression produces), rather than duplicating a second test for the same fact."
  - "The DOC-05 worked example in bindings/js/src/lua-api.ts needed no adjustment -- it already ran verbatim against a sandbox database and produced exactly the cells its own data table claims, so the plan's conditional example-edit branch (\"if and only if it does not run\") did not fire."
  - "build-release/ is gitignored (added to .gitignore next to build/) rather than deleted after verification, so a future Release-only regression hunt does not have to re-pay the ~2 minute configure+build cost from a clean tree; either way it must never be committed, and it is not."

requirements-completed: [TEST-06, TEST-07, TEST-08, DOC-05]

coverage:
  - id: D1
    description: "Combined separator+quote+CR+LF cell, and the same fixture repeated under a semicolon separator, round-trip byte-identically through db:read_csv"
    requirement: "TEST-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.CellWithSeparatorQuoteCrAndLfTogetherRoundTrips"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.CellWithSeparatorQuoteCrAndLfTogetherRoundTripsWithSemicolonSeparator"
        status: pass
    human_judgment: false
  - id: D2
    description: "Leading-separator, trailing-separator, and separator-only cells round-trip positionally in one row, so an off-by-one in the quote-trigger scan cannot hide behind only one shape"
    requirement: "TEST-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.LeadingTrailingAndSeparatorOnlyCellsRoundTripPositionally"
        status: pass
    human_judgment: false
  - id: D3
    description: "A cell containing CR immediately followed by LF round-trips as that exact two-byte sequence, neither collapsed, merged with the record terminator, nor split into two rows"
    requirement: "TEST-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.CrThenLfCellRoundTripsAsTwoByteSequenceWithoutSplittingRows"
        status: pass
    human_judgment: false
  - id: D4
    description: "An empty cell adjacent to a cell that DOES need quoting (contains the separator) round-trips empty with both neighbors intact"
    requirement: "TEST-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.EmptyCellAdjacentToAQuotedCellRoundTripsWithNeighborsIntact"
        status: pass
    human_judgment: false
  - id: D5
    description: "A multi-byte UTF-8 cell containing no quote byte (0x22) round-trips byte-identically and unmodified"
    requirement: "TEST-06"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.MultiByteUtf8CellWithNoQuoteByteRoundTripsUnmodified"
        status: pass
    human_judgment: false
  - id: D6
    description: "A lone quote character serializes and round-trips as a length-1 string; two quote characters as a length-2 string"
    requirement: "TEST-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.LoneQuoteCharacterCellRoundTripsAsLengthOne"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.TwoQuoteCharacterCellRoundTripsAsLengthTwo"
        status: pass
    human_judgment: false
  - id: D7
    description: "INT64_MIN and INT64_MAX (via math.mininteger/math.maxinteger) round-trip as their exact decimal text -- the buffer-size boundary for append_number's 32-byte array"
    requirement: "TEST-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.MinIntegerAndMaxIntegerRoundTripExactDecimalText"
        status: pass
    human_judgment: false
  - id: D8
    description: "A float re-write identity check (write, read as string, re-write, re-read, compare) passes for 0.1, 1e-7, and the largest finite double, catching a 5-decimal or 6-significant-digit truncation without pinning the exact text"
    requirement: "TEST-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.FloatReWriteIdentityRoundTripsForManySignificantDigitValues"
        status: pass
    human_judgment: false
  - id: D9
    description: "The DOC-05 worked example in bindings/js/src/lua-api.ts is extracted from the reference file at test time, executed verbatim, and its produced file's cells (including the interior nil at full row width, and the written header record) are asserted positionally against the example's own data table"
    requirement: "DOC-05"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.ReferenceWorkedExampleRunsAndRoundTripsItsOwnData"
        status: pass
    human_judgment: false
  - id: D10
    description: "The whole suite is green in an explicitly-configured Release build (never the release preset), proving no SOL_SAFE_GETTER-off marshalling regression"
    verification:
      - kind: other
        ref: "build-release/bin/quiver_tests.exe (1224/1224) and build-release/bin/quiver_c_tests.exe (557/557), configured via cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON"
        status: pass
    human_judgment: false

duration: ~45min
completed: 2026-09-16
status: complete
---

# Phase 4 Plan 3: Dirty-Cell, Numeric, and Worked-Example Proof Summary

**11 new tests proving the writer against the cells that actually break CSV writers, the int64/float number path, and the shipped `LUA_DB_API_REFERENCE` worked example -- all round-tripped through `db:read_csv`, plus a green explicitly-configured Release build.**

## Performance

- **Duration:** ~45 min
- **Completed:** 2026-09-16
- **Tasks:** 3
- **Files modified:** 2 (0 created, 2 modified)

## Accomplishments

- **TEST-06 dirty-cell suite** (8 new tests): a single cell carrying the separator, a bare quote,
  a CR, and an LF all at once (both under the default comma and under a configured semicolon
  separator); a lone quote character and two quote characters, each asserted by exact read-back
  length; three separate cells in one row proving the leading-separator, trailing-separator, and
  separator-only shapes cannot hide an off-by-one; an embedded CR-then-LF cell proven to keep its
  exact two-byte length and to leave the surrounding row count unchanged (3 rows written, 3 read
  back); an empty cell beside a comma-carrying neighbor proving FMT-02's narrowness without
  disturbing either side; and a multi-byte UTF-8 cell with no `0x22` byte anywhere in it, built via
  `string.char` so the assertion never depends on this file's own source encoding.
- **TEST-07 numeric suite** (2 new tests + 1 enhanced comment): `math.mininteger`/`math.maxinteger`
  round-trip as their exact decimal text (the `append_number` 32-byte buffer's boundary), and a
  float re-write identity check -- write, read back as a string, re-write that string, read again,
  compare -- passes for `0.1`, `1e-7`, and `1.7976931348623157e308` (`DBL_MAX`) without the test
  needing to know `append_number`'s exact output text. The pre-existing
  `9007199254740993`-exact-digit-string test (built ahead of schedule in 04-01) and the
  whole-float-equals-integer test already covered the rest of TEST-07's acceptance criteria; only
  the comment on the former was extended with the regression-value detail the plan's action text
  asked for.
- **DOC-05 worked-example execution** (1 new test + a reusable `extract_lua_example` helper): reads
  `bindings/js/src/lua-api.ts` at test run time, locates the `## CSV file writing` heading, extracts
  the fenced ```` ```lua ```` block, un-escapes the TypeScript template-literal backslash-backtick
  escaping, and runs the result verbatim against a sandbox database with a supplied `path` local.
  Asserts the produced file's records positionally against the example's own data
  (`Alpha`/`first`/`true`/`42` and `Beta`/`nil`/`false`/`3.5`), including that the **interior**
  `nil` at position 2 of the `Beta` row round-trips as an empty cell at **full row width** (4
  cells) -- a future edit that moved that `nil` to a trailing position would shorten the row to 3
  cells and fail this assertion, by design (FMT-08). Also asserts the header record (`name` /
  `note` / `active` / `score`) was actually written, not merely decoded. Verified the extraction
  fails loudly (a named `extract_lua_example: heading not found: ...` exception) when pointed at a
  non-matching heading, then restored the correct heading; the example itself needed no
  adjustment.
- **Release build proof**: configured a separate tree explicitly
  (`-DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`, never the `release`
  preset, which sets `QUIVER_BUILD_TESTS=OFF`), built it, and ran the full suite: **1224/1224**
  `quiver_tests` and **557/557** `quiver_c_tests`, both matching the Debug run's pass count exactly
  -- no `SOL_SAFE_GETTER`-off marshalling regression. `build-release/` is gitignored (added next to
  `build/`), never committed.
- Full Debug suite: **1224 tests pass** (up from 1213 at the start of this plan: +8 task 1, +2
  task 2, +1 task 3 = 11 new tests, landing at 45 total in the write_csv suites, up from 34). The
  JS `lua-api-sync` suite (6/6) passes unchanged.

## Task Commits

Each task was committed atomically:

1. **Task 1: Dirty-cell and lone-quote round trips (TEST-06, TEST-08)** - `010a336` (test)
2. **Task 2: Numeric round trips -- int64 path and float text identity (TEST-07)** - `3774df5` (test)
3. **Task 3: Run the shipped worked example, and exercise the Release build** - `a361d3f` (test)

**Plan metadata:** committed together with this SUMMARY (see final commit below).

_No task in this plan carried `tdd="true"`; all three were plain `type="auto"` test-only tasks._

## Files Created/Modified

- `tests/test_lua_runner_write_csv.cpp` - 11 new `TEST_F` cases, one enhanced comment, and the new
  `extract_lua_example` helper (anonymous namespace)
- `.gitignore` - added `build-release/` next to `build/`

## Decisions Made

See `key-decisions` in the frontmatter above. None architectural; all are implementation-detail
calls made within the plan's own instructions (how to assert FMT-02's narrowness within the
round-trip-only rule, which numeric tests were genuinely new vs. already covered, that the example
needed no edit, and gitignoring vs. deleting the Release tree).

## Deviations from Plan

None - plan executed exactly as written. The observation that most of TEST-07's coverage and the
example's runnability were already correct from 04-01 is not a deviation from THIS plan -- it meant
task 2's and task 3's actual new-code surface was narrower than the plan's action text implied,
which is exactly what each task's own `<read_first>` instructions asked to confirm before writing
anything.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- **Phase 4 complete.** All three plans (04-01, 04-02, 04-03) are done; every requirement listed in
  `04-CONTEXT.md`'s in-scope set (WRITE-01..05, WRITE-07, WRITE-08, FMT-01..06, FMT-08, FMT-09,
  LUA-09..11, TEST-06..09, TEST-12, DOC-05) is implemented and tested.
- **Ready for Phase 5**: FMT-07 (header-as-width-authority), WRITE-06 (flush-on-`run()`-exit for an
  unclosed writer), TEST-10/TEST-11, and DOC-06 (the `CHANGELOG.md`/root-`CLAUDE.md`/`src/CLAUDE.md`
  milestone write-up) are all untouched by this plan, as scoped.
- No blockers. Full C++ suite (1224 tests) passes in both Debug and an explicitly-configured
  Release build; the JS `lua-api-sync` suite (6 tests) passes; `clang-format --dry-run -Werror` is
  clean on every file this plan touched.

---
*Phase: 04-a-lua-script-writes-a-csv-file*
*Completed: 2026-09-16*

## Self-Check: PASSED
All modified files verified present; all task commit hashes verified in git log.
