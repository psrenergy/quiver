---
phase: 01-a-lua-script-reads-a-csv-file
plan: 03
subsystem: database
tags: [lua, sol2, csv-parser, sandbox, error-handling, documentation]

requires:
  - phase: 01-01
    provides: "quiver::csv_read::Reader, db:read_csv/db:read_csv_stream bindings, LuaSandboxTest fixture"
  - phase: 01-02
    provides: "read_csv_options_from_lua shared decoder, the separator option, D-22 evaluation-order fix (sandbox before options)"
provides:
  - "The five TEST-03 sandbox negatives (escape, in-memory, missing file, directory-as-path) plus the subdirectory positive control, asserted for both db:read_csv and db:read_csv_stream with full message pinning"
  - "The complete D-22 error-catalogue proof: ordering (in-memory before options, escape before missing-file), adjacency (unknown-key before bad-separator-value), the empty-file check extended to the stream form, and a blanket assertion that every negative in the suite starts with its own entry point's Cannot prefix"
  - "src/CLAUDE.md, root CLAUDE.md, tests/CLAUDE.md and CHANGELOG.md updated to describe the new Lua-only CSV surface"
affects: []

actuals:
  tokens: 5500
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Source-level fallback assertion for a D-22 catalogue entry with no reliably reproducible runtime trigger on Windows (the csv-parser wrapper), rather than silently dropping the requirement"

key-files:
  created:
    - .planning/phases/01-a-lua-script-reads-a-csv-file/01-03-SUMMARY.md
  modified:
    - tests/test_lua_runner_read_csv.cpp
    - src/CLAUDE.md
    - CLAUDE.md
    - tests/CLAUDE.md
    - CHANGELOG.md

key-decisions:
  - "D-22 entry 10 (the csv-parser wrapper's 'cannot read file' message) has no portable runtime trigger in this environment: every other failure mode (not-found/directory/empty) is intercepted inside Reader's constructor before csv::CSVReader is ever built, and std::filesystem::permissions has no effect on read access on Windows. Per the plan's own sanctioned fallback, a dedicated test asserts the wrapper text is present at the source level (reads src/csv_read.cpp at test time) instead of silently dropping the requirement."
  - "expect_prefixed_error strips the root Pattern 3 'Failed to run Lua script: ' envelope before checking the Pattern 1 prefix, since lua.run() wraps every script-level throw in that envelope (src/CLAUDE.md's own documented behavior) -- a plain rfind against the raw message would have failed on every case."
  - "The unknown-key-before-bad-separator-value adjacency (D-22 #4 before #5) holds structurally regardless of Lua's for_each iteration order, because read_csv_options_from_lua's unknown-key check fires inside the entries loop (throws immediately on the first non-separator key) while the separator-type/length checks run only after that loop completes -- so a table order that visits 'separator' first still can't out-race the unknown-key throw on any other key."

patterns-established: []

requirements-completed: [TEST-03, LUA-04, LUA-08]

coverage:
  - id: D1
    description: "The five sandbox negatives (relative escape, absolute escape, in-memory db, missing file, directory-as-path) are asserted for both db:read_csv and db:read_csv_stream, each pinning the full Cannot <op>: ... message rather than a lone keyword"
    requirement: "TEST-03"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EscapingPathThrowsForReadCsv"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EscapingPathThrowsForReadCsvStream"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.InMemoryDatabaseThrowsForReadCsv"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.InMemoryDatabaseThrowsForReadCsvStream"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.MissingFileThrowsForReadCsv"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.MissingFileThrowsForReadCsvStream"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.DirectoryAsPathThrowsForReadCsv"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.DirectoryAsPathThrowsForReadCsvStream"
        status: pass
    human_judgment: false
  - id: D2
    description: "The positive control: a CSV inside a subdirectory of the database directory reads successfully through both entry points, with matching rows -- without this, the negatives above would pass equally well against an implementation that rejects every path"
    requirement: "TEST-03"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.SubdirectoryPathReadsSuccessfullyForBothEntryPoints"
        status: pass
    human_judgment: false
  - id: D3
    description: "The empty-file check (already proven for db:read_csv in plan 01) is extended to db:read_csv_stream so both forms agree rather than one throwing and the other reporting zero rows"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EmptyFileThrowsForReadCsvStream"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-22's evaluation order holds under two simultaneous failures: an in-memory database with a positionally-passed separator reports the in-memory error (not the options error), and an escaping-and-missing path reports the escape error (not file-not-found)"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.InMemoryDatabaseReportsBeforeBadOptions"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EscapingPathReportsBeforeMissingFile"
        status: pass
    human_judgment: false
  - id: D5
    description: "An options table with both an unknown key and a non-string separator reports the unknown-key error, never the separator-type error (D-22 #4 before #5)"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.UnknownKeyReportsBeforeBadSeparatorValue"
        status: pass
    human_judgment: false
  - id: D6
    description: "Every negative case in the suite -- both entry points, every D-22 rejection -- raises a message beginning with the calling entry point's own Cannot read_csv: / Cannot read_csv_stream: prefix, so a future leak of a raw csv-parser/filesystem/sol2 message would fail here"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EveryNegativeCaseStartsWithItsOwnEntryPointPrefix"
        status: pass
    human_judgment: false
  - id: D7
    description: "D-22 entry 10 (the csv-parser wrapper 'cannot read file' message) has no portable runtime trigger on this platform -- every other failure mode is intercepted before csv::CSVReader is ever constructed -- so the wrapper's presence is proven at the source level instead of silently dropping the requirement"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.ParserWrapperMessageExistsInSource"
        status: pass
    human_judgment: true
    rationale: "The message text itself is proven present in src/csv_read.cpp, but the actual runtime firing of that catch block (an existing, non-empty, non-directory file that still fails to open) was not exercised in this session -- Windows permission bits don't block read access for the owner, and no other portable trigger was found. A human should decide whether a CI job on a different OS/permission model is worth adding later, or whether the construction-time and mid-iteration try/catch (both routing through the same message format, verified by code inspection) are sufficient assurance."
  - id: D8
    description: "src/CLAUDE.md, root CLAUDE.md, tests/CLAUDE.md and CHANGELOG.md each describe the new Lua-only db:read_csv/db:read_csv_stream surface, its sandboxing, and the pinned CSVFormat rationale, with no manifest version bumped and no new CHANGELOG version heading added"
    verification:
      - kind: other
        ref: "grep -c 'csv_read' src/CLAUDE.md (3), grep -c 'db:read_csv_stream' src/CLAUDE.md (2), grep -c 'db:read_csv_stream' CLAUDE.md (2) and grep -c 'db:read_csv\\b' CLAUDE.md (2), grep -c 'test_lua_runner_read_csv' tests/CLAUDE.md (1), grep -c 'read_csv' CHANGELOG.md (2); git diff shows no new '## [' heading and no manifest file touched"
        status: pass
    human_judgment: false

