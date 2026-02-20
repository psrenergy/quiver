# Domain Pitfalls

**Domain:** Explicit transaction control for SQLite wrapper library
**Researched:** 2026-02-20

## Critical Pitfalls

Mistakes that cause rewrites or major issues.

### Pitfall 1: Using `:memory:` Database for Benchmark Tests

**What goes wrong:** The benchmark shows negligible difference between batched and unbatched operations, leading to the false conclusion that explicit transactions provide no benefit.
**Why it happens:** In-memory databases have no fsync overhead. The entire performance benefit of explicit transactions comes from reducing fsync calls (N fsyncs down to 1).
**Consequences:** Benchmark is meaningless. Feature appears to provide no value. Stakeholders question why it was built.
**Prevention:** Always use file-based databases (`"bench_test.db"`) for transaction performance benchmarks. Clean up test files after the test completes.
**Detection:** If the speedup ratio is less than 2x for 50+ batched write operations, something is wrong -- likely using in-memory DB.

### Pitfall 2: TransactionGuard Rolling Back Inside a User Transaction

**What goes wrong:** An internal operation fails (e.g., type validation error in `create_element`), and the `TransactionGuard` destructor issues `ROLLBACK`, destroying the user's entire transaction including any prior successful operations.
**Why it happens:** The `TransactionGuard` destructor pattern (rollback on non-committed destruction) is correct for standalone internal transactions. But when nested inside a user transaction, `ROLLBACK` terminates the *outer* transaction too -- SQLite does not support nested `BEGIN`/`ROLLBACK`.
**Consequences:** User calls `begin_transaction()`, does 10 successful operations, the 11th fails, and suddenly all 10 are rolled back without the user explicitly choosing to rollback. Data loss.
**Prevention:** When `TransactionGuard` detects an active user transaction (via flag or `sqlite3_get_autocommit()`), it MUST become a complete no-op: no `BEGIN`, no `COMMIT`, no `ROLLBACK`. Exception propagation handles error communication -- the user catches the exception and decides whether to `rollback()` or `commit()` (committing the 10 successful operations and handling the 11th separately).
**Detection:** Write a test: begin user txn, create element A (success), create element B (fail), catch exception, verify element A is still readable within the transaction, then commit or rollback.

### Pitfall 3: Manual `is_in_transaction_` Flag Getting Out of Sync with SQLite State

**What goes wrong:** The flag says "in transaction" but SQLite has auto-rolled-back due to `SQLITE_FULL`, `SQLITE_IOERR`, `SQLITE_NOMEM`, or similar. Subsequent operations silently run in autocommit mode, each committing individually. The user believes they are in a transaction when they are not.
**Why it happens:** SQLite can automatically rollback a transaction on certain errors without any C API notification. The only way to detect this is `sqlite3_get_autocommit()`.
**Consequences:** Silent data integrity violations. Operations that should be atomic are committed individually. Partial state on crash.
**Prevention:** Two approaches:
1. **(STACK.md recommendation):** Use `sqlite3_get_autocommit()` directly instead of a flag. Always authoritative.
2. **(Alternative):** Use a flag but also check `sqlite3_get_autocommit()` in `commit()` and `rollback()` to detect and handle auto-rollback. If auto-rollback is detected, clear the flag and throw with a descriptive error.
**Detection:** This is hard to trigger in tests because the auto-rollback errors require disk-level conditions. Consider adding a safety check in `commit()`: if `sqlite3_get_autocommit()` returns non-zero but the flag is set, the transaction was auto-rolled-back.

## Moderate Pitfalls

### Pitfall 4: Not Cleaning Up Benchmark Files

**What goes wrong:** Benchmark test creates `.db` files, leaves them on disk. Subsequent test runs may use stale data or hit "table already exists" errors.
**Prevention:** Use `std::filesystem::remove()` in test cleanup (or use a unique temp directory). Consider wrapping cleanup in a RAII guard or GoogleTest `TearDown`.

### Pitfall 5: Forgetting to Update FFI Generator After C API Changes

**What goes wrong:** C API header has new functions, but Julia and Dart FFI bindings are stale. Tests pass for C++ and C API but fail for bindings.
**Why it happens:** The FFI generators (`bindings/julia/generator/generator.bat`, `bindings/dart/generator/generator.bat`) must be run manually after C API changes.
**Prevention:** Run generators as part of the implementation step. Add a note in the milestone checklist.
**Detection:** Binding tests will fail with "symbol not found" or similar FFI errors.

