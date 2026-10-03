---
phase: 01-behaviour-pins
verified: 2026-10-02T23:30:00Z
status: human_needed
score: 17/17 must-haves verified
behavior_unverified: 0
overrides_applied: 0
human_verification:
  - test: "Prohibition (judgment tier, plans 01 and 02): MUST NOT change production behaviour to make a pin green; no file under src/, include/, cmake/ or bindings/*/src modified, no existing test assertion edited or removed"
    expected: "Confirm the non-authoritative verdict UPHELD. Evidence: `git diff --stat 5b57e7c -- src include cmake bindings/*/src` is empty; `git diff -U0 5b57e7c -- tests/*.cpp tests/*.h tests/CMakeLists.txt bindings/js/test | grep '^-[^-]'` prints only the one Expression-only floor the plan told Task 1 to replace (still covered by the new four-type floor)"
    why_human: "judgment-tier prohibition; an LLM verdict is not authoritative and must not be absorbed silently into a pass"
  - test: "Prohibition (judgment tier, plans 01 and 02): MUST NOT revert, stage or commit the pre-existing uncommitted edits to .planning/PROJECT.md, REQUIREMENTS.md, ROADMAP.md"
    expected: "Confirm the non-authoritative verdict UPHELD. Evidence: those edits were committed before execution in 4b4728c ('docs: drop the LuaRunner -> Sandbox rename'). The later touches (2b7023d, e82c2a7, 2f62bec) are requirement/plan checkbox updates plus blank-line markdown normalisation, not the in-progress content"
    why_human: "judgment-tier prohibition; whether the blank-line normalisation in 2b7023d is acceptable is a maintainer call"
  - test: "Prohibition (judgment tier, plan 02): MUST NOT record a baseline count that was not observed"
    expected: "Confirm the non-authoritative verdict UPHELD. Evidence: re-observed by the verifier: Lua* = 444 tests / 12 suites in build/dev and build/release, LuaRunnerCApiTest = 27, quiver_tests = 1410, quiver_c_tests = 543 — all match STATE.md and 01-02-SUMMARY"
    why_human: "judgment-tier prohibition"
---

# Phase 1: Behaviour Pins Verification Report

