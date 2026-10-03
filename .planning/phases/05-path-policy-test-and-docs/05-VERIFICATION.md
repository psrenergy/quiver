---
phase: 05-path-policy-test-and-docs
verified: 2026-10-03T19:40:00Z
status: passed
score: 9/9 must-haves verified
behavior_unverified: 0
overrides_applied: 0
gap_closure_source: "delegated (Claude, adversarial) — gaps_found routed per the self-verify rule: fixed via code-review --fix, then independently re-verified"
re_verification:
  previous_status: gaps_found
  previous_score: 8/9
  previous_verified: 2026-10-03T19:10:00Z
  gaps_closed:
    - "Every removed planning ID that carried meaning is replaced by its one-line reason or by the name of the test that pins it (DOC-01; 05-02 must-have truth 2). Closed by 3c313dd (WR-01) and e2eca2a (IN-02), both comment-only."
  gaps_remaining: []
  regressions: []
---

# Phase 5: Path-Policy Test and Docs Verification Report

**Phase Goal:** The path-containment gate `resolve_sandboxed_path` has its own unit test, no planning-ID comment remains in the repo, and the docs, the shipped Lua reference and the `[0.13.0]` CHANGELOG fully describe the finished milestone.
**Verified:** 2026-10-03T19:40:00Z
**Status:** passed
**Re-verification:** Yes, after gap closure (previous run: gaps_found, 8/9)

## History

The previous verification (2026-10-03T19:10:00Z) found one gap, in DOC-01's quality clause ("replaced with their one-line reason or the test that pins them"). Stripping D-22 from `src/csv/csv_read.cpp` also removed the numbered Reader message catalogue that D-22 pointed at, but the ordinals that depended on it stayed behind:
- `csv_read.cpp:73-83` called three checks "the constructor's Pattern 1 catalogue" and said "do not reword them".
- `csv_read.cpp:134-135` cited that catalogue's "tenth entry".
- `tests/test_lua_runner_read_csv.cpp:1337` said "entry 10", and lines 1383 and 1396-1397 said `#1`, `#2`, `#3` and `#7`.

None of these resolved anywhere in the repo. Review finding WR-01 covered the same sites.

