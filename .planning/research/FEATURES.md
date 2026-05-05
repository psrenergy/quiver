# Feature Research

**Domain:** Lazy-evaluating dense CPU element-wise tensor framework in C++ (Quiver v0.8.0 milestone, "tensor")
**Researched:** 2026-05-05
**Confidence:** HIGH (cross-checked against Eigen, xtensor, Blaze, NumPy, and CRTP expression-template literature via Context7 and primary docs)
**Scope guard:** Only IN-scope items for v0.8.0 are MVP-eligible. OUT-of-scope items from `PROJECT.md` (Element/Database bridge, C API, bindings, autograd, GPU, sparse, matmul/einsum/decomp) are flagged "DEFERRED" wherever they appear.

---

## Categories (REQ-ID prefixes for the roadmapper)

These are the stable buckets that downstream `ROADMAP.md` should use to derive REQ-IDs:

| Category | Prefix suggestion | What it covers |
|----------|-------------------|----------------|
| **Storage** | `REQ-STG-*` | `Tensor<T>` owning container, shape/stride model, construction, element access |
| **Expression Core** | `REQ-EXP-*` | `Expression<Derived>` CRTP base, evaluation protocol, closure semantics, `eval()` |
| **Arithmetic** | `REQ-ARI-*` | Binary/unary lazy proxies, operator overloads (`+ - * /`, unary `-`), scalar broadcasts |
| **Math Functions** | `REQ-MAT-*` | Unary functions (`abs`, `exp`, `log`, `sqrt`) as expression nodes |
| **Broadcasting** | `REQ-BCT-*` | NumPy-style shape alignment, dimension padding, stride-zero broadcast nodes |
| **Reductions** | `REQ-RED-*` | `sum`, `mean`, `max` (full and per-axis), reducer expression nodes |
| **Assignment & Materialization** | `REQ-ASN-*` | `Tensor<T> = Expression`, fused-loop driver, alias-safe assignment |
| **Diagnostics** | `REQ-DIA-*` | Error messages, shape-mismatch reporting, `to_string` / `print` for Tensors |
| **Build & Test Harness** | `REQ-BLD-*` | Header-only structure, `tensor/` directory layout, GoogleTest scaffolding |

---

## Feature Landscape

### Table Stakes (Users Expect These)

Anyone who has used Eigen, xtensor, Blaze, or NumPy assumes these exist by default. Missing any one of them makes the framework feel like a toy.

| Feature | Why Expected | Complexity | v0.8.0? | Implementation Notes (CRTP / Expression Templates) |
|---------|--------------|------------|---------|----------------------------------------------------|
| **Owning storage `Tensor<T>`** with contiguous buffer, runtime shape | Every tensor lib has a concrete container at the leaves of the expression tree. Eigen has `Array`, xtensor `xarray`, NumPy `ndarray`. | LOW | YES (MVP) | Plain value type, Rule of Zero. Holds `std::vector<T> data_`, `std::vector<size_t> shape_`, `std::vector<size_t> strides_`. Inherits `Expression<Tensor<T>>`. |
| **Element access via `operator()`** with multi-index, plus 1-D `flat()` | Universal: `a(i, j, k)` (xtensor, Eigen `Array`, NumPy via `__getitem__`). | LOW | YES (MVP) | Single inline function; computes flat index via stride dot-product. |
| **CRTP `Expression<Derived>` base** with `shape()`, `operator()(idx...)`, `eval_at(linear)` or equivalent | The whole pattern depends on it; xtensor calls it `xexpression`, Eigen `MatrixBase`/`ArrayBase`. | MEDIUM | YES (MVP) | Base provides static `derived()` cast + interface. Avoid virtual functions; force compile-time dispatch. |
| **Element-wise `+ - * /`** between two expressions of the same shape | The "hello world" of any tensor lib. | LOW | YES (MVP) | One `BinaryOp<Op, L, R>` template + four operator overloads + four `Op` functor structs. |
| **Unary `-`** (negation) | Trivial omission would surprise users. | LOW | YES (MVP) | One `UnaryOp<Op, E>` instantiation. |
| **Scalar broadcasts**: `expr + 2.0`, `2.0 * expr`, etc., for all four ops, both sides | Eigen `array + 10`, xtensor `2 * (a + b)`. Without this, code becomes verbose. | LOW | YES (MVP) | Either a `ScalarExpression<T>` leaf wrapper, or two operator overloads per op (one taking `const T&` left, one right). xtensor uses the wrapper approach. |
| **Unary math: `abs`, `exp`, `log`, `sqrt`** | Listed explicitly in v0.8.0 scope. xtensor and Eigen ship dozens; these four are the floor. | LOW | YES (MVP) | Each is a `UnaryOp` with a stateless functor (`std::abs`, `std::exp`, etc.). Free functions in `quiver::tensor` namespace. |
| **NumPy-style shape broadcasting** between non-matching shapes (right-aligned, size-1 dims expand) | Explicitly in v0.8.0 scope. NumPy's broadcasting rules are the de-facto industry standard; xtensor copies them; PyTorch/TF/JAX all do the same. | HIGH | YES (MVP) | Requires (a) compile-time-or-runtime shape-merge function `broadcast_shape(s1, s2)`; (b) a `BroadcastView` or per-operand index-translator that maps a result-shape index back to the operand by zeroing strides on size-1 axes. xtensor uses `xbroadcast`. **This is the single biggest design call in the milestone.** |
| **Reductions over all axes**: `sum(t)`, `mean(t)`, `max(t)` returning a 0-D tensor (scalar) | xtensor `xt::sum(a)`, NumPy `np.sum(a)`. | MEDIUM | YES (MVP) | A `Reducer<Op, E>` expression node. Materializes by walking all elements. Returns a 0-D `Tensor<T>` (or a scalar via implicit conversion / `.item()`). |
| **Reductions over a single axis**: `sum(t, axis)`, `mean(t, axis)`, `max(t, axis)` | xtensor `xt::sum(a, {1})`, NumPy `np.sum(a, axis=1)`. | MEDIUM | YES (MVP) | Same `Reducer` node parameterized by an `axis` value; result shape drops that axis. Multi-axis (`{0, 1}`) deferred. |
| **Shape inspection**: `shape()`, `rank()` (a.k.a. `dimension()` in xtensor), `size()` | Universal API surface. Without these, users can't write generic code. | LOW | YES (MVP) | Pure const accessors on `Expression<Derived>` — required for broadcasting and reductions to work anyway. |
| **Assignment from expression to `Tensor<T>`** that drives the fused evaluation loop | `Tensor<T> r = a + b * c;` is the *whole point* of expression templates. | MEDIUM | YES (MVP) | `Tensor<T>::operator=(const Expression<E>& e)` resizes if needed, then fills via a single index loop. This is where fusion happens. |
| **Forced evaluation: `.eval()` or `make_tensor(expr)`** | Eigen has `.eval()`; xtensor has implicit assignment. Users *will* need to break a chain (caching, debugging, logging, lifetime). | LOW | YES (MVP) | Free function `eval(expr) -> Tensor<T>` that allocates and fills. Trivial wrapper over assignment-into-temp. |
| **Type promotion for mixed types** (`float + double -> double`, `int + float -> float`) | NumPy `result_type`, xtensor narrowing-cast machinery. With only `Tensor<T>` (single T per leaf) you still need `BinaryOp<Op, L, R>` to deduce `value_type` via `std::common_type_t`. | MEDIUM | YES (MVP) | `using value_type = std::common_type_t<typename L::value_type, typename R::value_type>;` inside `BinaryOp`. Without this, `Tensor<int> + Tensor<double>` fails to compile or silently truncates. |
| **`begin()` / `end()` or equivalent linear iteration** over a `Tensor<T>` | Lets users feed tensors into STL algos / range-based for. | LOW | SHOULD-HAVE | Iterators over the underlying contiguous buffer of `Tensor<T>` only — *not* on lazy expressions (those force evaluation if they want it). |
| **Sensible `to_string` / `operator<<`** for `Tensor<T>` | Required for tests, debugging, error messages. xtensor `xio.hpp`, Eigen has built-in `<<`. | LOW | SHOULD-HAVE | Plain text dump for small tensors; truncate with ellipsis for large ones. xtensor-style print. |
| **Move semantics on `Tensor<T>`** (delete copy if buffer is large? No — keep copy, be explicit) | Standard C++20. | LOW | YES (MVP) | Default copy/move. Buffer is `std::vector<T>` — Rule of Zero handles it. |

