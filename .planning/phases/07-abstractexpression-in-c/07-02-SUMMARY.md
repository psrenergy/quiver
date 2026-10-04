---
phase: 07-abstractexpression-in-c
plan: 02
subsystem: expression
status: complete
tags: [verification, linux, release, tidy, perf, baseline]
requires: [07-01]
provides:
  - "Linux GCC 13 and Clang 18/libc++ evidence for Phase 7 (full quiver_tests, quiver_c_tests, Lua*)"
  - "Windows Release + tidy + six-suite phase gate (build/abstract-check/phase_gate.sh)"
  - "Phase 8 baselines: Release warnings of src/lua_runner and 1M f:write / f:read medians, binaries in build/perf-phase7/"
affects: [08-typed-lua-parameters]
tech-stack:
  added: []
  patterns:
    - "each Linux result file starts with COMMIT=<sha>; the phase gate rejects one whose code differs from HEAD's"
key-files:
  created: []
  modified:
    - .planning/STATE.md
decisions:
  - "No fix(07-02) commit: both Linux toolchains built and passed HEAD's code with only the two pre-existing warnings"
  - "Phase 8 baselines: f:write 1M median 2174 ms, f:read 1M median 2082 ms, 0 Release warnings in src/lua_runner (C4702: 0)"
metrics:
  duration: 75min
  completed: 2026-10-04
  tasks: 3
  files: 1
actuals:
  tokens: 9000
  tasks: 3
  commits: 2
---

# Phase 7 Plan 02: Linux, Release and Phase 8 Baselines Summary

Phase 7's code builds and passes every suite on Linux GCC 13, Linux Clang 18/libc++ and Windows Release with no new
warning or tidy pair, and Phase 8 has its two recorded baselines (1M `f:write` 2174 ms, 1M `f:read` 2082 ms; 0 Release
warnings in `src/lua_runner`) with the exact binaries kept.

## Linux (Task 1)

Both runs built `git archive` of `f2769c3` (`COMMIT=f2769c3af094c430a31ef0b7cabdf686b22ced06`; `git diff --quiet f2769c3 HEAD
-- include src tests cmake CMakeLists.txt` exits 0), fresh `ubuntu:24.04` containers, run concurrently, Ninja default
parallelism. Evidence: `build/abstract-check/linux_{gcc,clang}.txt`.

| | GCC | Clang |
|---|---|---|
| Compiler | `c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` | `Ubuntu clang version 18.1.3`, `CMAKE_CXX_FLAGS=-stdlib=libc++` |
| `Lua*` (`LISTED`) | 475: 474 passed, 1 skipped | 475: 474 passed, 1 skipped |
| `LuaRunnerCApiTest` | 27 passed | 27 passed |
| `ExpressionFixture` (`EXPR_LISTED`) | 125 (= Windows) | 125 |
| full `quiver_tests` (`FULL_LISTED`) | 1459: 1456 passed, 3 skipped, 0 failed | 1459: 1456 passed, 3 skipped, 0 failed |
| full `quiver_c_tests` (`CAPI_LISTED`) | 543: 543 passed | 543: 543 passed |
| Warnings | only `time_properties.cpp` `-Wreturn-type` (x2, pre-existing) | only `test_migrations.cpp` `-Wself-assign-overloaded` (pre-existing) |
| `exit` | 0 | 0 |

No ambiguity on the base-parameter `==`/`!=`, no `-Woverloaded-virtual`, no missing override, no missing vtable or
typeinfo under `-fvisibility=hidden`. Linux lists 4 fewer `quiver_tests` than Windows (1459 vs 1463): 2 Windows-only
`Lua*` tests, as in v0.12.9, plus 2 other platform-conditional tests; 3 are skipped at run time.

## Windows phase gate (Task 2)

```
GATE PASS expr=125 tests=1463 lua=477 capi=543
PHASE GATE PASS expr=125 tests=1463 lua=477 capi=543 linux=gcc,clang tidy_removed=1
```

- Release: ExpressionFixture 125, quiver_tests 1463 (1463 passed), Lua* 477 in 12 suites, SandboxedPathTest 11,
  quiver_c_tests 543 (543 passed), ExpressionCApiFixture 73, LuaRunnerCApiTest 27.