duration: 45min
completed: 2026-09-15
status: complete
---

# Phase 01 Plan 03: The sandbox and error catalogue proven end to end Summary

**The five sandbox negatives and the complete D-22 error catalogue are now proven for both `db:read_csv` and `db:read_csv_stream`, and the new Lua-only CSV surface is recorded in every `CLAUDE.md` this repo's own rules require plus `CHANGELOG.md`.**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-09-15T12:05:00-03:00 (approx.)
- **Completed:** 2026-09-15T12:49:21-03:00
- **Tasks:** 2
- **Files modified:** 5 (1 test file, 3 `CLAUDE.md` files, 1 `CHANGELOG.md`)

## Accomplishments

- All five TEST-03 sandbox negatives (relative-path escape, absolute-path escape, in-memory
  database, missing file, directory-as-path) are asserted for **both** `db:read_csv` and
  `db:read_csv_stream`, each pinning the full `Cannot <op>: ...` message rather than a lone
  keyword — the TEST-03 prohibition this plan exists to satisfy.
- The subdirectory positive control reads successfully through both entry points with matching
  rows, so the four sandbox negatives above cannot be passing vacuously against an implementation
  that rejects every path.
- The empty-file check, previously only proven for `db:read_csv`, is now also proven for
  `db:read_csv_stream`.
- D-22's evaluation order and adjacency are proven directly: an in-memory database with a bad
  options table reports the in-memory error; an escaping-and-missing path reports the escape
  error; an options table with both an unknown key and a bad separator value reports the
  unknown-key error.
- A blanket assertion runs every negative case in the suite (both entry points, every D-22
  rejection, including the in-memory cases) through a helper that strips the root Pattern 3
  wrapper and checks the Pattern 1 prefix — so a future leak of a raw csv-parser, filesystem, or
  sol2 message would fail this test instead of reaching a script.
- D-22 entry 10 (the csv-parser wrapper) has no portable runtime trigger on this platform;
  documented as an assumption for human review rather than silently dropped, with a source-level
  test proving the wrapper text exists in `src/csv_read.cpp`.
- `src/CLAUDE.md` records `csv_read.h`/`csv_read.cpp` as the first internal `.cpp` in `src/` with
  no `include/quiver/` counterpart, why it stays internal, and the three pinned `CSVFormat`
  settings. Root `CLAUDE.md` records the reader as the newest documented per-binding omission
  (Lua-only, no counterpart anywhere) and adds both names to the sandbox decision's operation
  list. `tests/CLAUDE.md` adds a suite-inventory row. `CHANGELOG.md` carries a new `### Added`
  entry under the existing unreleased heading — no manifest version bumped, no new heading added.
- Full verification: `quiver_tests.exe` (1157/1157), `quiver_c_tests.exe` (557/557), and
  `scripts/test-all.bat`'s full cross-binding sweep (C++, C API, Julia, Dart, JavaScript, Python,
  CLI smoke) all green.

## Task Commits

Each task was committed atomically:

1. **Task 1: The sandbox holds and every documented error is the one that fires** — `85b1ade` (test)
2. **Task 2: The paperwork the house rules owe this change** — `e62903c` (docs)

## Files Created/Modified

