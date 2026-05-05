# Requirements: Quiver v0.8.0 (Tensor)

**Defined:** 2026-05-05
**Core Value:** One C++ codebase, identical behavior across every language binding — logic, error messages, and validation live in C++; bindings stay thin.
**Milestone Goal:** Add a header-only, lazy-evaluating tensor framework (Expression Templates with CRTP) to Quiver — pure C++ for v0.8.0, ready to bridge to Quiver Elements in a follow-up milestone.

---

## v0.8.0 Requirements

Requirements for the v0.8.0 milestone. Each maps to exactly one roadmap phase.

### Storage (STG)

- [x] **STG-01**: User can construct `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}` with a shape and optional fill value *(Phase 01 Plan 01)*
- [x] **STG-02**: User can query a tensor's `shape()`, `size()`, and `rank()` *(Phase 01 Plan 01)*
- [x] **STG-03**: User can index a tensor element by multi-dimensional coordinates (`tensor(i, j, k)`) *(Phase 01 Plan 01)*
- [x] **STG-04**: User can read raw contiguous data via `data() -> const T*` *(Phase 01 Plan 01)*
- [x] **STG-05**: User can iterate a tensor's elements via `begin()` / `end()` (range-for compatible) *(Phase 01 Plan 01)*

### Expression Core (EXP)

- [x] **EXP-01**: User can derive lazy expression types from `Expression<Derived>` — CRTP base providing `shape()`, `size()`, and `eval(linear_idx)` via static dispatch *(Phase 01 Plan 01)*
- [x] **EXP-02**: Generic code can constrain "is a tensor expression" via the `IsTensorExpr` concept (used by operator overloads and free functions to give readable error messages) *(Phase 01 Plan 01)*
- [ ] **EXP-03**: Lazy expressions hold their operands per closure semantics — leaves (`Tensor<T>`) by `const&`, intermediates by value — preventing dangling references in chained-expression patterns like `auto e = a + b; auto f = e + d;`

### Arithmetic (ARI)

- [ ] **ARI-01**: User can apply element-wise binary operators (`+`, `-`, `*`, `/`) between two expressions of identical (or broadcast-compatible — see BCT) shape, with `std::common_type_t` element-type promotion
- [ ] **ARI-02**: User can apply unary negation (`-expr`) to any tensor expression
- [ ] **ARI-03**: User can broadcast a scalar against a tensor expression on either side (`a + 2.0`, `2.0 + a`) for all four arithmetic operators
- [ ] **ARI-04**: User can use compound assignment operators (`+=`, `-=`, `*=`, `/=`) on a `Tensor<T>` against any compatible expression

### Math Functions (MAT)

- [ ] **MAT-01**: User can call `abs(expr)`, `exp(expr)`, `log(expr)`, `sqrt(expr)` as free functions in `quiver::tensor`, each returning a lazy `UnaryOp` expression

### Broadcasting (BCT)

- [ ] **BCT-01**: User can apply binary operators between expressions of differing shapes following NumPy right-align rules (shorter shape left-padded with 1s, size-1 dimensions stretched, mismatched non-1 dimensions throw)
- [ ] **BCT-02**: 0-D scalar broadcasts uniformly across `tensor + 1.0`, `tensor + Scalar<double>(1.0)`, and any future 0-D tensor result (no special-case discrepancies between scalar entry points)

### Reductions (RED)

- [ ] **RED-01**: User can call `sum(expr)`, `mean(expr)`, `max(expr)` returning full-tensor reductions as 0-D `Tensor<T>` (extractable via `.item()`)
- [ ] **RED-02**: Reductions promote accumulator types to prevent overflow and integer-division surprises — `sum<int8_t>` accumulates in `int64_t`, `sum<float>` accumulates in `double`, `mean<int>` returns floating-point even from integer input (NumPy-compatible)
- [ ] **RED-03**: Reductions on empty tensors follow defined policy — `sum({}) = 0`, `mean({}) = NaN`, `max({})` throws `Cannot max: tensor is empty` (Quiver three-pattern error)

### Assignment & Materialization (ASN)

