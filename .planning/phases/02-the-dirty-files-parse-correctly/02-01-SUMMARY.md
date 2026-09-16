---
phase: 02-the-dirty-files-parse-correctly
plan: 01
subsystem: csv-read
tags: [lua, csv-parser, header_row, LUA-05, LUA-08]
dependency-graph:
  requires: []
  provides:
    - "csv_read::Options.header_row (1-based, 0 = no header, default 1)"
    - "db:read_csv / db:read_csv_stream header_row option, decoded by read_csv_options_from_lua"
    - "Reader-synthesized past-EOF header error (Cannot <op>: header row <N> not found in file '<path>')"
  affects:
    - "src/csv_read.h"
    - "src/csv_read.cpp"
    - "src/lua_runner.cpp"
    - "bindings/js/src/lua-api.ts"
tech-stack:
  added: []
  patterns:
    - "header mode set before variable_columns() in make_format (call-order fix for csv-parser's header_row(row<0) side effect on variable_column_policy)"
    - "try scoped to only the CSVReader construction, so a post-construction Pattern 1 throw is not re-wrapped"
key-files:
  created: []
  modified:
    - src/csv_read.h
    - src/csv_read.cpp
    - src/lua_runner.cpp
    - bindings/js/src/lua-api.ts
    - tests/test_lua_runner_read_csv.cpp
    - CHANGELOG.md
    - src/CLAUDE.md
decisions:
  - "header_row is 1-based at the Lua boundary, 0 means no header, default 1 (D-20, locked pre-planning)"
  - "make_format sets header mode (no_header()/header_row(n-1)) BEFORE variable_columns(KEEP_NON_EMPTY), never after -- csv-parser's header_row(row<0) side-effects variable_column_policy back to KEEP"
  - "past-EOF header detection lives in Reader's constructor (post-construction check on options.header_row != 0 && header.empty()), not in a catch block, since csv-parser never throws for this case"
  - "fixed a stale comment on FutureHeaderKeyIsAnUnknownOptionToday (named the wrong future key) and the read_csv changelog/CLAUDE.md prose while touching the same area (Rule 2 -- correctness of published docs)"
actuals:
  tokens: 5243
  tasks: 3
  commits: 5
metrics:
  duration: ~45min
  completed: 2026-09-16
status: complete
---

# Phase 2 Plan 1: header_row option, end-to-end Summary

Threads a 1-based `header_row` option (0 = no header, default 1, per D-20) from the Lua options
table all the way through `csv::CSVFormat`, fixing two empirically-verified csv-parser traps along
the way: a call-order bug that silently downgraded the `KEEP_NON_EMPTY` blank-line pin, and a
past-EOF header request that csv-parser answers with silent success instead of an error.

## What Was Built

