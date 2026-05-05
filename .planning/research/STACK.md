# Stack Research — Lazy Tensor Framework (v0.8.0)

**Domain:** Header-only lazy-evaluating tensor library (Expression Templates with CRTP) inside an existing C++20 codebase
**Researched:** 2026-05-05
**Confidence:** HIGH

## Verdict (TL;DR for the roadmapper)

**Use NONE — pure C++20 standard library + the dependencies Quiver already pulls.**
v0.8.0 ships as a new header-only `include/quiver/tensor/` namespace built with `<concepts>`, `<type_traits>`, `<array>`, `<vector>`, `<numeric>`, and `<algorithm>`. No new FetchContent declarations are required for the core. The only dependency added to `cmake/Dependencies.cmake` is **GoogleTest** (already present, gated by `QUIVER_BUILD_TESTS`) — there is no benchmark, SIMD, or mdspan library to add for this milestone.

Phase 1 of the roadmap should be "scaffold tensor headers using only stdlib," not "add Highway dependency, then scaffold."

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **C++20 standard library** | (toolchain-bundled) | Primary "tensor framework toolkit" | Quiver already builds in C++20 (`set(CMAKE_CXX_STANDARD 20)` in top-level `CMakeLists.txt`). `<concepts>`, `<type_traits>`, `<array>`, `<numeric>`, `<algorithm>`, `<cstdint>` are all that's needed for CRTP expression templates, shape broadcasting, and reductions. No new dependency. |
| **CRTP idiom** | Language pattern | Static polymorphism for `Expression<Derived>` base | Industry pattern used by Eigen, Blaze, xtensor. Compile-time dispatch, zero v-table overhead, perfect fit for "header-only, deterministic compile-time dispatch" constraint stated in `.planning/PROJECT.md` line 83. |
| **`std::span` (C++20)** | (toolchain-bundled) | Non-owning views over flat data buffers | Already in C++20. Lets reductions and broadcasts operate on subranges without allocating. Good replacement for `mdspan` since Quiver's tensor only needs strided 1-D viewing — full `mdspan` is overkill. |

### Supporting Libraries (already vendored — reused, not re-added)

