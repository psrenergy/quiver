---
phase: 07-abstractexpression-in-c
verified: 2026-10-04T21:10:00Z
status: passed
human_verification_resolved: "delegated (Claude, adversarial) 2026-10-04 — all 4 items accepted: (1) EXPR-04 path equivalence re-read: old ctor make_shared<ExpressionFile>(file.get_file_path()) vs new BinaryFile::node() make_shared<ExpressionFile>(get_file_path()); (2) src/lua_runner diff is 2 lines (accessor call + comment), no bindings/ or tests/test_lua* change; (3) one ifelse overload, no metadata() alias, conversion explicit; (4) all 26 deleted test lines are metadata()->get_metadata() renames with unchanged expected values"
score: 33/34 must-haves verified (5/5 roadmap criteria; 1 backstop truth abstained)
behavior_unverified: 0
overrides_applied: 0
human_verification:
  - test: "Accept or reject the backstop truth 'Edge (EXPR-04, concurrency): Expression(file) reads only the file's path and its .toml, exactly as the removed implicit constructor did; no thread-safety guarantee added or removed'"
    expected: "Human confirms the code-path equivalence below is enough, or asks for a held-out test"
    why_human: "reason: insufficient_spec. The truth is tagged verification: backstop and no test exercises concurrent use. Evidence for a quick decision: base c20c0d0 had Expression(const BinaryFile& f) : node_(make_shared<ExpressionFile>(f.get_file_path())). HEAD has Expression(const AbstractExpression& e) : node_(e.node()), and BinaryFile::node() const returns make_shared<ExpressionFile>(get_file_path()) (src/expression/expression_file.cpp:30-32). Same reads (path, then the .toml through ExpressionFile's own BinaryFile), plus one const virtual dispatch. No new shared mutable state."
  - test: "Resolve judgment-tier prohibition (07-01): MUST NOT change anything a Lua script can observe or land later-phase work early"
    expected: "Human accepts. Non-authoritative LLM verdict: CLEAN"
    why_human: "Judgment-tier prohibition. Evidence: git diff c20c0d0..HEAD -- src/lua_runner is 2 lines (expression.cpp accessor call inside the Lua 'metadata' lambda, Lua name unchanged; binary.cpp comment only). No bindings/ change. No tests/test_lua* change. Lua* 477 pass (re-run by verifier). EQ debug and release OK ([true,true,false])."
  - test: "Resolve judgment-tier prohibition (07-01): MUST NOT satisfy a criterion by adding unrequested API or by deviating silently"
    expected: "Human accepts. Non-authoritative LLM verdict: CLEAN"
    why_human: "Judgment-tier prohibition. Evidence: expression.h declares one ifelse (three AbstractExpression operands, no double overload). git grep 'Expression::metadata' -- include src is empty, with no alias. static_assert(!is_convertible_v<const BinaryFile&, Expression>) holds. Both deviations (non-pure virtual get_metadata, criterion-3 ifelse amendment) are stated in 07-01-PLAN truths, 07-01-SUMMARY and the ROADMAP text."
  - test: "Resolve judgment-tier prohibition (07-02): MUST NOT reach green by changing what the gates compare against"
    expected: "Human accepts. Non-authoritative LLM verdict: CLEAN"
    why_human: "Judgment-tier prohibition. Evidence: git diff c20c0d0..HEAD -- tests touches only tests/test_expression.cpp. build/layout-check/baseline last modified 12:57, before the phase began. build/perf-phase7 SHA256SUMS verify OK, and COMMIT is f2769c3, whose include/src/tests/cmake equal HEAD's."
---

# Phase 7: AbstractExpression in C++ Verification Report

**Phase Goal:** In C++ a `BinaryFile` is an expression. `AbstractExpression` is the one parameter type of every expression operator, free function and method. `Expression` and `BinaryFile` derive from it. An expression never touches the caller's open file handle.
**Verified:** 2026-10-04
**Status:** human_needed. All code checks pass. The open items are 1 abstained backstop truth and 3 judgment-tier prohibitions; the LLM judges all of them clean.
**Re-verification:** No. This is the initial verification.

## Goal Achievement

