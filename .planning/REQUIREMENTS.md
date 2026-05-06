# Requirements: Quiver — Lazy Expressions

**Defined:** 2026-05-05
**Last restructured:** 2026-05-06 — milestone scope narrowed to C++ core; CAPI / bindings / TEST-02 / TEST-03 deferred to a future milestone.
**Core Value (v1.0):** Compose arithmetic on `.qvr` binary files in C++ with `(a + b) * 2.0`-style ergonomics, materialized lazily row-by-row via `expression.save(path)`.
**Future-milestone goal:** Same ergonomics in Julia, Dart, Python, JS/Deno, and Lua via the C API + per-language bindings (preserved verbatim under "Deferred to Future Milestone" below).

## v1 Requirements

### Core (C++ namespace `quiver::expr`)

- [ ] **CORE-01**: User can construct an `Expression` from an open `BinaryFile` via implicit conversion
- [ ] **CORE-02**: User can compose two Expressions with `+`, `-`, `*`, `/` to produce a new lazy Expression
- [ ] **CORE-03**: User can use scalar literals on either side of an operator (`a + 2.0`, `2.0 * a`) and the scalar broadcasts to the sibling's row shape
- [ ] **CORE-04**: User can chain operators into trees of arbitrary depth (`((a + b) - c) / 2.0`) and the tree materializes correctly
- [ ] **CORE-05**: User can call `expression.save(path)` to materialize the lazy tree to a new `.qvr` file
- [ ] **CORE-06**: Materialization iterates row-by-row (one `vector<double>` per row) reusing per-row buffers across iterations

### Verification

- [ ] **TEST-01**: C++ test suite verifies arithmetic correctness end-to-end (write inputs, evaluate `(a + b) * 2.0`-style expressions, read output, golden values match)

## Deferred to Future Milestone

These requirements were originally part of v1 but were moved out of scope on 2026-05-06 when phases 2–7 were removed. They remain valid future-milestone targets — re-add the corresponding phases to ROADMAP.md before re-engaging.

### C API (`quiver_expression_*`)

- [ ] **CAPI-01**: C API exposes opaque `quiver_expression_t` handle with `_free` lifecycle
- [ ] **CAPI-02**: C API exposes `quiver_expression_from_file` plus four binary ops (`add`, `sub`, `mul`, `div`) and four scalar variants (`add_scalar`, `sub_scalar`, `mul_scalar`, `div_scalar`) with both scalar-on-left and scalar-on-right where applicable
- [ ] **CAPI-03**: C API exposes `quiver_expression_save` returning `quiver_error_t`, with messages retrievable via `quiver_get_last_error`

### Bindings (one per language, identical capability surface)

- [ ] **BIND-01**: Julia binding exposes `Expression` with native `+ - * /` operators and `save(expr, path)` function
- [ ] **BIND-02**: Dart binding exposes `Expression` with native `+ - * /` operators and `save(path)` method
- [ ] **BIND-03**: Python binding exposes `Expression` with `__add__` / `__sub__` / `__mul__` / `__truediv__` (plus right-side variants) and `save(path)` method
- [ ] **BIND-04**: JS/Deno binding exposes `Expression` with `add`/`sub`/`mul`/`div` methods (no JS operator overloading) and `save(path)` method
- [ ] **BIND-05**: Lua binding exposes `expression` userdata with arithmetic metamethods (`__add`, `__sub`, `__mul`, `__div`) and `save(expr, path)` function

### Cross-layer verification

- [ ] **TEST-02**: C API test suite verifies error paths and lifetime (free-after-save, error retrieval, mismatched-shape rejection)
- [ ] **TEST-03**: Each of the 5 bindings has a round-trip test mirroring the showcase expression `(a + b) * 2.0`

## v2 Requirements

### Reductions

- **REDU-01**: User can sum across a dimension (`sum(a, "scenario")`)
- **REDU-02**: User can take mean across a dimension
- **REDU-03**: User can compute min/max across a dimension

### Extended ops

- **OPS-01**: Unary minus (`-a`)
- **OPS-02**: `pow(a, n)`
- **OPS-03**: `abs(a)`
- **OPS-04**: Element-wise `min(a, b)` and `max(a, b)`
- **OPS-05**: `where(cond, a, b)` requires comparison ops first

### Comparison & logical

- **CMP-01**: Element-wise comparisons (`<`, `<=`, `==`, `!=`, `>=`, `>`)
- **CMP-02**: Element-wise logical (`&&`, `||`, `!`) for boolean expressions

## Out of Scope

| Feature | Reason |
|---------|--------|
| CRTP / expression templates | Incompatible with FFI (5 bindings). Optimizes a non-bottleneck (hot path is I/O, not arithmetic). |
| Reduction operators that change output shape | Deferred to v2 once the same-shape arithmetic foundation lands |
| Comparison / logical / `where()` | Deferred to v2 with reductions |
| Mutating Database operations from expressions | Subsystem is binary-file-only by design — no SQL involvement |
| In-place evaluation (overwrite source file) | `save()` always produces a new file; simpler ownership and crash-safe |
| GPU / SIMD acceleration | Single-threaded scalar inner loop sufficient for v1; revisit if profiling shows arithmetic dominates I/O |
| Expression caching across `save()` calls | First call recomputes; expressions are cheap to rebuild |
| Symbolic simplification (`a - a → 0`) | Premature optimization; not in PyTorch/JAX style frameworks either at this layer |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORE-01 | Phase 1 | Pending |
| CORE-02 | Phase 1 | Pending |
| CORE-03 | Phase 1 | Pending |
| CORE-04 | Phase 1 | Pending |
| CORE-05 | Phase 1 | Pending |
| CORE-06 | Phase 1 | Pending |
| TEST-01 | Phase 1 | Pending |
| CAPI-01 | — | Deferred to future milestone |
| CAPI-02 | — | Deferred to future milestone |
| CAPI-03 | — | Deferred to future milestone |
| BIND-01 | — | Deferred to future milestone |
| BIND-02 | — | Deferred to future milestone |
| BIND-03 | — | Deferred to future milestone |
| BIND-04 | — | Deferred to future milestone |
| BIND-05 | — | Deferred to future milestone |
| TEST-02 | — | Deferred to future milestone |
| TEST-03 | — | Deferred to future milestone |

**Coverage:**
- v1.0 milestone requirements: 7 total (CORE-01..06, TEST-01)
- Mapped to phases: 7 ✓
- Deferred to future milestone: 10 (CAPI-01..03, BIND-01..05, TEST-02, TEST-03)
- Unmapped: 0

---
*Requirements defined: 2026-05-05*
*Last updated: 2026-05-06 — milestone scope narrowed to C++ core after phases 2-7 removed; CAPI / bindings / cross-layer verification deferred to future milestone.*
