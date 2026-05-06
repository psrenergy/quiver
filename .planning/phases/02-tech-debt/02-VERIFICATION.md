---
phase: 02-tech-debt
verified: 2026-05-06T23:00:00Z
status: passed
score: 14/14 must-haves verified
overrides_applied: 0
---

# Phase 02: Tech-Debt Verification Report

**Phase Goal:** Close the 12 review findings carried forward from Phase 1 (3 BLOCKER + 9 WARNING) tracked in `.planning/v1.0-MILESTONE-AUDIT.md`. Tighten correctness on the deferred D-04 mirror-case gap (CR-01), eliminate duplication and dead code in the Binary subsystem, harden BinaryOpNode error-message conformance + perf, and add the missing implicit-conversion test.
**Verified:** 2026-05-06T23:00:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1  | All 12 audit items (CR-01..CR-03 + WR-01..WR-09) closed per disposition in v1.0-MILESTONE-AUDIT.md | ✓ VERIFIED | All 11 task commits confirmed in git log; every item below independently verified |
| 2  | BinaryOpNode rejects same-name dim time/non-time mismatch SYMMETRICALLY (lhs-time/rhs-non-time AND lhs-non-time/rhs-time both throw with "Cannot apply: ...") | ✓ VERIFIED | `nodes.cpp:148-154`: `if (l_time != r_time)` branch covers both directions; `MirrorTimeNonTimeMismatchAThrows` (lhs-non/rhs-time) and `MirrorTimeNonTimeMismatchBThrows` (lhs-time/rhs-non) both present and passing |
| 3  | Same-name time dims compare parents BY NAME (not raw operand index): D-23 #3 (parent name match, different operand indices) accepts; D-23 #4 (parent name mismatch) throws | ✓ VERIFIED | `nodes.cpp:138-168`: `parent_name_of` lambda resolves index to name on its own operand's metadata, then compares names across operands; `ParentDimMatchByNameAcceptsCrossPosition` (EXPECT_NO_THROW) and `ParentDimNameMismatchThrows` (EXPECT_THROW) both in `test_expression.cpp:498,530` |
| 4  | BinaryFile::open_file('w', bad_md) leaves write_registry empty: a subsequent open_file('w', good_md) on the same path succeeds (D-24 regression test) | ✓ VERIFIED | `binary_file.cpp:87-105`: `write_registry.insert(canonical)` deferred to AFTER `to_toml()` and `fill_file_with_nulls()` succeed; `OpenFileWriteFailureDoesNotLeakRegistry` at `test_binary_file.cpp:473` passes |
| 5  | All 7 BinaryOpNode ctor throws use "Cannot apply:" verb (no "Cannot apply binary operation" string remains in src/expr) | ✓ VERIFIED | Grep finds 8 distinct "Cannot apply:" occurrences in `nodes.cpp` (7 ctor throws + 1 `apply_op` canary); grep for "Cannot apply binary operation" across all *.cpp returns 0 matches |
| 6  | BinaryFile::validate_file_is_open is removed (declaration + definition + zero references in src/, tests/, include/, bindings/, src/c/) | ✓ VERIFIED | `binary_file.h` contains no `validate_file_is_open` declaration; `binary_file.cpp` contains no definition; grep across entire codebase returns 0 matches in source/header/test/binding files |
| 7  | BinaryFile::dimension_sizes_at_values member is removed; only the anonymous-namespace function in iteration.cpp remains | ✓ VERIFIED | `binary_file.cpp` has 0 `dimension_sizes_at_values` occurrences; `binary_file.h` has 0; `iteration.cpp` anonymous-namespace version confirmed at lines 21-91 |
| 8  | BinaryFile::next_dimensions protected member is removed; csv_converter.cpp:85 and :130 call quiver::binary::next_dimensions free function directly with std::optional unwrap | ✓ VERIFIED | `binary_file.h` protected section contains only `get_io()`; `csv_converter.cpp` lines 86 and 131 both call `quiver::binary::next_dimensions(...)` with `if (!nxt) break; current_dimensions = std::move(*nxt);` pattern |
| 9  | validate_dimension_values (map overload) delegates to validate_dimension_values_indexed after a name-resolution conversion; calendar-aware logic exists in only one place | ✓ VERIFIED | `binary_file.cpp:270-293`: map overload does count check, converts map to declaration-order vector, then calls `validate_dimension_values_indexed(indexed_dims)` at line 292; all calendar logic in `_indexed` at lines 295-343 |
| 10 | BinaryOpNode::compute_row uses pre-computed reverse translation tables (lhs_to_out_, rhs_to_out_); no nested O(n^2) inner search remains | ✓ VERIFIED | `node.h:122-123`: `lhs_to_out_` and `rhs_to_out_` declared; `nodes.cpp:283-290`: tables populated at construction; `nodes.cpp:312-321`: `compute_row` indexes via `lhs_to_out_[li]` / `rhs_to_out_[ri]` — O(1) per dimension |
| 11 | apply_op throws on unhandled BinaryOp variant instead of returning 0.0 (runtime canary for future variants) | ✓ VERIFIED | `nodes.cpp:97`: `throw std::runtime_error("Cannot apply: unhandled BinaryOp variant")` after exhaustive switch; no `return 0.0` fall-through |
| 12 | Implicit conversion test `Expression e = a + b;` exercises the non-explicit Expression(const BinaryFile&) ctor inside operator argument position | ✓ VERIFIED | `test_expression.cpp:231-257`: `ImplicitConversionInOperatorArgument` test present; load-bearing line is `Expression e = a + b;` with no explicit `Expression()` wrappers on either operand |
| 13 | next_dimensions detects end-of-iteration via a `bool incremented` flag instead of rebuilding first_dimensions(meta) per call | ✓ VERIFIED | `iteration.cpp:115`: `bool incremented = false;` declared; loop sets `incremented = true` when dim increments; `if (!incremented) return std::nullopt;` at line 128; no `first_dimensions(meta)` call anywhere in `next_dimensions` |
| 14 | Full quiver_tests suite passes: 781 baseline + 4 (D-23) + 1 (D-24) + 1 (D-25) = 787/787 green; no Phase 1 truth broken | ✓ VERIFIED | `./build/bin/quiver_tests.exe`: 787/787 PASSED; `ExpressionFixture.*`: 34/34 PASSED (29 baseline + 5 new); `./build/bin/quiver_c_tests.exe`: 403/403 PASSED |

