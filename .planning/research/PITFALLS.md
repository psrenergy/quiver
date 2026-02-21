# Pitfalls Research

**Domain:** Adding explicit transaction control to an SQLite wrapper library with existing internal RAII transactions
**Researched:** 2026-02-20
**Confidence:** HIGH

## Critical Pitfalls

### Pitfall 1: Double BEGIN -- SQLite Does Not Support Nested Transactions

**What goes wrong:**
The existing `TransactionGuard` constructor calls `sqlite3_exec(db, "BEGIN TRANSACTION;")`. If a user calls `db.begin_transaction()` (starting a user transaction), and then calls `create_element()` (which internally constructs a `TransactionGuard`), SQLite returns error code 1: "cannot start a transaction within a transaction". The operation fails and the user's outer transaction is left in an indeterminate state.

This is the single most likely bug when adding user-facing transactions to this codebase, because every write method (`create_element`, `update_element`, `update_vector_*`, `update_set_*`, `update_time_series_group`, `update_time_series_files`) constructs its own `TransactionGuard`. There are 12+ call sites that will all fail inside a user transaction.

**Why it happens:**
SQLite flatly rejects `BEGIN` inside an active transaction. This is not a configurable nesting limitation -- it is a fundamental constraint of SQLite's transaction model. The only mechanism for nested transaction-like behavior is `SAVEPOINT`, which has its own complexity (see Pitfall 4).

**How to avoid:**
Make `TransactionGuard` nest-aware. In the constructor, check `sqlite3_get_autocommit(db)` -- if it returns 0 (already in a transaction), the guard becomes a no-op (does not issue BEGIN, does not issue COMMIT/ROLLBACK in destructor). If it returns non-zero (autocommit mode, no active transaction), proceed as today.

```cpp
explicit TransactionGuard(Impl& impl) : impl_(impl) {
    if (sqlite3_get_autocommit(impl_.db)) {
        impl_.begin_transaction();
        owns_transaction_ = true;
    }
    // else: already in a transaction, become a no-op
}
```

Add a `bool owns_transaction_ = false;` member. Only call `commit()` / `rollback()` in destructor/commit when `owns_transaction_` is true.

**Warning signs:**
- Any test that calls `begin_transaction()` then any write method will crash immediately
- Error message: "Failed to begin transaction: cannot start a transaction within a transaction"

**Phase to address:**
Phase 1 (C++ core) -- this must be the very first change, before any public API is exposed. All existing tests must continue passing with the modified `TransactionGuard`.

---

### Pitfall 2: TransactionGuard Rollback Inside User Transaction Destroys User's Work

**What goes wrong:**
An internal operation fails (e.g., type validation error in `create_element`), and the `TransactionGuard` destructor issues `ROLLBACK`, destroying the user's entire transaction including all prior successful operations.

**Why it happens:**
The `TransactionGuard` destructor pattern (rollback on non-committed destruction) is correct for standalone internal transactions. But when nested inside a user transaction, `ROLLBACK` terminates the outer transaction too -- SQLite does not scope ROLLBACK to any inner context. There is only one transaction, and ROLLBACK ends it entirely.

**How to avoid:**
When `TransactionGuard` detects an active user transaction (via `sqlite3_get_autocommit()` returning 0), it MUST become a complete no-op: no `BEGIN`, no `COMMIT`, no `ROLLBACK`. Exception propagation handles error communication -- the user catches the exception and decides whether to `rollback()` or `commit()`.

This is the direct consequence of Pitfall 1's fix: if the guard does not own the transaction, it must not touch it at all.

**Warning signs:**
- User calls `begin_transaction()`, does 10 successful creates, the 11th fails, and all 10 are silently rolled back
- Test: `begin` -> `create A` (success) -> `create B` (fail with type error) -> verify A is still readable -> works only if guard is a true no-op

**Phase to address:**
Phase 1 (C++ core) -- tested together with Pitfall 1 fix.

---

### Pitfall 3: Abandoned User Transactions Lock the Database

