---
phase: 04-a-lua-script-writes-a-csv-file
verified: 2026-09-16T23:05:14Z
status: passed
score: 5/5 success criteria verified (51/51 must-have truths across 04-01/04-02/04-03 plans hold)
behavior_unverified: 0
overrides_applied: 0
---

# Phase 4: A Lua script writes a CSV file — Verification Report

**Phase Goal:** A Lua script can create a CSV file in the database directory and write rows to it
— strings, numbers, booleans and `nil` cells — and `db:read_csv` over the same path returns
exactly what the script wrote

**Verified:** 2026-09-16T23:05:14Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

All verification below was performed against the actual codebase and by actually building and
running the test suites in this session — not by reading SUMMARY.md claims. Debug build was
already configured; I ran the full Debug suite plus the write_csv suite, then independently
configured, built, and ran a **second, from-scratch Release tree**
(`cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON
-DQUIVER_BUILD_C_API=ON`) to confirm the Release-only `SOL_SAFE_GETTER` claim rather than trusting
the SUMMARY's recorded numbers.

Independently observed results (this session):
- Debug: `quiver_tests.exe --gtest_filter='LuaRunner_WriteCsv*'` → **45/45 pass**.
- Debug: `quiver_tests.exe` (full suite) → **1224/1224 pass**.
- Debug: `bun test test/lua-api-sync.test.ts` (bindings/js) → **6/6 pass**.
- Release (freshly configured/built this session, not the SUMMARY's numbers): `quiver_tests.exe
  --gtest_filter='LuaRunner_WriteCsv*'` → **45/45 pass**; full suite → **1224/1224 pass**;
  `quiver_c_tests.exe` → **557/557 pass**. Matches the counts 04-03-SUMMARY.md reports, now
  independently reproduced rather than assumed.

### Observable Truths (ROADMAP Phase 4 Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | A script calls `db:write_csv(...)`, appends rows with `w:write_row{...}`, calls `w:close()`, and `db:read_csv` returns exactly the cells passed — including a cell with separator+quote+CR+LF together and a lone-quote cell | ✓ VERIFIED | `src/csv_write.cpp` `append_record` (quote-trigger set = separator, `"`, CR, LF; doubled-quote escaping); `src/lua_runner.cpp:301-341` cell dispatch; tests `WriteRowThenReadCsvRoundTripsPlainStrings`, `CellWithSeparatorQuoteCrAndLfTogetherRoundTrips`, `CellWithSeparatorQuoteCrAndLfTogetherRoundTripsWithSemicolonSeparator`, `LoneQuoteCharacterCellRoundTripsAsLengthOne`, `TwoQuoteCharacterCellRoundTripsAsLengthTwo` — all pass, run this session |
| 2 | `w:write_row({9007199254740993})` reads back as `9007199254740993`, not `...992`; a float written/read/re-written produces identical text | ✓ VERIFIED | `src/lua_runner.cpp:313-319` routes a Lua integer to `append_number`'s `int64_t` overload directly (no double intermediate); tests `IntegerCellRoundTripsExactDigitString`, `MinIntegerAndMaxIntegerRoundTripExactDecimalText`, `FloatReWriteIdentityRoundTripsForManySignificantDigitValues` pass |
| 3 | A non-finite number, a table cell, `write_row` after `close`, an escaping path, and an in-memory database each raise a Pattern 1 error naming the problem; `w:close()` twice is not an error | ✓ VERIFIED | `src/lua_runner.cpp:320-329` `std::isfinite` guard before `append_number`; `src/csv_write.cpp:135-138` closed-writer guard; `resolve_sandboxed_path` unchanged for sandbox/`:memory:`; tests `NonFiniteNumberCellThrowsNamingWriteRowAndRowOrdinal`, `TableCellThrowsNamingWriteRowAndCellIndex`, `WriteRowAfterCloseThrowsNamingWriteRow`, `LuaRunner_WriteCsvErrors.*` (8 tests, prefix+reason asserted separately), `DoubleCloseIsIdempotentNotAnError` all pass. The MEDIUM-confidence `std::to_chars` non-finite claim was executed on this toolchain (MSVC 19.51.36256) and recorded verbatim in 04-02-SUMMARY.md, confirming the guard is necessary regardless of platform spelling |
| 4 | A single-column file whose cells are `nil` or `""` round-trips with every row present | ✓ VERIFIED | `src/csv_write.cpp` `append_record`'s `lone_empty_cell` branch quotes a lone empty cell so csv_read's `KEEP_NON_EMPTY` never discards it; test `SingleColumnFileWithNilAndEmptyCellsRoundTripsEveryRow` passes |
| 5 | `LUA_DB_API_REFERENCE` documents `db:write_csv`/`w:write_row`/`w:close`, both options, truncate-at-open, and a worked example that runs verbatim; lua-api sync gate passes with the binding | ✓ VERIFIED | `bindings/js/src/lua-api.ts:701-739` "## CSV file writing" section (D-39 compact form, both D-40 halves present, D-34 clause present); `ReferenceWorkedExampleRunsAndRoundTripsItsOwnData` extracts the fenced block from the reference file at test run time (not transcribed — verified by reading `extract_lua_example`, which throws on a missing heading/fence/empty block) and asserts cells positionally including the interior-nil full-width row; test passes; `bun test test/lua-api-sync.test.ts` 6/6 pass this session |

**Score:** 5/5 success criteria verified (0 present-but-behavior-unverified)

### Requirements Coverage

All 23 requirement IDs assigned to Phase 4 are accounted for and marked Complete in
`REQUIREMENTS.md`'s traceability table; each has direct test evidence in
`tests/test_lua_runner_write_csv.cpp`, confirmed passing this session:

| Requirement | Status | Evidence |
|---|---|---|
| WRITE-01, WRITE-02 | ✓ SATISFIED | `db:write_csv`/`w:write_row`/`w:close` spine, plan 04-01 task 1 |
| WRITE-03 | ✓ SATISFIED | `HeaderIsWrittenAheadOfDataAndQuotedLikeARow` |
| WRITE-04 | ✓ SATISFIED | `InMemoryDatabaseIsPrefixedWriteCsvError`, `EscapingPathIsPrefixedWriteCsvError` |
| WRITE-05 | ✓ SATISFIED | `WriteRowAfterCloseThrowsNamingWriteRow`, `DoubleCloseIsIdempotentNotAnError` |
| WRITE-07 | ✓ SATISFIED | `MissingParentDirectoryThrowsAndDoesNotCreateIt` |
| WRITE-08 | ✓ SATISFIED | `ReopeningSamePathTruncatesExistingContent` |
| FMT-01, FMT-02, FMT-03 | ✓ SATISFIED | dirty-cell/lone-quote/CR-LF/empty-cell suite (10+ tests) |
| FMT-04 | ✓ SATISFIED | int64/float dispatch tests |
| FMT-05 | ✓ SATISFIED | non-finite guard tests + executed `std::to_chars` spot-check |
| FMT-06 | ✓ SATISFIED | `BooleanCellWritesOneOrZero` |
| FMT-08 | ✓ SATISFIED | `RowWidthComesFromMaxIntegerKeyNotKeyCount`, `RowWithZeroIntegerKeysWritesOneQuotedEmptyCell`, non-integer/sub-1 key throws |
| FMT-09 | ✓ SATISFIED | no-BOM / UTF-8-verbatim tests |
| LUA-09 | ✓ SATISFIED | collect-then-validate option decoder tests |
| LUA-10 | ✓ SATISFIED | `EscapingPathTakesPrecedenceOverInvalidSeparator` / `EscapingPathBeatsInvalidSeparator` |
| LUA-11 | ✓ SATISFIED | `sol::no_constructor` + `unique_ptr`, no `__gc`, confirmed by reading `src/lua_runner.cpp:702-722` |
| TEST-06 | ✓ SATISFIED | dirty-cell suite (separator/quote/CR/LF combos, leading/trailing/separator-only cells, CRLF, UTF-8) |
| TEST-07 | ✓ SATISFIED | int64-near-2^53, INT64_MIN/MAX, float re-write identity |
| TEST-08 | ✓ SATISFIED | lone-quote and two-quote length assertions |
| TEST-09 | ✓ SATISFIED | single-column nil/empty round trip |
| TEST-12 | ✓ SATISFIED | `LuaRunner_WriteCsvErrors` fixture, prefix+reason assertions, precedence test |
| DOC-05 | ✓ SATISFIED | reference section + extraction-execution test + sync gate |

No orphaned requirements: the phase's requirement list in `ROADMAP.md`/plan frontmatter exactly
matches `REQUIREMENTS.md`'s Phase-4-assigned rows. Phase-5-deferred rows (WRITE-06, FMT-07,
TEST-10, TEST-11, DOC-06) are correctly excluded from this phase's scope and are marked Pending
against Phase 5 in `REQUIREMENTS.md`, not silently dropped.

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| `src/utils/number.h` | `quiver::utils::append_number`, moved verbatim from `lua_runner.cpp` | ✓ VERIFIED | Present; body matches description; used by both `append_json_double`/`append_json` and the CSV cell formatter |
| `src/csv_write.h` / `src/csv_write.cpp` | Non-Pimpl writer, RFC-4180 emission, pinned message catalogue | ✓ VERIFIED | Present; matches D-37 (no Pimpl); catalogue comment block matches actual throw sites, cross-checked against `TEST-12` assertions |
| `tests/test_lua_runner_write_csv.cpp` | Round-trip-only assertions via `db:read_csv` | ✓ VERIFIED | 1104 lines, 45 `TEST_F` cases across two fixtures; spot-checked several — all read back through `db:read_csv`, none inspect the raw file for correctness (`std::filesystem::exists` is used only for the missing-directory non-creation check, which is not a content assertion) |
| `db:write_csv` binding + `CsvWriter` usertype | `src/lua_runner.cpp` | ✓ VERIFIED | `sol::no_constructor`, `std::unique_ptr` return, no `__gc`, path resolved before options decode |
| `bindings/js/src/lua-api.ts` CSV writing section | DOC-05 text | ✓ VERIFIED | Present, compact 10-line example, both option keys, truncate-at-open warning, D-34/D-40 clauses |

