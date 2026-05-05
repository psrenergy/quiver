# Roadmap: Quiver v0.8.0 (Tensor)

**Milestone:** v0.8.0 — Lazy CRTP Expression Template tensor framework
**Created:** 2026-05-05
**Total v0.8.0 requirements:** 25
**Mapped:** 25 / 25 (100%)
**Granularity:** standard (4 phases — see "Why 4 phases" below)

---

## Milestone Goal

Add a header-only, lazy-evaluating tensor framework to Quiver, namespaced under `quiver::tensor`,
mirroring the existing `quiver::binary` subsystem. Pure C++ for v0.8.0: storage + lazy expression
core + element-wise arithmetic with scalar broadcasts + four unary math functions + NumPy-style
shape broadcasting + full-tensor reductions (sum, mean, max). No bridge to `Database`, no C API,
no language bindings — those are deferred to v0.9.0+ per `PROJECT.md`.

## Discipline Check (cross-phase, all phases)

> **Pitfall #13 — bridge designed too early.** v0.8.0 makes **no diff to `include/quiver/c/`**
> and **no diff to `bindings/`**. The tensor namespace is C++-internal for this milestone.
> Every phase's plan MUST verify this invariant before completion (`git diff --stat
> include/quiver/c/ bindings/` shows zero rows touched by tensor work).

This rule is enforced by the success criterion shared by every phase:
"No new symbols in `include/quiver/c/`; no new files under `bindings/`."

## Why 4 Phases

The research summary suggested 5–6 phases (Storage, Arithmetic, Math, Broadcasting,
Reductions, Hardening). For v0.8.0 we collapse to **4** because:

1. **Math functions (REQ-MAT-01) fold into Arithmetic (Phase 2).** Once `UnaryOp` ships
   with negation, `abs`/`exp`/`log`/`sqrt` are one-line additions. Splitting them into
   their own phase produces no new design decisions and adds plan-overhead.
2. **The "Hardening" phase is dropped.** Its three intended deliverables —
   named-UPPERCASE `static_assert` messages (POL-05), the per-file compile-time CI budget
   (POL-08), and a benchmark vs. hand-written loop — are all explicitly deferred to v0.9.0+
   per REQUIREMENTS.md. The remaining hardening concerns (cross-compiler ODR link,
   aliasing regression test, allocation-count test) are absorbed into the success criteria
   of Phase 2 directly.
3. **Diagnostics (REQ-DIA-01) is the three-pattern error format only.** It's enforced
   in every phase that throws, so it doesn't deserve its own phase — it's a constant
   discipline. The diagnostic surface is finalized when Phase 4 ships its empty-tensor
   `max` error.

## Phases

- [ ] **Phase 1: Storage and CRTP Scaffold** — Build the `Tensor<T>` value type, the `Expression<Derived>` CRTP base, and the header/test/CMake wiring under `include/quiver/tensor/`.
- [ ] **Phase 2: Lazy Arithmetic and Materialization** — Ship lazy `BinaryOp`/`UnaryOp` with `+`, `-`, `*`, `/`, unary `-`, scalar broadcasts on both sides, compound assignment, four unary math functions, alias-safe assignment, and the three-pattern error contract.
- [ ] **Phase 3: NumPy-style Broadcasting** — Add `BroadcastView<E>` so non-matching shapes work via the right-align rule with size-1 stretching.
- [ ] **Phase 4: Reductions** — Ship `sum`, `mean`, `max` as full-tensor reductions returning 0-D `Tensor<T>` with NumPy-compatible accumulator promotion and empty-tensor policy.

## Phase Details

### Phase 1: Storage and CRTP Scaffold

**Goal**: User can construct a `quiver::tensor::Tensor<T>`, query its shape/size/rank,
index it by multi-dimensional coordinates, iterate it, and read its raw buffer — and
generic code can identify any tensor expression via the `IsTensorExpr` concept.

**Depends on**: Nothing (first phase of the milestone).

**Requirements**: STG-01, STG-02, STG-03, STG-04, STG-05, EXP-01, EXP-02,
BLD-01, BLD-02, BLD-03, BLD-04

**Success Criteria** (what must be TRUE):
  1. `quiver::tensor::Tensor<T>` exists for `T ∈ {float, double, int32_t, int64_t}`,
     supports construction from a shape (with optional fill), exposes `shape()`,
     `size()`, `rank()`, `data() -> const T*`, `operator()(i, j, ...)`, and
     `begin()`/`end()` iterators (REQ-STG-01..05).
  2. `quiver::tensor::Expression<Derived>` is defined as a CRTP base with `self()`,
     `shape()`, `size()`, `eval(linear_idx)`; `Tensor<T>` derives from it; the
     `IsTensorExpr` concept matches every type derived from `Expression<Derived>`
     (REQ-EXP-01, REQ-EXP-02).
  3. Headers live exclusively under `include/quiver/tensor/` and tests under
     `tests/test_tensor_*.cpp` (`test_tensor_storage.cpp` initially); the `tests/CMakeLists.txt`
     registers them; **no edits land in `src/CMakeLists.txt`** (header-only)
     (REQ-BLD-01, REQ-BLD-02, REQ-BLD-03).
  4. A CI test (CTest target or pre-commit grep) fails if any non-tensor public header
     under `include/quiver/` (e.g. `database.h`, `element.h`, `binary/*.h`) transitively
     pulls in `quiver/tensor/*.h` (REQ-BLD-04, prevents PITFALLS Pitfall #4 — header-only
     template heaviness leaking into the library-wide compile budget).
  5. **Discipline check:** `git diff --stat include/quiver/c/ bindings/` for the phase's
     PR shows zero rows changed (PITFALLS Pitfall #13).

**Plans:** 2 plans

Plans:
**Wave 1**
- [x] 01-01-PLAN.md — Create three foundational tensor headers (expression.h, shape.h, tensor.h) under include/quiver/tensor/ implementing CRTP base, IsTensorExpr concept, and Tensor<T> value type

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 01-02-PLAN.md — Wire Phase 1 verification: GoogleTest unit tests covering STG-01..05 + EXP-01..02, BLD-04 CMake script for include containment, tests/CMakeLists.txt integration

---

### Phase 2: Lazy Arithmetic and Materialization

**Goal**: User can write `Tensor<T> r = a + b * c - d.cast<T>();` (and `+=`, `-=`, `*=`,
`/=`, scalar broadcasts on both sides, four unary math functions) and get a single
fused, alias-safe loop that materializes only into the destination.

**Depends on**: Phase 1.

**Requirements**: EXP-03, ARI-01, ARI-02, ARI-03, ARI-04, MAT-01, ASN-01, ASN-02, DIA-01

**Success Criteria** (what must be TRUE):
  1. User can apply `+`, `-`, `*`, `/` (binary), unary `-`, and `+=`, `-=`, `*=`, `/=`
     between any two `IsTensorExpr` operands of identical shape, with element-type
     promotion via `std::common_type_t`; scalar broadcasts work on both sides for all
     four operators (`a + 2.0` and `2.0 + a`) (REQ-ARI-01..04).
  2. `quiver::tensor::abs`, `exp`, `log`, `sqrt` exist as free functions returning
     `UnaryOp` expressions; each is tested across `{int32_t, int64_t, float, double}`
     where mathematically defined (REQ-MAT-01).
  3. `BinaryOp<L, Op, R>` and `UnaryOp<E, Op>` use the closure-semantics storage policy
     (`detail::ref_selector` — leaves stored by `const&`, intermediates stored by
     value), so chained patterns like `auto e = a + b; auto f = e + d; Tensor<T> r = f;`
     never produce dangling references (REQ-EXP-03; PITFALLS Pitfalls #1, #2).
  4. **Aliasing regression test:** `a = a + a` and `a = expr_using_a` produce
     correct values (`Tensor<T>::operator=(const Expression<E>&)` materializes into a
     temporary buffer and then swaps into `data_`) — `tests/test_tensor_aliasing.cpp`
     covers this on every supported `T` (REQ-ASN-01; PITFALLS Pitfall #3).
  5. `eval(expr) -> Tensor<T>` free function forces materialization at any point in a
     chain (REQ-ASN-02).
  6. All shape-mismatch and type-mismatch failures throw `std::runtime_error` matching
     the Quiver three-pattern format from `CLAUDE.md` (`Cannot {operation}: {reason}`,
     `{Entity} not found: {identifier}`, `Failed to {operation}: {reason}`); the
     concrete shape values appear in shape-mismatch reasons (REQ-DIA-01;
     PITFALLS Pitfall #7).
  7. **Discipline check:** `git diff --stat include/quiver/c/ bindings/` for the phase's
     PR shows zero rows changed (PITFALLS Pitfall #13).

**Plans**: TBD

---

### Phase 3: NumPy-style Broadcasting

**Goal**: User can write `Tensor<double> result = matrix_3x4 + vector_4` (or any other
shape-compatible pair) and get a fused loop that follows the NumPy right-align rule.

**Depends on**: Phase 2 (reuses `ref_selector` storage policy and the materialization
loop from `Tensor<T>::operator=`).

**Requirements**: BCT-01, BCT-02

**Success Criteria** (what must be TRUE):
  1. Binary operators between two expressions of differing shapes apply NumPy's
     right-align rule: shorter shape is left-padded with 1s, size-1 dimensions are
     stretched, mismatched non-1 dimensions throw `Cannot broadcast: shapes [a,b]
     and [c,d] do not align` (REQ-BCT-01).
  2. The 0-D scalar broadcast pathway is unified: `tensor + 1.0`, `tensor +
     Scalar<double>(1.0)`, and (where applicable) `tensor + zero_d_tensor` all produce
     identical results — there are no special cases between scalar entry points
     (REQ-BCT-02; PITFALLS Pitfall #10).
  3. **Broadcast off-by-one regression test** (`tests/test_tensor_broadcast.cpp`):
     ~50 shape combinations across `(scalar, 1D, 2D, 3D)²` including 0-sized dims
     produce results identical to a NumPy oracle (PITFALLS Pitfall #9).
  4. **Broadcast aliasing test:** `a = bcast(b) + a` (and similar patterns where the
     destination overlaps an operand on the broadcast path) produce correct values
     thanks to Phase 2's materialize-via-temporary policy (PITFALLS Pitfall #3 verified
     under broadcasting).
  5. **Discipline check:** `git diff --stat include/quiver/c/ bindings/` for the phase's
     PR shows zero rows changed (PITFALLS Pitfall #13).

**Research flag:** YES — run `/gsd-research-phase` before planning. The NumPy spec has
corner cases (size-1 vs missing dim, 0-sized dims, 0-D scalar) that have produced
real-world bugs in xtensor (PR #1575, issue #907). The phase plan should fix the
oracle-test setup and the canonical 50-combination matrix before any code is written.

**Plans**: TBD

---

### Phase 4: Reductions

**Goal**: User can write `auto total = sum(a + b * c).item();` (and `mean`, `max`)
and get a 0-D `Tensor<T>` whose accumulator type prevents overflow on integer inputs
and whose empty-tensor behavior follows the documented NumPy-compatible policy.

**Depends on**: Phase 3 (the 0-D result tensor reuses the broadcast scalar machinery;
rank-shrinking arithmetic is shared).

**Requirements**: RED-01, RED-02, RED-03

**Success Criteria** (what must be TRUE):
  1. `sum(expr)`, `mean(expr)`, `max(expr)` return a 0-D `Tensor<T>` extractable via
     `.item()`; results match a hand-written reduction over the same expression
     (REQ-RED-01).
  2. Accumulator types are NumPy-compatible: `sum<int8_t>` and `sum<int32_t>`
     accumulate in `int64_t`; `sum<float>` accumulates in `double`; `mean<int>`
     returns floating-point even from integer input. Overflow regression test runs
     near `INT_MAX` (REQ-RED-02; PITFALLS Pitfall #11).
  3. Empty-tensor policy is enforced: `sum({}) = 0`, `mean({}) = NaN`, `max({})`
     throws `Cannot max: tensor is empty` (the Quiver three-pattern format)
     (REQ-RED-03; finalizes REQ-DIA-01 for reductions; PITFALLS Pitfall #12).
  4. **Reduce-into-self alias test:** `Tensor<T> a = ...; a = sum(a + 1.0);`
     produces the correct scalar (PITFALLS Pitfall #3 verified under reductions).
  5. **Discipline check:** `git diff --stat include/quiver/c/ bindings/` for the phase's
     PR shows zero rows changed (PITFALLS Pitfall #13).

**Research flag:** YES — run `/gsd-research-phase` before planning. Accumulator
promotion rules (NumPy / pandas / xtensor / Eigen disagree at the edges), NaN policy,
empty-tensor policy, and the lazy-node-eager-cache trade-off (Pattern 7 from
ARCHITECTURE.md) all need a documented reference for the plan.

**Plans**: TBD

---

## Phase Ordering Rationale

- **Phase 1 → Phase 2:** `Tensor<T>` storage and `Expression<Derived>` CRTP base must
  exist before any operator can derive from them. `IsTensorExpr` concept and the
  `quiver::tensor` namespace must lock in Phase 1 (HIGH cost-of-late-recovery for
  PITFALLS Pitfalls #7, #14).
- **Phase 2 → Phase 3:** `BroadcastView<E>` reuses Phase 2's `ref_selector` storage
  policy and Phase 2's materialize-via-temporary assignment loop. Building broadcasting
  before Phase 2 produces conflicting assumptions about how operands are stored.
- **Phase 3 → Phase 4:** A full reduction returns a 0-D `Tensor<T>` that *is* a 0-D
  scalar broadcast operand — exactly the surface Phase 3 finalized. The rank arithmetic
  for "drop axis k → rank-(N-1) result" is mechanically identical to Phase 3's
  shape-padding logic.
- **REQ-MAT-01 placement (inside Phase 2):** mechanical given Phase 2's `UnaryOp`
  pattern; one line per math function, one test per function per supported `T`.
  Splitting it into its own phase produces no design decisions.
- **REQ-DIA-01 spans the milestone:** error messages start in Phase 2 (the first
  thrown error — shape mismatch on `BinaryOp` construction), expand in Phase 3
  (broadcast incompatibility), finalize in Phase 4 (empty-tensor `max`). It is not
  a phase of its own; the three-pattern format is a constant discipline.
- **REQ-BLD-* lives entirely in Phase 1:** header layout, CMake wiring, test naming,
  include containment all decided at framework birth. The downstream phases only
  *add* test files following the convention — they do not revisit the layout.

## Out-of-Scope Reminder (do not add to any phase)

These were explicitly deferred to v0.9.0+ per REQUIREMENTS.md:

- Element/Database bridge (`Database::read_tensor`, `Element::set(Tensor)`) — BRG-01..03
- Tensor C API (`quiver_tensor_*`) — CAPI-01..02
- Language bindings (Julia/Dart/Python/JS/Lua) — BIND-01..05
- Single-axis and multi-axis reductions — RED-04..06
- Comparison operators with `Tensor<bool>`, mutable `data() -> T*`, static-rank
  `Tensor<T, N>`, named UPPERCASE `static_assert` messages, compile-time-budget CI
  test, `to_string` / `operator<<`, slicing/views/concat/reshape, trig/rounding/
  power/clip — POL-01..08
- Autograd, GPU, sparse, BLAS-backed linalg, convolutions — out of project scope

## Progress Table

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Storage and CRTP Scaffold | 2/2 | Ready for verify | — |
| 2. Lazy Arithmetic and Materialization | 0/? | Not started | — |
| 3. NumPy-style Broadcasting | 0/? | Not started | — |
| 4. Reductions | 0/? | Not started | — |

## Coverage

| Phase | REQ-IDs | Count |
|-------|---------|-------|
| 1 | STG-01, STG-02, STG-03, STG-04, STG-05, EXP-01, EXP-02, BLD-01, BLD-02, BLD-03, BLD-04 | 11 |
| 2 | EXP-03, ARI-01, ARI-02, ARI-03, ARI-04, MAT-01, ASN-01, ASN-02, DIA-01 | 9 |
| 3 | BCT-01, BCT-02 | 2 |
| 4 | RED-01, RED-02, RED-03 | 3 |
| **Total** | | **25 / 25** |

No orphan REQs. No phase without REQs. Cross-cutting REQ-DIA-01 is owned by Phase 2
(where the error contract is established) and verified in Phases 3 and 4.

---

*Roadmap created: 2026-05-05 — first GSD-tracked milestone for Quiver*
