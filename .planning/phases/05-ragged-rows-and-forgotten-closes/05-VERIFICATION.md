---
phase: 05-ragged-rows-and-forgotten-closes
verified: 2026-09-17T14:00:00Z
status: passed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
---

# Phase 5: Ragged rows and forgotten closes Verification Report

**Phase Goal:** A file written by an imperfect script — a row that does not match the header, a
script that never calls `close()` — still reads back complete and aligned.

**Verified:** 2026-09-17T14:00:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (ROADMAP Phase 5 success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | With a `header` of N names, a short row round-trips through `db:read_csv` with N fields, each value under the meant column | ✓ VERIFIED | `src/lua_runner.cpp:733-741` pads `cells` to `header_width` **before** `Writer::write_row` is called (confirmed by reading the code directly, not trusting the SUMMARY). `LuaRunner_WriteCsv.ShortRowPadsToHeaderWidthAndRoundTripsAligned`, `.EmptyRowPadsToMultiColumnHeaderWidth`, `.EmptyRowUnderSingleColumnHeaderStillRoundTrips`, `.MultiByteUtf8CellsDoNotChangeCellCounts` all pass (re-ran `quiver_tests.exe --gtest_filter=LuaRunner_WriteCsv.*`: 47/47 PASSED). Each asserts via `db:read_csv`, not raw bytes (`grep 'ifstream\|rdbuf'` over the test file returns nothing in the relevant bodies). |
| 2 | A row longer than the header raises a Pattern 1 error naming the row ordinal and both counts; rows already written remain on disk | ✓ VERIFIED | `src/lua_runner.cpp:734-737`: `throw std::runtime_error("Cannot write_row: row " + ... + " has " + ... + " cells but header declares " + ...)` — checked strictly before the pad branch, so a long row is never truncated. Catalogued in `src/csv_write.cpp`'s comment (`git diff` from pre-phase base is a single comment-only added line, functionally verified below). `RowLongerThanHeaderThrowsNamingOrdinalAndCounts` and `RejectedLongRowLeavesEarlierRowsOnDisk` both pass. |
| 3 | With no `header` given, rows of differing widths are written as-is and no width error is raised | ✓ VERIFIED | `header_width == 0` (from `csv_options.header.size()` with an empty/omitted header) short-circuits both branches in the `if (self.header_width != 0)` guard at `src/lua_runner.cpp:733`. `NoHeaderMeansNoWidthCheck` passes. |
| 4 | A script that returns without `w:close()` leaves a complete, re-readable file, asserted with the `LuaRunner` alive and undestroyed | ✓ VERIFIED | Read `tests/test_lua_runner_write_csv.cpp:1333-1403` directly: both `UnclosedWriterIsFlushedWhenRunReturns` and `ScriptErrorMidWriteStillLeavesEarlierRowsReadable` declare `quiver::LuaRunner lua(db)` **once**, call `lua.run(...)` **twice** on the same object, and never destroy/move/reset `lua` between the calls — the exact trap named in the verification method was checked and does not apply. `GcGuard` (`src/lua_runner.cpp:2202-2205`) is declared before `auto result = impl_->lua.safe_script(...)` and its destructor calls `collect_garbage()` exactly once (`grep -c collect_garbage src/lua_runner.cpp` = 1). Both tests pass in Debug (`quiver_tests.exe`: 1234/1234) and in a freshly rebuilt Release tree (`build-release/bin/quiver_tests.exe`: 1234/1234, `LuaRunner_WriteCsv.*`: 47/47). |
| 5 | `src/CLAUDE.md`, root `CLAUDE.md` and `CHANGELOG.md` record the writer and its design decisions | ✓ VERIFIED | All three files, plus `bindings/js/src/lua-api.ts` (D-50), read directly and contain the FMT-07/WRITE-06 content (see Documentation Coverage below). `grep -rn "is not exposed" --include=*.md --include=*.ts .` (excluding `.planning/`) returns nothing — both stale copies of the claim are gone. |

**Score:** 5/5 truths verified (0 present-but-behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner.cpp` — `CsvWriter::header_width` | `std::size_t` member, 0 = no enforcement | ✓ VERIFIED | Line 266, member; line 268-269, second constructor param; line 703-705, threaded from `csv_options.header.size()` in the `write_csv` factory |
| `src/lua_runner.cpp` — FMT-07 pad/throw block | in `write_row` lambda | ✓ VERIFIED | Lines 727-742, sits between `csv_row_cells_from_lua` and `self.writer.write_row(cells, ...)` |
| `src/csv_write.cpp` — catalogue line | comment-only | ✓ VERIFIED | `git diff` from pre-phase base (`4e6d9d9`) is a single `+` comment line; no functional code changed |
| `src/csv_write.h` | untouched | ✓ VERIFIED | `git diff 4e6d9d9 HEAD -- src/csv_write.h` is empty |
| `src/lua_runner.cpp` — `GcGuard` | RAII struct in `LuaRunner::run` | ✓ VERIFIED | Lines 2202-2205, declared before `safe_script` (line 2207) |
| `tests/test_lua_runner_write_csv.cpp` — TEST-10/TEST-11 cases | gtests | ✓ VERIFIED | 11 new tests found and passing (9 TEST-10 + 2 TEST-11), part of the 47-test `LuaRunner_WriteCsv` suite (36 pre-existing from Phase 4 + 11 new) |
| `CLAUDE.md`, `src/CLAUDE.md`, `CHANGELOG.md`, `bindings/js/src/lua-api.ts` | doc edits | ✓ VERIFIED | See Documentation Coverage |

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `csv_options.header.size()` (write_csv factory) | `CsvWriter::header_width` | constructor param | ✓ WIRED | `src/lua_runner.cpp:703-705` |
| `header_width` | `write_row`'s pad/throw branch | `if (self.header_width != 0)` | ✓ WIRED | `src/lua_runner.cpp:733-742`, checked before `self.writer.write_row(...)` at line 743 |
| `GcGuard` declaration | `safe_script` call | declaration order | ✓ WIRED | Guard at line 2202-2205, `safe_script` at line 2207 — guard declared first, so it is destroyed *after* `result`, confirmed by direct code read, matching D-46 |
| `CsvWriter` ownership (`unique_ptr` + `sol::no_constructor` + default `__gc`) | `collect_garbage()` | sol2 GC | ✓ WIRED | Proven behaviorally: `UnclosedWriterIsFlushedWhenRunReturns` fails without `GcGuard` (per 05-02-SUMMARY's recorded RED run, 0 bytes observed) and passes with it — a genuine before/after test, not presence-only |

### Behavioral Spot-Checks / Test Execution (independently re-run, not taken from SUMMARY)

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full C++ core suite (Debug) | `./build/bin/quiver_tests.exe` | 1234/1234 passed | ✓ PASS |
| Full C API suite (Debug) | `./build/bin/quiver_c_tests.exe` | 557/557 passed | ✓ PASS |
| `LuaRunner_WriteCsv` suite (Debug) | `--gtest_filter=LuaRunner_WriteCsv.*` | 47/47 passed | ✓ PASS |
| Full C++ core suite (Release, `SOL_SAFE_GETTER` off) | `./build-release/bin/quiver_tests.exe` (rebuilt fresh) | 1234/1234 passed | ✓ PASS |
| Full C API suite (Release) | `./build-release/bin/quiver_c_tests.exe` | 557/557 passed | ✓ PASS |
| `LuaRunner_WriteCsv` suite (Release) | `--gtest_filter=LuaRunner_WriteCsv.*` | 47/47 passed | ✓ PASS |
| lua-api sync gate | `cd bindings/js && bun test test/lua-api-sync.test.ts` | 6 pass, 0 fail | ✓ PASS |

Note: the `build-release/` tree on disk was stale (predated the post-review clang-format commit
`7e28730`); it was rebuilt during this verification (`cmake --build build-release --config
Release`) before re-running its tests, so the Release pass counts above are against current
source, not a cached binary.

### Documentation Coverage (DOC-06)

| File | Claim checked | Evidence |
|------|---------------|----------|
| root `CLAUDE.md` | Extended `db:read_csv` bullet states writer is streaming-only, Lua-only, sandboxed, truncate-at-open, two options, FMT-07's pad/throw rule, WRITE-06's flush | `CLAUDE.md:224-236` (read directly) |
| root `CLAUDE.md` | New `CSV file write` cross-layer table row beside `CSV file read` | `CLAUDE.md:645-646`, `N/A` in every non-Lua column |
| `src/CLAUDE.md` | FMT-07 lives in `CsvWriter`, not `Writer`; `GcGuard`/WRITE-06 bullet in `## LuaRunner` conventions | present verbatim (see file contents surfaced during verification) — states "FMT-07's row-width enforcement ... lives entirely in the Lua-layer `CsvWriter` wrapper" and "A writer left open when the script returns is still flushed" with `GcGuard` detail |
| `CHANGELOG.md` | Stale "not exposed" clause corrected; writer's own `### Added` entry inside existing `## [0.10.7] — unreleased` | `CHANGELOG.md:1-40`, no version number changed (confirmed no `0.10.x` diff line exists between what's committed and any manifest) |
| `bindings/js/src/lua-api.ts` | Two clauses (pad/throw rule, flush guarantee) added to existing `## CSV file writing` section, fenced example untouched | Lines 742-746, confirmed present; `bun test test/lua-api-sync.test.ts` and the `ReferenceWorkedExampleRunsAndRoundTripsItsOwnData` gtest both pass |
| repo-wide | No file still claims writing is unexposed | `grep -rn "is not exposed" --include=*.md --include=*.ts .` (excluding `.planning/`) returns nothing |

### Prohibitions Verified (must_haves.prohibitions across all three plans)

| Prohibition | Status | Evidence |
|-------------|--------|----------|
| Long row never silently truncated | ✓ HOLDS | Reject branch (`cells.size() > header_width`) checked strictly before pad branch |
| No correctness assertion reads raw file bytes | ✓ HOLDS | All new assertions round-trip through `db:read_csv`/Lua-side `csv.rows` |
| No new `db:write_csv` option key | ✓ HOLDS | `write_csv_options_from_lua` still recognizes only `separator` and `header` (`src/lua_runner.cpp:393-401`) |
| `csv_write::Writer`'s public interface unchanged | ✓ HOLDS | `git diff` on `src/csv_write.h` is empty |
| No warning/log/counter/`weak_ptr` registry for the unclosed writer | ✓ HOLDS | `grep -c 'log_warning\|weak_ptr' src/lua_runner.cpp` = 0 |
| Exactly one `collect_garbage()` call | ✓ HOLDS | `grep -c collect_garbage src/lua_runner.cpp` = 1 |
| No test destroys/moves/resets the `LuaRunner` before asserting (TEST-11 trap) | ✓ HOLDS | Verified by direct code read of both TEST-11 bodies |
| Five version manifests all still `0.10.6`; no version line changed in `CHANGELOG.md` | ✓ HOLDS | `CMakeLists.txt`, `bindings/js/package.json`, `bindings/python/pyproject.toml`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml` all read `0.10.6` |
| `src/csv_write.h` untouched | ✓ HOLDS | Empty diff |
| Declined items (unclosed-writer warning, overwrite guard, whole-file writer, atomic write, extra options) not documented as future work | ✓ HOLDS | Confirmed by reading all four edited doc files — no "planned"/"not yet"/"future" language found for these items |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| FMT-07 | 05-01 | Header length is row-width authority: pad short, throw long, no header = no check | ✓ SATISFIED | Code + tests above |
| WRITE-06 | 05-02 | Writer still open at `run()`'s return is flushed; no warning | ✓ SATISFIED | `GcGuard` + tests above |
| TEST-10 | 05-01 | Short row round-trips aligned; longer row throws | ✓ SATISFIED | 9 new tests, all passing |
| TEST-11 | 05-02 | Unclosed writer leaves complete file, asserted with runner alive | ✓ SATISFIED | 2 new tests, runner-alive trap checked directly |
| DOC-06 | 05-03 | Four files record the writer and design decisions | ✓ SATISFIED | Direct file reads above |

No orphaned requirements: ROADMAP Phase 5 lists exactly these five requirements, and all five appear across the three plans' `requirements:` frontmatter.

### Anti-Patterns Found

None. `grep -n "TBD|FIXME|XXX|TODO|HACK|PLACEHOLDER"` over every file this phase touched
(`src/lua_runner.cpp`, `src/csv_write.cpp`, `tests/test_lua_runner_write_csv.cpp`, `CLAUDE.md`,
`src/CLAUDE.md`, `CHANGELOG.md`, `bindings/js/src/lua-api.ts`) returned nothing. The code-review
artifact (`05-REVIEW.md`) flagged two clang-format drift nits (IN-01, IN-02); both were fixed in a
follow-up commit (`7e28730`, "style(05): apply clang-format to phase 5 edits") that this
verification confirmed is present on the branch and does not regress any test (47/47 still green
after that commit, checked directly in Debug and after a fresh Release rebuild).

### Human Verification Required

None. Every ROADMAP success criterion is either directly observable in the source (grep/read) or
independently re-run as an automated test in this session (not merely quoted from a SUMMARY.md).

### Gaps Summary

No gaps. All five ROADMAP Phase 5 success criteria hold, all five requirements are satisfied, all
prohibitions from the three plans' `must_haves.prohibitions` blocks hold, and the code review's
two Info-level formatting nits were resolved in a subsequent commit that this verification
confirmed does not disturb any test result. The full C++ core and C API suites pass in both Debug
and a freshly rebuilt Release configuration (`SOL_SAFE_GETTER` off), and the JS lua-api sync gate
passes.

One item is worth carrying forward as context rather than as a gap: DOC-06's own planning
artifacts (05-03-PLAN.md `flagged_assumptions`, 05-03-SUMMARY.md "Unresolved Edge") note that the
edge-probe process could not derive a behavioral predicate for DOC-06 beyond "the four files were
edited as specified, and no file still claims the writer is unexposed." This verification confirms
that narrower claim is true (direct file reads, repo-wide grep for the stale claim), and notes —
consistent with the planner's own framing — that a stronger DOC-06 gate does not exist in any
source artifact and was not fabricated here.

---

*Verified: 2026-09-17*
*Verifier: Claude (gsd-verifier)*
