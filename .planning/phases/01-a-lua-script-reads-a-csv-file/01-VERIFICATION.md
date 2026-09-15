---
phase: 01-a-lua-script-reads-a-csv-file
verified: 2026-09-15T14:10:00Z
status: human_needed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
human_verification:
  - test: "WR-01 (from 01-REVIEW.md): the three filesystem precondition checks in Reader::Reader (src/csv_read.cpp:54-62, fs::exists/fs::is_directory/fs::file_size) run before the try/catch that wraps csv::CSVReader construction. All three use the throwing std::filesystem overloads, which the standard permits to throw std::filesystem_error on ANY OS-level stat failure, not only 'path does not exist' (e.g. a permission-denied parent directory or a broken symlink). Confirmed by direct code reading: the code at lines 54-62 sits above the `try` at line 64."
    expected: "Decide whether to fix now (move the three checks inside the try, or wrap them in their own try/catch re-throwing through the same 'Cannot <op>: ' prefix) or accept as a documented, low-likelihood gap and track it as follow-up debt. Every one of the four REQUIRED sandbox/file negatives (escaping path, in-memory db, missing file, directory-as-path) is unaffected — those all throw hand-crafted 'Cannot <op>: ...' messages directly, not via a caught std::filesystem_error — so the phase's five success criteria are not blocked by this. The gap is narrower than the code review implies: an OS-level std::filesystem_error unrelated to the three explicit checks above (permission-denied, broken symlink) could still bypass the Pattern-1 prefix. LUA-08's blanket 'no unwrapped message' guarantee is not airtight for that narrow class of input."
    why_human: "No portable way was found (by the phase's own executor, documented in 01-03-SUMMARY.md as human_judgment: true) to trigger this on Windows — std::filesystem::permissions does not block owner read access. Whether this is worth a dedicated CI job on a different OS/permission model, or a preemptive code fix, is a product/risk-tolerance call, not something a grep or a local test run can resolve."
  - test: "D-22 catalogue entry 10 (the csv::CSVReader-construction-failure wrapper, 'Cannot <op>: cannot read file '<p>': <reason>') has no test that actually exercises the runtime failure path on this platform. tests/test_lua_runner_read_csv.cpp#ParserWrapperMessageExistsInSource only asserts the wrapper text is present in src/csv_read.cpp at the source level, not that it fires correctly at runtime."
    expected: "Confirm (in CI on Linux/macOS, or via a manufactured permission-denied file) that the try/catch around csv::CSVReader construction (src/csv_read.cpp:64-71) actually produces the documented 'Cannot <op>: cannot read file ...' message when the parser itself fails to open a file that passed the three precondition checks."
    why_human: "The phase's own SUMMARY (01-03-SUMMARY.md, coverage id D7) flags this explicitly as human_judgment: true — no portable trigger exists in the current environment, so the runtime firing of this catch block was never observed, only proven present by static inspection."
---

# Phase 01: A Lua script reads a CSV file Verification Report

**Phase Goal:** A Lua script can read a CSV file off disk — whole-file or row-by-row — with every
cell arriving as a string and every path resolved against the database directory.

