# Pitfalls Research

**Domain:** Lazy CRTP Expression Template tensor framework added to Quiver (C++20 library, v0.7.9 -> v0.8.0)
**Researched:** 2026-05-05
**Confidence:** HIGH for CRTP/Eigen-derived pitfalls (multiple Eigen and xtensor docs corroborate); MEDIUM for Quiver-integration-specific pitfalls (extrapolated from `.planning/codebase/CONCERNS.md` and the existing Binary subsystem layout); LOW for predicted FFI-bridge pitfalls (the bridge milestone is v0.9.0 and the surface is not designed yet).

This document catalogues the failure modes that real-world C++ tensor libraries (Eigen, xtensor, Blaze) have hit, plus the Quiver-specific ways those failures will manifest given the codebase's conventions, build matrix (MSVC + GCC + Clang), and the deferred-FFI roadmap. Each pitfall includes prevention, detection signal, and the v0.8.0 phase that should address it.

The v0.8.0 phase numbering used below corresponds to the typical lazy-tensor build-out for ROADMAP.md:
- **Phase 1** -- Storage / `Tensor<T>` value type, shape, strides, indexing
- **Phase 2** -- Element-wise binary/unary ops, scalar broadcast, materialization (assignment from expression)
- **Phase 3** -- NumPy-style shape broadcasting between non-matching shapes
- **Phase 4** -- Reductions (sum, mean, max) with optional axis
- **Phase 5** -- Hardening: compile-time budget, error-message quality, cross-compiler CI, test-coverage closure of "looks done but isn't" cases
The roadmapper may renumber; pitfalls keep their *content* mapping ("aliasing decision belongs with op materialization, not with reductions") regardless.

---

## Critical Pitfalls

### Pitfall 1: Dangling references in expression chains stored to `auto`

**What goes wrong:**
The classic CRTP expression template stores operands as `const Lhs&` / `const Rhs&` inside `BinaryOp`. This works for the in-place case `Tensor c = a + b;` because the temporary `BinaryOp<...>` lives only until the end of the statement. It silently breaks when intermediate expressions are stashed in `auto` variables and later combined:

```cpp
auto e = a + b;             // BinaryOp holds (a&, b&) — fine, both live
{
    Tensor d = some_temp(); // d is a Tensor; lifetime is the brace block
    auto f = e + d;         // BinaryOp holds (e&, d&) — BUT e is the prior expr, d is local
    Tensor result;          // ...elsewhere later...
    result = f;             // d is gone — segfault, or worse: silent corruption
}
```

