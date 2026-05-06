---
phase: 01-c-core
reviewed: 2026-05-06T00:00:00Z
depth: standard
files_reviewed: 13
files_reviewed_list:
  - include/quiver/binary/binary_file.h
  - include/quiver/binary/iteration.h
  - include/quiver/expr/expression.h
  - include/quiver/expr/node.h
  - src/CMakeLists.txt
  - src/binary/binary_file.cpp
  - src/binary/iteration.cpp
  - src/expr/expression.cpp
  - src/expr/nodes.cpp
  - tests/CMakeLists.txt
  - tests/test_binary_file.cpp
  - tests/test_expression.cpp
  - tests/test_iteration.cpp
findings:
  blocker: 3
  warning: 9
  total: 12
status: issues_found
---

# Phase 01 (c-core): Code Review Report

**Reviewed:** 2026-05-06
**Depth:** standard
**Files Reviewed:** 13
**Status:** issues_found

## Summary

The phase delivers two new subsystems: `quiver::binary` iteration helpers + fast `BinaryFile` overloads, and `quiver::expr` (Expression + Node). The general structure is sound — Pimpl/Rule-of-Zero choices match CLAUDE.md, error messages mostly follow the three canonical patterns, and the D-11 self-save check is implemented before the writer is opened (verified by `SelfSaveCollisionThrows`).

However, three correctness bugs need to ship-block:

1. **D-04 mismatch gap** — when lhs is non-time and rhs is time on the same dim name, validation silently passes and the time semantics of rhs are dropped.
2. **Write-registry leak on construction failure** — if `to_toml`, the binary fstream constructor, or `fill_file_with_nulls` throws after registry insert but before `registered_path` is set on the live BinaryFile, the canonical path stays in `write_registry` for the rest of the process lifetime.
3. **Error-message pattern violations** — multiple new `throw std::runtime_error(...)` sites use ad-hoc messages that do not match any of CLAUDE.md's three required patterns (e.g. `"Cannot apply binary operation: …"`).

In addition, one piece of dead code (`validate_file_is_open`), one large block of duplicated code (`dimension_sizes_at_values` copy-pasted between `binary_file.cpp` and `iteration.cpp`), and an over-restrictive D-04 check on `parent_dimension_index` (compared by raw operand index rather than by parent name) all degrade quality and should be addressed before this code accrues consumers.

## Critical Issues

### CR-01 (BLOCKER): D-04 only fires when the time dim is on lhs — silently demotes time→non-time when only rhs has time props

**File:** `src/expr/nodes.cpp:132-151`

**Issue:** The D-04 validation block iterates `lhs_meta.dimensions`, skips non-time dims at line 134, and only triggers the "is a time dimension on lhs but not on rhs" branch at line 140. The mirror case — lhs has dim X as a *non-time* dim and rhs has dim X as a *time* dim — is never caught. In that case the loop on line 133 skips X (because `!l_dim.is_time_dimension()`); the shape check at line 117 only compares sizes; Pass 1 at line 200 then writes the lhs version (`l_dim.time` = `nullopt`) into the broadcast metadata, silently discarding rhs's time properties. Tests do not catch this — `TimePropertiesMismatchThrows` only exercises the lhs-time, rhs-non-time direction (the one line 140 already handles).

The downstream effects are concrete: the resulting expression reports a non-time dimension to `Expression::save`, the iteration will not honor calendar-aware sizing, and the file produced will silently be wrong on the time axis.

**Fix:** Add a symmetric loop over rhs to catch the inverse mismatch (or restructure as a single loop over the union of names). Minimum change:

```cpp
// after the existing lhs-driven loop at line 151:
for (const auto& r_dim : rhs_meta.dimensions) {
    if (!r_dim.is_time_dimension())
        continue;
    int l_idx = find_dim_index(lhs_meta.dimensions, r_dim.name);
    if (l_idx < 0)
        continue;
    if (!lhs_meta.dimensions[l_idx].is_time_dimension()) {
        throw std::runtime_error("Cannot apply binary operation: dimension '" + r_dim.name +
                                 "' is a time dimension on rhs but not on lhs");
    }
    // (TimeProperties equality already checked from the lhs-driven loop when both are time dims.)
}
```

Add a regression test that mirrors `TimePropertiesMismatchThrows` with the time-ness on rhs instead of lhs.

