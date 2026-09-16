---
phase: 02-the-dirty-files-parse-correctly
verified: 2026-09-16T07:45:00Z
status: passed
score: 12/12 must-haves verified
behavior_unverified: 0
overrides_applied: 0
---

# Phase 2: The dirty files parse correctly — Verification Report

**Phase Goal:** The messy, arbitrary CSVs that arrive from real sources read correctly — including
the two real Maranhão files currently transcribed into scripts by hand.
**Verified:** 2026-09-16
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `"May 1, 2014",33` is two fields; a quoted newline does not split the record; a doubled quote unescapes to one | ✓ VERIFIED | `LuaRunner_ReadCsv.DirtyFileParsesEveryParserRequirement`, `GdRegressionQuotedCommaAndEnglishMonthNames` — ran directly, pass |
| 2 | A UTF-8 BOM + CRLF file yields a clean first header/cell; no `\r` on any cell; ragged rows parse without throwing or truncating | ✓ VERIFIED | `DirtyFileParsesEveryParserRequirement`, `LfAndCrlfEndingsParseIdentically`, `BomStrippedUnderExplicitHeaderRowAndNoHeader` — pass; real Energia fixture blob confirmed BOM present (`efbbbf`) + 261 CRLF pairs via `git show HEAD:...` |
| 3 | A script names the header row (or declares none), so junk rows around the header read correctly (real file: 1 junk row above, 1 units row below — D-22's corrected reading of this criterion) | ✓ VERIFIED | `header_row` option implemented (`src/csv_read.h/.cpp`, `src/lua_runner.cpp`); `EnergiaRegressionJunkRowAboveUnitsRowBelowHeader` reads the real file with `header_row=2`, asserts header came from line 2, skips the units row in-script — pass |
| 4 | A header where `ANO`/`Residencial` repeat and several names are blank leaves every column reachable, none shadowing another | ✓ VERIFIED | `RepeatedAndBlankHeaderNamesAllReachable` against the real 11-field Maranhão header — pass. Satisfied by design (positional array, no name→map) per D-21, not by new code — correctly not "fixed" |
| 5 | Reading the two real Maranhão CSVs yields the same values the hand-transcribed script hard-coded | ✓ VERIFIED | `EnergiaRegressionJunkRowAboveUnitsRowBelowHeader` → `2005-01 93943`, `2023-07 386433`; `GdRegressionQuotedCommaAndEnglishMonthNames` → `2014-05 33`, `2021-07 51818.33` — both ran directly and pass; both transform raw cells in Lua (regex date parsing, `gsub`+`tonumber` with the double-paren trap, month-name lookup table), not pre-baked constants; fixtures are the real files (byte-verified against git blob, not working tree) |

**Score:** 5/5 phase-goal truths verified (12/12 requirement-level must-haves — see below)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/csv_read.h` | `Options.header_row` field | ✓ VERIFIED | `int64_t header_row = 1;` present, documented |
| `src/csv_read.cpp` | `make_format` header-mode-before-`variable_columns` ordering; past-EOF synthesis | ✓ VERIFIED | Read directly — order is header-mode first, `variable_columns(KEEP_NON_EMPTY)` last; past-EOF check gated on `options.header_row != 0 && header.empty()`, outside the narrowed `try` |
| `src/lua_runner.cpp` | `read_csv_options_from_lua` accepts/validates `header_row`; stream `header` arg honors nil-when-absent | ✓ VERIFIED | Both present; the nil-sentinel fix (CR-01, commit `dc5ab7d`) confirmed in current source |
| `bindings/js/src/lua-api.ts` | documents `header_row` | ✓ VERIFIED | JS sync test (6/6) passes |
| `tests/fixtures/ma_energia_residencial.csv`, `ma_gd_data.csv` | real files, byte-exact | ✓ VERIFIED | `git show HEAD:...` confirms BOM + 261 CRLF pairs (Energia), 0 CRLF pairs (GD); `.gitattributes` carries `tests/fixtures/*.csv -text` |
| `tests/test_lua_runner_read_csv.cpp` | full fixture/regression/negative matrix | ✓ VERIFIED | 68 tests in suite, all pass |
| `src/CLAUDE.md`, `tests/CLAUDE.md`, `CHANGELOG.md` | DOC-04 paperwork | ✓ VERIFIED | All three grep-confirmed and read in full; accurate to the implementation |

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `Options.header_row` (1-based) | `csv::CSVFormat` (0-based) | `make_format` branch + clamp | ✓ WIRED | Confirmed by reading `csv_read.cpp` directly |
| `read_csv_options_from_lua` | both `db:read_csv` and `db:read_csv_stream` | shared decoder | ✓ WIRED | Single decoder function, both entry points route through it (LUA-03 preserved even in the stream `header` nil fix) |
| Real fixture bytes (git blob) | test assertions | `std::filesystem::copy_file` into sandbox | ✓ WIRED | Verified blob bytes independently of working tree; tests copy from `path_from(__FILE__, "fixtures/...")` into sandbox before reading |

### Behavioral Spot-Checks / Test Execution

Ran directly rather than trusting SUMMARY claims:

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full C++ core suite | `./build/bin/quiver_tests.exe` | 1179/1179 pass | ✓ PASS |
| Full C API suite | `./build/bin/quiver_c_tests.exe` | 557/557 pass | ✓ PASS |
| CSV read suite | `--gtest_filter='LuaRunner_ReadCsv*'` | 68/68 pass | ✓ PASS |
| Energia + GD regressions | `--gtest_filter='*Energia*:*Gd*'` | 2/2 pass | ✓ PASS |
| JS lua-api sync | `bun test test/lua-api-sync.test.ts` | 6/6 pass | ✓ PASS |
| **Release build, independently reconfigured** (not trusting the deleted `build-release` from 02-04-SUMMARY) | `cmake -S . -B build-release-verify -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON` then `--gtest_filter='LuaRunner*'` | 293/293 pass | ✓ PASS |

### Code Review Findings (02-REVIEW.md) — Disposition

| ID | Finding | Status |
|----|---------|--------|
| CR-01 (critical) | `db:read_csv_stream`'s `header` callback arg was an empty truthy table instead of nil under `header_row=0`, diverging from `db:read_csv` | ✓ FIXED post-review, commit `dc5ab7d` — verified in current `src/lua_runner.cpp` and covered by `StreamHeaderIsNilWhenWholeFileHeaderIsAbsent`, which passed in the run above |
| WR-01 (warning) | INT_MAX clamp had no regression test | ✓ ADDRESSED, commit `45df03a` — test added, with an honest comment explaining the clamp itself is unobservable from Lua (past-EOF fires first either way) rather than papering over it |
| WR-02 (warning) | `Options.header_row` negative-value contract enforced only by the Lua decoder, not the type itself | Accepted as-is — single-caller topology, consistent with project's "assume callers obey contracts" philosophy; explicitly optional in the review |
| IN-01 (info) | Minor duplication in `Reader`'s three precondition checks | Not required — explicitly against "clean over defensive" project philosophy |

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| PARSE-02 | 02-02 | ✓ SATISFIED | `DirtyFileParsesEveryParserRequirement` |
| PARSE-03 | 02-02 | ✓ SATISFIED | same test |
| PARSE-04 | 02-02 | ✓ SATISFIED | same test |
| PARSE-05 | 02-02 | ✓ SATISFIED | same test + `BomStrippedUnderExplicitHeaderRowAndNoHeader` |
| PARSE-06 | 02-02 | ✓ SATISFIED | same test + `LfAndCrlfEndingsParseIdentically` |
| PARSE-07 | 02-02 | ✓ SATISFIED | same test |
| LUA-05 | 02-01 | ✓ SATISFIED | `header_row` option, end-to-end |
| LUA-06 | 02-02 | ✓ SATISFIED | `RepeatedAndBlankHeaderNamesAllReachable` (satisfied by design, D-21, no code added — correctly not "fixed") |
| TEST-01 | 02-02 | ✓ SATISFIED | full dirty-file matrix |
| TEST-02 | 02-03 | ✓ SATISFIED | both real-file regressions, byte-verified fixtures |
| TEST-04 | 02-02 | ✓ SATISFIED | 4 decoder negatives + past-EOF negative (02-01), all genuine throws (`expect_lua_error`/`FAIL()` pattern, not vacuous) |
| TEST-05 | 02-04 (+ independently reverified here) | ✓ SATISFIED | 293/293 `LuaRunner*` pass in a from-scratch Release build with `QUIVER_BUILD_TESTS=ON` explicitly set (not the `release` preset, which is confirmed in `CMakePresets.json` to set `QUIVER_BUILD_TESTS: OFF`) |

No orphaned requirements: the 12 requirement IDs declared across the four plans exactly match the phase's traceability set (PARSE-02..07, LUA-05, LUA-06, TEST-01, TEST-02, TEST-04, TEST-05).

### Anti-Patterns Found

None. `grep -E "TBD|FIXME|XXX|TODO|HACK|PLACEHOLDER"` across every file touched by this phase
(`src/csv_read.h`, `src/csv_read.cpp`, `src/lua_runner.cpp`, `bindings/js/src/lua-api.ts`,
`tests/test_lua_runner_read_csv.cpp`) returned nothing. No debt markers, no stubs, no hardcoded
empty-return patterns in the diff.

### Post-Plan Fixes Verified

Two commits landed after plan 02-04 closed, both self-declared in the task brief and both checked
against the actual diff rather than trusted from commit messages:

- `dc5ab7d` — genuine bug (CR-01), genuine fix, genuine regression test (`StreamHeaderIsNilWhenWholeFileHeaderIsAbsent`), confirmed present and passing.
- `45df03a` — genuine regression test for the `make_format` call-order fix and huge `header_row` values, confirmed present and passing. The commit message is honest about what it does *not* prove (the INT_MAX clamp itself is unreachable from the Lua surface — the past-EOF guard fires first regardless) rather than overclaiming coverage.

### Human Verification Required

None. Every must-have above is closed by an executable test that was independently run during this
verification, not merely cited from a SUMMARY.

### Gaps Summary

None found. Phase goal achieved: the parser handles quoted separators, embedded newlines, doubled
quotes, BOM, CRLF, ragged rows, and adversarial (duplicate/blank) headers — all proven through the
Lua boundary — and the two real Maranhão files, committed byte-exact, replace their hand-transcribed
script with equivalent in-script transformations reaching the identical final values. The one
critical defect a code review found after plan closure (CR-01) was fixed and tested before this
verification ran, and the fix is confirmed genuine, not merely claimed.

---

_Verified: 2026-09-16_
_Verifier: Claude (gsd-verifier)_