### Key Link Verification

| From | To | Via | Status |
|---|---|---|---|
| `db:write_csv` | `resolve_sandboxed_path` | called before options decode | ✓ WIRED — confirmed by reading `src/lua_runner.cpp:693-697` and by the passing `EscapingPathTakesPrecedenceOverInvalidSeparator`/`EscapingPathBeatsInvalidSeparator` tests |
| `csv_write.cpp` / `lua_runner.cpp` cell formatter | `quiver::utils::append_number` | direct call, both int64 and double overloads | ✓ WIRED |
| `tests/test_lua_runner_write_csv.cpp` | `db:read_csv` | every correctness assertion round-trips | ✓ WIRED — confirmed by code reading; no raw-file string search used for correctness |
| `bindings/js/test/lua-api-sync.test.ts` | `src/lua_runner.cpp` | parses bound `db:` names, fails build if undocumented | ✓ WIRED — 6/6 pass this session |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| write_csv suite (Debug) | `quiver_tests.exe --gtest_filter='LuaRunner_WriteCsv*'` | 45/45 pass | ✓ PASS |
| Full suite (Debug) | `quiver_tests.exe` | 1224/1224 pass | ✓ PASS |
| write_csv suite (fresh Release build) | `build-release/bin/quiver_tests.exe --gtest_filter='LuaRunner_WriteCsv*'` | 45/45 pass | ✓ PASS |
| Full suite (fresh Release build) | `build-release/bin/quiver_tests.exe` | 1224/1224 pass | ✓ PASS |
| C API suite (fresh Release build) | `build-release/bin/quiver_c_tests.exe` | 557/557 pass | ✓ PASS |
| lua-api sync gate | `bun test test/lua-api-sync.test.ts` | 6/6 pass | ✓ PASS |

