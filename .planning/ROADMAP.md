# Roadmap: Quiver — Lazy Expressions (v1)

## Overview

This milestone adds a new `quiver::expr` subsystem that layers lazy arithmetic on top of the existing `BinaryFile` infrastructure. Scope is the C++ core only: `Expression` value type, `Node` AST hierarchy (FileNode, ScalarNode, BinaryOpNode), four binary operators with broadcasting-aware shape compatibility, scalar broadcast, and a row-by-row `save(path)` engine that materializes any expression tree into a new `.qvr` file. The C API and language bindings (CAPI-*, BIND-*, TEST-02, TEST-03) are deferred to a future milestone.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: C++ Core** - Build `quiver::expr` namespace, Node hierarchy, Expression class, operators, and row-by-row save engine, all covered by GoogleTest

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
- [ ] 01-02-PLAN.md — quiver::expr core: Expression value type, Node hierarchy (FileNode, ScalarNode, BinaryOpNode with full broadcast validation), 12 operator overloads, save engine with D-11 self-save check, baseline tests
- [ ] 01-03-PLAN.md — Comprehensive Expression test suite: full op matrix, 8 scalar broadcast variants, broadcast/union edge cases, large-grid smoke
