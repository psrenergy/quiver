# Roadmap: Quiver v0.3 -- Explicit Transactions

## Overview

Expose explicit transaction control (`begin_transaction`, `commit`, `rollback`) starting from the C++ core, wrapping through the C API, and propagating to all three language bindings (Julia, Dart, Lua). Each phase delivers a complete, testable layer. A benchmark phase proves the performance motivation is real.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: C++ Transaction Core** - Public begin/commit/rollback with nest-aware TransactionGuard
- [ ] **Phase 2: C API Transaction Surface** - Flat C functions wrapping the C++ transaction methods
- [ ] **Phase 3: Language Bindings** - Julia, Dart, and Lua bindings with convenience wrappers
- [ ] **Phase 4: Performance Benchmark** - Prove measurable speedup from batched transactions

## Phase Details

### Phase 1: C++ Transaction Core
**Goal**: Users of the C++ API can wrap multiple write operations in a single explicit transaction
**Depends on**: Nothing (first phase)
**Requirements**: TXN-01, TXN-02, TXN-03
**Success Criteria** (what must be TRUE):
  1. Caller can call `begin_transaction()`, perform multiple writes, then `commit()` or `rollback()` on a Database instance
  2. Existing write methods (`create_element`, `update_*`, `delete_element`) execute without error inside an explicit transaction -- no "cannot start a transaction within a transaction" failures
  3. Calling `commit()` or `rollback()` without an active transaction throws an error matching Quiver's "Cannot {operation}: {reason}" pattern
  4. Calling `begin_transaction()` when a transaction is already active throws an error matching Quiver's "Cannot {operation}: {reason}" pattern
  5. All existing C++ tests continue to pass unchanged (no regression)
**Plans**: TBD

Plans:
- [ ] 01-01: TBD

### Phase 2: C API Transaction Surface
**Goal**: C API consumers can control transactions through flat FFI-safe functions
**Depends on**: Phase 1
**Requirements**: CAPI-01
**Success Criteria** (what must be TRUE):
  1. `quiver_database_begin_transaction`, `quiver_database_commit`, and `quiver_database_rollback` exist as C functions returning `quiver_error_t`
  2. A C API caller can batch multiple `quiver_database_create_element` calls inside a single transaction with one commit
  3. Error cases (double begin, commit without begin) return `QUIVER_ERROR` with descriptive messages from `quiver_get_last_error()`
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: Language Bindings
**Goal**: Julia, Dart, and Lua users can control transactions idiomatically in their language
**Depends on**: Phase 2
**Requirements**: BIND-01, BIND-02, BIND-03, BIND-04, BIND-05, BIND-06
**Success Criteria** (what must be TRUE):
  1. Julia users can call `begin_transaction!(db)`, `commit!(db)`, `rollback!(db)` and use `transaction(db) do ... end` with auto commit on success and rollback on exception
  2. Dart users can call `db.beginTransaction()`, `db.commit()`, `db.rollback()` and use `db.transaction(() { ... })` with auto commit on success and rollback on exception
  3. Lua users can call `db:begin_transaction()`, `db:commit()`, `db:rollback()` and use `db:transaction(fn)` with pcall-based auto commit on success and rollback on error
  4. Error messages from misuse (double begin, commit without begin) propagate correctly from C++ through each binding without bindings crafting their own messages
  5. All existing binding tests continue to pass unchanged (no regression)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: Performance Benchmark
**Goal**: Measurable proof that explicit transactions improve write throughput
**Depends on**: Phase 1
**Requirements**: PERF-01
**Success Criteria** (what must be TRUE):
  1. A C++ benchmark test creates multiple elements with time series data both with and without an explicit wrapping transaction
  2. The batched-transaction variant completes measurably faster than the individual-transaction variant on a file-based database
  3. Benchmark results are printed to stdout so a human can observe the speedup ratio
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. C++ Transaction Core | 0/0 | Not started | - |
| 2. C API Transaction Surface | 0/0 | Not started | - |
| 3. Language Bindings | 0/0 | Not started | - |
| 4. Performance Benchmark | 0/0 | Not started | - |
