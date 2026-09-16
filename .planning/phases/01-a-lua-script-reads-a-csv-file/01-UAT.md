---
status: complete
phase: 01-a-lua-script-reads-a-csv-file
source: [01-VERIFICATION.md]
started: 2026-09-15T14:20:00Z
updated: 2026-09-16T00:30:00Z
---

## Current Test

number: 2
name: all tests resolved
expected: |
  Both items were closed by executable tests rather than human judgement.
awaiting: nothing

## Tests

### 1. WR-01 — filesystem precondition checks outside the try/catch
expected: Decide whether to fix now (move the three checks inside the try, or wrap them in their own try/catch re-throwing through the same "Cannot <op>: " prefix) or accept as documented low-likelihood debt. The four required sandbox/file negatives are unaffected; LUA-08's blanket "no unwrapped message" guarantee is not airtight for permission-denied / broken-symlink inputs.
result: pass
why_human: No portable way to trigger this on Windows — std::filesystem::permissions does not block owner read access. Whether it warrants a CI job on a different OS/permission model, or a preemptive fix, is a risk-tolerance call.

### 2. D-22 catalogue entry 10 — parser-construction-failure wrapper never fires at runtime
expected: Confirm (in CI on Linux/macOS, or via a manufactured permission-denied file) that the try/catch around csv::CSVReader construction (src/csv_read.cpp:64-71) actually produces "Cannot <op>: cannot read file '<p>': <reason>" when the parser fails to open a file that passed the three precondition checks. Today tests/test_lua_runner_read_csv.cpp#ParserWrapperMessageExistsInSource only asserts the wrapper text exists in source, not that it fires.
result: pass
why_human: Flagged by the phase's own SUMMARY (01-03-SUMMARY.md, coverage id D7) as human_judgment: true — no portable trigger exists in the current environment, so the catch block was proven present by static inspection only.

## Summary

total: 2
passed: 2
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

## Resolution

Both items are closed by executable tests. Neither needed human judgement — the
"no portable trigger on Windows" premise behind both was wrong, and was disproven
empirically before any code was changed.

### WR-01 — resolved, and the real bug was broader than reported

The code review located this in `src/csv_read.cpp`. Probing found that file's three
`fs::` calls are in practice unreachable with an OS error through the Lua boundary —
but the *same class of bug* was live one layer up, in `resolve_sandboxed_path`
(`src/lua_runner.cpp`), the single gate all nine file-touching Lua operations share.

`db:read_csv("NUL")` — any Windows reserved device name, any case, any directory —
made `weakly_canonical` throw, and the raw text reached the script:

    weakly_canonical: The parameter is incorrect.: "...\NUL"

No `Cannot read_csv:` prefix, so LUA-08 was broken for `open_file`, `bin_to_csv`,
`csv_to_bin`, `export_csv`, `import_csv`, `validate_migrations` and `expr:save` too —
not just `read_csv`. Fixed at the gate (commit `404020b`); `csv_read.cpp` hardened
with the non-throwing overloads as well, since `Reader` is internal C++ that a future
non-Lua caller would reach without that gate.

Why the earlier "not triggerable" conclusion was wrong: it rested on
`std::filesystem::permissions`, the weakest lever available on Windows. Stronger ones
were never tried. For the record, what each actually does there — a DENY ACE blocks
the open but *not* the metadata queries; an exclusive lock does the same; neither makes
`exists`/`is_directory`/`file_size` throw. A reserved device name does.

Tests: `LuaRunner_ReadCsv.DeviceNamePathIsReportedWithPrefix`, plus
`LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` spanning three more operations so
the fix cannot regress into a per-caller patch.

**Mutation-checked:** with the gate fix reverted, the test fails with the exact raw
`weakly_canonical:` message; with it restored, green. A test that passes either way
would have proven nothing.

### D-22 catalogue entry 10 — resolved

Fires at runtime. An exclusive lock (`CreateFileW`, `dwShareMode` 0) leaves a file that
passes all three preconditions — `exists=1, is_directory=0, file_size=8` — but cannot be
opened, producing exactly:

    Cannot read_csv: cannot read file 'locked.csv': Cannot open file ...

No elevation, no second process, no admin rights. `ParserWrapperMessageExistsInSource`,
which only grepped the source text for the wrapper, is deleted and replaced by
`UnreadableFileReportsParserFailure` (exclusive lock on Windows, `chmod 000` on POSIX,
skipped under root). It asserts the three preconditions still pass before reading, so it
cannot silently degrade into re-testing an earlier catalogue message.

Suites after the change: 1159 C++ (was 1157: three added, one placeholder deleted) and
557 C API, all passing.
