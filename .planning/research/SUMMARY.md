# Project Research Summary

**Project:** Quiver v0.8.0 — Lazy Tensor Framework
**Domain:** Header-only lazy-evaluating dense CPU element-wise tensor framework in C++20 (CRTP Expression Templates), added as a new subsystem in an existing SQLite-wrapper library
**Researched:** 2026-05-05
**Confidence:** HIGH

## Executive Summary

Quiver v0.8.0 adds a fourth subsystem (parallel to Database, Binary, LuaRunner) implementing a **lazy CRTP Expression Template tensor framework** under `include/quiver/tensor/`. The architecture is the canonical industry pattern shared by Eigen, xtensor, and Blaze: a CRTP `Expression<Derived>` base, leaf storage `Tensor<T>`, intermediate `BinaryOp`/`UnaryOp`/`BroadcastView`/`ReduceExpression` nodes, free-function operator overloads (or hidden friends), and a single materialization point at `Tensor<T>::operator=(const Expression<E>&)` where the fused loop runs. The stack verdict from STACK.md is unambiguous: **add NO new dependencies** — the entire framework ships using C++20 stdlib (`<concepts>`, `<type_traits>`, `<vector>`, `<cmath>`) and the existing GoogleTest harness. Eigen, xtensor, Blaze, Boost.MultiArray, Highway, xsimd, and `<mdspan>` are all explicitly rejected for this milestone (matrix-algebra scope mismatch, transitive deps, premature SIMD coupling, or C++23 toolchain risk).

The scope decomposes cleanly along functional lines from FEATURES.md: **storage (REQ-STG)** → **expression core (REQ-EXP)** → **arithmetic/assignment/materialization (REQ-ARI, REQ-ASN)** → **math functions (REQ-MAT)** → **NumPy-style shape broadcasting (REQ-BCT)** → **reductions sum/mean/max with optional axis (REQ-RED)** → **hardening**. Build/test infrastructure (REQ-BLD) and diagnostics (REQ-DIA, three-pattern error messages, `to_string`) span the milestone. Out-of-scope items from PROJECT.md must stay rigorously out: no Element/Database bridge (`Database::read_tensor`), no `quiver_tensor_*` C API, no language bindings, no autograd, no GPU, no sparse, no matmul/einsum/decompositions, no comparison operators with `Tensor<bool>`. The discipline check from PITFALLS.md Pitfall #13 is "no diff to `include/quiver/c/` or `bindings/` in v0.8.0."

The single biggest risk is the **storage policy decision in Phase 2** that interlocks Pitfalls #1 (dangling references), #2 (`auto`-with-expressions), #3 (aliasing on `a = a + a`), #5 (ODR via non-`inline` operators), #6 (ADL hijacking with `std::complex`), and #8 (implicit type narrowing). These six pitfalls all attack the same surface — how `BinaryOp` holds operands, how operator overloads are declared, how the assignment loop is structured. Eigen and xtensor both converged on the same answer (Eigen calls it `internal::ref_selector` + `nested_eval`; xtensor calls it `xclosure_t`): **leaves stored by `const&`, intermediate temporaries stored by value, operators as templates or hidden friends, materialize-via-temporary inside `operator=`**. Locking this policy in Phase 2 is HIGH-cost-of-late-recovery; the roadmap must treat Phase 2 as the milestone's correctness load-bearing phase. After that, Phase 4 (broadcasting) and Phase 5 (reductions) reuse the rank arithmetic and are the only phases likely to need additional research spikes — broadcasting because of NumPy rule corner cases (size-1 vs missing dim, 0-sized dims, 0-D scalars), reductions because of accumulator-type promotion (`mean<int>` integer-division, `sum<int>` overflow, NaN/empty/negative-axis policy).

## Key Findings

### Recommended Stack

**See `.planning/research/STACK.md` for full verdicts.** TL;DR: pure C++20 standard library + Quiver's existing dependencies. No new `FetchContent_Declare` lines for v0.8.0 core. GoogleTest is already declared and gated by `QUIVER_BUILD_TESTS`.

