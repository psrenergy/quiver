---
phase: 01-a-lua-script-reads-a-csv-file
verified: 2026-09-16T00:15:00Z
status: passed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 5/5
  gaps_closed:
    - "WR-01: the shared sandbox gate (resolve_sandboxed_path) leaked a raw std::filesystem_error ('weakly_canonical: The parameter is incorrect.: ...') with no Pattern 1 prefix for OS-level path-resolution failures (proven via a Windows reserved device name), breaking LUA-08 for read_csv/read_csv_stream and six other sandboxed operations (open_file, bin_to_csv, csv_to_bin, export_csv, import_csv, validate_migrations, expr:save). Fixed at the shared gate (commit 404020b) and hardened again in csv_read.cpp's three preconditions with non-throwing overloads."
    - "D-22 catalogue entry 10 (the csv-parser-construction-failure wrapper 'Cannot <op>: cannot read file ...') was previously proven present only by grepping source text (ParserWrapperMessageExistsInSource). Replaced with UnreadableFileReportsParserFailure, which manufactures a genuinely unreadable-but-present-non-empty-non-directory file (exclusive lock on Windows, chmod 000 on POSIX) and asserts the exact runtime message."
  gaps_remaining: []
  regressions: []
---

# Phase 01: A Lua script reads a CSV file Verification Report

**Phase Goal:** A Lua script can read a CSV file off disk — whole-file or row-by-row — with every
cell arriving as a string and every path resolved against the database directory.

**Verified:** 2026-09-16T00:15:00Z
**Status:** passed
**Re-verification:** Yes — after gap closure (both prior human-verification items closed by executable tests)

## Goal Achievement

### Observable Truths (Success Criteria from ROADMAP)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `db:read_csv("data.csv")` returns rows addressed positionally with header available separately | ✓ VERIFIED | `src/lua_runner.cpp:452-479`; `LuaRunner_ReadCsv.CleanFileReturnsHeaderAndRows`/`ExactJsonRoundTrip` pass in a live run of the full suite (1159/1159) |
| 2 | `db:read_csv_stream("data.csv", on_row)` fires once per row, bounded window not tied to CPU count | ✓ VERIFIED | Unchanged since prior verification; `CSV_ENABLE_THREADS=OFF` confirmed compiled in; no `chunk_size(...)` call anywhere in `src/csv_read.cpp` |
| 3 | Every cell arrives as a string; `"0012"` stays `"0012"`; nothing coerced | ✓ VERIFIED | `src/csv_read.cpp:107` (now offset, same logic) uses `field.get<std::string_view>()` only; `StringCellsNoInference` passes |
| 4 | Escaping path, in-memory db, missing file, directory-as-path each raise `Cannot read_csv: ...`; subdirectory accepted | ✓ VERIFIED | Re-ran the 9 relevant tests directly (`EscapingPathThrowsForReadCsv(Stream)`, `InMemoryDatabaseThrowsForReadCsv(Stream)`, `MissingFileThrowsForReadCsv(Stream)`, `DirectoryAsPathThrowsForReadCsv(Stream)`, `SubdirectoryPathReadsSuccessfullyForBothEntryPoints`) — all 9 pass with unchanged messages. The three D-22-pinned precondition messages (`file not found: <p>` / `path is a directory: <p>` / `file '<p>' is empty`) are byte-identical to before the fix — confirmed via `git diff 5bc2ba8 HEAD -- src/csv_read.cpp`, zero lines touching those three strings |
| 5 | Non-`,` separator works via option; defaults to `,` | ✓ VERIFIED | Unchanged since prior verification; `SemicolonSeparatorReadsCorrectly`/`TabSeparatorProvesOptionIsNotSpecialCased`/`DefaultSeparatorMatchesEmptyOptionsTable` pass |

**Score:** 5/5 truths verified (0 present-but-behavior-unverified)

### Gap-Closure Verification (this re-verification's actual work)