| Library | Version (in tree) | Purpose | When to Use |
|---------|-------------------|---------|-------------|
| GoogleTest | 1.17.0 | Tensor unit tests in `tests/test_tensor_*.cpp` | Mirror existing per-concern test split. Already present via `cmake/Dependencies.cmake` lines 56-64, gated by `QUIVER_BUILD_TESTS`. |
| spdlog | 1.17.0 | Optional debug tracing for shape-broadcast errors | Only in implementation files if needed; do **not** put `spdlog::*` calls in headers (it's PRIVATE-linked to `quiver` target — header inclusion would leak the dep). |

### Development Tools (already in use — no change)

| Tool | Purpose | Notes |
|------|---------|-------|
| clang-format 21 | Code formatting | LLVM base, 4-space, 120 cols, C++20. Same `.clang-format` covers tensor headers. |
| clang-tidy | Static analysis | Same `.clang-tidy`. Heavy template code may produce noisy diagnostics — consider per-file `// NOLINTBEGIN(misc-no-recursion)` if reductions trigger false positives. |
| cppcheck (pre-commit) | Pre-commit hook | Already wired. Heavy templates can confuse cppcheck — if it flags valid expression-template code, narrow the suppression locally rather than globally. |
| CMake FetchContent | Dependency vendoring | **No new entries** for v0.8.0 core. |

## Installation

```cmake
# cmake/Dependencies.cmake
# NO CHANGES for tensor v0.8.0. The tensor framework is implemented entirely
# in headers using C++20 stdlib facilities. GoogleTest is already declared
# above (gated by QUIVER_BUILD_TESTS) and covers test executables.
```

```cmake
# include/CMakeLists.txt or src/CMakeLists.txt — same file that defines
# the existing `quiver` INTERFACE/STATIC/SHARED target.
target_sources(quiver PUBLIC
    FILE_SET tensor_headers TYPE HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/include
    FILES
        include/quiver/tensor/expression.h
        include/quiver/tensor/tensor.h
        include/quiver/tensor/shape.h
        include/quiver/tensor/broadcast.h
        include/quiver/tensor/reductions.h
        # ...
)
# No new target_link_libraries() entries needed.
```

## Per-Library Verdict (the question's library survey)

| Library | Latest Stable | License | Verdict | Reasoning Tied to a Quiver Constraint |
|---------|---------------|---------|---------|---------------------------------------|
| **Eigen** | 5.0.1 (2025-11-08) | MPL 2.0 | **AVOID** for the tensor framework; **STUDY** as a reference | (a) Eigen 5.0 still requires only **C++14** — but its strength is *linear algebra* (matmul, decompositions, sparse) which `.planning/PROJECT.md` line 65 explicitly puts out of scope. (b) Adding Eigen as a transitive include — even header-only — bloats compile times of every TU that touches tensors; Quiver already has long compile times from sol2/Lua. (c) Eigen's API is matrix-algebra-shaped (`Matrix`, `Array`, `Map`), not the element-wise dataflow tensor we're building. The lazy-eval mechanics (`Eigen/src/Core/util/XprHelper.h`) are excellent reading, but consuming Eigen would force users (and bindings, eventually) to learn Eigen's vocabulary. (d) Pulling Eigen for "just the vector type" is anti-pattern (matches the question's "do not pull Eigen just for vector type" hint). |
| **xtensor** | 0.27.1 (requires C++20) | BSD-3-Clause | **AVOID** | (a) **Hard transitive dependency on xtl 0.8+** — confirmed via xtensor README. Adding xtensor means adding xtl, which means two new FetchContent declarations and two new license obligations for what Quiver wants to be a thin in-house layer. (b) xtensor 0.27.x changed its C++ requirement to C++20 — fine for Quiver, but the API was reshuffled in the 0.26→0.27 transition (still settling). (c) xtensor already implements *most* of what v0.8.0 wants, so consuming it would mean v0.8.0 becomes a wrapper around xtensor — that contradicts `.planning/PROJECT.md` line 96 ("Expression Templates with CRTP") which is a *learning/control* milestone, not a library-integration milestone. (d) Eventually consumed by 5+ language bindings; depending on xtensor pushes its API surface (broadcasting rules, dtype handling) into binding error messages, breaking the homogeneity principle from `CLAUDE.md`. |
| **Blaze** | 3.8 (2020-08-15) — **stale** | BSD-3-Clause | **AVOID** | (a) Last release nearly 6 years old per Bitbucket. Not actively maintained at the cadence Quiver expects from its deps (sqlite 2025-06, Highway 2026-04, Eigen 2025-11). Pinning to a stale dep injects risk for *every* downstream toolchain bump. (b) Blaze is matrix-algebra-focused (BLAS replacement) — same scope mismatch as Eigen. (c) CMake integration story is rough (Bitbucket-hosted, transitive-find quirks per the search results) — does not match Quiver's `FetchContent_Declare(GIT_REPOSITORY ...)` plug-and-play model. |
| **Boost.MultiArray** | tracks Boost (1.86 etc.) | BSL-1.0 | **AVOID** | (a) Pulling Boost into Quiver means a multi-hundred-MB FetchContent download for a single header. Quiver has been Boost-free since inception — adding it for tensors would dwarf the entire rest of the dependency graph. (b) `boost::multi_array` has *no expression templates / no lazy eval* — it's a storage container, not a compute framework. Doesn't match the milestone goal. |
| **Boost.uBLAS** | tracks Boost | BSL-1.0 | **AVOID** | (a) Same Boost-bloat objection. (b) uBLAS is widely considered legacy by the C++ numerical community (Eigen and Blaze surpass it on every benchmark and feature axis). (c) Linear-algebra-shaped, scope mismatch. |
| **C++23 `<mdspan>`** | (Kokkos reference: mdspan-0.6.0) | language feature / Apache-2.0 | **AVOID for v0.8.0** — revisit in v0.10.0+ | (a) `<mdspan>` is a C++23 feature. Quiver's CI matrix is GCC + Clang (Apple Clang on macOS) + MSVC; **MSVC `<mdspan>` is still in-progress** in Visual Studio 2026 14.51 builds (per Microsoft C++ blog). Apple Clang's libc++ historically lags on C++23. Bumping the project to C++23 would fail the multi-platform CI gate. (b) Kokkos' `mdspan-0.6.0` reference impl could be FetchContented as a backport — but that's adding a dep where Quiver's needs (1-D flat buffer + computed strides) are 30 lines of code. (c) `mdspan` adds value for *generic* multi-dimensional indexing, but Quiver's tensor is dense + dimension-ordered + already has shape metadata — no win. **Defer** until C++23 is the project baseline. |
| **C++20 `<ranges>`** | (toolchain-bundled) | language feature | **USE — with caution on macOS** | (a) Apple Clang's libc++ has historically had patchy `<ranges>` (search results show "ranges header does not exist" issues persisting into recent Xcode). MSVC and libstdc++ are full. (b) **Therefore: use `<ranges>` only in `.cpp` (not header) code if at all, and only after a CI check on `macos-latest` confirms availability.** Safer default for v0.8.0 headers: plain iterator + index loops, since the broadcast/reduction code is already algorithm-simple. |
| **C++20 `<concepts>`** | (toolchain-bundled) | language feature | **USE** | Universally supported across MSVC, GCC, Apple Clang at C++20. Use to constrain `Expression<Derived>` requirements (e.g., `requires { typename T::value_type; { e.shape() } -> std::convertible_to<Shape>; }`). Replaces older SFINAE patterns and gives much better error messages. |
| **C++17 `<execution>` parallel policies** | (toolchain-bundled) | language feature | **AVOID for v0.8.0** | (a) Apple Clang libc++ does **not** ship `std::execution::par` — confirmed via libcxx docs and JUCE forum thread. Using `std::reduce(std::execution::par, ...)` would either require a third-party shim (poolSTL, pstld) or break macOS CI. (b) v0.8.0 scope is correctness + lazy fusion; threading reductions is a v1.x performance concern. Defer. |
| **C++26 `std::execution` (sender/receiver)** | preview only | language feature | **AVOID** | Not a stable target across MSVC/GCC/Clang for production use in 2026. Out of scope. |
| **Google Highway** | 1.4.0 (2026-04-23) | Apache-2.0 | **AVOID for v0.8.0**; **CONSIDER for v1.x** SIMD milestone | (a) Highway is the right *future* answer for portable SIMD (NumPy adopted it via NEP-54 for the same reason). It's CMake-native, FetchContent-friendly, actively maintained. (b) **But v0.8.0 ships scalar correctness first.** Adding Highway now means tying the lazy core's design to Highway's batch types from day one — a premature coupling. The CRTP expression layer is supposed to be *evaluator-agnostic* so an `eval_into(buffer)` pass can later dispatch to Highway, OpenMP, or stdlib without redesign. (c) Highway adds non-trivial compile time and a SHARED-library export — it would need PRIVATE linkage in `src/CMakeLists.txt` (not `quiver_compiler_options` INTERFACE), and currently there are no compiled translation units in `src/tensor/` planned. |
| **xsimd** | 14.2.0 | BSD-3-Clause | **AVOID** | Same reasoning as Highway, plus: xsimd is the natural pair to xtensor — adopting it implicitly suggests xtensor adoption. Highway has stronger backing (NumPy, Google) and is the better future SIMD bet. |
| **Google Benchmark** | 1.9.5 (2026-01-21) | Apache-2.0 | **AVOID for v0.8.0**; **CONSIDER for v0.8.1+** if perf benchmarks become a phase | (a) Quiver already has `tests/benchmark/benchmark.cpp` as a hand-rolled timing harness — it's not a recurring concern. Adding Google Benchmark to formalize that is a cross-cutting decision that should not piggyback on the tensor milestone. (b) v0.8.0 acceptance is *correctness* (broadcast rules, reduction values) not throughput. GoogleTest assertions cover that. (c) If a future tensor phase needs micro-benchmarks (e.g., comparing fused vs. non-fused expression evaluation), reopen this question. |