---

### CR-02 (BLOCKER): `open_file('w', ...)` can leak entries into `write_registry` on partial-construction failure

**File:** `src/binary/binary_file.cpp:81-100`

**Issue:** The write-mode branch inserts `canonical` into `write_registry` at line 87, *before* any of the operations that can throw:
- `metadata->to_toml()` at line 91 calls `validate()` (`binary_metadata.cpp:367`) which throws on any structural issue (mismatched dim sizes, duplicate names, bad time-dim sizes, etc.).
- The `std::ofstream toml_file(...)` line 90 / `<<` line 91 may throw / fail silently on disk-full or permission errors.
- The `std::make_unique<std::fstream>(...)` line 95 may throw `std::bad_alloc`.
- `fill_file_with_nulls()` at line 98 may throw on write failure.

`registered_path` is only set on the BinaryFile's `Impl` at line 97, *after* the BinaryFile is constructed. If anything between line 87 and line 97 throws, no live BinaryFile owns the registered path, and `~Impl()` is never invoked with a non-empty `registered_path`. The entry stays in the static `write_registry` for the rest of the process — every subsequent attempt to open this path (read or write) raises `"Cannot open_file: file is already open for writing: …"`.

The most plausible trigger is a metadata bug surfaced by `validate()` inside `to_toml()`: a user calls `open_file(path, 'w', bad_md)`, gets a runtime_error, fixes their metadata, and now cannot reopen the path at all.

**Fix:** Use RAII or try/catch to guarantee unregistration on failure. One robust approach:

```cpp
case 'w': {
    if (!metadata.has_value()) {
        throw std::invalid_argument("Metadata must be provided when opening a file in write mode.");
    }

    // RAII guard removes the entry unless committed.
    write_registry.insert(canonical);
    bool committed = false;
    auto guard = std::unique_ptr<std::string, std::function<void(std::string*)>>(
        &canonical, [&committed](std::string* p) {
            if (!committed) write_registry.erase(*p);
        });

    std::ofstream toml_file(file_path + std::string(TOML_EXTENSION));
    toml_file << metadata->to_toml();

    auto io = std::make_unique<std::fstream>(
        file_path + std::string(QVR_EXTENSION), std::ios::out | std::ios::binary);
    BinaryFile binary_file(file_path, metadata.value(), std::move(io));
    binary_file.impl_->registered_path = canonical;  // ownership transfers here
    committed = true;                                 // disarm guard
    binary_file.fill_file_with_nulls();
    return binary_file;
}
```

Or simpler: set `binary_file.impl_->registered_path = canonical` *immediately* after construction and BEFORE `fill_file_with_nulls`, then wrap the entire success path in a try/catch that erases on failure. Add a regression test that supplies metadata which fails `validate()` (e.g., empty `labels`), confirms the throw, then verifies `open_file(path, 'w', good_md)` succeeds afterwards.

---

### CR-03 (BLOCKER): New `runtime_error` messages violate the three required CLAUDE.md patterns

**File:** `src/expr/nodes.cpp:110, 127-129, 141-142, 148-149, 157, 168-169, 177-178`; `src/expr/expression.cpp:68`; `src/binary/binary_file.cpp:60`

**Issue:** CLAUDE.md mandates exactly three formats for `std::runtime_error`:
1. `"Cannot {operation}: {reason}"`
2. `"{Entity} not found: {identifier}"`
3. `"Failed to {operation}: {reason}"`

The new code introduces messages that do not fit any of these. Examples from the diff:

| File:Line | Current message | Issue |
|-----------|-----------------|-------|
| `nodes.cpp:110` | `"Cannot apply binary operation: units differ ('…' vs '…')"` | `apply binary operation` is not the C++ method name. Closest pattern would be `"Cannot apply_op: …"` or rephrase as `"Failed to compose Expression: units differ …"`. |
| `nodes.cpp:127-129` | `"Cannot apply binary operation: dimension '…' has incompatible sizes …"` | Same issue. |
| `nodes.cpp:141-142, 148-149` | `"Cannot apply binary operation: dimension '…' is a time dimension on lhs but not on rhs"` | Same. |
| `nodes.cpp:157` | `"Cannot apply binary operation: initial_datetime differs"` | Same. |
| `nodes.cpp:168-169, 177-178` | `"Cannot apply binary operation: labels have …"` | Same. |
| `expression.cpp:68` | `"Cannot save: output path collides with input file '…'"` | `save` is the C++ method name — this one is actually compliant. Keep as exemplar. |
| `binary_file.cpp:60` | `"Cannot open_file: file is already open for writing: …"` | Compliant — `open_file` is the C++ method. |

