---
status: testing
phase: 01-a-lua-script-reads-a-csv-file
source: [01-VERIFICATION.md]
started: 2026-09-15T14:20:00Z
updated: 2026-09-15T14:20:00Z
---

## Current Test

number: 1
name: WR-01 — filesystem precondition checks sit outside the try/catch in Reader::Reader
expected: |
  Decide: fix now, or accept as documented follow-up debt.

  The three checks in src/csv_read.cpp:54-62 (fs::exists / fs::is_directory /
  fs::file_size) run ABOVE the try at line 64. All three use the throwing
  std::filesystem overloads, which may throw std::filesystem_error on any OS-level
  stat failure — not only "path does not exist". A permission-denied parent
  directory or a broken symlink could therefore reach Lua without the
  "Cannot read_csv: " Pattern-1 prefix, which is what LUA-08 guarantees.

  Scope is narrower than the code review implies: all four REQUIRED sandbox/file
  negatives (escaping path, in-memory db, missing file, directory-as-path) throw
  hand-crafted Pattern-1 messages directly and are verified working. None of the
  phase's five success criteria are blocked.

  Fix would be: move the three checks inside the try, or wrap them in their own
  try/catch re-throwing through the same prefix.
awaiting: user response

## Tests

### 1. WR-01 — filesystem precondition checks outside the try/catch
expected: Decide whether to fix now (move the three checks inside the try, or wrap them in their own try/catch re-throwing through the same "Cannot <op>: " prefix) or accept as documented low-likelihood debt. The four required sandbox/file negatives are unaffected; LUA-08's blanket "no unwrapped message" guarantee is not airtight for permission-denied / broken-symlink inputs.
why_human: No portable way to trigger this on Windows — std::filesystem::permissions does not block owner read access. Whether it warrants a CI job on a different OS/permission model, or a preemptive fix, is a risk-tolerance call.
result: [pending]

### 2. D-22 catalogue entry 10 — parser-construction-failure wrapper never fires at runtime
expected: Confirm (in CI on Linux/macOS, or via a manufactured permission-denied file) that the try/catch around csv::CSVReader construction (src/csv_read.cpp:64-71) actually produces "Cannot <op>: cannot read file '<p>': <reason>" when the parser fails to open a file that passed the three precondition checks. Today tests/test_lua_runner_read_csv.cpp#ParserWrapperMessageExistsInSource only asserts the wrapper text exists in source, not that it fires.
why_human: Flagged by the phase's own SUMMARY (01-03-SUMMARY.md, coverage id D7) as human_judgment: true — no portable trigger exists in the current environment, so the catch block was proven present by static inspection only.
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
