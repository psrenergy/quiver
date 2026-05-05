# Architecture Research

**Domain:** Lazy CRTP tensor framework integrated into Quiver (C++ SQLite wrapper, v0.7.9 → v0.8.0 "tensor")
**Researched:** 2026-05-05
**Confidence:** HIGH for placement/conventions inside Quiver (sourced from in-tree files); HIGH for canonical CRTP pattern (Eigen/xtensor/Blaze/Wikipedia all converge on the same shape); MEDIUM for the specific recommendation between "BroadcastView wrapper node" vs "BinaryOp-with-stride-translation" — both are shipped in production libraries, recommendation below is opinionated.

## Standard Architecture

### System Overview — Tensor Inside Quiver

The tensor framework adds a **fourth subsystem** to the C++ core, parallel to Database, Binary, and LuaRunner. v0.8.0 stops at the C++ core layer — the C API and binding rows are placeholders shown only to anchor where future work plugs in.

```
┌────────────────────────────────────────────────────────────────────────────┐
│                  Bindings  (Julia / Dart / Python / JS / Lua)              │
│        Database* / Binary* / LuaRunner — wired now                         │
│        Tensor*   — DEFERRED to v0.10.0 (after C API)                       │
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   │ FFI (extern "C", stable ABI)
┌──────────────────────────────────▼─────────────────────────────────────────┐
│                  C API  (libquiver_c — src/c/)                             │
│        quiver_database_*  /  quiver_binary_*  /  quiver_lua_runner_*       │
│        quiver_tensor_*    — DEFERRED to v0.9.0 (immediate-mode wrappers)   │
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   │ Direct C++ method calls
┌──────────────────────────────────▼─────────────────────────────────────────┐
│                  C++ Core  (libquiver — src/, include/quiver/)             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │  Database    │  │  Binary      │  │  LuaRunner   │  │  Tensor (NEW)  │  │
│  │  (Pimpl,     │  │  (Pimpl,     │  │  (Pimpl,     │  │  header-only,  │  │
│  │   sqlite3)   │  │   iostream)  │  │   sol2/lua)  │  │   no Pimpl)    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └────────────────┘  │
│                                                                            │
│   Cross-cutting:  spdlog, std::runtime_error (3 message patterns)          │
└────────────────────────────────────────────────────────────────────────────┘
```

The tensor subsystem is **a leaf** in this architecture for v0.8.0 — it has no upstream consumers in C++ (Database does not call into it), no C API surface, and no bindings. It compiles as part of `libquiver` but exposes only header-only templates; nothing in the existing `libquiver` translation units instantiates it.

### Component Responsibilities

| Component | Responsibility | Implementation in Quiver |
|-----------|----------------|--------------------------|
| `Expression<Derived>` | CRTP base; `self()` static cast; common interface (`shape()`, `size()`, `eval(i)`) | Header-only template, plain value type, no Pimpl |
| `Tensor<T>` | Owning storage leaf; assignment from any `Expression<E>` triggers materialization | Holds `std::vector<T>` data + `shape_t` + strides; non-template-templated value type with templated assignment operators |
| `BinaryOp<L, Op, R>` | Intermediate node for elementwise binary ops; evaluates `Op{}(L::eval(i), R::eval(i))` | Header-only template; **leaves stored by const ref via `ref_selector`, intermediates by value** |
| `UnaryOp<E, Op>` | Intermediate node for elementwise unary ops | Same storage rule as BinaryOp |
| `BroadcastView<E>` | Wraps an expression and exposes a different (broadcast-target) shape with stride translation | Separate Expression node — see "Broadcasting algorithm" below |
| `ReduceExpression<E, Op, Axis>` | Lazy reduction node; rank-shrunk shape; **eager** evaluation when materialized for sanity, see Pattern 6 | Header-only template |
| `shape_t` / strides | `std::vector<std::size_t>` shape, derived row-major strides via `compute_strides()` | Free functions in `shape.h` |
| Functors `ops::Add/Sub/Mul/Div/Neg/Abs/Exp/Log/Sqrt` | Elementwise compute kernels passed as `Op` template parameter | `struct Add { template<class A, class B> auto operator()(A a, B b) const { return a + b; } };` |
| Operator overloads `+ - * /` etc. | Build expression trees: `Expression<L> + Expression<R> -> BinaryOp<L, Add, R>` | Free functions in `quiver::tensor` namespace, taking `const Expression<X>&` arguments — ADL-safe |

### Why Header-Only and No Pimpl

Quiver's rule (`CLAUDE.md`, `CONVENTIONS.md`): **Pimpl only when hiding private dependencies; plain value types use Rule of Zero.** The tensor subsystem hides nothing — its dependencies are `<vector>`, `<cstddef>`, `<type_traits>`, `<utility>`, `<cmath>`, all standard. There is no SQLite, no sol2, no iostream to hide. Combined with the fundamental requirement that expression-template code must be visible at every instantiation site, this forces:

- **Headers only** for `Expression`, `Tensor<T>`, `BinaryOp`, `UnaryOp`, `BroadcastView`, `ReduceExpression`, ops functors, operator overloads, shape helpers.
- **Optional `src/tensor/errors.cpp`** *only* if non-template error formatters or large helpers benefit from a single TU. Default position: do not create `src/tensor/` at all in v0.8.0; revisit if a non-template helper accumulates.

This matches the existing precedent for `Dimension` and `TimeProperties` (`include/quiver/binary/dimension.h`, `time_properties.h`) — both are header-mostly value types in a Pimpl-anchored subsystem.

### Compiler Flags

