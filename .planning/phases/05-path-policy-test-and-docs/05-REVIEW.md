---
phase: 05-path-policy-test-and-docs
reviewed: 2026-10-03T18:44:48Z
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
  warning: 0
  info: 3
  total: 3
status: issues_found
---

# Phase 05: Code Review Report (iteration 2)

**Reviewed:** 2026-10-03T18:44:48Z
**Depth:** standard
**Files Reviewed:** 32 (scope carried from iteration 1; only `src/csv/csv_read.cpp` and `tests/test_lua_runner_read_csv.cpp` changed since, in commits `3c313dd` and `e2eca2a`)
**Status:** issues_found

## Summary

This is a re-review of the fixer's changes in `631a1d5..HEAD`. Those changes touch only the two files named above. Every other file is unchanged since iteration 1, so its findings carry forward as written.

**WR-01: resolved.**
- No catalogue ordinals, `entry N` or `#N` references are left in either file. A grep for `catalog`, `entry [0-9]`, `#[0-9]`, `tenth` and `ordinal` finds nothing in `csv_read.cpp` or `test_lua_runner_read_csv.cpp`. A repo-wide grep finds no `tenth entry` or `entry 10` anywhere outside `.planning/`.
- `csv_read.cpp:73-75` now calls the three checks "the existence, type and size preconditions". It no longer calls them a catalogue or claims the list is complete.
- `csv_read.cpp:83-85` and `:136-137` name the pinning tests instead. All four cited tests exist:
  - `MissingFileThrowsForReadCsv` (line 1215)
  - `DirectoryAsPathThrowsForReadCsv` (line 1237)
  - `EmptyFileThrows` (line 317)
  - `HeaderRowPastEndOfFileThrowsExactMessage` (line 346)
- Each of those tests asserts the full message text for its throw site. For example, `"Cannot read_csv: file not found: missing.csv"` matches line 96, and `"Cannot read_csv: header row 99 not found in file 'three.csv'"` matches line 139. So rewording any of the four messages fails its cited test.
- The test-file references are now resolvable:
  - `entry 10` became "the csv-parser wrapper's "cannot read file"".
  - `#1/#3` and `#2/#7` became the named errors (in-memory before options, escape before not-found).
  - The section headers no longer say "catalogue".

**IN-02: resolved.** The doubled "shape" is gone (`test_lua_runner_read_csv.cpp:231-232`).

**Invariants:**
- The diff contains only C++ `//` comment lines. No string literal, test name, `R"(...)"` Lua body or assert message changed, so no embedded Lua script's line count changed. The one +1 line net is in a C++ comment at line 1337, outside any raw string.
- No planning IDs were reintroduced.

**New problems in the changed hunks:** none. One wording point is not worth a finding: `csv_read.cpp:83` says "pinned exactly". `expect_lua_error` is a substring match, so a suffix appended to a message would not be caught. A reword of the existing text would be, which is what "do not reword them" guards against.

The three open Info findings live in files the fixer did not touch.

## Resolved

### WR-01: The rewritten Reader comments contradicted each other about what "the catalogue" is — RESOLVED

**File:** `src/csv/csv_read.cpp:73-85`, `src/csv/csv_read.cpp:136-137`, `tests/test_lua_runner_read_csv.cpp:1286,1337-1338,1380-1398`
**Resolution:** Commit `3c313dd` makes these changes:
- It drops the "tenth entry" ordinal and the "catalogue" framing.
- It describes lines 73-75 as the existence/type/size preconditions.
- It names the exact pinning tests at both comment sites.
- It replaces every `entry 10` / `#N` reference in the test file with the named error.

All cited test names exist.

### IN-02: Doubled word left by the rewrite — RESOLVED

**File:** `tests/test_lua_runner_read_csv.cpp:231-232`
**Resolution:** Commit `e2eca2a` fixed it. The comment now reads "...mirroring the real Maranhao file's shape. BOM stripping must hold under...".

## Info

### IN-01: The rule against a duplicate symbol in a static link describes a failure that archive linking cannot produce

**File:** `tests/CMakeLists.txt:55-57`, `tests/AGENTS.md:61-63`
**Issue:** Both places say to keep `path_policy.cpp` to one function, "or a static (`QUIVER_BUILD_SHARED=OFF`) link defines a symbol twice". Linkers do not work that way.
- `quiver_tests` compiles its own copy of `path_policy.cpp`, so that copy defines every symbol the file defines.
- Neither MSVC `link` nor GNU `ld` extracts a member from `libquiver.a`/`quiver.lib` for a symbol that is already defined. So the archive's `path_policy.o` is never pulled in, however many functions it contains.

What actually limits the test copy is that it is compiled without `quiver`'s PRIVATE compile definitions (`QUIVER_EXPORTS`, the `SOL_*` set). That is an ODR divergence hazard if `path_policy.cpp` ever depends on them. No CI job builds the static configuration (`QUIVER_BUILD_SHARED` appears in no workflow), so neither claim gets checked.
**Fix:** Replace the reason: "Keep path_policy.cpp free of sol2 and of anything that depends on quiver's PRIVATE compile definitions: the test copy is compiled without them."

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

_Reviewed: 2026-10-03T18:44:48Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