### Differentiators (Quiver-specific, energy-modeling-friendly)

These are NOT competition with general-purpose tensor libs; they are leverage points where Quiver's specific user (PSR Energy time-series workflows) gets value above what xtensor/Eigen offer "out of the box." None expand v0.8.0 scope; they're framing notes the roadmapper can use to prioritize.

| Feature | Value Proposition | Complexity | v0.8.0? | Notes |
|---------|-------------------|------------|---------|-------|
| **Naming consistent with Quiver's existing surface** (`snake_case`, `read_*` / `update_*` verbs preserved when applicable; namespace `quiver::tensor`) | Energy users already use Quiver's database/binary APIs in 5 languages. Tensor framework that breaks naming style adds cognitive load. | LOW | YES | Free, just discipline. `Tensor<T>`, `Expression<Derived>`, free functions `sum`, `mean`, `max`, `abs`, `exp`, `log`, `sqrt`, `eval`, `broadcast_shape`. No `Eigen`-style camelCase or `xt::`-style prefixes. |
| **Explicit `std::runtime_error` messages following Quiver's three-pattern rule** (`Cannot {op}: {reason}`, `{Entity} not found: {id}`, `Failed to {op}: {reason}`) | Same error-surface contract the rest of the codebase honors. | LOW | YES | Examples: `"Cannot broadcast: shapes [2,3] and [4,2] are incompatible at dimension 1"`, `"Cannot reduce: axis 3 out of range for tensor of rank 2"`. |
| **Header-only deliverable** with no SQLite/Element coupling | Tensor framework works as a standalone include for users who only want lazy math, without paying for the SQLite linkage. | LOW | YES | Already mandated by `PROJECT.md` constraint. Place in `include/quiver/tensor/`. Compiled translation unit only if a non-template helper is genuinely needed. |
| **Doubles as the leaf type for the future Binary/Element bridge** (v0.9.0+) | Anticipates the deferred bridge: when `Database::read_tensor` lands, it returns `Tensor<double>` shaped from `BinaryMetadata::dimensions`. Designing `Tensor<T>` with this in mind avoids a v0.9.0 redesign. | LOW | YES (design-only) | Constrain `Tensor<T>::shape_` representation to `std::vector<size_t>` (not a fixed `std::array<N>`), matching `BinaryMetadata::dimensions[i].size`. No code in v0.8.0; just a constraint on the storage type. |
| **Time-series-shaped construction helper** (e.g., a documented pattern for `Tensor<double>(time_series_view)` in a future milestone) | Energy users primarily multiply per-hour tensors, sum across scenarios, take max across forecasts. The natural shape is `[scenarios, periods, blocks]`. | N/A | DEFERRED to bridge (v0.9.0) | Out of scope for v0.8.0. Note here only so the roadmap doesn't accidentally close this off. |
| **Deterministic compile-time dispatch (no runtime polymorphism)** | Every binary operator inlines into the assignment loop. Energy users running scenario sweeps need predictable performance, not virtual-call overhead. | LOW (just discipline) | YES | No `virtual` keyword anywhere in the tensor framework. CRTP only. Aligns with existing Quiver value-type conventions (Element, Row). |
| **Zero hidden allocations between leaves and the assignment** | Lazy expressions hold *only* references/values, never `Tensor<T>` copies. The only allocation is the destination on assignment (or `eval()`). | MEDIUM | YES | This requires xtensor-style closure semantics: `BinaryOp` stores operands as `xclosure_t`-equivalent — by reference for lvalues, by value for rvalues. **See anti-features for the dangling-reference trap.** |