### Roadmap Success Criteria

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | `abstract_expression.h` declares `class QUIVER_API AbstractExpression` with one pure `node()` and non-virtual `save/aggregate/aggregate_agents/select_agents/rename_agents`. A shape test static-asserts the listed traits. | VERIFIED | Header read: `node()` is the only `= 0`. The five methods are non-virtual. Copy and move operations are protected, so base assignment is impossible. `AbstractExpressionShape` (tests/test_expression.cpp:2894) holds all 9 required static_asserts plus 2 for `AggregateOperation`. It compiles and passes. |
| 2 | Ownership. A file is still open and readable after `file.save` and `(file*2.0).save`. An Expression survives the close and destruction of its file. `save` holds the root in a local. | VERIFIED | `FileStaysOpenAndReadableAfterSave`, `FailedSaveLeavesFileOpen` and `ExpressionOutlivesItsFile` all pass. `save` starts with `const auto root = node();` (src/expression/expression.cpp:43). `BinaryFile::node()` returns a fresh `ExpressionFile(get_file_path())`, which holds its own `mutable BinaryFile file_`, never the caller's handle. The SUMMARY reports a mutation check: when `save` did not hold the root in a local, the tracer test crashed. |
| 3 | No `friend` in expression.h. Every operator and free function takes `const AbstractExpression&`, the double overloads stay, and each calls `node()` once per operand. A test runs `a+b`, `2.0+a` and `ifelse(a>1.0,a,b)` with no wrapper. | VERIFIED | `grep -n friend include/quiver/expression/expression.h` prints nothing. All 43 declarations take `const AbstractExpression&`. The operators route through `binary_op`/`unary_op`, one `node()` local per operand, left to right. `FileOperandsNeedNoWrapper` checks cell values. `NodeCalledOncePerOperandLeftToRight` pins counts and order, including `x==y` giving x then y and `ifelse` giving c, t, e. |
| 4 | `Expression::metadata` is gone. `get_metadata` returns the handle metadata on a file (also through the base) and the node metadata on an Expression. `AggregateOperation` is at namespace scope with the alias kept. The C API is unchanged except line 248. Two CHANGELOG BREAKING lines exist. | VERIFIED | `git grep 'Expression::metadata' -- include src` is empty. `GetMetadataReturnsHandleOrNodeMetadata` checks address identity. `expression_node.h:143` has `using Operation = AggregateOperation;`. `git diff c20c0d0..HEAD -- include/quiver/c bindings` is empty, and in `src/c` the only change is the 1/1 edit at `src/c/expression/expression.cpp:248`. CHANGELOG.md:63-71 under `[0.13.0] — unreleased` has both BREAKING entries (`Expression e(file);` and `get_metadata()`). |
| 5 | Suite counts. Windows Debug and Release pass. Linux GCC 13 and Clang 18/libc++ compile and pass. | VERIFIED | Verifier re-ran Debug: ExpressionFixture 125/125, Lua* 477/477 in 12 suites, ExpressionCApiFixture 73/73. quiver_tests lists 1463 and quiver_c_tests lists 543. Release: `PHASE GATE PASS expr=125 tests=1463 lua=477 capi=543 linux=gcc,clang` in phase-gate-out.txt. Linux logs show COMMIT=f2769c3, and `git diff --quiet f2769c3 HEAD -- include src tests cmake CMakeLists.txt` exits 0. GCC 13.3.0 and clang 18.1.3 with `-stdlib=libc++` both give Lua* 475 (474 passed, 1 skipped), FULL 1459 (1456 passed, 3 skipped), CAPI 543, exit=0, and no vtable, typeinfo or ambiguity error. |

### Plan Must-Have Truths (07-01: 20, 07-02: 9)

| Group | Status | Evidence |
|-------|--------|----------|
| 07-01 shape, no-wrapper, operator, metadata and AggregateOperation truths (1, 2, 5, 6, 7) | VERIFIED | As for criteria 1, 3 and 4 above. |
| 07-01 deviations recorded (non-pure virtual `get_metadata`, ifelse amendment) | VERIFIED | Stated in the plan, the SUMMARY and the ROADMAP. `get_metadata` is virtual with a base body and is overridden in binary_file.h:46. |
| 07-01 Lua surface unchanged | VERIFIED | 2-line `src/lua_runner` diff. Lua* 477 pass. EQ and GOLDEN OK in debug and release. |
| 07-01 Windows Debug counts (116+9, 1454+9, 543, 73, 27, 11) | VERIFIED | Re-run: 125 / 1463 / 543 / 73. The 9 new TEST_F lines are present. |
| 07-01 CHANGELOG and src/AGENTS.md | VERIFIED | CHANGELOG lines carry no planning IDs. src/AGENTS.md:31 and 1024-1033 updated. The "Caches an open" line (1038) is kept as planned. Root AGENTS.md is unchanged. |
| 07-01 edges: save twice, failed save, outlives file, interleaved reads, `a-a`/`a==a`, unopened file, ordering | VERIFIED | Each edge is pinned by a test named in the plan, and every one passes. These are behavior-dependent truths with passing behavioral tests. |
| 07-01 edge EXPR-04 concurrency (`verification: backstop`) | ABSTAINED: insufficient_spec | No test exercises concurrent use. Code-path equivalence with base is documented in human_verification. |
| 07-01 edge EXPR-06 C API unchanged | VERIFIED | `git diff --numstat` for src/c shows exactly `1 1 src/c/expression/expression.cpp`. `c_api.jl` is untouched. |
| 07-02 Linux GCC and Clang suites, `-fvisibility=hidden` link, only pre-existing warnings | VERIFIED | linux_gcc.txt warns only on time_properties.cpp `-Wreturn-type` (x2). Clang counts match. Exit is 0 on both. |
| 07-02 Windows Release counts, GOLDEN/EQ release, tidy (1 expected pair removed), test-all six PASS, versions 0.13.0 | VERIFIED | phase-gate-out.txt and test-all.txt (`All tests PASSED`). |
| 07-02 Phase 8 baselines (Release warnings 0, C4702 0, medians 2174/2082 ms, binaries plus SHA) | VERIFIED | release-warnings.txt is 0 bytes. runs.txt medians recompute to 2174 and 2082. `sha256sum -c` passes. STATE.md:166 holds the PR notes. |
| 07-02 stale-evidence guard (COMMIT line, gate rejects mismatch) | VERIFIED | Both Linux files start with `COMMIT=`. phase_gate.sh checks `linux_*`. |