**Core technologies:**
- **C++20 standard library** (`<concepts>`, `<type_traits>`, `<vector>`, `<array>`, `<cmath>`, `<numeric>`) — toolchain-bundled; CRTP, Expression Templates, scalar broadcast, NumPy broadcasting, and reductions can all be expressed without external libs.
- **CRTP idiom** — language pattern; static polymorphism for `Expression<Derived>` base; matches Eigen/xtensor/Blaze precedent; zero v-table overhead; required by the "deterministic compile-time dispatch" constraint in PROJECT.md.
- **`std::span` (C++20)** — non-owning views over flat data buffers; cheaper than backporting Kokkos `mdspan-0.6.0` since Quiver's tensor only needs strided 1-D viewing.
- **GoogleTest 1.17.0** (already vendored) — covers `tests/test_tensor_*.cpp`; mirrors existing per-concern split.
- **Header-only deliverable**: `include/quiver/tensor/` parallels `include/quiver/binary/`. `src/tensor/` is created only if a non-template helper accumulates (default: don't create).

**Explicitly avoided:** Eigen 5.0.1 (matrix-algebra scope mismatch, compile-time blast radius, vocabulary leak through bindings), xtensor 0.27.1 (hard transitive dep on xtl 0.8+, contradicts the "learning/control" framing), Blaze (stale, BLAS-replacement scope), Boost.MultiArray/uBLAS (Boost-bloat, no lazy eval), `<mdspan>` (C++23 toolchain risk on MSVC and Apple Clang), Highway/xsimd (premature SIMD coupling — the lazy core is supposed to stay evaluator-agnostic for a future `eval_into_simd` path), `std::execution::par` (Apple Clang libc++ does not ship parallel policies), Google Benchmark (correctness milestone, not throughput milestone).

### Expected Features

**See `.planning/research/FEATURES.md` for full feature landscape.** Categories use REQ-ID prefixes the roadmapper can lift directly: `STG` (Storage), `EXP` (Expression Core), `ARI` (Arithmetic), `MAT` (Math Functions), `BCT` (Broadcasting), `RED` (Reductions), `ASN` (Assignment & Materialization), `DIA` (Diagnostics), `BLD` (Build & Test Harness).

**Must have (table stakes — P1, in v0.8.0 MVP per PROJECT.md):**
- `STG`: `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}` with shape/strides/contiguous buffer, `operator()(idx...)`, copy/move
- `EXP`: `Expression<Derived>` CRTP base with `shape()`, `rank()`, `eval(linear_idx)`, closure semantics
- `ARI`: `BinaryOp` with `+ - * /`, type promotion via `std::common_type_t`, unary `-`, scalar broadcasts on both sides
- `ASN`: `Tensor<T>::operator=(const Expression<E>&)` with materialize-via-temporary for alias safety; `eval(expr) -> Tensor<T>` free function
- `BCT`: NumPy-style right-aligned shape broadcasting, size-1 expansion, scalar leaves
- `RED`: full-tensor `sum`, `mean`, `max` returning 0-D `Tensor<T>`; single-axis variants returning rank-shrunk tensors
- `DIA`: shape-mismatch error messages following Quiver's three-pattern rule (`Cannot broadcast: ...`, `Cannot reduce: ...`); `to_string` / `operator<<` for `Tensor<T>` with truncation
- `BLD`: `include/quiver/tensor/` header layout, `tests/test_tensor_*.cpp` mirroring existing convention

**Should have (P2 — within scope, low-risk):**
- `MAT`: `abs`, `exp`, `log`, `sqrt` as free functions in `quiver::tensor`
- `STG`: `begin()` / `end()` iterators on `Tensor<T>` (not on lazy expressions)
- `DIA`: `static_assert` on type-mismatched assignment with named UPPERCASE message (Eigen-style grep-able)

**Defer (v0.9.0+ per PROJECT.md):**
- Comparison operators (`< > == !=`) returning `Tensor<bool>` + `where` ternary + `any`/`all`
- Multi-axis reductions, `prod`, `argmin`/`argmax`, `nansum`/`nanmean`
- Reshape, transpose, slicing/views (`xt::view`-equivalent), `concat`/`stack`
- Trig (`sin`, `cos`), rounding (`floor`, `ceil`), `pow`, `clip`
- Element/Database bridge (`Database::read_tensor`, `Element::set(Tensor)`)
- Tensor C API (`quiver_tensor_*`) and language bindings
- Parallelization, autograd, GPU, sparse, BLAS-backed linalg

### Architecture Approach

**See `.planning/research/ARCHITECTURE.md` for full pattern catalog.** The tensor subsystem is **a leaf** in v0.8.0 — no upstream consumers in C++ (Database does not call into it), no C API surface, no bindings. It compiles as part of `libquiver` but exposes only header-only templates. Mirrors `include/quiver/binary/` exactly in layout depth.

**Major components:**
1. **`Expression<Derived>` (CRTP base)** — `self()` static cast; common interface (`shape()`, `size()`, `eval(i)`); no virtual functions; defined in `include/quiver/tensor/expression.h`.
2. **`Tensor<T>` (owning storage leaf)** — `std::vector<T> data_` + `shape_t shape_` (= `std::vector<std::size_t>`) + `shape_t strides_` (row-major); inherits `Expression<Tensor<T>>`; templated `operator=` runs the fused loop.
3. **`BinaryOp<L, Op, R>`, `UnaryOp<E, Op>`** — intermediate nodes; operands stored via `detail::ref_selector` (leaf `Tensor<T>` by `const&`, intermediates by value); evaluate as `Op{}(lhs_.eval(i), rhs_.eval(i))`.
4. **`BroadcastView<E>`** — separate Expression node wrapping inner expression with target shape + per-dim stride map (zero stride for stretched/padded dims); recommended over baking-broadcast-into-BinaryOp because rank-shrunk reductions reuse the same view machinery.
5. **`ReduceExpression<E, Op, axis>`** — lazy node, eager-on-materialize cache (`mutable std::optional<std::vector<T>> cache_`); first `eval(i)` runs the reduction loop, subsequent calls serve from cache.
6. **Functors (`ops::Add Sub Mul Div Neg Abs Exp Log Sqrt`)** — stateless structs in `quiver::tensor::ops::`; passed as the `Op` template parameter; never in `std::` namespace (anti-ADL-hijacking).
7. **Free-function operator overloads** in `quiver::tensor` namespace (or hidden friends inside `Expression`) — `operator+`, `-`, `*`, `/`, unary `-`, `abs`, `exp`, `log`, `sqrt`, `sum`, `mean`, `max`, `eval`, `broadcast_shape`. ADL-safe and ODR-safe.

**Header-only with no Pimpl** (existing `Dimension`/`TimeProperties` precedent). **Single materialization point** = `Tensor<T>::operator=(const Expression<E>&)`. After inlining at `-O2`, `Tensor<double> d = a + b * c;` collapses to one fused loop with zero intermediate buffers.

### Critical Pitfalls

**See `.planning/research/PITFALLS.md` for all 17 pitfalls + 18-item "looks done but isn't" checklist + recovery-cost matrix + pitfall-to-phase mapping.** Top items by recovery cost and breadth:

1. **Pitfall #1 — Dangling references in expression chains stored to `auto`** (HIGH cost). `auto e = a + b; { auto f = e + d; }; result = f;` silently breaks if `BinaryOp` stores by `const&` without policy. **Prevention:** adopt Eigen-style `ref_selector`/`nested_eval` storage in Phase 2.
2. **Pitfall #3 — Aliasing on `a = a + a`** (MEDIUM, blast radius grows per phase). **Prevention:** assignment materializes into a `std::vector<T>` first then swaps into `data_`.
3. **Pitfall #4 — Header-only template heaviness slows the entire `quiver` library compile** (MEDIUM, unbounded growth). **Prevention** in Phase 1: strict include containment — `quiver/tensor/*.h` is *never* included from `quiver/database.h`, `quiver/element.h`, `quiver/binary/*.h`. Add a CTest grep that fails if any non-tensor public header pulls in `tensor/`.
4. **Pitfalls #5 + #6 — ODR violations + ADL hijacking with `std::complex`** (LOW–MEDIUM). **Prevention** in Phase 2: hidden-friend operators inside `Expression` OR constrained templates — solves both pitfalls with one change.
5. **Pitfall #11 — `mean<int>` integer-division and `sum<int>` overflow** (LOW recovery, high user-visible). **Prevention** in Phase 5: default-promote accumulator (`sum<int8_t>` → `int64_t`); `mean` always returns floating-point.
6. **Pitfall #13 — Bridge designed too early** (HIGH; prevention is the only cure). **Discipline check across all phases:** no diff to `include/quiver/c/` or `bindings/` in v0.8.0.

## Implications for Roadmap

Based on the four research files, **the recommended phase structure for v0.8.0 is 5–6 phases**, ordered by feature dependency from FEATURES.md and pitfall prevention from PITFALLS.md.

### Phase 1: Storage, CRTP scaffold, and build harness
**Rationale:** Without `Tensor<T>` and `Expression<Derived>`, nothing compiles. Locks include-containment discipline (Pitfall #4) and namespace hygiene (Pitfall #14) at framework birth — both have HIGH cost-of-late-recovery if missed.
**Delivers:** `expression.h` (CRTP base, `IsTensorExpr` concept), `shape.h` (shape_t, `compute_strides`, `total_size`), `tensor.h` (Tensor<T> storage with `operator()(idx...)`, copy/move, `data()`, `size()`, `shape()`, `rank()`, `begin()`/`end()`), `tests/test_tensor.cpp`, CMake wiring, CI grep test for include containment, per-file compile-time budget (~10s Release on slowest CI runner).
**Addresses:** REQ-STG-*, REQ-EXP-* (CRTP base only), REQ-BLD-*, foundation for REQ-DIA-*.
**Avoids (Pitfalls):** #4, #14, partial #7.

### Phase 2: Element-wise arithmetic, assignment, materialization (load-bearing)
**Rationale:** This is the milestone's correctness lock-in. All HIGH-cost pitfalls (#1, #3, #5, #6, #8) and most MEDIUM-cost pitfalls (#15, #16) get prevented here. Once `BinaryOp`/`UnaryOp` ship, retrofitting their storage policy or operator-declaration form touches every instantiation. Treat as the most slack + scrutiny phase.
**Delivers:** `ops.h` (functor structs), `binary_op.h` (BinaryOp with `detail::ref_selector` storage policy; `+ - * /` as hidden friends or constrained templates), `unary_op.h` (UnaryOp + unary `-`), `Scalar<T>` 0-D wrapper for scalar broadcasts, `Tensor<T>::operator=(const Expression<E>&)` materialize-via-temporary for alias safety, `eval(expr) -> Tensor<T>`, `to_string`/`operator<<` with truncation, three-pattern error messages, `tests/test_tensor_arithmetic.cpp`, `tests/test_tensor_aliasing.cpp` (regression for `a = a + a`), `tests/test_tensor_adl.cpp` (`Tensor<std::complex<double>>` resolves correctly under `using namespace std;`), allocation-count regression test (zero allocs in `a + b + c + d`), cross-compiler multi-TU link test.
**Addresses:** REQ-EXP-* (closure semantics), REQ-ARI-*, REQ-ASN-*, core REQ-DIA-*. **Optionally include REQ-MAT-*** (`abs`, `exp`, `log`, `sqrt`) inside this phase unless it craters.
**Avoids (Pitfalls):** #1, #2, #3, #5, #6, #8, #15, #16.

### Phase 3 (optional split): Math functions
**Rationale:** Mechanical given Phase 2's `UnaryOp` pattern; one line per function. Split only if Phase 2 over budget.
**Delivers:** `abs`, `exp`, `log`, `sqrt` free functions; `tests/test_tensor_math.cpp` covering `{int32, int64, float, double}`.
**Addresses:** REQ-MAT-*.

### Phase 4: NumPy-style shape broadcasting
**Rationale:** Single biggest design call. Must come *after* Phase 2 (reuses storage policy and assignment loop) and *before* Phase 5 (reductions reuse rank arithmetic and per-dim stride map).
**Delivers:** `broadcast.h` (BroadcastView<E> separate Expression node with per-dim stride map; `broadcast_shape(a, b)` helper; `broadcast_to(expr, shape)` public function), NumPy right-align rule + size-1 stretch, 0-D scalar handling unified, `tests/test_tensor_broadcast.cpp` (~50 combinations across `(scalar, 1D, 2D, 3D)²`), NumPy round-trip oracle test, broadcast-aliasing regression, three-pattern error (`Cannot broadcast: shapes [3,4] and [3,5] do not align`).
**Addresses:** REQ-BCT-*, extends REQ-DIA-*.
**Avoids (Pitfalls):** #9, #10, verifies #3.
**Research flag:** **YES** — NumPy spec corner cases have bitten production tensor libraries (xtensor PR #1575, issue #907). Run `/gsd-research-phase` for this phase.

### Phase 5: Reductions (sum, mean, max with optional axis)
**Rationale:** Depends on broadcasting (rank arithmetic shared, per-axis result rank-shrinks like broadcast destination shapes). Introduces only reduction-specific pitfalls (#11 `mean<int>`, #12 axis/NaN/empty).
**Delivers:** `reduce.h` (ReduceExpression<E, Op, axis> with eager-on-materialize cache via `mutable std::optional<std::vector<T>>`), `sum`/`mean`/`max` free functions taking `(expression, axis = std::nullopt)`, result-rank rule (full reduction → 0-D, single-axis → rank-shrunk), accumulator promotion (`sum<int8>` → `int64`, `mean<int>` → `double`), `normalise_axis` helper supporting NumPy negative-axis, NaN propagation, empty-tensor policy (`sum({}) = 0`, `mean({}) = NaN`, `max({})` throws), `tests/test_tensor_reduce.cpp`, `tests/test_tensor_reduce_edge_cases.cpp` (negative axis, out-of-range, NaN, empty, overflow near INT_MAX), reduce-into-self alias test.
**Addresses:** REQ-RED-*, finishes REQ-DIA-* for reductions.
**Avoids (Pitfalls):** #11, #12, verifies #3.
**Research flag:** **YES** — accumulator promotion + NaN/empty policy + lazy/eager-cache pattern have multiple production precedents that disagree at the edges. Run `/gsd-research-phase`.

### Phase 6: Hardening / polish
**Rationale:** Pitfalls #7 (template error messages), #15 (allocation in ops), #17 (SIMD vectorization) and the 18-item "looks done but isn't" checklist need a dedicated sweep.
**Delivers:** Compile-time budget regression test, named-`static_assert` error-quality regression (compile a known-bad file, grep compiler output for `QUIVER_TENSOR_REQUIRES_*` UPPERCASE prefixes), benchmark vs hand-written `for` loop on `std::vector<double>` (within ~20% on Release), SIMD spot-check via `-fopt-info-vec` / `-Rpass=loop-vectorize`, cross-compiler ODR multi-TU link CI verification, "looks done but isn't" 18-item sweep, README/CONVENTIONS.md updates documenting `auto`-pitfall, aliasing policy, promotion policy, bridge non-goal.
**Addresses:** finishing REQ-DIA-*, polish for REQ-BLD-*.

### Phase Ordering Rationale

- **1 → 2:** Storage and CRTP base must exist before any operator can derive from them. Concepts and namespace hygiene must lock in Phase 1 (HIGH cost-of-late-recovery for #7, #14).
- **2 → 4 (broadcasting):** Broadcasting reuses Phase 2's `ref_selector` storage policy and assignment loop; building broadcasting before Phase 2 produces conflicting assumptions.
- **4 → 5 (reductions):** Reduction result shapes are a special case of broadcasting (0-D reduction broadcasts against everything; per-axis reduction produces a `(d-1)`-D tensor). Rank arithmetic is shared.
- **5 → 6:** Hardening sweep needs all features in place to assert across the surface.
- **REQ-MAT placement:** mechanical given Phase 2's `UnaryOp`; place inside Phase 2 unless that phase exceeds budget.
- **REQ-DIA spans the milestone:** error messages start in Phase 1 (concepts), expand each phase, polish in Phase 6 (named static_asserts, compile-error regression).
- **REQ-BLD spans the milestone:** header layout day one, test files added per phase, compile-budget verified in Phase 6.

### Research Flags

**Needs deeper research during planning** (`/gsd-research-phase`):
- **Phase 4 (Broadcasting)** — NumPy spec corner cases, xtensor real-world bugs (PR #1575, issue #907), oracle setup.
- **Phase 5 (Reductions)** — accumulator promotion across NumPy/pandas/xtensor/Eigen, NaN/empty policy convergence, lazy-node-eager-cache vs pure-lazy trade-off.

**Standard patterns** (skip):
- Phase 1, 2, 3 (if split), 6.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Verdicts cross-validated against direct file reads (CMakeLists, Dependencies.cmake, CompilerOptions), Context7, and toolchain compatibility matrices. "Use NONE" verdict tightly tied to PROJECT.md constraints. |
| Features | HIGH | Cross-checked against Eigen, xtensor, Blaze, NumPy. MEDIUM only on energy-domain differentiator framing (not validated against real PSR Energy code). |
| Architecture | HIGH for in-Quiver placement (sourced from in-tree files); HIGH for canonical CRTP pattern (Eigen + xtensor + Wikipedia + Blaze paper converge). MEDIUM on broadcaster-as-separate-node recommendation (both shipped in production). |
| Pitfalls | HIGH for CRTP/Eigen-derived (#1–3, #5–8, #15–17 documented in Eigen + cppreference + GCC); MEDIUM for Quiver-integration (#13 bridge, #14 naming) extrapolated from CONCERNS.md; LOW for predicted FFI-bridge pitfalls since v0.9.0 is not designed yet. |

**Overall confidence:** HIGH

### Gaps to Address

These need resolution during planning, before or during their respective phases:

- **Element-type set to ship in v0.8.0 (Phase 1):** Default `T ∈ {float, double, int32_t, int64_t}`. Decision needed: include `int8_t`/`int16_t`? `bool`? Recommendation: ship the four; `static_assert` against `Tensor<bool>` for v0.8.0; revisit in v0.9.0.
- **0-D reduction identity (Phase 5):** does `sum(a)` return `Tensor<T>` of shape `{}`, shape `{1}`, or raw `T`? Recommendation: 0-D `Tensor<T>` with `.item()` (matches xtensor); confirm in Phase 5 planning.
- **Compile-time budget threshold (Phase 1):** "10s Release on slowest CI runner" needs calibration — codebase analysis flags sol2/Lua compile time as current bottleneck.
- **Hidden-friend vs constrained-template operators (Phase 2):** both solve Pitfalls #5/#6. Recommendation: hidden friends for binary/unary operators, constrained templates for free functions like `sum`/`mean`/`max`/`abs`/`eval`.
- **Default float accumulator (Phase 5):** `sum<float>` accumulating in `double` is NumPy's choice (reduces FP error). Recommendation: NumPy-compatible by default; opt-out documented.
- **`src/tensor/` translation unit (cross-phase):** default position is "do not create"; re-evaluate at end of Phase 5 if non-template error formatters accumulate.
- **Compound assignment `+= -= *= /=` (Phase 2):** Recommendation: ship in Phase 2 — reuses materialize-via-temporary, cost is small.

## Sources

### Primary (HIGH confidence)

**Quiver internal:**
- `.planning/PROJECT.md`, `CLAUDE.md`, `.planning/codebase/{ARCHITECTURE,STRUCTURE,CONVENTIONS,CONCERNS}.md`
- `include/quiver/binary/{binary_file.h,dimension.h}` (precedents)
- `cmake/{Dependencies,CompilerOptions}.cmake`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`

**Canonical CRTP / Expression Template references:**
- [Wikipedia — Expression templates](https://en.wikipedia.org/wiki/Expression_templates)
- [Wikipedia — CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
- [cppreference — CRTP](https://en.cppreference.com/cpp/language/crtp)
- [Eigen — Class hierarchy](https://libeigen.gitlab.io/eigen/docs-nightly/TopicClassHierarchy.html)
- [Eigen — Common Pitfalls](https://libeigen.gitlab.io/eigen/docs-nightly/TopicPitfalls.html)
- [Eigen — Aliasing](https://libeigen.gitlab.io/eigen/docs-nightly/group__TopicAliasing.html)
- [Eigen — Lazy Evaluation and Aliasing](https://libeigen.gitlab.io/eigen/docs-nightly/TopicLazyEvaluation.html)
- [Eigen — Reductions, Visitors, Broadcasting](https://eigen.tuxfamily.org/dox/group__TutorialReductionsVisitorsBroadcasting.html)
- [xtensor — Concepts](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/developer/concepts.rst)
- [xtensor — Expressions and lazy evaluation](https://xtensor.readthedocs.io/en/latest/expression.html)
- [xtensor — Closure semantics](https://xtensor.readthedocs.io/en/latest/closure-semantics.html)
- [xtensor — `xbroadcast`](https://xtensor.readthedocs.io/en/latest/api/xbroadcast.html)
- [Blaze — Iglberger 2012 paper "Expression Templates Revisited"](https://blogs.fau.de/hager/files/2012/05/ET-SISC-Iglberger2012.pdf)
- [NumPy — Broadcasting basics](https://numpy.org/doc/stable/user/basics.broadcasting.html)

**Cross-compiler / standards:**
- [cppreference — ADL](https://en.cppreference.com/cpp/language/adl)
- [cppreference — ODR](https://en.cppreference.com/cpp/language/definition)
- [MSVC C++23 status](https://devblogs.microsoft.com/cppblog/c23-support-in-msvc-build-tools-14-51/)
- [libc++ C++20 status](https://libcxx.llvm.org/Status/Cxx20.html)
- [GCC `-Wdangling-reference`](https://trofi.github.io/posts/264-gcc-s-new-Wdangling-reference-warning.html)

**Library version verification (2026-05-05):**
- [Eigen 5.0.1](https://gitlab.com/libeigen/eigen/-/tags/5.0.1) (2025-11-08)
- [xtensor releases](https://github.com/xtensor-stack/xtensor/releases)
- [Highway 1.4.0](https://github.com/google/highway/releases/tag/1.4.0) (2026-04-23)
- [Google Benchmark v1.9.5](https://github.com/google/benchmark/releases/tag/v1.9.5)
- [Kokkos mdspan](https://github.com/kokkos/mdspan)

### Secondary (MEDIUM confidence)
- [Modernes C++ — Hidden Friends](https://www.modernescpp.com/index.php/argument-dependent-lookup-and-hidden-friends/)
- [Fluent C++ — CRTP](https://www.fluentcpp.com/2017/05/12/curiously-recurring-template-pattern/)
- [xtensor PR #1575 — broadcasting 0-sized dims](https://github.com/xtensor-stack/xtensor/pull/1575)
- [xtensor issue #907 — broadcast assign](https://github.com/xtensor-stack/xtensor/issues/907)
- [NumPy issue #23075 — nanmean overflow](https://github.com/numpy/numpy/issues/23075)
- [pandas issue #10155 — Mean overflows for integer dtypes](https://github.com/pandas-dev/pandas/issues/10155)
- [CERN slides — vectorisation with expression templates](https://indico.cern.ch/event/450743/contributions/1116439/attachments/1224058/1790961/out-iCSC2016-L2.pdf)

### Detailed research files (consult directly)
- `.planning/research/STACK.md` — full per-library verdicts, version compatibility matrix
- `.planning/research/FEATURES.md` — feature landscape, MVP definition, prioritization, naming, comparison reference table
- `.planning/research/ARCHITECTURE.md` — pattern catalog, CMake integration, data flow, anti-patterns
- `.planning/research/PITFALLS.md` — all 17 pitfalls + 18-item "looks done but isn't" checklist + pitfall-to-phase mapping

---
*Research completed: 2026-05-05*
*Ready for roadmap: yes*