**Verified:** 2026-09-15T14:10:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (Success Criteria from ROADMAP/task)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `db:read_csv("data.csv")` returns rows addressed positionally with header available separately | ✓ VERIFIED | `src/lua_runner.cpp:452-479`; test `LuaRunner_ReadCsv.CleanFileReturnsHeaderAndRows`/`ExactJsonRoundTrip` (pass, live run); independently confirmed via `quiver_cli.exe` manual script returning `{"header":["code","date"],"rows":[["0012","2024-01-15"]]}` |
| 2 | `db:read_csv_stream("data.csv", on_row)` fires once per row, bounded window not tied to CPU count | ✓ VERIFIED | `src/lua_runner.cpp:480-515`; `cmake/Dependencies.cmake:76` forces `CSV_ENABLE_THREADS OFF` — confirmed actually compiled in via `build/CMakeCache.txt` (`CSV_ENABLE_THREADS:BOOL=OFF`) and `build/compile_commands.json` (`-DCSV_ENABLE_THREADS=0`); csv-parser's own `orchestrator.hpp:78-93` `read_window_size()` returns `chunk_size` unmultiplied under the `#else` (non-threaded) branch — the CPU-count multiplication only exists in the `#if CSV_ENABLE_THREADS` branch, which is compiled out. No `chunk_size(...)` call exists anywhere in `src/csv_read.cpp` (grep confirmed) so no caller can move the window regardless |
| 3 | Every cell arrives as a string; `"0012"` stays `"0012"`; nothing coerced | ✓ VERIFIED | `src/csv_read.cpp:107` uses `field.get<std::string_view>()` only, never a numeric getter; test `StringCellsNoInference` (pass); independently confirmed via `quiver_cli.exe`: `{"t1":"string","r1":["0012","2024-01-15"]}` |
| 4 | Escaping path, in-memory db, missing file, directory-as-path each raise `Cannot read_csv: ...`; subdirectory accepted | ✓ VERIFIED | `resolve_sandboxed_path` reused verbatim (`src/lua_runner.cpp:458,488`); `src/csv_read.cpp:54-62` explicit not-found/directory/empty checks; 8 sandbox-negative tests + 1 subdirectory-positive test all pass for both entry points (`LuaRunner_ReadCsv.*`, live run, 47/47 total); independently confirmed via `quiver_cli.exe`: escaping path raised `Cannot read_csv: path '../outside.csv' escapes the database directory '...'`. **Caveat:** see human-verification item on WR-01 below — a narrow class of OS-level filesystem errors *outside* these four explicit checks could still leak unwrapped, though it does not affect the four required cases themselves |
| 5 | Non-`,` separator works via option; defaults to `,` | ✓ VERIFIED | `read_csv_options_from_lua` (`src/lua_runner.cpp:908-947`); tests `SemicolonSeparatorReadsCorrectly`, `TabSeparatorProvesOptionIsNotSpecialCased`, `DefaultSeparatorMatchesEmptyOptionsTable` (pass); independently confirmed via `quiver_cli.exe`: `db:read_csv("semi.csv", {separator=";"})` → `{"header":["a","b"],"rows":[["1","2"]]}` |

