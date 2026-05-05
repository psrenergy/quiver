# Phase 1: Storage and CRTP Scaffold - Research

**Researched:** 2026-05-05
**Domain:** Header-only C++20 lazy-tensor scaffolding (Tensor<T> storage value type, Expression<Derived> CRTP base, IsTensorExpr concept, header/test/CMake wiring under `include/quiver/tensor/`)
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

The following decisions were ratified in `.planning/phases/01-storage-and-crtp-scaffold/01-CONTEXT.md` (2026-05-05). Plans MUST honor them; planner may not relitigate.

**Tensor<T> Constructor Surface**
- **D-01:** Three constructors only in Phase 1: `Tensor(shape_t)`, `Tensor(shape_t, T fill)`, `Tensor(shape_t, std::initializer_list<T>)`. Iterator-range and `std::vector<T>&&` ctors are deferred to v0.9.0.
- **D-02:** `Tensor(shape_t)` (no fill) zero-initializes via `data_.assign(total_size(shape), T{})`. NumPy-compatible.
- **D-03:** No factory functions (`zeros`, `ones`, `full`, `arange`, `zeros_like`) in Phase 1 — defer to v0.9.0.
- **D-04:** Constructors accept 0-sized dimensions (`Tensor<double>({2, 0, 3})` valid; `total_size = 0`, `data_` empty).
- **D-05:** Initializer-list size mismatch throws `Cannot construct tensor: initializer_list size {got} does not match shape size {expected}` (Pattern 1).

