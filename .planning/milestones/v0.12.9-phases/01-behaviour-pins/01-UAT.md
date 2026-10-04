---
status: complete
phase: 01-behaviour-pins
source: [01-VERIFICATION.md]
started: 2026-10-03T00:13:38Z
updated: 2026-10-03T01:25:16Z
---

## Current Test

[testing complete]

## Tests

The maintainer delegated all three checks to Claude ("you check that"). Each was prosecuted by two independent
agents with different lenses (workflow run wf_17e026ed-6b8); none found a violation, so no defender round was needed.

### 1. No production behaviour changed to make a pin green; no existing test assertion edited or removed
expected: diff scope limited to tests/, bindings/js/test/ and two AGENTS.md files
result: pass
source: delegated (Claude, adversarial)
evidence: |
  src/include/cmake/CMakeLists/CMakePresets/.github/scripts, bindings/*/src (incl. loader.ts, lua-api.ts), the four
  manifests, CHANGELOG, tests/test_lua_runner.h, tests/schemas, tests/fixtures: tree hashes identical 5b57e7c vs HEAD;
  all 537 tracked files hash-match HEAD except .planning/config.json; src/lua_runner.cpp blob e968172f at base, HEAD
  and on disk. No C++ test line removed (16 TEST_F added, no DISABLED_; one assertion added inside OptionsAreStrict).
  JS sync test: the Expression-only floor and fixed four-type loop were replaced by strictly stronger checks.
  tests/CMakeLists.txt only adds the new source; SKBUILD turns tests off, and npm "files" excludes test/ and AGENTS.md.

### 2. Pre-existing PROJECT/REQUIREMENTS/ROADMAP edits not reverted or swept into commits
expected: in-progress edits committed in 4b4728c before execution; later commits tracking-only; blank lines acceptable
result: pass
source: delegated (Claude, adversarial)
evidence: |
  4b4728c (17:03) precedes the first execution commit 01830f7 (18:32). Every line it added is still present at HEAD
  (PROJECT 19/19, REQUIREMENTS 22/22, ROADMAP 30/30); PROJECT.md untouched since; Sandbox/rename wording counts equal.
  Every later hunk is checkbox, status cell, plan list or blank line (git diff -w --ignore-blank-lines leaves only
  tracking lines). The 21 added blank lines are gsd-tools' own _normalizeMd output (re-applied on every ROADMAP write);
  cmark-gfm renders identically except 5 "**Plans**:" lines that previously folded into the last success criterion
  (a rendering fix); roadmap analyze/get-phase output is identical on both layouts. Verdict: acceptable.

### 3. Recorded baseline counts were observed, not predicted
expected: Lua* = 444 tests / 12 suites, LuaRunnerCApiTest = 27, quiver_tests = 1410, quiver_c_tests = 543
result: pass
source: delegated (Claude, adversarial)
evidence: |
  Re-observed in build/dev, build/release and build/ (rebuilt first; it was stale): Lua* 444/12 listed and passed,
  quiver_tests 1410/43, quiver_c_tests 543/11, LuaRunnerCApiTest 27, sync test 6 pass, Python 350 passed.
  Per-commit TEST_F counts from git: 428 -> 430 -> 432 -> 438 -> 443 -> 444. 443/430 appear only as labelled
  point-in-time observations; 441 appears only as "the plan predicted". The AGENTS.md files contain no counts.

## Summary

total: 3
passed: 3
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

[none]

## Notes

- 4b4728c (the commit that captured the in-progress planning edits) was made by a Claude session; git cannot show
  maintainer authorisation, though it predates execution and matches the milestone change you described.
- ROADMAP progress row reads "In Progress|  |" until the phase transition rewrites it (tool output).
