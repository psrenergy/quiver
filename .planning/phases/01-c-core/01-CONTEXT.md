# Phase 1: C++ Core - Context

**Gathered:** 2026-05-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 1 delivers the `quiver::expr` subsystem in pure C++ — no C API, no bindings, no FFI. Scope:

- `Expression` value type wrapping `shared_ptr<Node>` (no Pimpl — Rule of Zero).
- Virtual `Node` AST hierarchy: `FileNode`, `ScalarNode`, `BinaryOpNode`.
- Four binary operators (`+ - * /`) with scalar broadcast on either side.
- Broadcasting-aware shape compatibility (named-dim union; n×n / 1×n / n×1 size rule).
- Row-by-row `save(path)` engine that materializes any expression tree into a new `.qvr` file.
- New `BinaryFile` fast read/write overloads + free-function row iterators in `quiver::binary` (extensions required by the engine).
- GoogleTest coverage in `tests/test_expression.cpp` covering CORE-01..CORE-06 and TEST-01.

Out of phase: C API (Phase 2), Julia/Dart/Python/JS/Lua bindings (Phases 3-7), reductions / comparison ops / `where()` (v2).

</domain>

<decisions>
## Implementation Decisions

### Shape Compatibility (BinaryOpNode)
- **D-01 — Compatibility model:** operands compose via **broadcasting with named-dim union**. Dimensions match by **name** (string). Same-name dims must satisfy `n×n` / `1×n` / `n×1` — size-1 broadcasts; `(n, m)` with `n,m > 1` and `n ≠ m` → error. Dims that exist on only one side are kept verbatim in the output. Significant departure from DESIGN.md's "strict equality" sketch — locks in here.
- **D-02 — Output dim order:** `output.dims = lhs.dims (in lhs order)` with rhs-only dims appended in rhs order. `a + b` and `b + a` may produce different output dim orderings; values are equivalent.
- **D-03 — Validation timing:** shape compatibility is validated at `BinaryOpNode` construction (eager). Mismatched expressions throw at the operator call, not at `save()`.
- **D-04 — Time-dim compatibility:** when both sides have a same-name time dimension, **`TimeProperties` must match exactly** (frequency, initial_value, parent_dimension_index). Mismatch → throw at construction.
- **D-05 — Operand dim ordering:** dimensions in the operands themselves can appear in any order; matching is purely by name. `compute_row` translates output-dim positions to per-operand input-dim positions.
- **D-06 — Shape error format:** on shape mismatch, throw `"Cannot apply binary operation: dimension '<name>' has incompatible sizes <a> vs <b> (broadcasting requires n×n, 1×n, or n×1)"` — pinpoints the failing dim and its sizes.

