---
phase: 02-tech-debt
plan: 01
subsystem: infra
tags: [cpp, refactoring, audit-cleanup, expressions, binary-file, iteration]

# Dependency graph
requires:
  - phase: 01-c-core
    provides: BinaryOpNode, BinaryFile write_registry, validate_dimension_values, next_dimensions
provides:
  - Symmetric BinaryOpNode time-dim compatibility check (mirror reject + parent-by-name)
  - Late-insert write_registry lifecycle (no leak on partial-construction failure)
  - Unified "Cannot apply:" verb across BinaryOpNode throws
  - Dead-code removal (validate_file_is_open, dimension_sizes_at_values member, next_dimensions delegate)
  - Single source of truth for calendar-aware time-dim consistency (validate_dimension_values_indexed only)
  - O(1) reverse translation tables in BinaryOpNode::compute_row
  - apply_op runtime canary for unhandled BinaryOp variants
  - Permanent regression test for implicit BinaryFile->Expression conversion
  - O(1) end-of-iteration detection in next_dimensions (no first_dimensions rebuild per call)
affects: v1.0 milestone tag, all future phases consuming BinaryOpNode/BinaryFile/iteration

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Late-insert resource registration: insert into shared registries only after all throwable construction steps succeed"
    - "Map-to-indexed delegation: public map-keyed APIs convert in declaration order, then delegate to the canonical indexed path"
    - "Pre-computed reverse translation tables: O(1) operand-index -> output-index lookup in hot loops"
    - "Exhaustive-switch runtime canary: throw on fall-through to surface forgotten enum variants"
    - "Bool incremented flag for end-of-iteration detection: avoid per-call first_dimensions rebuild"

key-files:
  created: []
  modified:
    - src/expr/nodes.cpp (CR-01 + WR-01 compatibility loop, CR-03 verb, WR-06 reverse tables, WR-07 canary)
    - include/quiver/expr/node.h (WR-06 lhs_to_out_/rhs_to_out_ private members)
    - src/binary/binary_file.cpp (CR-02 late-insert, WR-02 dead-code removal, WR-03 dedup, WR-04 delegate removal, WR-05 map delegates to indexed)
    - include/quiver/binary/binary_file.h (WR-02 + WR-04 declaration removals)
    - src/binary/iteration.cpp (WR-09 bool incremented flag)
    - src/binary/csv_converter.cpp (WR-04 collateral: re-point to free function with optional unwrap)
    - tests/test_expression.cpp (4 D-23 tests for CR-01 + WR-01, 1 D-25 test for WR-08)
    - tests/test_binary_file.cpp (1 D-24 regression test for CR-02)

key-decisions:
  - "ParentDimNameMismatchThrows test uses monthly+daily on both sides with parent-name divergence (not weekly+daily on one side) so the per-row write-time time-dim consistency validation passes; only the cross-operand parent-name check distinguishes the two metadata"
  - "Late-insert pattern preserves the function-prologue concurrent-writer rejection while moving registry population to after all throwable steps succeed -- destructor remains a no-op for empty registered_path"
  - "Map-version validate_dimension_values keeps the count check up front (preserves existing error message order), then converts map->vector while validating presence of each declared name, then delegates to indexed validator"
  - "Reverse translation tables built once at construction from the forward tables; the Pass-1/Pass-2 invariant of broadcast_meta_ guarantees no -1 entries (every operand dim appears in output)"

patterns-established:
  - "Pattern 1: Late-insert into shared registries -- insert AFTER all throwable construction steps succeed; rely on RAII destructor to clean up only when registration actually happened"
  - "Pattern 2: Map-keyed -> indexed delegation for FFI-adjacent validators -- preserve user-facing error messages on the map path, deduplicate calendar logic onto the indexed path"
  - "Pattern 3: Pre-computed reverse translation tables -- avoid O(n^2) inner search in compute_row hot loops by inverting the forward translation once at construction"

requirements-completed: []  # cleanup phase -- no REQUIREMENTS.md entries; closes audit-tracked debt items CR-01..03 + WR-01..09

# Metrics
duration: 27min
completed: 2026-05-06
---

# Phase 02 Plan 01: Tech-Debt Cleanup Summary

