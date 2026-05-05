# Phase 1: Storage and CRTP Scaffold - Context

**Gathered:** 2026-05-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Build the foundational `Tensor<T>` storage value type, the `Expression<Derived>` CRTP base, and the header/test/CMake wiring under `include/quiver/tensor/` that anchors all four phases of milestone v0.8.0.

**In scope:**
- `Tensor<T>` for `T ∈ {float, double, int32_t, int64_t}` — construction, shape/size/rank, multi-dim indexing, raw data access, iterators, `.item()` (REQ-STG-01..05)
- `Expression<Derived>` CRTP base with `self()`, `shape()`, `size()`, `eval(linear_idx)` (REQ-EXP-01)
- `IsTensorExpr` concept enforcing CRTP inheritance (REQ-EXP-02)
- Header layout under `include/quiver/tensor/` mirroring `include/quiver/binary/` (REQ-BLD-01)
- Per-concern test files under `tests/test_tensor_*.cpp`, starting with `test_tensor_storage.cpp` (REQ-BLD-02)
- `tests/CMakeLists.txt` integration; **no edits to `src/CMakeLists.txt`** (REQ-BLD-03)
- CI test enforcing include containment — no public Quiver header outside `include/quiver/tensor/` transitively pulls in tensor headers (REQ-BLD-04)

**Out of scope (deferred to Phase 2+):**
- Operator overloads (`+`, `-`, `*`, `/`, unary `-`, compound assignment) — Phase 2
- `BinaryOp`, `UnaryOp`, `ref_selector` storage policy — Phase 2
- `Tensor<T>::operator=(const Expression<E>&)` materialization — Phase 2
- Math functions (`abs`, `exp`, `log`, `sqrt`) — Phase 2
- Three-pattern error messages on shape/type mismatches in operators — Phase 2
- `BroadcastView<E>` and NumPy broadcasting — Phase 3
- Reductions (`sum`, `mean`, `max`) — Phase 4

