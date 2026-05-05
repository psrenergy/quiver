# Phase 1: Storage and CRTP Scaffold - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-05
**Phase:** 1-Storage and CRTP Scaffold
**Areas discussed:** Tensor<T> constructor surface, Indexing & bounds checking, 0-D tensor representation, BLD-04 enforcement mechanism

---

## Tensor<T> Constructor Surface

### Q1: Beyond shape+fill, what flat-data constructor(s) should Tensor<T> ship in Phase 1?

| Option | Description | Selected |
|--------|-------------|----------|
| initializer_list only | Tensor<double>({3,4}, {1,2,...,12}) — std::vector convention, ideal for test fixtures. xtensor-style. Defer iterator-range and take-ownership to v0.9.0. | ✓ |
| Add iterator-range too | Plus Tensor(shape, It begin, It end) for any input range. Two ctors instead of one. Slightly more compile time. | |
| Full kit: also take-ownership | Plus Tensor(shape, std::vector<T>&&) for zero-copy ownership transfer. Risks shaping API around future bridge work too early. | |
| Defer all flat-data ctors | Only Tensor(shape) and Tensor(shape, fill) ship. Tests write values via op()= loops. Adds friction in Phase 2 tests. | |

**User's choice:** initializer_list only (Recommended)
**Notes:** Minimal surface that meets test-fixture ergonomics for Phase 2-4. Iterator-range and ownership-transfer can be added later non-breaking.

### Q2: Should Phase 1 ship NumPy-style factory functions (zeros, ones, full)?

| Option | Description | Selected |
|--------|-------------|----------|
| Defer to v0.9.0 | Phase 1 stays minimal. Users call Tensor<double>(shape, 0.0) directly. Matches v0.8.0 minimal-scope bias. | ✓ |
| Ship zeros/ones/full in Phase 1 | Free functions in quiver::tensor returning Tensor<T>. NumPy idiom. Locks naming early. | |
| Ship zeros/ones/full + zeros_like(expr) | Adds zeros_like for Phase 4 reduction outputs. Pre-investing in Phase 4 ergonomics. May trigger CRTP traits earlier. | |

**User's choice:** Defer to v0.9.0 (Recommended)
**Notes:** Aligns with PROJECT.md "minimal scope" framing for v0.8.0.

### Q3: How should Tensor(shape) initialize storage when no fill value is given?

| Option | Description | Selected |
|--------|-------------|----------|
| Always zero-initialize | data_.assign(total_size, T{}). Predictable, NumPy np.zeros default. Slight perf cost on large shapes. | ✓ |
| Default-init (uninitialized for trivial types) | Like std::vector::resize. Eigen does this. Fast, but UB landmine for tests. | |
| Force fill argument | Drop the no-fill ctor entirely. Removes ambiguity but breaks "optional fill" wording. | |

**User's choice:** Always zero-initialize (Recommended)

### Q4: Should constructors accept 0-sized dimensions (e.g., Tensor<double>({2, 0, 3}))?

| Option | Description | Selected |
|--------|-------------|----------|
| Allow, total_size=0, no allocation | NumPy-compatible. Phase 3 broadcasting expects 0-sized dim handling. Phase 4 reductions need empty-tensor policy. | ✓ |
| Reject in Phase 1, revisit in Phase 3 | Throw "Cannot construct: zero-sized dimension". Retrofit risk for Phase 3 oracle tests. | |
| Allow but require all-or-nothing | Either total_size > 0, or shape exactly empty {}. Reject mixed shapes like {2,0,3}. Breaks NumPy compatibility. | |

**User's choice:** Allow, total_size=0, no allocation (Recommended)
**Notes:** Pre-empts Pitfall #9 retrofitting cost.

---

## Indexing & Bounds Checking

### Q1: What's the bounds-checking contract for tensor(i, j, k)?

| Option | Description | Selected |
|--------|-------------|----------|
| operator() unchecked + at() checked | Canonical std::vector convention. tensor(i,j,k) fast; tensor.at(i,j,k) throws three-pattern error. | ✓ |
| operator() always bounds-checked | Safer; defeats vectorization in tests using op() inside loops. | |
| operator() debug-only checks (assert) | Fast Release; doesn't match Quiver's runtime_error convention. | |
| operator() unchecked, no at() | Trust contract entirely. Smallest API. Tests rely on correctness. | |