**Indexing & Element Access**
- **D-06:** `operator()(Idx... idxs)` is unchecked — no rank/type/bounds validation. Hot path stays clean.
- **D-07:** `at(Idx... idxs)` is the bounds-and-rank-checked variant. Throws `Cannot access tensor: rank mismatch (expected {rank} indices, got {n})` and `Cannot access tensor: index {i,j,k} out of bounds for shape {a,b,c}` (Pattern 1).
- **D-08:** Indices are `std::size_t` — no negative indexing in Phase 1. Defer to v0.9.0 alongside RED-04.
- **D-09:** `begin()`/`end()` forward to `data_.begin()`/`data_.end()`, returning `std::vector<T>::iterator` / `const_iterator`. Range-for and `std::ranges` compatible.
- **D-10:** Mutable `operator()` returns `T&`; const returns `const T&` (or `T` by value — implementer's choice). `data()` is `const T*` only (mutable `data() -> T*` deferred via REQ-POL-02).

**0-D Tensor Representation**
- **D-11:** 0-D tensors use empty shape `shape_t{}` with `total_size = 1`. NumPy/xtensor convention. `rank() == 0`, `size() == 1`, `data_` has exactly one element.
- **D-12:** `.item()` ships in Phase 1. Returns `T` (the single element). Throws `Cannot item: tensor has {N} elements (expected 1)` if `size() != 1` (Pattern 1).
- **D-13:** No `Tensor<T>::scalar(value)` factory. Users construct 0-D tensors via `Tensor<double>({}, 42.0)` or `Tensor<double>({}, {42.0})`.
- **D-14:** `IsTensorExpr<T>` is the strict CRTP-derivation concept implemented via a template detection idiom. Duck-typed alternatives rejected (Anti-Pattern #3 ADL collision risk with `std::complex`).

**BLD-04 Include Containment Enforcement**
- **D-15:** Single mechanism: a CTest target running a CMake script (`cmake/CheckTensorIncludeContainment.cmake`). No pre-commit hook duplicate, no compile-time test file.
- **D-16:** Grep regex `#\s*include\s*[<"]quiver/tensor/` (matches both `<>` and `""` forms). Symbol-reference grep rejected (false-positive risk). AST-based clang tooling rejected (heavyweight).
- **D-17:** Scope `include/quiver/` recursive, **excluding** `include/quiver/tensor/`. Covers `include/quiver/*.h`, `include/quiver/binary/*.h`, `include/quiver/c/*.h`, `include/quiver/c/binary/*.h`.
- **D-18:** Wired via `add_test(NAME tensor_no_leakage COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CheckTensorIncludeContainment.cmake)` in `tests/CMakeLists.txt`. On match: prints offending file:line pairs and `message(FATAL_ERROR ...)`.

### Claude's Discretion

The following implementation choices were explicitly delegated to the planner/executor within the locked decisions above:

- Exact template-detection idiom for `IsTensorExpr` (`is_base_of_template_v` helper vs. `std::derived_from<T, Expression<U>>` via probe). Both are equivalent to users; pick one and justify. **Resolved in this research → see "Concept Idiom" section.**
- Header-file split granularity inside `include/quiver/tensor/`. Research recommends `expression.h`, `shape.h`, `tensor.h` for Phase 1; planner can split or merge if friction emerges. **Resolved in this research → see "Header Split" section.**
- Whether `compute_strides()`, `total_size()`, `ravel_index()` live in `shape.h` (recommended) or `detail::` namespace. **Resolved → public free functions in `shape.h`; reasoning in "Header Split".**
- Whether `eval(linear_idx)` on `Tensor<T>` returns `T` by value or `const T&`. **Resolved → return by value; reasoning in "Concept Idiom" section.**
- Test-file naming within `tests/test_tensor_*.cpp`. Recommend `test_tensor_storage.cpp` initially; sub-split if it grows past ~500 lines.
- Concrete error-message wording within the three-pattern format (the patterns and key phrases are locked; specific punctuation/word-choice unconstrained).

### Deferred Ideas (OUT OF SCOPE)

The following surfaced in discussion but belong in later milestones, not Phase 1:

- Iterator-range and `std::vector<T>&&` take-ownership constructors → v0.9.0
- Factory functions (`zeros`, `ones`, `full`, `arange`, `linspace`, `zeros_like`) → v0.9.0
- Negative indexing for element access → v0.9.0 (consistent with RED-04 negative-axis defer)
- Mutable `data() -> T*` accessor → v0.9.0 (REQ-POL-02)
- `Tensor<T>::scalar(value)` named factory → rejected as redundant; revisit only on user pain
- Symbol-reference or AST-based BLD-04 check → defer to a future hardening milestone
- Pre-commit hook duplicate of BLD-04 check → defer; CTest-only is sufficient for v0.8.0

Out-of-scope-for-the-milestone items (autograd, GPU, sparse, BLAS, Element/Database bridge, Tensor C API, language bindings) documented in `PROJECT.md` and `REQUIREMENTS.md` are not re-listed here. Plans MUST respect those exclusions.

</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| **STG-01** | User can construct `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}` with shape and optional fill | Constructor surface defined by D-01..D-05; element types fixed in `tensor.h` via explicit instantiation comment / static_assert (see "Code Examples" pattern). |
| **STG-02** | User can query `shape()`, `size()`, `rank()` | Cheap const accessors on `Tensor<T>` and on `Expression<Derived>` base via CRTP (Pattern 1, ARCHITECTURE.md). |
| **STG-03** | User can index a tensor element by multi-dim coordinates `tensor(i, j, k)` | `operator()(Idx...)` unchecked (D-06), `at(Idx...)` checked (D-07); both compute flat index via stride dot-product (Pattern 5, ARCHITECTURE.md). |
| **STG-04** | User can read raw contiguous data via `data() -> const T*` | Simple `data_.data()` forwarding from `std::vector<T>`; const-only per D-10. Future bridge (v0.9.0) needs this accessor stable (ARCHITECTURE.md "Future Integration"). |
| **STG-05** | User can iterate via `begin()` / `end()` (range-for compatible) | D-09 forwards to `data_.begin/end`; range-for + std::ranges compatible without abstraction. |
| **EXP-01** | Lazy expression types derive from `Expression<Derived>` providing `shape()`, `size()`, `eval(linear_idx)` via static dispatch | CRTP base with `self()` cast (Pattern 1). No virtual functions. `Tensor<T>` is the first concrete subclass; future `BinaryOp`/`UnaryOp`/`BroadcastView`/`ReduceExpression` plug in identically. |
| **EXP-02** | Generic code can constrain "is a tensor expression" via `IsTensorExpr` concept | Strict CRTP-derivation concept (D-14) implemented via `is_base_of_template_v<Expression, T>` helper. Used by Phase 2 operator overloads to give readable diagnostics (Pitfall #7). |
| **BLD-01** | Tensor framework lives under `include/quiver/tensor/` mirroring `include/quiver/binary/` | Header-only; mirror `binary/` layout depth and namespace nesting. No `src/tensor/` translation unit unless non-template helpers accumulate. |
| **BLD-02** | Per-concern GoogleTest files under `tests/test_tensor_*.cpp` matching existing `test_database_*.cpp`/`test_binary_*.cpp` conventions | Phase 1 ships `tests/test_tensor_storage.cpp`. Later phases add `test_tensor_arithmetic.cpp`, `test_tensor_aliasing.cpp`, etc. |
| **BLD-03** | Tensor sources/tests integrate via `tests/CMakeLists.txt` only — no edits to `src/CMakeLists.txt` (header-only) | Existing `target_include_directories(quiver PUBLIC ...)` and `install(DIRECTORY include/quiver ...)` rules already cover new headers. `quiver_compiler_options` applies as-is. |
| **BLD-04** | A CI test enforces include containment — no public Quiver header outside `include/quiver/tensor/` transitively pulls in tensor headers | Single CMake-script CTest target (D-15..D-18). Greps `#include` lines per D-16 regex. Excludes `include/quiver/tensor/` per D-17. Wired by `add_test` per D-18. |

</phase_requirements>

---

## Project Constraints (from CLAUDE.md)

The planner must verify all plans honor these directives:

| # | Directive | Source | Phase 1 implication |
|---|-----------|--------|---------------------|
| C-01 | All public C++ methods bound through to C API → Julia/Dart/Lua/Python | CLAUDE.md "Principles" | **Phase 1: NOT applicable.** Tensor is a C++-only milestone; explicitly deferred to v0.9.0+. Plans MUST NOT add C API surface. |
| C-02 | Pimpl only when hiding private dependencies; plain value types use Rule of Zero | CLAUDE.md "Principles", "C++ Patterns" | `Tensor<T>` has no private dependencies (only `<vector>`, `<cstddef>`, `<utility>`). **Plain value type, Rule of Zero, no Pimpl.** Mirror `Dimension`/`TimeProperties` precedent. |
| C-03 | Three error-message patterns: `Cannot {op}: {reason}` / `{Entity} not found: {id}` / `Failed to {op}: {reason}` | CLAUDE.md "Error Handling" | Phase 1 throws are limited to Pattern 1 (initializer-list mismatch, rank mismatch, bounds error, item-on-non-scalar). No Pattern 2 or 3 expected. |
| C-04 | Move semantics: delete copy where appropriate; default move; move ops MUST be `noexcept` | CLAUDE.md "Move Semantics" | `Tensor<T>` is COPYABLE (it owns a `std::vector<T>` — a regular value type, not a resource handle like `Database`). Default copy AND move; both move ops `noexcept` (Anti-Pattern #6 prevention). |
| C-05 | C++20 target — use modern features that simplify logic | CLAUDE.md "Principles" | Use `<concepts>` for `IsTensorExpr`. C++20 is already the project standard; verified in top-level `CMakeLists.txt`. |
| C-06 | Default to no comments; codebase optimized for human readability, not machine parsing | CLAUDE.md "Comments" | No `@file` headers, no Doxygen, no JSDoc. Inline comments only for non-obvious mechanics. |
| C-07 | Logic in C++; bindings stay thin | CLAUDE.md "Principles" | Phase 1: no bindings work, period. (Cross-checked against Pitfall #13 cross-phase discipline.) |
| C-08 | Self-updating CLAUDE.md | CLAUDE.md "Principles" | After Phase 1 ships, CLAUDE.md must gain a `tensor/` entry under "Architecture" (parallel to `binary/`). The planner SHOULD include this update as a final task. |

---

## Summary

Phase 1 builds the foundational scaffolding for Quiver's lazy CRTP tensor framework: an owning `Tensor<T>` value type, an `Expression<Derived>` CRTP base, an `IsTensorExpr` concept, and the header/test/CMake plumbing that anchors all four phases of milestone v0.8.0. The work is deliberately narrow — no operators, no broadcasting, no reductions — but the decisions made here set the floor for everything that follows.

Three properties dominate the phase. **First:** the framework is header-only, lives under `include/quiver/tensor/` mirroring the existing `include/quiver/binary/` layout, and adds zero `.cpp` files in `src/` (BLD-03). **Second:** `Tensor<T>` is a plain value type with Rule of Zero — no Pimpl, defaulted copy/move, `noexcept` on both move operations. It mirrors `Dimension`/`TimeProperties` precedent rather than `Database`/`BinaryFile` (Pimpl-anchored, hide private deps). **Third:** every public symbol lives under `quiver::tensor::*` (`quiver::tensor::detail::*` for internals); no leakage into `quiver::*` root namespace, no umbrella-header inclusion (`include/quiver/quiver.h`), no transitive include from any non-tensor public header.

The phase prevents four upstream pitfalls catalogued in `.planning/research/PITFALLS.md`. **Pitfall #4 (header-only compile-time blowup):** the BLD-04 CTest target enforces strict include containment via a CMake-script grep, so a future contributor adding `#include <quiver/tensor/tensor.h>` to `database.h` is caught at CI time, not three months later when the library compile time has tripled. **Pitfall #7 (template error messages that bury the bug):** the `IsTensorExpr` concept lands in Phase 1 — not as Phase 2 polish — so every operator overload introduced in Phase 2 already has a named, grep-friendly diagnostic. **Pitfall #13 (bridge designed too early):** zero diff to `include/quiver/c/`, `include/quiver/c/binary/`, or `bindings/` for the Phase 1 PR; verified by `git diff --stat` (empty rows on those paths) before any plan is marked complete. **Pitfall #14 (namespace hygiene):** `quiver::tensor::*`, never `quiver::*`, locks at framework birth — no late-renaming churn.

**Primary recommendation:** Ship six new files (three headers under `include/quiver/tensor/`, one test file under `tests/`, one CMake script under `cmake/`, and amendments to `tests/CMakeLists.txt`); zero modifications outside this list; the BLD-04 CTest target is the hardest external interface in the phase and the planner should scrutinize it the most.

---

## Architectural Responsibility Map

Tier classification for a header-only library subsystem differs from a multi-tier application; in this codebase, the relevant tiers are: **C++ Core (public)**, **C++ Core (internal/detail)**, **Test layer**, **Build/CI layer**, and **Documentation layer**.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| `Tensor<T>` storage value type | C++ Core (public, `quiver::tensor`) | — | User-facing leaf type; mirrors `Dimension`/`BinaryMetadata` precedent (header-mostly value type). |
| `Expression<Derived>` CRTP base | C++ Core (public, `quiver::tensor`) | — | All future tensor expression types derive from this; must be visible at every instantiation site (header-only, no Pimpl). |
| `IsTensorExpr` concept | C++ Core (public, `quiver::tensor`) | — | Public for Phase 2 operator overload constraints; readable diagnostics (Pitfall #7). |
| `shape_t` alias + `compute_strides`/`total_size`/`ravel_index` free functions | C++ Core (public, `quiver::tensor`) | — | Free functions per FEATURES.md and ARCHITECTURE.md (Pattern 5); kept public so future `BroadcastView`/`ReduceExpression` headers can call them without a `detail::` qualifier. |
| `is_base_of_template_v` helper trait | C++ Core (internal, `quiver::tensor::detail`) | — | Implementation detail of `IsTensorExpr`; users do not interact directly. Detail namespace prevents leakage. |
| Storage tests (`test_tensor_storage.cpp`) | Test layer | — | Per-concern test file mirroring `test_database_storage`/`test_binary_metadata` precedent. Single file for Phase 1; sub-split if it grows past ~500 lines. |
| BLD-04 CTest target (include-containment grep) | Build/CI layer | — | Single CMake-script test, registered via `add_test`. Runs as part of `ctest`; no compiled binary. |
| `tests/CMakeLists.txt` integration | Build/CI layer | — | Append `test_tensor_storage.cpp` to `quiver_tests` source list; append `add_test(NAME tensor_no_leakage ...)`. |
| CLAUDE.md "Architecture" update (final task) | Documentation layer | — | After Phase 1 lands, add `include/quiver/tensor/` entry parallel to existing `include/quiver/binary/` line. Self-updating discipline (CLAUDE.md C-08). |

**No work in:** `include/quiver/c/*`, `include/quiver/c/binary/*`, `bindings/*`, `src/*` (header-only; BLD-03 hard rule). Pitfall #13 verification step at end of phase: `git diff --stat include/quiver/c/ include/quiver/c/binary/ bindings/` returns zero rows.

---

## Standard Stack

### Core (verified — already in tree)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++20 standard library | toolchain-bundled | `<vector>`, `<cstddef>`, `<utility>`, `<concepts>`, `<type_traits>`, `<initializer_list>`, `<stdexcept>` for `Tensor<T>`, `Expression<Derived>`, `IsTensorExpr` | `[VERIFIED: include/quiver/binary/binary_metadata.h]` Quiver baseline is C++20; verified in top-level `CMakeLists.txt:8-12` (`set(CMAKE_CXX_STANDARD 20)`, `CMAKE_CXX_STANDARD_REQUIRED ON`). |
| GoogleTest | 1.17.0 | Storage tests in `tests/test_tensor_storage.cpp` | `[VERIFIED: cmake/Dependencies.cmake:56-64]` Already declared via FetchContent, gated by `QUIVER_BUILD_TESTS`. No new dep needed. |
| `quiver_compiler_options` INTERFACE target | — | `/W4 /permissive- /Zc:__cplusplus /utf-8` (MSVC), `-Wall -Wextra -Wpedantic` (GCC/Clang) applied to `quiver_tests` | `[VERIFIED: cmake/CompilerOptions.cmake:1-30]` Inherits transitively to test executable; no new flags. |

### Supporting (already in tree, NOT used by Phase 1)

| Library | Version | Purpose | Why NOT used in Phase 1 |
|---------|---------|---------|-------------------------|
| spdlog | 1.17.0 | Logging | `[CITED: STACK.md]` Header-only templates must NOT log inside `eval(i)` hot path (Pitfall #15 risk). Phase 1 has no logging; future phases should log only at `eval()` boundaries if at all. |
| sqlite3 | 3.50.2 | DB engine | Out of scope (Pitfall #13 — no bridge to Database in v0.8.0). |
| sol2/Lua | 3.5.0 / 5.4.8 | Lua scripting | Out of scope. |
| tomlplusplus | 3.4.0 | TOML serialization | Out of scope. |
| rapidcsv | 8.92 | CSV I/O | Out of scope. |
| argparse | 3.2 | CLI args | Out of scope. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff (and why rejected per STACK.md) |
|------------|-----------|------------------------------------------|
| Pure C++20 stdlib | Eigen 5.0.1 | `[CITED: STACK.md]` MPL-2.0 license; matrix-algebra-shaped (`Matrix`, `Array`, `Map`); compile-time blast radius; vocabulary leak through future bindings. **Rejected.** |
| Pure C++20 stdlib | xtensor 0.27.1 + xtl 0.8+ | `[CITED: STACK.md]` Hard transitive dep on xtl; consumes 80% of v0.8.0's intended surface (becomes a wrapper, not a learning milestone). **Rejected.** |
| Pure C++20 stdlib | Blaze 3.8 | `[CITED: STACK.md]` 6 years stale (last release 2020-08-15); BLAS-replacement scope mismatch. **Rejected.** |
| `std::vector<std::size_t>` for shape | `std::array<std::size_t, N>` (compile-time rank) | `[CITED: FEATURES.md]` Compile-time rank is a v0.9.0 question (POL-03). For v0.8.0, dynamic rank matches NumPy/xtensor and Quiver's existing `BinaryMetadata::dimensions` representation. |
| `std::span<T>` (C++20) | Kokkos `mdspan-0.6.0` backport | `[CITED: STACK.md]` `<mdspan>` is C++23; MSVC + Apple Clang lag. Phase 1 only needs flat-buffer access, not strided multi-dim viewing. **Rejected** for milestone. |

**Installation:** None. Phase 1 adds **zero** FetchContent_Declare entries. `[VERIFIED: cmake/Dependencies.cmake]` already covers GoogleTest 1.17.0 (the only dep used by Phase 1's test file).

**Version verification:**
```bash
grep -n "v1\." cmake/Dependencies.cmake | grep -i googletest -A1
# v1.17.0 (verified — already in repo at this version, no change for Phase 1)
```

---

## Architecture Patterns

### System Architecture Diagram

For Phase 1 specifically (no operators, no broadcasting, no reductions), the system is exceptionally simple — but the diagram must capture the *containment* relationships that Phase 1 enforces, because containment is what BLD-04 polices.

```
┌────────────────────────────────────────────────────────────────────────────┐
│                       USER C++ CODE (out of repo)                          │
│   #include <quiver/tensor/tensor.h>      ← OPT-IN; not pulled by quiver.h │
│                                                                            │
│   quiver::tensor::Tensor<double> t({2, 3, 4}, 0.0);                        │
│   t(0, 0, 0) = 42.0;       // operator()  unchecked   (D-06)               │
│   double v = t.at(0, 0, 0); // at(...)    bounds-checked (D-07)            │
│   for (auto& x : t)         // begin/end  range-for       (D-09)           │
│   { ... }                                                                  │
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   │ #include
┌──────────────────────────────────▼─────────────────────────────────────────┐
│                  include/quiver/tensor/   (NEW — Phase 1)                  │
│                                                                            │
│  ┌────────────────┐    ┌────────────────────┐    ┌──────────────────────┐ │
│  │ expression.h   │    │ shape.h            │    │ tensor.h             │ │
│  │                │    │                    │    │                      │ │
│  │ Expression<D>  │    │ shape_t alias      │    │ Tensor<T>            │ │
│  │   self()       │◄───┤ compute_strides    │◄───┤   - inherits         │ │
│  │   shape()      │    │ total_size         │    │     Expression<      │ │
│  │   size()       │    │ ravel_index        │    │       Tensor<T>>     │ │
│  │   eval(i)      │    │                    │    │   - 3 ctors          │ │
│  │                │    │                    │    │   - operator(),at,   │ │
│  │ IsTensorExpr<T>│    │                    │    │     begin/end, item, │ │
│  │   concept      │    │                    │    │     data, size,      │ │
│  │                │    │                    │    │     shape, rank      │ │
│  │ detail::is_    │    │                    │    │                      │ │
│  │   base_of_     │    │                    │    │                      │ │
│  │   template_v   │    │                    │    │                      │ │
│  └────────────────┘    └────────────────────┘    └──────────────────────┘ │
│                                                                            │
│  ┌─ Phase 2+ files reserved (NOT in Phase 1) ─────────────────────────┐   │
│  │ binary_op.h, unary_op.h, ops.h, broadcast.h, reduce.h              │   │
│  └────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────┘

       ┌──────────────────────────────────────────────────────────┐
       │ FORBIDDEN includes (BLD-04 enforces this at CI time)     │
       │                                                          │
       │   include/quiver/database.h     ──×──> tensor/...        │
       │   include/quiver/element.h      ──×──> tensor/...        │
       │   include/quiver/binary/*.h     ──×──> tensor/...        │
       │   include/quiver/c/*.h          ──×──> tensor/...        │
       │   include/quiver/c/binary/*.h   ──×──> tensor/...        │
       │   include/quiver/quiver.h       ──×──> tensor/...        │
       │                                                          │
       │ enforced by: cmake/CheckTensorIncludeContainment.cmake   │
       │   - file(GLOB_RECURSE) on include/quiver/                │
       │   - exclude include/quiver/tensor/  (per D-17)           │
       │   - file(STRINGS ... REGEX) for D-16 grep pattern        │
       │   - message(FATAL_ERROR) on match, with file:line list   │
       └──────────────────────────────────────────────────────────┘

      ┌──────────────────────────────────────────────────────────┐
      │ ZERO-DIFF surfaces (Pitfall #13; verified per phase PR)  │
      │                                                          │
      │   include/quiver/c/*           ── 0 lines changed         │
      │   include/quiver/c/binary/*    ── 0 lines changed         │
      │   bindings/julia/*             ── 0 lines changed         │
      │   bindings/dart/*              ── 0 lines changed         │
      │   bindings/python/*            ── 0 lines changed         │
      │   bindings/js/*                ── 0 lines changed         │
      │   src/CMakeLists.txt           ── 0 lines changed (BLD-03)│
      │                                                          │
      │ verified by: git diff --stat <those paths> at PR time    │
      └──────────────────────────────────────────────────────────┘
```

The diagram captures the three architectural invariants of Phase 1: **(1) opt-in surface** (users include `<quiver/tensor/...>` directly; never via the umbrella `quiver.h`), **(2) include containment** (no non-tensor header in `include/quiver/` pulls in `tensor/`), **(3) zero-diff for FFI/bindings** (cross-phase discipline).

### Recommended Project Structure

```
include/quiver/tensor/        # NEW directory — header-only public API (Phase 1)
├── expression.h              # Expression<Derived> CRTP base + IsTensorExpr concept +
│                             #   detail::is_base_of_template_v helper
├── shape.h                   # shape_t alias, compute_strides, total_size, ravel_index
└── tensor.h                  # Tensor<T> owning storage; 3 ctors; operator(), at, item,
                              #   data, size, shape, rank, begin, end; noexcept move

# Phase 2+ files reserved (NOT created in Phase 1):
#   binary_op.h, unary_op.h, ops.h     ← Phase 2
#   broadcast.h                         ← Phase 3
#   reduce.h                            ← Phase 4

cmake/                        # MOD: append one new file, no edits to existing files
└── CheckTensorIncludeContainment.cmake   # NEW — CMake script for BLD-04 leak detection

tests/                        # MOD: append one new test file, append CMake registration
├── CMakeLists.txt            # MOD: append `test_tensor_storage.cpp`, add `add_test(NAME tensor_no_leakage ...)`
└── test_tensor_storage.cpp   # NEW — covers all 11 phase requirements

# UNCHANGED (zero edits, verified by `git diff --stat`):
#   src/CMakeLists.txt              ← BLD-03 hard rule
#   include/quiver/quiver.h         ← umbrella; tensor stays opt-in
#   include/quiver/c/*              ← Pitfall #13
#   include/quiver/c/binary/*       ← Pitfall #13
#   bindings/*                      ← Pitfall #13
```

### Pattern 1: CRTP `Expression<Derived>` Base — Plain Value, No Pimpl

**What:** A templated base parameterized by its derived type, providing a `self()` static cast and the common interface (`shape()`, `size()`, `eval(linear_idx)`) that every tensor expression must expose. Plain value type with a protected default constructor and `Rule of Zero` for copy/move/destruct.

**When to use:** Always. Every concrete tensor expression type (Phase 1: just `Tensor<T>`; Phase 2+: `BinaryOp`, `UnaryOp`, etc.) inherits CRTP-style.

**Source:** `[CITED: ARCHITECTURE.md Pattern 1]`, `[CITED: cppreference CRTP]` ([cppreference CRTP](https://en.cppreference.com/cpp/language/crtp))

**Example:**
```cpp
// include/quiver/tensor/expression.h
namespace quiver::tensor {

template <class Derived>
class Expression {
public:
    Derived& self() noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

    std::size_t size() const { return self().size(); }
    const shape_t& shape() const { return self().shape(); }
    std::size_t rank() const { return shape().size(); }
    auto eval(std::size_t i) const { return self().eval(i); }

protected:
    Expression() = default;
    // Rule of Zero — defaulted copy/move/dtor; no virtual dtor (CRTP, never deleted polymorphically).
};

}  // namespace quiver::tensor
```

Notes:
- **No `virtual` keyword anywhere.** Deletion through a base pointer is undefined; CRTP discipline forbids it. Document in CONVENTIONS.md if needed.
- **`self()` is `noexcept`.** Cheap static cast; cannot throw.
- **Protected default ctor** prevents accidental construction of an unparameterized `Expression<T>`.

### Pattern 2: `Tensor<T>` Plain Value Type with Rule of Zero + `noexcept` move

**What:** Owning storage leaf. Holds `std::vector<T> data_` + `shape_t shape_` (= `std::vector<std::size_t>`) + `shape_t strides_` (precomputed row-major). Inherits `Expression<Tensor<T>>`. Plain value type, no Pimpl (no private dependencies to hide).

**When to use:** Always. The single concrete leaf for Phase 1.

**Source:** `[CITED: CONVENTIONS.md "Pimpl vs Value Types"]`, `[CITED: include/quiver/binary/dimension.h precedent]`, `[CITED: ARCHITECTURE.md Pattern 2/3]`

**Example:**
```cpp
// include/quiver/tensor/tensor.h
#include "expression.h"
#include "shape.h"

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace quiver::tensor {

template <class T>
class Tensor : public Expression<Tensor<T>> {
public:
    // D-01: three constructors only
    explicit Tensor(shape_t shape)
        : shape_(std::move(shape)), strides_(compute_strides(shape_)) {
        // D-02: zero-init via assign
        data_.assign(total_size(shape_), T{});
    }

    Tensor(shape_t shape, T fill)
        : shape_(std::move(shape)), strides_(compute_strides(shape_)) {
        data_.assign(total_size(shape_), fill);
    }

    Tensor(shape_t shape, std::initializer_list<T> init)
        : shape_(std::move(shape)), strides_(compute_strides(shape_)) {
        const std::size_t expected = total_size(shape_);
        if (init.size() != expected) {
            // D-05: Pattern 1 error
            throw std::runtime_error(
                "Cannot construct tensor: initializer_list size " + std::to_string(init.size()) +
                " does not match shape size " + std::to_string(expected));
        }
        data_.assign(init.begin(), init.end());
    }

    // Rule of Zero: defaulted copy + noexcept move (Anti-Pattern #6 prevention)
    Tensor(const Tensor&) = default;
    Tensor& operator=(const Tensor&) = default;
    Tensor(Tensor&&) noexcept = default;
    Tensor& operator=(Tensor&&) noexcept = default;
    ~Tensor() = default;

    // STG-02 accessors (Expression<> base also forwards these via self())
    const shape_t& shape() const noexcept { return shape_; }
    std::size_t size() const noexcept { return data_.size(); }
    std::size_t rank() const noexcept { return shape_.size(); }

    // STG-04 raw data (D-10: const T* only; mutable variant deferred to v0.9.0)
    const T* data() const noexcept { return data_.data(); }

    // STG-05 iterators (D-09: forward to vector)
    auto begin() noexcept { return data_.begin(); }
    auto end() noexcept { return data_.end(); }
    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }

    // STG-03 unchecked indexing (D-06; hot path)
    template <class... Idx>
    T& operator()(Idx... idxs) noexcept {
        return data_[ravel_index(strides_, idxs...)];
    }
    template <class... Idx>
    const T& operator()(Idx... idxs) const noexcept {
        return data_[ravel_index(strides_, idxs...)];
    }

    // STG-03 checked indexing (D-07; rank + bounds validation, Pattern 1 errors)
    template <class... Idx>
    T& at(Idx... idxs) {
        check_rank_and_bounds(idxs...);
        return data_[ravel_index(strides_, idxs...)];
    }
    template <class... Idx>
    const T& at(Idx... idxs) const {
        check_rank_and_bounds(idxs...);
        return data_[ravel_index(strides_, idxs...)];
    }

    // D-12 .item() — Pattern 1 error if not 0-D-or-1-element
    T item() const {
        if (data_.size() != 1) {
            throw std::runtime_error(
                "Cannot item: tensor has " + std::to_string(data_.size()) +
                " elements (expected 1)");
        }
        return data_[0];
    }

    // EXP-01: eval(linear_idx) for the CRTP interface — return by VALUE (see "Concept Idiom")
    T eval(std::size_t i) const noexcept { return data_[i]; }

private:
    template <class... Idx>
    void check_rank_and_bounds(Idx... idxs) const {
        constexpr std::size_t n = sizeof...(idxs);
        if (n != shape_.size()) {
            throw std::runtime_error(
                "Cannot access tensor: rank mismatch (expected " +
                std::to_string(shape_.size()) + " indices, got " + std::to_string(n) + ")");
        }
        // bounds check via fold-expression or ravel + per-dim guard;
        // implementer's choice — see "Code Examples" below for one approach.
    }

    std::vector<T> data_;
    shape_t shape_;
    shape_t strides_;
};

}  // namespace quiver::tensor
```

Notes:
- **Copy is enabled.** A `Tensor<T>` is a value (a `std::vector<T>` wrapper), not a resource handle. This differs from `Database`/`BinaryFile` (move-only because they hold `sqlite3*`/`std::iostream`). Users who want move semantics get them from `std::move`; users who want a copy get a plain `Tensor<T> b = a;`.
- **`noexcept` on move ops** is critical: `std::vector<Tensor<T>>` would silently copy on growth otherwise (Anti-Pattern #6). `std::vector<T>` and `shape_t` (also `std::vector`) make this trivially `noexcept`.
- **`eval(i)` returns `T` by value, not `const T&`** — see "Concept Idiom" decision below.

### Pattern 3: Free Functions in `shape.h`, NOT in `detail::`

**What:** `using shape_t = std::vector<std::size_t>` and three free functions: `compute_strides(shape) -> shape_t`, `total_size(shape) -> std::size_t`, `ravel_index(strides, idxs...) -> std::size_t`.

**When to use:** Always. Public functions in `quiver::tensor` namespace, not `quiver::tensor::detail`.

**Why public, not `detail::`:** Phase 2+ free files (`broadcast.h`, `reduce.h`) need to call `compute_strides` and `total_size` to compute target shapes for broadcast results and reduce-output shapes. Hiding them under `detail::` forces every Phase 2+ implementation to use `detail::compute_strides(...)` — a noisier API for no security benefit. They are pure value functions; no internal state to protect.

**Source:** `[CITED: ARCHITECTURE.md Pattern 5]`, `[CITED: FEATURES.md "shape inspection"]`

**Example:**
```cpp
// include/quiver/tensor/shape.h
#include <cstddef>
#include <vector>

namespace quiver::tensor {

using shape_t = std::vector<std::size_t>;

inline shape_t compute_strides(const shape_t& shape) {
    shape_t strides(shape.size());
    std::size_t s = 1;
    for (std::size_t i = shape.size(); i > 0; --i) {
        strides[i - 1] = s;
        s *= shape[i - 1];
    }
    return strides;
}

inline std::size_t total_size(const shape_t& shape) {
    // D-04: 0-sized dimensions valid → total_size = 0; D-11: empty shape → total_size = 1
    std::size_t n = 1;
    for (auto d : shape) n *= d;
    return n;
}

template <class... Idx>
inline std::size_t ravel_index(const shape_t& strides, Idx... idxs) noexcept {
    std::size_t result = 0;
    std::size_t i = 0;
    // C++17 fold expression; valid in C++20
    ((result += static_cast<std::size_t>(idxs) * strides[i++]), ...);
    return result;
}

}  // namespace quiver::tensor
```

Notes:
- **`compute_strides` for empty shape** returns an empty `shape_t{}` — strides loop doesn't execute when `shape.size() == 0`. Combined with `total_size(shape_t{}) = 1` (D-11), 0-D tensors have a single-element `data_` with no strides. `eval(0)` reads `data_[0]`; `operator()()` (no args) returns `data_[0]` via the empty fold expression in `ravel_index` (`result = 0`). Verify in tests.
- **`total_size` for 0-sized dim** (D-04): `Tensor<double>({2, 0, 3})` → `total_size = 0` → `data_.empty()`. No-op iterators are fine; `eval(0)` is UB by contract (size-zero tensor has nothing to eval); this matches NumPy.

### Anti-Patterns to Avoid (subset relevant to Phase 1)

- **Anti-Pattern: `Tensor<T>` as Pimpl.** It has no private dependencies to hide. `[CITED: CLAUDE.md "Principles"]` Pimpl is reserved for hiding sqlite3/sol2/iostream; tensor uses Rule of Zero.
- **Anti-Pattern: Adding `tensor.h` to `include/quiver/quiver.h`.** Defeats compile-time containment for translation units that don't use tensors. Phase 1 specifically reserves opt-in via `<quiver/tensor/...>` includes only. `[CITED: ARCHITECTURE.md "Note on aggregator"]`
- **Anti-Pattern: Functors in `std::` namespace (Phase 2 reserves `quiver::tensor::ops::` instead — N/A in Phase 1).** Reserved here so the planner doesn't accidentally create one before Phase 2.
- **Anti-Pattern: Throwing in `eval(i)` hot path.** Phase 1 has no opportunity to do this (no operators yet), but the planner should ensure `eval(i)` on `Tensor<T>` is `noexcept` and never validates. `[CITED: ARCHITECTURE.md Anti-Pattern 7]`

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Owning multi-dim contiguous storage | Custom `T*` + `size_t length_` + manual `new[]`/`delete[]` | `std::vector<T>` | RAII, exception safety, growth, copy/move correctness. Mirrors existing `Element`/`Row` precedent. |
| CRTP-derivation detection | Custom `is_base_of_template` from scratch | A 4-line helper template (see "Concept Idiom" section) | The helper is a known idiom; ~4 LOC; same as Eigen's `internal::is_arg_<>` pattern. Don't reinvent. |
| Stride computation | Custom recursive template | Free function in `shape.h` (loop) | Simpler, same codegen at -O2, easier to test. |
| Initializer-list construction | Custom variadic ctor | `std::initializer_list<T>` (C++11+) | Standard idiom; matches the constructor surface NumPy/xtensor users expect. |
| BLD-04 include leakage detection | Hand-written shell script (POSIX) + Windows batch (separate) | CMake-script CTest (`-P` mode) | One file, cross-platform, no shell-fork. `[CITED: ARCHITECTURE.md "CTest add_test() with CMake script pattern"]` |
| Per-database-method-style verbose error messages | Custom error-formatter class | The three Quiver patterns inline | `[CITED: CLAUDE.md "Error Handling"]` Pattern 1/2/3 are the codebase contract; bindings rely on the format. |

**Key insight:** Phase 1 is a scaffolding phase, not a feature phase. The discipline is to use the smallest standard tool that solves each sub-problem and resist the urge to build infrastructure that Phase 2-4 will need to redesign anyway.

---

## Header Split

**Decision:** Three headers under `include/quiver/tensor/` — `expression.h`, `shape.h`, `tensor.h` — exactly as research recommended, with no further sub-splitting in Phase 1.

**Justification:**

1. **One concept per header.** `expression.h` defines the Expression CRTP base AND the `IsTensorExpr` concept (and its `detail::is_base_of_template_v` helper). `shape.h` defines `shape_t`, `compute_strides`, `total_size`, `ravel_index`. `tensor.h` defines `Tensor<T>`. Mirrors the existing `binary/` precedent of `dimension.h` / `time_properties.h` / `binary_metadata.h`: one type or family per header.

2. **Dependency direction is acyclic.** `tensor.h` → `expression.h` (inheritance) and `shape.h` (member types + free fn calls). `expression.h` is standalone (only `<cstddef>`, `<concepts>`, `<type_traits>`). `shape.h` is standalone (only `<cstddef>`, `<vector>`). No circular includes; clear inclusion order.

3. **Phase 2+ extension preserves this layout.** Future `binary_op.h` includes `expression.h` and `shape.h` but NOT `tensor.h` (BinaryOp doesn't need the leaf storage type — `ref_selector` is forward-declared). Future `broadcast.h` includes `expression.h` + `shape.h`. This means Phase 1's split survives the milestone without retrofit.

4. **Single-header alternative rejected.** A single `tensor.h` containing Expression + shape + Tensor would balloon to ~400 lines by end of Phase 1, ~1500 lines by end of Phase 4 once binary_op/unary_op/broadcast/reduce land. Quiver convention (`binary/`) is multi-file from the start; matching it minimizes future churn.

5. **Detail-namespace alternative for `compute_strides`/`total_size`/`ravel_index` rejected.** They are pure value functions consumed by Phase 2+ public files (BinaryOp shape computation, BroadcastView, ReduceExpression). Forcing a `detail::` qualifier on every call site is noise without a corresponding security benefit — users CAN access them, but they have nothing to gain from doing so directly. Public free functions in `quiver::tensor` is the right level.

**Final boundary table:**

| Header | Contents | Includes (from same dir) | External includes |
|--------|----------|--------------------------|-------------------|
| `expression.h` | `Expression<Derived>` CRTP base, `IsTensorExpr` concept, `detail::is_base_of_template_v` | (none) | `<cstddef>`, `<concepts>`, `<type_traits>` |
| `shape.h` | `shape_t` alias, `compute_strides`, `total_size`, `ravel_index` | (none) | `<cstddef>`, `<vector>` |
| `tensor.h` | `Tensor<T>` value type | `expression.h`, `shape.h` | `<cstddef>`, `<initializer_list>`, `<stdexcept>`, `<string>`, `<vector>` |

**Note:** `compute_strides` and `ravel_index` could also be `[[nodiscard]]` for caller-discipline; the planner can decide. The locked decisions don't constrain this.

---

## Concept Idiom

**Decision:** Implement `IsTensorExpr` via a `detail::is_base_of_template_v<Expression, T>` helper, NOT via `std::derived_from<T, Expression<U>>` with a probe.

```cpp
// include/quiver/tensor/expression.h, namespace quiver::tensor::detail
template <template <class> class Base, class Derived>
struct is_base_of_template {
private:
    template <class U>
    static std::true_type  test(const Base<U>*);
    static std::false_type test(...);
public:
    static constexpr bool value = decltype(test(std::declval<Derived*>()))::value;
};

template <template <class> class Base, class Derived>
inline constexpr bool is_base_of_template_v = is_base_of_template<Base, Derived>::value;

// Public-namespace concept
template <class T>
concept IsTensorExpr = detail::is_base_of_template_v<Expression, std::remove_cvref_t<T>>;
```

**Justification:**

1. **`std::derived_from<T, Expression<U>>` requires naming `U`.** The C++20 `std::derived_from` concept is satisfied iff `Base` is a class type that is `Derived` or a public unambiguous base of `Derived`. `[CITED: cppreference std::derived_from]` It cannot express "derived from `Expression<U>` for some `U`" without quantifying `U` somehow — typically via a probe template that enumerates candidates, which doesn't work for an open set. The probe approach also has the deferred-completeness problem: at the point the constraint is evaluated, `Expression<Derived>` may not be a complete type if `Derived`'s definition is mid-parse. `[CITED: WebSearch 2026-05-05]`

2. **The `is_base_of_template` idiom does not require naming `U`.** The `test(const Base<U>*)` overload uses template-argument deduction to discover `U` from `Derived*`'s actual base. If any such `U` exists, the `true_type` overload wins. If none exists, the `(...)` fallback returns `false_type`. This is the exact mechanism Eigen uses internally (`internal::is_arg_evaluator<>` and friends) and the canonical answer in C++ template community. `[CITED: WebSearch — "is_base_of_template idiom"]`

3. **Works at the public-overload entry point** (Phase 2 will need this). When a Phase 2 free function is declared `template <class L, class R> requires (IsTensorExpr<L> || IsTensorExpr<R>) auto operator+(...)`, the constraint is evaluated against fully-formed concrete types (e.g. `Tensor<double>` or `BinaryOp<...>`); no incomplete-type problem. The diagnostic the user sees is `the constraint IsTensorExpr<...> was not satisfied` — readable, named, grep-friendly. Mitigates Pitfall #7.

4. **`std::remove_cvref_t<T>`** strips reference and cv-qualifiers so `IsTensorExpr<const Tensor<double>&>` evaluates the same as `IsTensorExpr<Tensor<double>>`. Avoids surprise when the concept is used to constrain forwarding-reference template parameters.

5. **No duck-typing.** D-14 explicitly rejects "has a `shape()` member" / "has an `eval(i)` member" approaches. They invite ADL collisions with `std::complex` (Anti-Pattern #3) and lose the "only Expression-derived types are tensor expressions" discipline. The strict CRTP-derivation concept matches Eigen and xtensor.

**Sub-decision: `eval(linear_idx)` returns by value, not `const T&`.**

Both are correct for `T ∈ {float, double, int32_t, int64_t}` (all four are <= 8 bytes; passing by value through a register is cheap). Returning by value is preferred because:
- **Phase 2+ intermediate nodes** (`BinaryOp::eval(i)` returning `Op{}(lhs_.eval(i), rhs_.eval(i))`) compute values on the fly; there's no `const T&` available to return. If `Tensor<T>::eval(i)` returns `const T&`, the type inference for `auto v = expr.eval(i);` differs between leaf and intermediate (`v` is `const T&` for a Tensor, `T` for a BinaryOp) — surprising.
- Returning by value normalizes the contract: every `eval(i)` returns a fresh value of the expression's element type. Clearer to reason about.
- At -O2, the compiler elides the value semantically into the same load it would emit for a reference return on these element types. Zero observable cost.

The `auto eval(std::size_t i) const { return self().eval(i); }` in `Expression<Derived>` then returns `T` from `Tensor<T>::eval(i)` — consistent across all expression types.

---

## Phase 1 Threats / Pitfalls Recap

The four pitfalls Phase 1 specifically prevents (per `.planning/research/PITFALLS.md`):

### Pitfall #4 — Header-only template heaviness slows the entire `quiver` library compile

**Mitigation (Phase 1, must-have):**
1. **`include/quiver/tensor/*.h` MUST NOT be transitively included by any non-tensor public header.** Enforced by the BLD-04 CTest target (D-15..D-18). The grep is mechanical: `#\s*include\s*[<"]quiver/tensor/` (D-16) over `include/quiver/` recursive minus `include/quiver/tensor/` (D-17). On match, `message(FATAL_ERROR ...)` with file:line list.
2. **`include/quiver/quiver.h` MUST NOT add a `#include <quiver/tensor/...>` line.** Tensor stays opt-in. The BLD-04 grep covers this case automatically (`quiver.h` is in scope, `tensor/` is excluded).
3. **No `tensor::*` symbol referenced from any non-tensor header.** A symbol-reference grep was rejected per D-16 (false-positive risk on doc strings); the include-line grep is the locked mechanism. Rely on the discipline of "don't add tensor includes anywhere except tensor headers and test files."

**Planner must-haves:**
- The CMake script (`cmake/CheckTensorIncludeContainment.cmake`) MUST be created with the exact regex from D-16 and the exact scope from D-17.
- The `add_test` registration in `tests/CMakeLists.txt` MUST run via `${CMAKE_COMMAND} -P` (D-18); no compiled binary.
- A test case in `test_tensor_storage.cpp` MUST verify the basic case (Tensor construction + element access) builds and links — this is the canary that catches BLD-04 misconfiguration (if the script silently fails open, no other test will catch it).
- A failure-mode plan: a manual one-time test where the planner adds an offending `#include <quiver/tensor/tensor.h>` to `include/quiver/database.h`, runs `ctest -R no_leakage`, verifies it fails with a useful diagnostic, then reverts. (NOT a permanent test — just a one-time manual verification during execution.)

### Pitfall #7 — Template error messages that bury the actual bug

**Mitigation (Phase 1, must-have):**
1. **`IsTensorExpr` concept declared in Phase 1**, not deferred to Phase 5 polish. Phase 2's operator overloads will constrain on it from day one (`requires IsTensorExpr<L> || IsTensorExpr<R>`); the diagnostic the user sees is named (`the constraint IsTensorExpr<...> was not satisfied`) instead of a 600-line nested template trace.
2. **Concept name follows Eigen's UPPERCASE-grep-friendly convention via the `requires` clause naming.** No Quiver-specific `static_assert` macros needed in Phase 1; the C++20 concept itself produces the named diagnostic.
3. **`detail::is_base_of_template_v` helper is pure metaprogramming; no SFINAE substitution-failure path is exposed.** Either `IsTensorExpr<T>` is satisfied or it isn't; the diagnostic doesn't depend on candidate enumeration.

**Planner must-haves:**
- The concept MUST be declared in `expression.h` (next to `Expression<Derived>`) so Phase 2's operator headers can `#include "expression.h"` and reference it.
- The detail helper MUST live under `quiver::tensor::detail::` (not at root) per Pattern 14 namespace hygiene.
- A negative test in `test_tensor_storage.cpp` SHOULD verify `IsTensorExpr<int>` is `false` and `IsTensorExpr<Tensor<double>>` is `true`. The test catches regressions where the helper accidentally accepts non-derived types.

### Pitfall #13 — Bridge designed too early (cross-phase discipline)

**Mitigation (every phase, must-have):**
1. **Zero diff to `include/quiver/c/`, `include/quiver/c/binary/`, `bindings/julia/`, `bindings/dart/`, `bindings/python/`, `bindings/js/` for the Phase 1 PR.** Verified via `git diff --stat <those paths>`.
2. **No new public function on `Database`, `BinaryFile`, `BinaryMetadata`, or any other existing class mentions `Tensor`.** Verified via `grep -r 'Tensor' include/quiver/ src/ --include='*.h' --include='*.cpp' | grep -v 'include/quiver/tensor' | grep -v 'src/tensor'` should yield zero results outside test files.
3. **No new symbol in `include/quiver/c/` or `include/quiver/c/binary/`.** Same grep, scoped to `c/`.
4. **No regenerated bindings.** `bindings/julia/generator/`, `bindings/dart/generator/`, `bindings/python/generator/` MUST NOT be invoked. v0.8.0 makes no header changes that matter to bindings; if a binding regeneration appears in a plan diff, the plan is out-of-scope.

**Planner must-haves:**
- A final task at the end of the phase plan: "Verify Pitfall #13 cross-phase discipline." Runs `git diff --stat` on the listed paths and asserts zero rows changed. If non-zero, the phase fails to land.
- The plan-summary MUST cite this verification as a hard gate, not a soft check.
- The plan MUST NOT include any "stub" or "preliminary" entries for the future v0.9.0 bridge — even commented-out code would be a discipline violation.

### Pitfall #14 — Naming collision with `Database` API in the same namespace tree

**Mitigation (Phase 1, must-have):**
1. **All tensor symbols live under `quiver::tensor::*`.** No `quiver::Tensor`, no `quiver::eval`, no `quiver::sum`. Enforced by code-review: any new symbol introduced under `quiver::` (root) outside an existing file is a bug.
2. **`quiver::tensor::detail::*` for internals.** `detail::is_base_of_template_v` MUST live there, not at `quiver::tensor::is_base_of_template_v`.
3. **Future `quiver::tensor::ops::` namespace reserved (Phase 2)** for arithmetic functors. Phase 1 doesn't ship any functors, but the planner MUST NOT create symbols at `quiver::tensor::Add`, `quiver::tensor::Sub`, etc. Reserve that namespace for Phase 2 by leaving it empty.
4. **Method names audit.** No `Tensor::read`, `Tensor::describe`, or `Tensor::write` — these collide cognitively with `Database::read_*`, `Database::describe`, `BinaryFile::write`. Use `Tensor::data`, `Tensor::at`, `Tensor::operator()`, `Tensor::item`, `Tensor::eval` — names that match the tensor domain, not the database domain.

**Planner must-haves:**
- Code-review check at the end of the phase: `grep -n 'namespace quiver' include/quiver/tensor/*.h | grep -v 'tensor\b'` should yield zero results except the closing comments. (Catches an accidental `namespace quiver {` at root inside a tensor header.)
- A test case in `test_tensor_storage.cpp` SHOULD use `quiver::tensor::Tensor<double>` (full qualification) at least once to verify the namespace nesting.

---

## Common Pitfalls

(Beyond the four phase-specific ones above, the remaining pitfalls relevant to verification:)

### Pitfall A: Forgetting `noexcept` on move ops

**What goes wrong:** `std::vector<Tensor<T>>` silently copies on growth instead of moving — silent O(N²) bytes moved.

**Why it happens:** Defaulted move ops without explicit `noexcept` annotation; some implementations or compiler-flags combinations don't infer `noexcept`.

**How to avoid:** Explicit annotation:
```cpp
Tensor(Tensor&&) noexcept = default;
Tensor& operator=(Tensor&&) noexcept = default;
```
Mirrors `Database` (`include/quiver/database.h:28-29`).

**Warning signs:** Performance regression in code using `std::vector<Tensor<T>>` past ~16 elements; `std::is_nothrow_move_constructible_v<Tensor<double>>` static_assert returns `false`.

### Pitfall B: 0-sized dimension `Tensor<double>({2, 0, 3})` triggers division-by-zero in stride math

**What goes wrong:** `compute_strides` for shape `{2, 0, 3}` works fine (returns `{0, 3, 1}` — outer stride is 0, which is correct: total size is 0, no element is reachable). But user code that does `Tensor<double>({2, 0, 3}); for (auto& x : t) ...` should produce zero iterations, not crash.

**Why it happens:** Some implementations default-construct strides as `1` and divide later, hitting division-by-zero on empty dims.

**How to avoid:** The `compute_strides` recipe in Pattern 3 multiplies, never divides — safe for 0-sized dims. `total_size({2, 0, 3}) = 0` (D-04). `data_.empty()`. Iteration is a no-op. `eval(0)` is UB by contract (no element to read); the planner MUST add a test case for this.

**Warning signs:** Test for `Tensor<double>({2, 0, 3})` segfaults or asserts on construction.

### Pitfall C: 0-D tensor (`shape_t{}`) — empty fold expression in `ravel_index`

**What goes wrong:** `Tensor<double>({}, 42.0); double v = t();` — calling `operator()` with no arguments. The fold expression `((result += idxs * strides[i++]), ...)` for zero parameters expands to nothing; `result` stays `0`. `data_[0]` is `42.0`. Correct, but easy to break with a "rank > 0" precondition.

**Why it happens:** Implementer adds `if (sizeof...(idxs) == 0) throw ...` thinking it's defensive; breaks 0-D tensors.

**How to avoid:** D-06 says `operator()` is unchecked — no rank precondition. Empty fold expression is well-defined C++17. Test case: `Tensor<double>({}, 42.0); EXPECT_EQ(t(), 42.0); EXPECT_EQ(t.item(), 42.0); EXPECT_EQ(t.size(), 1u); EXPECT_EQ(t.rank(), 0u);`

**Warning signs:** Compile error on `t()` for a 0-D tensor, or runtime error from a rank-precondition guard.

### Pitfall D: Initializer-list size validation triggered with rounding-error tolerance

**What goes wrong:** D-05 says initializer-list size MUST equal `total_size(shape)` exactly. A defensive implementer adds `if (init.size() < expected) ...` and zero-pads — silently breaks the contract.

**How to avoid:** Use `!=` comparison, not `<`, in the size check. Throw on mismatch, never zero-pad.

**Warning signs:** Test for `Tensor<double>({2, 3}, {1.0, 2.0})` (4-too-few elements) doesn't throw — silently zeros the tail.

### Pitfall E: Move-from-temporary in initializer-list constructor leaks the shape

**What goes wrong:** `Tensor<double>(std::move(my_shape), {1.0, 2.0, 3.0})` — `my_shape` is moved-from after the ctor, but if the implementer captures `shape` by value with no `std::move`, an extra copy happens.

**How to avoid:** All three ctors take `shape_t shape` by value (sink parameter); inside the ctor, `shape_(std::move(shape))` is used to steal into the member. This is the standard "sink parameter" idiom.

**Warning signs:** Profiling shows `std::vector<size_t>` allocations on tensor construction beyond the data buffer.

---

## Code Examples

Verified patterns drawn from existing precedents and matching the locked decisions:

### Example 1: Header file structure for `expression.h`

```cpp
// include/quiver/tensor/expression.h
// Source: ARCHITECTURE.md Pattern 1 + xtensor xexpression CRTP convention
#ifndef QUIVER_TENSOR_EXPRESSION_H
#define QUIVER_TENSOR_EXPRESSION_H

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace quiver::tensor {

// Forward declaration for shape_t use in the base interface
using shape_t = std::vector<std::size_t>;  // duplicated alias OK; or include shape.h

template <class Derived>
class Expression {
public:
    Derived& self() noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

    std::size_t size() const { return self().size(); }
    const shape_t& shape() const { return self().shape(); }
    std::size_t rank() const { return shape().size(); }
    auto eval(std::size_t i) const { return self().eval(i); }

protected:
    Expression() = default;
};

namespace detail {

template <template <class> class Base, class Derived>
struct is_base_of_template {
private:
    template <class U>
    static std::true_type test(const Base<U>*);
    static std::false_type test(...);
public:
    static constexpr bool value = decltype(test(std::declval<Derived*>()))::value;
};

template <template <class> class Base, class Derived>
inline constexpr bool is_base_of_template_v = is_base_of_template<Base, Derived>::value;

}  // namespace detail

template <class T>
concept IsTensorExpr = detail::is_base_of_template_v<Expression, std::remove_cvref_t<T>>;

}  // namespace quiver::tensor

#endif  // QUIVER_TENSOR_EXPRESSION_H
```

### Example 2: Test pattern for `test_tensor_storage.cpp`

```cpp
// tests/test_tensor_storage.cpp
// Source: existing test_binary_metadata.cpp pattern (no banner, includes-first)
#include <gtest/gtest.h>
#include <quiver/tensor/expression.h>
#include <quiver/tensor/shape.h>
#include <quiver/tensor/tensor.h>

#include <stdexcept>
#include <vector>

using namespace quiver::tensor;

// ============================================================================
// Tensor construction (STG-01)
// ============================================================================

TEST(TensorConstruction, ShapeOnlyZeroInitializes) {
    Tensor<double> t({2, 3, 4});  // D-02
    EXPECT_EQ(t.size(), 24u);
    EXPECT_EQ(t.rank(), 3u);
    for (auto& v : t) EXPECT_EQ(v, 0.0);
}

TEST(TensorConstruction, ShapeWithFill) {
    Tensor<int32_t> t({2, 3}, 42);
    EXPECT_EQ(t.size(), 6u);
    for (auto& v : t) EXPECT_EQ(v, 42);
}

TEST(TensorConstruction, ShapeWithInitList) {
    Tensor<float> t({2, 3}, {1.f, 2.f, 3.f, 4.f, 5.f, 6.f});
    EXPECT_EQ(t.at(0, 0), 1.f);
    EXPECT_EQ(t.at(1, 2), 6.f);
}

TEST(TensorConstruction, InitListMismatchThrowsPattern1) {
    EXPECT_THROW({
        try {
            Tensor<double> t({2, 3}, {1.0, 2.0});  // expects 6, got 2
        } catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("Cannot construct tensor"), std::string::npos);
            EXPECT_NE(std::string(e.what()).find("initializer_list size 2"), std::string::npos);
            EXPECT_NE(std::string(e.what()).find("shape size 6"), std::string::npos);
            throw;
        }
    }, std::runtime_error);
}

TEST(TensorConstruction, ZeroSizedDimensionValid) {
    Tensor<double> t({2, 0, 3});  // D-04
    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.rank(), 3u);
    EXPECT_EQ(t.begin(), t.end());  // no iterations
}

// ============================================================================
// 0-D tensor (D-11, D-12)
// ============================================================================

TEST(ZeroDTensor, EmptyShapeHasOneElement) {
    Tensor<double> t({}, 42.0);
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(t.rank(), 0u);
    EXPECT_EQ(t(), 42.0);  // empty fold OK
    EXPECT_EQ(t.item(), 42.0);  // D-12
}

TEST(ZeroDTensor, ItemThrowsOnNonScalar) {
    Tensor<int> t({3}, 5);
    EXPECT_THROW({
        try {
            (void) t.item();
        } catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("Cannot item"), std::string::npos);
            EXPECT_NE(std::string(e.what()).find("3 elements"), std::string::npos);
            throw;
        }
    }, std::runtime_error);
}

// ============================================================================
// Indexing (STG-03, D-06, D-07)
// ============================================================================

TEST(TensorIndexing, OperatorParensReadsAndWrites) {
    Tensor<int> t({2, 3});
    t(0, 0) = 1; t(0, 1) = 2; t(0, 2) = 3;
    t(1, 0) = 4; t(1, 1) = 5; t(1, 2) = 6;
    EXPECT_EQ(t(0, 0), 1);
    EXPECT_EQ(t(1, 2), 6);
}

TEST(TensorIndexing, AtThrowsOnRankMismatch) {
    Tensor<int> t({2, 3});
    EXPECT_THROW({
        try { (void) t.at(0); }  // expected 2 indices, got 1
        catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("Cannot access tensor"), std::string::npos);
            EXPECT_NE(std::string(e.what()).find("rank mismatch"), std::string::npos);
            throw;
        }
    }, std::runtime_error);
}

TEST(TensorIndexing, AtThrowsOnBoundsViolation) {
    Tensor<int> t({2, 3});
    EXPECT_THROW({
        try { (void) t.at(2, 0); }  // row 2 out of [0,2)
        catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("Cannot access tensor"), std::string::npos);
            EXPECT_NE(std::string(e.what()).find("out of bounds"), std::string::npos);
            throw;
        }
    }, std::runtime_error);
}

// ============================================================================
// Iteration (STG-05, D-09)
// ============================================================================

TEST(TensorIteration, RangeForCompatible) {
    Tensor<int> t({3}, {10, 20, 30});
    int sum = 0;
    for (auto v : t) sum += v;
    EXPECT_EQ(sum, 60);
}

// ============================================================================
// Move semantics (Anti-Pattern #6 prevention)
// ============================================================================

TEST(TensorMove, MoveOpsAreNoexcept) {
    static_assert(std::is_nothrow_move_constructible_v<Tensor<double>>);
    static_assert(std::is_nothrow_move_assignable_v<Tensor<double>>);
}

// ============================================================================
// Concept (EXP-02, D-14)
// ============================================================================

TEST(IsTensorExprConcept, AcceptsTensor) {
    static_assert(IsTensorExpr<Tensor<double>>);
    static_assert(IsTensorExpr<Tensor<int32_t>>);
    static_assert(IsTensorExpr<Tensor<float>>);
    static_assert(IsTensorExpr<Tensor<int64_t>>);
}

TEST(IsTensorExprConcept, RejectsNonExpression) {
    static_assert(!IsTensorExpr<int>);
    static_assert(!IsTensorExpr<std::vector<double>>);
    static_assert(!IsTensorExpr<double>);
}

// ============================================================================
// Element types (STG-01: T ∈ {float, double, int32_t, int64_t})
// ============================================================================

TEST(ElementTypes, AllFourTypesConstruct) {
    (void) Tensor<float>({2, 3}, 1.f);
    (void) Tensor<double>({2, 3}, 1.0);
    (void) Tensor<int32_t>({2, 3}, 1);
    (void) Tensor<int64_t>({2, 3}, int64_t{1});
}
```

### Example 3: BLD-04 CMake script `cmake/CheckTensorIncludeContainment.cmake`

```cmake
# cmake/CheckTensorIncludeContainment.cmake
# Source: D-15..D-18 + ARCHITECTURE.md "CTest add_test() with CMake script pattern"
#
# Run via: cmake -P cmake/CheckTensorIncludeContainment.cmake
# Returns FATAL_ERROR if any non-tensor public header transitively includes a tensor header.
# Wired into CTest via tests/CMakeLists.txt as: add_test(NAME tensor_no_leakage COMMAND ${CMAKE_COMMAND} -P ...)

# CMake scripts run with no project context; reconstruct the include scope from the script's path.
get_filename_component(SCRIPT_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(INCLUDE_DIR "${SCRIPT_DIR}/../include/quiver")

# D-17: scope is include/quiver/ recursive, EXCLUDING include/quiver/tensor/
file(GLOB_RECURSE ALL_HEADERS RELATIVE "${INCLUDE_DIR}" "${INCLUDE_DIR}/*.h")

set(VIOLATIONS "")
foreach(HEADER ${ALL_HEADERS})
    # Exclude the tensor subdirectory itself (D-17)
    if(HEADER MATCHES "^tensor/")
        continue()
    endif()

    set(FULL_PATH "${INCLUDE_DIR}/${HEADER}")
    # D-16: grep for #include of quiver/tensor/ in either <> or "" form
    file(STRINGS "${FULL_PATH}" MATCHES REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]quiver/tensor/")

    if(MATCHES)
        # Find the line numbers of each match for diagnostic
        file(STRINGS "${FULL_PATH}" ALL_LINES)
        set(LINE_NUM 0)
        foreach(LINE ${ALL_LINES})
            math(EXPR LINE_NUM "${LINE_NUM}+1")
            if(LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]quiver/tensor/")
                list(APPEND VIOLATIONS "  ${HEADER}:${LINE_NUM}: ${LINE}")
            endif()
        endforeach()
    endif()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n" VIOLATIONS_STR "${VIOLATIONS}")
    message(FATAL_ERROR
        "BLD-04 violation: non-tensor public header(s) in include/quiver/ pull in quiver/tensor/.\n"
        "Tensor headers MUST stay opt-in for users who include them directly.\n"
        "Allowing transitive pull-in causes header-only template heaviness to spread to every translation\n"
        "unit that touches database.h, element.h, binary/*.h, etc. (Pitfall #4).\n"
        "Offending lines:\n${VIOLATIONS_STR}\n")
endif()

message(STATUS "BLD-04: include containment OK — no public header transitively includes quiver/tensor/.")
```

### Example 4: `tests/CMakeLists.txt` amendments

```diff
 add_executable(quiver_tests
     test_binary_file.cpp
     test_csv_converter.cpp
     test_binary_metadata.cpp
     test_binary_time_properties.cpp
     test_database_create.cpp
     # ... (existing 21 entries unchanged) ...
+    test_tensor_storage.cpp
 )

 target_link_libraries(quiver_tests
     PRIVATE
         quiver
         quiver_compiler_options
         GTest::gtest_main
         GTest::gmock
 )

 # ... (existing DLL-copy + gtest_discover_tests blocks unchanged) ...

+# BLD-04: include containment — no public Quiver header outside include/quiver/tensor/
+# transitively pulls in quiver/tensor/*.h. Enforced by a CMake-script CTest target.
+add_test(NAME tensor_no_leakage
+         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CheckTensorIncludeContainment.cmake)
```

---

## File List

Concrete enumeration of every file touched by Phase 1. The planner uses this as input for pattern-mapper (which maps each file to an executable plan) and for the verification step that asserts no other files were modified.

| Path | Action | Role | Phase Requirement(s) |
|------|--------|------|----------------------|
| `include/quiver/tensor/expression.h` | NEW | Header — public C++ | EXP-01, EXP-02, BLD-01 |
| `include/quiver/tensor/shape.h` | NEW | Header — public C++ | STG-02, STG-03 (via `ravel_index`), BLD-01 |
| `include/quiver/tensor/tensor.h` | NEW | Header — public C++ | STG-01..05, BLD-01 |
| `tests/test_tensor_storage.cpp` | NEW | Test — GoogleTest | STG-01..05, EXP-01, EXP-02 (concept tests), BLD-02 |
| `cmake/CheckTensorIncludeContainment.cmake` | NEW | CMake script — BLD-04 enforcement | BLD-04 |
| `tests/CMakeLists.txt` | MOD | CMake — append `test_tensor_storage.cpp` to `quiver_tests`; add `add_test(tensor_no_leakage)` | BLD-02, BLD-03, BLD-04 |
| **Optionally:** `CLAUDE.md` | MOD | Documentation — add `tensor/` entry under "Architecture" parallel to `binary/` | C-08 (CLAUDE.md self-updating) |

**Files that MUST NOT be touched (zero diff):**

| Path | Why |
|------|-----|
| `src/CMakeLists.txt` | BLD-03 — header-only, hard rule |
| `include/quiver/quiver.h` | Tensor stays opt-in; Pitfall #4 prevention |
| `include/quiver/database.h`, `element.h`, `*.h` (root level) | Pitfall #4 / BLD-04 |
| `include/quiver/binary/*.h` | Pitfall #4 / BLD-04 |
| `include/quiver/c/*.h`, `c/binary/*.h` | Pitfall #13 |
| `bindings/*` (every subdirectory) | Pitfall #13 |
| `cmake/Dependencies.cmake`, `CompilerOptions.cmake`, `Platform.cmake` | No new deps; flags inherit |
| `CMakeLists.txt` (top-level) | No new options; C++20 is already set |

**Verification command (planner adds as final task):**
```bash
git diff --stat include/quiver/c/ include/quiver/c/binary/ bindings/ src/CMakeLists.txt cmake/Dependencies.cmake cmake/CompilerOptions.cmake CMakeLists.txt
# Expected: empty (no rows)
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SFINAE-based "is tensor expression" detection | C++20 concepts (`requires` clauses) | C++20 (2020) | Named diagnostics; one-line error vs 600-line trace. **Use concepts in Phase 1.** `[CITED: cppreference]` |
| Pre-C++20 CRTP base with virtual `eval` | CRTP base with `self()` static cast (no virtual) | Eigen / xtensor / Blaze convergence (2010s) | Zero-overhead static dispatch; required by the v0.8.0 "deterministic compile-time dispatch" constraint. **Use this in Phase 1.** `[CITED: ARCHITECTURE.md Pattern 1]` |
| `std::vector<std::valarray<T>>` for multi-dim storage | `std::vector<T>` + computed strides | NumPy-era convention (~2007); xtensor formalized | Cache-friendly contiguous buffer; lazy expressions can fuse. `valarray` is dead. **Use `std::vector<T>` + strides.** `[CITED: STACK.md "What NOT to Use"]` |
| `is_base_of_v<Base<Derived>, Derived>` directly in `requires` | `is_base_of_template_v<Base, Derived>` helper | C++ template community (~2015) | Avoids deferred-completeness problem at constraint-evaluation time. **Use the helper in Phase 1.** `[CITED: WebSearch — "is_base_of_template idiom"]` |

**Deprecated/outdated (do not adopt):**
- `std::valarray<T>`: Pre-C++11 vector type with weird semantics, no lazy eval, no broadcasting, no shape; not used by any modern numerical C++ codebase. `[CITED: STACK.md]`
- `boost::multi_array`: No expression templates / no lazy eval — it's a storage container, not a compute framework. `[CITED: STACK.md]`
- `boost::uBLAS`: Widely considered legacy by the C++ numerical community (Eigen/Blaze surpass it). `[CITED: STACK.md]`
- `<mdspan>` (C++23): Toolchain risk on MSVC + Apple Clang; defer until project bumps to C++23. `[CITED: STACK.md]`

---

## Assumptions Log

All claims tagged `[ASSUMED]` in this research. The discuss-phase / planner uses this section to identify decisions that need user confirmation before execution.

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| (none) | — | — | — |

**This table is empty.** All claims in this research were verified against in-tree files (`[VERIFIED: ...]`) or cited from upstream research/external authoritative sources (`[CITED: ...]`). No user confirmation is required before planning.

---

## Open Questions (RESOLVED)

> All four open questions below were resolved during phase planning. Each question retains its original analysis followed by a **RESOLVED:** line that locks the technical decision. These resolutions are scoped to Phase 1 (and the wider milestone v0.8.0 where noted) and are not user-facing decisions; locked user decisions live in CONTEXT.md as D-01..D-18.

1. **Should `cmake/CheckTensorIncludeContainment.cmake` also enforce that `include/quiver/tensor/*.h` headers have proper include guards?**
   - What we know: D-15..D-18 only specify the `#include`-leakage detection; they don't extend to other static checks.
   - What's unclear: A "while we're at it" addition could check that every new tensor header has `#ifndef QUIVER_TENSOR_*_H` / `#define ... ` / `#endif` triples. Mirrors `include/quiver/binary/dimension.h:1-3,24` precedent.
   - Recommendation: **Skip for Phase 1.** D-15..D-18 are explicit about scope; expanding the script's responsibility creeps. Catch missing include guards via the existing `clang-tidy` pass and CI multi-platform link (duplicate-symbol from missing guard would surface there).
   - **RESOLVED: NO.** `CheckTensorIncludeContainment.cmake` enforces include-leakage detection ONLY (D-15..D-18). Include-guard coverage is out of scope for v0.8.0; missing guards surface via clang-tidy and multi-platform link as already specified.

2. **Should `Tensor<T>::operator()(Idx... idxs) const` return `T` by value or `const T&`?**
   - What we know: D-10 says "implementer's choice; no observable difference for the supported `T`s".
   - What's unclear: The return type leaks into `auto v = t(0, 0);` deduction (`v` is `T` vs `const T&`).
   - Recommendation: **Return `const T&`** (matching `std::vector::operator[]`). Symmetric with the mutable variant returning `T&`; consistent with std-library convention. Note that `eval(i)` separately returns `T` by value (per "Concept Idiom" reasoning) — these are intentionally different methods with different contracts.
   - **RESOLVED: `const T&`.** `Tensor<T>::operator()(Idx...) const` returns `const T&` (matches `std::vector::operator[]`); the mutable overload returns `T&`. `eval(linear_idx)` separately returns `T` by value — different contract, intentionally divergent (see header-split rationale).

3. **Should `Tensor<T>` be `static_assert`-restricted to the four locked element types `{float, double, int32_t, int64_t}`, or left open?**
   - What we know: STG-01 says "for `T ∈ {float, double, int32_t, int64_t}`"; the lock is a coverage commitment, not a constraint.
   - What's unclear: A `static_assert` rejecting other types in Phase 1 is more restrictive than the locked spec; an open template lets users instantiate `Tensor<bool>` or `Tensor<std::complex<double>>` with no immediate guarantee they'll work.
   - Recommendation: **Open template.** Phase 1 ships tests for the four locked types; users instantiating others get well-defined behavior for storage operations (vector-like) and may hit failures only when Phase 2 operators encounter operator constraints that don't hold. Adding a `static_assert` would close off `Tensor<bool>` polish in v0.9.0 (POL-01: comparison operators returning `Tensor<bool>`). Leave the surface open; document the four "supported" types in CLAUDE.md.
   - **RESOLVED: NO restriction (open template).** Phase 1 covers `{float, double, int32_t, int64_t}` via tests but `Tensor<T>` carries no `static_assert`. Closing the surface would block v0.9.0 POL-01 (`Tensor<bool>` comparison operators).

4. **Does the `is_base_of_template` helper need a `protected` test path for non-public derivation?**
   - What we know: Phase 1 has only one concrete subclass (`Tensor<T>`), and it derives publicly. Phase 2+ types (`BinaryOp`, `UnaryOp`) will also derive publicly per the canonical CRTP pattern.
   - What's unclear: A `private`-derived class would not satisfy `is_base_of_template_v<Expression, ...>` because the conversion-to-pointer probe is rejected. Is this surprising?
   - Recommendation: **Accept the public-derivation-only contract.** No private CRTP derivation is planned for the milestone; if it ever becomes useful, the helper can grow a friend declaration. Document in expression.h's comment block: "Public derivation from `Expression<Derived>` is required."
   - **RESOLVED: NO.** `is_base_of_template` supports public CRTP derivation only. No private/protected CRTP is planned for v0.8.0; the helper may grow a friend declaration in a later milestone if needed.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| C++20 toolchain (MSVC 17 / GCC 12+ / Apple Clang 14+) | All headers + tests | ✓ (existing CI matrix) | — | — |
| CMake 3.26+ | Build, `add_test` registration, `cmake -P` script execution | ✓ (existing requirement) | 3.26+ | — |
| GoogleTest 1.17.0 | `tests/test_tensor_storage.cpp` | ✓ | 1.17.0 (FetchContent) | — |
| `quiver_compiler_options` INTERFACE target | Test executable warning flags | ✓ | — | — |
| `git` for `git diff --stat` verification | Final phase verification | ✓ | — | — |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** None.

Phase 1 is purely a code/config addition with no new external dependencies. The existing CI matrix (Windows MSVC, Linux GCC + Clang, macOS Apple Clang) already satisfies all C++20 requirements.

---

## Validation Architecture

> Included because `nyquist_validation` is not explicitly disabled in `.planning/config.json` (config.json absent at 2026-05-05; treat as enabled per agent default).

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 (`[VERIFIED: cmake/Dependencies.cmake:56-64]`) |
| Config file | `tests/CMakeLists.txt` — `add_executable(quiver_tests ...)` block (`[VERIFIED: tests/CMakeLists.txt:3-25]`) |
| Quick run command | `ctest -R "Tensor.*" --output-on-failure` (run only tensor tests, exits in <5s) |
| Full suite command | `ctest --output-on-failure` (runs all `quiver_tests` + `quiver_c_tests` if built + `tensor_no_leakage`) |
| Per-test executable | `./build/bin/quiver_tests.exe --gtest_filter="Tensor*"` (Windows) / `./build/bin/quiver_tests --gtest_filter='Tensor*'` (POSIX) |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STG-01 | Construct `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}` with shape and optional fill | unit | `ctest -R "ElementTypes.AllFourTypesConstruct\|TensorConstruction.ShapeOnly\|TensorConstruction.ShapeWithFill" -x` | ❌ Wave 0 |
| STG-02 | Query `shape()`, `size()`, `rank()` | unit | `ctest -R "TensorConstruction\|ZeroDTensor.EmptyShapeHasOneElement" -x` | ❌ Wave 0 |
| STG-03 | Multi-dim indexing `tensor(i, j, k)` | unit | `ctest -R "TensorIndexing\.OperatorParensReadsAndWrites\|TensorIndexing\.AtThrows.*" -x` | ❌ Wave 0 |
| STG-04 | Read raw data via `data() -> const T*` | unit | inline assertion: `EXPECT_EQ(t.data(), &t(0, 0))` for non-empty tensors | ❌ Wave 0 |
| STG-05 | Iterate via `begin()` / `end()` | unit | `ctest -R "TensorIteration.RangeForCompatible" -x` | ❌ Wave 0 |
| EXP-01 | Derive lazy expression types from `Expression<Derived>` providing `shape()`, `size()`, `eval(linear_idx)` via static dispatch | unit + static_assert | `ctest -R "TensorMove.MoveOpsAreNoexcept\|IsTensorExprConcept.AcceptsTensor" -x` | ❌ Wave 0 |
| EXP-02 | `IsTensorExpr` concept matches every type derived from `Expression<Derived>` | static_assert | `static_assert(IsTensorExpr<Tensor<double>>)` + `static_assert(!IsTensorExpr<int>)` inline in `test_tensor_storage.cpp` | ❌ Wave 0 |
| BLD-01 | Tensor framework lives under `include/quiver/tensor/` mirroring `include/quiver/binary/` | structural | `ls include/quiver/tensor/` returns `expression.h shape.h tensor.h` (manual / shell) | N/A — structural, not a runtime test |
| BLD-02 | Per-concern GoogleTest files under `tests/test_tensor_*.cpp` | structural | `ls tests/test_tensor_*.cpp` returns `test_tensor_storage.cpp` | N/A — structural |
| BLD-03 | Tensor sources/tests integrate via `tests/CMakeLists.txt` only — no edits to `src/CMakeLists.txt` | git diff | `git diff --stat src/CMakeLists.txt` returns empty | N/A — verification, not test |
| BLD-04 | CI test enforces include containment | CTest target | `ctest -R tensor_no_leakage` exits 0 in clean tree; FATAL_ERRORs if a `#include <quiver/tensor/...>` is added to any non-tensor public header | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest -R "Tensor\|tensor_no_leakage" --output-on-failure` (covers all Phase 1 tests including BLD-04 leak detection; ~5s)
- **Per wave merge:** `ctest --output-on-failure` (full suite — confirms no regression in existing 21 test files; ~30s on Release)
- **Phase gate:** Full suite green + `git diff --stat include/quiver/c/ include/quiver/c/binary/ bindings/ src/CMakeLists.txt` returns empty before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `tests/test_tensor_storage.cpp` — covers REQ-STG-01..05, EXP-01, EXP-02 (Example 2 above is the canonical content)
- [ ] `cmake/CheckTensorIncludeContainment.cmake` — covers REQ-BLD-04 (Example 3 above is the canonical content)
- [ ] `tests/CMakeLists.txt` amendment — registers both the new test file and the CTest leak-detection target (Example 4 above is the canonical diff)
- [ ] `include/quiver/tensor/expression.h` — `Expression<Derived>` + `IsTensorExpr` + `detail::is_base_of_template_v` (Example 1 above is the canonical content)
- [ ] `include/quiver/tensor/shape.h` — `shape_t` + free functions (Pattern 3 above is the canonical content)
- [ ] `include/quiver/tensor/tensor.h` — `Tensor<T>` value type (Pattern 2 above is the canonical content)
- [ ] (Framework install: NOT NEEDED — GoogleTest 1.17.0 already declared via FetchContent; no new install command.)

**Manual-only verification (justified):**
- BLD-04 negative-path verification (a one-time manual test where the planner adds an offending `#include` to `database.h`, runs `ctest -R tensor_no_leakage`, observes the FATAL_ERROR diagnostic, then reverts). This is a one-shot smoke test of the BLD-04 mechanism itself, not a permanent regression test.
- Pitfall #13 zero-diff check (`git diff --stat`) is a build-pipeline verification, not a code-runtime test.

---

## Sources

### Primary (HIGH confidence)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\CLAUDE.md` — top-level project rules (three error patterns, Pimpl-vs-value-type, CRTP discipline, Cross-Layer Naming)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\PROJECT.md` — v0.8.0 scope, constraints, deferred work
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\REQUIREMENTS.md` — 25 v0.8.0 requirements, traceability
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\ROADMAP.md` — phase ordering rationale, success criteria
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\STATE.md` — current milestone position
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\phases\01-storage-and-crtp-scaffold\01-CONTEXT.md` — 18 locked decisions
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\research\SUMMARY.md` — executive summary, recommended phase structure
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\research\ARCHITECTURE.md` — full pattern catalog (Pattern 1: CRTP base; Pattern 5: shape model; Anti-Pattern 6: noexcept move; Anti-Pattern 7: throwing in eval)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\research\PITFALLS.md` — all 17 pitfalls + 18-item "looks done" checklist (Phase 1 maps to #4, #7, #13, #14)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\research\STACK.md` — "use NONE" verdict for tensor; per-library reasoning
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\research\FEATURES.md` — feature landscape, MVP boundaries, P1/P2/Defer
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\codebase\STRUCTURE.md` — directory layout, "Where to add new code"
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\codebase\ARCHITECTURE.md` — three-layer pattern; tensor placement
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\.planning\codebase\CONVENTIONS.md` — Pimpl-vs-value-type, error patterns, move semantics
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\include\quiver\binary\dimension.h` — header-mostly value-type precedent (24 LOC, Rule of Zero, no Pimpl)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\include\quiver\binary\time_properties.h` — same precedent
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\include\quiver\binary\binary_metadata.h` — value type with serialization
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\include\quiver\database.h:23-29` — `noexcept` move-semantics pattern Tensor<T> mirrors
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\include\quiver\export.h` — QUIVER_API macro (header-only templates do NOT need this)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\cmake\CompilerOptions.cmake:1-30` — `quiver_compiler_options` INTERFACE target
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\cmake\Dependencies.cmake:56-64` — GoogleTest 1.17.0 already declared (no new dep for Phase 1)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\src\CMakeLists.txt` — DO NOT EDIT (header-only, BLD-03 hard rule)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\tests\CMakeLists.txt:3-46` — `quiver_tests` target structure for amendment
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\tests\test_binary_metadata.cpp:1-80` — existing test-file convention (no banner, includes-first, `using namespace quiver`, ============ section dividers)
- `[VERIFIED]` `C:\Development\DatabaseTmp\quiver\CMakeLists.txt:8-12` — C++20 standard locked

### Secondary (MEDIUM confidence — cited references)
- [Wikipedia — Expression templates](https://en.wikipedia.org/wiki/Expression_templates) — canonical pattern, leaf-vs-intermediate storage rule
- [cppreference — CRTP](https://en.cppreference.com/cpp/language/crtp) — language-level reference for the CRTP pattern
- [cppreference — `std::derived_from`](https://en.cppreference.com/w/cpp/concepts/derived_from) — C++20 standard concept (rejected for templated-base detection per "Concept Idiom" reasoning)
- [Eigen — Class hierarchy](https://libeigen.gitlab.io/eigen/docs-nightly/TopicClassHierarchy.html) — `MatrixBase<Derived>` precedent
- [xtensor — Concepts](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/developer/concepts.rst) — `xexpression` CRTP base, value semantics
- [Coding With Thomas — Static, Dynamic Polymorphism, CRTP and C++20 Concepts](https://www.codingwiththomas.com/blog/c-static-dynamic-polymorphism-crtp-and-c20s-concepts) — concept-constrained CRTP
- [Joel Filho — Using C++20 concepts as a CRTP alternative](https://joelfilho.com/blog/2021/emulating_crtp_with_cpp_concepts/) — CRTP alternatives discussion
- [oopscenities — C++20 useful concepts: Requiring type T derived from a base class](https://oopscenities.net/2021/07/13/c20-useful-concepts-requiring-type-t-to-be-derived-from-a-base-class/) — `is_base_of`-constrained concepts
- [iifx.dev — C++ Template Tricks: Solving the CRTP Constraint Problem](https://iifx.dev/en/articles/460217243/c-template-tricks-solving-the-crtp-constraint-problem) — deferred-completeness problem and helper-template approach
- [Modernes C++ — std::is_base_of](https://www.modernescpp.com/index.php/the-type-traits-library-std-is-base-of/) — `std::is_base_of` semantics

### Tertiary (LOW confidence — review-only)
- `[CITED]` Eigen / xtensor / Blaze convergence on `ref_selector` storage policy (Phase 2 concern; informs zero-cost intent for Phase 1 leaf design but not directly applied)
- `[CITED]` GCC `-Wdangling-reference` warning behavior (Phase 2-3 verification, not Phase 1)

---

## Goal-Backward Verification Hooks

What "Phase 1 done" looks like, in concrete commands and grep-checkable artifacts. The planner uses this section to wire the phase plan's success criteria.

### "Done" Definition

Phase 1 is complete when ALL of the following are true (in any order):

1. **Six files exist** at the paths listed in "File List" with the contents structured per "Code Examples".
2. **`./build/bin/quiver_tests.exe` runs all `Tensor*` test cases green** on Windows (and the equivalent on Linux/macOS, per the existing CI matrix). Sampled command: `ctest -R "Tensor.*" --output-on-failure`.
3. **`ctest -R tensor_no_leakage`** runs green on a clean tree.
4. **`ctest -R tensor_no_leakage`** runs RED (FATAL_ERROR) when a `#include <quiver/tensor/tensor.h>` is added to `include/quiver/database.h`. (One-shot manual verification during phase execution.)
5. **`git diff --stat src/CMakeLists.txt`** returns empty.
6. **`git diff --stat include/quiver/c/ include/quiver/c/binary/ bindings/`** returns empty.
7. **`grep -rn 'tensor' include/quiver/ --include='*.h' | grep -v 'include/quiver/tensor/' | grep -v -E ':[0-9]+:\s*//' `** returns empty (no tensor references in any non-tensor public header, after stripping comment lines).
8. **`grep -rn 'namespace quiver' include/quiver/tensor/ | grep -v 'namespace quiver::tensor' | grep -v -E '//[^ ]'`** returns empty (no `quiver::Tensor` at root namespace).
9. **`static_assert(std::is_nothrow_move_constructible_v<quiver::tensor::Tensor<double>>)`** compiles green inside `test_tensor_storage.cpp`.
10. **`static_assert(quiver::tensor::IsTensorExpr<quiver::tensor::Tensor<double>>)`** compiles green inside `test_tensor_storage.cpp`; **`static_assert(!quiver::tensor::IsTensorExpr<int>)`** also compiles green.

### Per-Requirement Verification Mapping

| Req ID | Verification command | Expected output |
|--------|----------------------|-----------------|
| STG-01 | `ctest -R "TensorConstruction\.ShapeOnly\|TensorConstruction\.ShapeWithFill\|TensorConstruction\.ShapeWithInitList\|ElementTypes\.AllFour"` | All green |
| STG-02 | `ctest -R "TensorConstruction"` covers `shape()`, `size()`, `rank()` accessor calls | All green |
| STG-03 | `ctest -R "TensorIndexing"` covers both `operator()` and `at(...)` paths | All green; `at` failures throw with Pattern 1 prefix |
| STG-04 | inline assertion in `TensorConstruction.ShapeWithInitList`: `EXPECT_EQ(t.data()[0], 1.f);` | Green |
| STG-05 | `ctest -R "TensorIteration\.RangeForCompatible"` | Green |
| EXP-01 | (storage test exercises `Expression<Tensor<T>>::self()` indirectly) — `Tensor<T>` derives from `Expression<Tensor<T>>`; verify by `static_assert(std::is_base_of_v<quiver::tensor::Expression<quiver::tensor::Tensor<double>>, quiver::tensor::Tensor<double>>)` | Compile green |
| EXP-02 | `ctest -R "IsTensorExprConcept"` plus `static_assert(IsTensorExpr<Tensor<double>>)` and `static_assert(!IsTensorExpr<int>)` | Green |
| BLD-01 | `ls include/quiver/tensor/` | `expression.h shape.h tensor.h` |
| BLD-02 | `ls tests/test_tensor_*.cpp` | `test_tensor_storage.cpp` |
| BLD-03 | `git diff --stat src/CMakeLists.txt` | empty |
| BLD-04 | `ctest -R tensor_no_leakage` (positive); plus a one-shot manual negative test (add offending include, observe FATAL_ERROR) | Positive green; manual negative test produces FATAL_ERROR with file:line |

### Cross-Phase Discipline Verification (Pitfall #13)

Final task in the phase plan, MUST appear and MUST be a hard gate:

```bash
# Pitfall #13 cross-phase discipline check
git diff --stat include/quiver/c/ include/quiver/c/binary/ \
                 bindings/julia/ bindings/dart/ bindings/python/ bindings/js/

# Expected output: empty (no rows)
# If non-empty: phase 1 has a discipline violation and CANNOT land.
```

If any line above shows `+` or `-` modifications, the planner MUST treat this as a phase-blocking error: roll back the offending commit, identify the cause (likely an accidental tensor reference in a Database/Element/Binary header or a regenerated binding), and resubmit.

### Compile-Time Sanity Check (Pitfall #4 indirect)

While Phase 1 doesn't introduce a formal compile-time-budget regression test (deferred to Phase 5+ per ROADMAP.md), the planner SHOULD spot-check that the phase didn't accidentally inflate `quiver_tests.exe` build time:

```bash
# Before phase 1 commit (record time):
time cmake --build build --target quiver_tests --config Release

# After phase 1 commit:
time cmake --build build --target quiver_tests --config Release
```

The added `test_tensor_storage.cpp` file should add < 5 seconds to the Release build on the slowest CI runner. If it adds > 30 seconds, the planner has either added too many `Tensor<T>` instantiations in tests or accidentally pulled `tensor/*.h` into a hot non-test header — investigate with `clang -ftime-trace` or `cl /d2templateStats` (per Pitfall #4 mitigation).

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — cross-validated against existing tree (cmake/Dependencies.cmake, CompilerOptions.cmake) and STACK.md verdicts
- Architecture: HIGH — Pattern 1, 2, 5 directly cited from ARCHITECTURE.md (HIGH confidence there from Eigen/xtensor/Blaze convergence) and existing Quiver precedent (binary/dimension.h)
- Concept idiom: HIGH — verified `is_base_of_template_v` is the canonical answer via WebSearch; `std::derived_from` insufficient for templated-base detection per cppreference
- Pitfalls: HIGH — full PITFALLS.md catalog already cataloged the four phase-1-relevant items; planner must-haves derived directly from "How to avoid" sections
- File list: HIGH — every NEW/MOD entry verified against existing tree structure and locked decisions D-01..D-18

**Research date:** 2026-05-05
**Valid until:** 2026-06-05 (30-day window for stable scaffolding research; no fast-moving libraries involved)

---

## RESEARCH COMPLETE
