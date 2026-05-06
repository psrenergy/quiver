---
phase: 02-tech-debt
reviewed: 2026-05-06T22:00:00Z
depth: standard
files_reviewed: 8
files_reviewed_list:
  - src/expr/nodes.cpp
  - include/quiver/expr/node.h
  - src/binary/binary_file.cpp
  - include/quiver/binary/binary_file.h
  - src/binary/iteration.cpp
  - src/binary/csv_converter.cpp
  - tests/test_expression.cpp
  - tests/test_binary_file.cpp
findings:
  critical: 0
  warning: 5
  info: 3
  total: 8
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-05-06T22:00:00Z
**Depth:** standard
**Files Reviewed:** 8
**Status:** issues_found

## Summary

Phase 2 closed 12 audit findings (CR-01..03 + WR-01..09) targeting BinaryOpNode validation symmetry, BinaryFile registry lifecycle, dead-code removal, dedup, and hot-path performance. The diff is well-scoped: each fix is small, tested, and aligned with the project's three-pattern error rule.

The headline fixes hold up under inspection:

- **CR-01 / WR-01 (symmetric mirror + parent-by-name compatibility):** The new `parent_name_of` lambda + symmetric `l_time != r_time` branch fix the originally asymmetric check. Tests `MirrorTimeNonTimeMismatchAThrows`, `MirrorTimeNonTimeMismatchBThrows`, `ParentDimMatchByNameAcceptsCrossPosition`, and `ParentDimNameMismatchThrows` cover the four corners.
- **CR-02 (late-insert registry):** `write_registry.insert(canonical)` is correctly deferred to AFTER `to_toml()` and `fill_file_with_nulls()` succeed. Throw paths leave `registered_path` empty so `~Impl()` is a no-op for the registry.
- **CR-03 ("Cannot apply:" verb):** All seven throws in `BinaryOpNode` ctor + the new `apply_op` canary use the unified verb.
- **WR-02..04 (dead-code removal):** No remaining callers of `validate_file_is_open`, the protected `next_dimensions` delegate, or the protected `dimension_sizes_at_values` member exist anywhere in `src/`, `include/`, or `bindings/`.
- **WR-05 (map-to-indexed delegation):** `validate_dimension_values` correctly preserves the count check (with original message) and "Missing required dimension" error before delegating to the indexed version.
- **WR-06 (reverse translation tables):** Pre-computed `lhs_to_out_` / `rhs_to_out_` correctly invert the forward tables; the Pass-1/Pass-2 invariant guarantees no `-1` entries.
- **WR-07 (apply_op canary):** Switch fall-through now throws instead of returning 0.0.
- **WR-08 (implicit conversion test):** `ImplicitConversionInOperatorArgument` verifies the load-bearing `Expression e = a + b;` line.
- **WR-09 (`bool incremented`):** End-of-iteration detection no longer rebuilds `first_dimensions(meta)` per call.

The remaining issues below are not regressions introduced by Phase 2; they are residual concerns surfaced during the review of the changed files.

## Warnings

### WR-01: Dead unused member `FileNode::own_dims_buf_` declared in public header

**File:** `include/quiver/expr/node.h:60`
**Issue:** `FileNode` declares `mutable std::vector<int64_t> own_dims_buf_;` as a private member, but it has zero references anywhere in the codebase. `FileNode::compute_row` in `src/expr/nodes.cpp:25-38` passes the parameter `dims` directly to `file_->read_into(dims, out, ...)` and never touches `own_dims_buf_`. A `grep` across `src/`, `include/`, `bindings/` returns the declaration as the only hit. This is dead code that:
1. Bloats every `FileNode` instance with an extra `vector<int64_t>` (24 bytes on 64-bit).
2. Suggests a translation step that doesn't exist, misleading future readers.
3. Was explicitly described as part of the CORE-06 buffer-reuse design (Phase 01 SUMMARY/RESEARCH/PLAN), so its absence in `compute_row` may indicate either incomplete implementation or out-of-date documentation.

Phase 2 was the audit-cleanup phase; if `own_dims_buf_` is genuinely unused, it qualifies as exactly the kind of dead-code that WR-02..04 swept. It was missed.

**Fix:** Delete the declaration:
```cpp
// include/quiver/expr/node.h:60 — remove this line
mutable std::vector<int64_t> own_dims_buf_;
```

### WR-02: `parent_name_of` returns empty string sentinel that can match a real dimension name