**Fix:** Settle on one method name and apply pattern 1 uniformly. Recommend: this is the constructor of `BinaryOpNode` invoked from operator overloads, so the user-visible operation is *constructing the Expression*. Use `"Cannot construct Expression: {reason}"` or `"Failed to combine Expressions: {reason}"`. Whichever you pick, apply it consistently across all six message sites in `BinaryOpNode::BinaryOpNode`.

This is BLOCKER because bindings (Phase 2+) will surface these strings unchanged, and once they are observed by users they become a backwards-compatibility liability per the project's "messages live in the C++ core; bindings surface them unchanged" rule.

---

## Warnings

### WR-01: D-04 over-restrictive — `parent_dimension_index` compared by raw operand index, not by parent name

**File:** `src/expr/nodes.cpp:146-147`

**Issue:** The check
```cpp
if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value ||
    lp.parent_dimension_index != rp.parent_dimension_index) {
```
treats `parent_dimension_index` as a value to be compared between operands. But this index is a *position into the OPERAND's own `dimensions` array*, not a stable semantic identifier. Two operands that have semantically identical time dims (e.g., `block` daily inside `month` monthly) but list non-time dims in different positions will have different `parent_dimension_index` values and be rejected.

Example: lhs = `[month(monthly), block(daily, parent=0)]`, rhs = `[month(monthly), scenario(non-time), block(daily, parent=0)]`. Both list `month` at index 0, so `parent_dimension_index = 0` matches. But rhs = `[scenario, month(monthly), block(daily, parent=1)]` (scenario before month) would be rejected even though the parent is *named* the same and the time dim is structurally identical from the data's perspective.

**Fix:** Compare parent dims by NAME, not by index:

```cpp
const auto& l_parent_name = (lp.parent_dimension_index >= 0)
    ? lhs_meta.dimensions[lp.parent_dimension_index].name : std::string{};
const auto& r_parent_name = (rp.parent_dimension_index >= 0)
    ? rhs_meta.dimensions[rp.parent_dimension_index].name : std::string{};
if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value || l_parent_name != r_parent_name) {
    throw std::runtime_error(...);
}
```

Add a regression test that combines two metadatas with the same time-dim semantics but different non-time dim positions.

---

### WR-02: `validate_file_is_open()` is dead code — declared and defined, never called

**File:** `include/quiver/binary/binary_file.h:67`, `src/binary/binary_file.cpp:263-267`

**Issue:** The method is declared in the header (private) and defined in the .cpp, but `Grep` confirms zero call sites in the entire codebase. CLAUDE.md is explicit: "Delete unused code, do not deprecate."

The function would also be unsound if used as-is: `impl_->io->good()` returns `false` after a normal EOF on a reader that has consumed the whole file, so calling `validate_file_is_open()` after the last `read` would falsely throw.

**Fix:** Delete the declaration and definition. If the intent was defensive checks before each read/write, that decision contradicts CLAUDE.md's "clean code over defensive code" principle and the existing API's contract that the caller cannot construct a BinaryFile without an open stream.

---

### WR-03: `dimension_sizes_at_values` is duplicated verbatim between `binary_file.cpp` and `iteration.cpp`

**File:** `src/binary/binary_file.cpp:394-464` vs `src/binary/iteration.cpp:21-91`

**Issue:** The 70-line function is copy-pasted between the two translation units. The comment at `iteration.cpp:17-20` admits as much: *"Lifted from src/binary/binary_file.cpp::dimension_sizes_at_values… The only substitution: the original impl reads `impl_->metadata.dimensions`; this version takes `const BinaryMetadata& metadata` directly."*

Two copies will inevitably diverge under future calendar-rule changes (e.g., adding a new `TimeFrequency` value).

