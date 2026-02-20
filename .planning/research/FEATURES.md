# Feature Research: Explicit Transaction Control

**Domain:** SQLite wrapper library -- explicit user-controlled transactions
**Researched:** 2026-02-20
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features that every SQLite wrapper library with explicit transaction support provides. Missing any of these makes the transaction API feel broken or incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| `begin_transaction()` | Every SQLite wrapper (rusqlite, better-sqlite3, Python sqlite3, go-sqlite3, Microsoft.Data.Sqlite, Dart sqlite3, Lua lsqlite3) exposes explicit begin. Users need to batch multiple operations into one atomic unit. | LOW | Maps directly to `sqlite3_exec(db, "BEGIN TRANSACTION")`. Quiver already has this as a private `Impl::begin_transaction()`. Promote to public. |
| `commit()` | Complement to begin. All libraries expose this. Without commit, transactions are useless. | LOW | Already exists as private `Impl::commit()`. Promote to public. |
| `rollback()` | Every library provides explicit rollback. Essential for error recovery paths. | LOW | Already exists as private `Impl::rollback()`. Note: current impl intentionally does not throw on failure (logs error instead). This is correct -- rollback is error recovery code. |
| Nest-aware internal TransactionGuard (no-op when user transaction active) | When a user wraps `create_element` + `update_time_series_group` in a single transaction, the internal `TransactionGuard` in each method must detect the active transaction and become a no-op. Otherwise it fails with "cannot start a transaction within a transaction". This is the core value proposition -- batch operations avoid redundant fsync. | MEDIUM | Use `sqlite3_get_autocommit(db)` to detect active transaction. If autocommit is off (returns 0), `TransactionGuard` constructor skips `BEGIN` and destructor skips `ROLLBACK`. This is the approach used by Peewee ORM, better-sqlite3, and countless other wrappers. |
| `in_transaction()` query | Python sqlite3 exposes `connection.in_transaction`, better-sqlite3 has `.inTransaction`, Dart sqlite3 exposes autocommit detection. Users need to check transaction state for conditional logic and error handling (detecting forced rollbacks by SQLite on errors like `SQLITE_FULL` or `SQLITE_BUSY`). | LOW | Maps to `sqlite3_get_autocommit(db) == 0`. Single function, no state to manage. Also used internally by TransactionGuard. |
| Error on misuse: double begin | Every library surfaces a clear error when `BEGIN` is called inside an active transaction. SQLite itself returns "cannot start a transaction within a transaction". | LOW | Let SQLite's native error propagate through Quiver's existing error handling. Alternatively, pre-check with `sqlite3_get_autocommit()` and throw Quiver-pattern error: "Cannot begin_transaction: transaction already active". |
| Error on misuse: commit without begin | Every library surfaces a clear error when `COMMIT` is called with no active transaction. SQLite returns "cannot commit - no transaction is active". | LOW | Let SQLite's native error propagate, or pre-check and throw Quiver-pattern error: "Cannot commit: no active transaction". |
| C API exposure | Quiver's architecture requires all public C++ methods to propagate through C API. FFI consumers expect flat function calls. | LOW | Four new functions following existing patterns: `quiver_database_begin_transaction`, `quiver_database_commit`, `quiver_database_rollback`, `quiver_database_in_transaction`. Return `quiver_error_t`, try/catch with `quiver_set_last_error()`. |
| Julia bindings | Julia convention: `!` suffix for mutating operations. Quiver binding consistency requires these. | LOW | `begin_transaction!(db)`, `commit!(db)`, `rollback!(db)`, `in_transaction(db)`. Thin wrappers over C API via `ccall`. |
| Dart bindings | Dart convention: camelCase. Quiver binding consistency requires these. | LOW | `beginTransaction()`, `commit()`, `rollback()`, `inTransaction` (getter). Thin wrappers over C API via FFI. |
| Lua bindings | Lua convention: snake_case matching C++. Quiver binding consistency requires these. | LOW | `db:begin_transaction()`, `db:commit()`, `db:rollback()`, `db:in_transaction()`. Expose via LuaRunner userdata methods. |