**Phase Goal:** Every existing behaviour that moving the code could silently break is pinned by a test that is defined, and green, in both Debug and Release, before any production source changes.
**Verified:** 2026-10-02
**Status:** human_needed (all truths verified; only the three judgment-tier prohibitions need a human sign-off, each with an evidence-backed UPHELD verdict)
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (roadmap success criteria + plan must_haves)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| SC1 | Full Pattern 1 messages for the 1,000,000 cap on `w:write_row({[1000001]='x'})` and the `db:write_csv` header `{[2e6]='a'}`; two-bad-argument order pins for open_file mode-before-path, containment-before-options, write_row type-before-closed | VERIFIED | `RowKeyPastMaximumWidthThrows`, `HeaderKeyPastMaximumWidthThrows` (asserts `key 2000000`), `OpenFileReportsInvalidModeBeforeEscapingPath`, `Escape*BeforeNonTableOptions` (read_csv, read_csv_stream, write_csv, export_csv, import_csv), `StreamReportsNonFunctionOnRowBeforeEscapingPath`, `NonTableRowOnClosedWriterReportsTheType`, `UnsupportedCellOnClosedWriterReportsClosed` — all pass in dev and release. Messages match `src/lua_runner.cpp` 365-368, 569, 768, 832, 906, 1368/1413 |
| SC2 | Runner move-constructed and move-assigned after write_csv/open_file registered handles; moved-to runner opens new handles in a second script and they close at its run() exit | VERIFIED | `tests/test_lua_runner_lifecycle.cpp`: `MoveConstructor`, `MoveAssignment` (+ `*OutlivesSource` freed-source variants, + `static_assert(sizeof(LuaRunner)==sizeof(void*))`). `open_handles("second")` then `expect_handles_closed("second")` checks `not g:is_open()`, re-reads the CSV and the flushed .qvr. Both lambdas are `[this]` captures (lua_runner.cpp open_file 759, write_csv 874). Pass in dev and release |
| SC3 | JS sync test fails on a zero-method BinaryFile/BinaryMetadata/Expression/CsvWriter or `open_libraries(` count != 1; mutation confirms | VERIFIED | `lua-api-sync.test.ts` 59-65. Verifier mutation: deleted CsvWriter `"write_row",` (l.898) and `"close",` (l.941) → `parse found the binding surface` failed with `["CsvWriter"]`, 5 pass / 1 fail; reverted, `git diff --exit-code -- src/` exit 0. Clean: 6 pass. `open_libraries(` occurs once in src (l.243) |
| SC4 | Every new test passes in dev and release; no Release-UB path exercised; phase diff limited to tests/, bindings/js/test/, tests/AGENTS.md, bindings/js/AGENTS.md | VERIFIED | 21-test filter (16 new + controls) passes in both builds. Bad values used are all explicitly type-checked `sol::object` paths (options `5`/`"x"` → l.569/1368/1413 check; on_row `5` → l.832; row `5` → l.906; `{print}` hits the closed check first); no non-table into a `sol::table` param, no non-string map key, no wrong-typed optional (open_file md is a real BinaryMetadata or absent). `git diff --name-only 5b57e7c -- . ':(exclude).planning'` = 10 paths, all in tests/ or bindings/js/{test,AGENTS.md} |
| SC5 | All six suites green; Lua* count and C API count recorded as baseline | VERIFIED | Verifier ran `scripts/test-all.bat` once: all six PASS, exit 0 (Python 350 passed). Lua* = 444 / 12 suites (dev, release); LuaRunnerCApiTest = 27. Recorded in `.planning/STATE.md` l.81 and l.91. 428 + 16 new TEST_Fs = 444 |
| P1 | Moved-to runner sees the source's Lua globals | VERIFIED | `assert(origin == 'first', ...)` in all four lifecycle tests |
| P2 | Existing width/key pins stay green (RowWidthComesFromMaxIntegerKeyNotKeyCount, RowWithZeroIntegerKeysWritesOneQuotedEmptyCell, NonIntegerRowKeyThrows, SubOneIntegerRowKeyThrows) | VERIFIED | All pass in dev and release |
| P3 | Key of exactly 1000000 stays legal (backstop, deliberately unpinned) | VERIFIED (explicit evidence) | Code: `if (max_index > kMaxWidth)` (l.365). Spot-check via release `quiver_cli`: `w:write_row({[1000000]='x'})` exit 0, 1,000,001-byte file |
| P4 | open_file('../escape','z') reports the mode error | VERIFIED | `OpenFileReportsInvalidModeBeforeEscapingPath` asserts `Cannot open_file: mode must be "r" or "w"` |
| P5 | read_csv/write_csv/export_csv/import_csv escape + options `5` report containment | VERIFIED | Four `*BeforeNonTableOptions` pins, full `Cannot <op>: path '<p>' escapes the database directory` |
| P6 | read_csv_stream on_row-before-path and containment-before-options | VERIFIED | `StreamReportsNonFunctionOnRowBeforeEscapingPath`, `StreamReportsEscapingPathBeforeNonTableOptions` |
| P7 | Closed writer: `write_row(5)` → type error; `write_row({print})` → closed error | VERIFIED | Two `*OnClosedWriter*` pins |
| P8 | Sync-test floors are per usertype; CsvWriter deletion fails with `["CsvWriter"]` despite BinaryFile's own `close` | VERIFIED | Verifier mutation output above |
| P9 | Failure output lists types in fixed order | VERIFIED | Literal array `["BinaryFile","BinaryMetadata","Expression","CsvWriter"].filter(...)` |
| P10 | Hand mutation never committed/built | VERIFIED | `git diff 5b57e7c -- src` empty; no src commit in range |
| P11 | tests/AGENTS.md lists `_lifecycle`, Release filter `Lua*`; both AGENTS.md describe the guards | VERIFIED | tests/AGENTS.md l.37, 40-44, 147, 178-180; bindings/js/AGENTS.md l.39-41 |
| P12 | dbMethods > 40 and quiverFns > 10 floors stay | VERIFIED | Lines 57-58 unchanged |