Two commits closed it: 3c313dd (WR-01) and e2eca2a (IN-02, a doubled word). `git diff 631a1d5..HEAD -- src tests` touches only `src/csv/csv_read.cpp` and `tests/test_lua_runner_read_csv.cpp`, and only in `//` comment lines. No string literal, test name, assert text or raw-string Lua body changed. The single net added line is a C++ comment outside any `R"(...)"`, and `git grep -nE '\]:[0-9]+:' -- tests` is still empty, so no Lua line-number assertion could have shifted.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | ROADMAP SC1: `SandboxedPathTest` calls `resolve_sandboxed_path` through a sol2-free header and covers containment, escape, the root, `:memory:` and the device-name prefix. It sits outside `Lua*`, and the `Lua*` and C API counts match the Phase 4 baseline | ✓ VERIFIED (regression) | No change to `src/lua_runner/` or `tests/test_sandboxed_path.cpp` since the last verification. Run at HEAD: SandboxedPathTest 11/11, nothing skipped in the full run, so the symlink case ran. `Lua*` 477/477 in 12 suites. LuaRunnerCApiTest 27/27 |
| 2 | ROADMAP SC2: a repo-wide planning-ID `git grep`, outside `.planning/` and `CHANGELOG.md`, returns 0 | ✓ VERIFIED (regression) | `bash build/phase5-check/ids.sh` at HEAD prints `IDS=0 FILES=0` |
| 3 | ROADMAP SC3: the eight AGENTS.md files describe the `src/lua_runner/` layout, the empty-array rule, text-only `load` and the safety flags | ✓ VERIFIED (regression) | No AGENTS.md file changed since 631a1d5 (diff stat lists only the two comment-only files plus `.planning` docs). The previous evidence stands |
| 4 | ROADMAP SC4: CHANGELOG `[0.13.0]` is complete, with its compare link, no rename entry and no IDs. `LUA_DB_API_REFERENCE` states the empty-array rule and what the sandbox does not limit. The sync test and suites are green. The version is 0.13.0 | ✓ VERIFIED (regression) | `CHANGELOG.md` and `lua-api.ts` are unchanged since 631a1d5. `bun test test/lua-api-sync.test.ts` passes 6/6. `assert_version.py` reports 0.13.0. Full `quiver_tests` passes 1454/1454 in 44 suites, and `quiver_c_tests` passes 543/543 |
| 5 | 05-01: a mutation check proves the new tests bite | ✓ VERIFIED (unchanged) | The mutation logs in `build/phase5-check/` still apply, since `path_policy.cpp` and the test file have not changed |
| 6 | 05-01: Linux GCC/Clang build the link shape and pass | ✓ VERIFIED (executor log, unchanged) | `linux_gcc.txt` and `linux_clang.txt`. Not re-run, because this host has no Linux toolchain and the fix commits touched no build or source logic |
| 7 | 05-02/05-03: the changes are comment-only apart from the approved diagnostic strings, and test names and expected values are unchanged | ✓ VERIFIED | Totals are still 1454 and 543. The fix commits are comment-only, as checked line by line in the diff above |
| 8 | 05-02 truth 2 (DOC-01 quality clause): each removed ID is replaced by its reason or its pinning test | ✓ VERIFIED (gap closed) | `csv_read.cpp:73-75` now describes "the existence, type and size preconditions", with no catalogue framing. Lines 83-85 name the pinning tests `MissingFileThrowsForReadCsv`, `DirectoryAsPathThrowsForReadCsv` and `EmptyFileThrows`. Lines 136-137 name `HeaderRowPastEndOfFileThrowsExactMessage`. I checked each cited test. Each exists and asserts the full message for its throw site: `"Cannot read_csv: file not found: missing.csv"` (test 1222, throw 95), `"...path is a directory: adir"` (1243, throw 105), `"...file 'empty.csv' is empty"` (324, throw 115), `"...header row 99 not found in file 'three.csv'"` (358, throw 139). Rewording any of these messages would fail its cited test. In the test file, "entry 10" now says "the csv-parser wrapper's 'cannot read file'", which names the message at `csv_read.cpp:126`. `#1`/`#3` and `#2`/`#7` are now the in-memory, options, escape and file-not-found errors, each named by its message. A repo-wide `git grep -iE 'catalog(ue)?\|tenth entry\|entry [0-9]+\|#[0-9]+\)'` outside `.planning/` finds no remaining ordinal. Every "catalogue" hit left points at a list that exists: the written-out catalogue in `src/csv/csv_write.cpp:9`/`csv_write.h:21`, which `src/lua_runner/csv.cpp:285` also references; the write_csv catalogue suite; and two `tests/AGENTS.md` lines (75, 94) whose generic "catalogue message" wording predates the phase and is explained in the same sentence ("the three preconditions"). None of those is an ordinal or the remnant of a removed planning ID |
| 9 | 05-04: STATE.md records the milestone's final counts | ✓ VERIFIED (unchanged) | The counts recorded there (1454/543, Lua* 477, SandboxedPathTest 11, C API 27, ID gate 0, 0.13.0) match the HEAD runs above |

