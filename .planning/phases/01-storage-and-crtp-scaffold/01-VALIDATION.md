---
phase: 1
slug: storage-and-crtp-scaffold
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-05
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (already declared in `cmake/Dependencies.cmake`) |
| **Config file** | `tests/CMakeLists.txt` (existing) |
| **Quick run command** | `ctest --test-dir build -R tensor --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~30 seconds (incremental Debug build); ~60 seconds (clean build) |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build -R tensor --output-on-failure`
- **After every plan wave:** Run `ctest --test-dir build --output-on-failure` (full suite — confirms cross-phase discipline holds)
- **Before `/gsd-verify-work`:** Full suite must be green AND `git diff --stat include/quiver/c/ include/quiver/c/binary/ bindings/ src/CMakeLists.txt` must show zero rows
- **Max feedback latency:** 60 seconds

---

## Per-Task Verification Map

> Concrete task IDs are filled in by gsd-planner; this map enumerates the verification surface.
> Each row maps a phase requirement to its automated check.

| Requirement | Wave | Plan (anticipated) | Test Type | Automated Command | File Exists | Status |
|-------------|------|--------------------|-----------|-------------------|-------------|--------|
| STG-01 (Tensor<T> for {float,double,int32_t,int64_t}) | 1 | 01 | unit (templated test) | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| STG-02 (shape/size/rank, data() const T*) | 1 | 01 | unit | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| STG-03 (operator()(i,j,...) and at()) | 1 | 01 | unit (death-test for at() bounds) | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| STG-04 (begin/end iterators, range-for) | 1 | 01 | unit | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| STG-05 (.item() with size==1 invariant) | 1 | 01 | unit (death-test for size!=1) | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| EXP-01 (Expression<Derived> CRTP base) | 1 | 01 | compile-time (static_assert) + runtime (eval) | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| EXP-02 (IsTensorExpr concept) | 1 | 01 | compile-time (static_assert positive + negative) | `ctest -R tensor_storage` | ❌ W0 | ⬜ pending |
| BLD-01 (headers under include/quiver/tensor/) | 1 | 02 | filesystem grep | `find include/quiver/tensor -name '*.h' \| wc -l` ≥ 3 | ✅ | ⬜ pending |
| BLD-02 (tests under tests/test_tensor_*.cpp) | 1 | 02 | filesystem grep | `ls tests/test_tensor_*.cpp` | ✅ | ⬜ pending |
| BLD-03 (no edits to src/CMakeLists.txt) | 1 | 02 | git diff gate | `git diff --stat src/CMakeLists.txt` returns zero rows | ✅ | ⬜ pending |
| BLD-04 (include containment via CTest) | 2 | 02 | CTest custom target | `ctest -R tensor_no_leakage` | ❌ W0 | ⬜ pending |

**Cross-phase discipline gate (final task in last wave):**
| Discipline | Wave | Check | Command |
|------------|------|-------|---------|
| Pitfall #13 — zero diff to C API and bindings | last | git diff stat | `git diff --stat include/quiver/c/ include/quiver/c/binary/ bindings/ src/CMakeLists.txt` ⇒ all zero rows |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_tensor_storage.cpp` — new test file; stubs for STG-01..05, EXP-01, EXP-02
- [ ] `cmake/CheckTensorIncludeContainment.cmake` — new CMake script; gates BLD-04
- [ ] `tests/CMakeLists.txt` — append `test_tensor_storage.cpp` to `quiver_tests` source list and register `add_test(NAME tensor_no_leakage COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CheckTensorIncludeContainment.cmake)`
- [ ] No new framework install needed — GoogleTest 1.17.0 already wired (`cmake/Dependencies.cmake:56-64`)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| (none) | — | All Phase 1 behaviors are automated via GoogleTest unit tests + CTest CMake-script gates | — |

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references (3 items above)
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter (after planner finalizes task IDs)

**Approval:** pending