- [ ] **ASN-01**: `Tensor<T>::operator=(const Expression<E>&)` materializes the source expression into a temporary buffer first, then swaps into the destination — guaranteeing alias safety for `a = a + a` and `a = expr_using_a` patterns
- [ ] **ASN-02**: User can force materialization at any point via `eval(expr) -> Tensor<T>` free function

### Diagnostics (DIA)

- [ ] **DIA-01**: All tensor errors follow Quiver's three-pattern rule — `Cannot {operation}: {reason}` for preconditions, `{Entity} not found: {identifier}` where applicable, `Failed to {operation}: {reason}` for runtime failures — with shape-mismatch reasons including the actual shapes (e.g. `Cannot broadcast: shapes [3,4] and [3,5] do not align`)

### Build & Test Harness (BLD)

- [x] **BLD-01**: Tensor framework lives under `include/quiver/tensor/` mirroring the existing `include/quiver/binary/` precedent (header-only, no `src/tensor/*.cpp` translation unit unless non-template helpers accumulate) *(Phase 01 Plan 01)*
- [ ] **BLD-02**: Per-concern GoogleTest files under `tests/test_tensor_*.cpp` matching the existing `test_database_*.cpp` / `test_binary_*.cpp` naming convention (`test_tensor_storage.cpp`, `test_tensor_arithmetic.cpp`, `test_tensor_broadcast.cpp`, `test_tensor_reduce.cpp`, `test_tensor_aliasing.cpp`, `test_tensor_adl.cpp`)
- [ ] **BLD-03**: New tensor sources/tests integrate via `tests/CMakeLists.txt` only — no edits to `src/CMakeLists.txt` (header-only); existing `quiver_compiler_options` (`/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic` on GCC/Clang) applies as-is
- [ ] **BLD-04**: A CI test enforces include containment — no public Quiver header outside `include/quiver/tensor/` (i.e. `quiver/database.h`, `quiver/element.h`, `quiver/binary/*.h`, etc.) transitively pulls in `quiver/tensor/*.h` (prevents library-wide compile-time regression)

---

## v0.9.0+ Requirements (Deferred)

Tracked but not in v0.8.0 roadmap.

### Element/Database Bridge
- **BRG-01**: `Database::read_tensor(collection, attribute)` returns `Tensor<double>` from a vector group
- **BRG-02**: `Element::set("attr", const Tensor<T>&)` inserts a tensor as vector/group data
- **BRG-03**: Round-trip Tensor ↔ Element ↔ Database

### C API (Tensor)
- **CAPI-01**: `quiver_tensor_*` immediate-mode wrappers (each op materializes; bindings see Tensor handles only)
- **CAPI-02**: Alloc/free co-located in `src/c/tensor/` per existing C API discipline

### Bindings (Tensor)
- **BIND-01**: Julia (`Quiver.jl`) tensor wrapper
- **BIND-02**: Dart (`quiverdb`) tensor wrapper
- **BIND-03**: Python (`quiverdb`) tensor wrapper
- **BIND-04**: JS/Deno (`@psrenergy/quiver`) tensor wrapper
- **BIND-05**: Lua tensor binding via sol2

### Reduction Extras
- **RED-04**: Single-axis reductions — `sum(t, axis)`, `mean(t, axis)`, `max(t, axis)` returning rank-shrunk tensors with NumPy negative-axis support
- **RED-05**: Multi-axis reductions — `sum(t, {0, 2})`
- **RED-06**: Reduction additions — `prod`, `min`, `argmin`, `argmax`, `nansum`, `nanmean`

### Tensor Polish
- **POL-01**: Comparison operators (`<`, `>`, `==`, `!=`) returning `Tensor<bool>` + `where` ternary + `any`/`all`
- **POL-02**: Mutable `data() -> T*` accessor (currently `const T*` only)
- **POL-03**: Static-rank `Tensor<T, N>` with `std::array<size_t, N>` shape (compile-time rank for hot loops)
- **POL-04**: `to_string` / `operator<<` for `Tensor<T>` with truncation
- **POL-05**: Named UPPERCASE `static_assert` messages for grep-able template diagnostics
- **POL-06**: Reshape, transpose, slicing/views (`xt::view`-equivalent), `concat`, `stack`
- **POL-07**: Trig/rounding/power/clip — `sin`, `cos`, `floor`, `ceil`, `pow`, `clip`
- **POL-08**: CI compile-time budget regression test