### Differentiators (Competitive Advantage)

Features that go beyond the minimum and would make Quiver's transaction API notably pleasant to use. These are where Quiver can feel more polished than raw SQLite wrappers.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Julia `transaction` do-block helper | Julia idiom: `transaction(db) do ... end` with automatic commit on success, rollback on exception. Feels native to Julia users. Analogous to better-sqlite3's `db.transaction(fn)` and rusqlite's RAII Transaction. | LOW | Pure Julia convenience wrapper composing `begin_transaction!`, `commit!`, `rollback!`. No C/C++ changes needed. Example: `transaction(db) do; create_element!(db, "Items", el); update_time_series_group!(db, ...); end` |
| Dart `transaction` callback helper | Dart idiom: `db.transaction(() { ... })` with automatic commit/rollback. Matches sqflite's `transaction()` method pattern that Dart developers already know. | LOW | Pure Dart convenience wrapper. No C/C++ changes needed. |
| Lua `transaction` callback helper | Lua idiom: `db:transaction(function() ... end)` with pcall-based error handling. Matches community patterns from lsqlite3 usage. | LOW | Pure Lua convenience wrapper using `pcall`. No C/C++ changes needed. |
| C++ benchmark demonstrating fsync savings | Quantified proof that explicit transactions improve performance. PROJECT.md lists this as a v0.3 requirement. The performance gain is dramatic: SQLite benchmarks consistently show 10-100x speedup for batched inserts in a single transaction vs. individual auto-committed statements, because each auto-commit triggers an fsync. | MEDIUM | Standalone benchmark binary. Use `create_element` + `update_time_series_group` in a loop, compare wall-clock time with and without wrapping transaction. Expect ~20-50x improvement for the create+update-time-series pattern. |
| Quiver-pattern error messages for misuse | Instead of SQLite's raw error strings, use Quiver's three error patterns. "Cannot begin_transaction: transaction already active" is clearer than "cannot start a transaction within a transaction". Consistent with the library's error handling philosophy. | LOW | Pre-check `sqlite3_get_autocommit()` before `BEGIN`/`COMMIT`/`ROLLBACK` and throw Quiver-format errors. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem reasonable but introduce complexity disproportionate to their value for Quiver's use case.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| SAVEPOINT-based nested transactions | Users may want composable transaction boundaries (function A starts transaction, calls function B which also starts transaction). rusqlite, better-sqlite3, Peewee, and Microsoft.Data.Sqlite all support this. | SAVEPOINTs add significant complexity: name management, RELEASE vs ROLLBACK TO semantics, interaction with COMMIT (which releases ALL savepoints), and partial rollback logic. Quiver is a single-user embedded database -- the composable middleware pattern that motivates savepoints does not apply. The no-op TransactionGuard pattern is simpler and sufficient for batching. | TransactionGuard becomes no-op when nested. User's outer `begin_transaction`/`commit` covers the full batch. If users genuinely need partial rollback, they can use raw SQL SAVEPOINTs via `query_string("SAVEPOINT sp1")`. |
| Transaction behavior modes (DEFERRED/IMMEDIATE/EXCLUSIVE) | rusqlite exposes `TransactionBehavior` enum, better-sqlite3 has `.deferred`/`.immediate`/`.exclusive` properties, Microsoft.Data.Sqlite has `deferred: bool`. Useful in multi-connection concurrent write scenarios to avoid SQLITE_BUSY on lock upgrade. | Quiver is a single-connection library (one `Database` object owns one `sqlite3*` handle). There is no contention between connections. DEFERRED (the default) acquires the write lock on first write statement, which is identical behavior for sequential write batches. Exposing modes adds API surface with zero benefit for the common case. | Use `BEGIN TRANSACTION` (DEFERRED) always. If a user has an exotic multi-connection scenario, they can use `query_string("BEGIN IMMEDIATE")` directly. |
| Automatic retry on SQLITE_BUSY during commit | Multi-connection scenarios may trigger SQLITE_BUSY on COMMIT when a reader holds a shared lock. Some libraries implement retry-with-backoff. | Quiver is single-connection. SQLITE_BUSY cannot occur. Even if it could, retry logic with backoff, timeout policies, and edge cases is far beyond the scope of "expose begin/commit/rollback". | Let the error propagate. Users can implement their own retry logic if they somehow encounter this. |
| Transaction isolation level configuration | Some ORMs expose isolation level settings. Developers from PostgreSQL/MySQL backgrounds expect this. | SQLite has a fixed isolation model (serializable for write transactions, snapshot isolation for reads). There is nothing to configure. Exposing a setting that does nothing is misleading. | Do not expose what does not exist. |
| Autocommit mode toggle | Python sqlite3's `autocommit` attribute lets users disable/enable automatic transaction wrapping globally. | Quiver's architecture already manages this through TransactionGuard. A global autocommit toggle would require rethinking every write method. The explicit `begin_transaction`/`commit` pair is the correct escape hatch for batching. | Users who want to batch operations use explicit `begin_transaction`/`commit`. No global mode switch needed. |
| C++ RAII Transaction wrapper in public API | rusqlite returns a `Transaction` object that commits on success and rolls back on drop. Elegant in Rust/C++. | PROJECT.md already decided on flat `begin`/`commit`/`rollback`. RAII wrapper does not cross FFI boundaries cleanly -- C API and bindings would still need flat begin/commit/rollback. Adding both creates two ways to do the same thing, violating Quiver's simplicity principle. | Flat `begin_transaction`/`commit`/`rollback` in C++ and C API. Idiomatic wrappers (do-block, callback, pcall) in each binding language. |
| Async transaction support | Dart and Node.js developers may expect async patterns. | Quiver is synchronous by design (C++ core, synchronous FFI). SQLite serializes all transactions. better-sqlite3 explicitly warns that "async functions always return after the first await, meaning the transaction is already committed before any async code executes." Async wrappers would be misleading. | Keep synchronous. Dart callers can wrap in their own async layers at the application level. |