The existing `quiver_compiler_options` INTERFACE target (`cmake/CompilerOptions.cmake`) is correct as-is for tensor headers: `/W4 /permissive- /Zc:__cplusplus /utf-8` on MSVC and `-Wall -Wextra -Wpedantic` on GCC/Clang. **No new flags needed.** Specifically:

- No `/bigobj` needed — only `lua_runner.cpp` instantiates that many templates today (sol2). Tensor expression nesting is bounded by source-code expressions, not generator macros, so `.obj` size stays tractable.
- `-Wno-unused-parameter` is already on (GCC/Clang) — relevant because some CRTP impls leave reduction-axis parameters unused on certain branches.

If a particular `tests/test_tensor_*.cpp` blows up with C1128 ("number of sections exceeded") on MSVC, set `/bigobj` per-source via `set_source_files_properties` (same mechanism as `lua_runner.cpp`). Not anticipated for v0.8.0 scope.

## Recommended Project Structure

This layout mirrors `binary/` exactly — same depth, same parallel `include/`/`src/`/`tests/` triple. Differences are called out inline.

```
include/quiver/tensor/        # NEW directory — header-only public API
├── expression.h              # Expression<Derived> CRTP base + ref_selector trait
├── shape.h                   # shape_t alias, compute_strides, broadcast_shapes,
│                             #     ravel_index, validate_assignable_shapes
├── tensor.h                  # Tensor<T> owning storage; templated ctor/operator=
├── binary_op.h               # BinaryOp<L, Op, R> + operator+ operator- operator* operator/
├── unary_op.h                # UnaryOp<E, Op> + operator- (unary), abs, exp, log, sqrt
├── ops.h                     # ops::Add Sub Mul Div Neg Abs Exp Log Sqrt functors
├── broadcast.h               # BroadcastView<E> (separate Expression node) + helpers
├── reduce.h                  # ReduceExpression<E, Op, axis_t> + sum/mean/max free funcs
└── tensor.h                  # (aggregator already named — see note below)

src/tensor/                   # OPTIONAL in v0.8.0 — create only if an error formatter
                              # or large non-template helper actually needs a TU.
                              # Default: do not create.

tests/                        # NEW test files, registered in tests/CMakeLists.txt
├── test_tensor.cpp           # Storage, shape, strides, assignment, materialization
├── test_tensor_arithmetic.cpp # +, -, *, /, unary -, fn(x) — same-shape ops
├── test_tensor_broadcast.cpp # right-align rule, dim-1 stretch, error on mismatch
└── test_tensor_reduce.cpp    # sum/mean/max, axis vs full reduction, rank shrink
```

> Note on aggregator: Quiver already exposes `include/quiver/quiver.h` as an umbrella header. The tensor subsystem should **not** add itself there — keep `<quiver/tensor/...>` opt-in so that translation units that don't use tensors are not paying the template parsing cost.

### CMake Integration

Two minimal changes; both isolated.

**`src/CMakeLists.txt`** — header-only means no new sources to add to `QUIVER_SOURCES`. The headers under `include/quiver/tensor/` are picked up automatically by the existing `target_include_directories(quiver PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)` and shipped by the existing `install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/quiver ...)` rule. **Zero edits needed** for the headers to ship.

If `src/tensor/errors.cpp` is created later, append:
```cmake
set(QUIVER_SOURCES
    ...
    binary/time_properties.cpp
    tensor/errors.cpp          # Add only if needed
)
```

**`tests/CMakeLists.txt`** — append to the existing `add_executable(quiver_tests ...)` block:
```cmake
add_executable(quiver_tests
    ...existing 21 test files...
    test_tensor.cpp
    test_tensor_arithmetic.cpp
    test_tensor_broadcast.cpp
    test_tensor_reduce.cpp
)
```

That's the entire build wiring. No new target, no new option, no new dependency.

### Structure Rationale

- **`include/quiver/tensor/` mirrors `include/quiver/binary/`** — same nesting depth, same `quiver::tensor` namespace pattern (`quiver::binary` exists implicitly via `BinaryFile`/`BinaryMetadata`; tensor will use an explicit nested `quiver::tensor` namespace because the type names `Tensor`, `Expression`, `BinaryOp` are common enough that namespace separation is worth it).
- **One concept per header.** `binary_op.h` defines `BinaryOp` *and* the four arithmetic operator overloads (`+`, `-`, `*`, `/`). This mirrors `database_create.cpp` containing both create logic and its supporting helpers — co-location > artificial split.
- **`ops.h` separate from `binary_op.h`.** Functors (`Add`, `Sub`, etc.) are independent of the node type and reused by `unary_op.h`'s `Neg`. Pulling them into a single header avoids a circular-include risk.
- **`broadcast.h` is its own header.** Broadcasting is conceptually a view, not a binary op — it can wrap leaves *or* intermediates. It needs its own translation unit-equivalent header for clarity even though the file is small.
- **`reduce.h` is separate from arithmetic.** Reductions shrink rank; everything else preserves rank. Different mental model, different file.
- **`tests/test_tensor_*.cpp` mirrors source headers** — same convention as `tests/test_database_*.cpp` ↔ `src/database_*.cpp`. Test file names reveal the feature, not the header.

## Architectural Patterns

### Pattern 1: CRTP Expression Base with `self()`

**What:** A templated base class parameterized by its derived type, enabling compile-time static polymorphism without virtual functions. Every expression type (Tensor, BinaryOp, UnaryOp, BroadcastView, ReduceExpression) derives from `Expression<Derived>`. The base provides a `self()` cast to access the derived type's interface.

**When to use:** This is the load-bearing pattern. Every public function that accepts "any expression" (e.g., `operator+`, `Tensor::operator=`) takes `const Expression<E>&` so all expression types can be passed by their CRTP base, then immediately cast back to `E` at zero runtime cost.

