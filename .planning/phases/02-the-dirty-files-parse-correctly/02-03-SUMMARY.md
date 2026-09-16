---
phase: 02-the-dirty-files-parse-correctly
plan: 03
subsystem: testing
tags: [lua, csv-parser, fixtures, gitattributes, TEST-02, real-file-regression]

requires:
  - phase: 02-the-dirty-files-parse-correctly (plan 01)
    provides: "csv_read::Options.header_row and read_csv_options_from_lua's header_row decoder"
provides:
  - "tests/fixtures/ma_energia_residencial.csv and ma_gd_data.csv -- the two real Maranhao CSVs, committed byte-exact (BOM + CRLF preserved), replacing the hand-transcribed script.lua"
  - "A .gitattributes exemption (tests/fixtures/*.csv -text) proving committed fixture bytes survive git's line-ending normalization"
  - "Two end-to-end regression tests (TEST-02) proving a Lua script reading each real file, then transforming in the script, reproduces script.lua's hard-coded final values"
affects:
  - "tests/fixtures/ (new directory)"
  - "tests/test_lua_runner_read_csv.cpp"
  - ".gitattributes"

actuals:
  tokens: 4685
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "R\"LUA(...)LUA\" custom raw-string delimiter (existing house pattern from test_lua_runner_describe.cpp/test_lua_runner_errors.cpp) is required whenever an embedded Lua pattern literal itself ends in the two-character sequence )\" -- with the default R\"(...)\" delimiter that exact sequence terminates the C++ raw string early, silently truncating the script and turning the remainder into ill-formed C++ tokens."

key-files:
  created:
    - tests/fixtures/ma_energia_residencial.csv
    - tests/fixtures/ma_gd_data.csv
  modified:
    - .gitattributes
    - tests/test_lua_runner_read_csv.cpp

key-decisions:
  - "The .gitattributes exemption (tests/fixtures/*.csv -text) was committed in its own commit BEFORE either fixture was staged, per D-24's explicit ordering requirement -- an exemption added in the same commit as the files, or after, would not retroactively un-normalize an already-normalized index entry."
  - "Verification was performed against the committed blob (git show :<path> / git show HEAD:<path>), never the working-tree file -- the working-tree copy would look correct on Windows even if the index entry had been silently normalized, which is exactly the failure mode D-24 warns is invisible."
  - "Both real-file tests use the R\"LUA(...)LUA\" delimiter instead of the file's usual bare R\"(...)\" because their date-pattern match() literals (\"(%d%d)/(%d%d)/(%d%d%d%d)\" and \"(%a+) (%d+), (%d+)\") each end in the exact )\" sequence that would otherwise terminate the raw string early -- discovered empirically when the first attempt produced a cascade of C2146/C2065 compile errors starting mid-script."

requirements-completed: [TEST-02]

coverage:
  - id: D1
    description: "The two real Maranhao CSVs are committed as byte-exact fixtures under tests/fixtures/, with a .gitattributes exemption staged and committed before either file, verified against the committed git blob (not the working tree) for BOM presence, CRLF byte pairs, and exact byte counts"
    requirement: "TEST-02"
    verification:
      - kind: unit
        ref: "git show HEAD:tests/fixtures/ma_energia_residencial.csv (BOM efbbbf, 261 CRLF pairs, 9400 bytes) and HEAD:tests/fixtures/ma_gd_data.csv (0 CRLF pairs, 2643 bytes)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A Lua script reading ma_energia_residencial.csv with header_row=2, skipping the units row and transforming DD/MM/YYYY dates and apostrophe-thousands-separator values, reproduces script.lua's hard-coded 2005-01 93943 and 2023-07 386433, and the header is verified to come from line 2 (not the junk title row)"
    requirement: "TEST-02"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EnergiaRegressionJunkRowAboveUnitsRowBelowHeader"
        status: pass
    human_judgment: false
  - id: D3
    description: "A Lua script reading ma_gd_data.csv with no header_row option (D-20's default), transforming English month names via a hand-rolled lookup table, reproduces script.lua's hard-coded 2014-05 33 and 2021-07 51818.33, and every row is verified to have exactly two fields despite the quoted comma in the date column"
    requirement: "TEST-02"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.GdRegressionQuotedCommaAndEnglishMonthNames"
        status: pass
    human_judgment: false

