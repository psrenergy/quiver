---
phase: 04-a-lua-script-writes-a-csv-file
reviewed: 2026-09-16T00:00:00Z
depth: standard
files_reviewed: 12
files_reviewed_list:
  - src/csv_write.h
  - src/csv_write.cpp
  - src/utils/number.h
  - src/lua_runner.cpp
  - src/CMakeLists.txt
  - tests/test_lua_runner_write_csv.cpp
  - tests/CMakeLists.txt
  - bindings/js/src/lua-api.ts
  - bindings/js/test/lua-api-sync.test.ts
  - src/CLAUDE.md
  - tests/CLAUDE.md
  - .gitignore
findings:
  critical: 1
  warning: 3
  info: 2
  total: 6
status: issues_found
---

# Phase 04-a: Code Review Report

**Reviewed:** 2026-09-16T00:00:00Z
**Depth:** standard
**Files Reviewed:** 12
**Status:** issues_found

## Summary

Reviewed the new Lua-only `db:write_csv` feature: the hand-rolled `quiver::csv_write::Writer`
(`src/csv_write.h/.cpp`), its `CsvWriter` Lua wrapper and cell/row/option decoders in
`src/lua_runner.cpp`, the shared `quiver::utils::append_number` helper, the build registration,
the new test suite, and the `bindings/js/src/lua-api.ts` documentation (verified against the
implementation, including the extracted-and-executed worked example).

The CSV quoting/escaping path (`append_record`), the int64-never-through-double number path, the
finiteness guard, the writer's `unique_ptr` + `sol::no_constructor` ownership, the Pattern 1
message catalogue (verified verbatim against the pinned comment block), and the path sandbox
integration are all correct and internally consistent — I traced every branch and every message
string and found no logic errors there. The one real defect is a missing bound on the
integer-key-driven row/header width, which lets a single Lua statement force an unbounded
allocation; the rest are lower-severity robustness/quality gaps.

## Critical Issues

### CR-01: Unbounded allocation from an untrusted script's integer table key (row width / header width)

**File:** `src/lua_runner.cpp:273-288` (`csv_row_cells_from_lua`) and `src/lua_runner.cpp:347-365`
(`csv_header_from_lua`)

**Issue:** Both functions size their output vector from the **maximum integer key present**, with
no upper bound:

```cpp
std::vector<std::string> cells(static_cast<std::size_t>(max_index));
```

A script needs no prior state and no valid schema access to trigger this — a single top-level
statement is enough:

```lua
local w = db:write_csv(path)
w:write_row({ [100000000] = "x" })   -- allocates/default-constructs 100,000,000 std::strings
```

or, even more directly, via the header option decoded before any row is written:

```lua
db:write_csv(path, { header = { [100000000] = "x" } })
```

For moderate N (tens to hundreds of millions) this does not throw at all — it succeeds after
consuming multiple GB of memory and multiple seconds of wall time constructing that many empty
`std::string`s, then writes one absurdly wide CSV line. For very large N it throws
`std::length_error`/`std::bad_alloc`, which is presumably caught and surfaced as a script error,
but the moderate-N case is a silent resource-exhaustion path with no diagnostic at all.

This is exactly the class of threat the same file already defends against elsewhere: the
return-value JSON encoder added just above (`kMaxReturnDepth`, `kMaxReturnBytes`) exists
specifically because "the script is untrusted input to a host" and "an untrusted script could
otherwise hang the host and exhaust memory." The two new CSV-writer decoders reintroduce the same
class of hazard with no analogous cap. (Note: the same "size from max index" shape already exists
for the time-series/vector/set group writers elsewhere in this file — this is not a wholly novel
pattern in the codebase, but the new code does not improve on it and is trivially reachable with
no other setup.)

**Fix:** Bound `max_index` before allocating, e.g. reject anything past a fixed ceiling (mirroring
the `kMaxReturnBytes`-style guard already established in this file), or size the vector from the
actual number of entries observed during the same walk and only expand lazily as needed instead of
eagerly allocating up front:

```cpp
constexpr std::int64_t kMaxCsvRowWidth = 1'000'000;  // or similar
// ...
if (pair.first.as<std::int64_t>() > kMaxCsvRowWidth) {
    throw std::runtime_error("Cannot " + operation + ": row key exceeds the maximum supported column count");
}
```

## Warnings

### WR-01: `append_number` ignores `std::to_chars`'s error code

**File:** `src/utils/number.h:16-17`

**Issue:**

```cpp
const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
out.append(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
```

`ec` is captured and never checked. This is currently safe only because the 32-byte buffer happens
to be large enough for every value of every type this function is instantiated for today
(`int64_t`, `double`, used by both the CSV writer and the Lua JSON return-value encoder). If
`to_chars` ever fails (buffer too small for some future `T`, or a build/library variance), the
documented failure behavior of `to_chars` leaves `[buffer.data(), end)` in an unspecified state —
`end` becomes `buffer.data() + buffer.size()` on failure, so the code would silently append 32
bytes of stale/zero-initialized buffer content into a CSV cell or a JSON number, with no error
raised anywhere.

