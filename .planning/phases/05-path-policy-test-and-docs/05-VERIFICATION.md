---
phase: 05-path-policy-test-and-docs
verified: 2026-10-03T19:10:00Z
status: gaps_found
score: 8/9 must-haves verified
behavior_unverified: 0
overrides_applied: 0
gaps:
  - truth: "Every removed planning ID that carried meaning is replaced by its one-line reason or by the name of the test that pins it (DOC-01; 05-02 must-have truth 2)"
    status: partial
    reason: "In src/csv/csv_read.cpp the D-22 reference was removed but the numbered catalogue it pointed at was not replaced. Lines 73-75 now call three checks (not-found, directory, empty) 'the constructor's Pattern 1 catalogue, in evaluation order'. Line 83 says 'the three messages below are part of that catalogue; do not reword them'. Lines 134-135 call header-not-found 'the tenth entry in this constructor's Pattern 1 catalogue'. A three-entry catalogue has no tenth entry. tests/test_lua_runner_read_csv.cpp:1337 calls a different message (the csv-parser wrapper) 'entry 10', and lines 1383/1396-1397 use '#1', '#2', '#3', '#7'. No source, test or AGENTS.md file lists the numbered catalogue (git grep -i catalogue). So these comments give neither a reason nor a pinning test, and a maintainer told 'do not reword them' cannot tell which messages that covers. This is review finding WR-01."
    artifacts:
      - path: "src/csv/csv_read.cpp"
        issue: "Lines 73-75 and 83 describe a three-entry 'catalogue'. Lines 134-135 cite its 'tenth entry'. The list was only defined by the deleted D-22 reference."
      - path: "tests/test_lua_runner_read_csv.cpp"
        issue: "Line 1337 ('entry 10') and lines 1383 and 1396-1397 ('#1', '#3', '#2', '#7') use catalogue ordinals that no longer resolve anywhere in the repo"
    missing:
      - "csv_read.cpp:134-135: replace the 'tenth entry' sentence with the name of the pinning test (LuaRunner_ReadCsv.HeaderRowPastEndOfFileThrowsExactMessage or whichever test asserts the header-not-found text) and 'do not reword it'"
      - "csv_read.cpp:73-83: describe the three checks as the existence/type/size preconditions, and name the tests that pin their messages instead of 'that catalogue'"
      - "test_lua_runner_read_csv.cpp:1337, 1383, 1396-1397: replace the ordinals with the message names (e.g. 'the in-memory error', 'the options error', 'the escape error', 'file-not-found'), or write the catalogue down once, as csv_write.cpp does"
---

# Phase 5: Path-Policy Test and Docs Verification Report