**Score:** 17/17 truths verified (0 present, behavior-unverified). Behaviour-dependent truths (move/close invariants, sync-test failure) are each backed by a passing named test or a verifier-run mutation.

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `tests/test_lua_runner_lifecycle.cpp` | VERIFIED | `class LuaRunner_Lifecycle : public LuaSandboxTest`, 4 tests, static_assert |
| `tests/CMakeLists.txt` | VERIFIED | l.41, between `_fk` and `_migrations` |
| `tests/test_lua_binary.cpp` | VERIFIED | `OpenFileReportsInvalidModeBeforeEscapingPath` |
| `tests/test_lua_runner_read_csv.cpp` | VERIFIED | 3 order pins |
| `tests/test_lua_runner_csv_export.cpp` / `_csv_import.cpp` | VERIFIED | `EscapeIsReportedBeforeNonTableOptions`; import also gains the `options must be a table` control in `OptionsAreStrict` |
| `tests/test_lua_runner_write_csv.cpp` | VERIFIED | 6 new pins incl. `NonTableOptionsThrows` control |
| `bindings/js/test/lua-api-sync.test.ts` | VERIFIED | `const unparsed`, `open_libraries\(` count guard |
| `tests/AGENTS.md`, `bindings/js/AGENTS.md`, `.planning/STATE.md` | VERIFIED | contents as above |

### Key Link Verification

| From | To | Status | Details |
|------|----|--------|---------|
| tests/CMakeLists.txt | test_lua_runner_lifecycle.cpp | WIRED | 4 LuaRunner_Lifecycle tests listed/run in both builds |
| test_lua_runner_write_csv.cpp | src/lua_runner.cpp cap | WIRED | message text identical to l.366-368 |
| test_lua_runner_lifecycle.cpp | test_lua_runner.h LuaSandboxTest | WIRED | `public LuaSandboxTest`, uses `sandbox`, `db_path()` |
| lua-api-sync.test.ts | src/lua_runner.cpp | WIRED | `readFileSync(CPP_PATH)` → `usertypeMethods`; mutation proves the parse is live |
| STATE.md | build/dev/bin/quiver_tests.exe | WIRED | 444/12 and 27 re-observed |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Lua* in Debug | `build/dev/bin/quiver_tests.exe --gtest_filter=Lua*` | 444 tests / 12 suites passed | PASS |
| Lua* in Release | `build/release/bin/quiver_tests.exe --gtest_filter=Lua*` | 444 tests / 12 suites passed | PASS |
| New pins + controls, both builds | named 21-test filter | 21/21 OK in each | PASS |
| C API baseline | `quiver_c_tests --gtest_filter=LuaRunnerCApiTest.*` | 27 passed | PASS |
| Full counts | `--gtest_list_tests` | quiver_tests 1410, quiver_c_tests 543 | PASS |
| Six suites | `scripts/test-all.bat` (run once) | all PASS, exit 0 | PASS |
| Sync test clean / mutated | `bun test test/lua-api-sync.test.ts` | 6 pass / mutated: 1 fail naming CsvWriter | PASS |
| Width boundary | release `quiver_cli` with `{[1000000]='x'}` | exit 0, 1,000,001 bytes | PASS |
| Formatting | clang-format 22.1.8 `--dry-run --Werror` on 6 touched .cpp | clean | PASS |

### Probe Execution

