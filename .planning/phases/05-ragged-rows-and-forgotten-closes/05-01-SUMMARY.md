---
phase: 05-ragged-rows-and-forgotten-closes
plan: 01
subsystem: database
tags: [lua, sol2, csv, write_csv, quiver]

# Dependency graph
requires:
  - phase: 04-lua-csv-writer
    provides: "db:write_csv / w:write_row / w:close binding, CsvWriter, csv_write::Writer, the TEST-12 message catalogue"
provides:
  - "CsvWriter::header_width (0 = no enforcement), threaded from write_csv's decoded header option"
  - "FMT-07: a write_row row shorter than the header pads with empty cells; a row longer than the header throws a Pattern 1 error naming the row ordinal and both counts"
  - "TEST-10: the round-trip boundary suite for FMT-07 (short/exact/long, both zero-cell header widths, no-header, multi-byte UTF-8)"
affects: ["05-02 (WRITE-06 unclosed-writer flush)", "05-03 (docs sync for FMT-07)"]

actuals:
  tokens: 3709
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Row-width enforcement lives entirely in the Lua-layer CsvWriter wrapper; csv_write::Writer's public interface stays untouched (D-41)"
    - "Pad-before-call ordering: the vector is grown to header width BEFORE Writer::write_row is invoked, since append_record is a pure function of what it receives (Pitfall 3)"

key-files:
  created: []
  modified:
    - src/lua_runner.cpp
    - src/csv_write.cpp
    - tests/test_lua_runner_write_csv.cpp

key-decisions:
  - "header_width is a std::size_t member on CsvWriter, seeded once at construction from csv_options.header.size(); the decoded header vector itself is discarded (D-42)"
  - "The reject branch (row wider than header) sits ahead of the pad branch in the same if (self.header_width != 0) block; a long row is never truncated"
  - "The pinned message and its catalogue comment (src/csv_write.cpp) were added in the same commit as the throw, per the file's own no-reword-without-updating-the-test rule (D-45)"

patterns-established:
  - "FMT-07 width check: 0 = no header = no enforcement; short pads; long throws naming ordinal, actual count, declared width"

requirements-completed: [FMT-07, TEST-10]

coverage:
  - id: D1
    description: "A row shorter than the header pads with empty cells and round-trips through db:read_csv with every value under the column the script meant (ROADMAP criterion 1)"
    requirement: "FMT-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.ShortRowPadsToHeaderWidthAndRoundTripsAligned"
        status: pass
    human_judgment: false
  - id: D2
    description: "A row longer than the header throws a Pattern 1 error naming the row ordinal and both counts, and the rows already written remain on disk (ROADMAP criterion 2)"
    requirement: "FMT-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.RowLongerThanHeaderThrowsNamingOrdinalAndCounts"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.RejectedLongRowLeavesEarlierRowsOnDisk"
        status: pass
    human_judgment: false
  - id: D3
    description: "With no header given (option omitted, or header = {}), rows of differing widths are written as-is and no width error is raised (ROADMAP criterion 3)"
    requirement: "FMT-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.NoHeaderMeansNoWidthCheck"
        status: pass
    human_judgment: false
  - id: D4
    description: "TEST-10 boundary trio (N-1 pads, N passes through unchanged, N+1 throws) plus both zero-cell header-width edge cases and the multi-byte UTF-8 cell-count-not-byte-count case"
    requirement: "TEST-10"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.ExactWidthRowPassesThroughUnchanged"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.EmptyRowPadsToMultiColumnHeaderWidth"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.EmptyRowUnderSingleColumnHeaderStillRoundTrips"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.MultiByteUtf8CellsDoNotChangeCellCounts"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-09-17
status: complete
---

# Phase 5 Plan 01: FMT-07 header-width enforcement for db:write_csv Summary

**`CsvWriter::header_width` makes `db:write_csv`'s `header` option the row-width authority: a short row pads with empty cells, a long row throws a Pattern 1 error naming the row ordinal and both counts, and a header-less writer enforces nothing — all proven by 9 new `db:read_csv` round-trip tests.**

## Performance

- **Duration:** 25 min
- **Started:** 2026-09-17T10:00:00-03:00 (approx)
- **Completed:** 2026-09-17T10:07:19-03:00
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- `CsvWriter::header_width` (0 = no enforcement) threaded from `write_csv`'s decoded `csv_options.header.size()`, with a second constructor parameter
- `write_row`'s FMT-07 block: pads a short row to header width before `Writer::write_row` ever sees it (Pitfall 3 ordering), and throws `Cannot write_row: row <N> has <M> cells but header declares <W>` for a row wider than the header, never truncating
- The pinned message added to `src/csv_write.cpp`'s TEST-12 catalogue comment in the same commit as the throw (comment-only change to that file — `Writer`'s interface and `src/csv_write.h` untouched)
- 9 new gtests, all asserting through `db:read_csv` (never raw file bytes): short-row pad, too-long throw + ordinal/count naming, file-intact-after-reject, exact-width pass-through, zero-cell pad under a 3-name header, zero-cell pad under a 1-name header (TEST-09 shape preserved), no-header (omitted and `{}`) leaves ragged rows untouched, and multi-byte UTF-8 header/cells prove the check counts cells, not bytes

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end "a short row reads back aligned" — one path only (tracer)** - `ae71162` (feat)
2. **Task 2: The throw side, the pinned message, and the TEST-10 boundary suite** - `3887688` (feat)

_Task 1 is a `type="tracer"` task: its `<verify>` (the round-trip test) was re-run and confirmed passing before Task 2's expansion began._

## Files Created/Modified
- `src/lua_runner.cpp` - `CsvWriter::header_width` member + constructor param; `write_csv` factory passes `csv_options.header.size()`; `write_row`'s FMT-07 pad/throw block
- `src/csv_write.cpp` - one new line in the pinned TEST-12 message catalogue comment (comment-only)
- `tests/test_lua_runner_write_csv.cpp` - 9 new `TEST_F`s in the `LuaRunner_WriteCsv` fixture (1 from Task 1, 8 from Task 2 — TEST-10's boundary trio plus the too-long file-intact case counted separately)

## Decisions Made
- Padding happens strictly before `Writer::write_row` is called, matching D-44's trace of `append_record`'s `lone_empty_cell` predicate — padding after the call would misclassify a genuinely-3-cell padded row as a lone-empty-cell blank line.
- The reject branch is checked before the pad branch inside one `if (self.header_width != 0)` guard, so a header-less writer (`header_width == 0`) takes neither path.
- Read-back assertions use `db:read_csv` with the default `header_row = 1` where a header was given (so `csv.header` names the columns) and `header_row = 0` where no header exists — matching the surrounding suite's existing convention rather than introducing a third style.

## Deviations from Plan

None - plan executed exactly as written. Both tasks' acceptance criteria (grep counts, diff scoping, full-suite green) were verified directly against the commands the plan specified.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `CsvWriter::header_width` and the FMT-07 pad/throw block are in place for 05-02 (WRITE-06's unclosed-writer flush) to build on without touching this plan's code.
- `src/csv_write.cpp`'s catalogue comment now includes the FMT-07 line 05-03's doc sync (DOC-06) will need to reference.
- Whole-suite regressions confirmed clean: `quiver_tests.exe` 1232/1232 passed, `quiver_c_tests.exe` 557/557 passed.

---
*Phase: 05-ragged-rows-and-forgotten-closes*
*Completed: 2026-09-17*