The Release build was configured and built from scratch in this verification session (not reused
from the executor's SUMMARY-recorded run, which no longer exists on disk since `build-release/` is
gitignored) — this is independent reproduction, not trust in the claim.

### Anti-Patterns Found

No `TODO`/`FIXME`/`XXX`/`placeholder`/"not yet implemented" markers found in any file this phase
created or modified (`src/csv_write.h`, `src/csv_write.cpp`, `src/utils/number.h`,
`tests/test_lua_runner_write_csv.cpp`). No stub returns, no hardcoded-empty data flowing to a
caller. Debt-marker gate: clean.

**Known issue from code review (04-REVIEW.md), treated as already-reported context, not
re-derived here:** CR-01 — `csv_row_cells_from_lua` (`src/lua_runner.cpp:273-288`) and
`csv_header_from_lua` (`:347-365`) size their output vector from the row/header table's maximum
integer key with no upper bound, so a single Lua statement with a very large key (e.g.
`{[100000000] = "x"}`) triggers a large, unbounded allocation before any content is written. This
is a real robustness/DoS-shaped gap (the file already has a `kMaxReturnDepth`/`kMaxReturnBytes`
precedent for exactly this class of untrusted-script hazard, applied to the JSON return encoder
but not here). **Judgment: this does not defeat any of the five ROADMAP success criteria or any
of the 23 requirement IDs in scope.** No success criterion or requirement asks for a bound on row
width, and every test in the suite uses small, well-formed tables — the round-trip behavior the
phase promises holds for all of them. It is a genuine latent DoS surface worth fixing (WARNING,
not BLOCKER), consistent with the review's own "critical" severity for code quality but outside
what this phase's goal requires. Recommend tracking it as a fast-follow fix (bound `max_index`,
mirroring the JSON encoder's caps) rather than reopening this phase.

### Human Verification Required

None. Every success criterion and requirement in scope was verified by reading the actual
implementation and by independently building and running the test suites (Debug and a
from-scratch Release configuration) in this session.

### Gaps Summary

No gaps. All five ROADMAP success criteria hold against the real codebase and real (independently
re-run) test suite, all 23 in-scope requirement IDs are satisfied with direct test evidence, and
the DOC-05 reference/binding pairing and lua-api sync gate are in place and passing. The one
known code-review finding (CR-01, unbounded allocation from an untrusted integer table key) is
carried forward as documented context per the orchestrator's instruction — it is a real
robustness gap but does not defeat phase goal achievement.

---

_Verified: 2026-09-16T23:05:14Z_
_Verifier: Claude (gsd-verifier)_
