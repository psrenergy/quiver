# Phase 1: C++ Core — Research

**Researched:** 2026-05-06
**Phase:** 1 of 7 (C++ Core)
**Domain:** New `quiver::expr` AST subsystem on top of existing `BinaryFile` + `BinaryMetadata`
**Confidence:** HIGH (all claims verified against in-repo code; no external library research required — phase uses only the existing stack)
**Requirements addressed:** CORE-01, CORE-02, CORE-03, CORE-04, CORE-05, CORE-06, TEST-01

## Summary

Phase 1 adds a new `quiver::expr` namespace to the C++ core. It introduces a small AST (`Node` virtual base + `FileNode`, `ScalarNode`, `BinaryOpNode`), a `Expression` value type wrapping `shared_ptr<Node>`, four binary operators with scalar broadcast, and a row-by-row `save(path)` engine. The save engine and `BinaryOpNode::compute_row` need three new public overloads on `BinaryFile` (D-13/D-14: typed-vector dims for fast read/write) and two new free functions in `quiver::binary` (D-12: `first_dimensions` / `next_dimensions`). All work goes into the existing `quiver` shared library target — no new third-party dependencies, no C API, no bindings, no FFI in this phase.

The locked broadcasting model (D-01..D-09) replaces DESIGN.md's "strict equality" sketch with named-dim union: dims match by string name, sizes follow `n×n` / `1×n` / `n×1` rule, dim union is computed at `BinaryOpNode` construction (eager validation), and labels are treated as a broadcastable trailing axis under the same rule. Output dim order is `lhs.dims ++ rhs-only dims`. Operator returns are validated and rejected at construction time, not at `save()`. Test coverage in `tests/test_expression.cpp` maps every requirement to a `TEST_F` case, all using the same `BinaryTempFileFixture`-style programmatic .qvr setup as `tests/test_binary_file.cpp`.

**Primary recommendation:** Land the `BinaryFile` D-13/D-14 overloads and the `quiver::binary` D-12 free functions first (both are mechanical additions), then build the AST on top. Engine inner loop must call `read_into` (skip-validation) with a single reused `vector<double>` row buffer to satisfy CORE-06 and the documented hot-path optimizations in `CLAUDE.md` §"Binary Performance Bottlenecks".

## User Constraints (from CONTEXT.md)

### Locked Decisions

**Shape Compatibility (BinaryOpNode)**
- **D-01 — Compatibility model:** operands compose via **broadcasting with named-dim union**. Dimensions match by **name** (string). Same-name dims must satisfy `n×n` / `1×n` / `n×1` — size-1 broadcasts; `(n, m)` with `n,m > 1` and `n ≠ m` → error. Dims that exist on only one side are kept verbatim in the output. Significant departure from DESIGN.md's "strict equality" sketch — locks in here.
- **D-02 — Output dim order:** `output.dims = lhs.dims (in lhs order)` with rhs-only dims appended in rhs order. `a + b` and `b + a` may produce different output dim orderings; values are equivalent.
- **D-03 — Validation timing:** shape compatibility is validated at `BinaryOpNode` construction (eager). Mismatched expressions throw at the operator call, not at `save()`.
- **D-04 — Time-dim compatibility:** when both sides have a same-name time dimension, **`TimeProperties` must match exactly** (frequency, initial_value, parent_dimension_index). Mismatch → throw at construction.
- **D-05 — Operand dim ordering:** dimensions in the operands themselves can appear in any order; matching is purely by name. `compute_row` translates output-dim positions to per-operand input-dim positions.
- **D-06 — Shape error format:** on shape mismatch, throw `"Cannot apply binary operation: dimension '<name>' has incompatible sizes <a> vs <b> (broadcasting requires n×n, 1×n, or n×1)"` — pinpoints the failing dim and its sizes.

