---
phase: 01-a-lua-script-reads-a-csv-file
reviewed: 2026-09-15T13:50:00Z
depth: standard
files_reviewed: 12
files_reviewed_list:
  - src/csv_read.h
  - src/csv_read.cpp
  - src/lua_runner.cpp
  - src/CMakeLists.txt
  - cmake/Dependencies.cmake
  - bindings/js/src/lua-api.ts
  - tests/test_lua_runner_read_csv.cpp
  - tests/CMakeLists.txt
  - CHANGELOG.md
  - CLAUDE.md
  - src/CLAUDE.md
  - tests/CLAUDE.md
findings:
  critical: 0
  warning: 1
  info: 2
  total: 3
status: issues_found
---

# Phase 01: Code Review Report

**Reviewed:** 2026-09-15T13:50:00Z
**Depth:** standard
**Files Reviewed:** 12
**Status:** issues_found

## Summary

This phase adds `db:read_csv` / `db:read_csv_stream` to the Lua runner, backed by a new internal
`quiver::csv_read::Reader` wrapping vincentlaucsb/csv-parser, plus the FetchContent wiring, the
new test suite, and the doc updates (`CLAUDE.md`, `src/CLAUDE.md`, `tests/CLAUDE.md`,
`bindings/js/src/lua-api.ts`, `CHANGELOG.md`).

The design holds up well under adversarial reading. Verified specifically per the review brief:

- **Sandbox evaluation order (D-22)**: `resolve_sandboxed_path` is called before
  `read_csv_options_from_lua` in *both* `db:read_csv` (`src/lua_runner.cpp:456-460`) and
  `db:read_csv_stream` (`:487-490`), so a sandbox violation always wins over a bad-option error —
  confirmed by the `EscapingPathReportsBeforeMissingFile` / `InMemoryDatabaseReportsBeforeBadOptions`
  tests, which pass.
  `resolved_path` (never `path`/`original_path`) for filesystem I/O.
- **Ownership/RAII**: `Reader` is a clean Pimpl (`unique_ptr<Impl>`), copy-deleted/move-defaulted;
  the sink invocation in `for_each_row` deliberately sits outside the try/catch so a re-thrown Lua
  error from `db:read_csv_stream`'s callback propagates unwrapped and the `csv::CSVReader` member
  is destroyed via normal stack unwinding — no manual cleanup path exists to get wrong. Verified
  with `StreamCallbackErrorPropagatesAndClosesFile` (asserts the file can be deleted immediately
  after the error propagates on Windows).
- **Decoder uniformity**: `read_csv_options_from_lua` is the single decoder for both entry points,
  and both hard-code identical validation order (type check → collect entries → unknown-key check →
  string check → length check); `UnknownKeyReportsBeforeBadSeparatorValue` and the
  `StreamUnknownOptionKeyNamesTheStreamEntryPoint` test confirm the two entry points cannot diverge
  and correctly report their own operation name.
- Ran the new suite locally (`quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv*'`): all 47 tests
  pass.
- `bindings/js/src/lua-api.ts` documents both new methods with the literal `db:read_csv` /
  `db:read_csv_stream` tokens the sync test greps for, and the prose (row-fed-count semantics,
  "table with nil holes" caveats N/A here since every cell is a string, sandbox text, options
  shape) matches the implementation.

One real gap found: the three filesystem precondition checks in `Reader`'s constructor are not
wrapped in the same try/catch that guards the `csv::CSVReader` construction immediately below them,
so an OS-level `std::filesystem_error` (not just "not found") can reach the sol2 boundary unwrapped,
violating the file's own documented LUA-08 guarantee. Also two minor code-quality nits.

## Warnings

### WR-01: Filesystem precondition checks in `Reader::Reader` can leak an unwrapped `std::filesystem_error`

**File:** `src/csv_read.cpp:49-72`
**Issue:** The three precondition checks — `fs::exists`, `fs::is_directory`, `fs::file_size` — run
*before* the `try` block that wraps `csv::CSVReader` construction:

```cpp
if (!fs::exists(resolved_path)) {
    throw std::runtime_error("Cannot " + operation + ": file not found: " + original_path);
}
if (fs::is_directory(resolved_path)) {
    throw std::runtime_error("Cannot " + operation + ": path is a directory: " + original_path);
}
if (fs::file_size(resolved_path) == 0) {
    throw std::runtime_error("Cannot " + operation + ": file '" + original_path + "' is empty");
}

try {
    csv::CSVReader reader(resolved_path, make_format(options));
    ...
} catch (const std::exception& e) {
    throw std::runtime_error("Cannot " + operation + ": cannot read file '" + original_path + "': " + e.what());
}
```