## Feature Dependencies

```
begin_transaction / commit / rollback (C++ public)
    |
    +--requires--> in_transaction() query (C++ public)
    |                  |
    |                  +--uses--> sqlite3_get_autocommit()
    |
    +--requires--> Nest-aware TransactionGuard modification
    |                  |
    |                  +--uses--> sqlite3_get_autocommit() (same mechanism)
    |
    +--enables---> C API exposure
    |                  |
    |                  +--enables--> Julia bindings (begin_transaction!, commit!, rollback!, in_transaction)
    |                  +--enables--> Dart bindings (beginTransaction, commit, rollback, inTransaction)
    |                  +--enables--> Lua bindings (begin_transaction, commit, rollback, in_transaction)
    |
    +--enables---> Binding-level convenience wrappers
    |                  |
    |                  +-- Julia: transaction(db) do ... end
    |                  +-- Dart: db.transaction(() { ... })
    |                  +-- Lua: db:transaction(function() ... end)
    |
    +--enables---> Performance benchmark (needs working explicit transactions to measure)

Misuse error messages (C++)
    +--requires--> in_transaction() / sqlite3_get_autocommit()
```

### Dependency Notes

- **TransactionGuard no-op requires `in_transaction` detection:** The guard must call `sqlite3_get_autocommit()` to know whether to issue BEGIN or skip. This is the same mechanism that powers the public `in_transaction()` method.
- **C API requires C++ methods:** Standard Quiver propagation chain. C API wraps C++ methods with try/catch and `quiver_set_last_error()`.
- **Bindings require C API:** Julia/Dart/Lua call C API via FFI. Binding-level convenience wrappers compose the flat begin/commit/rollback functions.
- **Benchmark requires working explicit transactions:** Cannot benchmark transaction batching without the feature implemented first.
- **`in_transaction()` should be implemented first or alongside begin/commit/rollback:** It is used both internally (TransactionGuard) and externally (user queries, binding convenience wrappers for error detection).

