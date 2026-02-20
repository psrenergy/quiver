# Feature Landscape

**Domain:** Explicit transaction control for SQLite wrapper library
**Researched:** 2026-02-20

## Table Stakes

Features that are expected for any transaction API. Missing = feels broken.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| `begin_transaction()` | Users expect explicit start of a transaction | Low | Already exists as private method -- just move to public |
| `commit()` | Users expect explicit commit | Low | Already exists as private method -- just move to public |
| `rollback()` | Users expect explicit rollback | Low | Already exists as private method -- just move to public |
| Nest-safe internal guards | Calling `create_element` inside a user transaction must not fail with "cannot start transaction within transaction" | Medium | Modify `TransactionGuard` to check `sqlite3_get_autocommit()` |
| Error on misuse: commit without begin | Users expect a clear error if they call `commit()` with no active transaction | Low | SQLite itself errors: "cannot commit - no transaction is active". Just let it propagate. |
| Error on misuse: double begin | Users expect a clear error if they call `begin_transaction()` twice | Low | SQLite itself errors: "cannot start a transaction within a transaction". Just let it propagate. |
| C API exposure | All C++ public methods must be in C API per project principle | Low | Three new flat functions following existing pattern |
| Julia binding | Julia users expect transaction control | Low | Three new functions following naming conventions |
| Dart binding | Dart users expect transaction control | Low | Three new methods following camelCase conventions |
| Lua binding | Lua users expect transaction control | Low | Three new methods on `db` userdata |

## Differentiators

Features that go beyond basic transaction control. Not expected, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Performance benchmark test | Proves the value of the feature with measurable speedup numbers | Medium | Uses `std::chrono::steady_clock` in a GoogleTest case |
| `in_transaction()` query method | Lets users check if a transaction is active before deciding whether to begin one | Low | Thin wrapper around `sqlite3_get_autocommit()`. Useful for defensive programming in bindings. |
| Binding-level RAII/callback wrappers | Julia `transaction(db) do ... end`, Dart `db.transaction(() { ... })`, Lua `db:transaction(fn)` | Medium | Convenience methods in bindings that call begin/commit/rollback with exception safety |

## Anti-Features

Features to explicitly NOT build in v0.3.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| SAVEPOINT support | Adds naming complexity, partial rollback semantics, and a much larger API surface for no user-facing benefit in this milestone | Use no-op TransactionGuard when nested. SAVEPOINT can be a future milestone if real demand emerges. |
| `BEGIN IMMEDIATE` / `BEGIN EXCLUSIVE` variants | The motivating use case is batching sequential writes. `DEFERRED` (default) acquires the write lock on first write, which is identical behavior for sequential write batches. | Ship `begin_transaction()` as `BEGIN DEFERRED`. Add `IMMEDIATE` variant only if users report `SQLITE_BUSY` contention issues. |
| Automatic retry on `SQLITE_BUSY` during commit | Complex retry logic with backoff, timeout policies, and edge cases. Way beyond the scope of "expose begin/commit/rollback". | Let the error propagate. Users can implement their own retry logic. |
| Transaction isolation level configuration | SQLite has a fixed isolation model (serializable). There is nothing to configure. | Do not expose what does not exist. |
| Distributed transactions / two-phase commit | SQLite is an embedded database. This concept does not apply. | Not applicable. |
| Automatic transaction wrapping for all read operations | Reads in SQLite already run in implicit transactions. Wrapping reads in explicit transactions only matters for snapshot consistency across multiple reads, which is an advanced use case. | Document that users can wrap reads in explicit transactions for snapshot isolation if needed. |

## Feature Dependencies

```
begin_transaction() (public)
  -> TransactionGuard nesting awareness (must detect active user txn)
    -> sqlite3_get_autocommit() check in TransactionGuard constructor

C API functions
  -> begin_transaction(), commit(), rollback() must be public first

Julia/Dart/Lua bindings
  -> C API functions must exist first

Benchmark test
  -> begin_transaction(), commit() must work
  -> TransactionGuard must be nest-aware (otherwise create_element fails inside user txn)
```

## MVP Recommendation

Prioritize:
1. **Move begin/commit/rollback to public** -- unlocks everything else
2. **Nest-aware TransactionGuard** -- required for correctness (create_element inside user txn)
3. **C API exposure** -- required before any binding work
4. **All four bindings** (Julia, Dart, Lua) -- project principle requires all bindings for all public methods
5. **Benchmark test** -- proves the feature delivers measurable value

Defer:
- `in_transaction()` query method: Nice to have, but not required for the core use case. Can be added in a follow-up.
- Binding-level RAII/callback wrappers: Nice DX improvement but can be a follow-up. The raw begin/commit/rollback functions are sufficient.
- `BEGIN IMMEDIATE`: No demonstrated need yet. `DEFERRED` matches existing internal behavior.

---
*Feature landscape for: Quiver v0.3 -- Explicit Transaction Control*
*Researched: 2026-02-20*
