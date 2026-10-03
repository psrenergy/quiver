---
status: testing
phase: 01-behaviour-pins
source: [01-VERIFICATION.md]
started: 2026-10-03T00:13:38Z
updated: 2026-10-03T00:13:38Z
---

## Current Test

number: 1
name: No production behaviour changed to make a pin green; no existing test assertion edited or removed
expected: |
  git diff --stat 5b57e7c..HEAD -- src include cmake 'bindings/*/src' is empty; removed test lines are only the
  planned Expression-only floor and its strictly-stronger WR-01 refactor (orchestrator: UPHELD)
awaiting: user response

## Tests

### 1. No production behaviour changed to make a pin green; no existing test assertion edited or removed
expected: diff scope limited to tests/, bindings/js/test/ and two AGENTS.md files (orchestrator: UPHELD)
result: [pending]

### 2. Pre-existing PROJECT/REQUIREMENTS/ROADMAP edits not reverted or swept into commits
expected: they were committed in 4b4728c before execution; later commits touch only status cells, checkboxes, the plan list and blank lines — is the blank-line normalisation acceptable? (orchestrator: UPHELD)
result: [pending]

### 3. Recorded baseline counts were observed, not predicted
expected: Lua* = 444 tests / 12 suites in build/dev and build/release, LuaRunnerCApiTest = 27, quiver_tests = 1410, quiver_c_tests = 543 (orchestrator re-observed 444/12 and 1410: UPHELD)
result: [pending]

## Summary

total: 3
passed: 0
issues: 0
pending: 3
skipped: 0
blocked: 0

## Gaps
