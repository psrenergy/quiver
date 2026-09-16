---
phase: 04-a-lua-script-writes-a-csv-file
plan: 01
subsystem: database
tags: [lua, sol2, csv, sqlite-wrapper, quiver-lua-runner]

# Dependency graph
requires:
  - phase: 04-a-lua-script-writes-a-csv-file
    provides: "db:read_csv / db:read_csv_stream (csv_read::Reader), resolve_sandboxed_path, the collect-then-validate options decoder shape, sol2 usertype ownership pattern (BinaryFile) -- all from the v1.0 read-side phases, reused unmodified"
provides:
  - "quiver::csv_write::Writer (src/csv_write.{h,cpp}) -- hand-rolled RFC-4180 CSV writer, not Pimpl'd"
  - "quiver::utils::append_number (src/utils/number.h) -- moved out of lua_runner.cpp, now shared by the JSON return-value encoder and the CSV cell formatter"
  - "db:write_csv Lua binding returning a CsvWriter sol2 usertype (w:write_row / w:close)"
  - "Full cell-type dispatch (string/int64/double/boolean/nil, reject table/function/userdata) and the max-integer-key row-width walk (FMT-08)"
  - "separator/header option validation and defaults for db:write_csv (LUA-09)"
  - "The full DOC-05 LUA_DB_API_REFERENCE section for CSV file writing"
affects: ["04-02 (non-finite guard FMT-05, write-after-close/idempotent-close WRITE-05, flush-on-run-exit WRITE-06)", "04-03 (header-as-width-authority FMT-07, executing the reference's worked example as a test)"]

# Actuals (#2632)
actuals:
  tokens: 12100
  tasks: 3
  commits: 4

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Non-Pimpl internal writer class mirroring csv_read's shape but deliberately skipping the Pimpl (D-37): no third-party headers to hide from lua_runner.cpp"
    - "Collect-then-validate Lua options decoder (LUA-09), copied from read_csv_options_from_lua's shape for write_csv's separator/header keys"
    - "sol2 usertype ownership via std::unique_ptr + sol::no_constructor + no explicit finalizer (LUA-11), mirroring BinaryFile"
    - "Row-cell dispatch order (nil, boolean, int64, double, string, else throw) mirrors table_to_element's heterogeneous dispatch; row width from a pairs-based max-integer-key walk, never size()/rawlen (FMT-08)"

key-files:
  created:
    - src/csv_write.h
    - src/csv_write.cpp
    - src/utils/number.h
    - tests/test_lua_runner_write_csv.cpp
  modified:
    - src/lua_runner.cpp
    - src/CMakeLists.txt
    - tests/CMakeLists.txt
    - bindings/js/src/lua-api.ts
    - bindings/js/test/lua-api-sync.test.ts
    - src/CLAUDE.md
    - tests/CLAUDE.md

key-decisions:
  - "Implemented WRITE-07 (missing parent directory reported, not silently created) even though it isn't in this plan's requirements frontmatter -- the plan's own task 1 action text specifies the check verbatim (mirroring csv_read.cpp's constructor precondition style), so it was built as part of the constructor rather than left half-finished."
  - "Task 1 (tracer) intentionally implemented only nil/string cell dispatch and zero recognized option keys, so task 2's TDD RED phase had 9 genuinely failing tests (integer/float/boolean cells, separator/header options) rather than a vacuous pass -- confirmed by running the full behavior-list test file before implementing task 2's dispatch branches."
  - "Two identifier-in-comment slips (mentioning __gc and lua_cell_as in prose while explicitly NOT using either) were caught by the plan's own acceptance-criteria greps (grep -c '__gc' / grep -c 'lua_cell_as' must be 0 / unchanged) and fixed by rewording the comments rather than suppressing the grep -- the gates exist precisely to keep those comments honest."

requirements-completed: [WRITE-01, WRITE-02, WRITE-03, WRITE-04, WRITE-08, FMT-01, FMT-02, FMT-03, FMT-04, FMT-06, FMT-08, FMT-09, LUA-09, LUA-10, LUA-11, TEST-09, DOC-05]