**What goes wrong:**
A user calls `begin_transaction()` but never calls `commit()` or `rollback()` -- due to an exception, early return, or simply forgetting. The transaction remains open. In SQLite, an open write transaction holds a RESERVED lock (or EXCLUSIVE during writes), blocking all other writers. If the `Database` object goes out of scope with an open transaction, `sqlite3_close_v2()` will eventually roll back, but the lock was held for the entire object lifetime.

In FFI scenarios (Julia, Dart, Lua), this is especially dangerous: if the binding user forgets to finalize the transaction, the database remains locked until garbage collection destroys the wrapper object -- which may never happen promptly in GC-based languages.

**Why it happens:**
Explicit `begin`/`commit`/`rollback` without RAII puts the burden of cleanup entirely on the user. C does not have exceptions, Julia/Dart use GC not RAII, and Lua has no destructors. Unlike the internal `TransactionGuard` which guarantees rollback in its destructor, user-facing transactions have no automatic safety net.

**How to avoid:**
1. In `Impl::~Impl()`, check `sqlite3_get_autocommit(db)` before closing. If a transaction is active (returns 0), log a warning and issue `ROLLBACK` before `sqlite3_close_v2()`.
2. In bindings, provide lambda/closure-based transaction wrappers that guarantee cleanup:

Julia:
```julia
function transaction(f, db::Database)
    begin_transaction!(db)
    try
        result = f()
        commit!(db)
        return result
    catch
        rollback!(db)
        rethrow()
    end
end
```

Dart:
```dart
T transaction<T>(T Function() action) {
    beginTransaction();
    try {
        final result = action();
        commit();
        return result;
    } catch (e) {
        rollback();
        rethrow();
    }
}
```

3. Document that calling `begin_transaction()` without a corresponding `commit()`/`rollback()` is a programming error.

**Warning signs:**
- "database is locked" errors in multi-connection scenarios
- Application hangs waiting for locks
- Test suite hangs or times out

**Phase to address:**
Phase 1 (C++ core) -- add destructor safety. Phase 3+ (bindings) -- provide idiomatic transaction wrappers.

---

### Pitfall 4: SAVEPOINT Complexity Leaking Into the Design

**What goes wrong:**
Instead of making `TransactionGuard` a no-op inside user transactions, the implementation uses `SAVEPOINT` for nested transaction support. This introduces significant complexity:

- `ROLLBACK TO savepoint` does not end the savepoint -- the transaction remains active, the savepoint stays on the stack
- `RELEASE savepoint` is not a durable commit -- an outer rollback undoes it
- Savepoint names must be tracked (depth counter or unique names)
- `COMMIT` releases ALL savepoints, breaking outer savepoint isolation
- Each savepoint opens a sub-journal (not free, though cheaper than full journals)
- Savepoint leaks: forgetting `RELEASE` after `ROLLBACK TO` leaves orphaned entries on the transaction stack
- Reusing savepoint names merely shadows the old one, potentially causing logic bugs

**Why it happens:**
Developers from PostgreSQL/SQL Server backgrounds expect nested transactions. They reach for SAVEPOINT as the SQLite equivalent. But SAVEPOINT semantics are subtly different and introduce edge cases that are hard to test across FFI boundaries.

**How to avoid:**
Do not use SAVEPOINT. The project's pending design decision ("TransactionGuard becomes no-op when nested") is correct. When a user transaction is active, internal `TransactionGuard` instances detect this and become no-ops. The user's transaction provides atomicity for all operations within it.

The no-op approach matches Quiver's philosophy: "Simple solutions over complex abstractions." The user transaction already provides the ACID guarantees. Internal guards adding SAVEPOINT nesting would be defensive code that violates "clean code over defensive code."

**Warning signs:**
- PR introducing `SAVEPOINT` names, depth counters, or savepoint stacks
- Edge case tests for "ROLLBACK TO then RELEASE" sequences
- Cross-binding differences in savepoint behavior

**Phase to address:**
Phase 1 (C++ core) -- validate the no-op design decision. Document the rationale in code comments so future maintainers do not "improve" it by adding SAVEPOINT support.

---

### Pitfall 5: Manual `is_in_transaction_` Flag Getting Out of Sync with SQLite State