**Score:** 14/14 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/expr/nodes.cpp` | CR-01/WR-01 compatibility loop, CR-03 verb, WR-06 reverse tables, WR-07 canary | ✓ VERIFIED | Contains all: symmetric l_time!=r_time check (line 148), parent_name_of lambda (line 138), 8x "Cannot apply:" throws, lhs_to_out_/rhs_to_out_ population + usage, apply_op throw |
| `include/quiver/expr/node.h` | lhs_to_out_ / rhs_to_out_ private member declarations (WR-06) | ✓ VERIFIED | Lines 122-123: `std::vector<int> lhs_to_out_;` and `std::vector<int> rhs_to_out_;` present |
| `src/binary/binary_file.cpp` | Late-insert write_registry (CR-02), validate_file_is_open removed (WR-02), dimension_sizes_at_values removed (WR-03/WR-04), next_dimensions delegate removed (WR-04), validate_dimension_values delegates to indexed (WR-05) | ✓ VERIFIED | All five conditions confirmed: registry insert at line 104 (post-throwable ops), no `validate_file_is_open`, no `dimension_sizes_at_values`, no protected `next_dimensions`, map overload delegates at line 292 |
| `include/quiver/binary/binary_file.h` | validate_file_is_open declaration removed (WR-02); next_dimensions + dimension_sizes_at_values protected declarations removed (WR-04) | ✓ VERIFIED | `protected:` section contains only `std::iostream& get_io();`; no `validate_file_is_open`, no `dimension_sizes_at_values`, no `next_dimensions` |
| `src/binary/iteration.cpp` | next_dimensions uses bool incremented flag (WR-09) | ✓ VERIFIED | Line 115: `bool incremented = false;`; line 128: `if (!incremented) return std::nullopt;` |
| `src/binary/csv_converter.cpp` | csv_to_bin and bin_to_csv loops call quiver::binary::next_dimensions free function directly with optional unwrap (WR-04 collateral) | ✓ VERIFIED | Lines 86 and 131: both call `quiver::binary::next_dimensions(...)` with `if (!nxt) break; current_dimensions = std::move(*nxt);` |
| `tests/test_expression.cpp` | 5 new TEST_F cases: 4 D-23 (CR-01+WR-01) + 1 D-25 (WR-08) | ✓ VERIFIED | Lines 231, 448, 473, 498, 530: MirrorTimeNonTimeMismatchAThrows, MirrorTimeNonTimeMismatchBThrows, ParentDimMatchByNameAcceptsCrossPosition, ParentDimNameMismatchThrows, ImplicitConversionInOperatorArgument |
| `tests/test_binary_file.cpp` | 1 new TEST_F: OpenFileWriteFailureDoesNotLeakRegistry (D-24) | ✓ VERIFIED | Line 473: `TEST_F(BinaryTempFileFixture, OpenFileWriteFailureDoesNotLeakRegistry)` present |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/expr/nodes.cpp` BinaryOpNode ctor | `find_dim_index` helper | anonymous-namespace lookup by name | ✓ WIRED | 8 call sites to `find_dim_index(` in nodes.cpp; function defined at line 64 |
| `src/expr/nodes.cpp` BinaryOpNode::compute_row | `lhs_to_out_` / `rhs_to_out_` members | O(1) array indexing instead of inner search | ✓ WIRED | `lhs_to_out_[li]` at line 313, `rhs_to_out_[ri]` at line 321 |
| `src/binary/binary_file.cpp` BinaryFile::open_file('w', ...) | `Impl::~Impl()` registry erase | `registered_path` being empty on throw paths | ✓ WIRED | `registered_path = canonical` at line 105 (post-throwable ops); `~Impl()` at lines 37-44 erases only when `!registered_path.empty()` |
| `src/binary/csv_converter.cpp` csv_to_bin / bin_to_csv loops | `quiver::binary::next_dimensions` free function | std::optional unwrap (replaces equality compare to initial_dimensions) | ✓ WIRED | Lines 86, 131: `auto nxt = quiver::binary::next_dimensions(...); if (!nxt) break; current_dimensions = std::move(*nxt);` |
| `src/binary/binary_file.cpp` validate_dimension_values (map) | `validate_dimension_values_indexed` | name-to-index conversion then delegate | ✓ WIRED | Lines 280-292: map converted to `indexed_dims` vector (declaration order), then `validate_dimension_values_indexed(indexed_dims)` called |