- `GOLDEN debug OK`, `EQ debug OK`, `GOLDEN release OK`, `EQ release OK` (`[true,true,false]`).
- Tidy (17 files): no new `(check, source line)` pair, no new header `(file, check)` pair, 0 `modernize-use-override`,
  0 `clang-diagnostic-error`. One base pair removed, as expected:
  `modernize-return-braced-init-list  return Expression(o.as<BinaryFile&>());` (in `to_expression`; the constructor is
  now `explicit`, so a braced return no longer applies).
- Root `AGENTS.md` and `bindings/` unchanged since BASE; `assert_version.py` all at 0.13.0.
- `scripts/test-all.bat` (Dart hook cache cleared first): C++, C API, Julia, Dart, JavaScript, Python all PASS,
  `All tests PASSED` (`build/abstract-check/test-all.txt`). `bun test test/lua-api-sync.test.ts`: 6 pass, 0 fail.

## Suite counts (EXPR-06)

| Build | quiver_tests | ExpressionFixture | quiver_c_tests |
|---|---|---|---|
| Windows Debug | 1463 | 125 | 543 |
| Windows Release | 1463 | 125 | 543 |
| Linux GCC 13 | 1459 (3 skipped) | 125 | 543 |
| Linux Clang 18 | 1459 (3 skipped) | 125 | 543 |

## Phase 8 baselines (Task 3)

**Release warnings** (`build/abstract-check/release_warnings.sh`):
`RELEASE WARNINGS tus=17 total=0 c4702=0 msvc=14.51.36231`. `release-warnings.txt` is empty: MSVC printed no warning
while recompiling the 17 `src/lua_runner` TUs.

**Binaries** (`build/perf-phase7/`, `COMMIT` = `f2769c3af094c430a31ef0b7cabdf686b22ced06`):

```
2dc91ffd58753cb92e09637c1e3883d14debd0a16de3182b798ffdb6da6c9a75  libquiver.dll
a9b1d08b07856cbc970f2326f21545c8181675e28fd01dca71f89d7be7159d1a  libquiver_c.dll
15b790ec61068de2d5c96a0fcc0ff0d7601bba05c19bdfba5d07784b5ee0f75f  quiver_cli.exe
```

**Benchmark** (`bench.sh phase7=build/perf-phase7/quiver_cli.exe`, 1000 x 1000 x 1-label file, one untimed warm-up,
five rounds, no container running):

| Workload | Runs (ms, round order) | Sorted | Median |
|---|---|---|---|
| `file_write` (1M `f:write`) | 2184, 1993, 2174, 2336, 2117 | 1993 2117 2174 2184 2336 | **2174 ms** |
| `file_read` (1M `f:read`) | 1964, 1857, 2082, 2255, 2108 | 1857 1964 2082 2108 2255 | **2082 ms** |

Phase 8 reruns `bench.sh phase7=build/perf-phase7/quiver_cli.exe phase8=build/release/bin/quiver_cli.exe` (interleaved)
and `release_warnings.sh`, then diffs. Both are in the STATE.md PR notes.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 | none | harness and evidence only (gitignored `build/`) |
| 2 | none | harness and evidence only |
| 3 | 5a8f5e7 | docs(07-02): record the Phase 8 perf and Release warning baselines |

No `fix(07-02)` commit was needed.

## Deviations from Plan

**1. [Rule 3 - Harness] `release_warnings.sh` TU count regex**
- **Found during:** Task 3
- **Issue:** Ninja on Windows prints `lua_runner\x.cpp.obj` (backslashes), so the planned `lua_runner/` pattern counted 0.
- **Fix:** match `lua_runner[\\/]`; the basename-reducing sed accepts both separators too. Harness only (gitignored).

Otherwise executed as written.

## Known Stubs

None.

## Self-Check: PASSED

- `build/abstract-check/{linux_gcc,linux_clang,tidy,tidy-pairs,tidy-removed,test-all,release-warnings}.txt`,
  `phase_gate.sh`, `release_warnings.sh`, `perf/{bench.sh,runs.txt}`, `build/perf-phase7/{COMMIT,SHA256SUMS}` exist.
- Commit 5a8f5e7 present in `git log`.