**What goes wrong:**
A boolean flag says "in transaction" but SQLite has auto-rolled-back the transaction due to `SQLITE_FULL`, `SQLITE_IOERR`, `SQLITE_NOMEM`, `SQLITE_BUSY`, or `SQLITE_INTERRUPT`. Subsequent operations silently run in autocommit mode, each committing individually. The user believes they are in a transaction when they are not.

**Why it happens:**
SQLite can automatically rollback a transaction on certain catastrophic errors without any C API notification or callback. The official documentation explicitly states that `sqlite3_get_autocommit()` is "the only way to find out whether SQLite automatically rolled back the transaction after an error." A boolean flag cannot capture this.

**How to avoid:**
Use `sqlite3_get_autocommit()` directly instead of a boolean flag. Always call it to query transaction state. Never cache the result in a variable that can become stale.

Alternatively, use `sqlite3_txn_state()` (available since SQLite 3.34.0; Quiver uses 3.50.2) for richer state information: `SQLITE_TXN_NONE`, `SQLITE_TXN_READ`, `SQLITE_TXN_WRITE`.

For the public `in_transaction()` API method, `!sqlite3_get_autocommit(db)` is the correct implementation.

**Warning signs:**
- Silent data integrity violations -- operations that should be atomic committed individually
- `commit()` throws "cannot commit - no transaction is active" unexpectedly
- Hard to reproduce because auto-rollback requires disk-level conditions

**Phase to address:**
Phase 1 (C++ core) -- use `sqlite3_get_autocommit()` exclusively, never a flag.

---

### Pitfall 6: Rollback in Cleanup Path Masking the Original Exception

**What goes wrong:**
A user's operation fails inside a transaction. The cleanup path (destructor, catch block, binding finally-equivalent) calls `rollback()`. But SQLite has already auto-rolled-back the transaction due to the error. The `ROLLBACK` SQL statement fails with "cannot rollback - no transaction is active", and this secondary error masks the original error.

