---
phase: 02-the-dirty-files-parse-correctly
reviewed: 2026-09-16T00:00:00Z
depth: standard
files_reviewed: 9
files_reviewed_list:
  - src/csv_read.h
  - src/csv_read.cpp
  - src/lua_runner.cpp
  - bindings/js/src/lua-api.ts
  - tests/test_lua_runner_read_csv.cpp
  - .gitattributes
  - tests/CLAUDE.md
  - src/CLAUDE.md
  - CHANGELOG.md
findings:
  critical: 1
  warning: 2
  info: 1
  total: 4
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-09-16T00:00:00Z
**Depth:** standard
**Files Reviewed:** 9
**Status:** issues_found

## Summary

This phase adds a `header_row` option to `db:read_csv`/`db:read_csv_stream`, fixes the
`make_format()` call-order hazard (`no_header()`'s `header_row(row < 0)` side effect resetting
`variable_column_policy`), synthesizes the "header row not found" error csv-parser never raises
itself, wraps `resolve_sandboxed_path`'s throwing filesystem calls so an OS-level failure (e.g. a
Windows device name) can't reach a script unwrapped, and commits two real-world CSV fixtures with
a matching `.gitattributes` `-text` exemption.

The two subtle fixes the phase set out to make (`make_format` call order, and the past-EOF error
gate) are both correct: I traced the diff against the pre-phase code, confirmed the order is
header-mode-then-`variable_columns`, confirmed the INT_MAX clamp arithmetic doesn't itself
overflow, and confirmed the past-EOF gate uses the caller's pre-translation `header_row` so it
cannot fire on the legitimate `header_row = 0` case. The two committed fixtures were verified
byte-for-byte (BOM + CRLF present on the Energia file, absent on the GD file) and the two
"regression" tests were hand-checked against the actual fixture bytes — `results[2] ==
"2005-01 93943"` and `results[224] == "2023-07 386433"` really do correspond to lines 4 and 226 of
`ma_energia_residencial.csv`, and `results[71] == "2021-07 51818.33"` really is line 72 of
`ma_gd_data.csv` — so these are genuine transformation assertions, not vacuous byte-length checks.
The `.gitattributes` exemption is correctly scoped and matches the two committed filenames.

However, I found one genuine behavioral bug: `db:read_csv_stream`'s `header` callback argument
does not honor the same "absent, not an empty table" sentinel that `db:read_csv`'s `csv.header`
does for `header_row = 0`, contradicting both an explicit in-file design comment (D-01) and the
agent-facing documentation's claim that the two are "the same array." No test exercises the
`header` argument's value under `header_row = 0` for the streaming entry point, so this gap is
currently undetected by the suite.

## Critical Issues

### CR-01: `db:read_csv_stream`'s `header` callback argument is always a table, never absent, breaking the documented no-header sentinel

**File:** `src/lua_runner.cpp:495` (contrast with the correct handling at `src/lua_runner.cpp:473-476`)

**Issue:** `db:read_csv` deliberately makes `csv.header` *absent* (`nil`), not an empty table, when
the file has no header (`header_row = 0`):

```cpp
// src/lua_runner.cpp:469-476
// `header` is absent (not an empty table) when the file has no header -- Phase 2's
// "no header" declaration reuses this same falsy sentinel, so the two must not
// collide (D-01). ...
const auto& header = reader.header();
if (!header.empty()) {
    result["header"] = to_lua_table(lua, header);
}
```

`db:read_csv_stream` does not apply this rule. It unconditionally builds a table from
`reader.header()` once, before the row loop, and passes it to every callback invocation:

```cpp
// src/lua_runner.cpp:495-499
const auto header_table = to_lua_table(lua, reader.header());

return reader.for_each_row([&](std::vector<std::string>&& cells, int64_t index) -> bool {
    const auto row_table = to_lua_table(lua, cells);
    auto result = on_row(row_table, index, header_table);
```

`to_lua_table` on an empty `std::vector<std::string>` returns `lua.create_table()` with zero
entries — a valid, non-nil Lua table. In Lua, `{}` is **truthy**. So under `header_row = 0`:

- `db:read_csv("f.csv", { header_row = 0 }).header` is `nil` (falsy — a script does
  `if csv.header then ... end` safely).
- `db:read_csv_stream("f.csv", function(row, index, header) ... end, { header_row = 0 })` passes
  `header = {}` (truthy) to every callback.

The same script pattern (`if header then ...`) behaves differently between the two entry points
for the exact feature (`header_row = 0`) this phase introduces — a direct violation of the file's
own D-01 comment ("the two must not collide") and of the shipped documentation:

```
// bindings/js/src/lua-api.ts:650
-- header: the same array db:read_csv returns, reachable here so a column can be found by
```

That claim is false whenever `header_row = 0`.

No test in `tests/test_lua_runner_read_csv.cpp` checks the `header` argument's *value* under
`header_row = 0` for the streaming entry point —
`StreamHeaderRowZeroBlankLineAgreesWithWholeFileRead` (line 395) passes a callback that ignores its
third parameter entirely, so this divergence currently has no regression coverage and would not be
caught by the existing suite.

