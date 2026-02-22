# Phase 1: C++ Transaction Core - Context

**Gathered:** 2026-02-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Public `begin_transaction()` / `commit()` / `rollback()` on Database, with internal TransactionGuard becoming nest-aware so existing write methods (create_element, update_*, delete_element) work correctly inside an explicit transaction. Also adds `in_transaction()` query method (pulled from TQRY-01 since the machinery is already required).

</domain>

<decisions>
## Implementation Decisions

### Nesting model
- Binary flag approach using `sqlite3_get_autocommit()` — no ref-counting, no depth tracking
- TransactionGuard checks autocommit state: if already in transaction, becomes complete no-op (no begin, no commit, no rollback on destruction)
- When TransactionGuard is a no-op and the inner operation throws, the guard silently skips rollback — exception propagates to the caller who controls the explicit transaction

### Error behavior
- Strict enforcement: all misuse throws, no silent no-ops
- `begin_transaction()` when already active: `"Cannot begin_transaction: transaction already active"`
- `commit()` without active transaction: `"Cannot commit: no active transaction"`
- `rollback()` without active transaction: `"Cannot rollback: no active transaction"`
- If SQLite auto-rolls-back after a failed statement, subsequent `rollback()` throws (no active transaction) — caller must handle the original error correctly
- Dangling transactions are caller's responsibility — Database does not auto-rollback on close. Bindings add RAII wrappers in Phase 3

### Error check placement
- Error checks (autocommit state validation) live in the public `Database` methods, not in `Impl`
- `Impl` methods stay low-level pass-throughs to SQLite — no validation logic there
- Clean separation: `Database` = contract enforcement, `Impl` = raw SQLite operations

### API surface
- `in_transaction()` added now (not deferred) — returns `bool`, wraps `sqlite3_get_autocommit()`
- Full stack propagation: C++ in Phase 1, C API in Phase 2, all bindings in Phase 3

### Claude's Discretion
- Whether `with_transaction(lambda)` helper on Impl also becomes nest-aware, based on actual usage in codebase
- Test organization and edge case coverage
- Exact logging (spdlog) messages for transaction operations

</decisions>

<specifics>
## Specific Ideas

- `begin_transaction()`, `commit()`, `rollback()` already exist as thin Impl pass-throughs — phase modifies them to add contract checks
- TransactionGuard already exists in `database_impl.h` — phase modifies it to check `sqlite3_get_autocommit()` before issuing BEGIN
- Error messages follow existing Quiver pattern exactly: "Cannot {method_name}: {specific reason}"

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-c-transaction-core*
*Context gathered: 2026-02-20*
