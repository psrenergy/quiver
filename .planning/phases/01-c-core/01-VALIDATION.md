---
phase: 1
slug: c-core
status: complete
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-06
audited: 2026-05-06
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Sourced from `01-RESEARCH.md` § Validation Architecture.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (already wired via FetchContent in `cmake/Dependencies.cmake`) |
| **Config file** | `tests/CMakeLists.txt` (no extra install needed) |
| **Quick run command** | `./build/bin/quiver_tests.exe --gtest_filter=ExpressionFixture.*` |
| **Full suite command** | `./build/bin/quiver_tests.exe` (or `ctest -C Debug --output-on-failure` from `build/`) |
| **Estimated runtime** | ~1.3 seconds (ExpressionFixture.* filter); ~10 seconds (full suite, 781 tests) |

---

## Sampling Rate

- **After every task commit:** Run `./build/bin/quiver_tests.exe --gtest_filter=ExpressionFixture.*`
- **After every plan wave:** Run `./build/bin/quiver_tests.exe`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

> Each requirement is mapped to its passing GoogleTest case(s). All ExpressionFixture cases ship in `tests/test_expression.cpp` (29 cases, all passing).

| Requirement | Test Case(s) | Wave | Test Type | Automated Command | Status |
|-------------|--------------|------|-----------|-------------------|--------|
| CORE-01 | `ExpressionFixture.IdentityFile` | post-impl | unit | `quiver_tests --gtest_filter=ExpressionFixture.IdentityFile` | COVERED |
| CORE-02 | `ExpressionFixture.AddTwoFiles`, `SubtractTwoFiles`, `MultiplyTwoFiles`, `DivideTwoFiles` | post-impl | unit | `quiver_tests --gtest_filter='ExpressionFixture.AddTwoFiles:ExpressionFixture.SubtractTwoFiles:ExpressionFixture.MultiplyTwoFiles:ExpressionFixture.DivideTwoFiles'` | COVERED |
| CORE-03 | `ExpressionFixture.ScalarBroadcast{Add,Subtract,Multiply,Divide}{Left,Right}` (8 cases) | post-impl | unit | `quiver_tests --gtest_filter='ExpressionFixture.ScalarBroadcast*'` | COVERED |
| CORE-04 | `ExpressionFixture.Chained`, `ExpressionFixture.SamePathTwice` | post-impl | unit | `quiver_tests --gtest_filter='ExpressionFixture.Chained:ExpressionFixture.SamePathTwice'` | COVERED |
| CORE-05 | `ExpressionFixture.SaveProducesReadableFile`, `SelfSaveCollisionThrows`, `SelfSaveCollisionThrowsWithCanonicalizedPath`, `SaveOpenedTwiceProducesSameOutput` | post-impl | unit | `quiver_tests --gtest_filter='ExpressionFixture.Save*:ExpressionFixture.SelfSave*'` | COVERED |
| CORE-06 | `ExpressionFixture.LargeGridCompletes` (smoke) + manual code review of `save()` body for single-buffer reuse | post-impl | unit + review | `quiver_tests --gtest_filter=ExpressionFixture.LargeGridCompletes` | COVERED (smoke automated; load-bearing code review in Manual-Only) |
| TEST-01 | All 29 `ExpressionFixture.*` cases incl. shape/unit/time/datetime/label rejections | post-impl | unit (full suite) | `quiver_tests --gtest_filter=ExpressionFixture.*` | COVERED |

**Error-path test cases (D-01..D-11 enforcement):**

| Decision | Test Case | What it verifies | Status |
|----------|-----------|------------------|--------|
| D-01, D-06 | `ExpressionFixture.MismatchedShapesThrows` | Same-name dim with `n×m` (n≠m, both >1) → throws with D-06 format | COVERED |
| D-01 | `ExpressionFixture.BroadcastSizeOneDim` | `1×n` and `n×1` succeed | COVERED |
| D-02 | `ExpressionFixture.UnionDimsAcrossOperands` | `[scenario,time]` + `[time,stage]` → output `[scenario,time,stage]` | COVERED |
| D-04 | `ExpressionFixture.TimePropertiesMismatchThrows` | Same-name dim with inconsistent time treatment across operands → throws | COVERED |
| D-05 | `ExpressionFixture.OperandDimsInDifferentOrder` | Same dim set, swapped operand order → translation table maps correctly | COVERED |
| D-07 | `ExpressionFixture.UnitMismatchThrows` | `lhs.unit != rhs.unit` → throws | COVERED |
| D-08 | `ExpressionFixture.BroadcastLabelsAxis` | 1 label vs n labels → broadcasts | COVERED |
| D-08 | `ExpressionFixture.LabelMismatchThrows` | Same non-singleton size, different content → throws | COVERED |
| D-09 | `ExpressionFixture.InitialDatetimeMismatchThrows` | Both have time dims but `initial_datetime` differs → throws | COVERED |
| D-10 | `ExpressionFixture.SamePathTwice` | `a + a` opens FileNode twice (no caching) | COVERED |
| D-11 | `ExpressionFixture.SelfSaveCollisionThrows`, `SelfSaveCollisionThrowsWithCanonicalizedPath` | `e.save(a's path)` throws **and** `a` is unchanged after | COVERED |

