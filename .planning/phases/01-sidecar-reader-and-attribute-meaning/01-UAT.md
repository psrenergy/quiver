---
status: testing
phase: 01-sidecar-reader-and-attribute-meaning
source: [01-VERIFICATION.md]
started: 2026-09-20T17:35:00Z
updated: 2026-09-20T17:35:00Z
---

## Current Test

number: 1
name: Root-migrations-path interpretation for READ-01 / D-16
expected: |
  Either confirm this degrade-to-empty behavior is acceptable for a case that cannot occur in
  real PSR studies, or file a follow-up to test/handle it explicitly.
awaiting: user response

## Tests

### 1. Root-migrations-path interpretation for READ-01 / D-16

`migrations_path` pointing at a filesystem root makes `parent_path()` the root itself, so
`ui_dir` resolves to `<root>/ui` and finds nothing (no `/` sibling exists above a root). This
case is untested and unspecified by any source artifact (01-01-PLAN.md flagged_assumptions #1).

expected: Confirm degrade-to-empty is acceptable for a case that cannot occur in real PSR studies, or file a follow-up.
result: [pending]

### 2. SAFE-01 interpretation of "byte-identical to today's"

Verified by comparing two runs of the same binary (sidecar present vs. a mirror with `ui/`
removed) inside one test process, rather than against a stored golden file
(01-01-PLAN.md flagged_assumptions #2).

expected: Sign off that in-process mirror comparison is sufficient evidence, or require a golden-file addition.
result: [pending]

### 3. SAFE-02 interpretation of "never fails"

Covers every `std::exception`-derived throw inside `load_ui_config`. Explicitly NOT a hard crash
from resource exhaustion (a sidecar large enough to exhaust memory), which is separately
dispositioned `accept` as threat-register row T-01-04 (01-01-PLAN.md flagged_assumptions #3).

expected: Sign off that resource-exhaustion DoS from a hostile `ui/*.toml` is out of scope for this milestone, or require a size cap.
result: [pending]

### 4. Judgment-tier prohibitions (5)

1. Sidecar text never replaces, reorders or omits a schema-derived fact.
2. A code-to-label pair renders verbatim even when it looks wrong (the recorded `HasCommitment` inversion).
3. `ui_config.cpp` never writes to the `ui/` or migrations tree.
4. `tests/test_database_lifecycle.cpp` was not edited to make this phase pass.
5. No fixture file was committed under `tests/schemas/ui/`.

expected: All 5 read as satisfied from source/git inspection during verification; human sign-off closes the judgment-tier gate.
result: [pending]

## Summary

total: 4
passed: 0
issues: 0
pending: 4
skipped: 0
blocked: 0

## Gaps
