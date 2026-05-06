# Roadmap: Quiver — Lazy Expressions (v1)

## Overview

This milestone adds a new `quiver::expr` subsystem that layers lazy arithmetic on top of the existing `BinaryFile` infrastructure. Scope is the C++ core only: `Expression` value type, `Node` AST hierarchy (FileNode, ScalarNode, BinaryOpNode), four binary operators with broadcasting-aware shape compatibility, scalar broadcast, and a row-by-row `save(path)` engine that materializes any expression tree into a new `.qvr` file. The C API and language bindings (CAPI-*, BIND-*, TEST-02, TEST-03) are deferred to a future milestone.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: C++ Core** - Build `quiver::expr` namespace, Node hierarchy, Expression class, operators, and row-by-row save engine, all covered by GoogleTest
- [ ] **Phase 2: Phase 1 Tech Debt** - Address tech debt accumulated in Phase 1 (CR-01..03 + WR-01..09) per v1.0-MILESTONE-AUDIT.md

## Phase Details

### Phase 1: C++ Core
**Goal**: Deliver a working `quiver::expr` subsystem in the C++ core — `Expression` value type, virtual `Node` AST (FileNode / ScalarNode / BinaryOpNode), four binary operators, scalar broadcast, and a row-by-row `save(path)` engine that materializes any expression tree into a new `.qvr` file.
**Depends on**: Nothing (first phase)
**Requirements**: CORE-01, CORE-02, CORE-03, CORE-04, CORE-05, CORE-06, TEST-01
**Success Criteria** (what must be TRUE):
  1. User can construct an `Expression` from an open `BinaryFile` (implicit conversion) and call `expression.save(path)` to copy the file row-by-row through the AST engine.
  2. User can write `(a + b) * 2.0` in C++ and `expression.save("out.qvr")` produces a `.qvr` file whose values match element-wise expectations against goldens.
  3. User can chain operators into trees of arbitrary depth (e.g. `((a + b) - c) / 2.0`, scalar on left or right) and the materialized output remains correct.
  4. `quiver_tests.exe` passes a new `test_expression.cpp` GoogleTest suite that covers AddTwoFilesAndScale, ScalarBroadcast, Chained, IdentityFile, and MismatchedShapes cases — verifying CORE-01..06 end-to-end.
  5. The `save()` loop reuses a single `vector<double>` row buffer across iterations (verifiable in code review) — no per-row heap allocation in the steady-state inner loop.
**Plans**: 3 plans
- [x] 01-01-PLAN.md — Binary subsystem extensions (D-12 free-function iterators, D-13 fast read overloads + read_into, D-14 fast write overload)
- [x] 01-02-PLAN.md — quiver::expr core: Expression value type, Node hierarchy (FileNode, ScalarNode, BinaryOpNode with full broadcast validation), 12 operator overloads, save engine with D-11 self-save check, baseline tests
- [x] 01-03-PLAN.md — Comprehensive Expression test suite: full op matrix, 8 scalar broadcast variants, broadcast/union edge cases, large-grid smoke

### Phase 2: Phase 1 Tech Debt
**Goal**: Close the 12 review findings carried forward from Phase 1 (3 BLOCKER + 9 WARNING) tracked in `.planning/v1.0-MILESTONE-AUDIT.md`. Tighten correctness on the deferred D-04 mirror-case gap (CR-01), eliminate duplication and dead code in the Binary subsystem, harden BinaryOpNode error-message conformance + perf, and add the missing implicit-conversion test.
**Depends on**: Phase 1 (operates on `src/expr/*.cpp`, `src/binary/*.cpp`, `include/quiver/binary/*.h`)
**Requirements**: None (cleanup phase — no new REQUIREMENTS.md entries; closes audit-tracked debt items)
**Success Criteria** (what must be TRUE):
  1. **CR-01 closed**: D-04 symmetric mirror-case is exercised by a new test (`lhs` non-time vs `rhs` time on same dim name) and `BinaryOpNode` either accepts via parent-name matching or rejects via the 3 CLAUDE.md error patterns — no silent semantic drop.
  2. **CR-02 closed**: `BinaryFile::open_file('w', ...)` no longer leaks the canonical path into `write_registry` on partial-construction failure (RAII guard or try/catch around registry insert + downstream construction).
  3. **CR-03 closed**: All `runtime_error` messages in `src/expr/nodes.cpp` conform to one of the three CLAUDE.md patterns (`"Cannot {op}: ..."` / `"... not found: ..."` / `"Failed to {op}: ..."`).
  4. **WR-01..09 closed**: D-04 over-restrictive parent-index comparison fixed (WR-01); dead `validate_file_is_open()` removed (WR-02); `dimension_sizes_at_values` deduped between `binary_file.cpp` and `iteration.cpp` (WR-03); unused `BinaryFile` members removed (WR-04); `validate_dimension_values` and `_indexed` deduped (WR-05); `BinaryOpNode::compute_row` O(n²) inner search replaced with pre-computed forward translation table (WR-06); `apply_op` exhaustive switch with no fall-through default (WR-07); implicit `BinaryFile→Expression` test added (WR-08); `next_dimensions` end-of-iteration check no longer rebuilds `first_dimensions` per call (WR-09).
  5. **Regression-free**: Full `quiver_tests` suite (currently 781/781) plus any new tests pass; no Phase 1 truth is broken.
**Plans**: 1 plan
- [x] 02-01-PLAN.md — Close all 12 audit items in audit-doc order via ~11 atomic commits (CR-01 bundles WR-01 per D-26): symmetric mirror reject + parent-by-name (D-15..D-17), late-insert write_registry (D-19), Cannot apply: verb (D-18), dead-code deletions (WR-02/03/04), validate_dimension_values dedup (D-21), reverse translation tables (WR-06), apply_op runtime canary (WR-07), implicit-conversion test (D-25), next_dimensions bool incremented flag (D-22). +6 new tests targeting 787/787 final.