## MVP Definition

### Launch With (v0.3 Core)

Minimum viable explicit transaction support -- what must ship.

- [ ] `begin_transaction()`, `commit()`, `rollback()` promoted to public C++ API
- [ ] `in_transaction()` const query method on Database
- [ ] Nest-aware TransactionGuard (no-op when `sqlite3_get_autocommit()` returns 0)
- [ ] C API: `quiver_database_begin_transaction`, `quiver_database_commit`, `quiver_database_rollback`, `quiver_database_in_transaction`
- [ ] Julia bindings: `begin_transaction!`, `commit!`, `rollback!`, `in_transaction`
- [ ] Dart bindings: `beginTransaction`, `commit`, `rollback`, `inTransaction`
- [ ] Lua bindings: `begin_transaction`, `commit`, `rollback`, `in_transaction`
- [ ] C++ tests: basic lifecycle, nesting, misuse errors
- [ ] C API tests: mirror C++ coverage
- [ ] Binding tests: each binding tests transaction lifecycle

### Add After Validation (v0.3 Polish)

Features to add once core transaction API is working and tested.

- [ ] Julia `transaction(db) do ... end` convenience wrapper
- [ ] Dart `db.transaction(() { ... })` convenience wrapper
- [ ] Lua `db:transaction(function() ... end)` convenience wrapper
- [ ] C++ benchmark: `create_element` + `update_time_series_group` performance with/without explicit transaction

### Future Consideration (v0.4+)

Features to defer until transaction API has real-world usage feedback.

- [ ] SAVEPOINT support -- only if users request composable nested transactions
- [ ] Transaction behavior modes (IMMEDIATE/EXCLUSIVE) -- only if multi-connection use cases emerge
- [ ] Commit/rollback notification hooks -- only if event-driven patterns are needed

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Public begin/commit/rollback (C++) | HIGH | LOW | P1 |
| in_transaction() query (C++) | HIGH | LOW | P1 |
| Nest-aware TransactionGuard | HIGH | MEDIUM | P1 |
| Misuse error messages | MEDIUM | LOW | P1 |
| C API exposure (4 functions) | HIGH | LOW | P1 |
| Julia bindings (4 functions) | HIGH | LOW | P1 |
| Dart bindings (4 functions) | HIGH | LOW | P1 |
| Lua bindings (4 functions) | HIGH | LOW | P1 |
| C++/C API/binding tests | HIGH | MEDIUM | P1 |
| Julia do-block helper | MEDIUM | LOW | P2 |
| Dart callback helper | MEDIUM | LOW | P2 |
| Lua callback helper | MEDIUM | LOW | P2 |
| Performance benchmark | MEDIUM | MEDIUM | P2 |
| SAVEPOINT nesting | LOW | HIGH | P3 |
| Transaction modes | LOW | LOW | P3 |

**Priority key:**
- P1: Must have for v0.3 launch
- P2: Should have, add during v0.3 polish
- P3: Nice to have, defer to future milestone

## Competitor Feature Analysis