**User's choice:** operator() unchecked + at() checked (Recommended)
**Notes:** CLAUDE.md anti-pattern #7 only forbids throws inside eval(i) hot path — op() is a user access point, not the assignment loop.

### Q2: Should multi-dim indexing accept negative indices (NumPy-style)?

| Option | Description | Selected |
|--------|-------------|----------|
| No, std::size_t only | Matches shape_t. Fastest, no per-access wrap branch. Aligns with deferred RED-04 negative-axis. | ✓ |
| Yes, accept signed indices and wrap | NumPy idiom; nicer for energy-modeling users. Per-access branch cost. Inconsistent with RED-04 defer. | |
| Defer the question — ship op(size_t) for now | Same mechanically, but template op() to flex later. | |

**User's choice:** No, std::size_t only (Recommended)

### Q3: What iterator type should Tensor<T>::begin()/end() expose?

| Option | Description | Selected |
|--------|-------------|----------|
| Expose std::vector<T>::iterator | Forward to data_.begin()/end(). Zero work, std::ranges-compatible. Future strided views get their own iterators. | ✓ |
| Raw T*/const T* | Less STL-ish but legitimate. Loses iterator type identity. | |
| Custom iterator class | Future-flexibility for strided views. Premature: views get their own Expression nodes. | |

**User's choice:** Expose std::vector<T>::iterator (Recommended)

### Q4: How should op()/at() handle rank-vs-index-count mismatch?

| Option | Description | Selected |
|--------|-------------|----------|
| op() trusts contract; at() throws | op(Idx... idxs) computes offset assuming sizeof...(Idx) == rank — wrong rank → UB. at() throws three-pattern error. | ✓ |
| Both op() and at() throw on rank mismatch | op() does cheap sizeof...(Idx) != rank check. Adds branch. Slight Q1 inconsistency. | |
| static_assert on type only; rank to at() | Compile-time integral type check. Doesn't help with rank (dynamic). | |
| Both: static_assert type + at() throws on rank | Type safety free; rank in at() only. | |

**User's choice:** op() trusts contract; at() throws (Recommended)

---

## 0-D Tensor Representation

### Q1: How should a 0-D (rank-0) tensor be represented in Quiver?

| Option | Description | Selected |
|--------|-------------|----------|
| Empty shape {}, total_size=1 | NumPy/xtensor convention. rank()=0, size()=1, data_ has 1 element. Phase 3 broadcasting falls out naturally. | ✓ |
| Always rank ≥ 1, use shape {1} | Avoids "empty vector with one element" weirdness. Loses semantic distinction. Phase 4 .item() needs runtime check. | |
| Separate Scalar<T> wrapper class | Distinct CRTP leaf for 0-D. Higher conceptual complexity. Breaks REQ-RED-01 wording ("returning 0-D Tensor<T>"). | |

**User's choice:** Empty shape {}, total_size=1 (Recommended)
**Notes:** HIGH cost-of-late-recovery decision — locks downstream Phases 3 and 4.

### Q2: Should .item() ship in Phase 1, or wait for Phase 4 reductions?

| Option | Description | Selected |
|--------|-------------|----------|
| Ship in Phase 1 | Tensor<T>::item() returns the single element, throws on size != 1. Tests can construct/verify 0-D tensors immediately. | ✓ |
| Defer to Phase 4 | Tightens Phase 1 surface. Tests use op()() or data()[0] for 0-D. | |

**User's choice:** Ship in Phase 1 (Recommended)

### Q3: How should users construct a 0-D Tensor<T>?

| Option | Description | Selected |
|--------|-------------|----------|
| Just the constructor: Tensor<double>({}, 42.0) | Reuses constructor surface from Area 1. No new API. Slightly clunky reading. | ✓ |
| Add Tensor<T>::scalar(T value) static factory | Self-documenting. Asymmetric with deferred zeros/ones. | |
| Add free function quiver::tensor::scalar<T>(value) | Symmetric with future zeros/ones. | |

**User's choice:** Just the constructor (Recommended)

### Q4: What should the IsTensorExpr concept actually check?

| Option | Description | Selected |
|--------|-------------|----------|
| Derives from Expression<U> | Strict CRTP via std::is_base_of-style detection. Eigen / xtensor convention. | ✓ |
| Duck-typed: has shape(), size(), eval(size_t) | Allows non-Expression types to participate. ADL collision risk with std::complex. | |
| Both: derives + has methods | Strictest. Slightly slower template instantiation. | |

