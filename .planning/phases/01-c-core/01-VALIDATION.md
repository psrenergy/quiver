---
phase: 1
slug: c-core
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-06
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
| **Quick run command** | `./build/bin/quiver_tests.exe --gtest_filter=Expression.*` |
| **Full suite command** | `./build/bin/quiver_tests.exe` (or `ctest -C Debug --output-on-failure` from `build/`) |
| **Estimated runtime** | ~5 seconds (Expression.* filter); ~30 seconds (full suite) |

---

## Sampling Rate

- **After every task commit:** Run `./build/bin/quiver_tests.exe --gtest_filter=Expression.*`
- **After every plan wave:** Run `./build/bin/quiver_tests.exe`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

> Task IDs are placeholders — populated by gsd-planner. The mapping below is from requirement → test case → command. After plans are written, each plan's tasks reference these test cases via `<automated>` blocks.

| Requirement | Test Case(s) | Wave | Test Type | Automated Command |
|-------------|--------------|------|-----------|-------------------|
| CORE-01 | `Expression.IdentityFile` | 0 (test scaffold) → covered by post-impl wave | unit | `quiver_tests --gtest_filter=Expression.IdentityFile` |
| CORE-02 | `Expression.AddTwoFiles*`, `SubtractTwoFiles`, `MultiplyTwoFiles`, `DivideTwoFiles` | post-impl | unit | `quiver_tests --gtest_filter='Expression.AddTwoFiles*:Expression.SubtractTwoFiles:Expression.MultiplyTwoFiles:Expression.DivideTwoFiles'` |
| CORE-03 | `Expression.ScalarBroadcast{Add,Subtract,Multiply,Divide}{Left,Right}` (8 cases) | post-impl | unit | `quiver_tests --gtest_filter='Expression.ScalarBroadcast*'` |
| CORE-04 | `Expression.Chained`, `Expression.SamePathTwice` | post-impl | unit | `quiver_tests --gtest_filter='Expression.Chained:Expression.SamePathTwice'` |
| CORE-05 | `Expression.SaveProducesReadableFile`, `Expression.SelfSaveCollisionThrows`, `Expression.SaveOpenedTwiceProducesSameOutput` | post-impl | unit | `quiver_tests --gtest_filter='Expression.SaveProducesReadableFile:Expression.SelfSaveCollisionThrows:Expression.SaveOpenedTwiceProducesSameOutput'` |
| CORE-06 | `Expression.LargeGridCompletes` + code-review of `save()` body for single-buffer reuse | post-impl | unit + review | `quiver_tests --gtest_filter=Expression.LargeGridCompletes` |
| TEST-01 | All Expression.* cases incl. shape/unit/time/datetime/label rejections | post-impl | unit (full suite) | `quiver_tests --gtest_filter=Expression.*` |

**Error-path test cases (D-01..D-11 enforcement):**

| Decision | Test Case | What it verifies |
|----------|-----------|------------------|
| D-01, D-06 | `Expression.MismatchedShapesThrows` | Same-name dim with `n×m` (n≠m, both >1) → throws with D-06 format |
| D-01 | `Expression.BroadcastSizeOneDim` | `1×n` and `n×1` succeed |
| D-02 | `Expression.UnionDimsAcrossOperands` | `[scenario,time]` + `[time,stage]` → output `[scenario,time,stage]` |
| D-04 | `Expression.TimePropertiesMismatchThrows` | Same-name time dim with different `frequency` → throws |
| D-07 | `Expression.UnitMismatchThrows` | `lhs.unit != rhs.unit` → throws |
| D-08 | `Expression.BroadcastLabelsAxis` | 1 label vs n labels → broadcasts |
| D-08 | `Expression.LabelMismatchThrows` | Same non-singleton size, different content → throws |
| D-09 | `Expression.InitialDatetimeMismatchThrows` | Both have time dims but `initial_datetime` differs → throws |
| D-11 | `Expression.SelfSaveCollisionThrows` | `e.save(a's path)` throws **and** `a` is unchanged after |

---

## Wave 0 Requirements

- [ ] `tests/test_expression.cpp` — new file, GoogleTest fixture mirroring `tests/test_binary_file.cpp` programmatic-`.qvr` setup; covers CORE-01..06 + TEST-01 (~28 cases enumerated in 01-RESEARCH.md § Validation Architecture)
- [ ] `tests/CMakeLists.txt` — register `test_expression.cpp` in `quiver_tests` target
- [ ] (Discretion) Extend `tests/test_binary_file.cpp` with `D-13/D-14 Fast Overloads` section, OR add `tests/test_binary_file_fast.cpp`. Default: extend in-place.
- [ ] (Discretion) Tests for `quiver::binary::first_dimensions` / `next_dimensions` free functions (D-12). Either a new `tests/test_iteration.cpp` or fold into `tests/test_binary_metadata.cpp`. Default: fold in.
- [ ] No new framework install — GoogleTest already wired.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Single-`vector<double>` row buffer reused across `save()` iterations (no per-row allocations in steady-state inner loop) | CORE-06 | Allocation count is not directly measurable in GoogleTest without an allocation profiler. Code review confirms no `std::vector<double>` instantiation, push_back, or resize() inside the per-row loop body. | Reviewer reads `src/expr/expression.cpp` `Expression::save()` and `BinaryOpNode::compute_row` — confirms `mutable std::vector<double> row_buf_, lhs_buf_, rhs_buf_` are members declared once; the inner loop body uses `read_into(...)` (skips allocation) and reuses these members. `Expression.LargeGridCompletes` provides a smoke-test backstop (100×100×5 grid completes within budget). |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies (planner enforced)
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references (`tests/test_expression.cpp` is the single Wave 0 file)
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s (filter run is ~5s)
- [ ] `nyquist_compliant: true` set in frontmatter (after planner wires test cases to tasks)

**Approval:** pending