**Fix:** Move the function to a shared internal header (e.g., `src/binary/binary_utils.h` already exists; add a `quiver::binary::detail::dimension_sizes_at_values(const BinaryMetadata&, const std::vector<int64_t>&)`). Make `BinaryFile::dimension_sizes_at_values` a thin wrapper that calls the free function with `impl_->metadata`. Remove the protected member if it is no longer needed (`Grep` shows `BinaryFile::dimension_sizes_at_values` itself is also unused, so this is also dead code).

---

### WR-04: `BinaryFile::dimension_sizes_at_values` and `BinaryFile::next_dimensions` (protected) appear unused

**File:** `include/quiver/binary/binary_file.h:76-77`, `src/binary/binary_file.cpp:381-464`

**Issue:** These are `protected:` methods presumably to be inherited by `CSVConverter`, but `csv_converter.cpp:85,130` calls `csv_writer.next_dimensions(...)` which now delegates to `quiver::binary::next_dimensions`. The protected wrapper exists as a thin pass-through. `dimension_sizes_at_values` (member) is not called from anywhere either. After WR-03, both members can be deleted entirely.

**Fix:** After consolidating `dimension_sizes_at_values` per WR-03, also remove `BinaryFile::next_dimensions` and update `CSVConverter` to call `quiver::binary::next_dimensions` directly. The protected `get_io()` method may be the only protected hook actually still required.

---

### WR-05: `validate_dimension_values` and `validate_dimension_values_indexed` duplicate ~50 lines

**File:** `src/binary/binary_file.cpp:269-321` vs `323-372`

**Issue:** Both functions perform the same logical sequence (count check, bounds check, time-dim consistency check) but operate on different argument types. The bodies are nearly identical except for the dim-name lookup vs positional access. Calendar-aware logic (lines 293-320 and 343-371) is duplicated verbatim.

**Fix:** Extract the time-consistency block into a helper that accepts a `std::function<int64_t(size_t)>` or a span, then call from both validators. This will keep the two surface APIs but de-duplicate the calendar logic.

---

### WR-06: `compute_row` does an O(n²) inner search for the output index of every operand dim, every row

**File:** `src/expr/nodes.cpp:283-310`

**Issue:** The inner loop:
```cpp
for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
    int out_i = -1;
    for (size_t k = 0; k < lhs_dim_translate_.size(); ++k) {
        if (lhs_dim_translate_[k] == static_cast<int>(li)) { out_i = static_cast<int>(k); break; }
    }
    ...
}
```
runs every row. The translation tables `lhs_dim_translate_` / `rhs_dim_translate_` already store the inverse mapping (output→operand). The constructor could pre-compute the *forward* mapping (operand→output) once, leaving `compute_row` as a single pass.

The plan claims pre-computed translation tables specifically to avoid per-row search; the current implementation only pre-computes one direction. For the `LargeGridCompletes` test (50×20×4 = 4000 rows, ~3 dims) the nested search is roughly 18000 extra integer comparisons — measurable but not fatal. For the binary-subsystem hot path (480×500×31 ≈ 7.4M rows), it would compound noticeably.

**Fix:** Add a member `std::vector<int> lhs_inverse_translate_;` populated in the constructor (`lhs_inverse_translate_[li] = out_i`) and use it directly in `compute_row`:

```cpp
for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
    const int out_i = lhs_inverse_translate_[li];
    int64_t coord = dims[out_i];
    if (lhs_dim_sizes_[out_i] == 1) coord = 1;
    lhs_dims_buf_[li] = coord;
}
```

Same for rhs.