### Pitfall 6: Testing Only Happy Path Nesting

**What goes wrong:** Tests verify that `create_element` works inside a user transaction (happy path) but miss edge cases: error inside user transaction, double commit, rollback-then-operations, commit-after-auto-rollback.
**Prevention:** Write explicit tests for:
- `begin` -> `create_element` (success) -> `create_element` (fail) -> `rollback` -> verify first element is gone
- `begin` -> `create_element` -> `commit` -> `commit` (should error)
- `begin` -> `rollback` -> `create_element` (should work in autocommit mode)
- `begin` -> `begin` (should error)
**Detection:** Code review checklist should include "did we test error paths within user transactions?"

### Pitfall 7: Benchmark Test Asserting Specific Speedup Ratios

**What goes wrong:** Test asserts `EXPECT_GT(ratio, 10.0)` but fails on CI because CI disk is an SSD with low fsync latency, or the machine is under load.
**Why it happens:** Absolute speedup ratios depend on hardware (SSD vs HDD), OS (Windows vs Linux fsync behavior), and system load.
**Prevention:** Keep assertions loose: `EXPECT_LT(with_txn_ms, without_txn_ms)` (batched is faster, period). Print the actual ratio for human inspection but do not gate CI on it. Or mark the benchmark test with a `DISABLED_` prefix if it is too flaky.
**Detection:** Flaky test failures in CI that pass locally.

## Minor Pitfalls

### Pitfall 8: Dart Extension Methods vs Class Methods

**What goes wrong:** Transaction methods defined as Dart extension methods are not visible when importing the library, or conflict with existing method names.
**Prevention:** Use `part`/`part of` pattern (consistent with existing Dart binding structure) rather than standalone extension files. Verify exports in the library barrel file.

### Pitfall 9: Lua `db:commit()` vs `db.commit()`

**What goes wrong:** Lua users call `db.commit()` (dot syntax) instead of `db:commit()` (colon syntax), failing to pass `self`.
**Prevention:** This is a standard Lua convention issue, not something we can prevent in the API. Document in examples using colon syntax. sol2's usertype binding handles this correctly by default.

### Pitfall 10: Logging Noise from No-Op TransactionGuard

**What goes wrong:** Every internal operation inside a user transaction logs "TransactionGuard: skipping BEGIN (user transaction active)" at DEBUG level. For a batch of 1000 operations, that is 1000 log lines saying the same thing.
**Prevention:** Use `trace` level for the no-op message, or omit it entirely. The `begin_transaction()` and `commit()` methods already log at `debug`/`info` level, which is sufficient.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| C++ Core (TransactionGuard) | Pitfall 2: Guard rollback inside user txn | No-op guard when user txn active. Exception propagation only. |
| C++ Core (Flag vs autocommit) | Pitfall 3: Flag out of sync | Use `sqlite3_get_autocommit()` or add sync check in commit/rollback |
| C API | Pitfall 5: Stale FFI bindings | Run generators after C API changes |
| Benchmark | Pitfall 1: In-memory DB | Use file-based DB |
| Benchmark | Pitfall 7: Flaky ratio assertions | Use loose assertions, print ratios for human review |
| Julia/Dart/Lua bindings | Pitfall 6: Happy-path-only testing | Write error-path tests in each binding |
| All tests | Pitfall 4: Leftover .db files | Clean up temp files in test teardown |

## Sources

- SQLite auto-rollback behavior: https://www.sqlite.org/c3ref/get_autocommit.html (documents SQLITE_FULL, SQLITE_IOERR, SQLITE_NOMEM, SQLITE_BUSY, SQLITE_INTERRUPT auto-rollback)
- SQLite transaction nesting constraint: https://www.sqlite.org/lang_transaction.html ("Transactions created using BEGIN...COMMIT do not nest")
- SQLite SAVEPOINT caveats: https://www.sqlite.org/lang_savepoint.html (inner commits not final, data only committed on outermost release)
- Existing codebase: `src/database_impl.h` (current TransactionGuard implementation), `src/database_create.cpp` (12+ TransactionGuard usage sites)

---
*Pitfalls research for: Quiver v0.3 -- Explicit Transaction Control*
*Researched: 2026-02-20*
