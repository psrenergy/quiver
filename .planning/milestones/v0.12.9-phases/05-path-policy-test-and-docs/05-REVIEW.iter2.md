---
phase: 05-path-policy-test-and-docs
reviewed: 2026-10-03T18:31:23Z
depth: standard
files_reviewed: 32
files_reviewed_list:
  - .gitattributes
  - AGENTS.md
  - CHANGELOG.md
  - bindings/dart/AGENTS.md
  - bindings/js/AGENTS.md
  - bindings/js/src/lua-api.ts
  - bindings/js/test/database-csv.test.ts
  - bindings/julia/AGENTS.md
  - bindings/python/AGENTS.md
  - bindings/python/tests/test_database_query.py
  - src/AGENTS.md
  - src/CMakeLists.txt
  - src/c/AGENTS.md
  - src/csv/csv_read.cpp
  - src/csv/csv_read.h
  - src/csv/csv_write.cpp
  - src/csv/csv_write.h
  - src/database_describe.cpp
  - src/lua_runner/internal.h
  - src/lua_runner/path_policy.cpp
  - src/lua_runner/path_policy.h
  - src/ui_metadata.cpp
  - tests/AGENTS.md
  - tests/CMakeLists.txt
  - tests/test_c_api_database_csv_export.cpp
  - tests/test_c_api_database_time_series_row.cpp
  - tests/test_database_time_series_row.cpp
  - tests/test_database_ui_metadata.cpp
  - tests/test_database_update.cpp
  - tests/test_lua_runner_read_csv.cpp
  - tests/test_lua_runner_write_csv.cpp
  - tests/test_sandboxed_path.cpp
findings:
  critical: 0
  warning: 1
  info: 4
  total: 5
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-10-03T18:31:23Z
**Depth:** standard
**Files Reviewed:** 32
**Status:** issues_found

## Summary

I reviewed the changes in `737b6af..HEAD`, excluding `.planning/`.

**TEST-01:** The work is sound. `path_policy.h` is sol2-free. `internal.h` now includes it instead of re-declaring the function. `quiver_tests` builds and links, and all 11 `SandboxedPathTest` cases pass on this MSVC Debug shared build, including the symlink case and the `_WIN32` device-name case. Every expected string matches the throw text in `path_policy.cpp` exactly. The suite name stays outside the `Lua*` filter as documented.

**DOC-01:** It changes comments and test messages only. I filtered every changed non-comment line in `tests/` and `src/`. The only non-comment changes are:
- the assert message strings in the Lua scripts;
- the `FAIL()` text;
- the new header and its CMake wiring.

Every Lua-script edit replaces one line with one line, so no embedded script's line count changed. A repo-wide grep finds no leftover planning IDs (`D-NN`, `PARSE-NN`, `LUA-NN`, `WRITE-NN`, `FMT-NN`, `TEST-NN`, `SAFE-NN`, `READ-NN`, `T-0N-NN`, `RESEARCH.md`, `CONTEXT.md`) in any source, test or AGENTS.md file.

**Test names:** Every test name cited in the new comments and docs exists in the suite, 22 names checked. Among them: `EnergiaRegressionJunkRowAboveUnitsRowBelowHeader`, `GdRegressionQuotedCommaAndEnglishMonthNames`, the five `pinned by` names in `csv_write.cpp`, `NoUiDirReportsUnchanged`, the four empty-array Lua tests, `Database.UpdateElementEmptyArrayClearsRows`, `Database.CreateElementWithEmptyArraySkipsSilently`, `LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` and `ReopeningSamePathTruncatesExistingContent`.

**DOC-02/03/04:** I checked the factual claims against the code, and most by running them:
- The CHANGELOG's `quiver.metadata_from_element` messages match what `quiver_cli` prints for each case. That covers empty labels, an empty dimension_sizes, an empty dimensions, both empty, and empty `time_dimensions`/`frequencies` being accepted.
- The `options must be a table, got string` text in `lua-api.ts` matches what `quiver_cli` prints.
- The sol2 flag names match `src/CMakeLists.txt`.
- The list of messages without the `got` suffix in `src/AGENTS.md` matches `csv.cpp`, `db_core.cpp`, `internal.h` and `db_write.cpp`.
- The section title cited for commit `f92af8d` is real.
- `lua-api-sync.test.ts` passes (6/6).

The findings below are documentation accuracy problems and one test-coverage gap. None of them changes behaviour.

## Warnings

### WR-01: The rewritten Reader comments now contradict each other about what "the catalogue" is

**File:** `src/csv/csv_read.cpp:73-83` and `src/csv/csv_read.cpp:134-135`
**Issue:** The DOC-01 rewrite removed the D-22 reference without saying what the catalogue is.
- Lines 73-75 now define "the constructor's Pattern 1 catalogue" as three checks, in order: not-found, then directory, then empty.
- Line 83 says "the three messages below are part of that catalogue; do not reword them".
- Lines 134-135 then call header-not-found "the tenth entry in this constructor's Pattern 1 catalogue".