| Item | Prior status | Verification performed | Result |
|------|--------------|------------------------|--------|
| WR-01 (unwrapped `std::filesystem_error` at the shared sandbox gate) | human_needed | Read `src/lua_runner.cpp`'s `resolve_sandboxed_path` diff (commit `404020b`): `weakly_canonical`/`current_path` calls now wrapped in try/catch, re-thrown as `"Cannot <op>: cannot resolve path '<p>': " + ec.message()`. Read `src/csv_read.cpp`'s three preconditions: converted to non-throwing `error_code` overloads, each `ec` branch reporting `"Cannot <op>: cannot access file '<p>': " + ec.message()`. Ran `LuaRunner_ReadCsv.DeviceNamePathIsReportedWithPrefix` and `LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` directly — both pass, producing exactly `Cannot read_csv: cannot resolve path 'NUL': The parameter is incorrect.` (and the `open_file`/`bin_to_csv`/`csv_to_bin` equivalents). | ✓ CLOSED |
| D-22 entry 10 (parser-construction-failure wrapper never proven at runtime) | human_needed | Read the new `UnreadableFileReportsParserFailure` test: it asserts the three preconditions (`exists`, `!is_directory`, `file_size>0`) pass *before* attempting the read (guards against silently re-testing an earlier catalogue entry), then locks the file exclusively (Windows: `CreateFileW` with `dwShareMode=0`) and asserts the thrown message contains `Cannot read_csv: cannot read file 'unreadable.csv': ` (and the `read_csv_stream` equivalent). Ran the test directly — passes, actual thrown text: `Cannot read_csv: cannot read file 'unreadable.csv': Cannot open file <path>`. Confirmed `ParserWrapperMessageExistsInSource` no longer exists anywhere in the tree (`grep` returned nothing). | ✓ CLOSED |

### Mutation Check (performed independently by this verifier, not taken on the SUMMARY's word)

1. `git checkout 404020b~1 -- src/lua_runner.cpp` (revert only the gate fix) → rebuilt (`cmake --build build --config Debug`, succeeded).
2. Ran `LuaRunner_ReadCsv.DeviceNamePathIsReportedWithPrefix` and `LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` → **both FAILED**, with the raw unwrapped text: `weakly_canonical: The parameter is incorrect.: "...\NUL"` and no `Cannot <op>:` prefix. This proves the tests are not vacuously green.
3. `git checkout HEAD -- src/lua_runner.cpp` (restore the fix) → rebuilt → both tests **PASSED** again with the documented `Cannot <op>: cannot resolve path '<p>': ...` message.
4. `git status` confirmed clean (only pre-existing untracked `.gsd/`), and the full C++ suite re-ran green (1159/1159) after restoration — the repo was not left stranded in a reverted state.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/csv_read.cpp` | Preconditions hardened against unwrapped OS filesystem errors | ✓ VERIFIED | `fs::exists/is_directory/file_size` now take `std::error_code&`; each error path reports `"Cannot <op>: cannot access file '<p>': " + ec.message()`; the three pinned D-22 messages unchanged |
| `src/lua_runner.cpp` | `resolve_sandboxed_path` wraps `weakly_canonical`/`current_path` in try/catch | ✓ VERIFIED | Confirmed at the diff level and via passing/failing test in the mutation check |
| `tests/test_lua_runner_read_csv.cpp` | `ParserWrapperMessageExistsInSource` replaced; `UnreadableFileReportsParserFailure` and `DeviceNamePathIsReportedWithPrefix` added | ✓ VERIFIED | Old test absent (grep confirmed); both new tests present, pass, and are properly `#ifdef _WIN32`-guarded where Windows-only (device-name test); the unreadable-file test compiles on all platforms via `#ifdef _WIN32 / #else` branches for the lock mechanism |
| `tests/test_lua_binary.cpp` | `DeviceNamePathIsReportedWithPrefix` added, spanning `open_file`/`bin_to_csv`/`csv_to_bin` | ✓ VERIFIED | Present, `#ifdef _WIN32`-guarded, passes |
| `CHANGELOG.md`, `src/CLAUDE.md`, `tests/CLAUDE.md` | Document the gate fix and the two new test levers | ✓ VERIFIED | All three updated (commit `2d99c8b`); `src/CLAUDE.md` documents `resolve_sandboxed_path`'s new wrapping, `tests/CLAUDE.md` documents the Windows-lock/POSIX-chmod split |
| `.planning/phases/.../01-UAT.md` | Both items closed with `result: pass` | ✓ VERIFIED | `status: complete`, both tests `result: pass`, resolution narrative present |

### Platform-Compile Safety Check