All three calls use the throwing (non-`error_code`) overloads of `std::filesystem` functions,
which the standard documents as throwing `std::filesystem_error` "if an error occurs" — not only
when the path doesn't exist. A permission-denied parent directory, a broken symlink, or any other
OS-level stat failure at this point throws a raw `std::filesystem_error` (e.g. `"filesystem error:
cannot get file size: Permission denied [...]"`) that reaches the Lua boundary without the
`"Cannot read_csv: ..."` / `"Cannot read_csv_stream: ..."` prefix. The file's own comment at line
69 states the invariant this violates: *"No csv-parser or standard-library message may reach Lua
unwrapped (LUA-08)"* — and the try/catch two lines below is there specifically to uphold it for the
`CSVReader` constructor, but not for these three checks. sol2's default exception handler will
still convert the exception into a Lua error (so this is not a crash), but the message shape breaks
the documented and tested contract, and any caller matching on the `"Cannot <op>: "` prefix (as the
test file's own `expect_prefixed_error` helper does) would fail against this path. The project's
own test (`ParserWrapperMessageExistsInSource`) acknowledges there is no portable way to trigger
this on the current CI matrix, which is presumably why it slipped through, but the code should
still uphold the guarantee rather than rely on it being untestable.

**Fix:** Move the three precondition checks inside the existing `try`, or wrap them with their own
try/catch that re-throws through the same `"Cannot " + operation + ": ..."` prefix, e.g.:

```cpp
try {
    if (!fs::exists(resolved_path)) {
        throw std::runtime_error("file not found: " + original_path);
    }
    if (fs::is_directory(resolved_path)) {
        throw std::runtime_error("path is a directory: " + original_path);
    }
    if (fs::file_size(resolved_path) == 0) {
        throw std::runtime_error("file '" + original_path + "' is empty");
    }
    csv::CSVReader reader(resolved_path, make_format(options));
    ...
} catch (const std::exception& e) {
    throw std::runtime_error("Cannot " + operation + ": " + e.what());
}
```
(adjusting the three inner messages so the combined string still matches the pinned test strings).

## Info

### IN-01: Dead branch in `Reader::for_each_row`

**File:** `src/csv_read.cpp:95-118`
**Issue:** `cells` is an `std::optional<std::vector<std::string>>` used to signal "no more rows",
but the only path that leaves it unset is the early `break` at line 97-99, which already exits the
enclosing `while (true)` loop directly (a `break` inside a `try` still breaks the loop it's nested
in, not just the `try`). Every other path through the `try` block unconditionally assigns
`cells = std::move(row_cells);` before falling out normally. Consequently the post-try check:

```cpp
if (!cells) {
    break;
}
```

at lines 116-118 can never evaluate true — it is unreachable. Harmless today, but it obscures the
actual control flow and will mislead the next person who touches this loop into thinking there is a
second way to end iteration.

**Fix:** Either drop the `std::optional` and `break` directly from inside the `try` (as is already
done for the `it == end` case), or restructure so the "no more rows" signal has exactly one path,
e.g.:

```cpp
while (it != end) {
    std::vector<std::string> row_cells;
    try {
        csv::CSVRow& row = *it;
        row_cells.reserve(row.size());
        for (csv::CSVField& field : row) {
            row_cells.emplace_back(field.get<std::string_view>());
        }
        ++it;
    } catch (const std::exception& e) {
        throw std::runtime_error(...);
    }
    ++index;
    if (!sink(std::move(row_cells), index)) {
        break;
    }
}
```

### IN-02: Inconsistent quoting style across the three precondition messages

**File:** `src/csv_read.cpp:54-62`
**Issue:** `"file not found: " + original_path` and `"path is a directory: " + original_path` leave
the path unquoted, while `"file '" + original_path + "' is empty"` quotes it. All three are
otherwise the same class of message (a bad path/file state for the same operation) and the pinned
test strings (`tests/test_lua_runner_read_csv.cpp:155,600,619`) show the inconsistency is
intentional-by-omission rather than a typo, but it's a minor readability wart worth normalizing —
most other Pattern 1 messages in this file (e.g. `resolve_sandboxed_path`'s `"path '...' escapes
..."`) quote the offending value.
**Fix:** Quote `original_path` consistently in all three messages (and update the three pinned
test strings to match) the next time this function is touched; not worth a standalone change.

---

_Reviewed: 2026-09-15T13:50:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