coverage:
  - id: D1
    description: "db:write_csv / w:write_row / w:close spine wired end to end: a Lua script writes rows and reads them back through db:read_csv"
    requirement: "WRITE-01"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.WriteRowThenReadCsvRoundTripsPlainStrings"
        status: pass
    human_judgment: false
  - id: D2
    description: "Cell-type dispatch: string verbatim, int64 via append_number (exact for values past double's mantissa), whole float with no synthetic decimal point, boolean as 1/0, nil as empty cell, table/function/userdata rejected naming write_row and the cell index"
    requirement: "FMT-04"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.IntegerCellRoundTripsExactDigitString"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.WholeFloatAndEqualIntegerProduceSameCellText"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.BooleanCellWritesOneOrZero"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.TableCellThrowsNamingWriteRowAndCellIndex"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.FunctionCellThrowsNamingWriteRowAndCellIndex"
        status: pass
    human_judgment: false
  - id: D3
    description: "Row width from a pairs-based max-integer-key walk (never size()/rawlen): interior hole writes an empty cell, zero integer keys write one quoted empty cell, a non-integer or sub-1 key throws naming write_row"
    requirement: "FMT-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.RowWidthComesFromMaxIntegerKeyNotKeyCount"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.RowWithZeroIntegerKeysWritesOneQuotedEmptyCell"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.NonIntegerRowKeyThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.SubOneIntegerRowKeyThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.SingleColumnFileWithNilAndEmptyCellsRoundTripsEveryRow"
        status: pass
    human_judgment: false
  - id: D4
    description: "separator/header are the only two accepted options, with collect-then-validate decoding, correct defaults (absent/nil/empty-table), and type/length validation"
    requirement: "LUA-09"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.UnknownOptionKeyThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.NonStringSeparatorThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.MultiCharacterSeparatorThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.NonTableHeaderThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.NonStringHeaderEntryThrows"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.AbsentNilAndEmptyOptionsAllMeanDefaults"
        status: pass
    human_judgment: false
  - id: D5
    description: "A non-empty header is WRITTEN (not merely decoded) as the first record, in order, quoted by the same emitter a data row uses"
    requirement: "WRITE-03"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.HeaderIsWrittenAheadOfDataAndQuotedLikeARow"
        status: pass
    human_judgment: false
  - id: D6
    description: "db:write_csv truncates an existing target at open (WRITE-08), asserted at runtime by reopening the same path"
    requirement: "WRITE-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_write_csv.cpp#LuaRunner_WriteCsv.ReopeningSamePathTruncatesExistingContent"
        status: pass
    human_judgment: false
  - id: D7
    description: "LUA_DB_API_REFERENCE carries the full compact DOC-05 CSV file writing section; the lua-api-sync gate passes in the same commit as the binding"
    requirement: "DOC-05"
    verification:
      - kind: unit
        ref: "bindings/js/test/lua-api-sync.test.ts (all 6 tests)"
        status: pass
    human_judgment: false

duration: 50min
completed: 2026-09-16
status: complete
---

# Phase 4 Plan 1: CSV Writer Spine, Cell Dispatch, and DOC-05 Reference Summary

**Hand-rolled `quiver::csv_write::Writer` behind a new `db:write_csv`/`CsvWriter` sol2 usertype, with full string/int64/double/boolean/nil cell dispatch, `separator`/`header` options, and the complete DOC-05 reference section — all proven end to end through `db:read_csv` round trips.**

## Performance

- **Duration:** ~50 min
- **Started:** 2026-09-16 (session start; exact epoch not captured)
- **Completed:** 2026-09-16T21:47:00Z
- **Tasks:** 3 (task 2 executed as RED then GREEN commits per its `tdd="true"` frontmatter)
- **Files modified:** 11 (4 created, 7 modified)

## Accomplishments