**Score:** 33/34 truths verified, 1 abstained (backstop), 0 present-but-behavior-unverified.

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `include/quiver/expression/abstract_expression.h` | VERIFIED | 58 lines. Holds the base class and the enum. Included by binary_file.h and expression.h. |
| `include/quiver/expression/expression.h` | VERIFIED | `class QUIVER_API Expression final : public AbstractExpression`, explicit constructors, no friend. |
| `include/quiver/binary/binary_file.h` | VERIFIED | `: public AbstractExpression`, with `node()` and `get_metadata()` overrides. |
| `src/expression/expression.cpp` | VERIFIED | Base methods, `binary_op`/`unary_op` helpers, `const auto root = node();`. |
| `src/expression/expression_file.cpp` | VERIFIED | `BinaryFile::node()` returns `make_shared<ExpressionFile>(get_file_path())`. |
| `tests/test_expression.cpp` | VERIFIED | 9 new substantive tests, all passing. |
| `CHANGELOG.md` | VERIFIED | Contains `Expression e(file);`. |
| `build/abstract-check/{gate,phase_gate,release_warnings}.sh`, `perf/bench.sh`, `build/fixes-check/linux.sh` | VERIFIED | Present. Gitignored harness. |

### Key Link Verification

| From | To | Status |
|------|----|--------|
| binary_file.h | abstract_expression.h (inheritance) | WIRED |
| expression_file.cpp `BinaryFile::node()` | ExpressionFile built from path | WIRED |
| expression.cpp `save` | root held in local | WIRED |
| src/c/expression/expression.cpp:248 | `expression->expression.get_metadata()` | WIRED |
| src/lua_runner/expression.cpp | `self.get_metadata()` | WIRED |
| src/lua_runner/binary.cpp | `quiver/expression/expression.h` include kept | WIRED (EQ probe `[true,true,false]`) |
| phase_gate.sh | gate.sh, linux_* | WIRED |
| bench.sh | build/perf-phase7/quiver_cli.exe | WIRED |

### Behavioral Spot-Checks (run by verifier)

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Build up to date | `cmake --build build --config Debug` | `no work to do` | PASS |
| Expression suite | `quiver_tests --gtest_filter=ExpressionFixture.*` | 125 passed | PASS |
| Lua surface unchanged | `quiver_tests --gtest_filter=Lua*` | 477 listed, 477 passed, 12 suites | PASS |
| C API expressions | `quiver_c_tests --gtest_filter=ExpressionCApiFixture.*` | 73 passed | PASS |
| Totals | `--gtest_list_tests` | quiver_tests 1463, quiver_c_tests 543 | PASS |
| Perf binaries intact | `sha256sum -c SHA256SUMS` | 3x OK | PASS |

### Probe Execution

No `probe-*.sh` declared or present. The phase's gates are build/abstract-check scripts, whose recorded output was inspected. Probe execution was skipped.

### Requirements Coverage

| Req | Plan | Status | Evidence |
|-----|------|--------|----------|
| EXPR-01 | 07-01 | SATISFIED | Criterion 1. Virtual destructor, protected copy and move, one pure `node()`, non-virtual methods. |
| EXPR-02 | 07-01 | SATISFIED | Criterion 2. `Expression final` keeps `node_`, and the file's `node()` is a fresh leaf. |
| EXPR-03 | 07-01 | SATISFIED | Criterion 3. |
| EXPR-04 | 07-01 | SATISFIED | `explicit Expression(const AbstractExpression&)`, the not-convertible static_assert, the CHANGELOG entry. Direct-init call sites (C API, tests) compile. |
| EXPR-05 | 07-01 | SATISFIED | Criterion 4, rename plus metadata semantics. |
| EXPR-06 | 07-01 | SATISFIED | Namespace-scope enum and alias, C API diff limited to one line, counts recorded. |
| EXPR-07 | 07-02 | SATISFIED | Linux GCC 13 and Clang 18/libc++ evidence. |

