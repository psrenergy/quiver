---
phase: 01-storage-and-crtp-scaffold
verified: 2026-05-05T22:07:39Z
status: passed
score: 28/28 must-haves verified
overrides_applied: 0
re_verification:
  previous_status: none
  previous_score: n/a
  gaps_closed: []
  gaps_remaining: []
  regressions: []
---

# Phase 1: Storage and CRTP Scaffold — Verification Report

**Phase Goal:** User can construct a `quiver::tensor::Tensor<T>`, query its shape/size/rank, index it by multi-dimensional coordinates, iterate it, and read its raw buffer — and generic code can identify any tensor expression via the `IsTensorExpr` concept.

**Verified:** 2026-05-05T22:07:39Z
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Phase Success Criteria (from ROADMAP.md)

| # | Success Criterion | Status | Evidence |
|---|-------------------|--------|----------|
| 1 | `Tensor<T>` exists for `T ∈ {float, double, int32_t, int64_t}` with shape/size/rank/data/operator()/begin-end | PASS | `include/quiver/tensor/tensor.h:17` declares `template <class T> class Tensor : public Expression<Tensor<T>>`; lines 47–56 expose `shape()`, `size()`, `rank()`, `data()`, `begin/end`. Test `ElementTypes.AllFourTypesConstruct` (line 18) constructs all four types successfully. |
| 2 | `Expression<Derived>` CRTP base with self/shape/size/eval; `Tensor<T>` derives; `IsTensorExpr` matches every `Expression<Derived>`-derived type | PASS | `expression.h:13–25` declares `Expression<Derived>` with `self()`, `shape()`, `size()`, `rank()`, `eval()`. `tensor.h:17` declares `Tensor : public Expression<Tensor<T>>`. `expression.h:47–48` declares `concept IsTensorExpr`. Test `ExpressionBase.TensorDerivesFromExpression` and `IsTensorExprConcept.{AcceptsTensor,RejectsNonExpression,StripsRefAndCV}` verify positive + negative + cv-stripping (all PASSED). |
| 3 | Headers exclusively under `include/quiver/tensor/`, tests under `tests/test_tensor_*.cpp`, registered in `tests/CMakeLists.txt`; no edits to `src/CMakeLists.txt` | PASS | `find include/quiver/tensor -name '*.h'` → 3 files. `tests/test_tensor_storage.cpp` matches naming convention. `tests/CMakeLists.txt:25` lists `test_tensor_storage.cpp` in `add_executable(quiver_tests ...)`. `git diff --stat master..HEAD -- src/CMakeLists.txt` returns empty (zero rows). |
| 4 | CI test fails if any non-tensor public header transitively pulls in `quiver/tensor/*.h` | PASS | `cmake/CheckTensorIncludeContainment.cmake` exists (57 LOC). `tests/CMakeLists.txt:51–52` registers `add_test(NAME tensor_no_leakage ...)`. `ctest -R tensor_no_leakage` PASSES on clean tree. **Independently verified negative path:** Verifier injected `#include <quiver/tensor/tensor.h>` into `include/quiver/binary/dimension.h`, ran `cmake -P cmake/CheckTensorIncludeContainment.cmake`, observed `CMake Error ... BLD-04 violation ... binary/dimension.h:1: #include <quiver/tensor/tensor.h>` and exit=1, then reverted (working tree clean post-revert). Mechanism functions both ways. |
| 5 | `git diff --stat include/quiver/c/ bindings/` shows zero rows changed (Pitfall #13) | PASS | `git diff --stat master..HEAD -- include/quiver/c/ include/quiver/c/binary/ bindings/ src/CMakeLists.txt cmake/Dependencies.cmake cmake/CompilerOptions.cmake CMakeLists.txt include/quiver/quiver.h` returns empty output. |

**Score: 5/5 success criteria PASSED**

---

### Observable Truths (from PLAN frontmatter, deduplicated against ROADMAP SCs)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | shape.h provides shape_t alias + compute_strides + total_size + ravel_index in `quiver::tensor` | PASS | `shape.h:9` `using shape_t = std::vector<std::size_t>;`; lines 11, 21, 27 declare the three free functions. `ShapeFreeFunctions.*` tests (4 cases) PASSED. |
| 2 | expression.h provides Expression<Derived> + IsTensorExpr + detail::is_base_of_template_v | PASS | `expression.h:13–25` Expression class; lines 32–43 `is_base_of_template`; line 48 IsTensorExpr concept. Test `IsTensorExprConcept.*` (3 cases) PASSED. |
| 3 | tensor.h provides Tensor<T> with 3 ctors, accessors, indexing, iterators, item, eval | PASS | `tensor.h:19–39` three constructors; 47–56 accessors; 60–77 operator()/at; 79–86 item(); 90 eval(); 53–56 begin/end. 28 GoogleTest cases all PASSED. |
| 4 | `Tensor<double>({2,3,4})` zero-initializes 24 elements per D-02 | PASS | `tensor.h:21` `data_.assign(total_size(shape_), T{})`. Test `TensorConstruction.ShapeOnlyZeroInitializes` runs `for (auto v : t) EXPECT_EQ(v, 0.0);` over 24 elements. PASSED. |
| 5 | `Tensor<int32_t>({2,3}, 42)` fills 6 elements with 42 per D-01 | PASS | `tensor.h:26` `data_.assign(total_size(shape_), fill)`. Test `TensorConstruction.ShapeWithFill` PASSED. |
| 6 | initializer_list ctor validates size==total_size(shape) per D-05 | PASS | `tensor.h:32–37` throws Pattern 1 `"Cannot construct tensor: initializer_list size ..."` on mismatch. Test `TensorConstruction.InitListMismatchThrowsPattern1` PASSED. |
| 7 | `Tensor<double>({2,0,3})` valid, size=0, rank=3 per D-04 | PASS | Multiplicative `total_size` returns 0 for any zero dim. Test `TensorConstruction.ZeroSizedDimensionValid` and `ShapeFreeFunctions.TotalSizeWithZeroDimIsZero` PASSED. |
| 8 | `Tensor<double>({}, 42.0)` valid 0-D; rank=0, size=1, t() via empty fold returns 42.0 per D-11 | PASS | `total_size(shape_t{}) == 1` (empty for-loop leaves n=1). Empty fold in `ravel_index` evaluates to 0. Test `ZeroDTensor.EmptyShapeHasOneElement` calls `t()` and `t.item()` returning 42.0 — both PASSED. |
| 9 | operator() unchecked per D-06; at() throws Pattern 1 on rank/bounds violation per D-07 | PASS | `tensor.h:60` `T& operator()(...)` is `noexcept` with no validation; `tensor.h:69` `at()` calls `check_rank_and_bounds` (lines 94–120) which throws Pattern 1. Tests `TensorIndexing.AtThrowsOnRankMismatch` and `AtThrowsOnBoundsViolation` PASSED. |
| 10 | Tensor<T> COPYABLE; movable with noexcept on both move ops per Anti-Pattern #6 | PASS | `tensor.h:41–44` defaults all four; lines 43–44 explicitly `noexcept`. Compile-time gate: 5 `static_assert(std::is_nothrow_move_constructible_v<Tensor<T>>)` calls (T = double, float, int32, int64) + `is_nothrow_move_assignable_v<Tensor<double>>` all compile green. Test `TensorMove.{MoveOpsAreNoexcept,CopyableValueType}` PASSED. |
| 11 | IsTensorExpr<Tensor<T>> true for T ∈ {float,double,int32_t,int64_t}; false for non-Expression types | PASS | `static_assert(IsTensorExpr<Tensor<float>>)` etc. (4 positive) and `static_assert(!IsTensorExpr<int>)` + `!IsTensorExpr<std::vector<double>>` etc. (4 negative) compile green. Tests PASSED. |
| 12 | Tensor<T> derives publicly from Expression<Tensor<T>> (CRTP) | PASS | `tensor.h:17` `class Tensor : public Expression<Tensor<T>>`. `static_assert(std::is_base_of_v<Expression<Tensor<double>>, Tensor<double>>)` (line 222) compiles. Test `ExpressionBase.TensorDerivesFromExpression` PASSED. |
| 13 | All thrown errors use Pattern 1 ("Cannot {op}: {reason}") | PASS | `grep -c` of each Pattern 1 wording in `tensor.h`: 4 sites total (`Cannot construct tensor`, `Cannot access tensor: rank mismatch`, `Cannot access tensor: index ... out of bounds`, `Cannot item`). All four PASS substring matches in tests. No Pattern 2 or Pattern 3 throws in Phase 1 surface (correct — none expected). |
| 14 | Cross-phase discipline (Pitfall #13): zero diff to include/quiver/c/, include/quiver/c/binary/, bindings/, src/CMakeLists.txt; tensor headers stay opt-in | PASS | `git diff --stat master..HEAD -- include/quiver/c/ include/quiver/c/binary/ bindings/ src/CMakeLists.txt cmake/Dependencies.cmake cmake/CompilerOptions.cmake CMakeLists.txt include/quiver/quiver.h` → empty. |
| 15 | Namespace hygiene (Pitfall #14): all symbols nested under quiver::tensor; helpers under quiver::tensor::detail | PASS | `grep -nP "^\s*namespace\s+quiver\s*\{"` over the three tensor headers returns no matches. Only `namespace quiver::tensor` and `namespace detail` (nested) appear. |
| 16 | tests/test_tensor_storage.cpp matches BLD-02 naming convention | PASS | File exists at `tests/test_tensor_storage.cpp`, matching `test_tensor_*.cpp` glob and the project's `test_database_*.cpp`/`test_binary_*.cpp` precedent. |
| 17 | cmake/CheckTensorIncludeContainment.cmake uses file(GLOB_RECURSE) + file(STRINGS REGEX) per D-15..D-18 | PASS | Lines 18 (GLOB_RECURSE), 32 (STRINGS REGEX), 23–25 (tensor/ exclusion via `if(HEADER MATCHES "^tensor/") continue() endif()`). CMake 3.17+ comment present at line 9. |
| 18 | tests/CMakeLists.txt has test_tensor_storage.cpp appended to quiver_tests source list | PASS | `tests/CMakeLists.txt:25` lists `test_tensor_storage.cpp` after `test_schema_validator.cpp`. |
| 19 | tests/CMakeLists.txt has add_test(NAME tensor_no_leakage ...) registered after gtest_discover_tests | PASS | `tests/CMakeLists.txt:51–52` after the `gtest_discover_tests(...)` block at lines 45–47. |
| 20 | static_assert(is_nothrow_move_constructible_v<Tensor<double>>) compiles green (Anti-Pattern #6) | PASS | Build clean. Compile-time gate live in `test_tensor_storage.cpp:199`. |
| 21 | static_assert(IsTensorExpr<Tensor<double>>) and !IsTensorExpr<int> both compile green (Pitfall #7) | PASS | `test_tensor_storage.cpp:240, 248` both compile. |
| 22 | EXPECT_THROW + Pattern 1 substring match for all four locked Phase 1 throws | PASS | NOTE-02 simplified idiom: `grep -c "EXPECT_THROW(" tests/test_tensor_storage.cpp` returns 0; `grep -c 'FAIL() << "expected std::runtime_error"' tests/test_tensor_storage.cpp` returns 4 (one per Pattern 1 throw site). All four substring-match tests PASSED. |
| 23 | Tests exercise all 4 element types per STG-01 | PASS | `ElementTypes.AllFourTypesConstruct` constructs `Tensor<float>`, `Tensor<double>`, `Tensor<int32_t>`, `Tensor<int64_t>`. Plus 4× `static_assert(is_nothrow_move_constructible_v<Tensor<T>>)` covers the same set at compile time. |
| 24 | Tests cover D-04 (0-sized dim) and D-11 (0-D tensor with empty shape) | PASS | `TensorConstruction.ZeroSizedDimensionValid` and `ZeroDTensor.{EmptyShapeHasOneElement,EmptyShapeWithInitList}` all PASSED. |
| 25 | Negative-path BLD-04 verified one-shot per NOTE-04 | PASS (independently re-verified) | Verifier independently injected `#include <quiver/tensor/tensor.h>` into `include/quiver/binary/dimension.h`, ran `cmake -P cmake/CheckTensorIncludeContainment.cmake`, observed `CMake Error ... BLD-04 violation ... binary/dimension.h:1: #include <quiver/tensor/tensor.h>` and exit=1, then restored the file (`git diff include/quiver/binary/dimension.h` empty post-revert). The mechanism's failure path is real and deterministic. |
| 26 | src/CMakeLists.txt has zero diff (BLD-03) | PASS | `git diff --stat master..HEAD -- src/CMakeLists.txt` returns empty. |
| 27 | include/quiver/c/, include/quiver/c/binary/, bindings/ have zero diff | PASS | Diff against master returns empty. |
| 28 | include/quiver/quiver.h has zero diff (umbrella stays clean; Pitfall #4) | PASS | Diff against master returns empty. |

**Score: 28/28 truths VERIFIED**

---

### Required Artifacts (Three-Level Verification)

| Artifact | Expected | L1 Exists | L2 Substantive | L3 Wired | L4 Data Flows | Status |
|----------|----------|-----------|----------------|----------|---------------|--------|
| `include/quiver/tensor/shape.h` | shape_t alias + 3 free functions in quiver::tensor | YES (37 LOC) | YES (compute_strides body uses multiplication only — Pitfall B mitigated; total_size empty-loop returns 1; ravel_index fold-expression noexcept) | YES (included by `expression.h:4`, `tensor.h:5`, and `test_tensor_storage.cpp:3`) | YES (compute_strides, total_size, ravel_index produce non-empty strides for non-empty shapes; `ShapeFreeFunctions.*` 4 tests PASSED) | VERIFIED |
| `include/quiver/tensor/expression.h` | Expression<Derived> CRTP + IsTensorExpr concept + detail::is_base_of_template_v | YES (52 LOC) | YES (Expression class with self/size/shape/rank/eval forwarding, protected default ctor; is_base_of_template detection idiom; IsTensorExpr concept) | YES (included by `tensor.h:4` and `test_tensor_storage.cpp:2`) | YES (concept correctly distinguishes Expression-derived vs non-derived types — verified by 7 positive + 4 negative + 3 cv-stripping `static_assert`s; runtime tests `IsTensorExprConcept.*` PASSED) | VERIFIED |
| `include/quiver/tensor/tensor.h` | Tensor<T> with 3 ctors, accessors, indexing, iterators, item, eval | YES (129 LOC) | YES (3 sink-param ctors, defaulted copy + noexcept move, 4 Pattern 1 throws, full member set) | YES (included by `test_tensor_storage.cpp:4`; CRTP inherits Expression; included only by tensor-internal scope) | YES (all 28 GoogleTest cases pass — runtime data flow through Tensor<T> verified end-to-end including 0-sized dims, 0-D tensors, copy/move semantics) | VERIFIED |
| `tests/test_tensor_storage.cpp` | 28 GoogleTest cases covering STG-01..05, EXP-01, EXP-02 + namespace + 0-D + 0-sized regression | YES (295 LOC) | YES (28 TEST cases, 21 static_asserts, 4 Pattern 1 substring asserts via NOTE-02 idiom, all 4 element types covered) | YES (listed in `tests/CMakeLists.txt:25`; gtest_discover_tests registers all 28 cases — verified via `quiver_tests.exe --gtest_list_tests`) | YES (28/29 tensor cases PASS; full regression 1170/1170 PASS) | VERIFIED |
| `cmake/CheckTensorIncludeContainment.cmake` | CMake script for BLD-04 enforcement runnable via `cmake -P` | YES (57 LOC) | YES (file(GLOB_RECURSE), file(STRINGS REGEX) with locked D-16 regex, FATAL_ERROR + STATUS branches, CMake 3.17+ comment per NOTE-01) | YES (registered as CTest target `tensor_no_leakage` in `tests/CMakeLists.txt:51–52`) | YES (positive case PASS on clean tree; negative case independently re-verified by verifier — injected violation triggered FATAL_ERROR with exact `binary/dimension.h:1: #include <quiver/tensor/tensor.h>` line) | VERIFIED |
| `tests/CMakeLists.txt` | Test integration with `test_tensor_storage.cpp` + `tensor_no_leakage` target | YES (modified, +6 LOC) | YES (test source appended at line 25; add_test block at lines 51–52; no other modifications) | YES (used by CMake configure → produces `quiver_tests.exe` and registers `tensor_no_leakage` CTest target) | YES (all 1170 tests run via this configuration; tensor binaries produced; test discovery works) | VERIFIED |

**All artifacts: VERIFIED at all four levels.**

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `tensor.h` | `expression.h` | `#include "expression.h"` + `class Tensor : public Expression<Tensor<T>>` | WIRED | `tensor.h:4` includes; line 17 declares CRTP inheritance. Compile-time `static_assert(std::is_base_of_v<Expression<Tensor<double>>, Tensor<double>>)` PASSES. |
| `tensor.h` | `shape.h` | `#include "shape.h"` for shape_t/compute_strides/total_size/ravel_index | WIRED | `tensor.h:5` includes; lines 19, 24, 29 use `shape_t`; lines 20, 25, 30 use `compute_strides`; lines 21, 26, 31 use `total_size`; lines 61, 65, 71, 76 use `ravel_index`. |
| `expression.h` | `shape.h` | `#include "shape.h"` for shape_t alias (single source of truth — WARNING-02 fix) | WIRED | `expression.h:4` includes; line 19 uses `shape_t`. No redeclaration of `using shape_t = ...` in expression.h (verified via grep returning 0). |
| `tests/test_tensor_storage.cpp` | tensor headers | `#include <quiver/tensor/{expression,shape,tensor}.h>` | WIRED | Lines 2–4 of test file include all three. Test compiles and runs. |
| `tests/CMakeLists.txt` | `tests/test_tensor_storage.cpp` | `add_executable(quiver_tests ... test_tensor_storage.cpp ...)` | WIRED | Line 25 lists the source. Resulting `quiver_tests.exe` discovers all 28 tensor TEST cases via gtest_discover_tests. |
| `tests/CMakeLists.txt` | `cmake/CheckTensorIncludeContainment.cmake` | `add_test(NAME tensor_no_leakage COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CheckTensorIncludeContainment.cmake)` | WIRED | Lines 51–52 register the test. `ctest -R tensor_no_leakage` runs the script. Both PASS (clean tree) and FAIL (violation injected) paths verified. |
| `CheckTensorIncludeContainment.cmake` | `include/quiver/*.h (excluding tensor/)` | `file(GLOB_RECURSE ALL_HEADERS RELATIVE include/quiver "*.h")` + `if(HEADER MATCHES "^tensor/") continue()` | WIRED | Lines 18 (glob) and 23–25 (exclusion). Verified scope by injecting violation in `include/quiver/binary/dimension.h` (which lies under `include/quiver/binary/` — covered by glob); the script detected it correctly. |

---

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|--------------|--------|--------------------|--------|
| `Tensor<T>` storage | `data_` (`std::vector<T>`) | Constructor: `data_.assign(total_size(shape_), T{})` or fill or initializer_list | YES — verified by 28 runtime tests reading written values, iterating, indexing, and copy semantics | FLOWING |
| `Tensor<T>` strides | `strides_` (`shape_t`) | Constructor calls `compute_strides(shape_)` | YES — multiplicative computation produces correct row-major strides; verified by `ShapeFreeFunctions.ComputeStridesRowMajor` ({2,3,4} → {12,4,1}) | FLOWING |
| `IsTensorExpr<T>` evaluation | concept value | `detail::is_base_of_template_v<Expression, std::remove_cvref_t<T>>` resolves at compile time via SFINAE probe | YES — `static_assert` positive cases prove acceptance; negative cases prove rejection; cv-stripping verified | FLOWING |
| `tensor_no_leakage` CTest target | exit code | CMake script greps `include/quiver/*.h` for `#include` of `quiver/tensor/` | YES — clean tree exit 0 + STATUS message; injected violation exit 1 + FATAL_ERROR message with file:line — both verified | FLOWING |

**No HOLLOW or DISCONNECTED artifacts.**

---

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| All tensor GoogleTest cases pass | `ctest --test-dir build -R "[Tt]ensor\|ElementTypes\|ZeroDTensor\|IsTensorExpr\|ExpressionBase\|ShapeFreeFunctions\|NamespaceHygiene" --output-on-failure` | 29/29 tests passed (28 GoogleTest + 1 tensor_no_leakage) | PASS |
| Full regression | `ctest --test-dir build --output-on-failure` | 1170/1170 tests passed in 130 sec | PASS |
| Build clean | `cmake --build build --config Debug --target quiver_tests` | exit 0; only relink (incremental) — no warnings reported in build log | PASS |
| BLD-04 positive (clean tree) | `cmake -P cmake/CheckTensorIncludeContainment.cmake` | exit 0, STATUS message `BLD-04: include containment OK -- no public header transitively includes quiver/tensor/.` | PASS |
| BLD-04 negative (injected violation) | inject `#include <quiver/tensor/tensor.h>` in `include/quiver/binary/dimension.h` then run script | exit 1, FATAL_ERROR with `BLD-04 violation` + offending line `binary/dimension.h:1: #include <quiver/tensor/tensor.h>` | PASS |
| Test binary discovers all 28 tensor tests | `build/bin/quiver_tests.exe --gtest_list_tests` | All 9 test suites (ElementTypes, TensorConstruction, ZeroDTensor, TensorIndexing, TensorData, TensorIteration, TensorMove, ExpressionBase, IsTensorExprConcept, ShapeFreeFunctions, NamespaceHygiene) listed with their cases | PASS |
| Working tree post-verification | `git status -s && git diff --stat` | empty (no leftover modifications from negative-path test) | PASS |

---

### Requirements Coverage

| REQ ID | Source Plan | Description | Status | Evidence |
|--------|-------------|-------------|--------|----------|
| STG-01 | 01-01 | Construct `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}` with shape and optional fill | SATISFIED | `Tensor<T>` is open template (no static_assert on T); `ElementTypes.AllFourTypesConstruct` runtime test + 5× `is_nothrow_move_constructible_v<Tensor<T>>` static_asserts cover all 4 types. |
| STG-02 | 01-01 | Query `shape()`, `size()`, `rank()` | SATISFIED | `tensor.h:47–49`. Tests `ShapeOnlyZeroInitializes`, `ShapeWithFill`, `ShapeWithInitList` all assert all three accessors. |
| STG-03 | 01-01 | Index by multi-dimensional coordinates `tensor(i,j,k)` | SATISFIED | `operator()` lines 60, 64; `at()` lines 68, 73. Tests `OperatorParensReadsAndWrites`, `AtPassesValidIndex`, `AtThrowsOnRankMismatch`, `AtThrowsOnBoundsViolation` cover all paths. |
| STG-04 | 01-01 | `data() -> const T*` for raw contiguous buffer | SATISFIED | `tensor.h:51` returns `const T*`. Test `TensorData.DataPointerMatchesFirstElement` reads `p[0]` and `p[5]`; `static_assert(std::is_same_v<decltype(t.data()), const float*>)` enforces const-only contract. |
| STG-05 | 01-01 | `begin()` / `end()` iterators (range-for compatible) | SATISFIED | `tensor.h:53–56`. Tests `RangeForCompatible` (range-for) and `BeginEndForwardToVector` (manual iterator) both PASS. |
| EXP-01 | 01-01 | `Expression<Derived>` CRTP base with `shape()`, `size()`, `eval(linear_idx)` | SATISFIED | `expression.h:13–25`. Test `ExpressionBase.TensorDerivesFromExpression` static_asserts inheritance; `EvalReturnsValue` asserts `eval(0)` returns `T` by value with `decltype` static_assert. |
| EXP-02 | 01-01 | `IsTensorExpr` concept matches every `Expression<Derived>`-derived type | SATISFIED | `expression.h:48`. Tests `IsTensorExprConcept.{AcceptsTensor, RejectsNonExpression, StripsRefAndCV}` all PASS. 11 distinct `static_assert` checks cover positive (4 types), negative (4 non-Expression types), and cv-stripping (3 reference forms). |
| BLD-01 | 01-01 | Tensor framework lives under `include/quiver/tensor/` mirroring `include/quiver/binary/` precedent (header-only) | SATISFIED | 3 headers under `include/quiver/tensor/`. No `src/tensor/` directory. `find src/ -name 'tensor*.cpp'` returns nothing. |
| BLD-02 | 01-02 | Per-concern GoogleTest files under `tests/test_tensor_*.cpp` | SATISFIED | `tests/test_tensor_storage.cpp` exists. Naming matches existing `test_database_*.cpp`/`test_binary_*.cpp` precedent. (REQUIREMENTS.md notes additional test_tensor_arithmetic/broadcast/reduce/aliasing/adl files are added in Phases 2–4 — out of scope for Phase 1.) |
| BLD-03 | 01-02 | New tensor sources/tests integrate via `tests/CMakeLists.txt` only — no edits to `src/CMakeLists.txt` | SATISFIED | `git diff --stat master..HEAD -- src/CMakeLists.txt` returns empty. Header-only design verified. |
| BLD-04 | 01-02 | CI test enforces include containment | SATISFIED | `cmake/CheckTensorIncludeContainment.cmake` + `add_test(NAME tensor_no_leakage ...)`. Both positive (PASS on clean) and negative (FAIL on violation) paths independently verified by verifier. |

**11/11 requirements SATISFIED. No orphaned requirements: REQUIREMENTS.md maps exactly STG-01..05, EXP-01, EXP-02, BLD-01..04 to Phase 1; all 11 appear in plan frontmatter.**

---

### Anti-Patterns Found

No blocker or warning-level anti-patterns were detected. The full anti-pattern scan ran on the 6 phase artifacts (3 tensor headers + 1 cmake script + 1 test source + 1 CMakeLists.txt MOD):

- **TODO/FIXME/PLACEHOLDER markers:** zero matches in any phase artifact.
- **Empty implementations / stub returns:** zero matches. Every function has substantive body. The two protected/private-default usages (`Expression() = default;`, `Tensor<T> dtor/copy/move = default`) are intentional Rule-of-Zero defaults, not stubs.
- **Hardcoded empty data:** the only `= []`/`= {}` patterns are intentional (e.g., `Tensor<double> t({}, 42.0)` in tests deliberately exercises 0-D tensor representation per D-11).
- **Comment-only handlers / console.log:** none.
- **Three-pattern error format violations:** none. All 4 throws use Pattern 1 with the locked wording verbatim.
- **Pitfall #4 leakage:** `grep -rn 'quiver/tensor' include/quiver/` returns no matches outside tensor/ — already independently verified.
- **Pitfall #14 namespace hygiene:** `grep -nP "^\s*namespace\s+quiver\s*\{" include/quiver/tensor/*.h` returns no matches — only `namespace quiver::tensor` and nested `namespace detail` appear.
- **Anti-Pattern #6 (silent copy-on-grow):** explicitly mitigated. Both `Tensor<T>(Tensor<T>&&)` and `Tensor<T>& operator=(Tensor<T>&&)` are `noexcept = default`. Compile-time gate via 5× `static_assert(std::is_nothrow_move_constructible_v<Tensor<T>>)`.

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | — | — | — | — |

---

### Pitfall / Anti-Pattern Guard Verification

| Guard | Mechanism | Status |
|-------|-----------|--------|
| **Pitfall #4 (header-only template heaviness leaking library-wide)** | `cmake/CheckTensorIncludeContainment.cmake` registered as CTest target; positive + negative paths verified independently | PASS |
| **Pitfall #13 (cross-phase discipline — bridge designed too early)** | `git diff --stat master..HEAD` over `include/quiver/c/`, `include/quiver/c/binary/`, `bindings/`, `src/CMakeLists.txt`, `cmake/Dependencies.cmake`, `cmake/CompilerOptions.cmake`, top-level `CMakeLists.txt`, `include/quiver/quiver.h` returns empty | PASS |
| **Pitfall #14 (namespace hygiene)** | `grep -nP "^\s*namespace\s+quiver\s*\{" include/quiver/tensor/*.h` returns empty; only `namespace quiver::tensor` and nested `detail::` appear | PASS |
| **Anti-Pattern #6 (noexcept move)** | `Tensor<T>(Tensor<T>&&) noexcept = default;` line 43; `Tensor<T>& operator=(Tensor<T>&&) noexcept = default;` line 44; `static_assert(std::is_nothrow_move_constructible_v<Tensor<T>>)` for all 4 element types | PASS |
| **Pitfall B (division-by-zero in stride math on 0-sized dims)** | `compute_strides` uses multiplication only — `awk '/inline shape_t compute_strides/,/^}$/' shape.h \| grep -c '/'` returns 0; runtime test `TensorConstruction.ZeroSizedDimensionValid` PASSES | PASS |
| **Pitfall C (empty fold expression for 0-D tensors)** | `ravel_index` C++17 fold `((result += idxs * strides[i++]), ...)` expands to nothing for 0 args; runtime test `ZeroDTensor.EmptyShapeHasOneElement` calls `t()` (no args) and asserts `42.0` | PASS |
| **Pitfall E (sink-parameter idiom)** | All 3 ctors take `shape_t shape` by value, then `shape_(std::move(shape))` member-init; verified at lines 19, 24, 29 of tensor.h | PASS |
| **WARNING-02 / NOTE-05 (single source of truth for shape_t)** | `using shape_t = std::vector<std::size_t>;` declared exactly once in `shape.h:9`; `expression.h` and `tensor.h` `#include "shape.h"` instead of redeclaring; verified by grep counts (1, 0, 0) | PASS |
| **NOTE-02 (simplified throw-assertion idiom)** | `grep -c "EXPECT_THROW(" tests/test_tensor_storage.cpp` returns 0; `grep -c 'FAIL() << "expected std::runtime_error"' tests/test_tensor_storage.cpp` returns 4 (one per Pattern 1 throw site) | PASS |
| **NOTE-04 (BLD-04 negative-path verification)** | Independently re-verified by verifier (not just trusting SUMMARY): inject + observe FATAL_ERROR + revert + `git diff` empty post-revert | PASS |

---

### Human Verification Required

None. Phase 1 ships pure C++20 header-only template code plus a CMake-script CTest target. There are no UI/UX, real-time, external-service, or visual concerns. Every phase requirement has automated verification (GoogleTest case + compile-time `static_assert` + CTest target). The validation strategy at `01-VALIDATION.md` explicitly states: *"All phase behaviors have automated verification."*

---

### Gaps Summary

No gaps. Phase 1 delivers exactly what the goal requires:

- All 5 ROADMAP success criteria PASS.
- All 11 requirements (STG-01..05, EXP-01, EXP-02, BLD-01..04) SATISFIED with concrete code + test evidence.
- All 4 critical pitfall guards (#4, #13, #14, Anti-Pattern #6) operate correctly.
- All artifacts pass all 4 verification levels (existence, substance, wiring, data flow).
- 28/28 tensor GoogleTest cases PASS; full regression 1170/1170 PASS.
- Cross-phase discipline gate is at-baseline: zero diff to all 8 protected paths.
- BLD-04 mechanism's failure path was independently re-verified by the verifier (not just trusted from SUMMARY) — the script does emit `FATAL_ERROR` with `BLD-04 violation` and the offending file:line when a violation is injected, and exits cleanly after revert.

The phase goal — *"User can construct a `quiver::tensor::Tensor<T>`, query its shape/size/rank, index it by multi-dimensional coordinates, iterate it, and read its raw buffer — and generic code can identify any tensor expression via the `IsTensorExpr` concept"* — is achieved, observable in the codebase, and exercised by passing tests at runtime and at compile time.

---

## Final Verdict: PASS

Phase 1 is goal-complete. The 28-truth must-have surface is fully verified, including independent re-verification of the BLD-04 negative path (the most failure-prone gate, since SUMMARY claims about manual one-shot tests are easy to overstate). The narrow scope of Phase 1 — header-only C++20 templates + a CMake script — leaves zero ambiguity in verification: if the test binary loads, lists 28 cases, and runs them green, every observable behavior of the goal has fired. Anti-Pattern #6 (silent copy-on-grow), Pitfalls #4 (header heaviness), #13 (cross-phase scope), and #14 (namespace hygiene) all have hard-gated machine-verifiable evidence on disk.

**Ready to proceed to Phase 2 (Lazy Arithmetic and Materialization).**

---

*Verified: 2026-05-05T22:07:39Z*
*Verifier: Claude (gsd-verifier)*