**Trade-offs:** Compile-time dispatch (faster than virtual); but error messages are template-heavy. CRTP base cannot access derived's typedefs directly without an external trait struct (see `xtensor` pattern: `xexpression_traits<Derived>::value_type`).

**Example** (canonical Eigen/xtensor/Wikipedia shape — see Sources):
```cpp
namespace quiver::tensor {

template <class Derived>
class Expression {
public:
    Derived& self() { return static_cast<Derived&>(*this); }
    const Derived& self() const { return static_cast<const Derived&>(*this); }

    std::size_t size() const { return self().size(); }
    const shape_t& shape() const { return self().shape(); }
    auto eval(std::size_t i) const { return self().eval(i); }
    // No virtual dtor: derived classes only, never deleted polymorphically.
protected:
    Expression() = default;
};

}  // namespace quiver::tensor
```

### Pattern 2: Leaf vs Intermediate Storage via `ref_selector`

**What:** Expression nodes must hold their operands. The lifetime rule, distilled from Eigen, xtensor, Blaze, and the canonical Wikipedia example: **leaves (named lvalues like `Tensor<T>`) are held by `const&`, intermediate temporaries are held by value.** Otherwise an intermediate's reference dangles when the temporary it pointed at is destroyed at end of the full expression.

**When to use:** Every BinaryOp, UnaryOp, BroadcastView, ReduceExpression. The selection happens via a trait that maps "leaf type → const-reference; otherwise value":

```cpp
namespace quiver::tensor::detail {

template <class T>
struct ref_selector {
    using type = T;  // intermediate: store by value
};

// Leaves (storage owners) are held by const&
template <class T>
struct ref_selector<Tensor<T>> {
    using type = const Tensor<T>&;
};

}  // namespace quiver::tensor::detail
```

Then `BinaryOp` uses it:

```cpp
template <class L, class Op, class R>
class BinaryOp : public Expression<BinaryOp<L, Op, R>> {
    typename detail::ref_selector<L>::type lhs_;
    typename detail::ref_selector<R>::type rhs_;
public:
    BinaryOp(const L& lhs, const R& rhs) : lhs_(lhs), rhs_(rhs) {}
    auto eval(std::size_t i) const { return Op{}(lhs_.eval(i), rhs_.eval(i)); }
    // shape() and size() forwarded to lhs_/rhs_ + broadcast_shapes() if applicable
};
```

**Trade-offs:** Adds a tiny trait specialization burden — every new leaf type must specialize `ref_selector`. For v0.8.0 there is exactly one leaf (`Tensor<T>`), so the cost is one specialization. Eigen named this `internal::ref_selector` — that name is fine to reuse since this code lives under `quiver::tensor::detail`, not `Eigen`.

**Example of the pitfall this prevents:**
```cpp
auto e = a + b + c;  // BinaryOp<BinaryOp<Tensor, Add, Tensor>, Add, Tensor>
                     // The inner BinaryOp is a temporary; outer must hold it BY VALUE.
                     // If the outer held it by const&, e is dangling at the next ;
Tensor<double> d = e;  // OK if stored by value; UB if stored by ref.
```

### Pattern 3: Materialization via Templated Assignment

**What:** Assignment from any `Expression<E>` to a `Tensor<T>` is the *only* point where the loop runs. The trigger is `Tensor<T>::operator=<E>(const Expression<E>&)`.

**When to use:** Always. There is no other materialization path. Element-access methods like `t(i, j)` on a Tensor read the storage; on an intermediate they invoke `eval(i)` recursively — but the canonical materialization is the assignment loop.

**Trade-offs:** Single canonical loop = predictable codegen and easy to reason about. The loop is `for (i = 0; i < size; ++i) data_[i] = expr.eval(i);`. Op fusion happens automatically because every `eval` chain inlines.

**Example:**
```cpp
template <class T>
class Tensor : public Expression<Tensor<T>> {
public:
    // Construction from an expression — runs the loop.
    template <class E>
    Tensor(const Expression<E>& expr) {
        const auto& e = expr.self();
        shape_ = e.shape();
        strides_ = compute_strides(shape_);
        data_.resize(e.size());
        assign(e);
    }

    // Assignment from an expression — runs the loop.
    template <class E>
    Tensor& operator=(const Expression<E>& expr) {
        const auto& e = expr.self();
        validate_assignable_shapes(shape_, e.shape());  // throws on mismatch
        assign(e);
        return *this;
    }

    // Self interface (so Tensor *is* an Expression)
    std::size_t size() const { return data_.size(); }
    const shape_t& shape() const { return shape_; }
    T eval(std::size_t i) const { return data_[i]; }

private:
    template <class E>
    void assign(const E& e) {
        // Single, predictable, fused loop:
        for (std::size_t i = 0; i < e.size(); ++i) {
            data_[i] = static_cast<T>(e.eval(i));
        }
    }

    std::vector<T> data_;
    shape_t shape_;
    shape_t strides_;
};
```

Note `validate_assignable_shapes` follows Quiver's three error patterns (`CLAUDE.md`):
```cpp
// Pattern 1 — Cannot {operation}: {reason}
throw std::runtime_error("Cannot assign tensor: shape mismatch (target [" +
    fmt(target_shape) + "], source [" + fmt(source_shape) + "])");
```

### Pattern 4: Free-Function Operator Overloads in the Tensor Namespace

**What:** Each elementwise operator (`+`, `-`, `*`, `/`, unary `-`) is a free function in `quiver::tensor` taking `const Expression<L>&` and `const Expression<R>&`, returning a `BinaryOp<L, ops::Add, R>`-style intermediate.

**When to use:** Always. Defining operators as free functions in the same namespace as `Expression<>` makes them findable by ADL when callers write `a + b` for tensor arguments. Defining them inside the class would prevent symmetric promotion (e.g., `scalar + tensor`).