**Output Metadata**
- **D-07 — Unit:** output `unit = lhs.unit` only when `lhs.unit == rhs.unit`. If they differ → throw at construction (`"Cannot apply binary operation: units differ ('<a>' vs '<b>')"`). Strict dimensional safety. Same rule across all four operators.
- **D-08 — Labels (broadcasting trailing axis):** `BinaryMetadata::labels` is treated as a broadcastable trailing axis with the same n×n / 1×n / n×1 rule:
  - `labels_a == labels_b` element-wise → output uses that vector.
  - `|labels_a| > 1` and `|labels_b| == 1` → output = `labels_a` (b's single value broadcasts across).
  - `|labels_a| == 1` and `|labels_b| > 1` → output = `labels_b` (a's single value broadcasts across).
  - Otherwise (different non-singleton sizes, OR same non-singleton sizes with different content) → throw at construction.
  The save engine broadcasts the size-1 side's value across the n-side's axis when computing each row.
- **D-09 — initial_datetime:** when both sides have any time dimension, their `initial_datetime` must be equal — else throw at construction. When only one side has time dims, the output inherits that side's `initial_datetime`. When neither has time dims, the field is unused.

**FileNode and Save Engine**
- **D-10 — FileNode caching:** no caching. Each `FileNode` owns its own `unique_ptr<BinaryFile>`, lazily opened on first `compute_row`. Multiple FileNodes referring to the same path open the file independently (`a + a` opens the file twice). Simple ownership, no canonicalization, RAII closes on FileNode destruction.
- **D-11 — Self-save collision:** at `save(path)` entry, walk the tree, collect FileNode paths, canonicalize, and throw early if `path` matches any input — `"Cannot save: output path collides with input file '<path>'"`. Eager check, runs **before** the writer is opened (avoids partial writes).

**BinaryFile Extensions**
- **D-12 — Row iteration:** add free functions in `quiver::binary` namespace operating on `BinaryMetadata` — `quiver::binary::first_dimensions(metadata)` and `quiver::binary::next_dimensions(metadata, current_dims)`. Pure metadata-driven, no `BinaryFile` instance needed. The save engine and `BinaryOpNode::compute_row` use these directly. Keeps `BinaryMetadata` as a Rule-of-Zero value type (no new member functions).
- **D-13 — Fast read overloads:** add to `BinaryFile`:
  - `std::vector<double> read(const std::vector<int64_t>& dims, bool allow_nulls = false)` — validates dims like the existing path. General-purpose fast read.
  - `void read_into(const std::vector<int64_t>& dims, std::vector<double>& out, bool allow_nulls = false)` — **skips dimension validation** (trusted-caller fast path). Engine uses this in the inner loop with a single reused row buffer; the engine guarantees valid dims via `next_dimensions`.
- **D-14 — Fast write overload:** add `void BinaryFile::write(const std::vector<double>& data, const std::vector<int64_t>& dims)` — validates dims (engine writes are roughly half the read count; cost is acceptable). Symmetric with D-13. The slow `unordered_map` write overload remains for now.

### Claude's Discretion
- Public/internal split for `Node` and subclasses. Default: follow DESIGN.md's public sketch (`include/quiver/expr/node.h`) unless planning surfaces a reason otherwise.
- Test fixture strategy. Default: programmatic `.qvr` generation in GoogleTest `SetUp()` (matches `test_binary_file.cpp` convention).
- Exact wording of error messages beyond the structures decided in D-06 / D-07 / D-09 / D-11.
- Coexistence vs deprecation of the `unordered_map`-based `read`/`write` overloads. Default: leave them — WIP project, breaking changes acceptable, but no need to remove what existing tests/CSV converter still use.
- Whether to remove the protected `BinaryFile::next_dimensions` member. Default: keep the protected member as a thin delegate to the new free function (or remove if no longer needed by `BinaryFile` internals).
- Header organization for the new free functions. Default: separate header `include/quiver/binary/iteration.h` keeps `BinaryMetadata` clean.

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope. v1 / v2 feature scoping is locked in `REQUIREMENTS.md` and `PROJECT.md`'s "Out of Scope" section (reductions, comparison ops, `where()`, unary minus, `pow`, `abs`, GPU/SIMD, in-place evaluation — all v2 or beyond).

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CORE-01 | User can construct an `Expression` from an open `BinaryFile` via implicit conversion | DESIGN.md sketch defines `Expression(const BinaryFile&)` non-explicit ctor; the `FileNode` captures `path` + `metadata` snapshot at construction (D-10). Verified — `BinaryFile` already exposes `get_file_path()` and `get_metadata()`. |
| CORE-02 | User can compose two Expressions with `+ - * /` to produce a new lazy Expression | Free-function operators in `quiver::expr` returning `Expression` wrapping `shared_ptr<BinaryOpNode>`. Eager shape/unit/time/labels validation per D-01..D-09. |
| CORE-03 | User can use scalar literals on either side of an operator and the scalar broadcasts to sibling's row shape | `ScalarNode` captures a 1-shape (or sibling-shape) metadata at construction. Each operator has 3 overloads: `(Expr, Expr)`, `(Expr, double)`, `(double, Expr)`. |
| CORE-04 | User can chain operators into trees of arbitrary depth | `BinaryOpNode` holds `shared_ptr<Node> lhs_, rhs_` — composes recursively without depth limit. Trees share subtrees via `shared_ptr`. |
| CORE-05 | User can call `expression.save(path)` to materialize the lazy tree to a new `.qvr` file | Engine in `Expression::save()`: `BinaryFile::open_file(path, 'w', root_meta)` → row-loop using `quiver::binary::next_dimensions` → `node_->compute_row(dims, row)` → `writer.write(row, dims)`. D-11 self-save check runs first. |
| CORE-06 | Materialization iterates row-by-row reusing per-row buffers across iterations | Engine declares `std::vector<double> row;` once outside the loop, calls `read_into(dims, row)` (D-13) on FileNodes via `BinaryOpNode::compute_row`. No per-row heap allocation in steady state. |
| TEST-01 | C++ test suite verifies arithmetic correctness end-to-end | New `tests/test_expression.cpp` with cases enumerated in Validation Architecture below. |

## Project Constraints (from CLAUDE.md)

Authoritative directives extracted from `CLAUDE.md` and `.planning/PROJECT.md`. Plans MUST verify compliance with each.

### Required Patterns
- **C++20 standard, no C++23 features** (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`).
- **Three error message patterns** (Pattern 1: `"Cannot {operation}: {reason}"`, Pattern 2: `"{Entity} not found: {identifier}"`, Pattern 3: `"Failed to {operation}: {reason}"`). All new throws in this phase use Pattern 1 (D-06, D-07, D-09, D-11).
- **Naming: `verb_[category_]type[_by_id]`** for methods. `lower_case` methods/members/params/namespaces. `CamelCase` types/enum-classes. Trailing `_` for private members.
- **C++20 idioms encouraged** when they simplify logic: designated initializers, `std::variant`, structured bindings, `std::optional`. Do not use trailing return types (disabled in `.clang-tidy`).
- **Pimpl ONLY when hiding private dependencies.** `Expression` wraps `shared_ptr<Node>` → no Pimpl, Rule of Zero. `Node` and subclasses use Rule of Zero where possible.
- **Move semantics:** resource types delete copy + default move. Value types are copyable + movable.
- **Public C++ symbols use `QUIVER_API`** export macro on classes/structs and free functions that need to be exported.
- **`clang-format-21`, LLVM base, 4-space indent, 120 cols, LF endings, `BreakBeforeBraces: Attach`, `PointerAlignment: Left`, `IncludeBlocks: Regroup`.**
- **GoogleTest pattern:** `TEST_F(Fixture, CamelCaseName)`. Use `EXPECT_THROW(expr, std::runtime_error)` for error paths. `TempFileFixture` style — `SetUp()` allocates a temp path, `TearDown()` removes `.qvr`/`.toml`/`.csv` artifacts.

### Forbidden Patterns
- **No new third-party dependencies for v1.** The design uses only `BinaryFile`, `BinaryMetadata`, the standard library.
- **No CRTP / expression templates / template-leaking surfaces in public headers** (rejected in PROJECT.md and DESIGN.md — incompatible with FFI for 5 bindings).
- **No defensive null checks inside the C++ core** ("clean code over defensive code" — assume callers obey contracts).
- **No ad-hoc error message formats.** Use the three canonical patterns.
- **Do NOT deprecate, delete unused code instead.** WIP project; breaking changes acceptable.
- **No symbolic simplification** (`a - a → 0`); no in-place evaluation; no GPU/SIMD; no expression caching across `save()` calls.

### Cross-Layer Implications (Phase 1 only — bindings come later)
- Phase 1 is **C++ core only**. No C API, no FFI, no bindings. C API arrives in Phase 2; bindings in Phases 3-7.
- However, **public surface decisions made now constrain Phase 2**: every public method on `Expression` must be C-API-translatable. `Expression::save(const std::string&)`, `Expression::metadata()`, the four operators, and `Expression(const BinaryFile&)` all map cleanly. No `std::variant` or `std::optional` returns in the public API surface.
- **sol2 visibility (Phase 7 implication, no design action this phase):** Lua binds C++ classes directly via `sol::new_usertype<Expression>(...)`. `Expression` must be a public, copyable type — already satisfied by the value-type + `shared_ptr<Node>` design. `Node` and subclasses do NOT need to be public for sol2 (only `Expression` is bound). Default to public Node hierarchy per DESIGN.md unless planning surfaces a reason to hide it; either way, sol2 binding in Phase 7 is unaffected.

## Architecture Overview

### AST shape (locked by DESIGN.md + D-01..D-11)

```
                    Expression (value type — no Pimpl)
                          │
                  shared_ptr<Node>   (shared subtrees → a + a reuses one Node*)
                          │
                          ▼
                  ┌──────────────┐
                  │   Node       │   virtual base, owns metadata() + compute_row()
                  └──────────────┘
                    ▲     ▲     ▲
                    │     │     │
        ┌───────────┘     │     └────────────┐
        │                 │                  │
   FileNode         ScalarNode          BinaryOpNode
   (path + meta     (double +           (op, lhs, rhs)
    snapshot,        broadcast meta      computes per-row,
    lazy file)       — sibling-driven)   reuses row buffers
```

### Flow: `Expression::save("out.qvr")` (engine, locked by CORE-05/06 + D-11)

```
                  ┌──────────────────────────────────┐
                  │  Expression::save(path)          │
                  └──────────────────────────────────┘
                                   │
              ┌────────────────────┼────────────────────────────────┐
              │                    │                                │
              ▼                    ▼                                ▼
    1. Walk tree → collect    2. Build output meta:        3. Open writer:
       FileNode paths;           root->metadata()              BinaryFile::open_file
       canonicalize; throw                                     (path, 'w', meta)
       D-11 if path matches.                                   (registers in
       (eager — before                                          write_registry,
       writer opens)                                            fills with NaN)
                                                                  │
                                                                  ▼
                                              ┌─────────────────────────────────┐
                                              │  std::vector<double> row;       │  ← one allocation,
                                              │  std::vector<int64_t> dims =    │     reused across
                                              │      first_dimensions(meta);    │     all iterations
                                              │  do {                           │     (CORE-06)
                                              │    root->compute_row(dims, row);│
                                              │    writer.write(row, dims);     │  ← D-14 fast path
                                              │  } while (next_dimensions(...));│
                                              └─────────────────────────────────┘
```

### Flow: `BinaryOpNode::compute_row(out_dims, out_row)`

```
   1. Translate out_dims → lhs_dims (size-1 broadcast: clamp to 1
      where lhs has a size-1 dim; drop dims that don't exist on lhs).
   2. Translate out_dims → rhs_dims (same).
   3. lhs_->compute_row(lhs_dims, lhs_buf_);  ← reused buffers as Node members
   4. rhs_->compute_row(rhs_dims, rhs_buf_);
   5. For each label index k in output:
        a = lhs_buf_[broadcast_label(k, lhs_label_count)]
        b = rhs_buf_[broadcast_label(k, rhs_label_count)]
        out_row[k] = apply(op_, a, b)
```

The buffers `lhs_buf_` and `rhs_buf_` are mutable members of `BinaryOpNode` so that `compute_row()` (a `const` method on `Node`) can reuse them. They start empty and grow on first call; subsequent calls reuse capacity.

### Component Responsibilities

| Component | Responsibility | Owned State |
|-----------|---------------|-------------|
| `Expression` | Public value type. Implicit conversion from `BinaryFile`. Operator overloads. `save(path)` engine entry. | `shared_ptr<Node> node_` |
| `Node` (virtual base) | Contract: `metadata()` returns metadata of this subtree's output; `compute_row(dims, out)` produces one row of doubles for the given output coordinates. | none |
| `FileNode` | Leaf wrapping a `.qvr` file. Captures path + metadata snapshot at construction (from a `BinaryFile&`). On first `compute_row`, opens the file (D-10, lazy). | `path_`, `meta_`, `mutable unique_ptr<BinaryFile> file_` |
| `ScalarNode` | Degenerate-shape leaf for scalar literals. Captures sibling metadata at construction so it can return a row of N copies of `value_` where N = label count of broadcast. | `value_`, `broadcast_meta_` |
| `BinaryOpNode` | Internal node. Constructor validates shape (D-01), unit (D-07), time props (D-04), initial_datetime (D-09), labels (D-08). Caches the broadcast metadata, the per-operand dim/label translation tables, and reusable `lhs_buf_`/`rhs_buf_`. | `op_`, `lhs_`, `rhs_`, broadcast_meta_, lhs_translate_, rhs_translate_, mutable lhs_buf_, mutable rhs_buf_ |
| Free fn `quiver::binary::first_dimensions(meta)` | Returns `vector<int64_t>` initialized to per-dim initial_value (1 for non-time, time->initial_value for time dims). | none — pure |
| Free fn `quiver::binary::next_dimensions(meta, current)` | Increments rightmost dim, carries over with time-dim reset adjustment per existing logic in `BinaryFile::next_dimensions`. Returns next position; engine signals end via "all-dims wrapped" detection. | none — pure |
| `BinaryFile::read(vector<int64_t>, allow_nulls)` (new) | Validates dims, computes file position via dimension index, returns fresh `vector<double>`. | reuses Pimpl |
| `BinaryFile::read_into(vector<int64_t>, vector<double>&, allow_nulls)` (new) | **Skips dim validation**. Computes file position, reads directly into provided buffer (resizes only if needed). | reuses Pimpl |
| `BinaryFile::write(vector<double>, vector<int64_t>)` (new) | Validates dims and data length, computes position, writes. | reuses Pimpl |

### Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Lazy expression composition (Expression / Node AST) | C++ core (`src/expr/`) | — | Logic must live in C++ per project principle "Intelligence: Logic resides in C++ layer." |
| Row-by-row materialization (save engine) | C++ core (`src/expr/expression.cpp`) | — | Same principle; engine consumes the existing `BinaryFile` API. |
| Binary file fast read/write overloads (D-13/D-14) | C++ binary subsystem (`src/binary/binary_file.cpp`) | — | Extension to existing `BinaryFile` Pimpl; lives where the existing `read`/`write` live. |
| Free-function row iteration (D-12) | C++ binary subsystem (`include/quiver/binary/iteration.h`, `src/binary/iteration.cpp`) | — | Pure metadata-driven; lives in the binary namespace per CLAUDE.md "New binary subsystem method." |
| GoogleTest coverage | C++ test target `quiver_tests` (`tests/test_expression.cpp`) | — | Standard project convention — mirrors source file naming. |
| C API / FFI | (deferred to Phase 2) | — | Out of scope this phase. |
| Bindings (Julia, Dart, Python, JS, Lua) | (deferred to Phases 3-7) | — | Out of scope this phase. |

## File Inventory

| Path | Type | Purpose |
|------|------|---------|
| `include/quiver/expr/expression.h` | New public header | `quiver::expr::Expression` class declaration; free-function operator overloads (`+ - * /`) for `(Expression, Expression)`, `(Expression, double)`, `(double, Expression)`. Exports via `QUIVER_API`. |
| `include/quiver/expr/node.h` | New public header (per DESIGN.md default) | Virtual `Node` base + `FileNode` / `ScalarNode` / `BinaryOpNode` declarations, `BinaryOp` enum class. Public so advanced users could subclass; bindings only need `Expression`. Exports via `QUIVER_API`. |
| `include/quiver/binary/iteration.h` | New public header (Claude's-discretion default) | Free functions `quiver::binary::first_dimensions(const BinaryMetadata&)` and `quiver::binary::next_dimensions(const BinaryMetadata&, const std::vector<int64_t>& current)`. Returns `std::optional<std::vector<int64_t>>` so engine can signal "no more rows" cleanly without sentinel comparisons. |
| `src/expr/expression.cpp` | New impl | `Expression` ctor/dtor/copy/move (compiler-generated), implicit `Expression(const BinaryFile&)` ctor (constructs `FileNode`), `metadata()`, `save(path)` engine, `node()` accessor, all four free-function operator overloads + scalar variants. |
| `src/expr/nodes.cpp` | New impl | `FileNode`, `ScalarNode`, `BinaryOpNode` definitions including `BinaryOpNode`'s broadcast validation logic (D-01..D-09), per-operand translation table builders, and `compute_row` implementations. |
| `src/binary/iteration.cpp` | New impl | Free-function impl. Reuses the time-dimension reset adjustment logic currently in `BinaryFile::next_dimensions` (lines 245-276) and `dimension_sizes_at_values` (lines 278-348). Refactor: lift those two into pure functions of `BinaryMetadata`; `BinaryFile::next_dimensions` becomes a thin delegate (or is removed if unused — see Claude's discretion). |
| `tests/test_expression.cpp` | New test | GoogleTest cases for CORE-01..06 + TEST-01. Uses fixture pattern from `test_binary_file.cpp` (programmatic `.qvr` setup, `.qvr`/`.toml` cleanup in `TearDown`). |
| `include/quiver/binary/binary_file.h` | EDIT — add overloads | Add 3 new public method declarations: `read(const vector<int64_t>&, bool)`, `read_into(const vector<int64_t>&, vector<double>&, bool)`, `write(const vector<double>&, const vector<int64_t>&)`. Existing `unordered_map` overloads remain. |
| `src/binary/binary_file.cpp` | EDIT — add overload impls | Implement the three new overloads. Each calls a private `calculate_file_position_indexed(const vector<int64_t>&)` that uses precomputed strides (dot product, no hashing) — net new helper. `read_into` skips `validate_dimension_values`; the validating overloads call it. |
| `src/CMakeLists.txt` | EDIT — add sources | Append `expr/expression.cpp`, `expr/nodes.cpp`, `binary/iteration.cpp` to `QUIVER_SOURCES`. |
| `tests/CMakeLists.txt` | EDIT — add test | Append `test_expression.cpp` to `quiver_tests` source list. |

**Deltas:** 7 new files (3 headers, 3 impls, 1 test) + 4 edited files (2 binary, 2 CMakeLists). No `cmake/Dependencies.cmake` change. No C API change. No binding change. No `tests/schemas/` change (binary subsystem doesn't use SQL fixtures).

## Existing Code Touchpoints

| File:Line | What lives there | How Phase 1 uses it |
|-----------|------------------|---------------------|
| `include/quiver/binary/binary_file.h:33-38` | `BinaryFile::open_file(path, mode, metadata)`, `read(unordered_map, bool)`, `write(data, unordered_map)` | `FileNode` opens via `open_file('r')`. Engine writer opens via `open_file('w', meta)`. New overloads added in same class. |
| `include/quiver/binary/binary_file.h:41-43` | `get_metadata()`, `get_file_path()` | `FileNode` ctor calls both to capture snapshot at Expression construction time (D-10). |
| `include/quiver/binary/binary_file.h:62-63` | Protected `next_dimensions(current)` and `dimension_sizes_at_values(values)` | Reference logic for the new free-function `quiver::binary::next_dimensions`. The protected member becomes a thin delegate (or is deleted — Claude's discretion). |
| `include/quiver/binary/binary_metadata.h:15-42` | `BinaryMetadata` struct (Rule of Zero), `validate()`, `validate_time_dimension_metadata()`, `validate_time_dimension_sizes()`, `add_dimension`, `add_time_dimension` | `BinaryOpNode` constructs the broadcasted output metadata using `add_dimension` / `add_time_dimension` and runs `validate()` to ensure the union is itself well-formed. |
| `src/binary/binary_file.cpp:18` | `static std::unordered_set<std::string> write_registry` | Save engine respects this transparently — opening writer on a path also held by a FileNode for read is fine; opening writer on a path another writer holds throws. D-11 check (path-collision pre-check) layers a friendlier error before the registry rejects. |
| `src/binary/binary_file.cpp:53-104` | `BinaryFile::open_file` with write registry registration | Used as-is by save engine (writer) and FileNode (reader). |
| `src/binary/binary_file.cpp:106-131` | `read(unordered_map, allow_nulls)` — existing slow path | Reference; new fast `read(vector<int64_t>)` parallels structure but uses precomputed strides. |
| `src/binary/binary_file.cpp:133-142` | `write(data, unordered_map)` — existing slow path | Reference; new fast `write(data, vector<int64_t>)` parallels structure. |
| `src/binary/binary_file.cpp:144-158` | `calculate_file_position(unordered_map)` — string lookup per dim | New private helper `calculate_file_position_indexed(vector<int64_t>)` performs the same math via dot product (no hashing). |
| `src/binary/binary_file.cpp:184-236` | `validate_dimension_values(unordered_map)` — ~19% of total time per CLAUDE.md | New `read_into` fast path skips this entirely (D-13). Validating overloads call a new index-keyed validator OR convert to the existing one (planning decision — index-keyed is cleaner). |
| `src/binary/binary_file.cpp:245-276` | `next_dimensions(current)` — protected, time-dim reset logic | **Refactor target:** lift body into free function `quiver::binary::next_dimensions(meta, current)` (D-12). Member becomes delegate or is removed. |
| `src/binary/binary_file.cpp:278-348` | `dimension_sizes_at_values(values)` — calendar arithmetic for variable-size time dims | **Refactor target:** lift into free function (likely private inside `iteration.cpp`) since `next_dimensions` calls it internally. |
| `src/binary/binary_file.cpp:350-377` | `fill_file_with_nulls()` — called once per writer open | No change. Save engine inherits the NaN pre-fill behavior — first `save()` call writes only the cells reachable by `next_dimensions`. Cells outside that traversal stay NaN, which is correct because those cells correspond to nonexistent (out-of-bounds) coords. |
| `src/CMakeLists.txt:2-28` | `QUIVER_SOURCES` list | Append three new sources. |
| `tests/CMakeLists.txt:3-25` | `quiver_tests` source list | Append `test_expression.cpp`. |
| `tests/test_binary_file.cpp:17-52` | `BinaryTempFileFixture` + `make_simple_metadata()` / `make_time_metadata()` | Pattern reference for `tests/test_expression.cpp`'s fixture: temp path, cleanup of `.qvr`/`.toml`/`.csv` in `TearDown`, helpers that build metadata via `BinaryMetadata::from_element(Element().set(...))`. |
| `src/lua_runner.cpp:24-127` | sol2 usertype binding pattern (Database) | Phase 7 reference only — no edit this phase. Confirms `Expression` (a copyable value type) will bind cleanly later via `sol::new_usertype<Expression>` with arithmetic metamethods. No Phase 1 work needed. |
| `include/quiver/element.h:1-50` | `Element` builder | `BinaryMetadata::from_element(Element())` — used by `BinaryTempFileFixture::make_*_metadata()` patterns; copy that pattern into the test fixture. |
| `include/quiver/export.h:1-19` | `QUIVER_API` macro | Decorate every public class/struct/free-function in the new `quiver::expr` and `quiver::binary::iteration` namespaces. |

## Implementation Approach

### 1. `BinaryFile` D-13/D-14 fast overloads — land first

**Why first:** Engine and `BinaryOpNode::compute_row` both depend on these. They are mechanical extensions to an existing class. Implementing and testing them in isolation gives the planner a clean dependency boundary.

**Approach:**
1. Add private member `std::vector<int64_t> strides_` to `BinaryFile::Impl`. Compute on construction (one stride per dimension, last dim has stride 1, multiply right-to-left). This is shared by all three new overloads.
2. Add private helper `int64_t calculate_file_position_indexed(const std::vector<int64_t>& dims) const` — pure dot product `sum((dims[i]-1) * strides_[i]) * label_count * sizeof(double)`. No string hashing.
3. Add private helper `void validate_dimension_values_indexed(const std::vector<int64_t>& dims) const` — equivalent to the existing string-keyed validator but accepts a vector in declaration order. (Or share a single implementation taking `int64_t fetch(size_t idx)` lambda.)
4. Implement public `std::vector<double> read(const std::vector<int64_t>&, bool)` — calls `validate_dimension_values_indexed`, allocates fresh `vector<double>`, reads, applies null-check.
5. Implement public `void read_into(const std::vector<int64_t>&, std::vector<double>&, bool)` — **does NOT call validate**. Resizes `out` to label count if its size differs (cheap when already-correct). Reads, applies null-check.
6. Implement public `void write(const std::vector<double>&, const std::vector<int64_t>&)` — calls validators, writes.

**Validation:** New tests in `test_binary_file.cpp` (or a new `test_binary_file_fast.cpp` — Claude's discretion) covering the three overloads' happy path, validation errors, and round-trip equality with the existing `unordered_map` overloads.

### 2. Free-function row iteration (D-12) — land second

**Why second:** Engine depends on it, but it's pure refactor of existing logic.

**Approach:**
1. Create `include/quiver/binary/iteration.h` with two declarations:
   ```cpp
   namespace quiver::binary {
   QUIVER_API std::vector<int64_t> first_dimensions(const BinaryMetadata& meta);
   QUIVER_API std::optional<std::vector<int64_t>> next_dimensions(const BinaryMetadata& meta,
                                                                  const std::vector<int64_t>& current);
   }
   ```
   `next_dimensions` returns `nullopt` when iteration completes (cleaner than mutating in-place + bool return).
2. Create `src/binary/iteration.cpp`. `first_dimensions` initializes a vector of size `meta.dimensions.size()`, with each entry = `dim.is_time_dimension() ? dim.time->initial_value : 1`.
3. `next_dimensions` ports the body of the existing protected `BinaryFile::next_dimensions` (lines 245-276) and the helper `dimension_sizes_at_values` (lines 278-348). Detect end-of-iteration: if after increment the result equals `first_dimensions(meta)`, return `nullopt`.
4. Decide: keep `BinaryFile::next_dimensions` as a delegate (`return *next_dimensions(impl_->metadata, current_dimensions);` with appropriate end-of-iteration handling) or remove it. **Default:** remove if no callers in `quiver`/`quiver_c` reference it (verify with grep). The protected member exists for legacy reasons; D-12 makes the free function authoritative.

**Validation:** New tests in `test_binary_metadata.cpp` (or `test_iteration.cpp`) for both functions across (a) non-time dims only, (b) single time dim, (c) multiple time dims with calendar reset (the case the existing inline comment in `binary_file.cpp:260-273` documents).

### 3. AST: `Node` virtual hierarchy + `Expression` value type

**Approach:**

**`Node` base class:**
```cpp
class QUIVER_API Node {
public:
    virtual ~Node() = default;
    virtual BinaryMetadata metadata() const = 0;
    virtual void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const = 0;
};
```
The signature receives `dims` in **output-metadata declaration order** and writes one row of doubles into `out`. `out` is resized only if size differs (cheap when already-correct). Each concrete Node decides how to translate `dims` (output-order) into its own operand-order coords.

**`FileNode`:**
- Members: `std::string path_`, `BinaryMetadata meta_`, `mutable std::unique_ptr<BinaryFile> file_`, `mutable std::vector<int64_t> own_dims_buf_`.
- Ctor: `FileNode(const BinaryFile& file)` captures `file.get_file_path()` and `file.get_metadata()` (D-10).
- `metadata()` returns `meta_` directly.
- `compute_row(dims, out)`:
  - On first call, `file_ = std::make_unique<BinaryFile>(BinaryFile::open_file(path_, 'r'))` (lazy open).
  - Translate output-order `dims` into own-metadata-order `own_dims_buf_`. (Identity mapping when this is a leaf — the parent BinaryOpNode is responsible for size-1 broadcast clamping before calling.)
  - Call `file_->read_into(own_dims_buf_, out)` (D-13 fast path).

**`ScalarNode`:**
- Members: `double value_`, `BinaryMetadata broadcast_meta_`.
- Ctor: `ScalarNode(double value, BinaryMetadata broadcast_meta)`. Caller (operator overload) passes the sibling's metadata so this scalar reports the right shape for downstream `BinaryOpNode` validation.
- `metadata()` returns `broadcast_meta_`.
- `compute_row(dims, out)`: ignore `dims`; `out.assign(broadcast_meta_.labels.size(), value_);`.

**`BinaryOpNode`** — the load-bearing node:
- Members:
  - `BinaryOp op_;`
  - `std::shared_ptr<Node> lhs_;`
  - `std::shared_ptr<Node> rhs_;`
  - `BinaryMetadata broadcast_meta_;` — pre-computed at construction (D-03).
  - `std::vector<int> lhs_dim_translate_;` — for each output dim index, the index in lhs's metadata, or -1 if absent.
  - `std::vector<int> rhs_dim_translate_;` — same for rhs.
  - `int lhs_label_count_;`, `int rhs_label_count_;` — for label-axis broadcast in `compute_row` (D-08).
  - `mutable std::vector<int64_t> lhs_dims_buf_, rhs_dims_buf_;` — reused per row.
  - `mutable std::vector<double> lhs_buf_, rhs_buf_;` — reused per row.
- Ctor: validate-and-build (the eager validation step from D-03). Ordered checks (cheaper checks first):
  1. **Unit (D-07):** `lhs.unit == rhs.unit` else throw `"Cannot apply binary operation: units differ ('<a>' vs '<b>')"`.
  2. **Shape (D-01):** for each dim name in `lhs.dimensions ∪ rhs.dimensions` (named-dim union):
     - If on both sides: enforce `n×n` / `1×n` / `n×1` (broadcast); else throw with D-06 format.
     - If on only one side: keep verbatim.
  3. **Time props (D-04):** for any dim present on both sides where either is time, `lhs.time` and `rhs.time` must be `==` (frequency, initial_value, parent_dimension_index). Throw `"Cannot apply binary operation: time dimension '<name>' has incompatible TimeProperties"` else.
  4. **Initial datetime (D-09):** if both sides have any time dim, `lhs.initial_datetime == rhs.initial_datetime` else throw `"Cannot apply binary operation: initial_datetime differs"`. If only one side has time dims, output inherits that side's value.
  5. **Labels (D-08):** apply the n×n / 1×n / n×1 rule to `lhs.labels` vs `rhs.labels`. Throw `"Cannot apply binary operation: labels have incompatible sizes <a> vs <b>"` on mismatch.
  6. Build `broadcast_meta_`: dims in lhs order, then rhs-only dims in rhs order (D-02). Sizes: `max(lhs_size, rhs_size)` (the n in n×n/1×n/n×1). Labels: the broadcast result. `unit`: `lhs.unit`. `initial_datetime`: per D-09. Time properties carried over with appropriate `parent_dimension_index` for the new dim ordering.
  7. Run `broadcast_meta_.validate()` to ensure the result is itself a valid `BinaryMetadata` (catches edge cases like duplicate dim names from a buggy union — defense in depth).
  8. Pre-compute `lhs_dim_translate_` and `rhs_dim_translate_` from `broadcast_meta_.dimensions` to each operand's metadata.
- `metadata()` returns `broadcast_meta_`.
- `compute_row(out_dims, out)`:
  1. Resize buffers if needed.
  2. Translate `out_dims` to `lhs_dims_buf_`: for each index `i` in lhs's dims, look up `out_dims[lhs_dim_translate_[i]]`, then clamp to 1 if lhs's size at that dim is 1 (size-1 broadcast).
  3. Same for `rhs_dims_buf_`.
  4. `lhs_->compute_row(lhs_dims_buf_, lhs_buf_)`.
  5. `rhs_->compute_row(rhs_dims_buf_, rhs_buf_)`.
  6. For each label index `k` in `out`: `out[k] = apply(op_, lhs_buf_[lhs_label_idx(k)], rhs_buf_[rhs_label_idx(k)])` where `lhs_label_idx(k) = lhs_label_count_ == 1 ? 0 : k` (size-1 label broadcast per D-08).

**`Expression`** value type:
```cpp
class QUIVER_API Expression {
public:
    Expression(const BinaryFile& file);                         // implicit — CORE-01
    explicit Expression(std::shared_ptr<Node> node);
    BinaryMetadata metadata() const { return node_->metadata(); }
    void save(const std::string& path) const;                   // engine — CORE-05/06
    const std::shared_ptr<Node>& node() const { return node_; }
private:
    std::shared_ptr<Node> node_;
};
```
`Expression(const BinaryFile&)` is non-explicit per DESIGN.md (CORE-01 says "implicit conversion"). It constructs `std::make_shared<FileNode>(file)`. Compiler-generated copy/move/dtor (Rule of Zero).

**Operator overloads** in `quiver::expr` namespace:
```cpp
Expression operator+(const Expression&, const Expression&);
Expression operator+(const Expression&, double);
Expression operator+(double, const Expression&);
// (× 4 for + - * /)
```
The `(Expression, double)` overload constructs a `ScalarNode(value, lhs.metadata())` and routes through `(Expression, Expression)`. The `(double, Expression)` overload does the symmetric construction with rhs's metadata as the broadcast shape. All overloads return `Expression(std::make_shared<BinaryOpNode>(op, lhs.node(), rhs.node()))`.

**Why `BinaryOp` enum + apply lambda over four virtual `compute_row` impls:** keeps the four operators in one place, easy to extend in v2 (`pow`, `min`, `max`). `apply(BinaryOp, double, double)` is a trivial switch — branch prediction makes it fast inside the inner loop, and the compiler inlines well.

### 4. Save engine (`Expression::save(path)`)

```cpp
void Expression::save(const std::string& path) const {
    // D-11: collect FileNode paths, canonicalize, throw if `path` matches any.
    std::vector<std::string> input_paths;
    collect_file_paths(*node_, input_paths);
    auto canonical_out = std::filesystem::weakly_canonical(path).string();
    for (const auto& in : input_paths) {
        if (std::filesystem::weakly_canonical(in).string() == canonical_out) {
            throw std::runtime_error("Cannot save: output path collides with input file '" + in + "'");
        }
    }

    const auto meta = node_->metadata();
    auto writer = BinaryFile::open_file(path, 'w', meta);

    std::vector<int64_t> dims = quiver::binary::first_dimensions(meta);
    std::vector<double> row;
    for (;;) {
        node_->compute_row(dims, row);
        writer.write(row, dims);                                   // D-14 fast path
        auto next = quiver::binary::next_dimensions(meta, dims);
        if (!next) break;                                          // CORE-06: single buffer reuse
        dims = std::move(*next);
    }
}
```

`collect_file_paths` is a small free function in `expression.cpp` that recursively walks the tree (`dynamic_cast<const FileNode*>` checks; otherwise descends into `BinaryOpNode` children). No defensive null checks (clean code over defensive code).

### 5. Test fixture pattern

Mirror `BinaryTempFileFixture` from `tests/test_binary_file.cpp:17-52`. New fixture in `tests/test_expression.cpp`:

```cpp
class ExpressionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path_a = (fs::temp_directory_path() / "quiver_expr_a").string();
        path_b = (fs::temp_directory_path() / "quiver_expr_b").string();
        path_out = (fs::temp_directory_path() / "quiver_expr_out").string();
    }
    void TearDown() override {
        for (const auto& p : {path_a, path_b, path_out})
            for (auto ext : {".qvr", ".toml"})
                if (fs::exists(p + ext)) fs::remove(p + ext);
    }
    std::string path_a, path_b, path_out;

    static BinaryMetadata make_simple_metadata() { /* row=3, col=2, val1/val2 — copy from test_binary_file.cpp */ }
    static BinaryMetadata make_metadata_with_extra_dim() { /* for UnionDimsAcrossOperands */ }
    static void write_qvr(const std::string& path, const BinaryMetadata& meta,
                          std::function<double(int64_t row, int64_t col, int64_t label)> fill) {
        // helper that opens writer, iterates all positions via first/next_dimensions, writes computed values.
    }
};
```

`write_qvr` is the key helper: programmatic .qvr generation parametrized by a fill function lets each test case fix inputs cleanly.

## Validation Architecture

Nyquist gate enforcement: every requirement has at least one named test case in `tests/test_expression.cpp` that runs in under 30 seconds via the existing `quiver_tests.exe` GoogleTest binary.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 (already wired in `tests/CMakeLists.txt:1`) |
| Config file | `tests/CMakeLists.txt` (top-level CMake handles GoogleTest via FetchContent in `cmake/Dependencies.cmake`) |
| Quick run command | `./build/bin/quiver_tests.exe --gtest_filter=Expression.*` |
| Full suite command | `./build/bin/quiver_tests.exe` (or `ctest -C Debug --output-on-failure` from `build/`) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CORE-01 | `Expression e = a;` (implicit conv from `BinaryFile&`); `e.save(path)` produces a file equal to `a` | unit | `quiver_tests --gtest_filter=Expression.IdentityFile` | ❌ Wave 0 |
| CORE-02 | `(a + b).save(path)` produces correct sum row-by-row; same for `-`, `*`, `/` | unit | `quiver_tests --gtest_filter=Expression.AddTwoFiles*:Expression.SubtractTwoFiles*:Expression.MultiplyTwoFiles*:Expression.DivideTwoFiles*` | ❌ Wave 0 |
| CORE-03 | `(a + 2.0).save`, `(2.0 + a).save`, all ops both sides — broadcasts scalar | unit | `quiver_tests --gtest_filter=Expression.ScalarBroadcast*` | ❌ Wave 0 |
| CORE-04 | `((a + b) - c) / 2.0` chain materializes correctly; `(a + a)` chain (same path twice) — D-10 | unit | `quiver_tests --gtest_filter=Expression.Chained:Expression.SamePathTwice` | ❌ Wave 0 |
| CORE-05 | `.save(path)` writes to disk with valid `.qvr` + `.toml`; reopening reads back; D-11 self-save throws | unit | `quiver_tests --gtest_filter=Expression.SaveProducesReadableFile:Expression.SelfSaveCollisionThrows` | ❌ Wave 0 |
| CORE-06 | Engine reuses one `vector<double>` row buffer; verify by code inspection (no `std::vector<double>` allocation in `save()` body inner loop) — also covered by performance smoke test on a 100×100×10 grid that completes within budget | code-review + unit | `quiver_tests --gtest_filter=Expression.LargeGridCompletes` | ❌ Wave 0 |
| TEST-01 | All cases above + edge cases: shape mismatch, unit mismatch, time-property mismatch, initial-datetime mismatch, label-broadcast | unit (full suite) | `quiver_tests --gtest_filter=Expression.*` | ❌ Wave 0 |

### Detailed test case enumeration (planner uses this list verbatim)

| Case Name | Maps To | What it verifies |
|-----------|---------|------------------|
| `IdentityFile` | CORE-01, CORE-05 | Construct `Expression e = a;` from a single `BinaryFile`. Call `e.save(out)`. Reopen `out` and assert byte-for-byte equality with `a` across all coords. |
| `AddTwoFilesAndScale` | CORE-02, CORE-04, TEST-01 | `(a + b) * 2.0`. Assert each output cell is `(a[i] + b[i]) * 2`. |
| `SubtractTwoFiles` | CORE-02 | `a - b`. Assert each cell. |
| `MultiplyTwoFiles` | CORE-02 | `a * b`. Assert each cell. |
| `DivideTwoFiles` | CORE-02 | `a / b` with non-zero `b`. Assert each cell. |
| `ScalarBroadcastAddRight` | CORE-03 | `a + 2.0`. Each cell = `a[i] + 2`. |
| `ScalarBroadcastAddLeft` | CORE-03 | `2.0 + a`. Each cell = `2 + a[i]`. |
| `ScalarBroadcastSubtractRight` | CORE-03 | `a - 2.0`. |
| `ScalarBroadcastSubtractLeft` | CORE-03 | `2.0 - a` (note: not commutative). |
| `ScalarBroadcastMultiplyRight` | CORE-03 | `a * 2.0`. |
| `ScalarBroadcastMultiplyLeft` | CORE-03 | `2.0 * a`. |
| `ScalarBroadcastDivideRight` | CORE-03 | `a / 2.0`. |
| `ScalarBroadcastDivideLeft` | CORE-03 | `2.0 / a`. |
| `Chained` | CORE-04 | `((a + b) - c) / 2.0` — three-deep tree with mixed scalar and file ops. |
| `SamePathTwice` | CORE-04, D-10 | `Expression doubled = a + a;` where `a` is a single FileNode. Save and verify each cell is `2 * a[i]`. Confirms each FileNode opens its own handle (no shared cache). |
| `MismatchedShapesThrows` | TEST-01, D-01, D-06 | `a` has dim `row=3`, `b` has dim `row=4`. `a + b` throws with the D-06 message format. |
| `BroadcastSizeOneDim` | TEST-01, D-01 | `a` has `row=3, col=2`, `b` has `row=1, col=2`. `a + b` succeeds; output has `row=3, col=2`; each row of `a` is added to `b`'s single row. |
| `BroadcastLabelsAxis` | TEST-01, D-08 | `a` has 1 label, `b` has 3 labels. `a + b` succeeds; output uses `b`'s labels; each output label is `a's value + b[label_i]`. |
| `LabelMismatchThrows` | TEST-01, D-08 | `a` has 2 labels `[v1,v2]`, `b` has 2 labels `[v1,v3]`. `a + b` throws (same non-singleton size, different content). |
| `UnionDimsAcrossOperands` | TEST-01, D-02 | `a` has `[scenario, time]`, `b` has `[time, stage]`. Output has `[scenario, time, stage]` per D-02 lhs-order rule. Verify materialized values. |
| `UnitMismatchThrows` | TEST-01, D-07 | `a.unit = "MW"`, `b.unit = "GWh"`. `a + b` throws with the D-07 format. |
| `TimePropertiesMismatchThrows` | TEST-01, D-04 | Both files have a time dim `month`, but `a.time->frequency = Monthly`, `b.time->frequency = Daily`. `a + b` throws. |
| `InitialDatetimeMismatchThrows` | TEST-01, D-09 | Both have time dims; `a.initial_datetime = 2025-01-01`, `b.initial_datetime = 2025-02-01`. `a + b` throws. |
| `SelfSaveCollisionThrows` | CORE-05, D-11 | `Expression e = a; e.save(a's path)` — throws with D-11 format. Verify `a`'s file is unchanged after the throw. |
| `SaveProducesReadableFile` | CORE-05 | After `e.save(out)`, reopen `out` via `BinaryFile::open_file('r')`, read every coord, assert metadata matches `e.metadata()`. |
| `LargeGridCompletes` | CORE-06 | 100×100×5 grid, `(a + b) * 2.0`. Completes in well under 1 second. Combined with code-review of `Expression::save()` body confirming single-buffer reuse, satisfies CORE-06. |
| `SaveOpenedTwiceProducesSameOutput` | CORE-05 | `e.save("out1"); e.save("out2");` — both files identical (no per-call mutation of expression state). |
| `EmptyMetadataMismatchThrows` | edge | Defense: smallest-failing-case (1×1 vs 1×1 with different label content) — throws. |

### Sampling Rate
- **Per task commit:** `./build/bin/quiver_tests.exe --gtest_filter=Expression.*` (~28 cases above; runs in seconds).
- **Per wave merge:** `./build/bin/quiver_tests.exe` (full suite — ensures no regression in existing binary/database tests).
- **Phase gate:** Full suite green before `/gsd-verify-work`. CTest invocation `ctest -C Debug --output-on-failure` from `build/`.

### Wave 0 Gaps
- [ ] `tests/test_expression.cpp` — covers CORE-01..06, TEST-01.
- [ ] (optional, Claude's discretion) `tests/test_iteration.cpp` — covers free `first_dimensions` / `next_dimensions` standalone (or fold into `test_binary_metadata.cpp`).
- [ ] (optional) Extension of `tests/test_binary_file.cpp` covering the new D-13/D-14 overloads (or a new `test_binary_file_fast.cpp`).
- [ ] No new framework install needed — GoogleTest already wired.

## Security Domain

`security_enforcement: true` and `security_asvs_level: 1` per `.planning/config.json`. This is a pure compute-on-local-files subsystem with no network surface, no new secrets, no auth, no PII. Most ASVS categories are N/A.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | N/A — embedded library, no auth layer |
| V3 Session Management | no | N/A — no sessions |
| V4 Access Control | no | N/A — local file I/O only; OS file permissions are the access control |
| V5 Input Validation | yes | All inputs (paths, scalar values, BinaryMetadata) flow through existing validators (`BinaryMetadata::validate()`). New `BinaryOpNode` ctor adds shape/unit/time/labels validation per D-01..D-09. Path inputs are passed verbatim to `BinaryFile::open_file` which already exists and is well-tested. |
| V6 Cryptography | no | N/A — no crypto |
| V7 Error Handling | yes | All throws use the three CLAUDE.md canonical patterns. C++ core handles error messages; bindings (later phases) surface them unchanged. No information disclosure beyond what the existing `BinaryFile` errors already reveal (file paths in errors — consistent with rest of codebase). |
| V8 Data Protection | no | N/A — no sensitive data; library handles user-supplied numerical tensors |
| V12 Files and Resources | yes | `Expression::save(path)` writes a new file. D-11 self-save check prevents overwriting input files (correctness + safety). `BinaryFile`'s existing write registry prevents concurrent open-for-write + open-for-read within a process. Path traversal not applicable — paths are caller-supplied; library does not concatenate user input into paths. RAII ensures file handles close on exception. |

### Known Threat Patterns for {C++ embedded library / file I/O}

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Use-after-free of `Expression` after `save()` | Tampering | Rule of Zero + `shared_ptr<Node>` ownership — `Expression` holds its own ref; save takes `const this`, no mutation. RAII closes `BinaryFile` handles in `FileNode` destructors. |
| Self-save corrupting input file | Tampering | D-11 eager pre-check at `save()` entry — throws before writer opens, so no partial overwrite is possible. Existing write registry would also catch the case at file-open time, but the friendlier message is preferred. |
| Resource exhaustion via deeply chained expressions | DoS | Tree depth is bounded by user-supplied operator chain length; no recursion in core algorithms beyond tree walk. `compute_row` recurses through operands but the depth equals operator count which is user-controlled. Practical limit is stack depth — no defensive cap (consistent with project philosophy "clean code over defensive code"). |
| Integer overflow in dimension sizes | Tampering | Dim sizes inherited from existing `BinaryMetadata::validate()` which already enforces positive sizes and time-dim size bounds (`time_constants.h`). New broadcast logic uses `max(lhs.size, rhs.size)` — both positive, no overflow risk for realistic dim sizes. |
| Concurrent `save()` to the same output | Tampering | Existing write-registry blocks two writers on the same path within a process. Cross-process concurrency is documented as unsupported (`CONCERNS.md` "Binary write registry"). No new threat introduced. |
| Path traversal | Tampering | Paths are passed verbatim to `std::filesystem::weakly_canonical` and the `BinaryFile` layer; library does not concatenate user-controlled fragments into paths. |
| Floating-point exceptions / NaN propagation | Information disclosure | Division by zero produces IEEE 754 inf/NaN — written to output, not thrown. Read-side null check (`allow_nulls`) is a separate concern handled by existing `BinaryFile::read`. Engine writes whatever `compute_row` returns; downstream consumers decide what to do with NaN cells. |

**Conclusion:** No new security controls beyond inheriting existing `BinaryFile` behavior. Phase is low-risk from a security standpoint — D-11 self-save check is the one new safety mechanism, and it's already specified.

## Common Pitfalls

### Pitfall 1: FileNode lazy-open vs. self-save check ordering
**What goes wrong:** If D-11 self-save check ran *after* opening the writer, `BinaryFile::open_file('w')` registers the path in the write-registry first, then any FileNode's lazy open of the same path would fail with the registry-level error rather than the friendlier D-11 message. Worse, the writer's `fill_file_with_nulls()` runs at open time, so the input file's data could already be wiped before the engine notices the collision.
**Why it happens:** Eager vs lazy opening interacts with the registry which is per-process global state.
**How to avoid:** D-11 specifies: walk the tree and canonicalize **before** calling `BinaryFile::open_file('w', path)`. The plan must enforce this ordering. Test: `SelfSaveCollisionThrows` must verify the input file is unchanged after the throw.
**Warning signs:** Test failure where `e.save(input_path)` wipes the input. Or registry "already open for writing" error in place of the D-11 message.

### Pitfall 2: Write registry interaction with FileNode reads
**What goes wrong:** `Expression::save(out)` opens the writer first (registers `out` in the registry). If a FileNode in the tree happens to point at `out` (D-11 missed it somehow — e.g., relative-vs-absolute path discrepancy), the FileNode's lazy open of `out` in read mode would throw `"Cannot open_file: file is already open for writing"` — a confusing error far from the actual root cause.
**Why it happens:** Registry stores canonicalized paths; D-11 must use the same canonicalization (`std::filesystem::weakly_canonical`) the registry uses.
**How to avoid:** D-11 implementation must call `weakly_canonical` on both the output path and every collected FileNode path. Verified by a test that uses different forms (relative path, absolute path, symlink) of the same file.
**Warning signs:** A test passes for absolute-path collision but fails for relative-path collision.

### Pitfall 3: Label-axis broadcast edge cases
**What goes wrong:** D-08 specifies `1×n` broadcast for labels. The implementation must broadcast each row computation (not just the metadata's label list). If `a.labels = ["v"]` and `b.labels = ["x", "y", "z"]`, then `compute_row` for output label index 0 reads `a.row[0]` and `b.row[0]`; for index 1 reads `a.row[0]` again (broadcast) and `b.row[1]`; for index 2 reads `a.row[0]` and `b.row[2]`. If the implementation accidentally indexes `a.row[1]` for output index 1, it will read past the end of `a`'s row buffer. Undefined behavior.
**Why it happens:** Easy to write `out[k] = apply(op, lhs_buf[k], rhs_buf[k])` without the broadcast clamp.
**How to avoid:** `BinaryOpNode` must precompute `lhs_label_count_` and `rhs_label_count_` and the inner loop reads `lhs_buf_[k % lhs_label_count_]` (or equivalently `lhs_label_count_ == 1 ? 0 : k`). Tests `BroadcastLabelsAxis` and `LabelMismatchThrows` verify both broadcast and rejection paths.
**Warning signs:** AddressSanitizer / valgrind heap-buffer-overflow on `BroadcastLabelsAxis`. Or the broadcast test passes but the per-cell value is wrong (e.g., `lhs_buf_[k]` returning whatever happens to be at `&lhs_buf_[1]`).

### Pitfall 4: `compute_row` recursing into BinaryOpNode without buffer reuse
**What goes wrong:** If `BinaryOpNode::compute_row` allocates `lhs_buf_` and `rhs_buf_` as locals (instead of mutable members), each call to `compute_row` allocates two `vector<double>` per node, undoing the per-row buffer reuse promised by CORE-06.
**Why it happens:** `Node::compute_row` is `const`; novice implementation might not realize you can use `mutable` members for buffers.
**How to avoid:** `BinaryOpNode` declares `mutable std::vector<double> lhs_buf_, rhs_buf_;`. `compute_row` is `const` from the user's perspective (the node itself is logically immutable — same inputs produce same outputs) but mutates these reuse buffers. Code review the inner loop. Test `LargeGridCompletes` provides a smoke check.
**Warning signs:** Allocation profiler showing N allocations per row at depth-N tree.

### Pitfall 5: Output dim union when a dim exists on both sides but in different positions
**What goes wrong:** D-02: output dim order = lhs dims (in lhs order) ++ rhs-only dims (in rhs order). When a dim name appears on both sides, it goes in the position where lhs has it. If `a.dims = [scenario, time]` and `b.dims = [time, stage]`, output is `[scenario, time, stage]` — `time` is in lhs's position (index 1), and `stage` (rhs-only) appends. But if you accidentally `[scenario, stage, time]` (forgetting that `time` was already placed by lhs), `BinaryMetadata::validate()` will not catch it — the metadata would be valid but the per-row computation would scramble axes.
**Why it happens:** The "rhs-only dims" filter must skip dims already present in lhs's list, regardless of their position in rhs.
**How to avoid:** Build output dims with two passes: pass 1 = lhs.dims in order; pass 2 = `[d for d in rhs.dims if d.name not in lhs.dim_names]`. Test `UnionDimsAcrossOperands` verifies the output order and value correctness explicitly.
**Warning signs:** `UnionDimsAcrossOperands` produces correct shape but wrong values (axis-permutation bug).

### Pitfall 6: Time-dim parent_dimension_index in broadcast metadata
**What goes wrong:** When the broadcast output reorders dimensions, time-dim `parent_dimension_index` values must be recomputed to point at the right (new) parent dim index. If the impl just copies `parent_dimension_index` from the source, the index can refer to the wrong dim or out-of-range.
**Why it happens:** `parent_dimension_index` is an index into the dimensions array; reordering invalidates indices.
**How to avoid:** When constructing `broadcast_meta_`, after assembling the dim list, walk through and for each time dim, find its parent by *name* in the source operand's metadata, then translate that to the index in `broadcast_meta_.dimensions`. Then call `broadcast_meta_.validate()` which exercises `validate_time_dimension_metadata()` and `validate_time_dimension_sizes()` and would catch frequency-ordering violations.
**Warning signs:** `validate_time_dimension_metadata()` throws inside `BinaryOpNode` ctor when it shouldn't, or `next_dimensions` gives wrong results because parent_dimension_index points to wrong dim.

### Pitfall 7: Assuming operand dim ordering matches output dim ordering (D-05)
**What goes wrong:** If the lhs has dims `[scenario, time]` and rhs has dims `[time, scenario]` (same set, swapped order), the engine produces `compute_row(out_dims)` where `out_dims` is in output order. Without translation, calling `lhs->compute_row(out_dims, lhs_buf)` passes coords in the wrong order to lhs.
**Why it happens:** D-05 explicitly allows operand dim ordering to differ from output ordering. Implementation must translate.
**How to avoid:** `BinaryOpNode` precomputes `lhs_dim_translate_` and `rhs_dim_translate_` (output-dim-index → operand-dim-index, or -1 for "absent"). When calling `lhs->compute_row`, build an operand-ordered `lhs_dims_buf_` from `out_dims` via the translation table. Test would be `OperandDimsInDifferentOrder` (add to the enumeration above if planning surfaces it).
**Warning signs:** `(a + b)` succeeds but values are scrambled when `a` and `b` have same dims in different order.

### Pitfall 8: `BinaryFile::write` overload resolution ambiguity
**What goes wrong:** The existing `write(data, unordered_map)` and the new `write(data, vector<int64_t>)` could be ambiguous if the call site uses an initializer list — `bf.write({1.0, 2.0}, {1, 2})` could resolve to either if both have constructors taking initializer lists.
**Why it happens:** `unordered_map<string, int64_t>` cannot be constructed from `{1, 2}` (needs string keys), but if the impl moves to a different signature it could become ambiguous.
**How to avoid:** Verify all existing call sites in tests/code still resolve correctly after adding the new overload. Use explicit `std::vector<int64_t>{1, 2}` in test code if needed. Adding a `static_assert` style overload-resolution test isn't required, but a quick build-clean of the existing test suite catches regressions.
**Warning signs:** Build error in `test_binary_file.cpp` after adding the new overload.

## Open Questions / Claude's Discretion (RESOLVED)

Items the planner should either address or call out as decisions during planning:

1. **Public/internal split for `Node` and subclasses.**
   - What we know: DESIGN.md sketches public headers (`include/quiver/expr/node.h`). Bindings only need `Expression` to be public (sol2 binds `Expression` directly; C API in Phase 2 wraps `Expression` opaquely; other bindings consume the C API).
   - What's unclear: Is there real value in letting users derive custom Nodes? v2 may add more node types (reductions, comparisons), but those are likely added in-tree, not by users.
   - Recommendation: **Default to public** per DESIGN.md. Cost is negligible (one extra header), and exposing the hierarchy keeps the design self-documenting. If planning surfaces a maintainability concern, move to `src/expr/internal/node.h` (private header).

2. **Header organization for `quiver::binary` free functions (D-12).**
   - Default per CONTEXT.md: separate header `include/quiver/binary/iteration.h`.
   - Alternative: co-locate in `binary_metadata.h` (slight cost: header gains non-member function declarations).
   - Recommendation: **Separate header.** Keeps `BinaryMetadata` clean (Rule of Zero, only struct + factories + validators). Symmetric with how `time_constants.h` is its own header in the binary subsystem.

3. **Coexistence of `BinaryFile::next_dimensions` (protected member) and `quiver::binary::next_dimensions` (free function).**
   - Default per CONTEXT.md: keep the protected member as a delegate (or remove if no internal callers).
   - Verification needed: grep `BinaryFile::next_dimensions` callers across `quiver` and `quiver_c` targets.
   - Recommendation: **Delete the protected member if no callers.** "Delete unused code, do not deprecate" per CLAUDE.md philosophy. If `BinaryFile` itself uses it internally (e.g., from `write` or `read`), keep it as an internal helper.

4. **`next_dimensions` end-of-iteration signaling.**
   - Default: return `std::optional<vector<int64_t>>` — `nullopt` means iteration done.
   - Alternative: return a bool `bool next_dimensions(meta, current, out_next)` (C-style out-param).
   - Recommendation: **`std::optional` return.** Idiomatic C++20, cleaner engine loop, no risk of forgetting to check the bool. Marginal allocation overhead is irrelevant against the file I/O cost dominating per-row work.

5. **Test fixture strategy.**
   - Default per CONTEXT.md: programmatic `.qvr` generation in `SetUp()`.
   - Recommendation: **Programmatic, no `tests/fixtures/expr/` directory.** Self-contained tests are easier to read and don't add a new fixture-management surface.

6. **Whether to treat `(a + a)` as a single shared-subtree or two independent FileNodes.**
   - D-10 locks: each FileNode opens its own handle. So `a + a` opens the file twice.
   - But: when a user writes `auto common = a + b; auto e = common * 2.0;`, `common`'s `BinaryOpNode` is shared via `shared_ptr<Node>`. That is correct and intentional (subtree sharing reduces compute cost in the future when caching is added).
   - Implementation note: `Expression::Expression(const BinaryFile&)` always constructs a fresh `FileNode`. Two independent calls with the same `BinaryFile` produce two independent FileNodes (correct per D-10). But `auto e2 = e1 + e1;` shares `e1`'s subtree (also correct — the subtree is just the FileNode, but the BinaryOpNode owns one shared_ptr to it).
   - Recommendation: No additional decision needed; D-10 + the natural shared_ptr semantics give the right behavior. Test `SamePathTwice` verifies the file-twice case explicitly.

7. **Where do the new overload tests live?**
   - Default: extend `tests/test_binary_file.cpp` with a new section `// Fast Overloads (D-13/D-14)`.
   - Alternative: new file `tests/test_binary_file_fast.cpp`.
   - Recommendation: **Extend `test_binary_file.cpp`.** Same fixture, same convention; one test target.

8. **Operator + for `Expression` vs `BinaryFile&`.**
   - DESIGN.md: `Expression(const BinaryFile&)` is implicit, so `a + b` where both are `BinaryFile&` triggers two implicit conversions and dispatches to the `Expression` operator. Confirmed by the C++ language rules (implicit conversion of arguments to an operator's parameter types).
   - But: a user-facing free function `operator+(const BinaryFile&, const BinaryFile&)` returning `Expression` is *not* needed and would conflict.
   - Recommendation: No new operator overloads on `BinaryFile`. The implicit conversion is sufficient.

## Pitfalls and Risks

(Distinct from "Common Pitfalls" — this is the cross-cutting risk register for the planner.)

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| `BinaryOpNode` ctor validation logic accumulates complexity (D-01..D-09 are 6 distinct checks) | Medium | Medium — bugs hide in branches not exercised by tests | Plan one task per validation check (`unit-mismatch`, `shape-mismatch`, `time-props`, `initial-datetime`, `labels`). Each task has a paired test. Final task assembles and runs all together. |
| Refactor of existing `BinaryFile::next_dimensions` accidentally changes semantics | Low | High — breaks every iteration consumer | Side-by-side test: keep the existing protected member (or stub it) for the duration of the refactor; have a test that calls both old and new and asserts identical output across (non-time, single-time, multi-time) metadata fixtures. |
| Symbol visibility / DLL export for `Node` virtual base | Low | High — link error or "missing vtable" on Windows | Apply `QUIVER_API` to `Node`, `FileNode`, `ScalarNode`, `BinaryOpNode`, and `Expression` consistently. Verified by building `quiver_tests.exe` on Windows. |
| `mutable unique_ptr<BinaryFile>` in FileNode + `shared_ptr<Node>` shared subtree → two threads calling `compute_row` race on `file_` lazy open | Low | Medium — the codebase is documented single-threaded, but this introduces a new sharper variant | Document in code comment; existing project policy is single-thread use. No new mitigation required. |
| Pre-fill NaN cost of large output files (`fill_file_with_nulls` writes the entire file) | Low | Low — known existing cost; engine inherits it | Inherited from `BinaryFile`; not a Phase 1 issue. Documented in CONCERNS.md. |
| Engine integer overflow on large dim sizes when computing total cell count | Low | Low — saved files would be unreasonably huge anyway | `BinaryMetadata::validate()` already enforces bounded dim sizes via `time_constants.h`. Non-time dims can be arbitrarily large in theory; `int64_t` strides and positions handle realistic sizes. |
| Test `LargeGridCompletes` is timing-dependent and could be flaky on slow CI runners | Medium | Low — false fails | Use a generous budget (e.g., 5 s for 100×100×5) or skip wall-clock assertions and rely on code-review for CORE-06 verification. Code review of the inner loop in `Expression::save()` is mandatory regardless. |
| `BinaryOp` enum scope — `quiver::expr::BinaryOp::Add` vs C++ `Add` shadowing | Low | Low | Use `enum class BinaryOp` (scoped). DESIGN.md already specifies this. |
| C++20 designated-initializer adoption in `BinaryMetadata` construction inside ctor | Low | Low | `BinaryMetadata` is a struct with public members; designated initializers work. Project already uses them (CONVENTIONS.md). |

## Sources

### Primary (HIGH confidence — verified in-repo)
- `.planning/PROJECT.md` — Project goals, constraints, key decisions, out-of-scope items.
- `.planning/REQUIREMENTS.md` — v1 requirements CORE-01..06, CAPI-01..03, BIND-01..05, TEST-01..03 with phase mapping.
- `.planning/ROADMAP.md` — 7-phase plan, dependency chain, per-phase success criteria.
- `.planning/research/DESIGN.md` — Architectural sketch for `quiver::expr` (AST shape, public C++ API sketch, save() pseudocode, build wiring, test plan, open questions). DESIGN.md's "strict equality" is superseded by D-01.
- `.planning/phases/01-c-core/01-CONTEXT.md` — Phase 1 locked decisions D-01 through D-14, Claude's discretion items, and existing-code reusable assets.
- `.planning/codebase/STRUCTURE.md` — Directory layout, "Where to add new code" ("New value type", "New binary subsystem method").
- `.planning/codebase/CONVENTIONS.md` — Naming, code style, error patterns, function design.
- `.planning/codebase/ARCHITECTURE.md` — Three-layer design, transaction model, error handling patterns.
- `.planning/codebase/TESTING.md` — GoogleTest organization, fixture conventions, test discovery.
- `.planning/codebase/STACK.md` — C++20, FetchContent dependencies, compiler flags, no new deps allowed.
- `.planning/codebase/CONCERNS.md` — "Binary Performance Bottlenecks" hot path documentation; thread-safety caveats on `write_registry`.
- `CLAUDE.md` — Error message patterns, naming conventions, ABI rules, Pimpl rule, "Binary Performance Bottlenecks" section motivating D-13/D-14.
- `include/quiver/binary/binary_file.h` — current `BinaryFile` API surface.
- `include/quiver/binary/binary_metadata.h` — current `BinaryMetadata` struct.
- `include/quiver/binary/dimension.h` — `Dimension` struct (name, size, optional TimeProperties).
- `include/quiver/binary/time_properties.h` — `TimeProperties` struct, `TimeFrequency` enum.
- `src/binary/binary_file.cpp` — write registry implementation (lines 17-44), `next_dimensions` body (lines 245-276), `dimension_sizes_at_values` (lines 278-348), `calculate_file_position` slow path (lines 144-158), `validate_dimension_values` slow path (lines 184-236).
- `src/binary/binary_metadata.cpp` — `BinaryMetadata::validate()` (lines 367-419), `validate_time_dimension_metadata()` (lines 421-458), `validate_time_dimension_sizes()` (lines 460-556), `from_element` (lines 106-206).
- `tests/test_binary_file.cpp` — `BinaryTempFileFixture` pattern (lines 17-52), test naming convention (`TEST_F(Fixture, CamelCaseName)`), programmatic metadata helpers `make_simple_metadata()` / `make_time_metadata()`.
- `src/CMakeLists.txt` — `QUIVER_SOURCES` list (lines 2-28), shared/static target wiring.
- `tests/CMakeLists.txt` — `quiver_tests` source list (lines 3-25), `gtest_discover_tests` registration.
- `src/lua_runner.cpp` — sol2 usertype binding pattern (lines 14-127), reference for Phase 7 sol2 binding of `Expression` (no Phase 1 work).
- `include/quiver/element.h` — `Element` builder used by `BinaryMetadata::from_element`.
- `include/quiver/export.h` — `QUIVER_API` export macro definition.
- `.planning/config.json` — `nyquist_validation: true`, `security_enforcement: true`, `security_asvs_level: 1`.

### Secondary (MEDIUM confidence)
None — all claims grounded in repo files.

### Tertiary (LOW confidence)
None.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| (none) | All claims in this research were verified against in-repo code, planning artifacts, or locked decisions in CONTEXT.md. | — | — |

**Empty assumptions table:** This phase uses only the existing C++20 stack and existing `BinaryFile`/`BinaryMetadata` APIs. No external libraries to research; no version drift concerns; every line of architectural guidance is grounded in either CONTEXT.md (locked decisions) or in-repo source (verified file:line references).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — uses only existing project deps (SQLite, spdlog, toml++, etc. — all already linked into `quiver`). No new deps.
- Architecture: HIGH — DESIGN.md provides the sketch, D-01..D-14 lock the open questions, every architectural choice maps to a verified in-repo reference.
- Pitfalls: HIGH — drawn from concrete file reading (write registry, validate_dimension_values, time-dim reset adjustment) and existing project documentation (`CONCERNS.md`, `CLAUDE.md` performance section).

**Research date:** 2026-05-06
**Valid until:** 2026-06-06 (30 days — codebase is stable; D-01..D-14 are locked; no fast-moving external dependencies)

## RESEARCH COMPLETE