- `quiver::csv_write::Writer` (src/csv_write.h/.cpp): a plain (non-Pimpl'd, D-37), RFC-4180 CSV
  writer with configurable separator, optional header, truncate-at-open, idempotent close, and
  quoting rules matching FMT-01/FMT-02/FMT-03/FMT-09 exactly.
- `quiver::utils::append_number` moved to `src/utils/number.h` (D-38), behavior-preserving; both
  pre-existing JSON-encoder call sites and the new CSV cell formatter now share it.
- `db:write_csv` binding + `CsvWriter` sol2 usertype (`sol::no_constructor`, `std::unique_ptr`
  ownership, no explicit finalizer — LUA-11), sandboxed via the unmodified
  `resolve_sandboxed_path` choke point, called before option decoding (LUA-10).
- Full cell-type dispatch in `src/lua_runner.cpp` (nil, boolean, int64, double, string, else
  Pattern 1) and a `pairs`-based max-integer-key row-width walk (FMT-08) — never
  `size()`/`lua_rawlen`.
- `separator`/`header` option decoding (LUA-09: collect-then-validate), with header validated by
  the same integer-key walk and written through the same record emitter a data row uses.
- 20 new GoogleTest cases in `tests/test_lua_runner_write_csv.cpp`, every correctness assertion
  round-tripping through `db:read_csv` — never reading the raw file.
- The full compact-form (D-39) DOC-05 section in `LUA_DB_API_REFERENCE`, with a 10-line worked
  example carrying an interior `nil` cell (never trailing) and both halves of the D-40 nil clause.

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end one-string-row round trip** - `27df018` (feat)
2. **Task 2: Cell type matrix, row walk, options** - `f9d3125` (test, RED) then `610efd7` (feat, GREEN)
3. **Task 3: Full DOC-05 reference + CLAUDE.md clauses** - `46e6a75` (docs)

_Task 2 carried `tdd="true"`; the RED commit contains 20 tests with 9 genuinely failing (confirmed
by running the suite before implementing), the GREEN commit turns all 20 green with no test-file
changes._

## Files Created/Modified

- `src/csv_write.h` - `quiver::csv_write::Options`/`Writer` declarations, non-Pimpl (D-37)
- `src/csv_write.cpp` - Constructor precondition checks, `append_record` (FMT-01/02/03/09), `write_row`/`close`
- `src/utils/number.h` - `quiver::utils::append_number`, moved verbatim from `lua_runner.cpp` (D-38)
- `tests/test_lua_runner_write_csv.cpp` - 20 tests, all round-tripping through `db:read_csv`
- `src/lua_runner.cpp` - `CsvWriter` struct, cell dispatch, row/header walks, options decoder, `db:write_csv` binding + usertype
- `src/CMakeLists.txt` - `csv_write.cpp` added to `QUIVER_SOURCES`
- `tests/CMakeLists.txt` - `test_lua_runner_write_csv.cpp` added to `quiver_tests`
- `bindings/js/src/lua-api.ts` - Full "CSV file writing" DOC-05 section
- `bindings/js/test/lua-api-sync.test.ts` - `"CsvWriter"` added to the usertype coverage array
- `src/CLAUDE.md` - File map entries, csv_write design paragraph, D-34/D-40 clauses, `append_number` relocation note
- `tests/CLAUDE.md` - `test_lua_runner_write_csv.cpp` inventory entry

## Decisions Made

- Implemented WRITE-07 (parent-directory-missing precondition) in the constructor even though it
  isn't in this plan's `requirements` list — the plan's own task 1 action text specifies it
  verbatim, mirroring `csv_read.cpp`'s constructor style; leaving it out would have meant an
  unchecked `std::ofstream::open` failure with no Pattern 1 wrapping.
- Task 1 deliberately implemented only nil/string cell dispatch and recognized zero option keys,
  so task 2's TDD RED phase had real failing tests (9 of 20) rather than a vacuous pass — verified
  by running the extended test file before writing task 2's implementation.
- Two comments briefly mentioned `__gc` and `lua_cell_as` (explaining what the code does NOT do)
  which tripped the plan's own literal-grep acceptance gates (`grep -c '__gc'` must be 0,
  `grep -c 'lua_cell_as'` must stay at baseline 7); reworded both comments to describe the same
  fact without the literal identifier, since the gates exist to catch exactly this kind of
  incidental reintroduction.

## Deviations from Plan

None - plan executed exactly as written. WRITE-07 was implemented per the plan's own task 1 action
text even though absent from the `requirements` frontmatter list (see Decisions above); this is a
plan-internal instruction being followed, not a deviation from it.

## Issues Encountered

- `clang-format --dry-run -Werror` flagged reflow-needed lines in `src/csv_write.cpp`,
  `src/lua_runner.cpp`, and `tests/test_lua_runner_write_csv.cpp` after task 2's implementation
  (mostly long boolean-chain conditions and hand-wrapped embedded Lua string concatenation).
  Resolved by running `clang-format -i` and re-verifying the full test suite plus the JS sync test
  still passed — no behavior change, folded into task 3's commit since it was discovered during
  that task's format-verification pass.
- MSVC reported `C4702: unreachable code` at task 1's checkpoint (the options decoder's stub
  "unknown option" loop, which unconditionally throws when non-empty). Expected transient state:
  resolved naturally once task 2 added real `separator`/`header` branches; confirmed gone in the
  task 2 build.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- **Ready for plan 04-02**: the writer, binding, and usertype are stable extension points.
  04-02 adds the FMT-05 non-finite-number guard (this plan's numeric path is finite-values-only by
  explicit instruction), WRITE-05 (write-after-close / idempotent-close enforcement — `close()` is
  already idempotent structurally, but `write_row` after `close` needs its own throw), and WRITE-06
  (flush-on-`run()`-exit for an unclosed writer).
- **Ready for plan 04-03**: FMT-07 (header-as-width-authority) and executing the DOC-05 worked
  example as a positional-assertion test are both explicitly deferred to that plan; the example's
  interior-nil cell placement is locked with an inline comment specifically so that test can assert
  positionally without the example having drifted.
- No blockers. Full C++ suite (1199 tests) and the JS `lua-api-sync` suite (6 tests) both pass;
  `clang-format --dry-run -Werror` is clean on every file this plan touched.

---
*Phase: 04-a-lua-script-writes-a-csv-file*
*Completed: 2026-09-16*

## Self-Check: PASSED
All created files verified present; all task commit hashes verified in git log.