## Why "Pure C++20 stdlib" Is the Right Answer Here

Reading `.planning/PROJECT.md` more carefully, three constraints converge on "no new dep":

1. **Header-only requirement** (line 83). New deps that are *also* header-only (Eigen, xtensor, Blaze) still impose include-time cost on every TU and a license-tracking burden. The lazy CRTP scaffolding for elementwise ops + broadcast + reduce is *less code* than the integration glue for a third-party library.

2. **Eventual binding consumption** (line 23-26 — "one C++ codebase, identical behavior across every language binding"). Consuming a third-party tensor library means binding error messages would surface that library's vocabulary (Eigen's `Eigen::CommaInitializer` errors, xtensor's `broadcast_error`). The `CLAUDE.md` "three error-message patterns" rule cannot be enforced if errors originate in a third-party library — they would all need to be wrapped, defeating the dependency's value.

3. **Out-of-scope linear algebra** (line 65). Eigen, Blaze, uBLAS, xtensor-blas all carry weight (compile time, binary size, license obligations) that pays off only when matmul/decompositions/sparse are wanted. Quiver is *not* taking those on. Carrying that weight for the slim subset Quiver wants is a bad trade.

The CRTP + element-wise + broadcast + reduce surface is genuinely small. Reference: a single-file CRTP tensor library in 600-1000 lines is well-documented in the C++ literature (see "Sources" — fluentcpp.com, modernescpp.com). With `<concepts>` (C++20) the implementation is materially cleaner than older SFINAE-heavy CRTP.

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Pure C++20 stdlib | Eigen 5.0.1 | If Quiver's roadmap pivots to *include* dense linear algebra (matmul, QR, SVD) — at that point Eigen is the canonical answer and the cost-benefit flips. |
| Pure C++20 stdlib | xtensor 0.27.1 + xtl 0.8.2 | If the goal becomes *full NumPy-API parity* (advanced indexing, einsum, broadcasting fancy indexing). Quiver explicitly excludes this in PROJECT.md line 65. |
| `std::array<size_t, N>` for shape | `std::vector<size_t>` for shape | If tensor rank must be runtime-dynamic (rare for the energy-modeling domain — most arrays have known rank at compile time). Start with `std::array<size_t, MAX_RANK>` + an active-rank field; revisit only if domain users push back. |
| `std::span` (C++20) | Kokkos `mdspan-0.6.0` | If Quiver later needs strided multi-dimensional views with arbitrary stride patterns (e.g., views into the middle of a 4-D tensor). Defer to a later milestone — flat buffers + manual stride math suffice for v0.8.0. |
| Sequential reductions | `std::execution::par` reductions | When threading wins justify the macOS/libc++ shim cost. Not v0.8.0. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `std::valarray` | Pre-C++11 vector type with weird semantics; no lazy eval, no broadcasting, no shape; not used by any modern numerical C++ codebase. | `std::vector<T>` or `std::array<T, N>` for storage; CRTP expressions for lazy compute. |
| Eigen pulled "just for the vector type" | Adds 50k+ LOC of headers and an LGPL-cleared MPL license obligation for a 30-line `Vec3` use case. | Quiver's own `Tensor<T>` is more idiomatic to its own codebase. |
| Boost (any) | Quiver is Boost-free. Adding Boost for tensors would dwarf the entire rest of the dep graph and force every binding's binary builder to deal with Boost. | C++20 stdlib facilities are sufficient. |
| Highway / xsimd in v0.8.0 | Couples the lazy expression layer to a specific SIMD vocabulary before the lazy layer's own design has stabilized. Defer until SIMD is the actual goal of a milestone. | Sequential evaluators in v0.8.0; revisit Highway in a future SIMD milestone. |
| C++23 `<mdspan>` | Forces a project-wide C++ standard bump to C++23, which currently fails MSVC 14.51 (Visual Studio 2026) for full mdspan and Apple Clang libc++ for several C++23 facilities. | Plain `std::span<T>` over flat buffers + manually computed strides until the project bumps to C++23. |
| Google Benchmark | Cross-cutting tooling decision that shouldn't ride on the tensor milestone. v0.8.0 is correctness-shaped, not throughput-shaped. | GoogleTest assertions for correctness; existing hand-rolled `tests/benchmark/benchmark.cpp` pattern if timing checks are needed. |
| `std::execution::par` reductions | Apple Clang libc++ doesn't ship parallel execution policies; would either break macOS CI or pull in poolSTL/pstld. | Sequential reductions for v0.8.0; threading is a v1.x performance milestone. |