**Score:** 5/5 truths verified (0 present-but-behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/csv_read.h` | Internal `Options`/`RowSink`/`Reader` declarations, no csv-parser leak | ✓ VERIFIED | Present; `class Reader` with Pimpl; zero `csv_reader` includes |
| `src/csv_read.cpp` | Single `CSVFormat` construction, single row loop, Pattern 1 wrapping | ✓ VERIFIED | Present, 129 lines; `make_format` pins `delimiter`/`variable_columns(KEEP_NON_EMPTY)`/`header_row(0)`; no `chunk_size`/`guess_csv` calls |
| `src/lua_runner.cpp` | `db:read_csv`/`db:read_csv_stream` registrations + shared options decoder | ✓ VERIFIED | Both registered (`:452`, `:480`); `read_csv_options_from_lua` at `:908`; both call `csv_read::Reader` (grep count 2) |
| `bindings/js/src/lua-api.ts` | Literal `db:read_csv`/`db:read_csv_stream` tokens + `{separator=","}` shown literally | ✓ VERIFIED | Both tokens present ≥2x each; `lua-api-sync.test.ts` passes (part of 203/203 JS suite) |
| `tests/test_lua_runner_read_csv.cpp` | `LuaRunner_ReadCsv` suite covering happy path, options, sandbox, catalogue | ✓ VERIFIED | 47 tests, all passing in a live run |
| `cmake/Dependencies.cmake` | csv-parser 5.3.0 FetchContent, threads/SIMD forced off | ✓ VERIFIED | `GIT_TAG 5.3.0`; all 4 cache vars FORCEd; confirmed actually applied in `CMakeCache.txt` and `compile_commands.json` |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|----|--------|---------|
| `src/lua_runner.cpp` | `src/csv_read.h` | both bindings construct `csv_read::Reader` | ✓ WIRED | `grep -c 'csv_read::Reader' src/lua_runner.cpp` = 2 |
| `src/lua_runner.cpp` | `resolve_sandboxed_path` | both bindings call it before options/reader | ✓ WIRED | Confirmed at `:458` and `:488`, in that order relative to options decoding (D-22 ordering) |
| `src/csv_read.cpp` | csv-parser | only TU including `internal/csv_reader.hpp` | ✓ WIRED | `grep -c 'csv_reader' src/lua_runner.cpp` = 0; `src/csv_read.cpp` includes it |
| `bindings/js/src/lua-api.ts` | `src/lua_runner.cpp` | sync test scrapes `bind.set_function` names | ✓ WIRED | `lua-api-sync.test.ts` passes |

### Behavioral Spot-Checks (independent, run by the verifier, not the executor)

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Whole-file read, string cells | `quiver_cli.exe` running a Lua script calling `db:read_csv("data.csv")` on a `code=0012,date=2024-01-15` fixture | `{"header":["code","date"],"r1":["0012","2024-01-15"],"t1":"string"}` | ✓ PASS |
| Non-comma separator | `quiver_cli.exe` running `db:read_csv("semi.csv", {separator=";"})` on `a;b\n1;2` | `{"header":["a","b"],"rows":[["1","2"]]}` | ✓ PASS |
| Sandbox escape | `quiver_cli.exe` running `pcall(function() return db:read_csv("../outside.csv") end)` | `Cannot read_csv: path '../outside.csv' escapes the database directory '...'` | ✓ PASS |
| Full C++ suite | `./build/bin/quiver_tests.exe` | 1157/1157 pass | ✓ PASS |
| Phase-specific suite | `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv*'` | 47/47 pass | ✓ PASS |
| JS binding suite (incl. lua-api-sync gate) | `bindings/js/test/test.bat` | 203/203 pass | ✓ PASS |
| CSV_ENABLE_THREADS actually compiled off | `grep CSV_ENABLE_THREADS build/CMakeCache.txt` + `compile_commands.json` | `CSV_ENABLE_THREADS:BOOL=OFF`, `-DCSV_ENABLE_THREADS=0` on csv-parser TUs | ✓ PASS |
| csv-parser's window formula is unmultiplied when threads are off | Read `build/_deps/csv_parser-src/include/internal/parser/orchestrator.hpp:78-93` | `#else` branch (threads off) returns `chunk_size` verbatim; only the `#if CSV_ENABLE_THREADS` branch multiplies by worker count | ✓ PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| PARSE-01 | 01-01 | Incremental CSV parsing, bounded memory | ✓ SATISFIED | `csv_read::Reader::for_each_row` streams via csv-parser iterator, never materializes beyond the sink's own accumulation choice |
| PARSE-08 | 01-02 | Configurable separator, defaults to `,` | ✓ SATISFIED | `read_csv_options_from_lua` + `Options::separator` |
| PARSE-09 | 01-01 | Bounded window, not tied to CPU count | ✓ SATISFIED | See truth #2 evidence above |
| LUA-01 | 01-01 | `db:read_csv(path)` whole-file, positional | ✓ SATISFIED | Tests + CLI probe |
| LUA-02 | 01-01 | `db:read_csv_stream(path, on_row)` bounded | ✓ SATISFIED | Tests (`StreamFiresOncePerRowWithIndexAndHeader`, `StreamEarlyStopReturnsPartialCount`, etc.) |
| LUA-03 | 01-01/01-03 | Both forms share one parser | ✓ SATISFIED | `grep -c 'csv_read::Reader' src/lua_runner.cpp` = 2; `StreamAndWholeFileReadYieldSameRows` test. **Note:** the plan itself flags the deeper "cannot diverge on any input" claim as an unresolved assumption pending Phase 2's dirty-input matrix — carried forward, not a Phase 1 gap |
| LUA-04 | 01-01/01-03 | Sandboxed like every other file op | ✓ SATISFIED | `resolve_sandboxed_path` reused verbatim; 8 negative + 1 positive tests pass |
| LUA-07 | 01-01 | Every cell a string, no inference | ✓ SATISFIED | Tests + CLI probe |
| LUA-08 | 01-02/01-03 | Errors are Pattern-1, never raw parser/fs/sol2 messages | ⚠️ MOSTLY SATISFIED | All documented catalogue entries 1-9 pass tests; entry 10 (parser-wrapper) proven only at source level, not at runtime (see human-verification item); WR-01 identifies a narrow unwrapped-exception path outside the four required negatives |
| TEST-03 | 01-03 | Sandbox negatives + subdirectory positive | ✓ SATISFIED | 9 tests, all pass |
| DOC-01 | 01-01/01-02 | `lua-api.ts` documents both entry points, literal tokens | ✓ SATISFIED | `lua-api-sync.test.ts` passes |

**No orphaned requirements** — the 11 IDs declared across the three plans exactly match the 11 IDs REQUIREMENTS.md maps to Phase 1.

### Anti-Patterns Found

None. No `TBD`/`FIXME`/`XXX`/`TODO`/`HACK`/`PLACEHOLDER` markers in `src/csv_read.h`, `src/csv_read.cpp`, or the CSV-related additions to `src/lua_runner.cpp`.

### Code Review Findings (01-REVIEW.md) — Cross-Checked Against Current Source

- **WR-01** (warning, unresolved): confirmed still present in `src/csv_read.cpp:54-62` — the three
  `fs::exists`/`fs::is_directory`/`fs::file_size` precondition checks sit above the `try` block at
  line 64, so a rare OS-level `std::filesystem_error` (permission-denied, broken symlink) *not*
  covered by those three explicit checks could reach Lua without the `"Cannot <op>: "` prefix.
  Does **not** affect any of the four required sandbox/file negatives (escaping path, in-memory
  db, missing file, directory-as-path) — those all throw hand-crafted messages directly. Carried
  forward as a human-verification item below rather than a blocking gap, matching the review's own
  disposition (warning, not critical).
- **IN-01, IN-02** (info-level, code-quality only): dead-branch `std::optional` and inconsistent
  quoting in precondition messages. Neither affects correctness or any success criterion — no
  action required for this verification.

## Deviations / Assumptions Carried Forward (documented, non-blocking)

Two edge-probe assumptions were flagged by the plans themselves as "unclassified — review
manually" and deliberately left unresolved rather than silently backstopped:
- **PARSE-08/unclassified**: whether the runtime-configurable separator has any unexamined shape
  edge beyond what was already tested. Judgment call, not a probe result.
- **LUA-03/unclassified**: whether "cannot diverge on any input" is fully proven by the structural
  guarantee (one reader, one format, one loop) plus row-for-row equality on clean input, or whether
  genuinely dirty input (Phase 2's scope) could still expose divergence. Explicitly deferred to
  Phase 2 by the plan's own text.

Neither blocks Phase 1's stated goal; both are pre-flagged for the developer, not newly discovered
here.

## Gaps Summary

No blocking gaps. All 5 stated success criteria are independently verified true in the codebase —
confirmed by build, full test suite (1157/1157 C++, 203/203 JS), the phase-specific suite
(47/47), and manual `quiver_cli.exe` behavioral probes run directly by this verifier (not
executor-reported). The phase goal is achieved.

Two items require human judgment before this is considered fully closed (see
`human_verification` in the frontmatter): whether WR-01's narrow unwrapped-filesystem-exception
path is worth fixing now vs. tracking as follow-up debt, and whether D-22 catalogue entry 10's
runtime firing should be confirmed on a non-Windows CI matrix. Both are explicitly acknowledged,
non-hidden gaps already surfaced by the phase's own code review and its own SUMMARY
(`human_judgment: true`) — this verification does not discover anything new, it confirms they are
real, still present in the current source, and correctly scoped as non-blocking to the phase goal.

---

*Verified: 2026-09-15T14:10:00Z*
*Verifier: Claude (gsd-verifier)*