The same bug shows up via `auto C = ((A+B).eval()).transpose();` -- the `eval()` returns a temporary `Tensor`, the `transpose()` view stores a reference to it, the temporary dies at the semicolon, and `C` is a view onto destroyed memory. Eigen documents this exact pitfall and explicitly recommends "do not use the auto keyword with Eigen's expressions, unless you are 100% sure about what you are doing." [Eigen common pitfalls](https://libeigen.gitlab.io/eigen/docs-nightly/TopicPitfalls.html).

**Why it happens:**
Reference-storage is the standard CRTP recipe in every blog tutorial ([Wuwei Lin's lazy evaluation post](https://wuwei.io/post/2018/06/lazy-evaluation-with-expression-templates-1/), Modernes C++, the Wikipedia Expression Templates article). It is correct *for single-statement use* and is the cheapest possible storage. The dangling-reference case appears only under usage patterns the tutorials do not cover.

**How to avoid (decided policy for v0.8.0):**
Use Eigen's `ref_selector`/`nested_eval` storage strategy: leaf `Tensor`s are stored by const reference (they are stable, non-owning operands); produced expressions (the temporary nodes themselves) are stored by value. The selector reduces to:
- If operand is a `Tensor<T>` (leaf, owning) -> store `const Tensor<T>&`.
- If operand is an `Expression` (produced node) -> store by value (it is cheap, holds only references/values itself).

This matches how Eigen's `internal::ref_selector<T>::type` and `nested_eval<T,N>` are organised. It costs one shallow copy of the small expression node, eliminates the failure mode entirely, and is the policy used by every production tensor library that has stabilised the issue.

Document the rule in the public header next to the `BinaryOp` template, and put a `static_assert` in `BinaryOp`'s constructor confirming `lhs_t` and `rhs_t` are the right kind (reference for leaf, value for expr). Explicitly forbid storing leaves by value (a leaf can be a 100-MB tensor; copying is unacceptable).

**Warning signs:**
- ASan / valgrind reporting heap-use-after-free during `assign(expr)`.
- A test that builds `auto e = ...; { auto f = e + g; } /* use f outside */` and crashes only at `-O0` (Release optimisations sometimes hide it by inlining).
- Compiler diagnostic `-Wreturn-stack-address` / `-Wdangling-reference` (GCC 13+) firing on a tensor-returning helper.

**Phase to address:** **Phase 2 (element-wise ops, materialization)** -- this is where the storage policy is decided. Once decided wrong, recovery requires touching every `BinaryOp`/`UnaryOp` instantiation. **Cost of late recovery: HIGH.**

---

### Pitfall 2: `auto x = a + b;` returns an unevaluated proxy

**What goes wrong:**
Users who are not familiar with expression templates write:
```cpp
auto x = a + b;       // x is a BinaryOp<Plus, Tensor, Tensor>, not Tensor
print(x);             // prints a type the user does not recognise
double v = x(0,0);    // re-evaluates the addition every call
for (...) y(i) = x(i); // recomputes x(i) every loop iteration
```
Two distinct failures here: (a) the type is surprising and surfaces in compile errors when the user passes `x` to a function expecting `Tensor`; (b) lazy re-evaluation is *the wrong choice* when the same expression is read many times -- the user expected a stored array and got a recipe.

**Why it happens:**
`auto` always deduces the producer's return type. For `operator+(Tensor, Tensor)` returning `BinaryOp<...>`, that *is* the proxy. The user is encoding an implicit "evaluate now" assumption that the language does not enforce.

**How to avoid:**
1. **Document in bold at the top of `tensor.h`**: "`auto t = expr;` does not evaluate. Use `Tensor t = expr;` or `auto t = expr.eval();` to materialize."
2. **Provide `.eval()`**: a member that materializes into a `Tensor<value_type>` of the result shape. Eigen and xtensor both expose this.
3. **Add a `[[nodiscard]] explicit` mark on the expression's conversion-to-`Tensor` operator** so accidental drops are warned. (Optional; consider whether it noises the API.)
4. **Resist over-design**: do *not* try to make `auto` Do The Right Thing via fancy proxy gymnastics. Eigen tried, xtensor tried, both gave up and just put it in the docs. Your time is better spent on broadcasting and reductions.

**Warning signs:**
- Compile errors mentioning `BinaryOp<...>` instead of `Tensor<...>` in user code.
- Slow loops where the same expression is indexed repeatedly (profile shows arithmetic on every `op[]` call).

**Phase to address:** **Phase 2** -- this is decided when the result type of `operator+` is fixed. Add the `.eval()` method and the doc note in the same commit.

---

### Pitfall 3: Aliasing -- `a = a + a` and `a = a.transpose()` silently corrupt

**What goes wrong:**
For element-wise ops the lazy policy `for i: a[i] = a[i] + a[i]` is correct (RHS reads `a[i]`, LHS writes `a[i]`, no overlap problem). But the moment the framework adds operations whose output index depends on a *different* input index -- transpose, reshape-with-stride, a row/column slice that overlaps the destination, broadcasting that re-reads earlier rows -- the in-place lazy evaluation produces wrong results without diagnostic.

Eigen's documentation is explicit:
> "Matrix multiplication is the only operation in Eigen that assumes aliasing by default... For all other operations, Eigen assumes that there is no aliasing issue and thus gives the wrong result if aliasing does in fact occur." [Eigen Aliasing](https://libeigen.gitlab.io/eigen/docs-nightly/group__TopicAliasing.html)

For Quiver's element-wise-only v0.8.0 scope, the *immediate* aliasing exposure is small. But the moment Phase 3 (broadcasting) lands, a broadcast of size-1 dim across a destination row that overlaps the source row creates this exact bug.

**Why it happens:**
The expression-template optimisation principle is "evaluate one element at a time, write it straight into the destination". Aliasing breaks the precondition that "RHS at iteration i is independent of LHS at iteration <i".

**How to avoid:**
Make a deliberate, documented aliasing policy *now*, in Phase 2, before user code grows around the wrong assumption.

Recommended policy for v0.8.0 (matches Eigen, simpler to implement than xtensor's heuristic):
- **Element-wise ops are alias-safe by construction.** No temporary needed.
- **Broadcasting (Phase 3) is alias-safe for the canonical pattern `result = bcast_lhs + bcast_rhs` because `result` is fresh.** Alias-unsafe iff `result` aliases `bcast_lhs` or `bcast_rhs` and the broadcast crosses the alias boundary.
- **Reductions (Phase 4) into a fresh `Tensor` are alias-safe.** Reducing into a slice of self is alias-unsafe.
- **Default behaviour: assume no aliasing**, document the cases that fail, and provide `.eval()` as the user-side escape hatch (`a = (a + a.something_that_reads_old_a()).eval();`).
- Provide `noalias()` (or a similarly named hint) only if the framework grows operations where Eigen-style automatic temporaries become useful. Not needed for element-wise.

Add at least one *aliasing-failure regression test* per phase that introduces a new alias-unsafe operation, so the policy is validated against actual code.

**Warning signs:**
- A reduction or broadcast test passes with fresh-destination input but fails when the destination is `a` itself.
- "First few elements correct, rest garbage" pattern in test output.
- xtensor issue [#907](https://github.com/xtensor-stack/xtensor/issues/907) "Broadcast assign of higher dim container doesn't throw" is a related class of silent-acceptance bug that Quiver should not replicate.

**Phase to address:** **Phase 2 (op materialization)** -- the assignment operator is where the temporary-or-not decision lives. **Phase 3 broadcasting** must add broadcast-aliasing tests; **Phase 4 reductions** must add reduce-into-self tests. Without an early policy decision, Phase 3 and Phase 4 land on contradictory assumptions.

---

### Pitfall 4: Header-only template heaviness slows the entire `quiver` library compile

**What goes wrong:**
The v0.8.0 framework is header-only by decision. Every `#include <quiver/tensor/tensor.h>` pulls in the full expression-template machinery: `Tensor<T>`, `BinaryOp<Op,L,R>` for every op, `UnaryOp`, `BroadcastView`, `Reduce<Axis,Op>`, plus all the SFINAE/concepts traits.

If *any* header in `include/quiver/` (currently SQLite, spdlog, sol2-clean) ever transitively includes `quiver/tensor/`, every `.cpp` in `src/` and `tests/` pays the template-instantiation cost. The CONCERNS.md document already notes that compile time is not actively budgeted. Eigen is the canonical example of this failure: copyprogramming.com cites that "Eigen's compilation slowness stems from extensive use of expression templates and inline functions, which force the compiler to instantiate massive amounts of template code in every translation unit", with reports of 75-80% of TU compile time spent on template analysis. [Eigen compile-time discussion](https://copyprogramming.com/howto/eigen3-take-a-long-time-to-compile-and-very-slow-when-debug)

**Why it happens:**
Templates are instantiated per TU; `inline` lets the linker dedupe but the compiler still processes the source every time. Each `BinaryOp<Plus, Tensor<double>, Tensor<double>>` is a distinct type, and every chained expression generates a tower (e.g. `BinaryOp<Plus, BinaryOp<Mul, Tensor, Tensor>, BinaryOp<Sub, Tensor, BinaryOp<Plus,...>>>`). Without containment, the cost spreads to TUs that do not even use tensors.

**How to avoid (Quiver-specific):**
1. **Strict include containment.** `quiver/tensor/*.h` is *never* included from `quiver/database.h`, `quiver/element.h`, `quiver/binary/*.h`, or any other public header. The bridge to `Database` (v0.9.0) lives behind a non-template C++ entry point, *or* in a separate header (`quiver/tensor_bridge.h`) the user opts into.
2. **Verify with a CI grep test.** Add a CTest that greps for `#include.*tensor/` in non-tensor public headers and fails if it finds anything.
3. **Time-budget the tests.** Add a CMake check that compiles `tests/test_tensor_*.cpp` separately and fails if any single file exceeds (suggested) 10s on the `release` configuration on the slowest CI runner. This catches accidental use of `Tensor<int>`, `Tensor<float>`, `Tensor<double>`, ... unbounded explosion in tests.
4. **Use `-ftime-trace` (Clang) and `/d2templateStats` (MSVC) at least once per phase** to confirm the framework is not the dominant compile cost. This is what Eigen's contributors do for regressions.
5. **Do not pre-instantiate everything in a `.cpp`.** That would defeat the header-only choice and re-impose linker work; only the bridge layer (v0.9.0) should pin types.

**Warning signs:**
- `quiver_tests.exe` build time grows faster than line count.
- A simple change in `quiver/tensor/expression.h` rebuilds 20+ TUs.
- MSVC PCH starts choking; Linux clang reports 100k+ template instantiations.

**Phase to address:** **Phase 1 (storage)** -- the include-containment rule must be set at the very first commit. Adding it later means undoing existing transitive includes. The CI grep test should land in Phase 1's CMake changes.

---

### Pitfall 5: ODR violations from non-`inline` operator overloads

**What goes wrong:**
A free-function `operator+` for tensors written in a header without `inline` (or without being a function template, or without being defined inside a class as a hidden friend) can be defined in multiple TUs, and the linker is allowed to reject the build (or worse: pick one definition silently and produce wrong results when the definitions disagree).

Concretely:
```cpp
// quiver/tensor/expression.h (NOT inline, NOT a template — bug)
quiver::tensor::Tensor<double> operator+(const Tensor<double>& a, const Tensor<double>& b);
```
This is an ODR-bomb: every TU that includes the header defines the symbol. With Quiver's MSVC + GCC + Clang matrix and `-fvisibility=hidden`, behaviour varies wildly between platforms. (Templates are exempt because `[basic.def.odr]` allows multiple definitions with identical token sequences; non-templates are not.)

**Why it happens:**
A developer writes a non-templated convenience overload (e.g. for `Tensor<double>` specifically, to make a simple test compile faster), forgetting `inline`. Templates and `constexpr` functions are implicitly inline; ordinary functions are not. The MS DLL boundary (`QUIVER_API`/visibility=hidden) hides the duplicate definition until link-time on a different platform.

**How to avoid:**
1. **Define operators as templates only**, e.g. `template <class L, class R> auto operator+(const L& l, const R& r) -> ...` constrained by a concept `IsTensorExpr`. Templates are implicitly inline.
2. **Or use the hidden-friend idiom**: define `operator+` *inside* the class body as `friend`. That makes it ADL-only, ODR-safe, AND solves Pitfall 6 in one move. [Modernes C++ on hidden friends](https://www.modernescpp.com/index.php/argument-dependent-lookup-and-hidden-friends/), [Just Software Solutions on hidden friends](https://www.justsoftwaresolutions.co.uk/cplusplus/hidden-friends.html).
3. **Never write a non-template, non-`inline` free operator overload in a header.** Add a clang-tidy check or a grep CI step.
4. **Cross-compiler CI catches this.** Quiver's existing matrix runs MSVC + GCC + Clang on Windows/Linux/macOS; an ODR violation will manifest as a duplicate-symbol link error on at least one of them. The unit tests do not need to be rich; the *link* of `quiver_tests.exe` is the check.

**Warning signs:**
- `multiple definition of operator+` from `ld.lld`.
- MSVC `LNK2005`.
- A single TU passes; a multi-TU test fails to link.

**Phase to address:** **Phase 2** (when operators are first defined). Pre-emptively decide: "all tensor operators are either templates or hidden friends." Document in `CONVENTIONS.md` next to the existing C++ patterns.

---

### Pitfall 6: ADL hijacking and `using namespace std` collisions

**What goes wrong:**
Tensor operators defined at namespace scope `quiver::tensor` are found via ADL when at least one operand is a `quiver::tensor::Tensor<...>`. But:

1. If `T == std::complex<double>`, then `std::` is also an associated namespace, and `std::operator+(complex, complex)` is in the candidate set. Overload resolution picks one based on conversion-rank rules. If the user wrote `Tensor<std::complex<double>> a, b; auto c = a + b;` and *Quiver's* `operator+` is templated on `IsTensorExpr` and `std::operator+` is templated on `complex<U>`, the resolution is decided by which is "more specialised" at the point of call. With `using namespace std;` on top, the result depends on header order.

2. Conversely, if a tensor element type's namespace defines its own `operator+` (e.g. user-defined arithmetic types), ADL finds *both* the user's and Quiver's, and overload resolution ambiguity errors on innocuous expressions.

**Why it happens:**
ADL is inherently transitive: any namespace associated with any operand contributes its `operator+` overloads. `std::complex`'s namespace is `std`, which contains `std::operator+`. C++ Core Guidelines warn that "Over-dependence on ADL can lead to semantic problems" ([Modernes C++ on ADL surprises](https://www.modernescpp.com/index.php/c-core-guidelines-argument-dependent-lookup-or-koenig-lookup/)).

**How to avoid:**
1. **Use the hidden-friend idiom** for `operator+`/`-`/`*`/`/` and the comparison operators on `Tensor` and `Expression`. Hidden friends are *only* found via ADL when at least one argument is the enclosing class type (or derived from it via CRTP). This eliminates the `using namespace std;` hijack: even if `std`'s overloads are visible via `using`, they cannot beat a hidden friend in overload ranking when the argument is `Tensor<...>`.
2. **Test the failure case explicitly.** Add `tests/test_tensor_adl.cpp` that does `using namespace std; Tensor<std::complex<double>> a, b; auto c = a + b;` and asserts it picks Quiver's overload (e.g. by checking the result's element type is computed by Quiver's operation, not `std::operator+(complex, complex)`).
3. **Constrain the templated overloads with concepts** (`requires IsTensorExpr<L> || IsTensorExpr<R>`) so the candidate set is empty when neither operand is a tensor; ranks correctly when one is.
4. Avoid putting the operators at root namespace (`quiver::operator+`); they belong at `quiver::tensor::operator+` or, better, hidden friends inside `quiver::tensor::Expression`.

**Warning signs:**
- Overload-ambiguity compiler errors that mention both Quiver's and `std::`'s overloads.
- A test with `Tensor<std::complex<...>>` fails an arithmetic assertion that the same test passes for `Tensor<double>`.
- Cppreference's [ADL page](https://en.cppreference.com/cpp/language/adl) is the spec.

**Phase to address:** **Phase 2** (operator definition). Same commit as Pitfall 5. The hidden-friend choice solves both.

---

### Pitfall 7: Template error messages that bury the actual bug

**What goes wrong:**
A user mistakenly writes `Tensor<double> + Tensor<int>`. Without explicit type-promotion machinery, the compiler walks the expression tree, fails to find a common type, and emits 600+ lines of nested template instantiations -- the canonical Eigen experience. The user has to scan for `static_assert` failures buried in the noise. If no `static_assert` exists, they get raw type-mismatch errors at the deepest internal node and can't tell what they did wrong.

**Why it happens:**
Default C++ template error reporting traces *every* substitution failure. With CRTP nesting `BinaryOp<Plus, BinaryOp<Mul, ...>, ...>`, the error chain is the depth of the expression times the width of the candidate set.

Eigen mitigates with EIGEN_STATIC_ASSERT macros that produce UPPERCASE_OBVIOUS messages users learn to grep for. ([Eigen StaticAssert source](https://eigen.tuxfamily.org/dox/StaticAssert_8h_source.html))

**How to avoid:**
1. **Use C++20 concepts**, not SFINAE. A `requires` clause names the failed predicate by name in the diagnostic. `requires IsTensorExpr<E>` reports "the constraint IsTensorExpr<...> was not satisfied" -- one line, named.
2. **Add early `static_assert` with named messages** at the public entry points:
   ```cpp
   static_assert(std::is_same_v<value_type_l, value_type_r>,
                 "QUIVER_TENSOR_REQUIRES_SAME_VALUE_TYPE: implicit type conversion is not "
                 "supported. Use .cast<T>() to make the conversion explicit.");
   ```
   The capitalised prefix is grep-friendly, mirroring Eigen's convention.
3. **Provide explicit `.cast<T>()`** so users have a documented escape (`a.cast<double>() + b`).
4. **Test the diagnostic** in a CTest that compiles a known-bad file and greps the compiler output for the static_assert message. This is unusual but extremely effective for catching regressions in error-message quality.
5. **Avoid implicit promotions** (Pitfall 8) -- they produce the worst messages because they delay the failure to the deepest internal type.

**Warning signs:**
- User reports "I got 600 lines of templates, I have no idea what's wrong."
- Stack-Overflow tag for your library has a recurring "what does this template error mean?" question.
- New contributors take >30 minutes to diagnose a typo in a tensor expression.

**Phase to address:** **Phase 5 (hardening)** for the polish (named static_asserts, error-message regression tests). But the *concept* declarations (`IsTensorExpr`, `IsScalar`, `IsBroadcastable`) belong in **Phase 1 (storage)** -- they are the basis of all later constraints. Late introduction means retrofitting concepts onto already-written templates.

---

### Pitfall 8: Implicit type conversion silently materialises with the wrong precision

**What goes wrong:**
```cpp
Tensor<int> i = float_expr;          // truncates each element silently
Tensor<double> d = int_tensor + 1.0; // promotion done per-element, not as planned
```
Without a documented promotion policy, two failure modes exist:
1. **Narrowing without diagnostic.** `Tensor<int> = expr_of_double` truncates each element.
2. **Mid-loop promotion.** If the framework does not pin the working type at `eval()` time but instead lets each leaf compute in its own type and promote at the binary-op node, the inner SIMD loop has scalar conversion ops in the hot path -- killing vectorisation (Pitfall 14).

NumPy is the obvious counterexample: it has a documented dtype-promotion algorithm; users still hit it, but at least it is specified.

**Why it happens:**
Lazy expressions defer the type decision. Without a policy, the framework either picks the first leaf's type, the destination's type, or the C++ usual-arithmetic-conversion result -- each can surprise the user.

**How to avoid:**
1. **Decide a promotion policy in Phase 2 and document it**. Recommended for v0.8.0: NumPy-like rules (`int + float -> float`, `int32 + int64 -> int64`, NaN-propagating for floats) with the working type fixed at the *outermost* op, not per-leaf.
2. **Explicit `.cast<T>()`** for cross-type ops; require it via `static_assert` (see Pitfall 7).
3. **Forbid narrowing assignment from an expression** with a different `value_type` than the destination's. `Tensor<int> = double_expr` should fail at compile time, not silently truncate. Eigen does this for matrices.
4. **Pin the inner-loop type** at `eval()` time so the SIMD loop runs on a single dtype.

**Warning signs:**
- A test with `Tensor<int>` produces unexpected zero-or-one results because float arithmetic was truncated.
- Performance is acceptable for `Tensor<double>` but 5x slower for `Tensor<int>` arithmetic mixed with float scalars.
- The user finds NumPy's promotion table and is surprised your library disagrees.

**Phase to address:** **Phase 2 (op materialization)** -- the moment `BinaryOp` decides its `value_type`. Late recovery touches every op.

---

## Broadcasting Pitfalls (Phase 3)

### Pitfall 9: Silent shape promotion (size-1 dim vs missing dim) -- the off-by-one trap

**What goes wrong:**
Broadcasting rules align dimensions from the trailing edge. The classic NumPy rule:
- A shape `(3,)` broadcast against `(2,3)` -> aligns trailing 3 with trailing 3. OK.
- A shape `(3,)` broadcast against `(3,2)` -> aligns trailing 3 with trailing 2. **Fails** -- but only if the framework actually *checks*. xtensor issue [#907](https://github.com/xtensor-stack/xtensor/issues/907) is precisely this: "Broadcast assign of higher dim container doesn't throw" -- the library accepted the operation and produced silent wrong output.

Common bugs in implementations:
- Aligning from the *leading* edge instead of the trailing edge (size-1 padding direction wrong).
- Treating size-1 as "broadcastable" but not size-0 (zero-sized dimension produces an empty result; some implementations divide by zero on stride calculation).
- Treating "missing dim" the same as "dim of size 1" -- they should be the same per NumPy, but some implementations distinguish (and then `(3,)` and `(1,3)` produce different results).
- An off-by-one in the alignment loop that drops the first or last dimension.

[NumPy broadcasting basics](https://numpy.org/doc/stable/user/basics.broadcasting.html), [Common NumPy broadcasting mistakes](https://academicnesthub.wordpress.com/2024/07/25/7-common-mistakes-to-avoid-when-using-numpy-broadcasting-rules/).

**How to avoid:**
1. **Implement the broadcast rule explicitly** in a function `broadcast_shape(shape_a, shape_b) -> std::optional<Shape>` that returns `nullopt` on incompatibility. Centralise; do not inline at every call site.
2. **Throw a typed error** following Quiver's three patterns: `"Cannot broadcast: shapes [3] and [3,2] do not align"`. (The error message is consumed unchanged through C API in v0.9.0+ -- start now with the right format.)
3. **Test matrix:** every combination of `(scalar, 1D, 2D, 3D)` x `(scalar, 1D, 2D, 3D)` x `(matching-trailing, mismatched, size-1-broadcast, size-0)`. ~50 tests. Cheap.
4. **Round-trip against NumPy** via a separate validation script that generates random shapes, runs both NumPy and Quiver, asserts identical results. Lock the agreement, treat NumPy as oracle.

**Warning signs:**
- A test passes for `(2,3)+(3,)` but fails for `(2,3)+(2,)` (i.e. the wrong rule was implemented).
- Off-by-one in result shape between Quiver and NumPy.
- A user reports "I expected a shape error and got a wrong-shaped result instead."

**Phase to address:** **Phase 3 (broadcasting)**. The shape arithmetic is the single most error-prone piece of the milestone.

---

### Pitfall 10: 0-D scalar broadcast is a special case implementations forget

**What goes wrong:**
`Tensor<double> a({3,3}); a + 1.0;` -- the scalar `1.0` should broadcast as a 0-D operand. If the framework treats a 0-D operand by entering the broadcast-shape function with `shape={}` or `shape={0}` or `shape={1}` inconsistently, the alignment logic mishandles it.

xtensor specifically had a bug here: "Fixed broadcasting of shape containing 0-sized dimensions" ([xtensor PR #1575](https://github.com/xtensor-stack/xtensor/pull/1575)).

**Why it happens:**
A 0-D tensor (`shape == {}`) and a 1-D tensor of size 1 (`shape == {1}`) and a *scalar* (raw `double`) are three different representations. Most code paths handle one or two but not the third.

**How to avoid:**
1. **Define an explicit `Scalar<T>` expression node** that wraps a raw `T` and presents itself with `rank == 0`. All scalar-tensor ops route through it.
2. **Test all three representations match**: `t + 1.0`, `t + Tensor<double>::scalar(1.0)`, `t + Tensor<double>{{1.0}}` should yield identical results.
3. **Test 0-sized dims as a separate axis of test matrix**: `Tensor<double>({0,3}) + Tensor<double>({3,3})` -- spec the behaviour (most likely "result is shape `(0,3)`, no work done"). Don't crash; don't silently materialise junk.

**Warning signs:**
- Compiler error "no matching overload for `t + 1.0`" surprises users.
- A test of `t + 1.0` works but `t + Tensor<double>::scalar(1.0)` differs.
- Division by zero in a stride calculation when one dim is 0.

**Phase to address:** **Phase 2 (scalar broadcast)** + **Phase 3 (general broadcasting)**. Both must agree on the 0-D representation.

---

## Reduction Pitfalls (Phase 4)

### Pitfall 11: Integer accumulator overflow and the `mean<int>` integer-division trap

**What goes wrong:**
`sum<int>` over a 10000-element `Tensor<int>` of values around 1e6 silently overflows `int32_t`. NumPy issue [#23075 nanmean overflow](https://github.com/numpy/numpy/issues/23075) and [issue #21506 division overflow](https://github.com/numpy/numpy/issues/21506) document the exact-same class of bug in NumPy's path.

`mean<int>` is even worse: a naive `sum / count` does *integer* division if both are `int`, returning a truncated result ("the average of [1,2,3,4] is 2"), which is almost never what the user wanted.

**How to avoid:**
1. **Default-promote the accumulator type** for reductions. `sum(Tensor<int8_t>)` accumulates in `int64_t`; `sum(Tensor<float>)` accumulates in `double`; etc. This is what NumPy does internally for many ufuncs and what pandas issue [#10155 Mean overflows for integer dtypes](https://github.com/pandas-dev/pandas/issues/10155) was filed against.
2. **For `mean`, return a floating type** even from integer input. `mean(Tensor<int>) -> double` (or `float` if the input is `float`). This is NumPy's choice. Make it explicit in the signature.
3. **Provide an opt-out** for users who want native dtype: `sum<KeepDType>(t)` returns the input dtype but is documented as "may overflow".
4. **Test with values near `INT_MAX`** -- explicit regression tests.

**Warning signs:**
- A `sum<int>` test of large values returns negative numbers (overflow wrap).
- `mean<int>([1,2,3,4])` returns `2`, not `2.5`.
- User-reported "mean is giving me wrong results" issues are almost always this.

**Phase to address:** **Phase 4 (reductions)**.

---

### Pitfall 12: Axis index validation, NaN propagation, and empty-tensor edge cases

**What goes wrong:**
Three sub-bugs commonly bundled together:

1. **Axis index errors:** negative axis (NumPy: `axis=-1` means last; many implementations don't support); axis >= rank (silently picks a dim that doesn't exist); axis on a 0-D tensor (must be a no-op or error -- not undefined).
2. **NaN propagation:** `mean([1.0, NaN, 3.0])` returns NaN by IEEE-754; some users want `nanmean` ignoring NaN. The library must not silently *change* the IEEE-754 default. Users who want `nanmean` should call it explicitly.
3. **Empty-tensor reductions:** `sum(Tensor<double>({0}))` -- by NumPy convention, sum of empty is the additive identity (`0`); max of empty is undefined and NumPy raises. Decide and document.

**How to avoid:**
1. **Validate axis in a single helper**: `normalise_axis(int axis, int rank) -> int`. Throw `"Cannot reduce: axis 5 out of range for rank-3 tensor"` (Quiver's "Cannot {operation}: {reason}" pattern). Test `axis = -1` (NumPy: last), `axis = -rank-1` (out of range), `axis >= rank` (out of range).
2. **NaN policy: IEEE-754 by default.** `mean` of a tensor containing NaN returns NaN. Document; provide `nansum`/`nanmean` later if needed (out of scope for v0.8.0 unless added explicitly).
3. **Empty-tensor policy:** match NumPy. `sum({}) = 0`, `mean({}) = NaN`, `max({})` throws (`"Cannot max: tensor is empty"`).
4. **Test all three sub-bugs explicitly** in one file: `test_tensor_reductions_edge_cases.cpp`.

**Warning signs:**
- `axis=-1` works in NumPy, doesn't compile / silently misbehaves in Quiver.
- A reduction with NaN returns 0 or some other "cleaned-up" value -- this is wrong; users will lose track of NaN-tainted data.
- `sum(empty_tensor)` segfaults instead of returning 0.

**Phase to address:** **Phase 4 (reductions)**.

---

## Integration Pitfalls (Quiver-specific)

### Pitfall 13: `Database::read_tensor`-style API designed before the lazy core stabilises (the bridge problem)

**What goes wrong:**
The PROJECT.md explicitly defers the `Element/Database bridge for tensors` to v0.9.0. But it is tempting to add a "small" hook in v0.8.0 -- e.g. a `Tensor<T>` constructor that accepts a `binary::BinaryFile&` -- to "validate the design end-to-end". Doing so couples the lazy framework to BinaryFile's hot-path concerns (the `unordered_map<string,int64_t>` dims param, the `validate_dimension_values` overhead per access). Worse, exposing a `Tensor<T>` returning function via the C API requires committing to a non-template wrapper signature now; v0.9.0 will then either (a) live with that signature forever, or (b) break it and invalidate every binding at the same time.

**Why it happens:**
The natural impulse is to dogfood the design against a real workload. With the binary subsystem already storing N-dim arrays, it is "obvious" to wire it through. The trap is that exposing the framework via a public function pins its entire compile-time and ABI surface before it is stable.

**How to avoid:**
1. **Hold the line.** v0.8.0 is C++-only, header-only, *no* public function returns or accepts a `Tensor<T>` from outside the tensor namespace.
2. **Keep validation inside `tests/test_tensor_*.cpp`.** If you want to test that a tensor can be populated from a `vector<double>` (which is what `BinaryFile::read` returns), the test does it locally, not via a public Database method.
3. **Mark the bridge in CONVENTIONS.md** as a deliberate non-goal for v0.8.0, citing the deferred-FFI rationale.
4. **Plan v0.9.0 bridge as immediate-mode**: the C API can never carry `BinaryOp<...>` across FFI; the bridge is "evaluate then export". Document this now so v0.8.0's API surface does not promise lazy-across-FFI.

**Warning signs:**
- A new public method on `Database` or `BinaryFile` mentions `Tensor`.
- A new symbol in `quiver/c/` references the tensor namespace.
- A binding (Julia/Dart/Python) needs to be regenerated to accommodate v0.8.0 -- it shouldn't.

**Phase to address:** **All phases.** This is a discipline check, not a single-phase concern. The success criterion is "no diff to `quiver/c/` or `bindings/` in v0.8.0."

---

### Pitfall 14: Naming collision with `Database` API in the same namespace tree

**What goes wrong:**
Quiver's existing `Database` has methods `describe()`, `read()` (via `read_scalar_*`, `read_vector_*`, etc.), and the binary subsystem has `BinaryFile::read`/`write`. A naive `Tensor::describe()` that prints shape and dtype is fine. A `Tensor::read()` (e.g. for "read from a stream") sitting next to `BinaryFile::read` in the same `quiver` parent namespace creates cognitive collision.

**How to avoid:**
1. **Place tensor in `quiver::tensor::`**, not `quiver::`. `quiver::tensor::Tensor`, `quiver::tensor::eval`, etc. This is what xtensor and Eigen do (`xt::`, `Eigen::`).
2. **Audit method names against the existing `Database` and `BinaryFile` APIs.** Conflicts are unlikely (different namespace, different concepts) but a 5-minute grep is cheap.
3. **Avoid the verb `read` on `Tensor`** as a member; prefer `at()`, `value()`, `operator()`, `eval()`. Reserve `read` for I/O.

**Warning signs:**
- A user asking "is `Tensor::read` the same thing as `BinaryFile::read`?" -- the answer should be obvious from the type alone.
- IDE auto-complete shows two `read` methods on different types in the same namespace.

**Phase to address:** **Phase 1 (storage)** -- this is naming hygiene at framework birth.

---

## Performance Traps (the whole point of expression templates)

### Pitfall 15: Heap allocation in `operator+`, `operator*`, etc.

**What goes wrong:**
A naive implementation has `operator+` returning `Tensor<T>` (eager, allocates) instead of a lightweight `BinaryOp` (lazy, no allocation). Or, the lazy `BinaryOp` accidentally materialises its operands by copy (Pitfall 1's storage policy).

**How to avoid:**
1. **`operator+` returns the smallest possible expression node** (`BinaryOp<Plus, Lhs, Rhs>` with reference-or-by-value storage per Pitfall 1).
2. **`BinaryOp` ctor is `noexcept` and trivial.**
3. **Benchmark allocation count** in a tight loop: build `a + b + c + d` 10000 times in a benchmark; assert allocation count is 0 (track with an allocator wrapper or `__attribute__((no_instrument_function))` on `new`/`delete`).
4. **Single materialisation at assignment.** `Tensor c = a + b + c + d;` allocates once for `c`, not three times for intermediates.

**Warning signs:**
- Profile shows `operator new` in arithmetic hot paths.
- Performance is similar to a hand-written loop only at small N; at large N it diverges.

**Phase to address:** **Phase 2 (op materialization)** + **Phase 5 (perf hardening)**.

---

### Pitfall 16: Materialising into a `Tensor` that wasn't pre-sized -> resize + copy

**What goes wrong:**
```cpp
Tensor<double> result;       // empty / unallocated
result = big_expression;     // resize + element-wise write (extra alloc + clear)
```
versus
```cpp
Tensor<double> result(shape);
result = big_expression;     // direct write into existing buffer
```
A naive `operator=` always resizes; it does not detect the already-correct case.

**How to avoid:**
1. **Resize-only-if-needed in `operator=`**: check shape, only resize on mismatch.
2. **Provide `result.evaluate_into(expr)`** as an explicit zero-resize path for tight loops (mirrors the recommendation in `.planning/codebase/CONCERNS.md` for the BinaryFile `read_into` improvement that has the same shape).
3. **Test both paths**: pre-sized and unsized destination, identical results, but allocation count differs.

**Warning signs:**
- Profile shows resize+memset on every assignment in a loop.
- `Tensor<double> r; for (...) r = expr_i;` is slower than expected.

**Phase to address:** **Phase 2** -- the assignment operator.

---

### Pitfall 17: SIMD vectorisation defeated by `if` branches on the index path

**What goes wrong:**
The framework's `operator()(i,j,...)` walks strides and decides "is this a broadcast dim, a stride dim, or a contiguous dim" via `if` branches *inside* the inner loop. The compiler cannot vectorise because the branch is data-dependent.

[CERN slides on vectorisation with expression templates](https://indico.cern.ch/event/450743/contributions/1116439/attachments/1224058/1790961/out-iCSC2016-L2.pdf), [arXiv on ET vectorisation](https://ar5iv.labs.arxiv.org/html/1109.1264).

**How to avoid:**
1. **Specialise on the contiguous case at compile time.** A `Tensor<T>` with no broadcast / no strided source has a `contiguous_iterator` that the inner loop uses; broadcast/strided sources use a slower iterator. The branch is in the *outer* code, not in the inner loop.
2. **Mark `operator[]` / `operator()` `noexcept` and `constexpr`** where possible -- limits the optimiser's pessimism.
3. **Check vectorisation with `-fopt-info-vec`** (GCC) / `-Rpass=loop-vectorize` (Clang) on the canonical `Tensor c = a + b;` case; assert the vectoriser kicks in.
4. **Write a benchmark** comparing `quiver::tensor` add to a hand-written `for` loop on `std::vector<double>`. Goal: within ~20% on Release. Anything worse means SIMD is not happening.

**Warning signs:**
- The benchmark for tensor add is 3-5x slower than the equivalent `std::vector<double>` loop.
- Disassembly shows scalar adds in the hot path instead of `addpd` / `vaddpd`.
- Performance differs heavily between MSVC and Clang -- usually a MSVC-vectorisation-failure story (per [Eigen MSVC inlining issues](https://copyprogramming.com/howto/eigen3-take-a-long-time-to-compile-and-very-slow-when-debug)).

**Phase to address:** **Phase 5 (hardening)** for the benchmark; **Phase 1 (storage)** for the `noexcept`/`constexpr` annotations on `operator[]`.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Reference-store all operands (no `ref_selector`) | Simpler `BinaryOp` template | Dangling refs once users use `auto` | **Never** -- bake the right policy in Phase 2. |
| Skip concepts; use SFINAE / no constraint | Faster to type | 600-line errors permanently | Acceptable in private helpers; **never** at the public-operator surface. |
| Implicit promotion (`Tensor<int> = double_expr` truncates silently) | "Just works" for common cases | Users hit data-loss bugs months later | **Never** -- emit `static_assert` or require `.cast<>()`. |
| Single-axis-only reductions ("axis=0 is enough for v0.8.0") | Half the code, half the tests | Phase 4 expansion adds N-axis later, design churn | Acceptable as MVP if **documented**; not acceptable to silently drop the `axis` param. |
| Skip CI grep test for include containment | Save 10 minutes in Phase 1 | Eventually `database.h` includes `tensor.h` indirectly, library compile time triples | **Never** -- the grep test is 5 lines of CMake. |
| `operator+` as non-`inline` non-template free function | Easier to debug | ODR violation breaks Linux build | **Never** -- always template or hidden friend. |
| Skip aliasing tests in Phase 2 ("element-wise can't alias") | Save 5 tests | Phase 3 broadcast lands an aliasing bug nobody catches | Acceptable only if Phase 3 *adds* the aliasing tests in the broadcast PR. |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Binary subsystem (`BinaryFile`) | Use `BinaryFile::read` to populate a `Tensor` via a hidden coupling | Keep separate; `BinaryFile::read` returns `vector<double>`; tensor is constructed from that buffer at the call site. **No new public coupling in v0.8.0.** |
| Public C++ headers (`include/quiver/`) | Add `#include <quiver/tensor/...>` to a non-tensor public header to "use a `Tensor<T>` for a single field" | Don't. Forward-declare; let users compose. The compile-time blast radius is too large. |
| C API (`include/quiver/c/`) | Add a `quiver_database_read_tensor` even just to "scaffold the bridge" | **No C API changes in v0.8.0.** The C API is the ABI boundary; pre-mature exposure pins the design. |
| Bindings (Julia/Dart/Python/JS/Lua) | Regenerate bindings "just in case the tensor namespace shows up" | Don't regenerate; v0.8.0 makes no header changes that matter to bindings. |
| spdlog (existing logger) | Add `spdlog::debug("tensor op")` calls inside the inner loop | Templates are header-only; logging from inside an inner loop pollutes every TU and crushes Release perf. **Log only at `eval()` boundaries, if at all.** |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Allocation in op overloads | `operator new` in profile | Lazy proxy returns; benchmark for 0 allocs in tight loops | At any N once the inner loop allocates per element |
| Resize on every assignment | Slow loop with re-targeted destination | Resize-only-if-needed in `operator=`; provide `evaluate_into()` | When users re-use a `result` buffer in a loop |
| Per-element type conversion | 5x slowdown on mixed-dtype expressions | Pin working type at `eval()` time; `static_assert` on dtype mismatch | When `Tensor<int>` and `Tensor<float>` are mixed |
| SIMD-defeating index branches | Scalar adds in disasm | Specialise contiguous path; `noexcept`/`constexpr` accessors | Always; observable from N >= 64 elements on AVX2 |
| `unordered_map<string, ...>` for dim names (BinaryFile lesson) | 40% time in hash | Don't repeat the BinaryFile mistake -- shape is `array<size_t,N>`, not a name->size map | Always; degenerates with rank |
| Compile-time blowup on chained ops | `quiver_tests.exe` build slows | CI compile-time budget per file (suggested: 10s Release on slowest runner) | Once expression chains exceed ~6 ops; gets worse with each phase |

---

## Security Mistakes

Tensor frameworks are mostly internal compute; the security surface is small. The relevant items:

| Mistake | Risk | Prevention |
|---------|------|------------|
| Index `operator()(i,j,...)` without bounds check in Debug | Out-of-bounds read; with attacker-controlled indices, disclosure | `assert` in Debug; `noexcept` in Release. (Eigen's pattern.) Or use `[]` for unchecked, `.at()` for checked, mirroring `std::vector`. |
| Integer overflow in shape/stride calculation | A multi-billion-element shape passed in causes signed overflow in `int64_t` stride math, producing a small wrap-around index that points into adjacent memory | Compute shape/stride in a saturating or checked manner; reject shapes with product > some platform max. |
| Assuming `T*` data buffer is non-null in `operator()` | Segfault on a moved-from `Tensor` | Define moved-from state as "empty" (size 0); document that operations on moved-from are UB, not "garbage data". |

---

## UX Pitfalls

For a C++ library, "UX" = developer experience.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| `auto x = a + b;` returns proxy, not `Tensor` | Surprising compile errors when `x` is used in non-template contexts | Document in bold. Provide `.eval()`. (Pitfall 2.) |
| Operator coverage is `+`, `-`, `*`, `/` but no `+=` | Users write `a = a + b;` instead of `a += b;` -- two reads of `a`, alias risk | Add compound-assign in the same Phase 2 commit as the binary ops. |
| Reduction returns shape including reduced dim of size 1 | Surprises users expecting `sum(rank-2 tensor, axis=0)` to return rank-1 | Implement both `keepdims=true` and `keepdims=false`; default to `false` (matches NumPy). |
| Error message points at internal type, not at user code | "I get an error in `BinaryOp<Mul, ...>`, line 42" -- meaningless | Concepts + named static_asserts + Quiver's three-pattern error format. (Pitfall 7.) |
| `Tensor` printing not implemented | Users `cout << t` and get an unhelpful overload | Provide a default `operator<<` that prints shape and a head-tail snippet of values. |
| Shape mismatch errors don't say *which* shapes | "broadcast failed" -- helpful as "compile error: 27" | Always include both shapes in the error: `"Cannot broadcast: shapes [3,4] and [3,5] do not align"`. |

---

## "Looks Done But Isn't" Checklist

These are the patterns that make a tensor framework pass its own tests but fail in real use. The roadmap should explicitly add tests for each.

- [ ] **Element-type coverage:** Tests use `Tensor<double>` everywhere; nobody tries `Tensor<int>`, `Tensor<float>`, `Tensor<int64_t>` -- verify every public op has a test for each of `{int32, int64, float, double}` at minimum.
- [ ] **Compound assignment:** `+`, `-`, `*`, `/` are tested; `+=`, `-=`, `*=`, `/=` are silently absent. Verify symmetric coverage.
- [ ] **Mean integer-division bug:** `mean<int>([1,2,3,4])` returns `2` instead of `2.5`. Verify `mean` returns floating-point even from integer input.
- [ ] **`auto` storage:** the test `Tensor<T> = a + b;` works; the test `auto e = a + b; ...; Tensor<T> = e;` is missing. Verify both paths.
- [ ] **Assignment vs construction:** `Tensor<T> a = expr;` works (copy-init); `Tensor<T> a; a = expr;` (default-construct + assign) has a separate code path that may resize differently. Verify both.
- [ ] **0-D scalar broadcast:** broadcasting tested for 2D-on-2D; 0-D scalar broadcast (`t + 1.0`) is a separate code path. Verify.
- [ ] **Negative axis:** reductions tested with `axis=0`, `axis=1`; not with `axis=-1`. Verify NumPy-compatible negative-axis semantics.
- [ ] **Empty-tensor reductions:** `sum(empty)`, `mean(empty)`, `max(empty)` -- specify and test.
- [ ] **NaN propagation:** reductions of tensors containing NaN return NaN, not "cleaned" values. Test.
- [ ] **Aliasing:** `a = a + a` works for element-wise; `a = a.broadcast(...)` and `a = a.transpose()` (Phase 6+) need explicit aliasing handling. Even for element-wise, add a regression test.
- [ ] **Chained-expression compile budget:** a single TU with `a + b * c - d / e + f * g - h` compiles in <X seconds on Release. Track regression.
- [ ] **Cross-compiler ODR:** `quiver_tests` links cleanly on MSVC + GCC + Clang. Multi-TU test that includes the tensor header from two TUs and uses operators in both.
- [ ] **`std::complex<T>` ADL test:** `using namespace std; Tensor<std::complex<double>> a, b; auto c = a + b;` resolves to Quiver's overload, not `std::operator+(complex, complex)` element-wise.
- [ ] **`Tensor<bool>` arithmetic:** Eigen had inconsistent results for boolean matrices ("matrix multiplication with boolean coefficients produces inconsistent, unpredictable results"). Either support or `static_assert`-forbid; do not silently do something nonsensical.
- [ ] **Move-from-temporary in expression chain:** `auto e = make_tensor() + b; Tensor c = e;` -- the leaf `make_tensor()` is a temporary destroyed at the end of the constructing statement. Verify the storage policy handles this (it should, via `nested_eval`/by-value storage of materialised leaves).
- [ ] **`Tensor<T>` const-correctness:** `const Tensor<T> a; auto e = a + a;` -- the `const` should propagate; `e` should be assignable to a non-const `Tensor<T>` but should not allow `a` to be modified.
- [ ] **CMake install:** `install(DIRECTORY include/quiver/tensor ...)` is added to CMakeLists -- forgetting this is the #1 install-time bug.
- [ ] **`-Werror` clean** on all platforms (the codebase currently lacks `-Werror`; tensor's heavy template path is the most likely place to *introduce* warnings on a future MSVC update).
- [ ] **DLL / hidden-visibility:** the templated tensor types are header-only and need no `QUIVER_API` annotation. Verify nothing in the framework uses `QUIVER_API` accidentally.

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Dangling references (Pitfall 1) | **HIGH** -- redesign of storage policy touches every BinaryOp/UnaryOp | Add `ref_selector`/`nested_eval` traits; change `BinaryOp` storage to use them; bulk-rebuild all tests; document the new policy. Plan a half-day. |
| `auto` returns proxy (Pitfall 2) | **LOW** -- documentation + `.eval()` method | Add `.eval()`; bold note in header; one paragraph in CONVENTIONS.md. |
| Aliasing wrong (Pitfall 3) | **MEDIUM** -- add `.eval()` recommendation per case + maybe `noalias()` opt-in | Document the failing patterns; require `.eval()` from users at the call site for those cases; add per-case regression tests. |
| Compile-time blowup (Pitfall 4) | **MEDIUM** -- audit transitive includes; add the CI grep | Find every public header that pulled in tensor; replace with forward decl. Add CMake CI step. |
| ODR violation (Pitfall 5) | **LOW** -- mark functions `inline`/template/hidden friend | Mechanical fix once identified; CI catches it on next push. |
| ADL hijacking (Pitfall 6) | **MEDIUM** -- convert to hidden friends | The hidden-friend rewrite is a per-class change; ~1 day for the framework. |
| 600-line errors (Pitfall 7) | **MEDIUM** -- retrofit concepts onto existing templates | Concepts decay back into SFINAE if poorly applied; doable but tedious. ~1 day. |
| Implicit type narrowing (Pitfall 8) | **LOW** -- add static_assert | Mechanical; insert the static_assert; users get explicit `.cast<>()` errors. |
| Broadcast off-by-one (Pitfall 9) | **MEDIUM** -- the broadcast helper is reused in many places | Centralise `broadcast_shape`; replace ad-hoc alignment. ~half-day. |
| 0-D scalar handling (Pitfall 10) | **LOW** -- introduce `Scalar<T>` proxy; route ops through it | Mechanical. |
| `mean<int>` bug (Pitfall 11) | **LOW** -- change `mean` return type to floating-point; deprecate any callers expecting integer | Trivial code change; subtle migration if users existed (they don't yet -- v0.8.0 is the first release). |
| Axis / NaN / empty (Pitfall 12) | **LOW** -- add `normalise_axis`, document NaN policy, add empty-tensor branch | Mechanical; ~half-day. |
| Bridge designed too early (Pitfall 13) | **HIGH** -- if the C API is committed to a tensor signature, every binding regenerates and the v0.9.0 design is constrained | **Prevention is the only cure.** No public bridge methods in v0.8.0. |
| Naming collision (Pitfall 14) | **LOW** -- rename in `quiver::tensor::` namespace | Mechanical; do it before users exist. |
| Allocation in ops (Pitfall 15) | **LOW** | Make returns lightweight; benchmark catches. |
| Resize on assign (Pitfall 16) | **LOW** | Add the size-check branch in `operator=`. |
| SIMD defeated (Pitfall 17) | **MEDIUM** -- specialise the contiguous path | The contiguous-iterator specialisation may require an `if constexpr` switch high up in the iteration loop. ~1 day. |

---

## Pitfall-to-Phase Mapping

The recommended phase ordering for v0.8.0 is **Storage -> Ops/Materialisation -> Broadcasting -> Reductions -> Hardening**. Each pitfall maps to the *earliest* phase that should prevent it; some need verification at later phases too.

| # | Pitfall | Prevention Phase | Verification |
|---|---------|------------------|--------------|
| 1 | Dangling references in expressions | Phase 2 | ASan-clean test of `auto e = a+b; { ... }; result = e;` |
| 2 | `auto` returns proxy | Phase 2 | Doc test; `.eval()` provided; warning compile-test |
| 3 | Aliasing on `a = a + a` | Phase 2 | Aliasing regression tests added in Phases 2, 3, 4 |
| 4 | Compile-time blowup | Phase 1 | CI grep test for include containment; per-file compile budget |
| 5 | ODR violations | Phase 2 | Cross-compiler link of multi-TU test |
| 6 | ADL hijacking | Phase 2 | Test with `Tensor<std::complex<...>>` + `using namespace std` |
| 7 | 600-line error messages | Phase 1 (concepts) + Phase 5 (polish) | Compile-error regression test grepping for named static_assert |
| 8 | Implicit type conversion | Phase 2 | `Tensor<int> = double_expr` is a *compile error*, not a silent truncation |
| 9 | Broadcast off-by-one | Phase 3 | Round-trip vs NumPy on ~50 shape combinations |
| 10 | 0-D scalar broadcast | Phase 2 (scalar) + Phase 3 (general) | Test all three representations match |
| 11 | `mean<int>` integer division / `sum<int>` overflow | Phase 4 | Tests with `int32` near `INT_MAX`; `mean` returns `double` |
| 12 | Axis / NaN / empty in reductions | Phase 4 | Edge-case test file |
| 13 | Bridge designed too early | All phases (discipline) | "No diff to `quiver/c/` or `bindings/` in v0.8.0" success criterion |
| 14 | Namespace naming collision | Phase 1 | Code review; namespace under `quiver::tensor::` |
| 15 | Heap allocation in ops | Phase 2 + Phase 5 | Benchmark with allocation counter; assert 0 allocs in `a+b+c+d` chain |
| 16 | Resize on every assignment | Phase 2 | Test with pre-sized destination + allocation counter |
| 17 | SIMD vectorisation defeated | Phase 5 | Benchmark vs hand-written loop on `std::vector<double>`; disasm spot-check |

**Phase-summary success criteria** (for ROADMAP.md to lift verbatim):
- **Phase 1 (Storage):** include containment grep test; per-file compile budget set; `quiver::tensor::` namespace; concepts (`IsTensorExpr` etc.) defined; `noexcept`/`constexpr` accessors.
- **Phase 2 (Ops & materialization):** `ref_selector`-style storage policy; `.eval()` method; aliasing policy documented; promotion-policy `static_assert`s; hidden-friend operators; allocation-count regression test; pre-sized vs unsized destination tests; ADL test with `std::complex`.
- **Phase 3 (Broadcasting):** `broadcast_shape` helper; ~50-test NumPy round-trip; 0-D scalar handling unified; broadcast-aliasing regression test.
- **Phase 4 (Reductions):** `normalise_axis`; promoted accumulator types; `mean` returns floating-point; NaN/empty edge cases; reduce-into-self alias test.
- **Phase 5 (Hardening):** compile-time budget; benchmark vs hand loop; named-static_assert error-quality regression test; cross-compiler CI link of multi-TU tests; `-Werror` in tensor TUs only.

---

## Sources

### Authoritative (HIGH confidence)
- [Eigen: Common Pitfalls](https://libeigen.gitlab.io/eigen/docs-nightly/TopicPitfalls.html) -- `auto` keyword warning, expression-template dangling-reference example, ternary type-inference trap, alignment, boolean-matrix inconsistency
- [Eigen: Aliasing](https://libeigen.gitlab.io/eigen/docs-nightly/group__TopicAliasing.html) -- "matrix multiplication is the only operation in Eigen that assumes aliasing by default"; `eval()` and `noalias()` rules
- [Eigen: Lazy Evaluation and Aliasing](https://libeigen.gitlab.io/eigen/docs-nightly/TopicLazyEvaluation.html) -- when Eigen inserts temporaries; `evaluate-before-nesting` and `evaluate-before-assigning` flags
- [Eigen: StaticAssert source](https://eigen.tuxfamily.org/dox/StaticAssert_8h_source.html) -- UPPERCASE convention for grep-able messages
- [NumPy Broadcasting basics](https://numpy.org/doc/stable/user/basics.broadcasting.html) -- canonical broadcasting rules; trailing-edge alignment
- [cppreference: Argument-dependent lookup](https://en.cppreference.com/cpp/language/adl) -- ADL rules

### Verified (MEDIUM confidence)
- [Modernes C++: ADL and Hidden Friends](https://www.modernescpp.com/index.php/argument-dependent-lookup-and-hidden-friends/) -- hidden-friend idiom for ODR-safe operators
- [Just Software Solutions: The Power of Hidden Friends in C++](https://www.justsoftwaresolutions.co.uk/cplusplus/hidden-friends.html) -- benefits of hidden friends including ADL containment
- [Modernes C++: ADL surprises (C++ Core Guidelines)](https://www.modernescpp.com/index.php/c-core-guidelines-argument-dependent-lookup-or-koenig-lookup/) -- "over-dependence on ADL can lead to semantic problems"
- [xtensor PR #1575: Fixed broadcasting of shape containing 0-sized dimensions](https://github.com/xtensor-stack/xtensor/pull/1575) -- evidence of 0-sized broadcast bugs in production tensor libraries
- [xtensor issue #907: Broadcast assign of higher dim container doesn't throw](https://github.com/xtensor-stack/xtensor/issues/907) -- silent acceptance of incompatible broadcasts
- [NumPy issue #23075: Unexpected behaviour with `nanmean` compared to `mean`, overflow encountered in reduce](https://github.com/numpy/numpy/issues/23075) -- mean reduction overflow
- [NumPy issue #21506: Division overflow tracking issue](https://github.com/numpy/numpy/issues/21506) -- integer division overflow class
- [pandas issue #10155: Mean overflows for integer dtypes](https://github.com/pandas-dev/pandas/issues/10155) -- the mean-overflow class
- [Wuwei Lin: Lazy Evaluation with Expression Templates (1)](https://wuwei.io/post/2018/06/lazy-evaluation-with-expression-templates-1/) -- canonical CRTP skeleton with reference-storage operands
- [Eigen3 compile-time discussion](https://copyprogramming.com/howto/eigen3-take-a-long-time-to-compile-and-very-slow-when-debug) -- 75-80% of TU time spent in template instantiation; MSVC inlining issues
- [Common NumPy broadcasting mistakes](https://academicnesthub.wordpress.com/2024/07/25/7-common-mistakes-to-avoid-when-using-numpy-broadcasting-rules/) -- enumerated common bugs
- [Hacking C++: ADL](https://hackingcpp.com/cpp/lang/adl) -- ADL mechanics

### Reference (lower priority but useful)
- [Wikipedia: Expression templates](https://en.wikipedia.org/wiki/Expression_templates) -- background; deferred-evaluation rationale
- [Wikipedia: Curiously recurring template pattern](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) -- CRTP basics
- [Modernes C++: Avoiding Temporaries with Expression Templates](https://www.modernescpp.com/index.php/avoiding-temporaries-with-expression-templates/) -- the temporary-elimination idiom
- [Vectorization with Expression Templates (CERN slides)](https://indico.cern.ch/event/450743/contributions/1116439/attachments/1224058/1790961/out-iCSC2016-L2.pdf) -- SIMD considerations
- [arXiv: A New Vectorization Technique for Expression Templates in C++](https://ar5iv.labs.arxiv.org/html/1109.1264) -- formal SIMD-ET treatment
- [GitHub issue: Vector expression templates bug — behaves like reference to expired temporary (libsimdpp #127)](https://github.com/p12tic/libsimdpp/issues/127) -- a real-world dangling-temporary bug in another tensor lib

### Quiver-internal (Quiver-specific pitfalls)
- `.planning/PROJECT.md` -- v0.8.0 scope, header-only constraint, deferred-FFI rationale
- `.planning/codebase/CONCERNS.md` -- existing pain points (per-row prepares, `unordered_map<string,...>` hot-path, no `-Werror`, no compile-time budget) that the tensor framework MUST NOT replicate
- `.planning/codebase/CONVENTIONS.md` -- error-message patterns, naming rules, Pimpl-or-value-type discipline
- `CLAUDE.md` -- canonical error patterns, namespace conventions, Binary subsystem hot-path lessons

---

*Pitfalls research for: lazy CRTP Expression Template tensor framework integrated into the Quiver C++ library, scoped to v0.8.0 (C++-only), with explicit awareness of the deferred v0.9.0 FFI bridge.*
*Researched: 2026-05-05*