| Feature | rusqlite (Rust) | better-sqlite3 (Node.js) | Python sqlite3 | go-sqlite3 (Go) | Microsoft.Data.Sqlite (C#) | Quiver (Our Approach) |
|---------|----------------|--------------------------|-----------------|------------------|----------------------------|----------------------|
| Begin/Commit/Rollback | `conn.transaction()` returns RAII `Transaction` | `db.transaction(fn)` wraps function | `conn.commit()` / `conn.rollback()` | `db.BeginTx()` returns `sql.Tx` | `conn.BeginTransaction()` returns `SqliteTransaction` | Flat `begin_transaction()` / `commit()` / `rollback()` on Database |
| Nesting strategy | `Savepoint` type via `tx.savepoint()` | Automatic savepoints for nested `transaction()` calls | Manual `SAVEPOINT` SQL | Manual `SAVEPOINT` SQL | Savepoints (v6.0+) | No-op TransactionGuard; manual SAVEPOINT via `query_string` if needed |
| Transaction modes | `TransactionBehavior` enum (Deferred/Immediate/Exclusive) | `.deferred` / `.immediate` / `.exclusive` properties | `isolation_level` attribute | `sql.TxOptions` | `BeginTransaction(deferred: bool)` | DEFERRED only (single-connection; modes via raw `query_string` SQL if needed) |
| State query | Not directly exposed (borrow checker prevents misuse) | `.inTransaction` boolean | `.in_transaction` boolean | Implicit via `sql.Tx` lifecycle | Implicit via `SqliteTransaction` lifecycle | `in_transaction()` boolean |
| Auto-rollback on scope exit | Yes (default DropBehavior::Rollback) | Yes (exception in transaction fn triggers rollback) | Yes (close() rolls back pending) | `defer tx.Rollback()` idiom | Dispose rolls back | Binding-level convenience wrappers handle this; flat API requires explicit rollback |
| Error on misuse | Compile-time via borrow checker (prevents nested begin) | Runtime (SQLite error propagated) | Runtime (SQLite error) | Runtime (SQLite error) | Runtime (SQLite error) | Runtime with Quiver-pattern error messages (pre-checked) |

### Key Ecosystem Insight

The ecosystem splits into two camps:

1. **Object-based transactions** (rusqlite, Go, C#): `begin()` returns a transaction object. Operations happen on the transaction object. Object scope determines lifetime. Works well in statically typed languages with ownership semantics.

2. **State-based transactions** (Python, Lua, Dart sqlite3, better-sqlite3): `begin()` changes connection state. Operations happen on the same connection object. State is queried via boolean property. Works well across FFI boundaries and in dynamic languages.

Quiver's flat API falls squarely in camp 2, which is the correct choice for a C API that must cross FFI boundaries to Julia, Dart, and Lua. Binding languages can layer object-based or callback-based wrappers on top if desired.

## Sources

- [SQLite Transaction Documentation](https://www.sqlite.org/lang_transaction.html) -- official transaction semantics, BEGIN types, nesting restrictions
- [SQLite sqlite3_get_autocommit()](https://sqlite.org/c3ref/get_autocommit.html) -- autocommit detection API
- [SQLite Savepoints](https://sqlite.org/lang_savepoint.html) -- SAVEPOINT/RELEASE/ROLLBACK TO semantics
- [SQLite Speed Comparison](https://www.sqlite.org/speed.html) -- transaction batching performance impact
- [rusqlite Transaction](https://docs.rs/rusqlite/latest/rusqlite/struct.Transaction.html) -- RAII transaction with savepoint nesting
- [better-sqlite3 API](https://github.com/WiseLibs/better-sqlite3/blob/master/docs/api.md) -- .transaction() wrapper, .inTransaction, nested savepoints
- [better-sqlite3 Nested Transactions Issue #38](https://github.com/WiseLibs/better-sqlite3/issues/38) -- savepoint nesting design discussion
- [Python sqlite3 Documentation](https://docs.python.org/3/library/sqlite3.html) -- in_transaction, autocommit, isolation_level
- [Go database/sql Transactions](https://go.dev/doc/database/execute-transactions) -- BeginTx, defer Rollback pattern
- [Microsoft.Data.Sqlite Transactions](https://learn.microsoft.com/en-us/dotnet/standard/data/sqlite/transactions) -- BeginTransaction, deferred, savepoints
- [Dart sqlite3 package](https://pub.dev/packages/sqlite3) -- manual BEGIN/COMMIT/ROLLBACK via execute()
- [PowerSync SQLite Optimizations](https://www.powersync.com/blog/sqlite-optimizations-for-ultra-high-performance) -- WAL mode, synchronous pragma, fsync overhead

---
*Feature research for: Explicit Transaction Control in SQLite Wrapper Library*
*Researched: 2026-02-20*
