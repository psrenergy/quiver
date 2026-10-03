---
phase: 05-path-policy-test-and-docs
fixed_at: 2026-10-03T18:42:52Z
review_path: .planning/phases/05-path-policy-test-and-docs/05-REVIEW.md
iteration: 1
findings_in_scope: 1
fixed: 1
skipped: 0
status: all_fixed
---

# Phase 05: Code Review Fix Report

**Fixed at:** 2026-10-03T18:42:52Z
**Source review:** .planning/phases/05-path-policy-test-and-docs/05-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 1 (WR-01; scope critical_warning)
- Fixed: 1
- Skipped: 0
- Extra fix applied on request: IN-02 (same comment area, called out by the verifier)

## Fixed Issues

### WR-01: The rewritten Reader comments now contradict each other about what "the catalogue" is

**Files modified:** `src/csv/csv_read.cpp`, `tests/test_lua_runner_read_csv.cpp`
**Commit:** 3c313dd
**Applied fix:** Fixed every site the review and the verifier listed. The edits change comments only.
- `csv_read.cpp:73-75`: the three checks are now described as the existence, type and size preconditions, not as "the constructor's Pattern 1 catalogue".
- `csv_read.cpp:83`: "part of that catalogue; do not reword them" now names the tests that pin those messages exactly: `LuaRunner_ReadCsv.MissingFileThrowsForReadCsv`, `.DirectoryAsPathThrowsForReadCsv` and `.EmptyFileThrows`.
- `csv_read.cpp:134-135`: "the tenth entry" now reads "Pinned by `LuaRunner_ReadCsv.HeaderRowPastEndOfFileThrowsExactMessage`; do not reword it."
- `test_lua_runner_read_csv.cpp:1337`: "entry 10" now names the message, the csv-parser wrapper's "cannot read file". The comment also names the three earlier messages (not-found, directory, empty).
- `test_lua_runner_read_csv.cpp:1383, 1396-1397`: `#1`, `#2`, `#3` and `#7` are replaced by message names: the in-memory error, the options error, the escape error and file-not-found.
- Two section headers that referred to the undefined catalogue were renamed. "the remaining Reader catalogue entries" became "the remaining Reader messages", and "catalogue ordering + adjacency" became "error ordering + adjacency".

I grepped for each cited test name, and all four exist. `src/lua_runner/csv.cpp:285` still mentions "message catalogue", but it points at the catalogue written out in `src/csv/csv_write.cpp`, which exists, so I left it alone.

### IN-02: Doubled word left by the rewrite (applied on request)

**Files modified:** `tests/test_lua_runner_read_csv.cpp`
**Commit:** e2eca2a
**Applied fix:** "the real Maranhao file's shape / shape." now reads "the real Maranhao file's shape."

## Skipped Issues

### IN-01: The rule against a duplicate symbol in a static link describes a failure that archive linking cannot produce

**File:** `tests/CMakeLists.txt:55-57`, `tests/AGENTS.md:61-63`
**Reason:** Out of scope (fix_scope critical_warning; the orchestrator said to leave it).
**Original issue:** The duplicate-symbol rationale is technically wrong. The real hazard is that the test copy of `path_policy.cpp` is compiled without `quiver`'s PRIVATE compile definitions.

### IN-03: One "pinned by" citation pins only part of its message

**File:** `src/csv/csv_write.cpp:35-36`
**Reason:** Out of scope (fix_scope critical_warning; the orchestrator said to leave it).
**Original issue:** `NonFiniteNumberCellIsPrefixedWriteRowError` asserts only the prefix and the reason, not the `row <N> cell #<M>` part.

### IN-04: No unconditional SandboxedPathTest case covers a sibling directory whose name starts with the sandbox path

**File:** `tests/test_sandboxed_path.cpp:72-76`
**Reason:** Out of scope (fix_scope critical_warning; the orchestrator said to leave it).
**Original issue:** The `<sandbox>_evil` prefix bypass is tested only by the symlink case, and that case can skip.

## Verification

All checks ran in the main checkout (`C:/Development/Quiver/quiver1`, branch `rs/runner`). No worktree was used, so you can reproduce these numbers from that tree.
- No string literal, test name or embedded Lua script line changed. Every edit is to a C++ `//` comment.
- `uvx clang-format@22.1.8 --dry-run --Werror` is clean on both touched files.
- `cmake --build build --config Debug` succeeded.
- `quiver_tests --gtest_filter=LuaRunner_ReadCsv*`: 72/72 passed.
- Full `quiver_tests`: 1454/1454 passed in 44 suites, unchanged from the phase baseline.
- The planning-ID gate (`bash build/phase5-check/ids.sh`) still prints `IDS=0 FILES=0`.
- After the build, `grep -i 'catalogue|entry [0-9]|tenth|(#[0-9]'` finds nothing in `csv_read.cpp` or `test_lua_runner_read_csv.cpp`.

---

_Fixed: 2026-10-03T18:42:52Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