**Cross-phase discipline (every phase, including this one):**
- Zero diff to `include/quiver/c/` and `bindings/` — verified via `git diff --stat` (PITFALLS Pitfall #13)
- All thrown errors follow Quiver's three-pattern format (`Cannot {op}: {reason}` / `{Entity} not found: {id}` / `Failed to {op}: {reason}`)

</domain>

<decisions>
## Implementation Decisions

### Tensor<T> Constructor Surface

- **D-01:** Ship three constructors in Phase 1: `Tensor(shape_t)`, `Tensor(shape_t, T fill)`, `Tensor(shape_t, std::initializer_list<T>)`. Iterator-range and `std::vector<T>&&` take-ownership constructors are deferred to v0.9.0 — they are non-breaking additions and not justified by Phase 2-4 test needs.
- **D-02:** `Tensor(shape_t)` (no fill) zero-initializes storage via `data_.assign(total_size(shape), T{})`. Predictable; matches NumPy `np.zeros` default expectation. Slight perf cost vs default-init is negligible compared to subsequent compute.
- **D-03:** No factory functions (`zeros`, `ones`, `full`, `arange`, `zeros_like`) in Phase 1 — defer to v0.9.0. Users call `Tensor<double>(shape, 0.0)` directly. Matches v0.8.0 minimal-scope bias from PROJECT.md.
- **D-04:** Constructors accept 0-sized dimensions (`Tensor<double>({2, 0, 3})` is valid; `total_size = 0`, `data_` empty). NumPy-compatible. Pre-empts Phase 3 broadcasting Pitfall #9 (0-sized dim handling) and Phase 4 empty-tensor reduction policy.
- **D-05:** Initializer-list constructor validates that the list size equals `total_size(shape)`. Mismatch throws `Cannot construct tensor: initializer_list size {got} does not match shape size {expected}` (Pattern 1).

### Indexing & Element Access

- **D-06:** `operator()(Idx... idxs)` is unchecked (no rank, type, or bounds validation in the operator). Caller is contractually responsible for passing the right number of `std::size_t`-convertible indices in the valid range. Hot path stays clean for tests in Phase 2-4 that use `op()` to populate tensor values.
- **D-07:** `at(Idx... idxs)` is the bounds-and-rank-checked variant. Throws `Cannot access tensor: rank mismatch (expected {rank} indices, got {n})` on rank mismatch and `Cannot access tensor: index {i,j,k} out of bounds for shape {a,b,c}` on bounds violation (Pattern 1, three-pattern format).
- **D-08:** Indices are `std::size_t` — no negative indexing in Phase 1. Matches `shape_t` element type. NumPy-style negative indexing (both element and axis) is uniformly deferred to v0.9.0 alongside `RED-04` negative-axis support.
- **D-09:** `begin()` and `end()` return `std::vector<T>::iterator` / `const_iterator` by forwarding to `data_.begin()` / `data_.end()`. Range-for and `std::ranges` compatible. Future strided views get their own iterator types; no need to abstract Tensor's iterator now.
- **D-10:** Mutable `operator()` returns `T&`; const `operator()` returns `const T&` (or `T` by value — implementer's choice, no observable difference for the supported `T`s). Required so test fixtures can write `t(0, 0, 0) = 42.0`. Note: REQ-POL-02 defers mutable `data() -> T*` to v0.9.0; `data()` remains `const T*` only.

### 0-D Tensor Representation

- **D-11:** 0-D tensors use empty shape `shape_t{}` with `total_size = 1`. NumPy/xtensor convention. `rank() == 0`, `size() == 1`, `data_` has exactly one element. No special-case branching in the assignment loop or the upcoming Phase 3 broadcasting algorithm (right-align rule pads with 1s to reach the broadcast target shape — empty shape pads everywhere, which is exactly the desired behavior).
- **D-12:** `.item()` ships in Phase 1 as part of Tensor<T>'s core interface. Returns `T` (the single element). Throws `Cannot item: tensor has {N} elements (expected 1)` if `size() != 1` (Pattern 1). Phase 4 reductions reuse this method on their 0-D output.
- **D-13:** No named factory for 0-D construction (`Tensor<T>::scalar(value)` rejected). Users construct 0-D tensors via the existing constructors: `Tensor<double>({}, 42.0)` (shape + fill) or `Tensor<double>({}, {42.0})` (shape + init-list). Asymmetric naming with deferred `zeros`/`ones`/`full` would be confusing.
- **D-14:** `IsTensorExpr<T>` is the strict CRTP-derivation concept: `T` satisfies `IsTensorExpr` iff `T` derives from `Expression<U>` for some `U`. Implemented via a template detection idiom (`is_base_of_template_v` or equivalent). Duck-typed alternatives rejected — they invite ADL collisions with `std::complex` (Anti-Pattern #3) and lose the discipline of "only Expression-derived types are tensor expressions".

### BLD-04 Include Containment Enforcement

- **D-15:** Single mechanism: a CTest target (`tensor_no_leakage_test` or similar) that runs a CMake script (`cmake/CheckTensorIncludeContainment.cmake`). No pre-commit hook duplicate, no compile-time test file. Simplicity over belt-and-suspenders for v0.8.0.
- **D-16:** The check greps for `#include` lines matching the regex `#\s*include\s*[<"]quiver/tensor/`. Detects both angle-bracket and quoted forms. Symbol-reference grepping rejected (false-positive risk on doc comments mentioning tensor); AST-based clang tooling rejected (heavyweight dependency for a textual check).
- **D-17:** Scope: `include/quiver/` recursive, **excluding** `include/quiver/tensor/`. Covers public C++ headers (`include/quiver/*.h`, `include/quiver/binary/*.h`) and public C API headers (`include/quiver/c/*.h`, `include/quiver/c/binary/*.h`). Matches REQ-BLD-04 wording precisely. New public headers automatically covered — no test maintenance.
- **D-18:** Wiring: `tests/CMakeLists.txt` adds `add_test(NAME tensor_no_leakage COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CheckTensorIncludeContainment.cmake)`. Runs as `ctest -R no_leakage`. No compiled binary; cross-platform via CMake script (no shell-script Windows/POSIX fork). On match, the script prints offending file:line pairs and `message(FATAL_ERROR ...)`s with a diagnostic that names the BLD-04 requirement.

### Claude's Discretion

The following implementation details are unconstrained by user input — Claude (planner/executor) selects the best mechanical approach within the locked decisions above:

- The exact template-detection idiom for `IsTensorExpr` (e.g., `std::is_base_of_v<Expression<U>, T>` with a helper, vs. `std::derived_from<T, Expression<U>>` via a probe). Both are equivalent for users.
- Header-file split granularity inside `include/quiver/tensor/`. Research recommends `expression.h`, `shape.h`, `tensor.h` for Phase 1 — but the planner can split or merge if friction emerges.
- Whether `compute_strides()`, `total_size()`, and `ravel_index()` live in `shape.h` (recommended) vs an internal `detail::` namespace.
- Whether `eval(linear_idx)` on `Tensor<T>` is `T` by value or `const T&` — both are correct given `T ∈ {float, double, int32_t, int64_t}`.
- Test file naming within `tests/test_tensor_*.cpp` for Phase 1 — recommend `test_tensor_storage.cpp` initially, with sub-splits if it grows beyond ~500 lines.
- Concrete error message wording within the three-pattern format (the patterns and key phrases are locked; specific punctuation/word-choice is unconstrained).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project & Milestone Scope
- `.planning/PROJECT.md` — v0.8.0 scope, constraints, deferred work, Key Decisions table (CRTP/Expression Templates locked)
- `.planning/REQUIREMENTS.md` — v0.8.0 requirements (Phase 1 owns STG-01..05, EXP-01, EXP-02, BLD-01..04)
- `.planning/ROADMAP.md` §"Phase 1: Storage and CRTP Scaffold" — phase goal, success criteria, cross-phase discipline rule
- `.planning/STATE.md` — current milestone position

### Research (HIGH confidence; consume verbatim)
- `.planning/research/SUMMARY.md` — executive summary, recommended phase structure, gaps to address
- `.planning/research/ARCHITECTURE.md` — full pattern catalog: Pattern 1 (CRTP base), Pattern 5 (shape model), Anti-Patterns #3 (ADL hijacking), #5 (pass-by-value slicing), #6 (noexcept on move)
- `.planning/research/PITFALLS.md` — all 17 pitfalls + 18-item "looks done but isn't" checklist; **Phase 1 specifically addresses #4 (header-only template heaviness), #7 (template error messages), #13 (bridge designed too early), #14 (namespace hygiene)**
- `.planning/research/STACK.md` — full per-library verdicts; "use NONE" for tensor (C++20 stdlib + existing GoogleTest only)
- `.planning/research/FEATURES.md` — feature landscape, MVP boundaries, P1/P2/Defer prioritization

### Codebase Maps (HIGH confidence; consume verbatim)
- `.planning/codebase/STRUCTURE.md` — directory layout, naming conventions, "Where to add new code" section
- `.planning/codebase/ARCHITECTURE.md` — three-layer pattern (C++ core → C API → bindings), tensor placement
- `.planning/codebase/CONVENTIONS.md` — Pimpl vs. Rule of Zero rules, error patterns, move semantics
- `.planning/codebase/STACK.md` — current dependencies (no new ones for v0.8.0 core)
- `.planning/codebase/TESTING.md` — GoogleTest harness conventions
- `.planning/codebase/CONCERNS.md` — known compile-time hot spots (sol2 dominates currently)

### Project Conventions (consume in full)
- `CLAUDE.md` — top-level principles, the three error-message patterns, Pimpl-vs-value-type rule, naming conventions, ABI/move-semantics rules

### Existing Code Anchors (read for precedent, do not modify)
- `include/quiver/binary/dimension.h` — header-mostly value type precedent
- `include/quiver/binary/time_properties.h` — same precedent
- `include/quiver/binary/binary_metadata.h` — value type with TOML serialization (similar layout depth)
- `include/quiver/database.h` — `noexcept` move semantics pattern (lines 24-29) — Tensor<T> must mirror
- `include/quiver/export.h` — `QUIVER_API` macro (note: header-only templates do not need export annotation)
- `cmake/CompilerOptions.cmake` — `quiver_compiler_options` INTERFACE target; reused as-is for tensor headers (no new flags)
- `cmake/Dependencies.cmake` — FetchContent declarations; **no additions for v0.8.0 core**
- `src/CMakeLists.txt` — **DO NOT EDIT**; header-only tensor headers ship via existing `install(DIRECTORY include/quiver ...)` rule
- `tests/CMakeLists.txt` — append new `test_tensor_*.cpp` entries to existing `quiver_tests` executable; add `add_test(...)` for the BLD-04 leakage test

### External Authoritative References (cite during planning; review on edge cases)
- [Wikipedia — Expression templates](https://en.wikipedia.org/wiki/Expression_templates) — canonical pattern, leaf-vs-intermediate storage rule
- [cppreference — CRTP](https://en.cppreference.com/cpp/language/crtp) — language-level reference
- [Eigen — Class hierarchy](https://libeigen.gitlab.io/eigen/docs-nightly/TopicClassHierarchy.html) — `MatrixBase<Derived>` precedent
- [xtensor — Concepts](https://github.com/xtensor-stack/xtensor/blob/master/docs/source/developer/concepts.rst) — `xexpression` CRTP base, value semantics

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- **Header-mostly value-type precedent (`include/quiver/binary/dimension.h`, `time_properties.h`)**: Mirror the layout depth, namespace nesting style (`quiver::binary::*` → `quiver::tensor::*`), and Rule-of-Zero discipline. No `.cpp` companions needed for value types unless non-template helpers accumulate.
- **Per-concern source-file split (`src/database_*.cpp`, `src/binary/*.cpp`)**: Establishes "one feature per file" precedent. Even though Phase 1 is header-only, the same per-concern split applies to headers under `include/quiver/tensor/` (e.g., `expression.h`, `shape.h`, `tensor.h` separate, not one mega-header).
- **Per-concern test files (`tests/test_database_*.cpp`, `tests/test_binary_*.cpp`)**: Start `tests/test_tensor_storage.cpp` for Phase 1; later phases add `test_tensor_arithmetic.cpp`, `test_tensor_aliasing.cpp`, `test_tensor_broadcast.cpp`, `test_tensor_reduce.cpp`. Names mirror the source/feature, not the header file.
- **Three-pattern error formatter** (`CLAUDE.md` "Error Handling" section): Reuse exactly. Tensor errors must thread through Pattern 1 (`Cannot {op}: {reason}`), Pattern 2 (`{Entity} not found: ...` — unlikely in Phase 1), Pattern 3 (`Failed to {op}: ...` — unlikely in Phase 1).
- **CMake INTERFACE target `quiver_compiler_options`** (`cmake/CompilerOptions.cmake`): Provides `/W4 /permissive-` on MSVC and `-Wall -Wextra -Wpedantic` on GCC/Clang. Tensor headers inherit it transitively via the `quiver` target — no new compiler-flag entries needed.
- **CTest add_test() with CMake script pattern**: Quiver already drives all tests through CTest. The BLD-04 leakage check uses `add_test(NAME ... COMMAND ${CMAKE_COMMAND} -P ...)` — no new infrastructure required.

### Established Patterns

- **Plain value type with Rule of Zero** for non-Pimpl classes (`Element`, `Row`, `Migration`, `Dimension`, `TimeProperties`, `BinaryMetadata`). Tensor<T> matches: no Pimpl, default copy/move, `noexcept` on move.
- **Move noexcept defaults**: `Database`, `BinaryFile`, etc., explicitly `= default` move ctor/assign with `noexcept`. Tensor<T> must mirror to avoid silent copy-on-grow when stored in `std::vector<Tensor<T>>` (Anti-Pattern #6).
- **Namespace nesting**: top-level `quiver::`, subsystem `quiver::binary::*` (existing), `quiver::tensor::*` (new). Detail/internal helpers under `quiver::tensor::detail::`. Functors specifically under `quiver::tensor::ops::` (Phase 2 — not Phase 1, but the namespace is reserved now).
- **Templates over runtime polymorphism** for performance-sensitive paths (no virtuals; CRTP enforces this). Matches Quiver's Pimpl/value-type discipline: hide nothing behind virtual dispatch when static dispatch suffices.
- **Header-only public surface for value types** matches the existing pattern in `include/quiver/binary/dimension.h` (value type with no `.cpp`).

### Integration Points

- **`tests/CMakeLists.txt`**: Append `test_tensor_storage.cpp` to existing `quiver_tests` add_executable list. Add `add_test(NAME tensor_no_leakage ...)` for BLD-04 enforcement.
- **`cmake/CheckTensorIncludeContainment.cmake`** (NEW): Self-contained CMake script invoked by CTest. Uses `file(GLOB_RECURSE)` to enumerate `include/quiver/*.h`, `file(STRINGS ... REGEX ...)` to grep for tensor includes, `message(FATAL_ERROR ...)` on match.
- **No edits to `src/CMakeLists.txt`** (REQ-BLD-03 hard rule). The existing `install(DIRECTORY include/quiver ...)` rule and `target_include_directories(quiver PUBLIC ...)` already package new tensor headers.
- **No edits to `include/quiver/quiver.h`** (umbrella header). Tensor must be opt-in via `<quiver/tensor/...>` includes only; pulling it into the umbrella defeats compile-time containment for translation units that don't use tensors.
- **No edits to `include/quiver/c/`, `include/quiver/c/binary/`, `bindings/`** (Pitfall #13 cross-phase discipline; verify with `git diff --stat`).

</code_context>

<specifics>
## Specific Ideas

The following preferences are derived from upstream artifacts (PROJECT.md, ROADMAP.md, REQUIREMENTS.md, research/) and are MANDATORY:

- Mirror `include/quiver/binary/` directory structure for `include/quiver/tensor/` — same depth, same nesting style.
- Use `std::vector<std::size_t>` as `shape_t`, with row-major strides via `compute_strides(shape) -> shape_t` free function in `shape.h`.
- Functors will live in `quiver::tensor::ops::` (Phase 2). Phase 1 reserves the namespace but ships no functors.
- All tensor errors thread through Quiver's three-pattern format. Phase 1 throws are limited to:
  - `Cannot construct tensor: initializer_list size {got} does not match shape size {expected}`
  - `Cannot access tensor: rank mismatch (expected {rank} indices, got {n})` (in `at()`)
  - `Cannot access tensor: index {i,j,k} out of bounds for shape {a,b,c}` (in `at()`)
  - `Cannot item: tensor has {N} elements (expected 1)` (in `.item()`)

No additional user-specific style preferences captured during this discussion — the user delegated mechanical implementation to standard Quiver conventions and accepted Recommended options on every question.

</specifics>

<deferred>
## Deferred Ideas

The following ideas surfaced during gray-area presentation but belong in later milestones, not Phase 1:

- **Iterator-range and `std::vector<T>&&` take-ownership constructors** — defer to v0.9.0. Non-breaking additions; revisit if v0.9.0 Database bridge needs them for `read_tensor`.
- **Factory functions (`zeros`, `ones`, `full`, `arange`, `linspace`, `zeros_like`)** — defer to v0.9.0 alongside other ergonomic improvements (POL-* in REQUIREMENTS.md).
- **Negative indexing for element access (`tensor(-1)`)** — defer to v0.9.0; consistent with RED-04 (negative axis) defer.
- **Mutable `data() -> T*` accessor** — already deferred to v0.9.0 (REQ-POL-02).
- **`Tensor<T>::scalar(value)` named factory** — rejected as redundant with the constructor; revisit only if user feedback in v0.9.0 indicates pain.
- **Symbol-reference grepping or AST-based BLD-04 check** — defer to a future hardening milestone if `#include`-line grep proves insufficient.
- **Pre-commit hook duplicate of BLD-04 check** — defer; CTest-only is sufficient for v0.8.0.

Out-of-scope-for-the-milestone items already documented in PROJECT.md and REQUIREMENTS.md ("Out of Scope" sections) — autograd, GPU, sparse, BLAS-backed linalg, Element/Database bridge, Tensor C API, language bindings — are not re-listed here. Downstream agents must respect those exclusions.

</deferred>

---

*Phase: 1-Storage and CRTP Scaffold*
*Context gathered: 2026-05-05*