duration: ~35min
completed: 2026-09-16
status: complete
---

# Phase 2 Plan 3: Real-file fixtures and TEST-02 regression Summary

Commits the two real Maranhão CSVs as byte-exact git fixtures (surviving this machine's
`core.autocrlf=true` normalization hazard) and proves, with two end-to-end Lua tests, that reading
and transforming them in-script reproduces the values the hand-transcribed `script.lua` hard-coded
— the phase's actual claim that the file can replace the transcription (D-23).

## Performance

- **Duration:** ~35 min
- **Tasks:** 3
- **Files modified:** 4 (`.gitattributes`, 2 new fixtures, `tests/test_lua_runner_read_csv.cpp`)

## Accomplishments

- **Task 1 — byte-exact fixtures, gitattributes hazard avoided:** Added
  `tests/fixtures/*.csv -text` to `.gitattributes` and committed it alone, before staging either
  fixture (D-24's required ordering). Copied both source CSVs binary (`cp -p`, no editor, no unix
  text tool) into `tests/fixtures/` under their ASCII names. Verified the **committed git blob**
  (`git show :<path>` before commit, `git show HEAD:<path>` after) — not the working-tree file,
  which would look correct on Windows regardless — carries the Energia file's UTF-8 BOM (`ef bb
  bf`) and all 261 CRLF byte pairs, and that the GD file carries zero CRLF pairs. Byte counts
  match the sources exactly (9400 and 2643 bytes). `git check-attr -a` confirmed `text: unset` on
  both fixtures before they were staged.
- **Task 2 — Energia regression (TEST-02):** `EnergiaRegressionJunkRowAboveUnitsRowBelowHeader`
  copies the committed fixture into the `LuaSandboxTest` sandbox (`db:read_csv` resolves relative
  paths against the database directory, not the source tree), reads it with `{ header_row = 2 }`,
  asserts the header came from line 2 (`ANO`/`Residencial`) and not the junk title row, skips
  `rows[1]` (the units row, D-22), transforms each remaining row's `DD/MM/YYYY` date and
  apostrophe-padded value (`tonumber((row[6]:gsub("['%s]", "")))` — the double-parens gsub trap,
  D-23), and asserts the exact targets `2005-01 93943` (first data row) and `2023-07 386433`
  (`rows[224]`, corresponding to line 226 of the file).
- **Task 3 — GD regression (TEST-02):** `GdRegressionQuotedCommaAndEnglishMonthNames` copies the
  GD fixture into the sandbox, reads it with **no options table** (proving D-20's default
  `header_row = 1` survived plan 02-01's change), asserts every row has exactly two fields despite
  the quoted comma inside the date column (PARSE-02 in production form — a quoting bug here would
  silently shift the value column), transforms each row's English month name via a hand-rolled
  12-entry lookup table (the Lua sandbox has `os` unloaded, so no date library is available —
  LUA-07 script-side work by design), and asserts the exact targets `2014-05 33` (first data row)
  and `2021-07 51818.33` (`rows[71]`, line 72).

## Task Commits

Each task was committed atomically:

1. **Task 1a: `.gitattributes` exemption, committed first** — `624cd2b` (chore)
2. **Task 1b: the two byte-exact fixtures** — `8eb8b48` (test)
3. **Task 2: Energia regression** — `b09d352` (test)
4. **Task 3: GD regression** — `0fbf8f8` (test)

**Plan metadata:** committed alongside this SUMMARY (see below)

_No TDD RED/GREEN split: both regression tests exercise existing reader behavior (the
`header_row` option shipped in plan 02-01) with new Lua-side transformation logic; the "RED" phase
was catching and fixing the raw-string delimiter collision below before either test could compile
at all, which is documented as a deviation rather than a formal RED commit since no production
code was touched._

## Files Created/Modified

- `.gitattributes` — added the `tests/fixtures/*.csv -text` exemption with an explanatory comment.
- `tests/fixtures/ma_energia_residencial.csv` (new) — real Energia file, byte-identical to source.
- `tests/fixtures/ma_gd_data.csv` (new) — real GD file, byte-identical to source.
- `tests/test_lua_runner_read_csv.cpp` — 2 new `TEST_F` cases (66 total, up from 64).

## Decisions Made

- Committed the `.gitattributes` change as its own commit before touching either fixture, exactly
  as D-24 requires — verified this was necessary by confirming `git check-attr -a` reported
  `text: unset` only after the attribute commit landed.
- Verified fixture bytes against the git-internal representation (`git show :<path>` for the
  index, `git show HEAD:<path>` for the committed tree) at every checkpoint, never the
  working-tree copy — the working tree is exactly the view that would still look correct if the
  normalization hazard had silently fired.
- Used the `R"LUA(...)LUA"` custom delimiter (already established in
  `test_lua_runner_describe.cpp` and `test_lua_runner_errors.cpp`) for both new tests' Lua scripts,
  once the default `R"(...)"` delimiter was found to collide with the embedded regex-like Lua
  pattern literals.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Raw-string delimiter collision truncated both scripts silently at compile time**
- **Found during:** Task 2 (first build attempt)
- **Issue:** The Energia script's `row[5]:match("(%d%d)/(%d%d)/(%d%d%d%d)")` literal ends in the
  exact two-character sequence `)"`, which is also the terminator for the enclosing default-
  delimited `R"( ... )"` raw string. The C++ raw string ended mid-script; everything after that
  point was parsed as ordinary (ill-formed) C++ tokens, producing a cascade of unrelated-looking
  `C2146`/`C2065`/`C2504` errors starting several lines later, not at the actual point of failure.
  The GD script's `row[1]:match("(%a+) (%d+), (%d+)")` literal has the identical collision.
  (A clang-format pass surfaced the same root cause first, silently reformatting the "leaked" C++
  tokens as if they were real code — a symptom of the same premature termination, not a separate
  clang-format bug.)
- **Fix:** Switched both scripts to the `R"LUA(...)LUA"` delimiter already used elsewhere in this
  test suite (`test_lua_runner_describe.cpp`, `test_lua_runner_errors.cpp`), which cannot collide
  with any Lua-side `)"` substring.
- **Files modified:** `tests/test_lua_runner_read_csv.cpp`
- **Commits:** b09d352, 0fbf8f8 (fixed before either commit landed — no separate fix commit needed)

No other deviations. Both real-file transformation targets matched on the first script run once
the delimiter was corrected — the column indices and transformation logic from 02-RESEARCH.md
were exact.

## Issues Encountered

- The raw-string delimiter collision above (Rule 1, caught and fixed before committing).
- No other issues. The source directory named in the precondition existed and contained both
  files; no halt was needed.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- TEST-02 is fully discharged: both real Maranhão files are committed as byte-exact fixtures and
  proven, end-to-end through the Lua boundary, to reproduce the transcribed script's values.
- TEST-05 (Release-build parity) remains for plan 02-04, along with DOC-02/DOC-03 (the
  agent-reference rewrite, out of this plan's scope).
- `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv.*'` — 66/66 pass.
- `./build/bin/quiver_tests.exe` (full suite) — 1177/1177 pass.
- `./build/bin/quiver_c_tests.exe` (full C API suite) — 557/557 pass.
- `clang-format --dry-run --Werror` on the touched C++ file — clean.

## Known Stubs

None. No stub patterns, placeholder values, or unwired data paths were introduced.

## Threat Flags

None beyond the phase's own registered threat (T-02-06, committed fixture tampering via git
line-ending normalization), which this plan's Task 1 discharges exactly as specified: the
`.gitattributes` exemption staged and committed before the fixtures, verified against the
committed blob rather than the working tree.

## Self-Check: PASSED

- `.gitattributes` — FOUND, contains `tests/fixtures/*.csv -text`.
- `tests/fixtures/ma_energia_residencial.csv` — FOUND, `git show HEAD:` confirms BOM + 261 CRLF
  pairs + 9400 bytes.
- `tests/fixtures/ma_gd_data.csv` — FOUND, `git show HEAD:` confirms 0 CRLF pairs + 2643 bytes.
- `tests/test_lua_runner_read_csv.cpp` — FOUND, contains both new `TEST_F` names.
- Commit 624cd2b — FOUND in `git log`.
- Commit 8eb8b48 — FOUND in `git log`.
- Commit b09d352 — FOUND in `git log`.
- Commit 0fbf8f8 — FOUND in `git log`.
