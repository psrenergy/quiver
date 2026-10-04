---
phase: 08-typed-expression-parameters-in-lua
plan: 02
subsystem: lua_runner
status: complete
tags: [lua, sol2, benchmark, release, msvc, linux, verification]
requires:
  - "08-01: compile-time sol2 base traits, typed operands, file expression methods"
provides:
  - "Release proof: Debug counts, golden, eq, warnings (C4702 guard shown load-bearing), tidy"
  - "LUA-06 decision with data: the compile-time traits stay (runtime tag cost +2.3% on f:read)"
  - "Linux GCC 13 / Clang 18 and six-suite evidence, final counts in STATE.md"
affects:
  - "Phase 9 docs (root AGENTS.md AbstractExpression decision, cross-layer rows)"
tech-stack:
  added: []
  patterns:
    - "Interleaved three-binary benchmark in one session, judged only against the same session's before medians"
key-files:
  created: []
  modified:
    - src/AGENTS.md
    - .planning/STATE.md
decisions:
  - "The compile-time traits stay: the runtime base-classes tag cost +2.3% on f:read (at least +2.0% on one workload); traits landed at +1.4% / -9.3%, within +5.0%"
  - "No swap commit, so FileAndExpressionKeepTableIndex stays and N remains 13"
metrics:
  duration: 75min
  completed: 2026-10-04
actuals:
  tokens: 3500
  tasks: 3
  commits: 2
---

# Phase 8 Plan 2: Release, Benchmark and Cross-Compiler Proof Summary

The benchmark rule kept the compile-time sol2 traits: the runtime tag cost +2.3% on `f:read`. Release now matches Debug test for test with zero `src/lua_runner` warnings, and the `disable : 4702` guard is what keeps that count at zero. Linux GCC 13 and Clang 18 pass with 488 Lua* tests (487 passed, 1 skipped), and all six binding suites are green.

## Task 1: Release phase gate

- `PHASE GATE PASS lua=490 luaexpr=41 tests=1476 capi=543 warnings=0 c4702=0 tidy_new=0` (`build/typed-check/phase-gate-out.txt`). It covers the Debug gate (`GATE PASS lua=490 luaexpr=41 tests=1476 capi=543`, GOLDEN/SURFACE DELTA/EQ debug OK, SYNC OK), the Release listings (LuaExpressionTest 41, Lua* 490 in 12 suites, quiver_tests 1476, ExpressionFixture 125, SandboxedPathTest 11, quiver_c_tests 543, ExpressionCApiFixture 73, LuaRunnerCApiTest 27), both full Release suites passing, `GOLDEN release OK`, `EQ release OK`, the FORM `traits` greps, the out-of-scope diff and assert_version.
- Release warnings with the guard: `RELEASE WARNINGS tus=17 total=0 c4702=0 msvc=14.51.36231`. The list equals `build/abstract-check/release-warnings.txt` (empty).
- Mutation, with the `pragma warning(disable : 4702)` line deleted: `RELEASE WARNINGS tus=17 total=2 c4702=2 msvc=14.51.36231`. The two unique entries are `function_types_overloaded.hpp` and `stack.hpp`, from 42 raw C4702 lines in the build log. Evidence is in `build/typed-check/release-warnings-no-pragma.txt`. After the change was reverted, `git diff --quiet -- src` exited 0.
- Tidy: 17 files, no new (check, line) pair against Phase 7's final `tidy-pairs.txt` (13 pairs both sides), no new header pair, no `clang-diagnostic-error`.

## Task 2: Benchmark and LUA-06 decision

The tag variant was a working-tree copy that was measured and then reverted. It was never staged. The edit is in `build/typed-check/tag-variant.diff`: internal.h back to BASE, plus `sol::base_classes, sol::bases<AbstractExpression>()` in the `BinaryFile` and `Expression` usertypes. clang-format splits that into two lines.
- Release `LuaExpressionTest.*`: 40 passed, 1 failed. The failure is `FileAndExpressionKeepTableIndex` (`file __index is a table` assertion), which is the expected bite.
- `EQ release OK`.
- Golden diff (`build/typed-check/tag-golden-diff.txt`): only surface.txt changed, and only `__index:table` -> `__index:function` in BinaryFile and Expression.
- Restored tree: `git diff --quiet HEAD -- src` OK, no `sol::bases<` in src, Release LuaExpressionTest 41/41 pass.

Benchmark: one session, one untimed warm-up, then five timed rounds of 1M calls. Each round ran before, tag, traits for file_write and then file_read. Raw runs are in `build/typed-check/perf-runs.txt` (30 lines, 10 per label). Phase 7 runs are preserved in `build/abstract-check/perf/runs-phase7.txt`.