## Stack Patterns by Variant

**If the tensor framework grows into linear algebra (matmul, decomp) — out-of-scope per PROJECT.md but possible in v1.x:**
- Use Eigen 5.x as the engine, expose Quiver-shaped wrappers in `include/quiver/tensor/linalg.h`.
- Eigen would link PRIVATE to a (new) compiled `quiver_tensor` translation unit, *not* the public headers — keeps Eigen's macros and templates from leaking through `quiver.h`.

**If a future SIMD milestone is opened:**
- Add Google Highway 1.4.0+ via FetchContent.
- Highway is a SHARED library; declare with `set(HWY_ENABLE_TESTS OFF)`, `set(HWY_ENABLE_EXAMPLES OFF)`, `set(HWY_ENABLE_INSTALL OFF)`, link PRIVATE to `quiver` (its headers are not part of the C++ public API).
- The lazy CRTP expression evaluator gets a new path: `eval_into_simd(buffer)` that the Tensor's `operator=` can dispatch to when both sides have compatible alignment.

**If multi-rank tensors with full strided views become needed:**
- Adopt Kokkos `mdspan-0.6.0` as a backport (rather than waiting for full C++23 toolchain support).
- Or wait until MSVC 14.52+ ships full `<mdspan>` and bump the project's C++ standard.