Not applicable: no probes declared, no `scripts/*/tests/probe-*.sh` in scope.

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| PIN-01 | 01-01 | SATISFIED | SC1 cap pins |
| PIN-02 | 01-01 | SATISFIED | SC1 order pins |
| PIN-03 | 01-02 | SATISFIED | SC3 + mutation |
| PIN-04 | 01-01 | SATISFIED | SC2 lifecycle pins |
| PIN-05 | 01-01, 01-02 | SATISFIED | SC4 Release-defined bad values only; green in release |

No orphaned requirements: REQUIREMENTS.md maps exactly PIN-01..05 to Phase 1, all claimed.

### Anti-Patterns Found

None. No TBD/FIXME/XXX/TODO in added lines; no planning IDs in added lines (the `FMT-02`/`WRITE-08` comments visible in diff context are pre-existing, left for Phase 5's sweep).

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| tests/test_lua_runner_lifecycle.cpp | 8 | `static_assert(sizeof(LuaRunner) == sizeof(void*))` | Info | Relies on `unique_ptr<Impl>` with default deleter being pointer-sized (true on MSVC/libstdc++/libc++). Phase 2 must keep a default-deleter pimpl or update the assert |

### Human Verification Required

Three judgment-tier prohibitions (frontmatter `human_verification`). For each the verifier's non-authoritative verdict is UPHELD with deterministic evidence; a human only needs to confirm:

1. **No production change / no edited assertion** — src/include/cmake/bindings/*/src diff empty; the only removed test line is the planned Expression-floor replacement.
2. **Pre-existing planning edits untouched** — they were committed in 4b4728c before execution; later commits only flip checkboxes and normalise blank lines in ROADMAP/REQUIREMENTS.
3. **Baseline was observed, not predicted** — verifier re-observed 444/12 and 27.

### Gaps Summary

No gaps. Every roadmap success criterion and plan truth holds in the codebase, with tests re-run by the verifier in both presets and the sync-test guard confirmed by an independent mutation. Note: the verifier's mutation check rewrote and restored `src/lua_runner.cpp` (content identical, `git diff --exit-code -- src/` exit 0), so its mtime changed and the next dev/release build will recompile that one file.

---

_Verified: 2026-10-02_
_Verifier: Claude (gsd-verifier)_

## Post-verification changes

Code review (01-REVIEW.md) found 2 warnings; both were fixed in `7155e24` after this report was written:

- **WR-02:** the lifecycle pins now also assert the CSV writer is closed (`pcall(w.write_row, ...)` fails with
  "already closed"), not only that its row reached disk.
- **WR-01:** the sync test derives the usertype list from the parse (four known types as a floor). Mutation-checked:
  an added `Fake` usertype with an undocumented method fails the coverage test, and an added usertype that parses to
  nothing fails the guard; `src/` restored byte-identical.

Re-observed after the fix: `Lua*` = 444 tests / 12 suites passing in build/dev and build/release; sync test 6/6;
clang-format 22.1.8 and biome clean. Must-have score unchanged (17/17).

## Orchestrator cross-check of the human items (not authoritative)

The orchestrator re-derived all three prohibitions independently and found each UPHELD:

1. `git diff --stat 5b57e7c..HEAD -- src include cmake 'bindings/*/src'` is empty; the 10 changed paths are all under
   `tests/`, `bindings/js/test/` or the two AGENTS.md files. Removed lines are the planned Expression-only floor, the
   WR-01 refactor of the same sync test (strictly stronger), and rewrapped AGENTS.md prose.
2. Since `4b4728c`, PROJECT/REQUIREMENTS/ROADMAP changes are status cells, checkboxes and the plan list only.
3. 444 / 12 observed by the orchestrator in both builds (full dev `quiver_tests` = 1410).

Status stays `human_needed` until a maintainer signs off (item 2's blank-line normalisation is a maintainer call).