### Output Metadata
- **D-07 — Unit:** output `unit = lhs.unit` only when `lhs.unit == rhs.unit`. If they differ → throw at construction (`"Cannot apply binary operation: units differ ('<a>' vs '<b>')"`). Strict dimensional safety. Same rule across all four operators.
- **D-08 — Labels (broadcasting trailing axis):** `BinaryMetadata::labels` is treated as a broadcastable trailing axis with the same n×n / 1×n / n×1 rule:
  - `labels_a == labels_b` element-wise → output uses that vector.
  - `|labels_a| > 1` and `|labels_b| == 1` → output = `labels_a` (b's single value broadcasts across).
  - `|labels_a| == 1` and `|labels_b| > 1` → output = `labels_b` (a's single value broadcasts across).
  - Otherwise (different non-singleton sizes, OR same non-singleton sizes with different content) → throw at construction.
  The save engine broadcasts the size-1 side's value across the n-side's axis when computing each row.
- **D-09 — initial_datetime:** when both sides have any time dimension, their `initial_datetime` must be equal — else throw at construction. When only one side has time dims, the output inherits that side's `initial_datetime`. When neither has time dims, the field is unused.

### FileNode and Save Engine
- **D-10 — FileNode caching:** no caching. Each `FileNode` owns its own `unique_ptr<BinaryFile>`, lazily opened on first `compute_row`. Multiple FileNodes referring to the same path open the file independently (`a + a` opens the file twice). Simple ownership, no canonicalization, RAII closes on FileNode destruction.
- **D-11 — Self-save collision:** at `save(path)` entry, walk the tree, collect FileNode paths, canonicalize, and throw early if `path` matches any input — `"Cannot save: output path collides with input file '<path>'"`. Eager check, runs **before** the writer is opened (avoids partial writes).

### BinaryFile Extensions
- **D-12 — Row iteration:** add free functions in `quiver::binary` namespace operating on `BinaryMetadata` — `quiver::binary::first_dimensions(metadata)` and `quiver::binary::next_dimensions(metadata, current_dims)`. Pure metadata-driven, no `BinaryFile` instance needed. The save engine and `BinaryOpNode::compute_row` use these directly. Keeps `BinaryMetadata` as a Rule-of-Zero value type (no new member functions).
- **D-13 — Fast read overloads:** add to `BinaryFile`:
  - `std::vector<double> read(const std::vector<int64_t>& dims, bool allow_nulls = false)` — validates dims like the existing path. General-purpose fast read.
  - `void read_into(const std::vector<int64_t>& dims, std::vector<double>& out, bool allow_nulls = false)` — **skips dimension validation** (trusted-caller fast path). Engine uses this in the inner loop with a single reused row buffer; the engine guarantees valid dims via `next_dimensions`.
- **D-14 — Fast write overload:** add `void BinaryFile::write(const std::vector<double>& data, const std::vector<int64_t>& dims)` — validates dims (engine writes are roughly half the read count; cost is acceptable). Symmetric with D-13. The slow `unordered_map` write overload remains for now.

### Claude's Discretion
- **Public/internal split for `Node` and subclasses.** DESIGN.md sketches public headers (`include/quiver/expr/node.h`); follow that unless planning surfaces a reason to keep them internal. Bindings only need `Expression`, but a public Node hierarchy lets advanced users derive custom nodes.
- **Test fixture strategy.** Programmatic `.qvr` generation in GoogleTest `SetUp()` (matches `test_binary_file.cpp` convention) vs committed fixtures under `tests/fixtures/expr/`. Prefer programmatic to keep tests self-contained.
- **Exact wording of error messages** beyond the structures decided in D-06 / D-07 / D-09 / D-11.
- **Coexistence vs deprecation of the `unordered_map`-based `read`/`write` overloads.** Phase 1 adds the new overloads; whether to mark the old ones deprecated or leave them is a Claude call (default: leave them — WIP project, breaking changes acceptable, but no need to remove what existing tests/CSV converter still use).
- **Whether to remove the protected `BinaryFile::next_dimensions` member** in favor of the new free function. Default: keep the protected member as a thin delegate to the new free function (or remove if no longer needed by `BinaryFile` internals).
- **Header organization for the new free functions.** Either co-locate in `binary_metadata.h` or create `include/quiver/binary/iteration.h`. Default: separate header keeps `BinaryMetadata` clean.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project planning
- `.planning/PROJECT.md` — Quiver project context, locked decisions for the lazy-expressions subsystem, constraints (C++20, no new third-party deps).
- `.planning/REQUIREMENTS.md` — v1 requirements (CORE-01..06 and TEST-01 map to Phase 1; v2 deferred items remain out of scope).
- `.planning/ROADMAP.md` §"Phase 1: C++ Core" — phase goal, dependencies, requirement mapping, 5 success criteria.
- `.planning/research/DESIGN.md` — architectural sketch for `quiver::expr`: AST shape, public C++ API sketch, internal Node hierarchy, save() engine pseudocode, build wiring, test plan, open questions. **Note:** DESIGN.md's "strict shape equality" assumption is superseded by D-01 (broadcasting model).

### Codebase analysis (2026-04-18)
- `.planning/codebase/STRUCTURE.md` — directory layout, naming conventions, "Where to add new code" (especially "New value type" and "New binary subsystem method").
- `.planning/codebase/CONVENTIONS.md` — naming, code style, error patterns, comments, function design.
- `.planning/codebase/ARCHITECTURE.md` — three-layer design, transaction model, error handling patterns.
- `.planning/codebase/TESTING.md` — GoogleTest organization, fixture conventions.
- `.planning/codebase/STACK.md` — toolchain, C++20 setup, FetchContent dependencies.
- `.planning/codebase/CONCERNS.md` — known issues to watch.

### Project conventions
- `CLAUDE.md` — error patterns (`Cannot {op}: {reason}` / `{Entity} not found: {id}` / `Failed to {op}: {reason}`), naming, ABI rules, Pimpl rule, "Clean code over defensive code" principle.
- `CLAUDE.md` §"Binary Performance Bottlenecks" — concrete numbers for the `unordered_map` dims (~40%) and `validate_dimension_values` (~19%) overheads. Motivates D-13 (skip-validation in `read_into`) and the choice to add fast overloads in this phase.

### Existing code touched in this phase
- `include/quiver/binary/binary_file.h` — current `BinaryFile` API; gains new `read(vector<int64_t>)`, `read_into(...)`, `write(data, vector<int64_t>)` overloads.
- `include/quiver/binary/binary_metadata.h` — current `BinaryMetadata` struct (Rule of Zero); the new free functions `first_dimensions` / `next_dimensions` live alongside it in the `quiver::binary` namespace, in either the same header or a new `iteration.h`.
- `src/binary/binary_file.cpp` — write registry (lines ~17-44), `next_dimensions` reference impl (lines ~245-276), `dimension_sizes_at_values` (lines ~278+) — iteration logic to expose as free functions per D-12.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`BinaryFile::open_file(path, 'r'|'w', metadata?)`** — used as-is by `FileNode` (read) and the save engine (write). Returns a movable `BinaryFile`; the FileNode owns its handle via `unique_ptr<BinaryFile>`.
- **`BinaryFile::next_dimensions` + `dimension_sizes_at_values`** (currently `protected` in `binary_file.cpp`) — reference implementations for the new free-function iterator helpers (D-12). Logic includes time-dimension consistency adjustment when an inner time dim resets before its parent dim increments.
- **Write registry in `src/binary/binary_file.cpp`** — already prevents reading a file currently registered as a writer. The eager pre-check in D-11 layers a friendlier error message on top.
- **`BinaryMetadata::validate()` / `validate_time_dimension_metadata()` / `validate_time_dimension_sizes()`** — should be invoked when constructing the output metadata for `BinaryOpNode` to ensure the broadcasted union is itself a valid metadata.

### Established Patterns
- **Rule-of-Zero value types** (`Element`, `Row`, `BinaryMetadata`, `Dimension`, `TimeProperties`) — `Expression` follows this. No Pimpl, value semantics, copyable + movable.
- **Pimpl reserved for classes hiding private dependencies** (CLAUDE.md). `Expression` exposes nothing private → no Pimpl. `BinaryFile` keeps its existing Pimpl.
- **Three error patterns** — all new throws in this phase use `"Cannot {op}: {reason}"` (D-06 / D-07 / D-09 / D-11).
- **One-file-per-concern naming** — new sources: `src/expr/expression.cpp` (engine + operators), `src/expr/nodes.cpp` (Node hierarchy implementations). Mirrors the `database_*.cpp` per-concern split.
- **Test conventions** — `tests/test_expression.cpp` mirrors implementation. GoogleTest `TEST_F` fixtures with programmatic `.qvr` setup (see `tests/test_binary_file.cpp`). Test exec is `quiver_tests.exe`.

### Integration Points
- **New public headers:** `include/quiver/expr/expression.h`, `include/quiver/expr/node.h` (DESIGN.md sketches; pending Claude's-discretion review of public-vs-internal Node).
- **New implementation files:** `src/expr/expression.cpp`, `src/expr/nodes.cpp`.
- **New iterator helpers:** free functions in `quiver::binary` namespace — header location is Claude's choice (co-locate in `binary_metadata.h` vs new `iteration.h`).
- **`BinaryFile` API extensions:** D-13 / D-14 add three new public overloads on `BinaryFile`. Existing `unordered_map`-based methods stay (CSV converter and existing tests still depend on them; coexistence is fine).
- **Build wiring:** add new sources to the `quiver` target in `src/CMakeLists.txt`. No new dependencies, no `cmake/Dependencies.cmake` change.
- **Tests:** `tests/test_expression.cpp` added to `quiver_tests` in `tests/CMakeLists.txt`. Suggested cases (DESIGN.md + the broadcast decisions above): `IdentityFile`, `AddTwoFilesAndScale`, `ScalarBroadcast` (each of `+ - * /`), `Chained`, `MismatchedShapes`, `BroadcastSizeOneDim`, `BroadcastLabelsAxis`, `UnionDimsAcrossOperands` (e.g., `a=[scenario,time] + b=[time,stage]`), `UnitMismatchThrows`, `TimePropertiesMismatchThrows`, `SelfSaveCollisionThrows`.

</code_context>

<specifics>
## Specific Ideas

The user's broadcasting model is the central decision and deviates from DESIGN.md's "strict equality" sketch. Captured verbatim from the discussion:

> **D-01 (shape rule):** "It should be the union of the dimensions. And if dimensions match, those size should: n×n, 1×n, n×1. If is n×m it should raise an error."

> **D-08 (labels rule):** "If labels_a == labels_b: ok; elseif size(labels_a) > 1 and size(labels_b) == 1: ok; elseif size(labels_a) == 1 and size(labels_b) > 1: ok; else error"

These rules apply uniformly to all four binary operators (`+ - * /`) and to scalar-broadcast operators (`a + 2.0`, `2.0 * a`). Scalars are conceptually a degenerate case of a 1-shape operand: a `ScalarNode` always broadcasts against its sibling.

The save engine reuses a single per-row `vector<double>` output buffer across iterations (DESIGN.md "per-row temporaries reused", reinforced by D-13 — `read_into` writes into a caller-provided buffer with no allocation in the inner loop).

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope. v1 / v2 feature scoping is already locked in `REQUIREMENTS.md` and `PROJECT.md`'s "Out of Scope" section (reductions, comparison ops, `where()`, unary minus, `pow`, `abs`, GPU/SIMD, in-place evaluation — all v2 or beyond).

</deferred>

---

*Phase: 1-C++ Core*
*Context gathered: 2026-05-06*