## Version Compatibility

| Item | Compatible With | Notes |
|------|-----------------|-------|
| Quiver C++20 baseline | MSVC 17 2022, GCC 12+, Apple Clang 14+ | Already validated by CI. `<concepts>`, `<span>`, designated initializers all work. |
| `<ranges>` | MSVC ✓, libstdc++ ✓, libc++ partial on older Apple Clang | Use defensively; verify on `macos-latest` CI before relying on `std::ranges::*` in headers. |
| `<execution>` parallel policies | MSVC ✓, libstdc++ ✓ (with `-ltbb`), libc++ ✗ | Reason to defer parallel reductions. |
| `<mdspan>` (C++23) | GCC 14+ ✓, Clang 18+ ✓, MSVC 14.51 partial, Apple Clang ✗ | Reason to defer. |
| GoogleTest 1.17.0 (existing) | All three platforms in CI | Already in use; covers tensor unit tests with no change. |

## Where the Tensor Code Lives (file-layout hint for the roadmapper)

Mirroring Quiver's existing `include/quiver/binary/` + `src/binary/` parallel-subsystem pattern, the tensor framework should be structured as:

```
include/quiver/tensor/        # Public C++ headers (header-only)
  expression.h                # Expression<Derived> CRTP base + concepts
  tensor.h                    # Concrete Tensor<T, Rank> storage + assignment
  shape.h                     # Shape<N> compile-time + runtime shape utilities
  broadcast.h                 # NumPy-style broadcast rule + broadcasted iterators
  ops.h                       # Element-wise +, -, *, / ; unary -, abs, exp, log, sqrt
  reductions.h                # sum, mean, max with optional axis
  tensor.h                    # Aggregate include
src/tensor/                   # (Likely empty for v0.8.0 — header-only.)
                              # Add a single .cpp only if non-template helpers
                              # need a translation unit (e.g., error message construction).
tests/test_tensor_*.cpp       # GoogleTest cases mirroring per-concern split:
                              #   test_tensor_expression.cpp
                              #   test_tensor_broadcast.cpp
                              #   test_tensor_ops.cpp
                              #   test_tensor_reductions.cpp
```

No new directory under `bindings/` for v0.8.0 — bindings are explicitly out of scope per PROJECT.md line 60-62.

## Sources