### Anti-Features (Look Essential, Are Traps for v0.8.0)

These are operations that *seem* like obvious table stakes but cause silent correctness bugs, scope creep, or user confusion in a v0.8.0 lazy framework. The "Better approach" column is the one the framework should adopt.

| Anti-Feature | Why Requested | Why Problematic | Better Approach |
|--------------|---------------|-----------------|-----------------|
| **In-place mutation of a lazy expression** (e.g., `a + b *= 2.0`, `(a + b)[0] = 1`) | Looks like NumPy `a[i] = x`. Users assume slices/expressions are mutable. | A lazy `BinaryOp<Plus, A, B>` doesn't *own* a buffer — there's nowhere to mutate. Allowing it would mean either silently materializing on `op[]=` (hidden cost) or a confusing compile error. | Make `Expression<Derived>::operator()` return by **value** (read-only). Mutation only on `Tensor<T>`. Document explicitly: *expressions are values, tensors are vessels*. |
| **Implicit memoization / evaluation cache on expressions** | "I called `expr(0,0)` twice — surely it cached?" | Caching means hidden state, hidden allocation, and locking concerns. Breaks the "lazy is free" mental model and reintroduces ownership questions for an Expression. | No caching. Each `operator()(i,j,k)` recomputes. If user wants caching, `auto t = eval(expr)` materializes once. State this in the API doc upfront. |
| **Automatic parallelization on every op** (OpenMP-on-by-default) | Eigen and Blaze do this. "Why is mine slower?" | Parallel-by-default has tail-latency surprises (small tensors), thread-affinity issues, conflicts with user-managed thread pools, and turns every op into a scheduling decision. v0.8.0 has no perf budget for this. | Sequential by default. Mark hot loops with `#pragma omp simd` only where it's safe (no data deps). Parallelism is a v1.x decision once the API is stable. |
| **"Zero-cost" copies that secretly materialize** (e.g., `Tensor<T> b = a + 1; Tensor<T> c = b;` triggers a hidden eval inside `b`) | Users want copy-on-write semantics. | The lazy expression returned by `a + 1` cannot be safely held in a `Tensor<T>` — assignment *must* materialize. Pretending otherwise hides allocations behind innocent-looking statements. | Be explicit: `Tensor<T>` always owns. `auto x = a + 1;` returns the expression (unevaluated). `Tensor<T> x = a + 1;` materializes (allocates). State this in two sentences in the README. xtensor and Eigen do exactly this. |
| **`auto` storage of expression** with operands as rvalues (the "auto + temporary" dangling reference) | `auto e = make_a() + make_b();` looks innocent. | If `BinaryOp` stores operands by reference and `make_a()` returned a temporary, the temporary dies at the end of the full-expression — `e` is a dangling-reference time bomb. xtensor solved this with `xclosure_t`. | Use **closure semantics**: store operand by value if it was passed as rvalue (move it), by reference if passed as lvalue. Mirrors xtensor exactly. Add a static_assert against storing dangling temporaries where possible. |
| **Aliased self-assignment** (`a = a.transpose()`, `a = a + a.col(0)`) without temporary | NumPy "just works" because of eager eval. | A *fused* assignment that reads from the destination while writing produces wrong values silently. Eigen flags this with `EvaluatorFlags`; xtensor uses temporaries. | For v0.8.0 (no transpose, no view ops), the only realistic alias is `a = expr_involving_a`. Force materialization into a temporary inside `Tensor<T>::operator=`: build the result in a local `std::vector<T>`, then swap into `data_`. Trivial — and zero edge cases. **This is the single most important correctness rule and must be a v0.8.0 acceptance test.** |
| **Multi-axis reductions in v0.8.0** (`sum(t, {0, 2})`) | NumPy and xtensor support a list of axes. | Multi-axis reduction is *not* hard, but it expands the reducer template surface (axis container, axis ordering rules, keepdims behavior) and inflates the test matrix. v0.8.0 explicitly says "optional axis" (singular). | Single axis (or no axis = full reduction) only. Defer multi-axis to v0.9.0. The MVP literally needs `sum`, `mean`, `max` — three reducers × `{full, single-axis}` = 6 entry points. |
| **`keepdims` flag, `dtype` argument, `where` mask, `out` parameter** on reductions | NumPy ships these. | Each is its own feature: `keepdims` doubles the test matrix; `dtype` is type-promotion-on-output (different from BinaryOp promotion); `where` is conditional reduction; `out` re-introduces the alias problem. | Out of scope. Reductions take only `(expression, axis?)`. Document the omission. v0.9.0 candidates. |
| **Comparison operators returning bool tensors** (`a < b`, `a == b`) plus `where(mask, a, b)` | xtensor, NumPy, PyTorch all do this. Looks like an obvious table-stake. | Adds a `Tensor<bool>` instantiation, requires comparison `BinaryOp` instances, requires `where` ternary node, requires `any`/`all` reductions. Also: comparing floating-point with `==` invites bugs. | **Defer to v0.9.0.** v0.8.0 scope explicitly says "operator overloads (+, -, *, /, unary -)". Adding `<`, `>`, `==`, etc., here is scope creep. (Keep this in mind: this is the **most likely creep**, because they feel cheap. They aren't — they pull in `Tensor<bool>` and `where`.) |
| **Slicing / views (`a(range, all, range)`)** | xtensor `xt::view`, NumPy `a[1:3, :, ::2]`. | Views are a distinct expression hierarchy with stride manipulation, lifetime questions (view-of-rvalue), and expand the test matrix dramatically. They are *not* in v0.8.0 scope. | Deferred. Users assemble per-element via `operator()`. Add `xt::view`-equivalent in v0.9.0+. |
| **Reshape, transpose, concatenation, stack** | NumPy `reshape`, `transpose`, `concatenate`. | Each is its own expression node. Transpose has aliasing pitfalls. Reshape needs to know if a view is possible. Out of v0.8.0 scope. | Deferred. v0.8.0 is *element-wise* — explicit name. |
| **Linear algebra (matmul, dot, decompositions, einsum)** | "It's a tensor library, surely it has matmul." | Explicitly out of scope per `PROJECT.md`. Adding matmul opens the BLAS dependency question, which Quiver's stack does not have. | Deferred indefinitely. State in README: *"Quiver's tensor framework is a lazy element-wise framework. For BLAS-backed linear algebra, use Eigen / Blaze / xtensor-blas."* |
| **Autograd / autodiff** | Modern users assume PyTorch-style `backward()`. | Out of v0.8.0 scope per `PROJECT.md`. Would require a tape mechanism on top of the lazy graph. | Deferred. Out of roadmap. |
| **GPU / device dispatch** | "CPU only? In 2026?" | Out of v0.8.0 scope per `PROJECT.md`. Each kernel needs a CUDA/SYCL counterpart. | Deferred indefinitely. |

---

## Feature Dependencies

The following graph shows the order in which features must be implemented. **Edges are "X requires Y"** — Y must land in an earlier phase (or earlier task within a phase) than X. This is what the roadmapper uses to sequence phases.

```
                        ┌─────────────────────────────────┐
                        │   REQ-STG: Tensor<T> storage    │
                        │   (shape, strides, data buffer) │
                        └──────────────┬──────────────────┘
                                       │ requires
                                       ▼
                        ┌─────────────────────────────────┐
                        │   REQ-EXP: Expression<Derived>  │
                        │   CRTP base + closure semantics │
                        └──────────────┬──────────────────┘
                                       │ requires
              ┌────────────────────────┼────────────────────────┐
              │                        │                        │
              ▼                        ▼                        ▼
    ┌────────────────────┐  ┌────────────────────┐  ┌──────────────────────┐
    │ REQ-ARI: BinaryOp  │  │  REQ-ARI: UnaryOp  │  │ REQ-ARI: Scalar      │
    │ (+ - * /)          │  │  (unary -)         │  │ broadcast (a + 2.0)  │
    └─────────┬──────────┘  └────────┬───────────┘  └──────────┬───────────┘
              │                      │                         │
              └──────────────────────┼─────────────────────────┘
                                     │
                                     ▼
                       ┌─────────────────────────────────┐
                       │   REQ-ASN: assignment driver    │
                       │   Tensor<T> = Expression        │
                       │   (this is where fusion lives)  │
                       └──────────────┬──────────────────┘
                                      │ requires
              ┌───────────────────────┼───────────────────────┐
              │                       │                       │
              ▼                       ▼                       ▼
    ┌────────────────────┐  ┌──────────────────────┐  ┌──────────────────────┐
    │ REQ-MAT: abs, exp, │  │ REQ-BCT: shape       │  │ REQ-DIA: error       │
    │ log, sqrt          │  │ broadcasting (NumPy) │  │ messages, to_string  │
    │ (UnaryOp clones)   │  │                      │  │                      │
    └────────────────────┘  └──────────┬───────────┘  └──────────────────────┘
                                       │ requires
                                       ▼
                       ┌─────────────────────────────────┐
                       │   REQ-RED: reductions           │
                       │   sum / mean / max              │
                       │   (full, then per-axis)         │
                       └─────────────────────────────────┘
```

### Dependency Notes

- **REQ-EXP requires REQ-STG.** The CRTP base needs *a* concrete expression to plug into; without `Tensor<T>` you can't write a single test. Build storage first; it's also the simplest piece.
- **REQ-ARI requires REQ-EXP.** Operators are lazy proxies; without the base they have nothing to derive from.
- **REQ-ASN requires REQ-ARI.** You can't write the fused-assignment loop until you have a non-trivial expression to drive it. Empirically: scalar broadcast (`Tensor<T> r = a + 1.0;`) is the smallest end-to-end test possible — implement assignment in service of *one* binary op + scalar broadcast first.
- **REQ-MAT requires REQ-ASN.** Math functions are just more `UnaryOp` instances; gated by working assignment so each new function is a one-line addition with one test.
- **REQ-BCT requires REQ-ASN.** Broadcasting changes how the assignment loop translates indices per operand. It's the deepest single feature — needs a working baseline assignment to compare against. **This is where the most research time will go.**
- **REQ-RED requires REQ-BCT.** A 0-D reduction result has shape `[]` and broadcasts against everything. Per-axis reductions produce `(d-1)`-D results that users will then broadcast against the original tensor. The reducer node and the broadcaster share the index-translation machinery, so building broadcast first lets the reducer reuse it.
- **REQ-DIA enhances REQ-ASN, REQ-BCT, REQ-RED.** Error messages from shape mismatch are the user's first impression — they should be in place before any of those three features ship to a user-facing test.
- **REQ-BLD spans the whole milestone.** Header layout, GoogleTest scaffolding, and the `tests/test_tensor_*.cpp` files mirror the rest of the codebase and need to exist on day one of phase 1.

### Conflict notes

- **REQ-ARI scalar broadcasts and REQ-BCT shape broadcasting must agree on the leaf type.** A scalar can be modeled either as a 0-D `Tensor<T>`, as a `ScalarExpression<T>` leaf, or via dual operator overloads. **Pick one** before REQ-BCT lands; xtensor models scalar-as-0-D, which simplifies things — recommend the same.
- **REQ-RED axis-handling and any future REQ for slicing/views (out of scope) share `axis_t` semantics.** Don't bake an `int` axis type that v0.9.0 will have to redo. Use a documented type alias (`using axis_t = std::ptrdiff_t;` matches xtensor) so the v0.9.0 multi-axis reducer and slicing can reuse it.

---

## MVP Definition

### Launch With (v0.8.0 — minimum to be called a tensor framework)

The line between "lazy vector op library" and "tensor framework" is **multi-dimensional shape with broadcasting**. Without that, it's a 1-D toy.

- [ ] **REQ-STG: `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}`** with shape, strides, contiguous buffer, `operator()(idx...)`, copy/move. — *Without this, nothing else compiles.*
- [ ] **REQ-EXP: `Expression<Derived>` CRTP base** with `shape()`, `rank()`, `eval_at(linear_idx)` or `operator()(idx...)`, closure semantics for operand storage. — *Without this, the lazy machinery doesn't exist.*
- [ ] **REQ-ARI: `BinaryOp` with `+ - * /` operators**, including type promotion via `std::common_type_t`. — *The point of the library.*
- [ ] **REQ-ARI: `UnaryOp` with unary `-`**, plus scalar broadcasts on both sides for all four binary ops. — *Pragmatic minimum.*
- [ ] **REQ-ASN: assignment from any `Expression<E>` to `Tensor<T>`**, with materialize-via-temporary for alias safety. — *The *only* place evaluation happens.*
- [ ] **REQ-ASN: free function `eval(expr) -> Tensor<T>`** for forced materialization. — *Users will need to break a chain.*
- [ ] **REQ-BCT: NumPy-style shape broadcasting** between two operands of compatible shapes (right-aligned, size-1 expansion, scalar leaves). — *Without this, two tensors of shape `[24]` and `[2,24]` can't multiply, which breaks the explicit "scaling load curves" use case.*
- [ ] **REQ-RED: full-tensor reductions `sum`, `mean`, `max`** returning a 0-D tensor (or scalar via `.item()`). — *Three lines per reducer; without them, "compute mean across scenarios" is unwriteable.*
- [ ] **REQ-RED: single-axis reductions `sum(t, axis)`, `mean(t, axis)`, `max(t, axis)`**. — *Single-axis is explicitly in v0.8.0 scope; multi-axis is not.*
- [ ] **REQ-DIA: shape-mismatch error messages** following the three-pattern rule (`Cannot broadcast: ...`, `Cannot reduce: ...`). — *Acceptance: every shape-mismatch failure mode produces a message that names both shapes.*
- [ ] **REQ-DIA: `to_string` / `operator<<` for `Tensor<T>`** with truncation for large tensors. — *Required for tests and debugging; without it, every test failure is unreadable.*
- [ ] **REQ-BLD: `include/quiver/tensor/` header layout, GoogleTest harness `tests/test_tensor_*.cpp`** mirroring the existing codebase split. — *Without this, the milestone has nowhere to live.*

### Should-Have (v0.8.0 if time permits, still in scope per PROJECT.md)

- [ ] **REQ-MAT: `abs`, `exp`, `log`, `sqrt`** as free functions returning `UnaryOp` expressions. — *Explicitly listed in v0.8.0 scope. They are five-line each. Should not be cut unless the milestone is cratering.*
- [ ] **REQ-STG: `begin()` / `end()` iterators** on `Tensor<T>`. — *Cheap, useful, low-risk.*
- [ ] **REQ-DIA: `static_assert` on type-mismatched assignment** (e.g., assigning `Expression<float>` into `Tensor<int>` fails to compile with a readable message). — *Catches a class of bugs at the line they're written.*

### Future Consideration (v0.9.0+, deferred)

- [ ] **Comparison operators (`< > <= >= == !=`) returning `Tensor<bool>` expressions**, plus `where`, `any`, `all`. — *xtensor / NumPy table-stake. Pulls in `Tensor<bool>` and ternary expression nodes. v0.9.0 candidate.*
- [ ] **Multi-axis reductions** (`sum(t, {0, 2})`), `prod`, `argmin`/`argmax`, `nanmin`/`nanmax`/`nansum`. — *Each is incremental; bundle when v0.9.0 expands the reducer surface.*
- [ ] **Reshape, transpose, slicing/views (`xt::view`-equivalent), broadcast-explicit** (`broadcast_to`). — *Distinct expression hierarchy. Single biggest API expansion v0.9.0 could ship.*
- [ ] **Element / Database bridge** (`Database::read_tensor`, `Element::set(Tensor)`). — *Explicitly deferred per `PROJECT.md`.*
- [ ] **Tensor C API** (`quiver_tensor_*`) and the **immediate-mode wrappers** that make Expression Templates surviveable across FFI. — *Deferred per `PROJECT.md`.*
- [ ] **Tensor language bindings** (Julia / Dart / Python / JS / Lua). — *Deferred per `PROJECT.md`.*
- [ ] **Concatenation / stacking** (`concat`, `stack`). — *Useful, distinct expression node.*
- [ ] **Trigonometry (`sin`, `cos`, ...), rounding (`floor`, `ceil`, `round`), power (`pow`, `square`), `clip`**. — *All are one-line additions once the math-function pattern (REQ-MAT) is established.*
- [ ] **`std::span` interop, NumPy-buffer-protocol bridge for Python**. — *Crosses with the bindings milestone.*
- [ ] **Parallelization (OpenMP / std::execution)**. — *Performance milestone, separate from API.*
- [ ] **Autograd, GPU, sparse, BLAS-backed linalg**. — *Out of roadmap per `PROJECT.md`.*

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| `Tensor<T>` storage (REQ-STG core) | HIGH | LOW | **P1** |
| `Expression<Derived>` CRTP base (REQ-EXP) | HIGH | MEDIUM | **P1** |
| `BinaryOp` + `+ - * /` (REQ-ARI) | HIGH | LOW | **P1** |
| Unary `-` + scalar broadcasts (REQ-ARI) | HIGH | LOW | **P1** |
| Assignment-driven fusion (REQ-ASN) | HIGH | MEDIUM | **P1** |
| `eval()` (REQ-ASN) | MEDIUM | LOW | **P1** |
| NumPy-style shape broadcasting (REQ-BCT) | HIGH | HIGH | **P1** |
| Full-tensor reductions sum/mean/max (REQ-RED) | HIGH | MEDIUM | **P1** |
| Single-axis reductions (REQ-RED) | HIGH | MEDIUM | **P1** |
| Error messages, `to_string` (REQ-DIA) | MEDIUM | LOW | **P1** |
| Build/test harness (REQ-BLD) | (infra) | LOW | **P1** |
| `abs`, `exp`, `log`, `sqrt` (REQ-MAT) | MEDIUM | LOW | **P2** |
| `begin()` / `end()` iterators | MEDIUM | LOW | **P2** |
| `static_assert` on type mismatch | MEDIUM | LOW | **P2** |
| Comparison ops + `Tensor<bool>` + `where` | MEDIUM | MEDIUM | **P3 (v0.9.0)** |
| Multi-axis reductions, `prod`, `argmax` | MEDIUM | LOW | **P3 (v0.9.0)** |
| Slicing / views | HIGH | HIGH | **P3 (v0.9.0)** |
| Reshape / transpose | MEDIUM | MEDIUM | **P3 (v0.9.0)** |
| Database / Element bridge | HIGH | MEDIUM | **P3 (v0.9.0)** |
| C API + bindings | HIGH | HIGH | **P3 (v0.9.0+)** |
| Trig / rounding / `pow` / `clip` | LOW | LOW | **P3 (v0.9.0)** |
| OpenMP parallelism | MEDIUM | MEDIUM | **P3 (v1.x)** |
| Autograd, GPU, sparse, BLAS | (varies) | HIGH | **out of roadmap** |

**Priority key:**
- **P1** — Must have for v0.8.0 launch. Core scope from `PROJECT.md`.
- **P2** — Should have for v0.8.0 if time permits. Within scope, low-risk additions.
- **P3** — Deferred to v0.9.0 or later. Out of v0.8.0 scope per `PROJECT.md`, but inevitable.

---

## Comparison Reference Table (Naming Decisions)

For each operation Quiver might expose at v0.8.0, here is how the major ecosystems name and shape it. This is the input the roadmapper / API designer needs to pick Quiver's name.

| Operation | Eigen (`ArrayXd`) | xtensor (`xarray`) | Blaze (`DynamicMatrix`) | NumPy | C++ stdlib (mdspan / ranges) | **Quiver suggestion** |
|-----------|-------------------|--------------------|--------------------------|-------|------------------------------|------------------------|
| Container | `Eigen::ArrayXd`, `MatrixXd` (matrix vs array distinction) | `xt::xarray<T>`, `xt::xtensor<T, N>` (dynamic vs fixed rank) | `blaze::DynamicVector<T>`, `DynamicMatrix<T>` | `np.ndarray` | `std::mdspan<T, extents>` (non-owning); no owning equivalent | **`quiver::tensor::Tensor<T>`** — single owning type, dynamic rank, namespace `quiver::tensor` |
| CRTP base | `Eigen::ArrayBase<Derived>`, `MatrixBase<Derived>` | `xt::xexpression<D>`, `xt::xfunction<F, A...>` | `Vector<VT, TF>`, `Matrix<MT, SO>` (CRTP via tags) | N/A (Python; uses `__array_function__` protocol) | N/A | **`quiver::tensor::Expression<Derived>`** |
| Element-wise add | `a + b` (Array; `Matrix + Matrix` is matrix add) | `a + b` | `a + b` | `a + b` | n/a (mdspan has no operators) | **`a + b`** — operator overloaded on `Expression<Derived>` |
| Element-wise multiply | `a * b` (Array; `Matrix * Matrix` is matmul) | `a * b` | `a * b` is matmul (!), `a % b` is element-wise (!) | `a * b` | n/a | **`a * b`** — element-wise; no `*` overload for matmul, since matmul is OUT of scope |
| Scalar broadcast | `a + 10`, `2 * a` | `2 * (a + b)` | `a + 10` | `a + 10` | n/a | **`a + 10.0`** — both directions, all four ops |
| Negate | `-a` | `-a` | `-a` | `-a` | n/a | **`-a`** |
| Absolute value | `a.abs()` (member) | `xt::abs(a)` (free function) | `abs(a)` (free function via ADL) | `np.abs(a)`, `abs(a)` | n/a | **`quiver::tensor::abs(a)`** — free function. Matches xtensor / NumPy / Blaze (3 vs 1 for member-style). Quiver uses snake_case free functions elsewhere. |
| Exp / Log / Sqrt | `a.exp()`, `a.log()`, `a.sqrt()` | `xt::exp(a)`, `xt::log(a)`, `xt::sqrt(a)` | `exp(a)`, `log(a)`, `sqrt(a)` | `np.exp(a)`, `np.log(a)`, `np.sqrt(a)` | n/a | **`exp(a)`, `log(a)`, `sqrt(a)`** in `quiver::tensor` namespace |
| Sum (full) | `a.sum()` (member, eager) | `xt::sum(a)()` (lazy 0-D, then `()` to scalarize) | `sum(a)` | `np.sum(a)`, `a.sum()` | n/a | **`sum(a)`** — free function, returns 0-D `Tensor<T>` (or scalar via `.item()`) |
| Sum (axis) | `a.colwise().sum()` / `rowwise().sum()` (member, axis-by-orientation) | `xt::sum(a, {axis})` or `xt::sum(a, axis)` | n/a (matrix-only orientation) | `np.sum(a, axis=k)` | n/a | **`sum(a, axis)`** — free function, axis is `std::ptrdiff_t` (xtensor convention) |
| Mean | `a.mean()` (member); axis via `colwise()` | `xt::mean(a)`, `xt::mean(a, {axis})` | `mean(a)` | `np.mean(a)`, `np.mean(a, axis=k)` | n/a | **`mean(a)`, `mean(a, axis)`** |
| Max (reduction, not element-wise) | `a.maxCoeff()` (member, full); `a.colwise().maxCoeff()` (axis) | `xt::amax(a)`, `xt::amax(a, {axis})`; **note xtensor uses `amax` to disambiguate from element-wise `maximum`** | `max(a)` | `np.max(a)`, `np.amax(a)` | n/a | **`max(a)`, `max(a, axis)`** — Quiver picks `max` (NumPy/Blaze/Eigen-`maxCoeff` style) over xtensor's `amax`. The element-wise binary `maximum(a, b)` is OUT of v0.8.0 scope, so the name conflict that motivated xtensor's choice does not exist. |
| Force evaluate | `a.eval()` (member) | implicit on `xarray` assignment; explicit via `xt::eval` (less common) | `evaluate(a)` | `np.asarray(a)` (rarely needed, eager) | n/a | **`eval(a)`** — free function. Member `.eval()` is a non-Quiver convention; free functions match the rest of the namespace. |
| Shape | `a.rows()`, `a.cols()`, no rank | `a.shape()`, `a.dimension()` | `rows(a)`, `columns(a)` | `a.shape`, `a.ndim` | `e.extent(d)`, `e.rank()` | **`a.shape()`** (returns `std::vector<size_t>` — matches xtensor and Quiver `BinaryMetadata::dimensions`), **`a.rank()`** (matches mdspan / Python `ndim`) |
| Comparison `<`, `==` | `(a < b)` (Array); returns Array<bool> | `a < b`, `xt::equal(a, b)` | `a < b`, `equal(a, b)` | `a < b` | n/a | **DEFER to v0.9.0** — out of v0.8.0 scope |

### Key naming decisions Quiver should lock in

1. **Free functions, not members**, for non-trivial operations (`sum`, `mean`, `max`, `abs`, `exp`, `log`, `sqrt`, `eval`). Matches xtensor, NumPy, and Quiver's own `read_*` / `update_*` free-function-style. Members on `Tensor<T>` only for accessors (`shape`, `rank`, `size`, `data`).
2. **No matrix/array distinction.** Quiver has *one* `Tensor<T>` with element-wise semantics. No matmul means no ambiguity in `*`. (Eigen's Array vs Matrix split is justified there because they ship matmul; Quiver doesn't.)
3. **`max` not `amax` / `maxCoeff`** — element-wise `maximum(a, b)` is out of v0.8.0 scope, so the disambiguation that drove xtensor and Eigen's naming doesn't apply.
4. **`axis` is `std::ptrdiff_t`** (matches xtensor and Python `int`). Negative values for end-relative indexing is a v0.9.0 question; v0.8.0 accepts non-negative only.
5. **Namespace `quiver::tensor`**, not `quiver` directly. Avoids polluting `quiver::` with `sum`, `mean`, `max` symbols that have nothing to do with the database.

---

## Anti-Pattern Reference (For PITFALLS.md handoff)

The roadmapper / pitfalls researcher should treat the following as direct inputs:

1. **Aliased self-assignment fusion bug** — `a = a + a.flat(0)` semantically; even though v0.8.0 has no slicing, `a = a + a` *can* go wrong if assignment writes destinations as it reads sources. Fix: assignment materializes into a `std::vector<T>` first, swap into `data_`. Cite Eigen's "Lazy Evaluation and Aliasing" doc.
2. **Dangling reference from `auto e = make() + make();`** — the xtensor-`xclosure_t` pattern. Operands stored by reference if lvalue, by value if rvalue. Document this and add a static-assert helper for common foot-guns.
3. **Hidden allocations in chained expressions** — only the final `Tensor<T>` assignment / `eval()` should allocate. Tests should assert with a custom allocator that intermediate expressions allocate zero bytes.
4. **Type promotion silent narrowing** — `Tensor<int> r = Tensor<double>(...) + 1.0;` should be a compile error or very loud warning. Use `std::common_type_t` in BinaryOp deduction; require explicit `cast<int>` (deferred but document the placeholder).
5. **0-D vs scalar identity** — what does `sum(a)` return: `Tensor<T>` of shape `{}`, `Tensor<T>` of shape `{1}`, or `T`? Pick *one* (recommend 0-D `Tensor<T>` with explicit `.item()` extraction, matching xtensor) and stick with it across all reductions. Inconsistency here will make every reduction test confusing.
6. **Broadcasting shape rule corner cases** — what about empty tensors (`shape = {0, 3}` + `shape = {3}`)? What about `{1}` vs `{}`? NumPy has well-documented rules; Quiver should adopt them verbatim and cite them. Don't invent your own.

---

## Sources

### Primary documentation (Context7-verified, HIGH confidence)

- [xtensor — Expressions and lazy evaluation (docs/source/expression.rst)](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/expression.rst)
- [xtensor — Operators and functions (docs/source/operator.rst)](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/operator.rst)
- [xtensor — Reducing functions API](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/api/reducing_functions.rst)
- [xtensor — Concepts (CRTP base xexpression)](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/developer/concepts.rst)
- [xtensor — Implementation classes (xfunction, xreducer, xbroadcast)](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/developer/implementation_classes.rst)
- [xtensor — Closure semantics (rvalue/lvalue capture for expression operands)](https://xtensor.readthedocs.io/en/latest/closure-semantics.html)
- [xtensor — From NumPy to xtensor (naming-translation table)](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/numpy.rst)
- [xtensor — Views (xt::view, slicing semantics — for "what we are NOT building")](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/view.rst)
- [Eigen — The Array class and coefficient-wise operations](https://eigen.tuxfamily.org/dox/group__TutorialArrayClass.html)
- [Eigen — Catalog of coefficient-wise math functions](https://eigen.tuxfamily.org/dox/group__CoeffwiseMathFunctions.html)
- [Eigen — Lazy Evaluation and Aliasing](https://libeigen.gitlab.io/eigen/docs-nightly/TopicLazyEvaluation.html)
- [Eigen — Reductions, Visitors, and Broadcasting tutorial](https://gitlab.com/libeigen/eigen/-/blob/master/doc/TutorialReductionsVisitorsBroadcasting.dox)
- [NumPy — Broadcasting basics (the de-facto industry standard)](https://numpy.org/doc/stable/user/basics.broadcasting.html)
- [NumPy — Universal functions (ufunc) basics](https://numpy.org/doc/2.0/user/basics.ufuncs.html)

### Secondary references (MEDIUM confidence; ecosystem context)

- [Blaze C++ math library (live-clones/blaze) — Smart Expression Templates](https://github.com/live-clones/blaze)
- [Iglberger et al. — "Expression Templates Revisited" (Blaze paper)](https://blogs.fau.de/hager/files/2012/05/ET-SISC-Iglberger2012.pdf)
- [Roman Poya — Performance comparison: Eigen vs Blaze vs Fastor vs Armadillo vs xtensor](https://romanpoya.medium.com/a-look-at-the-performance-of-expression-templates-in-c-eigen-vs-blaze-vs-fastor-vs-armadillo-vs-2474ed38d982)
- [Marc — On designing Tenseur, A C++ tensor library with lazy evaluation](https://istmarc.github.io/post/2024/10/27/on-designing-tenseur-a-c-tensor-library-with-lazy-evaluation/)
- [Jeremy Rifkin — Expression Templates in C++](https://rifkin.dev/blog/expression-templates)
- [Wikipedia — Expression templates (overview)](https://en.wikipedia.org/wiki/Expression_templates)
- [cppreference — Curiously Recurring Template Pattern (CRTP)](https://en.cppreference.com/cpp/language/crtp)
- [Tensor (robclu) — C++ expression-template tensor library reference](https://robclu.github.io/tensor/)

### Internal (Quiver project context)

- `.planning/PROJECT.md` — v0.8.0 milestone scope, Out-of-Scope list, key decisions
- `.planning/codebase/ARCHITECTURE.md` — layering, header-only constraint for tensor framework
- `.planning/codebase/STRUCTURE.md` — directory layout for new subsystems (mirror `binary/`)
- `CLAUDE.md` — naming conventions, three-pattern error messages, RAII / Rule-of-Zero discipline
- `include/quiver/binary/binary_file.h` — closest existing analog (Pimpl + value-type metadata pair); informs `tensor/` directory layout

---

*Feature research for: lazy-evaluating dense CPU element-wise tensor framework in C++*
*Researched: 2026-05-05*
*Confidence: HIGH on table-stakes and anti-features (cross-validated across Eigen, xtensor, Blaze, NumPy via Context7); MEDIUM on energy-domain differentiators (informed by PROJECT.md context, not validated against real PSR Energy code).*