**File:** `src/expr/nodes.cpp:138-140`
**Issue:** The `parent_name_of` lambda returns `std::string{}` (empty string) when `parent_idx < 0` to represent "no parent":
```cpp
auto parent_name_of = [](int64_t parent_idx, const BinaryMetadata& m) -> std::string {
    return (parent_idx >= 0) ? m.dimensions[parent_idx].name : std::string{};
};
```
Then on line 165, `l_parent != r_parent` is used to compare. If a user constructs a `BinaryMetadata` whose dimension has an empty `name` string (`""`), and the other side has `parent_idx == -1` (root), the equality check would return `true` (false match). `BinaryMetadata::validate()` does not forbid empty dimension names — it only checks uniqueness — so this is reachable through `from_toml()` with hand-crafted TOML or directly assigning to fields.

The fix used in the rest of the codebase is to use `std::optional<std::string>` or compare a separate `has_parent` boolean alongside the name.

**Fix:** Use a sentinel that cannot collide with any dimension name (or carry the `parent_idx >= 0` flag separately):
```cpp
auto parent_name_of = [](int64_t parent_idx, const BinaryMetadata& m) -> std::optional<std::string> {
    return (parent_idx >= 0) ? std::optional<std::string>(m.dimensions[parent_idx].name) : std::nullopt;
};
// ...
const auto l_parent = parent_name_of(lp.parent_dimension_index, lhs_meta);
const auto r_parent = parent_name_of(rp.parent_dimension_index, rhs_meta);
if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value || l_parent != r_parent) { ... }
```

### WR-03: `parent_name_of` indexes `m.dimensions[parent_idx]` without bounds check

**File:** `src/expr/nodes.cpp:138-140`
**Issue:** When `parent_idx >= 0`, the lambda dereferences `m.dimensions[parent_idx]` directly. There is no bounds check that `parent_idx < m.dimensions.size()`. If a metadata with an out-of-range `parent_dimension_index` (positive but `>= dimensions.size()`) reaches this code, `operator[]` is undefined behavior. The same UB applies to `validate_time_dimension_sizes` in `binary_metadata.cpp:467`, but since this lambda runs in the BinaryOpNode constructor — a public API surface — the metadata may not have been validated before reaching here when one operand is constructed from a `ScalarNode` adopting a sibling's metadata.

Practically reachable only through hand-crafted invalid metadata, but the project's "clean code over defensive code" stance assumes callers obey contracts; in this case, the lambda is called against the OUTPUT of `metadata()` which is supposed to be invariant-correct. Phase 1 did not add a parent-index bounds check to `validate_time_dimension_metadata()`. Worth either adding the validation or documenting the precondition in `parent_name_of`.

**Fix:** Either add validation in `BinaryMetadata::validate_time_dimension_metadata()`:
```cpp
if (dim.is_time_dimension() && dim.time->parent_dimension_index >= 0 &&
    static_cast<size_t>(dim.time->parent_dimension_index) >= dimensions.size()) {
    throw std::runtime_error("Time dimension '" + dim.name + "' parent_dimension_index out of bounds");
}
```
or replace `operator[]` with `at()` in `parent_name_of` so the failure surfaces as `std::out_of_range`.

### WR-04: Pre-existing TOML truncation on validation failure (now exposed by new test path)

**File:** `src/binary/binary_file.cpp:91-93`
**Issue:** Constructing `std::ofstream toml_file(file_path + ".toml")` on line 92 opens the file with `std::ios::out` mode by default, which TRUNCATES any existing TOML file at that path. If `metadata->to_toml()` on line 93 throws (which is exactly what the new `OpenFileWriteFailureDoesNotLeakRegistry` test triggers via invalid version), the truncation has already happened. The previously-valid TOML on disk is gone.

The CR-02 fix correctly addresses **registry leak**, but the comment on line 102-103 ("Throw paths above leave both empty so the destructor is a no-op for the registry on those paths") oversells the property — file system state is NOT preserved across throw paths. This is pre-existing, but Phase 2's late-insert restructuring made it a more visible concern: the new test `OpenFileWriteFailureDoesNotLeakRegistry` succeeds only because TearDown removes the path between subtests, so the truncation effect is invisible. A real-world user opening an existing-and-valid `.qvr/.toml` pair for write with bad metadata loses the original metadata file.

**Fix:** Move the `to_toml()` call to before the file is opened:
```cpp
// Validate + serialize FIRST (these can throw) — produces just a string in memory.
std::string toml_content = metadata->to_toml();

// Only after success do we touch the filesystem.
std::ofstream toml_file(file_path + std::string(TOML_EXTENSION));
toml_file << toml_content;

auto io = std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::out | std::ios::binary);
BinaryFile binary_file(file_path, metadata.value(), std::move(io));
binary_file.fill_file_with_nulls();

write_registry.insert(canonical);
binary_file.impl_->registered_path = canonical;
return binary_file;
```

### WR-05: `write_registry` is not thread-safe; CLAUDE.md asserts this but the late-insert change documents only single-process safety

