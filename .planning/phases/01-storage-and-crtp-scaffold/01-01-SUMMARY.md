---
phase: 01-storage-and-crtp-scaffold
plan: 01
subsystem: tensor
tags: [crtp, expression-templates, c++20-concepts, header-only, value-type, rule-of-zero]

# Dependency graph
requires: []
provides:
  - "include/quiver/tensor/shape.h: shape_t alias + compute_strides + total_size + ravel_index free functions"
  - "include/quiver/tensor/expression.h: Expression<Derived> CRTP base + IsTensorExpr concept + detail::is_base_of_template_v helper"
  - "include/quiver/tensor/tensor.h: Tensor<T> value type with 3 ctors, indexing, iteration, .item(), eval(), CRTP inheritance"
  - "Single-source-of-truth shape_t topology (declared once in shape.h)"
  - "Locked Pattern 1 error contracts for tensor construction and access"
affects: [phase-01-plan-02, phase-02-arithmetic, phase-03-broadcasting, phase-04-reductions]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CRTP base class (Expression<Derived>) — first in-tree precedent"
    - "C++20 concept-driven type filtering (IsTensorExpr) — first in-tree precedent"
    - "is_base_of_template_v helper (public-derivation-only probe via implicit-conversion-to-pointer)"
    - "Header-only value type with explicit noexcept move (Anti-Pattern #6 prevention)"
    - "Single-source-of-truth alias topology (shape_t in shape.h, transitively obtained by sibling headers)"
    - "Sink-parameter idiom (Pitfall E) on all three Tensor<T> constructors"
    - "Pattern 1 error format only for Phase 1 throws (Cannot {op}: {reason})"

key-files:
  created:
    - "include/quiver/tensor/shape.h (37 LOC)"
    - "include/quiver/tensor/expression.h (52 LOC)"
    - "include/quiver/tensor/tensor.h (129 LOC)"
  modified: []

key-decisions:
  - "shape.h is the single source of truth for shape_t (WARNING-02 / NOTE-05 fix); expression.h and tensor.h #include it rather than redeclare"
  - "expression.h trims includes to <cstddef>, <type_traits>, <utility> — no <vector>, no <concepts> (NOTE-05)"
  - "All three tensor headers nest exclusively in namespace quiver::tensor — zero root-namespace leak (Pitfall #14)"
  - "Tensor<T> is COPYABLE with noexcept move = default (value type, not resource handle); diverges from Database non-copyable resource pattern"
  - "operator() returns const T& on const overload (Q2 RESOLVED), matches std::vector::operator[] semantics"
  - "Tensor<T> is OPEN template (Q3 RESOLVED): no static_assert restricting T to {float, double, int32_t, int64_t}"
  - "is_base_of_template uses public-derivation-only probe (Q4 RESOLVED): no friend or alternative probe for v0.8.0"
  - "compute_strides uses multiplication only (Pitfall B) — division-by-zero on 0-sized dims structurally impossible"
  - "ravel_index empty fold expression (Pitfall C) — 0-D tensor with 0 args yields linear index 0"

patterns-established:
  - "CRTP base in include/quiver/tensor/expression.h — Phase 2 BinaryOp/UnaryOp will inherit publicly via : public Expression<Derived>"
  - "IsTensorExpr concept gates Phase 2 operator overloads (template <class L, class R> requires (IsTensorExpr<L> && IsTensorExpr<R>))"
  - "Pattern 1 error format (Cannot {op}: {reason}) for all Phase 1 throws — Phase 2 broadcasting failures continue this pattern"
  - "Cross-phase discipline gate: zero diff to include/quiver/c/, include/quiver/c/binary/, bindings/, src/CMakeLists.txt — Phase 1 Plan 02 will add a CTest target enforcing this permanently (BLD-04)"

requirements-completed: [STG-01, STG-02, STG-03, STG-04, STG-05, EXP-01, EXP-02, BLD-01]

# Metrics
duration: 6min
completed: 2026-05-05
---

# Phase 1 Plan 01: Storage and CRTP Scaffold Summary

**Three header-only files under `include/quiver/tensor/` establishing the public API anchor for milestone v0.8.0: `Tensor<T>` value type with three constructors and full indexing/iteration surface, `Expression<Derived>` CRTP base, and the `IsTensorExpr` concept — all ready for Plan 02's verification test file.**

## Performance

- **Duration:** 6 min 9 sec
- **Started:** 2026-05-05T21:31:12Z
- **Completed:** 2026-05-05T21:37:22Z
- **Tasks:** 3 / 3
- **Files created:** 3 (218 LOC total: 37 + 52 + 129)
- **Files modified:** 0

## Accomplishments

