---
phase: 05-ragged-rows-and-forgotten-closes
reviewed: 2026-09-17T00:00:00Z
depth: standard
files_reviewed: 6
files_reviewed_list:
  - src/lua_runner.cpp
  - src/csv_write.cpp
  - tests/test_lua_runner_write_csv.cpp
  - CLAUDE.md
  - src/CLAUDE.md
  - bindings/js/src/lua-api.ts
findings:
  critical: 0
  warning: 0
  info: 2
  total: 2
status: clean
---

# Phase 5: Code Review Report

**Reviewed:** 2026-09-17
**Depth:** standard
**Files Reviewed:** 6 (plus `CHANGELOG.md`, which was also diffed for accuracy)
**Status:** clean

## Summary

Reviewed FMT-07 (header-width enforcement in `db:write_row`), WRITE-06 (`GcGuard`'s
`collect_garbage()` flush in `LuaRunner::run`), the 9+2 new tests in
`tests/test_lua_runner_write_csv.cpp`, and the four documentation files (`CLAUDE.md`,
`src/CLAUDE.md`, `CHANGELOG.md`, `bindings/js/src/lua-api.ts`) against the locked decisions in
`05-CONTEXT.md` (D-41 through D-53).

**FMT-07** (`src/lua_runner.cpp:713-745`): the pad/throw block sits after the WRITE-05
closed-writer guard and before `Writer::write_row` is invoked, exactly as D-41/D-44 require.
`header_width == 0` disables enforcement unambiguously (D-42). The reject branch (`cells.size() >
header_width`) is checked strictly before the pad branch, so a long row is never truncated.
`cells.resize(header_width)` default-constructs new elements as empty strings, matching the
declared padding semantics, and can only grow the vector (the only call site is behind `cells.size()
< header_width`). The throw message — `"Cannot write_row: row <N> has <M> cells but header
declares <W>"` — uses the same `"Cannot " + operation + ": row " + N + ...` shape as the
neighbouring FMT-05 message and reuses `next_row_index`/`row_index` with identical 1-based,
header-never-counted, ordinal-unchanged-on-reject semantics (verified by reading FMT-05's block at
`lua_runner.cpp:326-335` and the D-43 test `RowLongerThanHeaderThrowsNamingOrdinalAndCounts`, which
asserts ordinal 3 after two accepted rows). `src/csv_write.cpp`'s diff is confirmed comment-only
(one line added to the pinned TEST-12 message catalogue; no functional line touched).

**WRITE-06** (`src/lua_runner.cpp:2190-2220`): `GcGuard` is declared as the first statement in
`run()`, strictly before `auto result = impl_->lua.safe_script(...)`. C++ destroys stack locals in
reverse declaration order, so `result` (holding the live Lua stack reference) is destroyed before
`gc_guard` fires `collect_garbage()`, matching D-46's ordering requirement. The guard fires on all
three exits examined: the invalid-`result`/throw path (`run()` throws at line 2210, unwinding
destroys `result` then `gc_guard`), the empty-return path (line 2213-2215), and the normal
JSON-encoded return (line 2217-2219) — there is exactly one `collect_garbage()` call in the file,
matching D-48. The destructor's `lua.collect_garbage()` operates on `impl_->lua`, a `sol::state`
value member of `Impl` that outlives the call (no dangling-reference risk).

**Tests**: every new assertion in the FMT-07 and WRITE-06 test blocks round-trips through
`db:read_csv` rather than inspecting raw bytes or the emitted string directly, consistent with the
project's stated weak-test trap avoidance. The two TEST-11 tests correctly keep the `LuaRunner`
alive and undestroyed across both `lua.run()` calls (the ROADMAP criterion 4 trap called out in
`05-CONTEXT.md`), and the observed-byte-count values are attached only to `FAIL()` diagnostics, never
to the pass/fail assertion. `RejectedLongRowLeavesEarlierRowsOnDisk` correctly calls `w:close()`
after the caught `pcall` failure and then re-reads via `db:read_csv`, proving the writer is not left
in a broken state by the rejected row.

**Docs**: `CLAUDE.md`, `src/CLAUDE.md`, `CHANGELOG.md`, and `bindings/js/src/lua-api.ts` all
accurately describe the shipped behavior (header-as-width-authority, pad-short/throw-long, the
`GcGuard`/`collect_garbage()` flush covering the throw path, no warning emitted). The
`bindings/js/src/lua-api.ts` addition uses the file's established backslash-escaped-backtick
convention throughout (confirmed the whole reference is one JS template literal starting at line
20) — no unescaped backtick or `${` was introduced, so the added prose does not risk breaking the
build.

Two trivial, non-blocking formatting nits are noted below; nothing else warranted a finding. The
implementation matches its locked decisions precisely — no invented findings.

## Info

### IN-01: New `write_csv` lines deviate from `.clang-format`

**File:** `src/lua_runner.cpp:704-705`
**Issue:** The continuation line inside the `write_csv` factory,

```cpp
return std::make_unique<CsvWriter>(quiver::csv_write::Writer(resolved, path, "write_csv", csv_options),
                                                     header_width);
```

is indented two columns further than `clang-format -style=file` produces (confirmed by running
`clang-format` against this file and diffing: the only difference from the formatted output is this
one line). The pre-phase file was already clang-format-clean (diffing the pre-phase revision through
the same formatter produced only an unrelated, pre-existing include-ordering difference), so this
drift was introduced by this phase and was not caught by `scripts/format.bat` before commit.
**Fix:** Run `scripts/format.bat` (or `clang-format -i src/lua_runner.cpp`) to re-align the
continuation to `header_width);` at the formatter's column.

### IN-02: New test lines deviate from `.clang-format`

**File:** `tests/test_lua_runner_write_csv.cpp:923-924`, `:1355`, `:1380`, `:1391` (offsets against
the current file; see below)
**Issue:** Four spots in the new test bodies format differently under `clang-format -style=file`
than what was committed — e.g. `check(")" + path1 + R"(", nil)` at line 923 is reformatted by the
tool onto two lines, and a couple of the `path + R"(` continuations inside the new WRITE-06 tests
are indented one level off. Diffing the pre-phase revision of this same file through the same
formatter produced zero differences, so — like IN-01 — this is new drift from this phase's edits,
not pre-existing debt.
**Fix:** Run `scripts/format.bat` and re-verify with `git diff` that only whitespace changed.

---

_Reviewed: 2026-09-17_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
