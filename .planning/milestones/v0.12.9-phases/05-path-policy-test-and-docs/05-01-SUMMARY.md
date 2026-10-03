---
phase: 05-path-policy-test-and-docs
plan: 01
subsystem: testing
tags: [lua-runner, sandbox, path-policy, gtest, cmake]
status: complete
requires: []
provides:
  - "src/lua_runner/path_policy.h: sol2-free declaration of resolve_sandboxed_path"
  - "SandboxedPathTest (11 on Windows, 10 elsewhere) in quiver_tests"
  - "Gitignored Phase 5 harness build/phase5-check/ (PHASE_BASE, lists, golden-before, ids.regex/ids.sh, gate.sh, linux.sh)"
affects: [05-02, 05-03, 05-04]
tech-stack:
  added: []
  patterns:
    - "A test reaches a hidden library function by compiling its own copy of a one-function TU (path_policy.cpp) into quiver_tests"
key-files:
  created:
    - src/lua_runner/path_policy.h
    - tests/test_sandboxed_path.cpp
  modified:
    - src/lua_runner/path_policy.cpp
    - src/lua_runner/internal.h
    - tests/CMakeLists.txt
    - src/CMakeLists.txt
    - src/AGENTS.md
    - tests/AGENTS.md
decisions:
  - "quiver_tests compiles its own copy of src/lua_runner/path_policy.cpp; nothing new is exported from libquiver, and the link works on MSVC, GCC 13 and Clang 18/libc++"
  - "path_policy.cpp must stay a one-function file (a static QUIVER_BUILD_SHARED=OFF link would otherwise define a symbol twice); stated in a tests/CMakeLists.txt comment and tests/AGENTS.md"
metrics:
  duration: "40 min"
  completed: 2026-10-03
actuals:
  tokens: 3100
  tasks: 2
  commits: 3
---

# Phase 5 Plan 01: Path-Policy Unit Test Summary

`resolve_sandboxed_path` now has its own unit test, `SandboxedPathTest`. It calls the function directly through a new sol2-free header, `src/lua_runner/path_policy.h`, and `quiver_tests` compiles its own copy of `path_policy.cpp`. The test asserts the exact Pattern 1 message for containment, escapes, the root itself, `:memory:` and the device-name prefix. Two mutations of the gate make it fail. It passes on MSVC Debug and Release, GCC 13 and Clang 18/libc++.

## What was built

- **`src/lua_runner/path_policy.h`** (new): include guard, `quiver/database.h` + `<string>` only, one declaration in `quiver::lua_internal`, and no export macro. `internal.h` includes it, and its own declaration line is deleted.
- **`src/lua_runner/path_policy.cpp`**: line 1 now includes `lua_runner/path_policy.h` instead of `internal.h`. Beyond that, the only change is the blank line clang-format requires after a main header (see Deviations). The body and messages are byte-identical.
- **`tests/CMakeLists.txt`**: adds `test_sandboxed_path.cpp`, appends `${CMAKE_SOURCE_DIR}/src/lua_runner/path_policy.cpp` with the static-link comment, and adds `target_include_directories(quiver_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)`. The test TU's compile command has no sol2 include path (checked in `build/compile_commands.json`).
- **`tests/test_sandboxed_path.cpp`**: `SandboxedPathTest : LuaSandboxTest`, with expectations built from `weakly_canonical(sandbox)`.
- **`src/CMakeLists.txt`**: lists `lua_runner/path_policy.h`.
- **`src/AGENTS.md`**: adds a `path_policy.h` line to the file map, and the "Filesystem sandbox" bullet now names `SandboxedPathTest`. **`tests/AGENTS.md`**: a new bullet describes the test, its link shape, the one-function constraint, its suite name staying outside `Lua*`, and the symlink skip.

## Harness (gitignored, `build/phase5-check/`)

- `PHASE_BASE` = `737b6af`; `list-tests.txt` (1443), `list-c.txt` (543); `golden-before/` (golden debug at the base: `GOLDEN debug OK`, so no base drift).
- `ids.regex` holds the research gate G plus `|IN|WR|CR|HARD|CLEAN|REN`. Base sanity: `bash build/phase5-check/ids.sh | tail -1` printed **`IDS=256 FILES=19`**, and it still does after this plan.
- `gate.sh` and `linux.sh` work as the plan specifies. `gate.sh` strips CRs on both sides before the list comparison (see Deviations).

## Results