**File:** `src/binary/binary_file.cpp:19, 59-61, 104`
**Issue:** `write_registry` is a translation-unit-local `std::unordered_set<std::string>` mutated from `open_file()` and `~Impl()` without synchronization. `CLAUDE.md` documents this with "Not thread-safe or multi-process safe." However, the new comments at lines 87-89 and 101-103 describe the late-insert as a fix to a deterministic behavior and don't reiterate the thread-safety constraint. With the insert moved AFTER `fill_file_with_nulls()` (which can hit disk I/O in a chunk loop), the window between the prologue check at line 59 and the insert at line 104 is now substantially larger — a concurrent caller could pass the prologue check (registry empty), proceed to also call `fill_file_with_nulls()`, and one of them would silently corrupt the other's data.

This is a pre-existing concern but the late-insert widens the TOCTOU window. If the project policy is single-threaded use only, the comment block at line 87-89 should explicitly say so; otherwise, a `std::mutex` guarding `write_registry` access is needed.

**Fix:** Add an explicit thread-safety note in the late-insert comment OR (better) introduce a single mutex:
```cpp
static std::mutex write_registry_mutex;
static std::unordered_set<std::string> write_registry;
// ...
{
    std::lock_guard<std::mutex> lock(write_registry_mutex);
    if (write_registry.count(canonical)) {
        throw std::runtime_error("Cannot open_file: file is already open for writing: " + canonical);
    }
}
// ... do throwable work ...
{
    std::lock_guard<std::mutex> lock(write_registry_mutex);
    write_registry.insert(canonical);
    binary_file.impl_->registered_path = canonical;
}
```
And update `~Impl()` accordingly.

## Info

### IN-01: SUMMARY claims "Throw paths leave the registry empty" but the prologue check still rejects concurrent writers — comment is slightly misleading

**File:** `src/binary/binary_file.cpp:87-89, 101-103`
**Issue:** The two comment blocks talk about "leave both empty so the destructor is a no-op." This is correct for `registered_path`, but the registry is not "left empty" if a concurrent caller already inserted on a different path. The phrasing reads as if Phase 2 introduced the throw-path no-leak property, when actually it just fixed the throw-path-leak that the original `write_registry.insert(canonical)` BEFORE `to_toml()` introduced. Minor wording.

**Fix:** Tighten the comment:
```cpp
// All risky operations succeeded -- now register. If any step above threw, the
// registry never received `canonical` and the BinaryFile object never reached
// `registered_path = canonical`, so ~Impl() correctly skips erase.
```

### IN-02: `ParentDimNameMismatchThrows` test comment overlooks that pre-row write-time validation also runs

**File:** `tests/test_expression.cpp:530-558`
**Issue:** The comment claims "Both metadata use monthly+daily so the per-row write-time validation succeeds — only the cross-operand parent-name check distinguishes them." This is true for `from_element` construction (which infers parent_dimension_index=0 for both), but `parent_name_of(lp.parent_dimension_index=0, lhs_meta)` returns `"month"` while `parent_name_of(rp.parent_dimension_index=0, rhs_meta)` returns `"stage"`. The test is correct but the comment leaves it implicit which side's resolution returns which name. Future maintainers debugging this test would benefit from a one-line callout that the parent for `block` resolves to `"month"` (lhs) vs `"stage"` (rhs) AT THE NAME LEVEL.

**Fix:** Add a clarifying comment:
```cpp
// lhs: dimensions[0]="month", so parent_name_of(0, lhs_meta) == "month"
// rhs: dimensions[0]="stage", so parent_name_of(0, rhs_meta) == "stage"
// Cross-operand: "month" != "stage" -> throw on D-17.
```

### IN-03: `OpenFileWriteFailureDoesNotLeakRegistry` does not verify path state on disk

**File:** `tests/test_binary_file.cpp:473-493`
**Issue:** The test verifies the registry is clean (a successful second `open_file('w', ...)` call), but it does not check that the second call's TOML/QVR content is correct or that no leftover empty file from the failed first call survives. Combined with WR-04 (pre-existing TOML truncation), the second call DOES re-truncate the empty TOML file before writing fresh content, so the test happens to pass. A defensive assertion would catch the WR-04 bug if it were ever fixed wrongly:
```cpp
EXPECT_NO_THROW({
    auto writer = BinaryFile::open_file(path, 'w', good_md);
    writer.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
});
// Reopen and verify metadata content survived.
auto reopened = BinaryFile::open_file(path, 'r');
EXPECT_EQ(reopened.get_metadata().version, "1");
EXPECT_EQ(reopened.get_metadata().labels.size(), 2u);
```

**Fix:** Add post-write metadata assertion (as shown above).

---

_Reviewed: 2026-05-06T22:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