- `tests/test_lua_runner_read_csv.cpp` — 24 new tests: 8 sandbox negatives (4 cases × 2 entry
  points), 1 subdirectory positive control, 1 empty-file-for-stream extension, 1 source-level
  parser-wrapper fallback, 2 catalogue-ordering cases, 1 adjacency case, 1 blanket message-prefix
  assertion over 17 negative scripts (both file-backed and in-memory); plus the `lp()` path-escape
  helper and `expect_prefixed_error()` helper in the file's anonymous namespace
- `src/CLAUDE.md` — File Map row for `csv_read.h`/`csv_read.cpp` with the "first internal `.cpp`
  with no public header" note and the three pinned `CSVFormat` settings; both new operation names
  added to the LuaRunner filesystem-sandbox bullet
- `CLAUDE.md` (root) — both names added to the Lua sandbox design decision's operation list; new
  Design Decisions entry recording the CSV reader as Lua-only with no counterpart anywhere
- `tests/CLAUDE.md` — suite-inventory row for `test_lua_runner_read_csv.cpp`, noting it has no
  sibling elsewhere and its fixtures are written at runtime, not committed
- `CHANGELOG.md` — `### Added` entry describing `db:read_csv`/`db:read_csv_stream` from the
  caller's side, placed above the previously-first entry in the existing unreleased section

## Decisions Made

- **D-22 entry 10's runtime path is not exercised** (documented as an assumption in the SUMMARY's
  coverage block, `human_judgment: true`): every other Phase-1 failure mode is intercepted inside
  `Reader`'s constructor (not-found/directory/empty) before `csv::CSVReader` is ever built, and
  Windows' `std::filesystem::permissions` has no effect on read access for the file owner, so
  there is no portable way in this environment to make an existing, non-empty, non-directory file
  fail to open. Per the plan's own sanctioned fallback, a dedicated test
  (`ParserWrapperMessageExistsInSource`) reads `src/csv_read.cpp` at test time and asserts the
  `"cannot read file '"` wrapper text is present, rather than silently dropping the requirement.
- **`expect_prefixed_error` strips the `"Failed to run Lua script: "` envelope** before checking
  the Pattern 1 prefix. `lua.run()` wraps every script-level throw in that root Pattern 3 message
  (documented in `src/CLAUDE.md`'s LuaRunner section), so a raw `rfind` against the unwrapped
  message would have failed on every one of the 17 negative cases in the blanket test — caught
  immediately on first run and fixed inline (not a deviation needing its own entry below, since
  it was caught before any commit).
- **The unknown-key-before-bad-separator adjacency is structural, not order-dependent.**
  `read_csv_options_from_lua` throws on the first non-`"separator"` key inside its entries loop,
  before the separator value is ever type/length-checked (that check runs only after the loop
  completes) — so regardless of which order Lua's `for_each` visits `{ delim = ";", separator =
  59 }`, the unknown-key throw always wins. Verified directly by
  `UnknownKeyReportsBeforeBadSeparatorValue` rather than assumed from reading the code.
- **The LUA-03 "cannot diverge on any input" assumption flagged in the plan's frontmatter is left
  unresolved, as the plan itself directs.** The plan's structural answer (both entry points share
  one `csv_read::Reader`, one `CSVFormat`, one loop) is unchanged by this plan; whether that
  structural guarantee is the whole of what "cannot diverge" means for genuinely dirty input is
  explicitly deferred to Phase 2's dirty-input matrix, per the plan's own note. No new probing was
  attempted here — it was out of this plan's scope (TEST-03/LUA-04/LUA-08 only).

## Deviations from Plan

None — plan executed as written. The `expect_prefixed_error` envelope-stripping fix (noted above
under Decisions Made) was corrected during initial test authoring, before any task commit, and
does not meet the bar for a Rule 1-4 deviation (no committed code was ever wrong).

## Issues Encountered

None beyond the parser-wrapper runtime-reproducibility limitation documented above, which the
plan explicitly anticipated and sanctioned a fallback for.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 1 (`01-a-lua-script-reads-a-csv-file`) is now fully executed: all three plans (tracer,
  separator option, sandbox/error-catalogue proof + paperwork) are committed and verified.
- Phase 2 (dirty-input handling: BOM, CRLF, quoted separators/newlines, junk header rows,
  duplicate/blank header names) can build directly on `csv_read::Reader` and its `Options` struct
  without touching the sandbox, the options decoder's shape, or the two Lua entry points' argument
  order — only new `Options` keys and `make_format` behavior are in scope there.
- The flagged LUA-03 assumption (whether "cannot diverge on any input" holds beyond the structural
  guarantee) is exactly what Phase 2's dirty-input matrix will exercise for the first time on
  genuinely adversarial input; it is not re-flagged as a blocker here, just carried forward.
- No blockers.

## Self-Check: PASSED

All modified files verified present on disk; both task commit hashes (`85b1ade`, `e62903c`)
verified present in `git log --oneline --all`.

---
*Phase: 01-a-lua-script-reads-a-csv-file*
*Completed: 2026-09-15*