**Plan 01 binary-subsystem support tests (5 IterationTest + 9 BinaryTempFileFixture fast-overload cases):**

| Decision | Test Case(s) | Status |
|----------|--------------|--------|
| D-12 | `IterationTest.{FirstDimensionsOnNonTimeMetadata, FirstDimensionsOnTimeMetadata, NextDimensionsTraversesNonTimeMetadata, NextDimensionsReturnsNullOptOnEnd, NextDimensionsTraversesTimeMetadata}` | COVERED |
| D-13 | `BinaryTempFileFixture.{FastReadOverloadRoundTrip, FastReadOverloadMatchesUnorderedMap, FastReadOverloadValidatesBounds, FastReadOverloadValidatesCount, ReadIntoSkipsValidation, ReadIntoReusesBuffer}` | COVERED |
| D-14 | `BinaryTempFileFixture.{FastWriteOverloadValidatesDataLength, FastWriteOverloadValidatesBounds, FastWriteOverloadAcceptsTimeMetadata}` | COVERED |

---

## Wave 0 Requirements

- [x] `tests/test_expression.cpp` — new file, GoogleTest fixture mirroring `tests/test_binary_file.cpp` programmatic-`.qvr` setup; covers CORE-01..06 + TEST-01 (29 cases — 14 in Plan 02 + 15 in Plan 03).
- [x] `tests/CMakeLists.txt` — registers `test_expression.cpp` in `quiver_tests` target.
- [x] (Discretion) `tests/test_binary_file.cpp` extended with `Fast Overloads (D-13/D-14)` section (9 cases). Default chosen: extend in-place.
- [x] (Discretion) `tests/test_iteration.cpp` — new file with 5 cases for `quiver::binary::first_dimensions` / `next_dimensions` (D-12). Chosen over folding into `test_binary_metadata.cpp`: dedicated file keeps the iteration-specific calendar-aware traversal logic isolated.
- [x] No new framework install — GoogleTest already wired.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Single-`vector<double>` row buffer reused across `save()` iterations (no per-row allocations in steady-state inner loop) | CORE-06 | Allocation count is not directly measurable in GoogleTest without an allocation profiler. Code review confirms no `std::vector<double>` instantiation, push_back, or resize() inside the per-row loop body. | Reviewer reads `src/expr/expression.cpp` `Expression::save()` and `src/expr/nodes.cpp` `BinaryOpNode::compute_row` — confirms `mutable std::vector<double> lhs_buf_, rhs_buf_` (and FileNode `own_dims_buf_`) are members declared once; the inner loop body uses `read_into(...)` (skips allocation) and reuses these members. `ExpressionFixture.LargeGridCompletes` provides a smoke-test backstop (50×20×4 grid completes within 5s budget; observed ~310ms locally on Debug). Verified by Plan 02 Task 8 grep audit (see `01-02-SUMMARY.md`). |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (planner enforced)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references (`tests/test_expression.cpp`, `tests/test_iteration.cpp`, `tests/test_binary_file.cpp` D-13/D-14 extension)
- [x] No watch-mode flags
- [x] Feedback latency < 30s (`ExpressionFixture.*` filter runs in ~1.3s)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-05-06

## Validation Audit 2026-05-06

| Metric | Count |
|--------|-------|
| Requirements audited | 7 (CORE-01..06, TEST-01) |
| Decisions audited | 11 (D-01..D-11, plus D-12/D-13/D-14 binary-subsystem) |
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
| Documentation defects fixed | 3 (filter pattern `Expression.*` → `ExpressionFixture.*`; case count `~28` → 29; sign-off + frontmatter status) |

**Coverage verification (all green):**
- `quiver_tests --gtest_filter="ExpressionFixture.*"` → 29 / 29 passed
- `quiver_tests --gtest_filter="IterationTest.*"` → 5 / 5 passed
- `quiver_tests --gtest_filter="BinaryTempFileFixture.FastReadOverload*:BinaryTempFileFixture.ReadInto*:BinaryTempFileFixture.FastWriteOverload*"` → 9 / 9 passed
- Full `quiver_tests` suite → 781 / 781 passed

**Outcome:** Phase 1 is Nyquist-compliant. Every CORE-* requirement and every locked decision (D-01..D-14) has at least one passing automated test case. CORE-06's load-bearing verification remains the manual code review (Manual-Only entry preserved); the `LargeGridCompletes` smoke test provides the runtime backstop.
