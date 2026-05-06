---
phase: 2
slug: tech-debt
status: validated
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-06
audited: 2026-05-06
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (FetchContent in `cmake/Dependencies.cmake`) |
| **Config file** | `tests/CMakeLists.txt` — already wires `test_expression.cpp` (line 9) and `test_binary_file.cpp` (line 4) |
| **Quick run command** | `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.*:BinaryTempFileFixture.*:IterationTest.*:CSVConverter*"` |
| **Full suite command** | `./build/bin/quiver_tests.exe` |
| **Estimated runtime** | Quick: ~3 sec · Full: ~10 sec (787 tests target post-Phase-2; 781 baseline) |

---

## Sampling Rate

- **After every task commit:** Run quick command (~120 tests, ~3 sec)
- **After every plan wave:** Run full suite (~787 tests post-Phase-2, ~10 sec)
- **Before `/gsd-verify-work`:** Full quiver_tests + quiver_c_tests must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Audit ID | Wave | Behavior to verify | Test Type | Automated Command | File Exists | Status |
|---------|----------|------|---------------------|-----------|-------------------|-------------|--------|
| 02-01-01 | CR-01 (mirror A) | 1 | `lhs` non-time + `rhs` time on same dim name → `BinaryOpNode` ctor throws | unit (D-23 #1) | `quiver_tests.exe --gtest_filter="ExpressionFixture.MirrorTimeNonTimeMismatchAThrows"` | ✅ test_expression.cpp:448 | ✅ green |
| 02-01-02 | CR-01 (mirror B) | 1 | `lhs` time + `rhs` non-time on same dim name → throws (regression backstop) | unit (D-23 #2) | `quiver_tests.exe --gtest_filter="ExpressionFixture.MirrorTimeNonTimeMismatchBThrows"` | ✅ test_expression.cpp:473 | ✅ green |
| 02-01-03 | WR-01 (positive) | 1 | Both sides time, same dim name, parent-name matches at different operand indices → accepts | unit (D-23 #3) | `quiver_tests.exe --gtest_filter="ExpressionFixture.ParentDimMatchByNameAcceptsCrossPosition"` | ✅ test_expression.cpp:498 | ✅ green |
| 02-01-04 | WR-01 (negative) | 1 | Both sides time, same dim name, parent-name differs → throws | unit (D-23 #4) | `quiver_tests.exe --gtest_filter="ExpressionFixture.ParentDimNameMismatchThrows"` | ✅ test_expression.cpp:530 | ✅ green |
| 02-01-05 | CR-02 | 1 | `open_file('w', bad_md)` throws then `open_file('w', good_md)` on same path succeeds | regression (D-24) | `quiver_tests.exe --gtest_filter="BinaryTempFileFixture.OpenFileWriteFailureDoesNotLeakRegistry"` | ✅ test_binary_file.cpp:473 | ✅ green |
| 02-01-06 | WR-08 | 1 | `Expression e = a + b;` (implicit conversion in operator argument) compiles and evaluates | unit (D-25) | `quiver_tests.exe --gtest_filter="ExpressionFixture.ImplicitConversionInOperatorArgument"` | ✅ test_expression.cpp:231 | ✅ green |
| 02-01-07 | CR-03 | 1 | All `BinaryOpNode` ctor throws use `"Cannot apply: ..."` verb | grep + commit review | `grep -rn "Cannot apply binary operation" src/ include/ tests/` → 0 matches post-fix | EXISTING (no new test) | ✅ green (manual) |
| 02-01-08 | WR-02 | 1 | `validate_file_is_open()` declaration + definition removed | build + smoke (existing 781 tests) | `cmake --build build && quiver_tests.exe` | EXISTING | ✅ green |
| 02-01-09 | WR-03 | 1 | Duplicate `dimension_sizes_at_values` removed from `binary_file.cpp` | build + CSVConverter regression | `quiver_tests.exe --gtest_filter="CSVConverter*"` (37 tests) | EXISTING | ✅ green |
| 02-01-10 | WR-04 | 1 | Protected `next_dimensions` and `dimension_sizes_at_values` removed; csv_converter re-pointed | build + regression | `quiver_tests.exe --gtest_filter="CSVConverter*:IterationTest.*"` (42 tests) | EXISTING | ✅ green |
| 02-01-11 | WR-05 | 1 | `validate_dimension_values` (map) delegates to `_indexed` | regression | `quiver_tests.exe --gtest_filter="BinaryTempFileFixture.*ValidationCoordinates*:BinaryTempFileFixture.SingleTime*"` | EXISTING | ✅ green |
| 02-01-12 | WR-06 | 1 | `compute_row` uses pre-computed reverse table; result correctness preserved | regression | `quiver_tests.exe --gtest_filter="ExpressionFixture.*"` (34 tests including LargeGridCompletes runtime backstop) | EXISTING | ✅ green |
| 02-01-13 | WR-07 | 1 | `apply_op` post-switch throw added (canary for future variants) | code review + build | Verified at `src/expr/nodes.cpp:97`: `throw std::runtime_error("Cannot apply: unhandled BinaryOp variant")` | EXISTING (no new test) | ✅ green (manual) |
| 02-01-14 | WR-09 | 1 | `next_dimensions` no longer rebuilds `first_dimensions` per call | regression | `quiver_tests.exe --gtest_filter="IterationTest.*:ExpressionFixture.*"` (34 tests) | EXISTING | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

*Note: Task IDs are illustrative — final task numbering is the planner's call. Per D-26, ~11 atomic commits in audit order; some audit items may share a commit (e.g., CR-01 + WR-01 bundle per D-15/D-16).*

---

## Wave 0 Requirements

- [x] `tests/test_expression.cpp` — added 4 new `TEST_F(ExpressionFixture, ...)` cases for D-23 (CR-01 + WR-01 coverage) — lines 448, 473, 498, 530
- [x] `tests/test_expression.cpp` — added 1 new `TEST_F(ExpressionFixture, ImplicitConversionInOperatorArgument)` for D-25 (WR-08 coverage) — line 231
- [x] `tests/test_binary_file.cpp` — added 1 new `TEST_F(BinaryTempFileFixture, OpenFileWriteFailureDoesNotLeakRegistry)` for D-24 (CR-02 coverage) — line 473
- [x] No framework install required — GoogleTest already wired
- [x] No CMake updates required — both test files are already in `tests/CMakeLists.txt` (lines 4, 9)
- [x] No new fixtures required — existing `ExpressionFixture` and `BinaryTempFileFixture` cover all 6 new tests

**Test count after Phase 2:** 781 (baseline) + 4 (D-23) + 1 (D-24) + 1 (D-25) = **787 tests** (achieved, verified per `02-VERIFICATION.md` Truth #14).

---

## Manual-Only Verifications

| Behavior | Audit ID | Why Manual | Test Instructions |
|----------|----------|------------|-------------------|
| All `BinaryOpNode` throws use `"Cannot apply: ..."` verb | CR-03 | Cosmetic message-format conformance; no behavior change observable through tests | `grep -rn "Cannot apply binary operation" src/expr` must return 0 matches; spot-check that all ctor throws read `"Cannot apply: <reason>"` |
| `apply_op` post-switch throw triggers if a new `BinaryOp` variant is added | WR-07 | No current variant exercises the new throw; testing it requires UB (`static_cast<BinaryOp>(99)`) which is not recommended | Code review of `nodes.cpp:82-94` confirms `return 0.0;` is replaced with a `throw std::runtime_error("Cannot apply: unhandled BinaryOp variant")`. Future variant additions will surface compiler warnings if the switch lacks coverage. |

---

## Validation Sign-Off

- [x] All audit items have automated regression coverage OR documented manual-verification path
- [x] Sampling continuity: every commit runs the quick filter; every wave runs full suite
- [x] Wave 0 covers all 6 NEW tests (D-23 ×4, D-24 ×1, D-25 ×1)
- [x] No watch-mode flags
- [x] Feedback latency < 10 sec (quick) / < 15 sec (full) — measured 6.3 sec for 126-test quick filter
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved (2026-05-06)

---

## Validation Audit 2026-05-06

| Metric | Count |
|--------|-------|
| Audit items reviewed | 14 |
| COVERED (automated) | 12 |
| COVERED (manual-only, pre-documented) | 2 (CR-03, WR-07) |
| PARTIAL | 0 |
| MISSING | 0 |
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |

**Evidence:**
- Quick filter (`ExpressionFixture.*:BinaryTempFileFixture.*:IterationTest.*:CSVConverter*`): 126/126 PASSED in 6.3 sec
- Six NEW tests filter: 6/6 PASSED in 0.4 sec
- Manual CR-03: `grep -rn "Cannot apply binary operation" src/ include/ tests/` → 0 matches
- Manual WR-07: `src/expr/nodes.cpp:97` contains `throw std::runtime_error("Cannot apply: unhandled BinaryOp variant")`
- Full suite per `02-VERIFICATION.md`: 787/787 quiver_tests + 403/403 quiver_c_tests

**Outcome:** Phase 2 is Nyquist-compliant. No gap-filling tests required; no auditor agent dispatched.