**Trade-offs:** Each operator must be declared in a header; instantiations are deduplicated by the linker (see Pattern 5 / Anti-Pattern 2 on ODR).

**Example:**
```cpp
// In binary_op.h, namespace quiver::tensor
template <class L, class R>
auto operator+(const Expression<L>& l, const Expression<R>& r) {
    return BinaryOp<L, ops::Add, R>(l.self(), r.self());
}
template <class L, class R>
auto operator*(const Expression<L>& l, const Expression<R>& r) {
    return BinaryOp<L, ops::Mul, R>(l.self(), r.self());
}
// ...same for - / unary - via UnaryOp
```

### Pattern 5: Shape Model — `vector<size_t>` + Row-Major Strides

**What:** A shape is `using shape_t = std::vector<std::size_t>`, dynamic-rank, runtime-checked. Strides are computed row-major (last dim has stride 1) via a free function `compute_strides(shape) -> shape_t`.

**When to use:** Default. For v0.8.0 (CPU-only, dense, dynamic-rank intentional), this matches NumPy/xtensor and is simpler than Eigen's mixed compile-time/runtime sizes.

**Trade-offs:** vs **xtensor**'s approach (also dynamic-rank `std::vector<size_t>` plus a fixed-size `std::array<size_t, N>` variant `xtensor<T, N>`) — Quiver chooses dynamic-only for v0.8.0 to keep the surface small. Adding a fixed-rank variant is mechanical (template the rank, swap `vector` for `array`) and can wait for a later milestone if profiling shows allocation pressure on small tensors.

**Trade-offs:** vs **Eigen**'s heavily compile-time approach — Eigen pays for shape generality with significant compile times. Quiver's tensor subsystem is dwarfed by sol2 in compile cost, so this trade-off is moot.

**Example:**
```cpp
// shape.h, namespace quiver::tensor
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
    std::size_t n = 1;
    for (auto d : shape) n *= d;
    return n;
}
```

### Pattern 6: Broadcasting — Separate `BroadcastView<E>` Wrapper Node

**What:** Broadcasting is a *view-shaped* expression that wraps an inner expression and exposes a different shape. The algorithm is **NumPy's right-align rule** (`numpy.org/doc/stable/user/basics.broadcasting.html`):

```
1. Right-align the two shapes (pad the shorter on the LEFT with 1s).
2. For each aligned dimension pair (a, b):
     - if a == b -> dim is a
     - if a == 1 -> dim is b (left tensor stretches)
     - if b == 1 -> dim is a (right tensor stretches)
     - else -> error: "Cannot broadcast: incompatible shapes [...] vs [...]"
```

**When to use:** When a `BinaryOp<L, Op, R>` is constructed and `L::shape() != R::shape()`. Two architectural options exist:

**Option A — Separate `BroadcastView<E>` wrapper node** (RECOMMENDED for Quiver):

```
operator+(L, R)
  └─> compute target_shape = broadcast_shapes(L.shape(), R.shape())
  └─> if (L.shape() != target_shape)  L' = BroadcastView(L, target_shape)
  └─> if (R.shape() != target_shape)  R' = BroadcastView(R, target_shape)
  └─> return BinaryOp<L', Op, R'>(L', R')
```

`BroadcastView<E>` stores the inner expression (using `ref_selector`) plus the target shape and a stride map (per-dim: stride of inner if dim matches; 0 if dim was stretched from size 1; 0 if dim was prepended). `eval(i)` translates the linear `i` through the target strides → multi-index → inner strides (with broadcasted dims contributing 0).

**Option B — Broadcasting baked into BinaryOp**: BinaryOp itself owns the broadcast logic; on `eval(i)` it computes per-operand offsets. xtensor does this *internally* but Eigen's general design (and Blaze) prefer separate Expression nodes. Wrapping is cleaner: each node has one responsibility, and reducing-then-broadcasting compositions stay readable.

**Recommendation: Option A.** It mirrors xtensor's `xbroadcast` directly (`xtensor.readthedocs.io/en/latest/api/xbroadcast.html`) and Quiver's existing precedent of "one concept per type" (Database vs Binary vs LuaRunner). It also gives us a public `broadcast_to(expr, shape)` function for free, which is useful for explicit broadcasts.

**Trade-offs:**
- Option A is one extra node-type to maintain, and one extra layer of `eval` indirection — but inlining flattens it at -O2.
- Option B is faster to type for v0.8.0 but mixes concerns and gets harder when reductions land.

**Example:**
```cpp
// broadcast.h, namespace quiver::tensor
inline shape_t broadcast_shapes(const shape_t& a, const shape_t& b) {
    const std::size_t n = std::max(a.size(), b.size());
    shape_t out(n);
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t da = (k < n - a.size()) ? 1 : a[k - (n - a.size())];
        std::size_t db = (k < n - b.size()) ? 1 : b[k - (n - b.size())];
        if (da == db || da == 1 || db == 1) {
            out[k] = std::max(da, db);
        } else {
            throw std::runtime_error("Cannot broadcast: incompatible shapes");
        }
    }
    return out;
}

template <class E>
class BroadcastView : public Expression<BroadcastView<E>> {
    typename detail::ref_selector<E>::type inner_;
    shape_t target_shape_;
    shape_t inner_stride_map_;  // per target-dim: inner-stride (or 0 if stretched/padded)
public:
    BroadcastView(const E& inner, shape_t target);
    const shape_t& shape() const { return target_shape_; }
    std::size_t size() const { return total_size(target_shape_); }
    auto eval(std::size_t i) const {
        // translate linear i (in target strides) -> inner linear offset
        std::size_t inner_i = 0;
        // ... per-dim arithmetic using target strides + inner_stride_map_
        return inner_.eval(inner_i);
    }
};
```