- **`src/csv_read.h`** — `Options` gained `int64_t header_row = 1;` (1-based, 0 = no header, D-20).
- **`src/csv_read.cpp`**:
  - `make_format()` now branches on `options.header_row` (`no_header()` for 0, else
    `header_row(n-1)` with a value clamped into `int` range before the cast), and — the load-bearing
    part — sets the header mode **before** `format.variable_columns(KEEP_NON_EMPTY)`, not after.
    `CSVFormat::header_row(int row)` overwrites `variable_column_policy` to plain `KEEP` whenever
    `row < 0` (`no_header()`'s path), so calling `variable_columns()` first would have silently
    resurrected phantom blank-line rows for every no-header read (Phase 1's D-13 guarantee). The
    existing D-12 comment block was extended (not replaced) with this ordering rationale, citing
    `csv_format.cpp:44`.
  - `Reader`'s constructor now scopes its `try` to just the `csv::CSVReader` construction, then
    checks the result: if the caller explicitly asked for a header (`options.header_row != 0`) and
    `get_col_names()` came back empty, it throws
    `Cannot <op>: header row <N> not found in file '<path>'` — csv-parser itself never raises for a
    header row past EOF; it silently returns an empty header with zero data rows (`trim_header`
    drains the row queue looking for the index and gives up quietly). The check is gated on the
    caller's *original* 1-based request, not on header emptiness alone, since `header_row = 0`
    also produces an empty header by design and is not an error. Kept outside the `try` so it is
    not re-caught and double-prefixed by the surrounding catch.
- **`src/lua_runner.cpp`** — `read_csv_options_from_lua` (the one shared decoder for
  `db:read_csv`/`db:read_csv_stream`, LUA-03) now accepts `header_row` alongside `separator`:
  `get_type() == sol::type::number` then `.is<int64_t>()` (rejects a quoted `"2"` and a fractional
  `2.5`), then a non-negative check — each with its own Pattern 1 message, so "-1" is reported as
  negative rather than "not an integer" (a true statement vs. a lie).
- **`bindings/js/src/lua-api.ts`** — the CSV file reading section now documents `header_row`
  (1-based, default 1, `0` = no header, past-EOF throws). Not a build-gate requirement (the sync
  test only matches bound names/usertype methods/stdlib list, not option keys) but a correctness
  fix to published agent-facing docs, made per the plan's explicit instruction.
- **`tests/test_lua_runner_read_csv.cpp`** — 9 new tests across the three tasks (50 → 56 net after
  the earlier 48-test baseline plus 2 in Task 1 and net growth through Tasks 2-3): the tracer's
  end-to-end `header_row = 2` read against a junk/header/units/data file with an exact JSON
  assertion, `header_row = 1` matching the no-options default, the three past-EOF negatives
  (`read_csv` exact message, `read_csv_stream` naming itself, header-on-last-line succeeding with
  empty rows), and the three `header_row = 0` cases (no-header JSON round-trip, the permanent
  blank-line-mid-file ordering guard, and the streaming entry point agreeing on row count).
  Corrected the now-misleading comment on `FutureHeaderKeyIsAnUnknownOptionToday` (it anticipated
  a future key named `header`, but the actual key that shipped is `header_row`).
- **`CHANGELOG.md` / `src/CLAUDE.md`** — updated in place: the changelog's Phase 1 `db:read_csv`
  entry said `separator` was the only option key, which this plan makes false; `src/CLAUDE.md`'s
  `make_format` paragraph now describes the option-driven header row and the call-order constraint.

## Verification

- `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv.*'` — 56/56 pass (48 pre-existing
  + 9 new, minus 1 folded into a corrected comment — net 56).
- `./build/bin/quiver_tests.exe` (full suite) — 1167/1167 pass.
- `./build/bin/quiver_c_tests.exe` (full C API suite) — 557/557 pass.
- `cd bindings/js && bun test test/lua-api-sync.test.ts` — 6/6 pass.
- `bun run lint` in `bindings/js` — pre-existing, repo-wide CRLF/format lint debt confirmed present
  even with this plan's changes stashed (1706 lines of lint output either way); not touched, per
  root CLAUDE.md's "Do Not Fix" list (no drive-by JS lint fixes).
- `clang-format --dry-run --Werror` on every touched C++ file — clean.
- `grep -n "variable_columns" src/csv_read.cpp` confirms the call sits after the header-mode
  branch (plan's own verification step).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - doc correctness] Corrected a stale test comment naming the wrong future option key**
- **Found during:** Task 1
- **Issue:** `FutureHeaderKeyIsAnUnknownOptionToday`'s comment said "A key Phase 2 will legitimately
  add" but the key it names in the test body is `header`, whereas D-20 settled on `header_row`.
  Left uncorrected, a future reader could conclude the test becomes obsolete once `header_row`
  ships, which is false — `header` stays unknown forever.
- **Fix:** Reworded the comment to state `header_row`, not `header`, is what Phase 2 added.
- **Files modified:** `tests/test_lua_runner_read_csv.cpp`
- **Commit:** a8663a4

**2. [Rule 2 - doc correctness] Updated CHANGELOG.md and src/CLAUDE.md prose that this plan made false**
- **Found during:** Task 1 (noticed while reading the existing read_csv documentation)
- **Issue:** CHANGELOG.md's Phase 1 entry said `separator` was `db:read_csv`'s "only key today";
  `src/CLAUDE.md`'s `make_format` paragraph described a hardcoded `header_row(0)`. Both became
  factually wrong the moment this plan shipped an `Options.header_row` field.
- **Fix:** Added a CHANGELOG bullet under `[0.10.4] — unreleased` documenting `header_row` (no
  version bump — non-breaking addition, all five manifests already agree at 0.10.4) and rewrote
  the `src/CLAUDE.md` paragraph to describe the option-driven header row and the load-bearing call
  order.
- **Files modified:** `CHANGELOG.md`, `src/CLAUDE.md`
- **Commits:** 570f314, 693c417

No other deviations. All three tasks executed as planned; the two must-fix findings from
02-RESEARCH.md (call-order side effect, silent past-EOF) were implemented exactly as specified and
are each covered by a dedicated regression test.

### Noted, Not Acted On

Per the plan's own instruction: a file consisting of a single blank line is non-empty (passes the
existing size check) but may resolve to a zero-length header under the default `header_row = 1`,
which the new past-EOF check now turns into a throw where Phase 1 silently returned an empty
result. This input was not encountered during test-writing (no test constructs it), no requirement
asks for it, and the plan explicitly says to record rather than guard against it — recorded here
per that instruction.

## Known Stubs

None. No stub patterns, placeholder values, or unwired data paths were introduced.

## Threat Flags

None. `header_row` is a bounds/type-validated integer, not a new file-path input surface; the
existing sandbox (`resolve_sandboxed_path`) is unchanged and untouched by this plan.

## Self-Check: PASSED

- `src/csv_read.h` — FOUND, contains `header_row` field.
- `src/csv_read.cpp` — FOUND, contains reordered `make_format` and the past-EOF check.
- `src/lua_runner.cpp` — FOUND, `read_csv_options_from_lua` accepts `header_row`.
- `bindings/js/src/lua-api.ts` — FOUND, documents `header_row`.
- `tests/test_lua_runner_read_csv.cpp` — FOUND, 9 new `header_row` tests present.
- Commit a8663a4 — FOUND in `git log`.
- Commit e8ae3f6 — FOUND in `git log`.
- Commit 684b928 — FOUND in `git log`.
- Commit 570f314 — FOUND in `git log`.
- Commit 693c417 — FOUND in `git log`.