**Fix:** Mirror the whole-file path's sentinel — pass Lua `nil` instead of an empty table when the
header is empty:

```cpp
sol::object header_arg = reader.header().empty()
    ? sol::object(sol::lua_nil)
    : sol::object(to_lua_table(lua, reader.header()));

return reader.for_each_row([&](std::vector<std::string>&& cells, int64_t index) -> bool {
    const auto row_table = to_lua_table(lua, cells);
    auto result = on_row(row_table, index, header_arg);
    ...
```

and add a test asserting `header == nil` inside the stream callback when `header_row = 0` (the
mirror of `BomStrippedUnderExplicitHeaderRowAndNoHeader`'s `csv.header == nil` assertion, routed
through `db:read_csv_stream` instead).

## Warnings

### WR-01: The `header_row` INT_MAX-clamp path in `make_format()` has no regression test

**File:** `src/csv_read.cpp:53-60`

**Issue:** The clamp that prevents a caller-supplied `header_row` at/above `INT_MAX` from
truncating into a negative `int` (and re-triggering `no_header()`'s `variable_column_policy` reset)
is exactly the kind of narrow, easy-to-regress arithmetic the phase's own comments flag as
load-bearing:

```cpp
const int64_t zero_based = options.header_row - 1;
constexpr int64_t kMaxRow = std::numeric_limits<int>::max();
format.header_row(static_cast<int>(zero_based > kMaxRow ? kMaxRow : zero_based));
```

I traced the arithmetic by hand and it is correct (no double-negative, no off-by-one at the
boundary `zero_based == kMaxRow`). But `tests/test_lua_runner_read_csv.cpp` has no case that drives
`header_row` anywhere near `INT_MAX` (e.g. `2147483647`, `2147483648`, or Lua's own int64 max
`9223372036854775807`) through `db:read_csv`. `src/CLAUDE.md`/`tests/CLAUDE.md` don't mention this
edge either. A future refactor of `make_format` (e.g. someone "simplifying" the clamp expression)
would compile and pass every existing test while silently reintroducing the truncation-to-negative
bug this code exists to prevent — the Reader's own past-EOF check would then likely absorb the
symptom into a differently-worded error, or in the worst case (if some future int-range edge is
missed) re-trigger the `variable_column_policy` reset for a value that isn't `0`.

**Fix:** Add one test exercising a `header_row` at/just-above `INT_MAX` (e.g.
`{ header_row = 2147483648 }`) against a small file, asserting the past-EOF message (`"header row
2147483648 not found in file ..."`) rather than a crash, silent misparse, or reintroduced
phantom-blank-row behavior.

### WR-02: `csv_read::Options.header_row`'s "non-negative, 0 means no-header" contract is enforced only by the Lua decoder, not by `Reader`/`make_format`

**File:** `src/csv_read.h:21-26`, `src/csv_read.cpp:47-63`

**Issue:** `Options.header_row` is a public `int64_t` with no validation at the type or
`make_format`/`Reader` level. The only place that rejects a negative value is
`read_csv_options_from_lua` in `src/lua_runner.cpp:981-985`. If any future internal caller
constructs `csv_read::Options{.header_row = -5}` directly (bypassing the Lua decoder — plausible
given `csv_read.h`'s own header says it exists specifically because Lua needs it, but nothing in
the type prevents a second caller from appearing), `make_format` computes `zero_based = -6` and
calls `format.header_row(-6)`, which is `< 0` and silently behaves as "no header" (via the same
`csv-parser` side effect the phase's comments are otherwise careful about), rather than raising any
error naming the bad value. This is consistent with the project's "assume callers obey contracts"
philosophy for a single-caller internal type, but it is worth naming explicitly since the contract
("negative other than exactly interpreting `0` as sentinel" is undefined behavior, not a defined
one) lives entirely outside the type that owns it.

**Fix:** Optional — not required given the current single-caller (Lua) topology, but if a second
caller is ever added, either (a) validate `header_row >= 0` in `Reader`'s constructor as a fourth
Pattern-1 precondition, or (b) document the contract on the `Options::header_row` member itself
(current comment only explains the `0` sentinel, not the negative-input contract).

## Info

### IN-01: Repeated three-line `ec`-check-and-throw pattern in `Reader::Reader`

**File:** `src/csv_read.cpp:80-105`

**Issue:** The `exists` / `is_directory` / `file_size` precondition checks each repeat the same
four-line "if (ec) throw 'cannot access file'" block verbatim. This is minor duplication (12 lines
across three call sites) that a small helper (e.g. `check_ec(ec, operation, original_path)`) could
collapse to three one-line calls.

**Fix:** Not required — the project explicitly prefers "clean code over defensive code" and
"simple solutions over complex abstractions," and the D-22 comment above these checks pins the
exact wording per-message, which a shared helper would still need to parameterize. Flagging only
because it's the one piece of local duplication in an otherwise tight diff; leave as-is unless
touched again.

---

_Reviewed: 2026-09-16T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