All 7 IDs map to Phase 7 in REQUIREMENTS.md, and plans claim them all (07-01: 01-06, 07-02: 07). No requirement is orphaned.

### Anti-Patterns Found

No TBD, FIXME, XXX, TODO, HACK or PLACEHOLDER added in the phase diff (`include src tests CHANGELOG.md`). No stubs.

Advisory items from 07-REVIEW.md. None of them blocks the goal:

| File | Line | Issue | Severity |
|------|------|-------|----------|
| `abstract_expression.h` | 30-32 | WR-01: the default `get_metadata()` body would return a dangling reference for a future subclass that builds a fresh node per call without overriding it. No current subclass is affected: `BinaryFile` overrides, `Expression` retains `node_`, and the test `CountingExpression` wraps a retained Expression. | Warning |
| `src/AGENTS.md` | ~1009, ~1020 | IN-04: still names `Expression::save` and shows a `sqrt(Expression(a))` wrapper. | Info |
| `CHANGELOG.md` | 63-69 | IN-03: the BREAKING line names only `Expression e = file;` and not the other copy-init forms or `final`. | Info |
| `tests/test_expression.cpp` | 2945-2960 | IN-05: 16 file-operand forms are checked only for no-throw. | Info |

### Human Verification Required

#### 1. Backstop truth: EXPR-04 concurrency (insufficient_spec)

**Test:** Decide whether the code-path equivalence is enough evidence, or add a held-out test.
**Expected:** `Expression(file)` performs the same reads as the removed implicit constructor.
**Why human:** The truth is tagged `verification: backstop`, and no concurrent test exists. Base: `node_(make_shared<ExpressionFile>(file.get_file_path()))`. HEAD: `node_(e.node())`, which dispatches to `BinaryFile::node()`, which returns `make_shared<ExpressionFile>(get_file_path())`. These are the same.

#### 2-4. Judgment-tier prohibitions (07-01 x2, 07-02 x1)

**Test:** Accept the non-authoritative CLEAN verdicts recorded in the frontmatter.
**Expected:** No Lua-observable change, no unrequested API or silent deviation, no gate baseline edited.
**Why human:** Judgment-tier prohibitions need explicit human resolution.

### Gaps Summary

There are no gaps. Every roadmap success criterion and requirement is backed by code and by passing tests that the verifier re-ran, and the Linux and Release evidence is tied to code identical to HEAD. Status is `human_needed`, not `passed`, only because the protocol routes the backstop concurrency truth and the three judgment-tier prohibitions to a human. All four are expected to be accepted. Consider WR-01 before Phase 8 adds more subclasses or bindings of `AbstractExpression`.

---

_Verified: 2026-10-04_
_Verifier: Claude (gsd-verifier)_

## Human Verification Resolution

Delegated to Claude per the project's standing instruction (verify checkpoints adversarially rather than ask). Each item was re-checked independently of the verifier, and all four are accepted:

1. **EXPR-04 concurrency backstop.** The data path is the same. At `c20c0d0` the constructor was `Expression(const BinaryFile&) : node_(make_shared<ExpressionFile>(file.get_file_path()))`. At HEAD, `Expression(const AbstractExpression& e) : node_(e.node())` dispatches to `BinaryFile::node()`, which returns `make_shared<ExpressionFile>(get_file_path())`. No new shared mutable state, so no held-out test is needed.
2. **Lua-observable change (07-01).** `git diff c20c0d0..HEAD -- src/lua_runner` is two lines: the accessor call inside the Lua `metadata` lambda (the Lua name is unchanged) and a comment. There are no `bindings/` or `tests/test_lua*` changes. `Lua*` passes 477 of 477, and the EQ probe prints `[true,true,false]`.
3. **Unrequested API (07-01).** There is one `ifelse` overload with three `AbstractExpression` operands, no `metadata()` alias, and the conversion from a file is not implicit (static-asserted). Both deviations are documented in the plan, the summary and the roadmap.
4. **Gate baselines (07-02).** In `tests/`, only `test_expression.cpp` changed. All 26 deleted lines are `metadata()` → `get_metadata()` renames with the same expected values.

The advisory warning WR-01 in 07-REVIEW.md remains open and is not a gap: the default `get_metadata()` body is safe only for subclasses that keep their node alive. Settle it before Phase 8.