**Library version verification (2026-05-05):**
- [Eigen 5.0.1 release on GitLab](https://gitlab.com/libeigen/eigen/-/tags/5.0.1) — verified 2025-11-08 release; Eigen 5.0 requires C++14 minimum, master will require C++17. **HIGH** confidence (raw GitLab API).
- [xtensor 0.27.1 on GitHub](https://github.com/xtensor-stack/xtensor/releases) — verified via GitHub API; xtensor 0.27.x requires C++20. **HIGH** confidence.
- [xtensor README — header-only nature and xtl dependency](https://github.com/xtensor-stack/xtensor/blob/master/README.md) — confirms hard transitive dependency on xtl 0.8+. **HIGH** confidence.
- [xtl 0.8.2 on GitHub](https://github.com/xtensor-stack/xtl) — xtensor's mandatory dependency. **HIGH** confidence.
- [Google Highway 1.4.0 release](https://github.com/google/highway/releases/tag/1.4.0) — 2026-04-23. **HIGH** confidence.
- [xsimd 14.2.0 on GitHub](https://github.com/xtensor-stack/xsimd) — latest tag; xsimd is the SIMD backend for xtensor. **HIGH** confidence.
- [Google Benchmark v1.9.5 release](https://github.com/google/benchmark/releases/tag/v1.9.5) — 2026-01-21. **HIGH** confidence.
- [Kokkos mdspan-0.6.0](https://github.com/kokkos/mdspan) — reference implementation of C++23 mdspan, suitable for FetchContent backport. **HIGH** confidence.
- [Blaze C++ math library on GitHub mirror](https://github.com/parsa/blaze) and [Bitbucket origin](https://bitbucket.org/blaze-lib/blaze/) — Blaze 3.8 (2020-08-15) is the latest published release; library has not had a tagged release since. **MEDIUM** confidence (no further activity confirmed).

**Compiler support matrix:**
- [cppreference C++ compiler support](https://en.cppreference.com/w/cpp/compiler_support.html) — C++20 baseline coverage. **HIGH** confidence.
- [cppreference C++23 compiler support](https://en.cppreference.com/cpp/compiler_support/23) — `<mdspan>` partial across toolchains. **HIGH** confidence.
- [MSVC C++23 status (Microsoft C++ Team Blog)](https://devblogs.microsoft.com/cppblog/c23-support-in-msvc-build-tools-14-51/) — confirms in-progress mdspan support in VS 2026 14.51/14.52. **HIGH** confidence.
- [libc++ C++20 status](https://libcxx.llvm.org/Status/Cxx20.html) — Apple Clang libc++ details. **HIGH** confidence.
- [JUCE forum: parallel execution on iOS/macOS](https://forum.juce.com/t/c-support-for-execution-policies-on-ios/61261) — confirms libc++ does not ship `std::execution`. **MEDIUM** confidence (multi-source agreement).

**Pattern references (CRTP + Expression Templates):**
- [fluentcpp.com — Curiously Recurring Template Pattern](https://www.fluentcpp.com/2017/05/12/curiously-recurring-template-pattern/) — canonical introduction. **HIGH** confidence.
- [modernescpp.com — C++ is lazy: CRTP](https://www.modernescpp.com/index.php/c-is-still-lazy/) — expression templates with CRTP walk-through. **HIGH** confidence.
- [cppreference CRTP](https://en.cppreference.com/cpp/language/crtp) — language reference. **HIGH** confidence.

**Quiver codebase facts (verified by direct file read):**
- `C:\Development\DatabaseTmp\quiver\CMakeLists.txt` — C++20 standard, CMake 3.26+ minimum.
- `C:\Development\DatabaseTmp\quiver\cmake\Dependencies.cmake` — current FetchContent declarations and versioning style.
- `C:\Development\DatabaseTmp\quiver\cmake\CompilerOptions.cmake` — MSVC `/W4 /permissive- /Zc:__cplusplus`, GCC/Clang `-Wall -Wextra -Wpedantic`.
- `C:\Development\DatabaseTmp\quiver\.planning\PROJECT.md` lines 50-66, 83, 96 — milestone scope and constraints.
- `C:\Development\DatabaseTmp\quiver\.planning\codebase\STACK.md` — existing dependency inventory.
- `C:\Development\DatabaseTmp\quiver\CLAUDE.md` — naming conventions, error-message patterns, Pimpl-vs-value guidance, header-only mandate for tensor framework.

---
*Stack research for: lazy CRTP-based tensor framework added to C++20 SQLite wrapper library (Quiver v0.8.0)*
*Researched: 2026-05-05*