### Pattern 7: Reductions — Lazy Node, Eager Evaluation on Materialization

**What:** `sum(expr, axis)`, `mean(expr, axis)`, `max(expr, axis)` build a `ReduceExpression<E, Op, AxisSpec>` that exposes the rank-shrunk shape but, **on materialization**, runs an eager loop populating a temporary buffer. The lazy wrapper is for composability (`sum(a + b * c, axis=0) + d`); the actual reduction loop is eager because it fundamentally cannot be expressed as a per-output-index `eval(i)` chain that fuses with surrounding ops without re-doing work.

**When to use:** This is the standard pragmatic policy in Eigen (`colwise()/rowwise()` partial reductions are lazy at the type level but materialize internally) and xtensor (`xreducer` similarly).

**Trade-offs:**
- Pure laziness (recompute on every `eval(i)`) re-does O(reduced_size) work per element of the consumer.
- Pure eagerness (compute and store immediately) gives up fusion opportunities.
- The "lazy node, eager-on-materialize" hybrid: `ReduceExpression` lazily holds its source, but the first time someone calls `eval(i)` on it, it computes the full reduction into an internal `std::vector<T>` cache and serves subsequent `eval(i)` from cache. This loses fusion across the reduction boundary but preserves it on either side, and is dramatically simpler.

**Result-rank rule:** `expr.shape() = [d0, d1, ..., dN-1]`; reducing along `axis=k` (single axis) gives shape `[d0, ..., d_{k-1}, d_{k+1}, ..., d_{N-1}]` — drop axis k. Reducing all axes (`axis=std::nullopt`) yields shape `[]` (a scalar).

**Example sketch:**
```cpp
// reduce.h, namespace quiver::tensor
template <class E, class Op>
class ReduceExpression : public Expression<ReduceExpression<E, Op>> {
    typename detail::ref_selector<E>::type inner_;
    std::optional<std::size_t> axis_;
    shape_t out_shape_;
    mutable std::optional<std::vector<value_type_t<E>>> cache_;
public:
    ReduceExpression(const E& inner, std::optional<std::size_t> axis);
    const shape_t& shape() const { return out_shape_; }
    std::size_t size() const { return total_size(out_shape_); }
    auto eval(std::size_t i) const {
        if (!cache_) materialize();
        return (*cache_)[i];
    }
private:
    void materialize() const;  // runs the eager reduction loop
};

template <class E>
auto sum(const Expression<E>& e, std::optional<std::size_t> axis = std::nullopt) {
    return ReduceExpression<E, ops::Add>(e.self(), axis);
}
```

For v0.8.0, this is acceptable — the alternative (pure laziness with re-computation on every consumer `eval`) breaks performance contracts users expect from a numerical lib.

## Data Flow

### Compile-Time Type Construction

For `Tensor<double> d = a + b * c;`, where `a`, `b`, `c` are `Tensor<double>` lvalues:

```
operator*(b, c)
  Resolves to:  operator*(const Expression<Tensor<double>>&, const Expression<Tensor<double>>&)
  Returns:      BinaryOp<Tensor<double>, ops::Mul, Tensor<double>>     [TEMPORARY #1]
                  └ lhs_: const Tensor<double>&  (b)         [ref_selector picks ref]
                  └ rhs_: const Tensor<double>&  (c)         [ref_selector picks ref]

operator+(a, TEMPORARY #1)
  Resolves to:  operator+(const Expression<Tensor<double>>&,
                          const Expression<BinaryOp<Tensor<double>, ops::Mul, Tensor<double>>>&)
  Returns:      BinaryOp<Tensor<double>, ops::Add,
                          BinaryOp<Tensor<double>, ops::Mul, Tensor<double>>>  [TEMPORARY #2]
                  └ lhs_: const Tensor<double>&  (a)
                  └ rhs_: BinaryOp<Tensor, ops::Mul, Tensor>     [ref_selector picks BY VALUE]
                          └ lhs_: const Tensor<double>& (b)
                          └ rhs_: const Tensor<double>& (c)

Tensor<double> d = TEMPORARY #2;   // invokes Tensor<double>::Tensor<E>(const Expression<E>&)
```

### Runtime: When the Loop Runs

```
Tensor<double>::Tensor<BinaryOp<...>>(expr)
  └ expr.self() returns const ref to the BinaryOp tree
  └ shape_ = e.shape()         // BinaryOp::shape() returns broadcast of lhs/rhs shapes
  └ data_.resize(e.size())     // total_size(shape_)
  └ assign(e):
      for (i = 0; i < e.size(); ++i)
          data_[i] = e.eval(i)
                       └ ops::Add{}(a.eval(i), inner.eval(i))
                                                  └ ops::Mul{}(b.eval(i), c.eval(i))
                                                                  └ b.data_[i] * c.data_[i]
                       └ a.data_[i] + (b.data_[i] * c.data_[i])
```

After inlining at `-O2`, this collapses to a single fused loop:
```cpp
for (std::size_t i = 0; i < n; ++i)
    d.data_[i] = a.data_[i] + b.data_[i] * c.data_[i];
```

Zero intermediate buffers, zero temporary allocations, single pass — this is the entire point of expression templates.

### Stack vs Heap

For `Tensor<double> d = a + b * c;`:

| Object | Where it lives | Lifetime |
|--------|----------------|----------|
| `a`, `b`, `c` storage (`std::vector<double>`) | Heap (vector buffer) | Caller-managed |
| `a`, `b`, `c` `Tensor` headers (shape, strides, ptr) | Wherever caller put them — usually stack | Caller-managed |
| `BinaryOp<T, Mul, T>` (TEMPORARY #1) | Stack (rvalue, lives until end of full-expression) | Until `;` |
| `BinaryOp<T, Add, BinaryOp<...>>` (TEMPORARY #2) | Stack | Until `;` |
| `d.data_` | Heap (vector buffer, `n * sizeof(double)` bytes) | Until `d` destructs |
| `d` header | Caller's stack | Until `d` destructs |

**Key invariants:**
- **No new heap allocation per intermediate.** Only the final destination `d` allocates (its `std::vector<double>` resize).
- **All intermediates are stack values.** Thanks to ref_selector, the *temporary* BinaryOp is held by value inside the outer BinaryOp — so the whole expression tree is one stack frame's worth of bytes.
- **The `const&` to `a` (held inside the outer BinaryOp) is safe** because `a` outlives the full-expression statement.

## Scaling Considerations

This is a numerical library — "scale" means tensor size and expression depth, not concurrent users.

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Small tensors (<1k elements), shallow expressions | No-op. The current design wins on compile-time fusion alone. |
| Medium tensors (1k–10M elements), 3–6 level expressions | No-op. Inlining still fully fuses; loop is memory-bandwidth-bound. |
| Large tensors (10M–1B elements), deep expressions (7+ levels) | Consider parallel `assign()` (OpenMP `#pragma omp parallel for` over the index range) — single change point in `Tensor<T>::assign(e)`. |
| Reductions over very large arrays | The eager-on-materialize cache (Pattern 7) is fine until you need parallel reductions; switch to a chunked parallel reduction in `ReduceExpression::materialize()`. |
| 100M+ element tensors crossing CPU cache | Needs blocking — out of scope for v0.8.0 per `PROJECT.md` ("CPU-only for v0.8.0 and likely well beyond"). |

### Scaling Priorities (when v0.8.0 ships and is used)

1. **First bottleneck (memory bandwidth):** The fused loop is bound by RAM throughput, not compute. Adding parallelism will help on multi-core machines for large tensors. Single-line change in `Tensor::assign`.
2. **Second bottleneck (reduction caching):** If users compose many reductions (`sum(sum(x, axis=0), axis=0)`), the per-reduction cache adds bytes. Mitigation: lazy evaluation of intermediate reductions only when the final consumer reads them — but this is a v0.10.0+ optimization concern, not a v0.8.0 scope item.
3. **Third bottleneck (template compile time):** If sol2 in `lua_runner.cpp` doesn't already saturate compile time, very deep tensor expressions might. Solution if it ever bites: extern template instantiations for the most common `Tensor<double>` ops in `src/tensor/extern_instantiations.cpp`.

## Anti-Patterns

### Anti-Pattern 1: Storing Expressions in `auto`

**What people do:** `auto e = a + b; auto e2 = e * c; Tensor<double> d = e2;`

**Why it's wrong:** `auto e = a + b` deduces `e` as `BinaryOp<Tensor, ops::Add, Tensor>`. That object holds `const Tensor&` to `a` and `b` (safe — they outlive). But subsequent `auto e2 = e * c` returns a `BinaryOp<BinaryOp<...>, ops::Mul, Tensor>` whose `lhs_` is a *value-stored copy* of `e` (good per ref_selector). Now `e` and `e2` are both stack values. Fine here — but the moment any *intermediate* in the chain is a temporary held by the outer node, deferred materialization (storing `e2` and using it many statements later) breaks once any leaf goes out of scope. **Eigen explicitly warns: "do not store the result of an expression in `auto`."** Quiver should reuse this rule.

**Do this instead:** Materialize early — `Tensor<double> d = a + b * c;` runs the loop *now* and writes to `d`'s storage. Use `auto` only for one-shot expressions consumed in the same full-expression statement. If a user *needs* a deferred view, give them an explicit `materialize()` or assign to a `Tensor<T>` immediately.

### Anti-Pattern 2: ODR Violation via Operator Overloads in Different Namespaces

**What people do:** Define `operator+(Tensor, Tensor)` once in a header, then *also* provide an alternate overload in user code or in a "compat" header that's pulled into some translation units but not others.

**Why it's wrong:** Per cppreference (`en.cppreference.com/cpp/language/definition`): even with identical tokens, ODR is violated if name lookup in different TUs resolves to different entities. With operator overloads in headers, this is a real silent-failure risk — different TUs link to different operator instantiations that may have different observable behavior.

**Do this instead:** Define every tensor operator overload exactly once, in `quiver::tensor` namespace, in `binary_op.h` / `unary_op.h` / `reduce.h`. Forbid users from adding overloads in `quiver::tensor` (no ADL extension points). All operators are templates of `Expression<L>` arguments — users can specialize their own derived expression types but must not redefine `operator+` for tensors.

### Anti-Pattern 3: Functors in `std::` Namespace (ADL Hijacking)

**What people do:** Define `Add`, `Sub`, etc. functors in `std::` or import them via `using std::abs;` in tensor headers.

**Why it's wrong:** ADL (`en.cppreference.com/cpp/language/adl`) finds operators based on argument namespace. Putting tensor-specific functors in `std::` makes them findable when ADL processes `std::vector<Tensor<double>>` or other unrelated types — turning `abs(some_std_vector)` into a tensor-domain lookup. This is "ADL hijacking."

**Do this instead:** All tensor functors live in `quiver::tensor::ops::` (e.g., `quiver::tensor::ops::Add`). Free-function aliases (`abs`, `exp`, `log`, `sqrt`) live in `quiver::tensor` and forward to `UnaryOp<E, ops::Abs>` etc. Users write `quiver::tensor::abs(t)` or, after `using namespace quiver::tensor;`, `abs(t)` — but never accidentally collide with `std::abs`.

### Anti-Pattern 4: Eager Materialization in Copy Constructors

**What people do:** Define `BinaryOp(const BinaryOp&)` to immediately compute the elements into a buffer, "for safety."

**Why it's wrong:** The whole point of expression templates is *not* allocating intermediate buffers. An eager copy constructor defeats that — every time an intermediate is value-stored (which ref_selector does deliberately for non-leaf operands), you'd pay an O(N) allocation and copy. You'd also bloat compile output, since the inlined fused loop is replaced by an out-of-line eager evaluation.

**Do this instead:** Let copy/move constructors of intermediates be the compiler's default (= Rule of Zero, matching Quiver's value-type convention). Ensure leaves (`Tensor<T>`) have correct `std::vector<T>` move semantics so move-construction of intermediates is cheap.

### Anti-Pattern 5: Passing Expressions to Functions by Value

**What people do:**
```cpp
template <class E>
Tensor<double> compute(Expression<E> expr) { /* ... */ }   // Slicing! And copies!
```

**Why it's wrong:** Pass-by-value of `Expression<E>` slices to the base type, losing the derived information (CRTP cast still works through `self()` but if you accept by `Expression<E>` value, derived state copies in via slicing — surprising and wasteful).

**Do this instead:**
```cpp
template <class E>
Tensor<double> compute(const Expression<E>& expr) { /* ... */ }
```

Always `const Expression<E>&` for parameters. Quiver's existing `.clang-tidy` already flags some of this via `bugprone-*` checks.

### Anti-Pattern 6: Forgetting `noexcept` on Move Operations (Performance Cliff)

**What people do:** Default move constructors without thinking about `noexcept`.

**Why it's wrong:** `std::vector<T>` uses `noexcept(std::is_nothrow_move_constructible_v<T>)` to decide whether to *move* or *copy* during reallocation. If `Tensor<T>`'s move constructor isn't `noexcept`, a `std::vector<Tensor<T>>` will copy on growth — silent O(N²) total bytes moved.

**Do this instead:** Mirror Database's pattern (`include/quiver/database.h:24-29`):
```cpp
Tensor(Tensor&& other) noexcept = default;
Tensor& operator=(Tensor&& other) noexcept = default;
```
Same for `BinaryOp`, `UnaryOp`, `BroadcastView`, `ReduceExpression`. With `std::vector<T>` and `shape_t` (also `std::vector`) as members, all move constructors are noexcept-eligible.

### Anti-Pattern 7: Throwing in `eval(i)` Hot Path

**What people do:** Bounds-check inside `eval(std::size_t i)` and throw on out-of-range.

**Why it's wrong:** `eval(i)` is called O(N) times in the assignment loop. A throw-on-bounds check defeats vectorization and inlining.

**Do this instead:** Validate shapes at expression-construction time (in operator overloads, BinaryOp constructors, BroadcastView constructors). After validation, `eval(i)` is contractually safe and never throws. This matches Quiver's existing principle: "Clean code over defensive code (assume callers obey contracts, avoid excessive null checks)" (`CLAUDE.md`).

## Integration Points

### External Services

The tensor subsystem has **zero external service dependencies** in v0.8.0. No SQLite, no file I/O, no Lua, no network. Pure C++ standard library + headers.

### Internal Boundaries (v0.8.0)

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `Tensor<T>` ↔ `BinaryOp/UnaryOp/...` | Direct C++ template instantiation | All within `quiver::tensor` namespace; no FFI |
| Tensor ↔ Database | **None in v0.8.0** | Deferred to v0.9.0 (see below) |
| Tensor ↔ Binary | **None in v0.8.0** | Deferred indefinitely; no roadmap need yet |
| Tensor ↔ LuaRunner | **None in v0.8.0** | Deferred until tensor C API exists |

### Future Integration Points (DO NOT IMPLEMENT in v0.8.0 — sketch only)

These are documented here so the v0.8.0 design does not foreclose them, NOT as v0.8.0 scope.

#### v0.9.0 — Database Bridge

```cpp
// include/quiver/database.h — additional method
class Database {
public:
    // ...existing methods...
    quiver::tensor::Tensor<double>
    read_tensor(const std::string& collection,
                const std::string& vector_attr);
    // Reads a vector group as a 1-D Tensor<double>.

    void update_tensor(const std::string& collection,
                       int64_t id,
                       const std::string& vector_attr,
                       const quiver::tensor::Tensor<double>& tensor);
};

// include/quiver/element.h — overload
Element& set(const std::string& key, const quiver::tensor::Tensor<double>& t);
// Stores tensor as a vector attribute (1-D) or refuses with
// "Cannot set tensor as scalar: rank > 1" for higher ranks (deferred to v1.0).
```

Design constraint to preserve in v0.8.0: `Tensor<T>` exposes `data() -> const T*` and `size()` as cheap accessors so the future bridge can pull out a `const double*` for SQLite without extra copies.

#### v0.9.0 — Tensor C API (immediate-mode, opaque handle)

Expression templates **cannot cross FFI** — every C-API tensor operation is an immediate-mode wrapper that materializes a `Tensor<T>` and returns an opaque handle.

```cpp
// include/quiver/c/tensor.h (FUTURE — sketch only, NOT v0.8.0)
typedef struct quiver_tensor quiver_tensor_t;

quiver_error_t quiver_tensor_create_double(
    const size_t* shape, size_t rank,
    quiver_tensor_t** out_tensor);

quiver_error_t quiver_tensor_add(
    const quiver_tensor_t* a, const quiver_tensor_t* b,
    quiver_tensor_t** out_result);  // Materializes immediately.

quiver_error_t quiver_tensor_data(
    const quiver_tensor_t* t, const double** out_data, size_t* out_size);

void quiver_tensor_close(quiver_tensor_t* t);
void quiver_tensor_free_double_array(double*);  // for read-out-and-copy
```

Design constraint to preserve in v0.8.0: `Tensor<T>` is move-only (per Quiver convention for resource owners) — copy is deleted, move-noexcept defaulted. Same shape as `BinaryFile`, `Database`, `LuaRunner`.

#### v0.10.0 — Bindings

| Binding | Pattern |
|---------|---------|
| Julia | `Tensor` userdata over `quiver_tensor_t*`; `+`, `-`, `*`, `/` overloaded; `materialize()` no-op (already eager via C API) |
| Dart | `Tensor` class wrapping `Pointer<quiver_tensor_t>`; `Tensor.fromList()`, `operator +` etc. |
| Python | Numpy compatibility goal — `__array_interface__` on `Tensor` so `numpy.asarray(t)` is zero-copy |
| JS | `Tensor` with TypedArray bridges (`Float64Array.from(tensor)`) |
| Lua | `Tensor` userdata; metamethods `__add`, `__sub`, `__mul`, `__div`; same as Julia pattern |

The thin-binding rule (`CLAUDE.md`) means none of these add logic — they all marshal into the C API.

## Sources

### Authoritative — Existing Quiver Codebase (HIGH confidence)
- `C:/Development/DatabaseTmp/quiver/CLAUDE.md` — top-level principles and patterns
- `C:/Development/DatabaseTmp/quiver/.planning/PROJECT.md` — v0.8.0 scope, constraints, deferred work
- `C:/Development/DatabaseTmp/quiver/.planning/codebase/ARCHITECTURE.md` — three-layer pattern
- `C:/Development/DatabaseTmp/quiver/.planning/codebase/STRUCTURE.md` — directory layout precedent
- `C:/Development/DatabaseTmp/quiver/.planning/codebase/CONVENTIONS.md` — Pimpl vs Rule of Zero rules; error patterns
- `C:/Development/DatabaseTmp/quiver/include/quiver/binary/binary_file.h` — Pimpl precedent
- `C:/Development/DatabaseTmp/quiver/include/quiver/binary/binary_metadata.h` — value-type precedent
- `C:/Development/DatabaseTmp/quiver/include/quiver/binary/dimension.h` — header-mostly value type pattern
- `C:/Development/DatabaseTmp/quiver/include/quiver/export.h` — `QUIVER_API` macro
- `C:/Development/DatabaseTmp/quiver/src/CMakeLists.txt` — how subsystems hook into the build
- `C:/Development/DatabaseTmp/quiver/tests/CMakeLists.txt` — how tests are added
- `C:/Development/DatabaseTmp/quiver/cmake/CompilerOptions.cmake` — `quiver_compiler_options` interface target

### Authoritative — Canonical CRTP Expression Template References (HIGH confidence — multiple sources converge)
- [Expression templates — Wikipedia](https://en.wikipedia.org/wiki/Expression_templates) — canonical `VecExpression`/`Vec3Sum`/`operator+` example; the leaf-vs-intermediate storage rule is stated explicitly here.
- [Eigen: The class hierarchy](https://libeigen.gitlab.io/eigen/docs-nightly/TopicClassHierarchy.html) — `MatrixBase<Derived>` CRTP root.
- [xtensor — Concepts (developer docs)](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/developer/concepts.rst) — `xexpression` CRTP base, value semantics.
- [xtensor — Expressions and lazy evaluation](https://xtensor.readthedocs.io/en/latest/expression.html) — materialization semantics.
- [xtensor — `xbroadcast`](https://xtensor.readthedocs.io/en/latest/api/xbroadcast.html) — broadcasting as a separate Expression node, mirroring Pattern 6 Option A.
- [Eigen: Reductions, visitors and broadcasting](https://eigen.tuxfamily.org/dox/group__TutorialReductionsVisitorsBroadcasting.html) — `colwise/rowwise` partial-reduction pattern.
- [Blaze — EXPRESSION TEMPLATES REVISITED (Iglberger 2012)](https://blogs.fau.de/hager/files/2012/05/ET-SISC-Iglberger2012.pdf) — Blaze's smart expression-template paper; modern best practices.
- [Curiously recurring template pattern — Wikipedia](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) — CRTP fundamentals.

### Authoritative — Broadcasting Algorithm (HIGH confidence)
- [NumPy v2.4 Manual — Broadcasting](https://numpy.org/doc/stable/user/basics.broadcasting.html) — right-align rule, dim-1 stretch, mismatch error.

### Authoritative — Pitfall References (HIGH confidence)
- [cppreference — Argument-dependent lookup](https://en.cppreference.com/cpp/language/adl) — ADL behavior with operators.
- [cppreference — Definitions and ODR](https://en.cppreference.com/cpp/language/definition) — ODR violation via overload resolution.
- [cppreference — Reference initialization](https://en.cppreference.com/w/cpp/language/reference_initialization.html) — temporary lifetime extension rules.
- [GCC's new `-Wdangling-reference` warning](https://trofi.github.io/posts/264-gcc-s-new-Wdangling-reference-warning.html) — modern compiler diagnostics for the dangling-reference pitfall.

### Supporting (MEDIUM confidence)
- [Fluent C++ — CRTP](https://www.fluentcpp.com/2017/05/12/curiously-recurring-template-pattern/) — accessible CRTP explainer.
- [Modernes C++ — C++ is Lazy: CRTP](https://www.modernescpp.com/index.php/c-is-still-lazy/) — CRTP with expression templates.

---

*Architecture research for: lazy CRTP tensor framework integrated with Quiver*
*Researched: 2026-05-05*