**All 12 audit items (CR-01..CR-03 + WR-01..WR-09) closed: BinaryOpNode validation made symmetric and parent-by-name, BinaryFile write_registry no longer leaks on partial-construction failure, error verb unified to "Cannot apply:", three dead-code paths removed, calendar-aware time-dim logic deduplicated, BinaryOpNode hot path uses O(1) reverse translation, apply_op fails loudly on unknown variants, implicit-conversion regression test added, next_dimensions detects end-of-iteration without per-call first_dimensions rebuild.**

## Performance

- **Duration:** ~27 min
- **Started:** 2026-05-06T20:22:32Z
- **Completed:** 2026-05-06T20:49:25Z
- **Tasks:** 11
- **Files modified:** 8 (src/expr/nodes.cpp, include/quiver/expr/node.h, src/binary/binary_file.cpp, include/quiver/binary/binary_file.h, src/binary/iteration.cpp, src/binary/csv_converter.cpp, tests/test_expression.cpp, tests/test_binary_file.cpp)
- **New tests:** +6 (4 D-23 + 1 D-24 + 1 D-25); total quiver_tests 781 -> 787

## Accomplishments

- **CR-01 + WR-01 (security strengthening):** BinaryOpNode time-dim compatibility now rejects same-name time/non-time mismatch SYMMETRICALLY (lhs-time/rhs-non-time AND lhs-non-time/rhs-time both throw). Parent dimension is compared BY NAME (each side's parent_dimension_index resolved to a name on its own side, then compared across operands), so cross-position parent matches accept and parent-name mismatches reject correctly.
- **CR-02 (DoS mitigation):** BinaryFile::open_file('w', ...) now defers `write_registry.insert(canonical)` and `binary_file.impl_->registered_path = canonical` to AFTER `to_toml()` and `fill_file_with_nulls()` both succeed. Throw paths leave the registry empty, so a subsequent legitimate writer on the same path can succeed.
- **CR-03:** All 7 BinaryOpNode ctor throws now use the `"Cannot apply: ..."` verb (matches CLAUDE.md three-pattern rule); the obsolete `"Cannot apply binary operation: ..."` prefix is gone repo-wide.
- **WR-02 + WR-03 + WR-04:** Three dead-code paths removed:
  * `validate_file_is_open` (declaration + 5-line definition) -- zero callers anywhere
  * `BinaryFile::dimension_sizes_at_values` (~70-line member body + protected declaration) -- canonical impl lives in iteration.cpp
  * `BinaryFile::next_dimensions` (thin delegate + protected declaration) -- csv_converter.cpp now calls `quiver::binary::next_dimensions` free function with optional unwrap
- **WR-05:** Calendar-aware time-dim consistency check now lives in exactly one place (`validate_dimension_values_indexed`); the map-keyed `validate_dimension_values` converts to declaration-order vector and delegates.
- **WR-06 (perf):** `BinaryOpNode::compute_row` now uses pre-computed `lhs_to_out_` / `rhs_to_out_` reverse tables for O(1) operand->output translation (was O(n) inner search per operand dim per row).
- **WR-07 (security strengthening):** `apply_op` throws `std::runtime_error("Cannot apply: unhandled BinaryOp variant")` instead of returning 0.0 on switch fall-through. Future enum additions or static_cast invariant violations surface as runtime errors.
- **WR-08:** New regression test `ImplicitConversionInOperatorArgument` permanently backstops the `Expression e = a + b;` line where both operands undergo implicit `BinaryFile -> Expression` conversion at the operator+ argument-binding step.
- **WR-09 (perf):** `next_dimensions` uses a `bool incremented` flag for end-of-iteration detection; no more per-call `first_dimensions(meta)` rebuild (~7.3M wasted vector allocations on a 480x500x31 sweep).

## Task Commits

Each task was committed atomically in audit order:

1. **Task 1: CR-01 + WR-01** - `53eed8e` (fix) -- symmetric mirror reject + parent-by-name + 4 D-23 tests
2. **Task 2: CR-02** - `6905ed7` (fix) -- late-insert write_registry + 1 D-24 regression test
3. **Task 3: CR-03** - `31a8644` (fix) -- "Cannot apply:" verb across BinaryOpNode throws
4. **Task 4: WR-02** - `3b882d0` (fix) -- delete dead validate_file_is_open
5. **Task 5: WR-03** - `02e849e` (fix) -- delete duplicate dimension_sizes_at_values member body
6. **Task 6: WR-04** - `afe6830` (fix) -- delete protected next_dimensions/dimension_sizes_at_values; re-point csv_converter
7. **Task 7: WR-05** - `5551af3` (fix) -- validate_dimension_values map delegates to indexed
8. **Task 8: WR-06** - `729048e` (fix) -- pre-computed reverse translation tables in BinaryOpNode
9. **Task 9: WR-07** - `d63a604` (fix) -- apply_op throws on unhandled BinaryOp variant
10. **Task 10: WR-08** - `08dbdf3` (test) -- ImplicitConversionInOperatorArgument
11. **Task 11: WR-09** - `1c6e599` (fix) -- next_dimensions bool incremented flag

## Files Created/Modified

- `src/expr/nodes.cpp` -- BinaryOpNode ctor compatibility loop rewritten (CR-01 + WR-01); 5 throw verbs unified to "Cannot apply:" (CR-03); reverse translation table populated at construction and consumed in compute_row (WR-06); apply_op fall-through replaced with throw (WR-07).
- `include/quiver/expr/node.h` -- two new private members `lhs_to_out_` / `rhs_to_out_` (WR-06).
- `src/binary/binary_file.cpp` -- late-insert write_registry lifecycle in `case 'w':` (CR-02); deleted `validate_file_is_open` definition (WR-02); deleted duplicate `dimension_sizes_at_values` body (WR-03); deleted thin `next_dimensions` delegate (WR-04); rewrote map-keyed `validate_dimension_values` to convert + delegate (WR-05).
- `include/quiver/binary/binary_file.h` -- removed `validate_file_is_open` declaration (WR-02); removed protected `next_dimensions` + `dimension_sizes_at_values` declarations (WR-04).
- `src/binary/iteration.cpp` -- `next_dimensions` now uses `bool incremented` flag for end-of-iteration detection (WR-09).
- `src/binary/csv_converter.cpp` -- both `csv_to_bin` and `bin_to_csv` loops now call `quiver::binary::next_dimensions` free function directly with `std::optional` unwrap; added `#include "quiver/binary/iteration.h"` (WR-04 collateral).
- `tests/test_expression.cpp` -- 4 D-23 tests (`MirrorTimeNonTimeMismatchAThrows`, `MirrorTimeNonTimeMismatchBThrows`, `ParentDimMatchByNameAcceptsCrossPosition`, `ParentDimNameMismatchThrows`) and 1 D-25 test (`ImplicitConversionInOperatorArgument`).
- `tests/test_binary_file.cpp` -- 1 D-24 test (`OpenFileWriteFailureDoesNotLeakRegistry`).

## Decisions Made

- **D-23 #4 metadata redesign:** the planned weekly+daily metadata for `md_b` failed at write-time validation because `datetime_to_int` for `Daily` returns `ymd.day()` (day-of-month, 1-31) regardless of parent frequency. Substituted monthly+daily on both sides with the parent dim renamed to `stage` on `md_b` -- preserves the cross-operand parent-NAME divergence (`month` vs `stage`) without tripping the per-row write-time consistency check. Documented as a deviation below.
- All other tasks executed exactly as written in the plan.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] D-23 #4 test metadata adjusted to avoid spurious write-time validation failure**
- **Found during:** Task 1 (CR-01 + WR-01)
- **Issue:** The plan-specified metadata for `ParentDimNameMismatchThrows` used weekly+daily on `md_b`. Both `compute_time_dimension_initial_values` and `datetime_to_int` for the `Daily` case return `ymd.day()` (day-of-month, 1-31), which is incompatible with a weekly parent (where days range 1-7 representing day-of-week). The first iteration `[1, 1]` -> writing day=1 inside the wrong calendar context produced "Invalid values for time dimensions" before the test could even reach the EXPECT_THROW block.
- **Fix:** Replaced `md_b`'s `time_dimensions=["week", "day"]`, `frequencies=["weekly", "daily"]` with `time_dimensions=["stage", "block"]`, `frequencies=["monthly", "daily"]`. Both sides now use monthly+daily so per-row write validation succeeds; only the cross-operand parent NAMES differ (`month` vs `stage`), which is exactly the WR-01 case under test.
- **Files modified:** tests/test_expression.cpp
- **Verification:** `ExpressionFixture.ParentDimNameMismatchThrows` reports PASSED with the throw originating from the new BinaryOpNode parent-by-name comparison (Task 1 GREEN-phase code), not from a spurious write-time validation.
- **Committed in:** 53eed8e (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** No scope creep. Test still exercises the exact WR-01 contract (parent-name divergence rejected). The deviation documented an upstream limitation in `datetime_to_int` (Daily returns day-of-month, not day-of-week or day-of-year) that pre-dates Phase 2 and is out of scope.

## Issues Encountered

- None. The build was already current at execution start; all 11 tasks compiled cleanly and passed verification on the first build after each edit.

## User Setup Required

None - this is a pure C++ refactor. No external services, no configuration, no new dependencies.

## Threat Surface Scan

No new attack surface introduced. Net security delta is positive across V5 (Input Validation), V8 (Data Protection), V12 (Files/Resources):

- **T-2-101 (DoS / Tampering, V12 + V8):** CR-02 fix prevents the `write_registry` from getting stuck after a partial-construction failure (e.g., disk full during `fill_file_with_nulls()`, validation throw during `to_toml()`). Subsequent legitimate writers on the same canonical path can succeed.
- **T-2-102 (Tampering / Repudiation, V5):** WR-07 replaces the `apply_op` switch fall-through `return 0.0;` with a throw. Future `BinaryOp` variants added without updating `apply_op` now surface as runtime errors instead of silent zero outputs.
- **T-2-103 (Tampering, V5):** CR-01 + WR-01 close the asymmetry where lhs non-time + rhs time on the same dim previously passed validation. The mirror-reject also closes the parent-by-RAW-INDEX comparison that erroneously rejected same-name time dims with parents at different operand positions but same parent name.

## TDD Gate Compliance

This plan is `type: execute`, not `type: tdd`, so plan-level RED/GREEN/REFACTOR gating does not apply. However, three tasks (Task 1, Task 2, Task 10) had `tdd="true"` per their per-task frontmatter:

- **Task 1 (CR-01 + WR-01):** D-23 tests added in commit 53eed8e BEFORE the GREEN-phase nodes.cpp edit (within the same commit, RED phase first by file order). Tests fail on Phase 1 code; pass after the in-commit source change.
- **Task 2 (CR-02):** D-24 test added in commit 6905ed7 BEFORE the GREEN-phase binary_file.cpp edit (within the same commit). Test fails on Phase 1 code; passes after the in-commit source change.
- **Task 10 (WR-08):** D-25 test added in commit 08dbdf3. The non-explicit `Expression(const BinaryFile&)` constructor at expression.h was already present in Phase 1, so the test passes immediately on the post-Phase-1 baseline -- it serves as a permanent regression backstop, not a behavior-driving test.

## Next Phase Readiness

- 12/12 audit items closed.
- 787/787 quiver_tests + 403/403 quiver_c_tests green.
- v1.0 ready to tag once orchestrator merges this worktree and runs the milestone-close workflow.
- No blockers, no deferred items, no open questions.

## Self-Check: PASSED

**Created files:** None (this plan only modified files; no new files were created.)

**Commit hashes verified in `git log --oneline -12`:**
- FOUND: 53eed8e (CR-01 + WR-01)
- FOUND: 6905ed7 (CR-02)
- FOUND: 31a8644 (CR-03)
- FOUND: 3b882d0 (WR-02)
- FOUND: 02e849e (WR-03)
- FOUND: afe6830 (WR-04)
- FOUND: 5551af3 (WR-05)
- FOUND: 729048e (WR-06)
- FOUND: d63a604 (WR-07)
- FOUND: 08dbdf3 (WR-08)
- FOUND: 1c6e599 (WR-09)

**Modified files verified:**
- FOUND: src/expr/nodes.cpp
- FOUND: include/quiver/expr/node.h
- FOUND: src/binary/binary_file.cpp
- FOUND: include/quiver/binary/binary_file.h
- FOUND: src/binary/iteration.cpp
- FOUND: src/binary/csv_converter.cpp
- FOUND: tests/test_expression.cpp
- FOUND: tests/test_binary_file.cpp

---
*Phase: 02-tech-debt*
*Completed: 2026-05-06*