(Performance is technically out of v1 scope per the review brief, but the prompt's reminder "Hot path performance and the D-11 self-save check are both in scope" elevates this to a quality concern worth flagging.)

---

### WR-07: `apply_op` returns `0.0` after an exhaustive switch — silences a warning by hiding a logic bug

**File:** `src/expr/nodes.cpp:82-94`

**Issue:**
```cpp
double apply_op(BinaryOp op, double a, double b) {
    switch (op) {
    case BinaryOp::Add:      return a + b;
    case BinaryOp::Subtract: return a - b;
    case BinaryOp::Multiply: return a * b;
    case BinaryOp::Divide:   return a / b;
    }
    return 0.0;  // unreachable; silences MSVC missing-return warning
}
```
The `return 0.0` will produce a *silent wrong answer* if a future `BinaryOp` variant is added but `apply_op` is not updated — the compiler can no longer warn because the fall-through is reachable. Better: throw, abort, or use `[[unreachable]]`.

**Fix:** Replace with `__builtin_unreachable();` (GCC/Clang) / `__assume(false);` (MSVC) wrapped in a portable macro, or simply `throw std::logic_error("Unhandled BinaryOp variant")`. C++23 has `std::unreachable()` — but the project is C++20, so use a manual abort:

```cpp
default:
    throw std::logic_error("Unhandled BinaryOp variant");
```

Add `[[gnu::cold]]` if you want to keep the perf characteristic.

---

### WR-08: `Expression(const BinaryFile& file)` is implicit-by-design but lacks a unit test exercising the conversion in a function call argument

**File:** `include/quiver/expr/expression.h:23` and tests

**Issue:** The header comment at `expression.h:19-23` makes it explicit that `Expression(const BinaryFile&)` is *not* declared `explicit` so users can write `Expression e = a;`. Tests confirm this case. But no test exercises the more interesting implicit-conversion path: passing a `BinaryFile` directly to a function expecting `const Expression&` — which is the actual reason for declining `explicit`. A future reviewer who sees the comment "NOT explicit by design" without a test asserting that property could mistakenly add `explicit` and break the contract.

**Fix:** Add a single test that takes a `BinaryFile` and passes it through implicit conversion in an operator overload, e.g.:

```cpp
TEST_F(ExpressionFixture, ImplicitConversionInOperator) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = a + 2.0;        // implicit conversion of `a` in the lhs of operator+
    e.save(path_out);
}
```

Without `explicit` removed, this expression must compile.

---

### WR-09: `next_dimensions` rebuilds `first_dimensions(meta)` on every call to detect end-of-iteration

**File:** `src/binary/iteration.cpp:104-140`

**Issue:** Lines 136-138:
```cpp
if (next == first_dimensions(meta)) {
    return std::nullopt;
}
```
allocates a fresh vector and re-runs the time-dim initial-value computation each call to compare. For a 7.3M-cell traversal this is 7.3M wasted vector allocations.

**Fix:** Detect the wrap by checking whether the increment loop wrapped past the outermost (time-aware) initial_value. The increment loop already knows when every digit reset to its initial — when the OUTERMOST dim resets, iteration is complete. Track that explicitly with a boolean flag inside the for loop:

```cpp
bool wrapped = true;
for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i) {
    if (next[i] < current_sizes[i]) {
        next[i] += 1;
        wrapped = false;
        break;
    } else {
        next[i] = 1;
    }
}
if (wrapped) return std::nullopt;
```

This eliminates the vector comparison and `first_dimensions` recomputation. Also more robust: it correctly signals end-of-iteration without the equality-compare against potentially stale `first_dimensions(meta)`.

(Same note as WR-06 about performance scope — but `next_dimensions` is in the `Expression::save` row loop, which the prompt explicitly flags as in-scope.)

---

## Notes on items NOT flagged

- **`Dimension d{l_dim.name, out_size, l_dim.time}` (nodes.cpp:200)** — verified safe: in the `r_idx >= 0` overlap case, D-04 has already enforced that lhs and rhs agree on time-ness (modulo the CR-01 gap), so copying lhs's `time` is well-defined. After CR-01 is fixed, this remains correct.
- **`go_to_position` cross-mode safety (binary_file.cpp:245-261)** — verified safe: a BinaryFile is opened in either `ios::in | ios::binary` or `ios::out | ios::binary`, never both, so the read/write pointer aliasing concern doesn't apply.
- **`fill_file_with_nulls` truncation behavior (binary_file.cpp:466-493)** — verified safe: `std::ios::out | std::ios::binary` (without `ate` or `app`) truncates per the C++ standard.
- **Move semantics on `BinaryFile`** — verified safe: `unique_ptr<Impl>::operator=(unique_ptr<Impl>&&)` calls `~Impl()` on the previous Impl, which correctly unregisters and flushes. Test `MoveAssignWriterUnregistersOldPath` covers this.
- **`compute_row` size-1 broadcast clamp** — verified correct.
- **`number_of_time_dimensions` recount in BinaryOpNode constructor (nodes.cpp:237-240)** — verified correct.
- **`writer.write(row, dims)` in `Expression::save`** — uses the `vector<int64_t>` overload, which validates per call but is the only write path that does. The plan accepts this.

---

_Reviewed: 2026-05-06_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