**Phase Goal:** The path-containment gate `resolve_sandboxed_path` has its own unit test, no planning-ID comment remains in the repo, and the docs, the shipped Lua reference and the `[0.13.0]` CHANGELOG fully describe the finished milestone.
**Verified:** 2026-10-03T19:10:00Z
**Status:** gaps_found (one narrow doc-accuracy gap; all four roadmap success criteria hold)
**Re-verification:** No, this is the initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | ROADMAP SC1: `SandboxedPathTest` calls `resolve_sandboxed_path` through a sol2-free header. It covers containment, escape rejection, the root itself, `:memory:` and the device-name prefix. It is outside `Lua*`. `Lua*` and C API counts match the Phase 4 baseline | ✓ VERIFIED | `src/lua_runner/path_policy.h` includes only `quiver/database.h` and `<string>`. `internal.h:4` includes it, and the duplicate declaration was removed. `path_policy.cpp` differs from base 737b6af only in its first include. The header has no export macro. Link choice: `tests/CMakeLists.txt:58` compiles `path_policy.cpp` into `quiver_tests`, and line 70 adds `src/` as an include dir. `tests/test_sandboxed_path.cpp` has 11 cases: relative, subdirectory and absolute-inside paths, `..` that stays inside, `..` escape, normalised escape, absolute outside, the root itself (`.`, `sub/..`, absolute), a symlink pointing outside, `:memory:`, and the `_WIN32` device name with its exact prefix and a non-empty reason. Runs: SandboxedPathTest 11/11, `Lua*` 477/477 in 12 suites, LuaRunnerCApiTest 27/27. Phase 4's verification recorded 477 and 27 as the corrected baseline |
| 2 | ROADMAP SC2: a repo-wide `git grep` for planning-ID comments outside `.planning/` and `CHANGELOG.md` returns 0 | ✓ VERIFIED | `build/phase5-check/ids.sh` prints `IDS=0 FILES=0`. Its regex covers legacy, milestone and review prefixes, `NN-*.md`, `Phase N` and `C1-C8`/`M1-M16`. An independent run of the same pattern with no lookbehind exclusions finds only the Unicode "C1 control block" (`src/database_describe.cpp:57-77`, `src/AGENTS.md:226-227`, `tests/test_database_ui_metadata.cpp:507-531`, `tests/AGENTS.md:23`). Those are real false positives. A broad `\.planning\|PLAN\.md\|[A-Z]{2,8}-NN` grep finds nothing. Root `AGENTS.md:154` now cites commit `f92af8d` by section title instead of the old `.planning/...PITFALLS.md`, "Pitfall 4" path |
| 3 | ROADMAP SC3: the root, `src/`, `src/c/`, `tests/` and four binding AGENTS.md files describe the `src/lua_runner/` layout, the C7 (empty-array) rule, text-only `load` and the safety flags | ✓ VERIFIED | Root covers the layout, the empty-array rule, text-only `load`/`run()` (lines 84-87) and the flags. `src/AGENTS.md` covers the layout (line 46 tree, line 647 conventions), the empty-array rule (line 808), text-only `load` (lines 730-735) and the flags (lines 796-845: `SOL_SAFE_NUMERICS`, `SOL_ALL_SAFETIES_ON`, `SOL_PRINT_ERRORS=0`, `SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`, `SOL_NO_NIL`). `tests/AGENTS.md` covers text-only `load` (line 38), the `path_policy.cpp` link (line 61) and the getter and stack check both off (lines 157-158). `src/c/AGENTS.md:42` and the julia:91, dart:126, python:114 and js:118 AGENTS.md lines all cover text-only `run()`. Dart (line 176) covers the empty-array rule. The js file (line 37) and julia file (line 160) reference `src/lua_runner/` paths. Each file covers what applies to its own area. That matches the 05-04 truth ("what is true of their area") |
| 4 | ROADMAP SC4: CHANGELOG `[0.13.0]` is complete, with the compare link, no rename entry and no planning ID. `LUA_DB_API_REFERENCE` states the empty-array rule and what the sandbox does not limit. The sync test and six suites are green. The version is 0.13.0 | ✓ VERIFIED | `CHANGELOG.md:8-75` has 5 BREAKING entries: table args type-checked, wrong-type optional args, empty array clears (now with the `quiver.metadata_from_element` sentence), text-only `load`/`run()`, and Release safeties. It has 4 `### Fixed` entries. Line 1375 has the `[0.13.0]` compare link `v0.12.9...v0.13.0`, and tag v0.12.9 exists. The ID/rename grep over the section matches only `expr:rename_agents`, which is a false positive. `bindings/js/src/lua-api.ts:117-122` has the "What the sandbox does not limit" bullet: instructions, memory, wall time and globals persisting across `run()`. Lines 314-319 have the empty-array rule. `bun test test/lua-api-sync.test.ts` passes 6/6. `ReferenceWorkedExampleRunsAndRoundTripsItsOwnData` passes. `build/phase5-check/test-all.txt` was written 15:19, after commit a3d57e4 at 14:58, and shows all six suites PASS. `git diff a3d57e4 HEAD` outside `.planning` is empty. `assert_version.py` reports "All project files at 0.13.0" |
| 5 | 05-01: a mutation check proves the new tests bite | ✓ VERIFIED | `build/phase5-check/mutation-dotdot.txt`: disabling the `..` check makes the 4 escape cases FAIL. `mutation-root.txt` covers dropping the root-itself check. `path_policy.cpp` is back at its base body, per the diff above |
| 6 | 05-01: Linux GCC/Clang build the link shape and pass | ✓ VERIFIED (executor log) | `build/phase5-check/linux_gcc.txt` and `linux_clang.txt` each show `Lua*` 475 run / 474 passed (1 root skip), SandboxedPathTest 10/10 and C API 27/27. That 475 vs 477 matches the two `_WIN32`-only Lua device-name tests. Not re-run here, because this host has no Linux toolchain |
| 7 | 05-02/05-03: changes are comment-only apart from the approved diagnostic strings, and test names and expected values are unchanged | ✓ VERIFIED | Full quiver_tests is 1454 = Phase 4's 1443 + 11 SandboxedPathTest. C API is 543, unchanged. 05-REVIEW independently filtered every non-comment change: only assert/FAIL message text, the new header and the CMake wiring changed |
| 8 | 05-02 truth 2 (DOC-01 quality clause): each removed ID is replaced by its reason or its pinning test | ✗ PARTIAL | See the gap below. The csv_read.cpp catalogue comments and the read_csv test ordinals point at a numbered list that the deleted D-22 reference defined and that no longer exists in the repo |
| 9 | 05-04: STATE.md records the milestone's final counts | ✓ VERIFIED | `.planning/STATE.md:147` records Lua* 477/12, SandboxedPathTest 11 (Win) and 10 (Linux), C API 27, full 1454/543, ID gate 0 and version 0.13.0 |