A three-entry catalogue has no tenth entry. The ordinal pointed at the old D-22 list, which also counted the options-decoder errors in `src/lua_runner/csv.cpp`, and that list is no longer written down anywhere in the source. Before this change, the read_csv test file already called a different message entry 10 (the csv-parser wrapper). With the IDs gone, nobody can check either numbering.

Lines 73-75 also leave out messages the constructor does raise: the `cannot access file` / OS-refused branches, the csv-parser wrapper (`cannot read file`), and header-not-found. So "the catalogue, in evaluation order" is also incomplete. A maintainer who follows "do not reword them" cannot tell which messages are pinned.
**Fix:** Drop the ordinal and name the pinning test instead:
```cpp
// header by design, and that is not an error. Pinned by
// LuaRunner_ReadCsv.HeaderRowPastEndOfFileThrowsExactMessage; do not reword it.
```
On lines 73-75, describe the three checks as the existence/type/size preconditions, not as "the catalogue". If the catalogue matters, list it once, as `csv_write.cpp` does.

## Info

### IN-01: The rule against a duplicate symbol in a static link describes a failure that archive linking cannot produce

**File:** `tests/CMakeLists.txt:55-57`, `tests/AGENTS.md:61-63`
**Issue:** Both places say to keep `path_policy.cpp` to one function, "or a static (`QUIVER_BUILD_SHARED=OFF`) link defines a symbol twice". Linkers do not work that way.
- `quiver_tests` compiles its own copy of `path_policy.cpp`, so that copy defines every symbol the file defines.
- Neither MSVC `link` nor GNU `ld` extracts a member from `libquiver.a`/`quiver.lib` for a symbol that is already defined. So the archive's `path_policy.o` is never pulled in, however many functions it contains.

What actually limits the test copy is that it is compiled without `quiver`'s PRIVATE compile definitions (`QUIVER_EXPORTS`, the `SOL_*` set). That is an ODR divergence hazard if `path_policy.cpp` ever depends on them. No CI job builds the static configuration (`QUIVER_BUILD_SHARED` appears in no workflow), so neither claim gets checked.
**Fix:** Replace the reason: "Keep path_policy.cpp free of sol2 and of anything that depends on quiver's PRIVATE compile definitions: the test copy is compiled without them."

### IN-02: Doubled word left by the rewrite

**File:** `tests/test_lua_runner_read_csv.cpp:231-232`
**Issue:** The comment reads "mirroring the real Maranhao file's shape / shape. BOM stripping must hold...". The rewrite removed `(D-22). PARSE-05` but kept both halves of "shape".
**Fix:** `// BOM + a junk title line above the real header, mirroring the real Maranhao file's shape. BOM stripping must hold under ...`

### IN-03: One "pinned by" citation pins only part of its message

**File:** `src/csv/csv_write.cpp:35-36`
**Issue:** The new header says that an entry followed by a "pinned by" line "names one test that asserts it". The entry `"Cannot write_row: row <N> cell #<M> is not a finite number"` cites `LuaRunner_WriteCsvErrors.NonFiniteNumberCellIsPrefixedWriteRowError`. That test asserts only the `Cannot write_row: ` prefix and the `is not a finite number` suffix. The `row <N> cell #<M>` part is checked only loosely by `LuaRunner_WriteCsv.NonFiniteNumberCellThrowsNamingWriteRowAndRowOrdinal`, through `find("3")` and `find("#1")`. The parent-directory entry is honestly labelled "(prefix only)", and this one needs the same qualifier.
**Fix:** Write `pinned (prefix and reason, not the row/cell numbers) by ...`, or tighten the test to `"row 1 cell #1 is not a finite number"`.

### IN-04: No unconditional SandboxedPathTest case covers a sibling directory whose name starts with the sandbox path

**File:** `tests/test_sandboxed_path.cpp:72-76`
**Issue:** The classic containment bypass is a sibling directory whose name begins with the root's name, such as `<sandbox>_evil/x.csv`. A naive string-prefix check accepts it. The code is correct: `lexically_relative` compares whole components. But the new unit suite exercises this only through `SymlinkPointingOutsideIsRejected`, whose target happens to be `<sandbox>_outside`. That test calls `GTEST_SKIP` wherever a directory symlink cannot be created, which is typical on Windows without Developer Mode. `AbsolutePathOutsideIsRejected` uses the parent directory, which a prefix check would also reject.
**Fix:** Add one case that does not depend on symlinks:
```cpp
TEST_F(SandboxedPathTest, SiblingSharingTheRootPrefixIsRejected) {
    quiver::Database db(db_path(), quiet());
    const auto sibling = sandbox.string() + "_evil/x.csv";
    EXPECT_EQ(error_of(db, "write_csv", sibling), escapes("write_csv", sibling));
}
```

---

_Reviewed: 2026-10-03T18:31:23Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
