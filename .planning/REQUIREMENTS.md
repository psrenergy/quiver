# Requirements: Quiver

**Defined:** 2026-02-20
**Core Value:** Reliable, schema-validated SQLite access through a single C++ core with mechanically-derived bindings that feel native in every target language.

## v0.3 Requirements

Requirements for Explicit Transactions milestone. Each maps to roadmap phases.

### Transaction Control

- [x] **TXN-01**: Caller can wrap multiple write operations in a single transaction via `begin_transaction` / `commit` / `rollback` on the public C++ API
- [x] **TXN-02**: Existing write methods (create_element, update_*, delete_element) work correctly inside an explicit transaction without "cannot start a transaction within a transaction" errors
- [x] **TXN-03**: Misusing transaction API (double begin, commit/rollback without begin) throws Quiver-pattern error messages

### C API

- [ ] **CAPI-01**: C API exposes `quiver_database_begin_transaction` / `quiver_database_commit` / `quiver_database_rollback` / `quiver_database_in_transaction` as flat functions returning `quiver_error_t`
  - Includes TQRY-01 scope: `in_transaction()` exposed as `quiver_database_in_transaction(db, bool* out_active)`

### Bindings

- [ ] **BIND-01**: Julia binding exposes `begin_transaction!` / `commit!` / `rollback!`
- [ ] **BIND-02**: Dart binding exposes `beginTransaction` / `commit` / `rollback`
- [ ] **BIND-03**: Lua binding exposes `begin_transaction` / `commit` / `rollback`
- [ ] **BIND-04**: Julia provides `transaction(db) do...end` with auto commit on success / rollback on exception
- [ ] **BIND-05**: Dart provides `db.transaction(() {...})` with auto commit/rollback
- [ ] **BIND-06**: Lua provides `db:transaction(fn)` with pcall-based auto commit/rollback

### Performance

- [ ] **PERF-01**: C++ benchmark demonstrates measurable speedup from batching `create_element` + `update_time_series_group` in a single transaction vs separate transactions

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

None currently.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| SAVEPOINT-based nested transactions | No-op TransactionGuard is simpler and sufficient; SAVEPOINT adds complexity with zero benefit for single-connection use |
| Transaction behavior modes (DEFERRED/IMMEDIATE/EXCLUSIVE) | Single-connection library; DEFERRED is correct for sequential write batches |
| C++ RAII Transaction wrapper in public API | Flat begin/commit/rollback is simpler for FFI; bindings layer their own RAII |
| Autocommit mode toggle | Explicit begin/commit is the correct escape hatch; global toggle would require rethinking every write method |
| Automatic retry on SQLITE_BUSY | Single-connection; SQLITE_BUSY cannot occur |
| Async transaction support | Quiver is synchronous by design; SQLite serializes all transactions |
| Python bindings | Stub exists, not implementing this milestone |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| TXN-01 | Phase 1 | Complete |
| TXN-02 | Phase 1 | Complete |
| TXN-03 | Phase 1 | Complete |
| CAPI-01 | Phase 2 | Pending |
| TQRY-01 | Phase 2 | Pending |
| BIND-01 | Phase 3 | Pending |
| BIND-02 | Phase 3 | Pending |
| BIND-03 | Phase 3 | Pending |
| BIND-04 | Phase 3 | Pending |
| BIND-05 | Phase 3 | Pending |
| BIND-06 | Phase 3 | Pending |
| PERF-01 | Phase 4 | Pending |

**Coverage:**
- v0.3 requirements: 12 total (11 original + TQRY-01 moved from Future)
- Mapped to phases: 12
- Unmapped: 0

---
*Requirements defined: 2026-02-20*
*Last updated: 2026-02-20 after roadmap creation*