**Fix:** Either assert success or throw:

```cpp
const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
if (ec != std::errc()) {
    throw std::runtime_error("Failed to format number: buffer too small");
}
out.append(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
```

### WR-02: Options-table keys cast to `std::string` without a type check

**File:** `src/lua_runner.cpp:382-383` (new `write_csv_options_from_lua`, mirroring the
pre-existing `read_csv_options_from_lua` at `~1144-1145`)

**Issue:**

```cpp
options.as<sol::table>().for_each(
    [&](sol::object key, sol::object value) { entries.emplace_back(key.as<std::string>(), std::move(value)); });
```

`key.as<std::string>()` is called unconditionally, without first checking `key.is<std::string>()`
the way every other converter in this file does (`lua_cell_as<T>`, the file's own documented "one
checked Lua-value→T conversion" — see the comment directly above `lua_cell_as`). A non-numeric,
non-string key (e.g. `{ [true] = 1 }` or `{ [{}] = 1 }`) reaching this `for_each` produces an
unchecked cast whose behavior depends on sol2's unsafe-getter path (which is the default in
Release builds per this file's own `SOL_SAFE_GETTER` commentary a few hundred lines below). This
is new code added for `write_csv`, even though it duplicates a pattern already present in
`read_csv_options_from_lua`.

**Fix:** Guard the key type before casting, consistent with `lua_cell_as`:

```cpp
options.as<sol::table>().for_each([&](sol::object key, sol::object value) {
    if (!key.is<std::string>()) {
        throw std::runtime_error("Cannot " + operation + ": option keys must be strings");
    }
    entries.emplace_back(key.as<std::string>(), std::move(value));
});
```
(Consider fixing `read_csv_options_from_lua` the same way in a follow-up, since it has the
identical gap.)

### WR-03: `Writer::close()` leaves the file open if `flush()` fails

**File:** `src/csv_write.cpp:150-160`

**Issue:**

```cpp
void Writer::close(const std::string& operation) {
    if (closed_) {
        return;
    }
    out_.flush();
    if (out_.fail()) {
        throw std::runtime_error("Cannot " + operation + ": failed to flush file '" + original_path_ + "'");
    }
    out_.close();
    closed_ = true;
}
```

If `flush()` fails, the function throws before reaching `out_.close()`, so `closed_` stays
`false` and the underlying file descriptor stays open until the `Writer` (or its owning
`CsvWriter`) is eventually destroyed — which, per this phase's own documented caveat, may be
delayed indefinitely by Lua's non-deterministic GC. A retried `w:close()` after catching the
error will attempt to flush an already-failed stream again (harmless, but also not clearly
useful).

**Fix:** Close the descriptor regardless of the flush outcome so the resource isn't held open
after a reported failure:

```cpp
out_.flush();
const bool flush_failed = out_.fail();
out_.close();
closed_ = true;
if (flush_failed) {
    throw std::runtime_error("Cannot " + operation + ": failed to flush file '" + original_path_ + "'");
}
```

## Info

### IN-01: Confirmed — `src/csv_write.cpp` `performance-unnecessary-value-param`

**File:** `src/csv_write.cpp:91` (`Writer::Writer`)

Confirmed valid. Of the four constructor parameters, only `original_path` is moved into a member
(`original_path_(std::move(original_path))`); `resolved_path` and `operation` are read but never
stored or moved, and `options` is only read (`options.separator`, `options.header`) and never
moved either. Passing all four by value forces a copy at every call site for the three that gain
nothing from value semantics. Recommend `const std::string& resolved_path`, `const std::string&
operation`, `const Options& options` (keep `original_path` by value for the existing move).

### IN-02: Confirmed/dismissed — `tests/test_lua_runner_write_csv.cpp` clang-tidy signal

- `readability-identifier-naming` on `LuaRunner_WriteCsv` / `LuaRunner_WriteCsvErrors` —
  **dismissed**: matches the established GoogleTest fixture-naming convention already used
  elsewhere in this suite (e.g. `LuaRunner_ReadCsv` in `test_lua_runner_read_csv.cpp`), not a
  phase-specific defect.
- `performance-faster-string-find`, `modernize-raw-string-literal`, `modernize-use-starts-ends-with`
  — **dismissed**: style-only findings confined to test code (e.g. `rfind(x, 0) == 0` instead of
  `starts_with`), with no effect on test reliability; per review scope, test-file style issues are
  out of bounds unless they affect correctness of the test itself, which they do not here.

`src/lua_runner.cpp`'s pre-existing `performance-unnecessary-value-param` / `bugprone-empty-catch`
findings were confirmed to sit at lines that predate this phase (e.g. the `transaction`/`dry_run`
lambdas' `catch (...) {}` around a best-effort rollback, and the long-standing
`NOLINTBEGIN(performance-unnecessary-value-parameter)` blocks around the sol2 lambda bindings) and
are correctly excluded from this review as instructed.

---

_Reviewed: 2026-09-16T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