- `quiver::tensor::Tensor<T>` value type with three constructors (zero-init, fill, initializer_list with size validation), indexing via unchecked `operator()` and bounds-checked `at()`, iterator surface forwarding to `data_`, `.item()` for 0-D extraction, and `eval(linear_idx)` returning `T` by value
- `Expression<Derived>` CRTP base with `self()` / `size()` / `shape()` / `rank()` / `eval()` forwarding methods and protected default constructor
- `IsTensorExpr` C++20 concept built on `detail::is_base_of_template_v`, the public-derivation-only probe — strict CRTP-derivation semantics that reject duck-typed types (Anti-Pattern #3 prevention)
- Single-source-of-truth `shape_t` topology: `using shape_t = std::vector<std::size_t>;` declared once in `shape.h`, obtained transitively by `expression.h` and `tensor.h` via `#include "shape.h"` — eliminates the typedef-redeclaration warning under `/W4 /permissive-` (WARNING-02)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create `include/quiver/tensor/shape.h`** — `4710f56` (feat)
2. **Task 2: Create `include/quiver/tensor/expression.h`** — `511884d` (feat)
3. **Task 3: Create `include/quiver/tensor/tensor.h`** — `33fba76` (feat)

## Files Created/Modified

**Created:**
- `include/quiver/tensor/shape.h` (37 LOC) — `shape_t` alias (single source of truth) + 3 inline header-only free functions (`compute_strides`, `total_size`, `ravel_index`) under `namespace quiver::tensor`. No `.cpp` companion.
- `include/quiver/tensor/expression.h` (52 LOC) — `Expression<Derived>` CRTP base (protected default ctor, `self()` / `size()` / `shape()` / `rank()` / `eval()` forwarding), `detail::is_base_of_template_v` helper (public-derivation-only probe), `IsTensorExpr` concept on top of `std::remove_cvref_t<T>`. Includes `shape.h` for `shape_t`; trimmed stdlib to `<cstddef>` + `<type_traits>` + `<utility>` only.
- `include/quiver/tensor/tensor.h` (129 LOC) — `Tensor<T> : public Expression<Tensor<T>>` value type. Three constructors with sink-parameter `shape_t shape` by value (Pitfall E). Defaulted copy + explicit `noexcept = default` move (Anti-Pattern #6 prevention). `operator()` unchecked (mutable `T&` / const `const T&` per Q2). `at()` bounds-and-rank-checked, throws Pattern 1. `data() -> const T*` only (D-10). `begin()` / `end()` forward to `data_`. `.item()` and `eval(linear_idx) -> T` by value.

**Modified:** None — header-only opt-in design; no edits to `src/CMakeLists.txt`, `include/quiver/quiver.h`, or any other public header.

## Decisions Made

All 14 of the locked decisions from `01-CONTEXT.md` (D-01..D-14) and all four resolved Open Questions from `01-RESEARCH.md` (Q1..Q4) were honored verbatim:

- **D-01..D-05** (constructor surface): three ctors only, zero-init default, 0-sized dims valid, initializer-list size validation throwing Pattern 1
- **D-06..D-10** (indexing): unchecked `operator()`, checked `at()`, `std::size_t` indices, `begin/end` forwarding, `data() -> const T*` only
- **D-11..D-13** (0-D tensors): empty shape representation, `.item()` shipping with Pattern 1 error, no named factory
- **D-14** (concept): strict CRTP-derivation `IsTensorExpr`
- **Q1** (RESOLVED — accept): three constructors only, no iterator-range / vector-rvalue ctors
- **Q2** (RESOLVED — `const T&`): const `operator()` returns reference, mutable returns mutable reference; `eval(linear_idx)` separately returns `T` by value (intentional contract divergence for Phase 2+ intermediate nodes)
- **Q3** (RESOLVED — open template): no `static_assert` restricting `T`; verified by grep returning 0
- **Q4** (RESOLVED — public CRTP only): `is_base_of_template` probe relies on implicit-conversion-to-`const Base<U>*`, naturally rejected by language for non-public derivation; documented via inline comment

Beyond what the plan locked, no additional decisions were made. The plan provided exact-content code blocks for all three files; they were transcribed verbatim with no improvisation.

## Verification Results

All 7 verification-block checks from the plan passed:

| Check | Expected | Result |
|-------|----------|--------|
| 1. `cmake --build build --config Debug --target quiver` exit code | 0 | 0 (no work to do — tensor headers not transitively pulled in by `quiver` target, as designed) |
| 3. Cross-phase discipline (`git diff --stat` of 7 protected paths) | empty | empty |
| 4. Pitfall #4 include-leakage (`grep -rn 'quiver/tensor' include/quiver/ --include='*.h'`) | only tensor-internal | none (tensor headers use relative includes, no `quiver/tensor` strings appear anywhere) |
| 5. Pitfall #14 namespace hygiene (root `namespace quiver {` in tensor headers) | empty | empty |
| 6. WARNING-02 `using shape_t` count (expression.h, shape.h, tensor.h) | 0, 1, 0 | 0, 1, 0 |
| 7. Files NOT touched (`git diff --stat src/CMakeLists.txt include/quiver/quiver.h ...`) | empty | empty |

**Smoke compile test (extra, not required by plan):** authored a synthetic TU including `<quiver/tensor/tensor.h>` and exercising `static_assert(std::is_base_of_v<Expression<Tensor<double>>, Tensor<double>>)`, `static_assert(IsTensorExpr<Tensor<double>>)`, `static_assert(!IsTensorExpr<int>)`, `static_assert(std::is_nothrow_move_constructible_v<Tensor<double>>)`, `static_assert(std::is_nothrow_move_assignable_v<Tensor<double>>)`. All passed under `g++ -std=c++20 -Wall -Wextra -Wpedantic -fsyntax-only`. Synthetic file deleted after verification — Plan 02 will wire the production GoogleTest TU.

## Pitfalls Mitigated

- **Pitfall #4** (header-only template heaviness leaking into library compile budget): no `quiver/tensor/*.h` include appears in any non-tensor header under `include/quiver/`. The umbrella `include/quiver/quiver.h` is unmodified.
- **Pitfall #13** (bridge designed too early): zero diff to `include/quiver/c/`, `include/quiver/c/binary/`, `bindings/`, `src/CMakeLists.txt`, `cmake/Dependencies.cmake`, `cmake/CompilerOptions.cmake`, top-level `CMakeLists.txt`.
- **Pitfall #14** (namespace hygiene): all symbols nest under `quiver::tensor`; helper traits under `quiver::tensor::detail`. No leakage into root `quiver::` namespace.
- **Pitfall B** (division-by-zero on 0-sized dims in stride math): `compute_strides` uses multiplication only — verified by `awk '/inline shape_t compute_strides/,/^}$/' shape.h | grep -c '/'` returning 0.
- **Pitfall C** (empty fold in `ravel_index` for 0-D tensors): the C++17 fold `((result += idxs * strides[i++]), ...)` expands to nothing when 0 indices supplied, leaving `result = 0` — exactly what 0-D `t()` needs.
- **Pitfall E** (sink-parameter idiom): all three `Tensor<T>` constructors take `shape_t shape` by value, then `shape_(std::move(shape))` member-init.
- **Anti-Pattern #6** (silent copy-on-grow when stored in `std::vector<Tensor<T>>`): both move ops are explicitly `noexcept = default`. Verified by smoke `static_assert`.
- **WARNING-02 / NOTE-05** (duplicate `shape_t` alias declaration): declared once in `shape.h`, transitively obtained elsewhere; no redundant `<vector>` or `<concepts>` includes.

## Deviations from Plan

None — plan executed exactly as written. Every code block in the three task `<action>` sections was transcribed verbatim. No bug fixes, no missing functionality auto-added, no architectural changes required.

## Issues Encountered

None. The plan provided exact, locked content for every header; transcription was mechanical and the build verified clean on every commit.

## User Setup Required

None — pure C++ header-only library code. No external services, no environment variables, no dashboards.

## Next Phase Readiness

**Plan 02 prerequisites are met:**
- `include/quiver/tensor/{shape, expression, tensor}.h` exist and compile standalone
- Public API surface is locked for Plan 02's `tests/test_tensor_storage.cpp`:
  - `quiver::tensor::Tensor<T>` (three ctors, accessors, indexing both unchecked and checked, iterators, `.item()`, `eval()`)
  - `quiver::tensor::Expression<Derived>` CRTP base
  - `quiver::tensor::IsTensorExpr` concept
  - `quiver::tensor::compute_strides`, `total_size`, `ravel_index` free functions
  - All four locked Pattern 1 error messages embedded with exact wording
- `static_assert(std::is_nothrow_move_constructible_v<Tensor<double>>)` and `static_assert(IsTensorExpr<Tensor<double>>)` will pass when Plan 02 wires the test
- Cross-phase discipline gate is at-baseline (zero diff in protected paths) — Plan 02 will add the BLD-04 CTest target as a permanent enforcement mechanism

**No blockers.** Phase 2 (lazy arithmetic) can begin once Plan 02 ships its verification harness.

---

## Self-Check: PASSED

**Files claimed → verified:**
- `include/quiver/tensor/shape.h` — FOUND (37 LOC)
- `include/quiver/tensor/expression.h` — FOUND (52 LOC)
- `include/quiver/tensor/tensor.h` — FOUND (129 LOC)

**Commits claimed → verified:**
- `4710f56` (Task 1: shape.h) — FOUND in git log
- `511884d` (Task 2: expression.h) — FOUND in git log
- `33fba76` (Task 3: tensor.h) — FOUND in git log

---
*Phase: 01-storage-and-crtp-scaffold*
*Completed: 2026-05-05*