---

## Out of Scope

Explicitly excluded from v0.8.0 AND from current Quiver roadmap. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| **Autograd / autodiff** | Would require a tape mechanism on top of the lazy graph. Not in roadmap; out of project scope. |
| **GPU / device dispatch** | CPU-only by design. Out of scope at the project level for v0.8.0+. |
| **Sparse tensors** | Would require a separate Expression hierarchy and storage model. No demand from current users. |
| **BLAS-backed linear algebra** (`matmul`, decompositions, einsum) | Quiver tensor is a *lazy element-wise* framework, not a numerical-algebra library. Use Eigen/Blaze if linalg is needed alongside. |
| **Convolutions** | ML/DL territory; not aligned with Quiver's data-management focus. |
| **`<mdspan>` adoption** | C++23 baseline not committed; MSVC/Apple Clang lag. Revisit after Quiver bumps to C++23. |
| **SIMD libraries (Highway, xsimd)** | Premature coupling for v0.8.0; future SIMD milestone may revisit. Keep CRTP evaluator-agnostic. |
| **Renaming `Binary*` to `Tensor*`** | Existing binary subsystem keeps current names. Tensor is a parallel namespace, not a rename. |
| **Parallel reductions (`std::execution::par`)** | Apple Clang libc++ doesn't ship parallel policies; would break macOS CI. Defer to a future perf milestone. |
| **Eigen / xtensor / Blaze as a dependency** | All three rejected per STACK.md research — scope mismatch (Eigen/Blaze are linalg-shaped), transitive deps (xtensor/xtl), or staleness (Blaze 6 years old). Quiver tensor is ground-up. |
| **Boost dependencies** | Quiver is Boost-free; not changing for one tensor framework. |

---

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| STG-01 | Phase 1 | Pending |
| STG-02 | Phase 1 | Pending |
| STG-03 | Phase 1 | Pending |
| STG-04 | Phase 1 | Pending |
| STG-05 | Phase 1 | Pending |
| EXP-01 | Phase 1 | Pending |
| EXP-02 | Phase 1 | Pending |
| EXP-03 | Phase 2 | Pending |
| ARI-01 | Phase 2 | Pending |
| ARI-02 | Phase 2 | Pending |
| ARI-03 | Phase 2 | Pending |
| ARI-04 | Phase 2 | Pending |
| MAT-01 | Phase 2 | Pending |
| BCT-01 | Phase 3 | Pending |
| BCT-02 | Phase 3 | Pending |
| RED-01 | Phase 4 | Pending |
| RED-02 | Phase 4 | Pending |
| RED-03 | Phase 4 | Pending |
| ASN-01 | Phase 2 | Pending |
| ASN-02 | Phase 2 | Pending |
| DIA-01 | Phase 2 | Pending |
| BLD-01 | Phase 1 | Pending |
| BLD-02 | Phase 1 | Pending |
| BLD-03 | Phase 1 | Pending |
| BLD-04 | Phase 1 | Pending |

**Coverage:**
- v0.8.0 requirements: 25 total
- Mapped to phases: 25
- Unmapped: 0 ✓

**Phase distribution:**
- Phase 1 (Storage and CRTP Scaffold): 11 requirements (STG-01..05, EXP-01, EXP-02, BLD-01..04)
- Phase 2 (Lazy Arithmetic and Materialization): 9 requirements (EXP-03, ARI-01..04, MAT-01, ASN-01, ASN-02, DIA-01)
- Phase 3 (NumPy-style Broadcasting): 2 requirements (BCT-01, BCT-02)
- Phase 4 (Reductions): 3 requirements (RED-01, RED-02, RED-03)

---
*Requirements defined: 2026-05-05*
*Last updated: 2026-05-05 — traceability filled by roadmap creation (4 phases, 25/25 mapped)*