| Workload | before (phase7) | tag | tag % | traits | traits % |
|---|---|---|---|---|---|
| `f:write` 1M | 2490 ms | 2538 ms | +1.9 | 2525 ms | +1.4 |
| `f:read` 1M | 2390 ms | 2446 ms | +2.3 | 2168 ms | -9.3 |

Phase 7 baseline (a different session, not compared): 2174 / 2082 ms.

**Decision (LUA-06 as written, FA-7 branch "traits stay"):** tag% >= 2.0 on `f:read` (+2.3), so the tag costs measurably and the traits stay. The landed traits are +1.4% and -9.3%, both within +5.0%, so no rerun was needed. `build/typed-check/FORM` = `traits`. No refactor commit was made, and `FileAndExpressionKeepTableIndex` is kept.

**Caveat:** single runs spread from 1838 to 3497 ms, far wider than the 2% margin. The medians decide only because the plan fixed that rule beforehand (FA-6). The +2.3% tag cost is inside the noise, and it is recorded that way in src/AGENTS.md and STATE.md.

Binaries: `build/perf-phase8-traits/` (COMMIT b784b93) and `build/perf-phase8-tag/` (`tag variant of b784b93`), each with `SHA256SUMS`. quiver_cli.exe is byte-identical across both; only libquiver.dll differs.

## Task 3: Linux and six suites

Docker was available. Both runs built `6446659`, whose code paths equal HEAD. P7 = 475, F7 = 1459, N = 13. The new tests contain no `_WIN32`.

| Toolchain | Lua* LISTED | passed + skipped | FULL_LISTED | passed + skipped | LuaRunnerCApiTest | ExpressionFixture | quiver_c_tests | warnings |
|---|---|---|---|---|---|---|---|---|
| GCC 13.3.0 | 488 | 487 + 1 | 1472 | 1469 + 3 | 27 | 125 | 543 | 2, both WARN_OK |
| Clang 18.1.3 / libc++ | 488 | 487 + 1 | 1472 | 1469 + 3 | 27 | 125 | 543 | 1, WARN_OK |

There are no `[  FAILED` lines and both logs end `exit=0`. OperandErrorsNameTheOperation and OperandErrorsReportTheLeftmostBadOperand pass on both toolchains, and the Release golden `binary.txt` is byte-identical to the Debug baseline.

Six suites (`build/typed-check/test-all.txt`, run after clearing the Dart hook cache): C++, C API, Julia, Dart, JavaScript and Python PASS, `All tests PASSED`. `bun test test/lua-api-sync.test.ts`: 6 pass, 0 fail.

## Final counts

Windows Debug and Release: quiver_tests 1476, Lua* 490 in 12 suites, LuaExpressionTest 41, SandboxedPathTest 11, ExpressionFixture 125, quiver_c_tests 543, LuaRunnerCApiTest 27, ExpressionCApiFixture 73. Linux: see the table above.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 | (none) | harness and evidence only, gitignored |
| 2 | 6446659 | docs(08-02): record the Lua base-registration benchmark |
| 3 | d5ba8f8 | docs(08-02): record the final Lua test counts |

## Deviations from Plan

- **Unique vs raw C4702 count:** the no-guard script reports c4702=2 because the list is deduplicated per header. The raw build log has 42 C4702 lines. The plan expected "at least 1" (the research said 34 raw), so this passes; both numbers are recorded.
- The tag edit's one plan line became two lines after clang-format 22.1.8. The plan required formatting, and both lines are in the measured diff.

Otherwise the plan was executed as written. No `fix(08-02)` was needed.

## Flagged assumptions (restated)

- **FA-5:** the C4702 guard covers every template instantiated from expression.cpp's includes. The other 16 TUs still report C4702.
- **FA-6:** the +2% / +5% thresholds are plan decisions. This session's run-to-run spread (about +-30%) dwarfs them.
- **FA-7:** the "traits stay" branch applied. The table `__index` and compile-time registration are kept.

## Known Stubs

None.

## Threat Flags

None. T-08-09 is mitigated: the tag was reverted before the benchmark, and a tag build fails `FileAndExpressionKeepTableIndex`. T-08-10 is mitigated: the guard is one warning number in one TU, and total=0 across 17 TUs. T-08-12 is mitigated: binaries, SHA256SUMS, raw runs and the rerun command are kept.

## Self-Check: PASSED

- build/typed-check/{phase_gate.sh, release_warnings.sh, FORM, tag-variant.diff, tag-golden-diff.txt, bench.txt, perf-runs.txt, linux_gcc.txt, linux_clang.txt, test-all.txt}, build/perf-phase8-{tag,traits}/SHA256SUMS present.
- Commits 6446659 and d5ba8f8 present in `git log`.