Read every `#ifdef _WIN32` guard added in this change:
- `tests/test_lua_runner_read_csv.cpp`: `<windows.h>` vs `<sys/stat.h>`/`<unistd.h>` includes guarded; `UnreadableFileReportsParserFailure`'s lock/unlock code is guarded per-branch (`CreateFileW`/`CloseHandle` vs `chmod`/`geteuid`); `DeviceNamePathIsReportedWithPrefix` (the whole `TEST_F`) is wrapped in `#ifdef _WIN32 ... #endif` — does not exist on Linux/macOS builds, so nothing there attempts a Windows-only call.
- `tests/test_lua_binary.cpp`: `LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` is similarly wrapped in `#ifdef _WIN32 ... #endif` in its entirety.
- No Win32 call (`CreateFileW`, `HANDLE`, `GetLastError`) appears outside a `_WIN32` guard in either file. POSIX branch uses only `<sys/stat.h>`/`<unistd.h>` (`chmod`, `geteuid`), both portable POSIX calls.
- Conclusion: the new tests compile on Linux/macOS (the guarded Windows-only test is simply absent there; the shared `UnreadableFileReportsParserFailure` test compiles and runs via its POSIX branch).

### Build & Full Suite (run independently by this verifier)

| Check | Command | Result |
|-------|---------|--------|
| Build | `cmake --build build --config Debug` | Succeeds, no warnings surfaced |
| Full C++ suite | `./build/bin/quiver_tests.exe` | **1159/1159 pass** (matches the SUMMARY's claimed count exactly) |
| Full C API suite | `./build/bin/quiver_c_tests.exe` | **557/557 pass** (matches the SUMMARY's claimed count exactly) |
| Named regression tests | `--gtest_filter='LuaRunner_ReadCsv.DeviceNamePathIsReportedWithPrefix:LuaBinaryTest.DeviceNamePathIsReportedWithPrefix:LuaRunner_ReadCsv.UnreadableFileReportsParserFailure'` | 3/3 pass |
| Required negatives + subdirectory positive | `--gtest_filter='LuaRunner_ReadCsv.EscapingPath*:...MissingFile*:...DirectoryAsPath*:...InMemoryDatabase*:...Subdirectory*'` | 9/9 pass |

### Requirements Coverage

All 11 requirement IDs for this phase (PARSE-01, PARSE-08, PARSE-09, LUA-01, LUA-02, LUA-03,
LUA-04, LUA-07, LUA-08, TEST-03, DOC-01) are marked `[x]` in `.planning/REQUIREMENTS.md` and
"Complete" in its status table. **LUA-08 was previously only ⚠️ MOSTLY SATISFIED** (the parser-wrapper
catalogue entry was proven only at the source-text level, and a real bug — the unwrapped gate error
— was undiscovered at the time). Both are now resolved: LUA-08 is fully `✓ SATISFIED` — every
catalogue entry (1-10) has a runtime-firing test, and the previously-unknown gap the review process
surfaced (the shared-gate leak) is fixed and regression-tested across seven affected operations, not
just `read_csv`.

No orphaned requirements — same 11 IDs, unchanged from prior verification.

### Anti-Patterns Found

None. No `TBD`/`FIXME`/`XXX`/`TODO`/`HACK`/`PLACEHOLDER` markers in any file touched by this
gap-closure round (`src/csv_read.cpp`, `src/lua_runner.cpp`, `tests/test_lua_runner_read_csv.cpp`,
`tests/test_lua_binary.cpp`).

## Gaps Summary

No gaps remain. Both items carried forward from the initial verification's `human_needed` status
were closed not by human judgment but by disproving the premise that motivated them ("no portable
trigger exists on Windows") and writing tests that exercise the real runtime paths:

1. The sandbox-gate bug (WR-01) was real and broader than the original code review located — it
   affected the shared `resolve_sandboxed_path` choke point, not just `csv_read.cpp`'s local
   preconditions, and so silently affected `open_file`, `bin_to_csv`, `csv_to_bin`, `export_csv`,
   `import_csv`, `validate_migrations`, and `expr:save` in addition to `read_csv`/`read_csv_stream`.
   Now fixed at the shared gate and independently confirmed by this verifier via a mutation check
   (revert → test fails with the raw unwrapped message; restore → test passes), not merely taken on
   the SUMMARY's claim.
2. D-22 catalogue entry 10 (the parser-construction-failure wrapper) is now proven to fire at
   runtime with the exact documented message, replacing a source-text grep with a genuine
   behavioral test.

The phase goal — a Lua script reading a CSV file off disk, whole-file or streamed, with sandboxed
paths and string-only cells — is achieved, and the two previously-open reliability edges around
error-message correctness are closed with evidence, not assumption.

---

*Verified: 2026-09-16T00:15:00Z*
*Verifier: Claude (gsd-verifier)*