**User's choice:** Derives from Expression<U> (Recommended)

---

## BLD-04 Enforcement Mechanism

### Q1: What's the primary enforcement mechanism for BLD-04?

| Option | Description | Selected |
|--------|-------------|----------|
| CTest grep target | add_test running grep -rE 'quiver/tensor' include/quiver/ --exclude-dir=tensor. Simple, fast, mirrors CI patterns. | ✓ |
| Pre-commit hook | Local check before commit. Bypassable via --no-verify. Adds hook contributors may not run. | |
| Compile-time test file | test_no_tensor_leakage.cpp with #ifdef QUIVER_TENSOR_INCLUDED guards. Mechanically airtight, slightly heavier. | |
| Layered: CTest grep + compile-time test | Belt-and-suspenders. Two test targets, more maintenance. | |

**User's choice:** CTest grep target (Recommended)

### Q2: What patterns should the grep test detect as violations?

| Option | Description | Selected |
|--------|-------------|----------|
| #include lines only | grep -E '#\s*include\s*[<"]quiver/tensor/'. Detects both forms. Sufficient — symbol references can't materialize without an include somewhere. | ✓ |
| #include + symbol references | Adds 'quiver::tensor::' or 'Tensor<' grepping. False-positive risk on doc comments. | |
| AST-based check (clang tooling) | Most precise. Heavyweight clang dependency. | |

**User's choice:** #include lines only (Recommended)

### Q3: What directory scope should the leak check cover?

| Option | Description | Selected |
|--------|-------------|----------|
| All public headers | include/quiver/ recursive, excluding include/quiver/tensor/. Covers public C++ and C API headers. New headers auto-covered. | ✓ |
| Public + src/ files | Also recurse into src/ excluding src/tensor/. Catches compile-cost leakage at source level. | |
| Just include/quiver/ (excluding tensor/, c/, binary/) | Narrow scope. Misses binary/*.h transitively pulling tensor. | |

**User's choice:** All public headers (Recommended)

### Q4: How should the leak check be wired into Quiver's existing build infrastructure?

| Option | Description | Selected |
|--------|-------------|----------|
| CTest add_test() + CMake script | tests/CMakeLists.txt add_test calling cmake/CheckTensorIncludeContainment.cmake. ctest -R no_leakage. Cross-platform via CMake. Natural fit. | ✓ |
| Pre-commit hook + CTest | Two integration points; same script. Extra robustness. | |
| Standalone scripts/check-bld04.{bat,sh} | Two scripts (Windows + POSIX). Doesn't fit CTest pattern. Platform-fragmentation risk. | |
| GitHub Actions step in ci.yml | Pure CI step. No local-developer feedback. Doesn't satisfy "CI test" wording strictly. | |

**User's choice:** CTest add_test() + CMake script (Recommended)

---

## Claude's Discretion

The user accepted the Recommended option on every question, which delegates the following implementation choices to Claude:

- Exact template-detection idiom for IsTensorExpr (`std::is_base_of_v<Expression<U>, T>` helper vs. `std::derived_from` probe).
- Header-file split granularity inside `include/quiver/tensor/` (current research recommendation: `expression.h`, `shape.h`, `tensor.h`).
- Whether `compute_strides()`, `total_size()`, `ravel_index()` live in `shape.h` or `quiver::tensor::detail::`.
- Whether `eval(linear_idx)` returns `T` by value or `const T&`.
- Test file naming sub-splits within `tests/test_tensor_*.cpp` if the initial `test_tensor_storage.cpp` grows beyond ~500 lines.
- Concrete error-message wording within the locked three-pattern format.

## Deferred Ideas

The following ideas were noted during discussion but belong in later milestones (see CONTEXT.md "Deferred Ideas" for the canonical list):

- Iterator-range and `std::vector<T>&&` take-ownership constructors — v0.9.0
- Factory functions (`zeros`, `ones`, `full`, `zeros_like`) — v0.9.0
- Negative element indexing — v0.9.0
- Mutable `data() -> T*` accessor — v0.9.0 (REQ-POL-02)
- `Tensor<T>::scalar(value)` named factory — rejected; revisit only if v0.9.0 user feedback indicates pain
- Symbol-reference grepping / AST-based BLD-04 check — future hardening milestone
- Pre-commit hook duplicate of BLD-04 check — future hardening milestone