**Score:** 8/9 truths verified (0 present but behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner/path_policy.h` | sol2-free declaration | ✓ VERIFIED | 18 lines. It includes only `quiver/database.h` and `<string>`, and has no export macro |
| `tests/test_sandboxed_path.cpp` | `class SandboxedPathTest` with 11 cases on Windows | ✓ VERIFIED | Substantive. Every case asserts exact Pattern 1 text built from the throw sites in `path_policy.cpp` |
| `tests/CMakeLists.txt` | test source, the path_policy.cpp copy, src include | ✓ VERIFIED | Lines 54, 58 and 70 |
| `bindings/js/src/lua-api.ts` | "What the sandbox does not limit" bullet | ✓ VERIFIED | Lines 117-122 |
| `CHANGELOG.md` | `metadata_from_element` sentence | ✓ VERIFIED | Inside the empty-array BREAKING entry |
| `src/csv/csv_write.cpp` | catalogue whose entries name the pinning tests | ✓ VERIFIED (with info IN-03) | One entry's pin covers only the prefix and reason. See Anti-Patterns |
| `.planning/STATE.md` | final counts | ✓ VERIFIED | Line 147 |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/lua_runner/internal.h` | `path_policy.h` | quoted include | ✓ WIRED | `internal.h:4`. The old declaration was deleted, so there is one declaration |
| `tests/test_sandboxed_path.cpp` | `src/lua_runner/path_policy.cpp` | compiled into quiver_tests | ✓ WIRED | Builds, links and runs 11/11 against the exported `quiver::Database` |
| `lua-api.ts` | `lua-api-sync.test.ts` | bound-name/stdlib sync | ✓ WIRED | 6/6 pass |
| `test_lua_runner_write_csv.cpp` worked-example test | `lua-api.ts` `## CSV file writing` | reads the reference from disk | ✓ WIRED | 1/1 pass |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Gate unit tests | `quiver_tests --gtest_filter=SandboxedPathTest*` | 11/11 passed (symlink case ran, not skipped) | ✓ PASS |
| Lua baseline holds | `quiver_tests --gtest_filter=Lua*` | 477/477, 12 suites | ✓ PASS |
| C API Lua baseline | `quiver_c_tests --gtest_filter=LuaRunnerCApiTest*` | 27/27 | ✓ PASS |
| Reference sync | `bun test test/lua-api-sync.test.ts` | 6 pass, 0 fail | ✓ PASS |
| Reference worked example | `quiver_tests --gtest_filter=*ReferenceWorkedExample*` | 1/1 | ✓ PASS |
| Planning-ID gate | `bash build/phase5-check/ids.sh` | `IDS=0 FILES=0` | ✓ PASS |
| Version | `uv run python scripts/assert_version.py` | All project files at 0.13.0 | ✓ PASS |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exists, and the plans declare none. Step 7c: SKIPPED.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| TEST-01 | 05-01 | Direct unit test through a sol2-free header, outside `Lua*` | ✓ SATISFIED | Truths 1, 5, 6 |
| DOC-01 | 05-02, 05-03 | Planning IDs replaced repo-wide with their reason or pinning test | ⚠ PARTIAL | The grep is at 0 (truth 2). The replacement quality fails at the csv_read catalogue comments (truth 8, gap) |
| DOC-02 | 05-02, 05-04 | The eight AGENTS.md files describe the layout, empty-array rule, text-only load and flags | ✓ SATISFIED | Truth 3 |
| DOC-03 | 05-04 | `[0.13.0]` section with the BREAKING and Fixed entries | ✓ SATISFIED | Truth 4 |
| DOC-04 | 05-04 | Reference states the empty-array rule and the sandbox's non-limits; sync test green | ✓ SATISFIED | Truth 4 |

REQUIREMENTS.md maps no IDs to Phase 5 beyond these five, so none are orphaned.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/csv/csv_read.cpp` | 73-83, 134-135 | Orphaned catalogue ordinal ("tenth entry" of a three-entry catalogue), and "do not reword" pins a list that does not exist (WR-01) | 🛑 Blocker (DOC-01 quality) | Maintainers cannot tell which messages are pinned. Behavior is unaffected |
| `tests/test_lua_runner_read_csv.cpp` | 1337, 1383, 1396-1397 | The same orphaned ordinals ("entry 10", "#1/#2/#3/#7") | 🛑 Blocker (same gap) | Same as above |
| `tests/test_lua_runner_read_csv.cpp` | 231-232 | Doubled word "shape / shape" (IN-02) | ℹ Info | Cosmetic |
| `src/csv/csv_write.cpp` | 35-36 | A "pinned by" test asserts only the prefix and reason, not the row/cell numbers (IN-03) | ⚠ Warning | Overstates what the test pins |
| `tests/CMakeLists.txt` / `tests/AGENTS.md` | 55-57 / 61-63 | The rationale for the static-link duplicate symbol is technically wrong. The real hazard is the missing PRIVATE compile definitions (IN-01) | ℹ Info | Misleading rationale. The link works |
| `tests/test_sandboxed_path.cpp` | — | No unconditional case for a `<sandbox>_evil` sibling-prefix (IN-04). Only the symlink case covers it, and that case can skip | ℹ Info | SC1's listed coverage (containment, escape, root, `:memory:`, device) is met. This is extra hardening |

No TBD, FIXME or XXX markers appear in phase-touched files.

### Human Verification Required

None. Every truth was checked programmatically.

### Gaps Summary

The phase goal holds on every roadmap success criterion:
- the gate has its own sol2-free unit suite, which bites under mutation and keeps the `Lua*` 477 and C API 27 baselines;
- the repo-wide planning-ID grep is 0;
- the eight AGENTS.md files, the CHANGELOG `[0.13.0]` section and the shipped Lua reference describe the milestone;
- the sync test and the six suites are green at 0.13.0.

One gap remains. It is DOC-01's own quality clause ("replaced with their one-line reason or the test that pins them"), which also appears as 05-02 must-have truth 2. When D-22 was stripped from `src/csv/csv_read.cpp`, its numbered Reader message catalogue went with it, but the ordinals that depended on it stayed:
- the "tenth entry" in csv_read.cpp;
- "entry 10", "#1", "#2", "#3" and "#7" in the read_csv tests.

csv_read.cpp also redefines the catalogue as three messages and tells maintainers not to reword them. The result contradicts itself and cannot be checked. It is a comment-only fix to two files, and it is the same work as review finding WR-01, which is already queued for the automatic fix pass. Closing it needs no code or test-behaviour change. IN-02 (the doubled word) sits in the same test file and can go in the same edit.

---

_Verified: 2026-10-03T19:10:00Z_
_Verifier: Claude (gsd-verifier)_