| Case | Win Debug | Win Release | GCC 13 | Clang 18/libc++ |
|------|-----------|-------------|--------|-----------------|
| RelativePathResolvesInsideTheDatabaseDirectory | PASS | PASS | PASS | PASS |
| SubdirectoryIsAllowed | PASS | PASS | PASS | PASS |
| AbsolutePathInsideIsAllowed | PASS | PASS | PASS | PASS |
| DotDotThatStaysInsideIsAllowed | PASS | PASS | PASS | PASS |
| DotDotEscapeIsRejected | PASS | PASS | PASS | PASS |
| NormalisedEscapeIsRejected | PASS | PASS | PASS | PASS |
| AbsolutePathOutsideIsRejected | PASS | PASS | PASS | PASS |
| RootItselfIsRejected (all three elements kept, including absolute `sandbox.string()`) | PASS | PASS | PASS | PASS |
| SymlinkPointingOutsideIsRejected | PASS (ran; Developer Mode) | PASS | PASS (root) | PASS (root) |
| InMemoryDatabaseIsRejectedBeforeContainment | PASS | PASS | PASS | PASS |
| DeviceNameIsReportedWithPrefix (`_WIN32` only) | PASS | PASS | n/a | n/a |

No case was SKIPPED on any platform.

Gate lines:
- Task 1: `P5 GATE PASS lua=477 sandboxed=1 capi=27 tests=1444 c=543`
- Task 2: `P5 GATE PASS lua=477 sandboxed=11 capi=27 tests=1454 c=543` (re-run after the loop fix: same line)
- `WAVE GATE PASS lua=477 tidy=14` (Release Lua* 477 / C API 27, `GOLDEN release OK`, tidy 14 warnings on `src/lua_runner/`, the same baseline set, GCC 14 syntax pass clean)
- Golden debug: `GOLDEN debug OK`, and `diff -r golden-before out/debug` is empty after both tasks.
- Linux (at `aac7c2b`), GCC 13.3.0 and Clang 18.1.3: `LISTED=475`, `[  PASSED  ] 474 tests.` + `[  SKIPPED ] 1 test.`, `SANDBOXED=10` with `[  PASSED  ] 10 tests.`, C API `[  PASSED  ] 27 tests.`, `exit=0`, and no warning from `test_sandboxed_path.cpp`.
- `bun test test/lua-api-sync.test.ts`: 6 pass, so the new header did not disturb the token count.

Mutation checks (`path_policy.cpp` restored with `git checkout`; `git diff --exit-code` passes afterwards):
- `mutation-dotdot.txt` (`".."` comparison forced false):
  `[  FAILED  ] SandboxedPathTest.DotDotEscapeIsRejected`, `NormalisedEscapeIsRejected`, `AbsolutePathOutsideIsRejected`, `SymlinkPointingOutsideIsRejected`
- `mutation-root.txt` (`rel == "."` term dropped): `[  FAILED  ] SandboxedPathTest.RootItselfIsRejected` (`"."` and `"sub/.."` both reported `<no throw>`)

Tracer gate (auto, delegated self-verification): the tracer's verify was re-run end-to-end after its commit and passed. The sol2-free claim was checked against the compile command. The mutations above then showed the test exercises a live copy of the gate.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] clang-format requires a blank line after the main header in `path_policy.cpp`**
- **Found during:** Task 1
- **Issue:** With `lua_runner/path_policy.h` as line 1, `IncludeBlocks: Regroup` treats it as the main header and requires a blank line after it. The `--dry-run --Werror` check failed.
- **Fix:** Applied clang-format. The file's diff is now the include swap plus one blank line. The plan's `grep -cE '^[-+][^-+]'` count is still 2, and the body is untouched.
- **Commit:** 2692d1b

**2. [Rule 1 - Bug] `-Wrange-loop-construct` on GCC 13 / Clang 18**
- **Found during:** Task 2 Linux run
- **Issue:** `for (const std::string path : {...})` copies each element, and both Linux compilers warned.
- **Fix:** The loop variable is now `const std::string&` in `RootItselfIsRejected` and `DeviceNameIsReportedWithPrefix`. Re-ran the Windows gate, Release and both Linux toolchains, and the warning is gone.
- **Commit:** aac7c2b

**3. [Rule 3 - Blocking] Harness: CRLF in the saved test lists**
- **Found during:** Task 1
- **Issue:** MSYS awk drops CRs, so the filtered list never byte-matched `list-tests.txt`.
- **Fix:** `gate.sh` strips CRs on both sides before `cmp`. This affects the gitignored harness only.

**4. Placement:** `lua_runner/path_policy.h` sits right after `lua_runner/path_policy.cpp` in `QUIVER_SOURCES`, which keeps the list alphabetical, rather than literally beside `internal.h`.

## TDD Gate Compliance

The tests pin behaviour that already exists, so there was no failing RED run before an implementation. The two recorded mutations stand in for RED: each pin was shown to fail against a weakened gate. No `feat` commit exists because no production behaviour changed.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: src/lua_runner/path_policy.h, tests/test_sandboxed_path.cpp, build/phase5-check/{PHASE_BASE,list-tests.txt,list-c.txt,golden-before,ids.regex,ids.sh,gate.sh,linux.sh,mutation-dotdot.txt,mutation-root.txt,linux_gcc.txt,linux_clang.txt}
- FOUND commits: 2692d1b, 3bcc194, aac7c2b
- No `.planning/config.json` or `.gsd/` path in `git log --name-only 737b6af..HEAD`
