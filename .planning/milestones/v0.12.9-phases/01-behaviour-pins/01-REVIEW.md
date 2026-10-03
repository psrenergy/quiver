---
phase: 01-behaviour-pins
reviewed: 2026-10-02T23:56:23Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - bindings/js/AGENTS.md
  - bindings/js/test/lua-api-sync.test.ts
  - tests/AGENTS.md
  - tests/CMakeLists.txt
  - tests/test_lua_binary.cpp
  - tests/test_lua_runner_csv_export.cpp
  - tests/test_lua_runner_csv_import.cpp
  - tests/test_lua_runner_lifecycle.cpp
  - tests/test_lua_runner_read_csv.cpp
  - tests/test_lua_runner_write_csv.cpp
findings:
  critical: 0
  warning: 2
  info: 2
  total: 4
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-10-02T23:56:23Z
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

I reviewed `git diff e4b388b..HEAD` for the ten listed files and checked each new pin against the code it
guards in `src/lua_runner.cpp`, `src/csv/csv_write.cpp` and `include/quiver/lua_runner.h`.

Every new ordering pin matches the evaluation order in the source and would fail if that order changed:
- `open_file` checks the mode before resolving the path.
- `export_csv`, `import_csv`, `read_csv`, `read_csv_stream` and `write_csv` resolve the path before
  decoding options.
- `read_csv_stream` checks `on_row` before resolving the path.
- `write_row` checks the row type, then the closed state, then the cells.

Both sides of each ordering pin have their own standalone test, so neither error can pass vacuously. The
exception is `on_row must be a function`, but its order pin already proves that message exists.

The width-cap messages match `csv_max_integer_key`. The `[2e6]` key really is normalized to an integer
by Lua 5.4 before sol2 sees it. `sizeof(std::unique_ptr<Impl>) == sizeof(void*)` holds for the default
deleter on every supported ABI. The lifecycle fixture uses its own sandbox for each test, so the fixed
file names cannot collide.

I found no blockers. Both warnings are about how strong the pins are, not about behaviour that is wrong.

I did not re-raise the D-04 exclusions or the fixture-name clang-tidy convention.

## Narrative Findings (AI reviewer)

## Warnings

### WR-01: Usertype guard is a hardcoded, duplicated allowlist, so a new usertype's methods go unchecked

**File:** `bindings/js/test/lua-api-sync.test.ts:60` and `:81`
**Issue:** The new meta-guard (`["BinaryFile", "BinaryMetadata", "Expression", "CsvWriter"]`, line 60)
and the `:<name>(` coverage loop (line 81) each carry their own copy of the same type list.
`usertypeMethods` already contains every `new_usertype<T>` it parsed.
- If a `new_usertype<T>` is added, for example by the upcoming `lua_runner.cpp` split, the parser records
  its methods, but no assertion ever checks that they are documented. That is the same silent pass this
  phase set out to close.
- If someone edits one of the two lists and not the other, they drift apart without any error.

The `Database` usertype is not affected. The "no documented name has been removed" test fails loudly if
that usertype parses to nothing.
**Fix:** Build the list once from the parse, and keep the floor as a separate check:
```ts
const DOCUMENTED_USERTYPES = [...usertypeMethods.keys()].filter((t) => t !== "Database");
// meta-guard
expect(DOCUMENTED_USERTYPES.sort()).toEqual(
  expect.arrayContaining(["BinaryFile", "BinaryMetadata", "CsvWriter", "Expression"]),
);
expect(DOCUMENTED_USERTYPES.filter((t) => !usertypeMethods.get(t)?.size)).toEqual([]);
// coverage loop
for (const type of DOCUMENTED_USERTYPES) { ... }
```

### WR-02: The lifecycle CSV check proves the writer was flushed, not that it was closed, and relies on `std::ofstream` buffering

**File:** `tests/test_lua_runner_lifecycle.cpp:39-40`
**Issue:** `expect_handles_closed` checks the binary handle directly (`g:is_open()`). For the CSV writer it
only checks that `name.csv` reads back one row. That check is meaningful only while
`csv_write::Writer::write_row` leaves a 2-byte record sitting in the `ofstream` buffer
(`src/csv/csv_write.cpp:149`, no flush).

Suppose the writer starts flushing after each row, for example for crash-safety. Then the file reads back
correctly even when the registry never closed the writer, and the CSV half of all four lifecycle pins
passes without testing anything. That is exactly what these pins are meant to detect when the move breaks
the registry wiring.
**Fix:** Also check the writer's closed state directly, so the pin does not depend on buffering:
```lua
local ok, err = pcall(w.write_row, w, { 'y' })
assert(not ok and tostring(err):find('already closed', 1, true), 'csv writer outlived its run()')
```
Put this check before the `db:read_csv` call, which can stay as the flush check.

## Info

### IN-01: `open_libraries(` count also matches comments and misses other spellings

**File:** `bindings/js/test/lua-api-sync.test.ts:65`
**Issue:** `CPP.match(/open_libraries\(/g)` counts every occurrence, comments included. A future comment
such as "see `open_libraries(` above" would fail the test with a misleading message. A second call
written as `open_libraries (`, or one split across lines between the name and the `(`, would not be
counted. Today the source has exactly one occurrence, so the test is correct now.
**Fix:** Match only call shapes, e.g. `/\.open_libraries\s*\(/g`, and accept that comments are a known
limitation, or strip `//` comments before counting.

### IN-02: Single-quoted gtest filter does nothing under cmd.exe

**File:** `tests/AGENTS.md:147`
**Issue:** `--gtest_filter='Lua*'` works in Git Bash. In cmd.exe the single quotes are passed through
literally, so the filter matches zero tests and the run still exits 0. The project memory already
records that quoted filters break through `cmd //c`. The quoting predates this phase, but this phase
edited the line.
**Fix:** Write it as `--gtest_filter=Lua*` (no quotes are needed for this pattern in either shell), or
use double quotes.

---

_Reviewed: 2026-10-02T23:56:23Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

## Resolution

- **WR-01:** fixed in `7155e24` (usertype list derived from the parse, four known types as a floor; mutation-checked).
- **WR-02:** fixed in `7155e24` (lifecycle pins assert "already closed" on a further `write_row`).
- **IN-01:** no change. A comment mentioning `open_libraries(` makes the count fail loudly, not pass vacuously, and
  clang-format never emits `open_libraries (`.
- **IN-02:** no change. `--gtest_filter='Lua*'` is correct in bash and PowerShell, the shells the repo docs use; the
  quoting predates this phase (only the filter value changed).