This exact bug hit Microsoft.Data.Sqlite (dotnet/efcore #36561): `Commit()` throws `SQLITE_FULL`, SQLite auto-rolls-back, then `Dispose()` tries to rollback and throws a different error, masking the original "disk full" message.

**Why it happens:**
Certain SQLite errors (`SQLITE_FULL`, `SQLITE_IOERR`, `SQLITE_NOMEM`, `SQLITE_BUSY`, `SQLITE_INTERRUPT`) cause SQLite to automatically rollback the transaction. After auto-rollback, `sqlite3_get_autocommit()` returns non-zero (back in autocommit mode). If cleanup code blindly issues `ROLLBACK`, SQLite returns an error.

**How to avoid:**
1. Before calling `ROLLBACK` in any cleanup path, check `sqlite3_get_autocommit(db)`. If it returns non-zero, the transaction is already gone -- skip the rollback.
2. The existing `Impl::rollback()` already does not throw on failure (it logs and swallows). Keep this behavior for internal paths.
3. In the public `rollback()` method, throw if called without an active transaction -- helps users detect logic errors. But the internal cleanup must remain no-throw.

```cpp
// Public method -- throws on misuse
void Database::rollback() {
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot rollback: no transaction is active");
    }
    impl_->rollback();
}

// Destructor path -- never throws
Impl::~Impl() {
    if (db && !sqlite3_get_autocommit(db)) {
        logger->warn("Destroying database with open transaction -- rolling back");
        rollback();  // internal, swallows errors
    }
    if (db) sqlite3_close_v2(db);
}
```

**Warning signs:**
- Error messages about "no transaction is active" when there clearly should be one
- Original errors swallowed, only secondary errors visible
- Confusing error messages from bindings

**Phase to address:**
Phase 1 (C++ core) -- check autocommit state before rollback in all cleanup paths.

---

### Pitfall 7: Benchmark Measuring Wrong Configuration (Journal Mode / Synchronous)

**What goes wrong:**
The project motivation for explicit transactions is performance: `create_element` + `update_time_series_group` currently run two separate transactions (two fsyncs). The benchmark should show improvement from wrapping them in one transaction. But benchmark results are meaningless if SQLite configuration is not controlled:

1. **WAL mode vs. DELETE journal**: WAL is much faster for writes. If the benchmark DB is in WAL mode, the single-transaction improvement appears smaller (WAL already reduces fsync cost).
2. **synchronous=FULL vs. NORMAL**: `FULL` (the default) fsyncs every commit -- the exact cost batching eliminates. `NORMAL` in WAL mode only fsyncs on checkpoint, making the improvement appear minimal.
3. **synchronous=OFF**: Eliminates fsyncs entirely. Two transactions vs. one shows near-zero difference.
4. **In-memory database (`:memory:`)**: No fsync at all. Benchmark is meaningless.

**Why it happens:**
Developers run benchmarks on default settings without realizing SQLite's default is conservative (`DELETE` journal, `synchronous=FULL`). Or they copy "recommended pragmas" from blog posts (`WAL`, `synchronous=NORMAL`) which minimize the exact cost being measured.

**How to avoid:**
1. Benchmark with explicit, documented SQLite configuration. Pin `journal_mode=DELETE` and `synchronous=FULL` for the "maximum improvement" benchmark. Also benchmark with `journal_mode=WAL` and `synchronous=NORMAL` for "production" comparison.
2. Always use file-based databases, never `:memory:`.
3. Report both configurations and actual speedup ratios.
4. Keep assertions loose: `EXPECT_LT(batched_ms, unbatched_ms)`. Print ratios for human inspection but do not gate CI on specific speedup numbers -- they depend on hardware (SSD vs HDD), OS, and system load.

**Warning signs:**
- Benchmark shows "only 10% improvement" -- likely running WAL+NORMAL
- Benchmark shows "50x improvement" on local machine but fails CI assertion -- hardware-dependent
- No PRAGMA configuration documented in benchmark code

**Phase to address:**
Benchmark phase -- must be designed before the feature is considered "complete".

---

### Pitfall 8: BEGIN DEFERRED vs. BEGIN IMMEDIATE -- Wrong Lock Type

**What goes wrong:**
SQLite's `BEGIN` starts a DEFERRED transaction -- no locks are acquired until the first read or write. If a user calls `begin_transaction()` intending to batch writes, but first does a read, they acquire a SHARED lock. When the first write executes, SQLite tries to promote to RESERVED, which can fail with `SQLITE_BUSY` if another connection holds a SHARED lock. This is the classic "reader starves writer" problem.

**Why it happens:**
Most wrapper libraries use plain `BEGIN` (DEFERRED) by default because it is the simplest. But for Quiver's transaction batching use case, `BEGIN IMMEDIATE` is almost always correct: the user intends to write, so acquiring the write lock upfront avoids lock-promotion failures.

**How to avoid:**
Use `BEGIN IMMEDIATE` for user-facing `begin_transaction()`:
- Quiver's primary use case is batching writes (create + update in one transaction)
- Single-connection usage means no contention from within the same process
- `BEGIN IMMEDIATE` is strictly safer than `BEGIN DEFERRED` for write transactions
- Performance cost of acquiring RESERVED lock upfront is negligible

**Warning signs:**
- `SQLITE_BUSY` errors when the first write executes inside a user transaction
- Intermittent failures in multi-connection setups

**Phase to address:**
Phase 1 (C++ core) -- use `BEGIN IMMEDIATE` in the implementation.

---

### Pitfall 9: commit() and rollback() Called Without Active Transaction

**What goes wrong:**
A user calls `commit()` or `rollback()` without a preceding `begin_transaction()`. Or calls `commit()` twice. In SQLite:
- `COMMIT` without an active transaction returns an error
- `ROLLBACK` without an active transaction returns an error
- `COMMIT` after a prior `COMMIT` returns an error

If not handled, the raw SQLite error message leaks through the C API to binding users.

**Why it happens:**
With explicit begin/commit/rollback (not RAII), tracking transaction state is error-prone. Conditional logic, early returns, and exception handling create opportunities for mismatched calls.

**How to avoid:**
In the public C++ methods, check `sqlite3_get_autocommit(db)` before executing:
- `begin_transaction()`: if autocommit is OFF, throw `"Cannot begin_transaction: transaction already active"`
- `commit()`: if autocommit is ON, throw `"Cannot commit: no transaction is active"`
- `rollback()`: if autocommit is ON, throw `"Cannot rollback: no transaction is active"`

These follow the existing error message pattern: `"Cannot {operation}: {reason}"`.

**Warning signs:**
- Raw SQLite error messages appearing in `quiver_get_last_error()`
- Double-commit bugs in binding code
- Confusing state after error recovery

**Phase to address:**
Phase 1 (C++ core) -- validate preconditions in all three public methods.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| `bool in_transaction_` flag instead of `sqlite3_get_autocommit()` | Avoids C API call | Flag becomes stale after auto-rollback; silent data integrity violations | Never -- SQLite is the source of truth |
| Skipping precondition checks on `commit()`/`rollback()` | Less code | Silent corruption when called without active transaction; confusing errors propagate to bindings | Never |
| Adding SAVEPOINT for nesting | Handles hypothetical edge cases | Massive complexity for zero practical benefit; different semantics than real nested transactions | Never for this project |
| Exposing only raw `begin`/`commit`/`rollback` in bindings, no wrappers | Ships faster | Binding users forget cleanup, file bug reports about locked databases | Only temporarily; wrappers must ship with the binding phase |
| Using plain `BEGIN` instead of `BEGIN IMMEDIATE` | Matches most examples online | Lock-promotion failures under concurrent access | Never for Quiver's write-batching use case |

## Integration Gotchas

Common mistakes when connecting transaction control across the C++/C/binding stack.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| C++ to C API | Forgetting to catch exceptions from `begin_transaction()` / `commit()` / `rollback()` | Wrap in standard try-catch with `quiver_set_last_error()`, return `QUIVER_ERROR` |
| C API to Julia | Calling `ccall` for `begin_transaction` but not checking return code | Julia wrapper must check error code and throw Julia exception |
| C API to Dart | Not handling the case where `commit()` returns `QUIVER_ERROR` | Dart wrapper must throw `QuiverException` on error |
| C API to Lua | Exposing `begin_transaction`/`commit`/`rollback` as separate methods without `db:transaction(fn)` convenience | Provide `db:transaction(function() ... end)` that guarantees rollback on Lua error |
| Lua scripting | Script calls `db:begin_transaction()` and errors before committing | `LuaRunner::run()` must check `sqlite3_get_autocommit()` after script execution; if transaction is open, auto-rollback and log warning |
| Internal private vs. public methods | `database.h` already has private `begin_transaction()`, `commit()`, `rollback()` (lines 197-199) | Make these public. Do not create separate public/private versions. The same methods serve both roles. |
| Benchmark | Measuring through C API or bindings instead of C++ directly | Binding overhead can obscure fsync improvement; benchmark C++ core first |

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| User wraps every single operation in `begin`/`commit` | Extra overhead, no benefit (operations already have implicit transactions) | Document that single operations do not need explicit transactions | Immediately -- adds overhead |
| Very large transaction (100K+ operations) | WAL file grows unboundedly, checkpoint cannot run, memory pressure | Document recommended batch sizes; suggest chunked transactions | 10K+ operations in DELETE journal; 100K+ in WAL |
| Benchmark without warmup | First iteration includes initialization, page cache warmup | Discard first N iterations or run setup outside measured block | First benchmark run |
| Holding transaction open during computation | Database locked for duration of unrelated work | Keep transactions short: prepare data, then begin-write-commit | As soon as multiple connections exist |
| Benchmark on in-memory database | Shows zero improvement because no fsync | Always use file-based database for transaction benchmarks | Immediately |
| Flaky benchmark assertions | `EXPECT_GT(ratio, 10.0)` fails on different hardware | Use loose assertions (`batched < unbatched`); print ratios for human review | Different CI hardware |

## Security Mistakes

Domain-specific security issues relevant to transaction control.

| Mistake | Risk | Prevention |
|---------|------|------------|
| Exposing `begin_transaction` in Lua without cleanup | Lua script opens transaction, errors out, transaction stays open, locking DB | `LuaRunner::run()` auto-rollbacks open transactions after script execution |
| `query_*()` methods inside explicit transaction | User can execute DDL (`DROP TABLE`, `ALTER TABLE`) atomically inside transaction | Already a risk with `query_*()` methods; transactions do not change this. Document that `query_*()` participates in user transactions. |

## UX Pitfalls

Common user experience mistakes in this domain.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| No way to query transaction state | User cannot conditionally begin a transaction in reusable code | Provide `bool in_transaction() const` wrapping `!sqlite3_get_autocommit(db)` |
| Raw SQLite error leaking: "cannot start a transaction within a transaction" | User sees internal SQLite message instead of Quiver-style | Intercept in `begin_transaction()`: `"Cannot begin_transaction: transaction already active"` |
| Rollback does not re-throw original error | User calls rollback in catch block but loses what originally failed | Document: rollback is cleanup; re-throw original exception from the catch block |
| No feedback about auto-rollback | User commits after auto-rollback, gets confusing error | `commit()` should throw `"Cannot commit: no transaction is active"` with clear messaging |
| Logging noise from no-op TransactionGuard | 1000-operation batch produces 1000 "skipping BEGIN" debug lines | Use `trace` level for no-op message, or omit entirely |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **TransactionGuard nest-awareness:** Tested with ALL write methods, not just `create_element`. Verify: `update_element`, `update_vector_*` (3 types), `update_set_*` (3 types), `update_time_series_group`, `update_time_series_files`, `update_scalar_relation`, `import_csv`
- [ ] **Error inside user transaction:** Test that a failed operation does NOT rollback prior successful operations within the same user transaction
- [ ] **commit() after auto-rollback:** Test what happens when `commit()` is called after SQLite has auto-rolled-back (should throw clear error, not raw SQLite message)
- [ ] **Destructor cleanup:** `Database` destructor rolls back open transaction without crash or leak
- [ ] **C API error propagation:** `quiver_database_begin_transaction` returns `QUIVER_ERROR` with proper message when transaction already active
- [ ] **Binding convenience wrappers:** Julia `transaction()`, Dart `transaction()`, Lua `db:transaction()` all (a) rollback on exception, (b) re-throw original error, (c) do not mask errors
- [ ] **LuaRunner safety:** `LuaRunner::run()` auto-rollbacks any open transaction when script ends or errors
- [ ] **Benchmark configuration:** Documents PRAGMA settings, runs with at least two configs (DELETE+FULL, WAL+NORMAL), uses file-based DB
- [ ] **Read operations inside transaction:** `read_scalar_*`, `read_vector_*`, `query_*` work correctly inside explicit transaction (they do not use TransactionGuard, so should work, but verify)
- [ ] **Double-call guards:** `begin` then `begin` throws. `commit` then `commit` throws. `rollback` without `begin` throws.
- [ ] **FFI generators re-run:** Julia and Dart FFI generators executed after C API header changes

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Double BEGIN crash (P1) | LOW | Fix TransactionGuard to check autocommit; all existing tests verify the fix |
| Guard rollback destroys user work (P2) | LOW | Make guard full no-op; add test for partial-failure-within-transaction |
| Abandoned transaction / locked DB (P3) | LOW | Close and reopen Database; SQLite auto-rolls-back on close. Add destructor safety. |
| SAVEPOINT complexity (P4) | HIGH | Remove SAVEPOINT, revert to no-op. Significant rewrite if deeply integrated. Prevention is critical. |
| Stale flag (P5) | MEDIUM | Replace flag with `sqlite3_get_autocommit()` calls. Audit all usage sites. |
| Masked error from rollback (P6) | MEDIUM | Add autocommit check before rollback in all cleanup paths; audit C API catch blocks |
| Wrong benchmark config (P7) | LOW | Re-run with correct PRAGMAs; add PRAGMA setup to benchmark code |
| Wrong lock type (P8) | LOW | Change `BEGIN` to `BEGIN IMMEDIATE`; single line change |
| Mismatched commit/rollback (P9) | LOW | Add precondition checks; straightforward |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| P1: Double BEGIN | Phase 1: C++ core | Existing tests pass + new test: `begin_transaction()` then `create_element()` succeeds |
| P2: Guard rollback in user txn | Phase 1: C++ core | Test: begin, create A, create B (fails), verify A still readable, commit |
| P3: Abandoned transaction | Phase 1 + Phase 3 bindings | Destructor test: destroy with open txn, no crash. Binding wrapper tests. |
| P4: SAVEPOINT complexity | Phase 1: C++ core | Code review: no SAVEPOINT in codebase. Design doc rejects it. |
| P5: Stale flag | Phase 1: C++ core | No flag in implementation; `sqlite3_get_autocommit()` used directly |
| P6: Masked errors | Phase 1: C++ core | Test: force error inside transaction, verify original error propagates through bindings |
| P7: Wrong benchmark config | Benchmark phase | Benchmark code has explicit PRAGMA setup and documents settings |
| P8: DEFERRED vs IMMEDIATE | Phase 1: C++ core | Implementation uses `BEGIN IMMEDIATE`. Grep for plain `BEGIN TRANSACTION` confirms no stragglers. |
| P9: Mismatched calls | Phase 1: C++ core | Tests: commit without begin throws, rollback without begin throws, double begin throws, double commit throws |

## Sources

- [SQLite Transaction Documentation](https://www.sqlite.org/lang_transaction.html) -- HIGH confidence: official docs on BEGIN/COMMIT/ROLLBACK semantics, nesting constraints
- [SQLite SAVEPOINT Documentation](https://sqlite.org/lang_savepoint.html) -- HIGH confidence: official docs on SAVEPOINT/RELEASE/ROLLBACK TO semantics
- [sqlite3_get_autocommit() Documentation](https://sqlite.org/c3ref/get_autocommit.html) -- HIGH confidence: thread safety warning, auto-rollback detection, the "only way to find out" quote
- [sqlite3_txn_state() Documentation](https://sqlite.org/c3ref/txn_state.html) -- HIGH confidence: introduced SQLite 3.34.0, finer-grained than get_autocommit
- [SQLite Threading Modes](https://sqlite.org/threadsafe.html) -- HIGH confidence: official threading model
- [SQLite File Locking and Concurrency](https://sqlite.org/lockingv3.html) -- HIGH confidence: SHARED/RESERVED/EXCLUSIVE lock semantics
- [sqlite3_close() Documentation](https://www.sqlite.org/c3ref/close.html) -- HIGH confidence: behavior on close with open transaction
- [dotnet/efcore Issue #36561](https://github.com/dotnet/efcore/issues/36561) -- HIGH confidence: real-world bug demonstrating rollback masking original error
- [sqlx Issue #3634](https://github.com/launchbadge/sqlx/issues/3634) -- MEDIUM confidence: demonstrates SAVEPOINT leak after ROLLBACK TO
- [SQLiteCpp (SRombauts)](https://github.com/SRombauts/SQLiteCpp) -- MEDIUM confidence: reference C++ wrapper with RAII transaction pattern
- [trailofbits/sqlite_wrapper](https://github.com/trailofbits/sqlite_wrapper) -- MEDIUM confidence: reference wrapper with both RAII guard and manual methods
- [SQLite Performance Tuning (phiresky)](https://phiresky.github.io/blog/2020/sqlite-performance-tuning/) -- MEDIUM confidence: WAL vs journal, synchronous settings
- [PowerSync SQLite Optimizations](https://www.powersync.com/blog/sqlite-optimizations-for-ultra-high-performance) -- MEDIUM confidence: production PRAGMA recommendations
- [SQLite Performance Testing (Draken)](https://ericdraken.com/sqlite-performance-testing/) -- MEDIUM confidence: comprehensive PRAGMA benchmarking
- Existing codebase: `src/database_impl.h` (TransactionGuard), `src/database_create.cpp` (12+ usage sites), `include/quiver/database.h` (private begin/commit/rollback on lines 197-199)

---
*Pitfalls research for: Quiver v0.3 -- Explicit Transaction Control*
*Researched: 2026-02-20*