**Score:** 9/9 truths verified (0 present but behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner/path_policy.h` | sol2-free declaration | ✓ VERIFIED | Unchanged since the last verification |
| `tests/test_sandboxed_path.cpp` | 11 cases on Windows | ✓ VERIFIED | 11/11 pass at HEAD |
| `tests/CMakeLists.txt` | test source, the path_policy.cpp copy, src include | ✓ VERIFIED | Unchanged |
| `bindings/js/src/lua-api.ts` | sandbox-limits bullet and empty-array rule | ✓ VERIFIED | Unchanged. The sync test passes 6/6 |
| `CHANGELOG.md` | complete `[0.13.0]` section | ✓ VERIFIED | Unchanged |
| `src/csv/csv_read.cpp` | comments name the reason or the pinning test | ✓ VERIFIED | Fixed by 3c313dd. All four cited tests exist and pin their messages |
| `tests/test_lua_runner_read_csv.cpp` | no dangling ordinals | ✓ VERIFIED | Fixed by 3c313dd and e2eca2a. `LuaRunner_ReadCsv*` passes 72/72 |
| `src/csv/csv_write.cpp` | catalogue whose entries name pinning tests | ✓ VERIFIED (info IN-03) | One "pinned by" entry overstates its coverage slightly. See Anti-Patterns |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/lua_runner/internal.h` | `path_policy.h` | quoted include | ✓ WIRED | Unchanged |
| `tests/test_sandboxed_path.cpp` | `src/lua_runner/path_policy.cpp` | compiled into quiver_tests | ✓ WIRED | 11/11 |
| `lua-api.ts` | `lua-api-sync.test.ts` | bound-name/stdlib sync | ✓ WIRED | 6/6 |
| `csv_read.cpp` "do not reword" comments | `LuaRunner_ReadCsv` tests | named test citations | ✓ WIRED | Each cited test asserts the exact message text from its throw site |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Build is current | `cmake --build build --config Debug` | `ninja: no work to do.` | ✓ PASS |
| Planning-ID gate | `bash build/phase5-check/ids.sh` | `IDS=0 FILES=0` | ✓ PASS |
| No Lua line-number asserts | `git grep -nE '\]:[0-9]+:' -- tests` | empty | ✓ PASS |
| Gate unit tests | `quiver_tests --gtest_filter=SandboxedPathTest*` | 11/11 | ✓ PASS |
| Lua baseline | `quiver_tests --gtest_filter=Lua*` | 477/477, 12 suites | ✓ PASS |
| Touched suite | `quiver_tests --gtest_filter=LuaRunner_ReadCsv*` | 72/72 | ✓ PASS |
| Full core suite | `quiver_tests` | 1454/1454, 44 suites, no skips or failures | ✓ PASS |
| C API suite | `quiver_c_tests` | 543/543 | ✓ PASS |
| C API Lua baseline | `quiver_c_tests --gtest_filter=LuaRunnerCApiTest*` | 27/27 | ✓ PASS |
| Reference sync | `bun test test/lua-api-sync.test.ts` | 6 pass, 0 fail | ✓ PASS |
| Version | `uv run python scripts/assert_version.py` | 0.13.0 | ✓ PASS |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exists, and the plans declare none. Step 7c: SKIPPED.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| TEST-01 | 05-01 | Direct unit test through a sol2-free header, outside `Lua*` | ✓ SATISFIED | Truths 1, 5, 6 |
| DOC-01 | 05-02, 05-03 | Planning IDs replaced repo-wide with their reason or pinning test | ✓ SATISFIED | Truth 2 (gate at 0) and truth 8 (replacements resolve, gap closed) |
| DOC-02 | 05-02, 05-04 | The eight AGENTS.md files describe the layout, empty-array rule, text-only load and flags | ✓ SATISFIED | Truth 3 |
| DOC-03 | 05-04 | `[0.13.0]` section with BREAKING and Fixed entries | ✓ SATISFIED | Truth 4 |
| DOC-04 | 05-04 | Reference states the empty-array rule and the sandbox's non-limits; sync test green | ✓ SATISFIED | Truth 4 |

REQUIREMENTS.md maps no other IDs to Phase 5, so none are orphaned.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/csv/csv_write.cpp` | 35-36 | A "pinned by" test asserts the prefix and the reason but not the `row <N> cell #<M>` part (IN-03) | ℹ Info | The citation still names a real test that fails if the message's reason is reworded, so it meets DOC-01's "the test that pins them". It only overstates how much of the text is pinned. The previous report's Warning label is lowered to match the reviewer's Info |
| `tests/CMakeLists.txt` / `tests/AGENTS.md` | 55-57 / 61-63 | The static-link duplicate-symbol rationale is technically wrong. The real hazard is the missing PRIVATE compile definitions (IN-01) | ℹ Info | The rule it backs ("keep path_policy.cpp to the one sol2-free function") is still safe. This is outside DOC-02's listed topics (layout, empty-array rule, text-only load, flags), and the link works |
| `tests/test_sandboxed_path.cpp` | 72-76 | No unconditional `<sandbox>_evil` sibling-prefix case (IN-04) | ℹ Info | SC1's listed coverage is met, and the code compares whole components. On this host the symlink case ran and covers the prefix case. This is extra hardening, not a criterion |
| `src/csv/csv_read.cpp` | 83 | "pinned exactly" while `expect_lua_error` matches substrings | ℹ Info | A reword is caught. Only an appended suffix would not be. Raised by the reviewer and not worth a finding |

No TBD, FIXME or XXX markers appear in the phase-touched files. No new problems appear in the fix hunks.

**Do the three open info findings defeat a success criterion?** No.
- IN-04 is a coverage addition beyond SC1's enumerated list.
- IN-01 concerns a rationale outside SC3/DOC-02's scope.
- IN-03 still cites a real, resolvable pinning test, which is all DOC-01 asks for.

All three are worth a follow-up, and none is a gap.

### Human Verification Required

None. Every truth was checked programmatically.

### Gaps Summary

There are no gaps. The DOC-01 replacement-quality gap from the previous run is closed. The Reader comments name four existing tests that each assert their message in full, and the test file names each message instead of using an ordinal. No unresolvable catalogue ordinal remains anywhere outside `.planning/`. The fix commits are comment-only. At HEAD, with the build current, every regression check matches the phase baseline: ID gate 0, `Lua*` 477, SandboxedPathTest 11, full 1454, C API 543, LuaRunnerCApiTest 27, sync 6/6, version 0.13.0.

---

_Verified: 2026-10-03T19:40:00Z_
_Verifier: Claude (gsd-verifier)_