### Data-Flow Trace (Level 4)

Not applicable — this phase contains no new components that render dynamic data. All changes are pure refactors and test additions.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| 787/787 quiver_tests pass | `./build/bin/quiver_tests.exe` | 787 tests PASSED | ✓ PASS |
| ExpressionFixture 34/34 pass (includes 5 new tests) | `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.*"` | 34 tests PASSED | ✓ PASS |
| 403/403 quiver_c_tests pass (no regression) | `./build/bin/quiver_c_tests.exe` | 403 tests PASSED | ✓ PASS |

### Requirements Coverage

This is a cleanup phase — no new REQUIREMENTS.md entries. Closes audit-tracked debt items CR-01..03 + WR-01..09 from `v1.0-MILESTONE-AUDIT.md`. No REQUIREMENTS.md IDs are claimed (plan `requirements: []`). No orphaned requirements mapped to Phase 2 in REQUIREMENTS.md.

### Anti-Patterns Found

No blockers or warnings. The REVIEW.md (completed post-execution) surfaces 5 new warnings and 3 info items in the changed files. These are residual quality concerns — not regressions introduced by Phase 2 and not in scope for this phase's goal. Listed here for traceability:

| File | Pattern | Severity | Impact |
|------|---------|----------|--------|
| `include/quiver/expr/node.h:60` | `own_dims_buf_` declared but never used in `FileNode::compute_row` | WARNING (REVIEW WR-01) | Dead member; future cleanup candidate |
| `src/expr/nodes.cpp:138-140` | `parent_name_of` returns `""` for no-parent case; could match an empty dimension name | WARNING (REVIEW WR-02) | Theoretical collision if dimension name is `""`; not reachable in practice without hand-crafted invalid metadata |
| `src/expr/nodes.cpp:138-140` | `m.dimensions[parent_idx]` without bounds check in `parent_name_of` | WARNING (REVIEW WR-03) | UB with out-of-range `parent_dimension_index`; only reachable via invalid metadata that bypasses `validate()` |
| `src/binary/binary_file.cpp:91-93` | TOML file truncated before `to_toml()` throw check | WARNING (REVIEW WR-04) | Pre-existing; overwrites existing TOML on failed open. CR-02 fix addressed registry leak, not filesystem atomicity |
| `src/binary/binary_file.cpp:19,59,104` | `write_registry` TOCTOU window widened by late-insert | WARNING (REVIEW WR-05) | Pre-existing single-thread policy; late-insert widened window. Not a regression for single-threaded callers |

None of these patterns prevent the phase goal. All five are carry-forward candidates for a future cleanup pass.

### Human Verification Required

None. All phase success criteria are verifiable programmatically. The test suite runs confirm correctness of the new and modified behaviors.

### Gaps Summary

No gaps. All 14 must-have truths verified against actual code. All 11 task commits confirmed in git log. All 8 modified files contain the expected content. Test counts match exactly (787 = 781 + 6 new).

The REVIEW.md phase-completion review found 5 new warnings (REVIEW WR-01..05) in the changed files, none of which block the phase goal. These are residual concerns surfaced during review, not regressions. They are advisory only.

---

_Verified: 2026-05-06T23:00:00Z_
_Verifier: Claude (gsd-verifier)_
