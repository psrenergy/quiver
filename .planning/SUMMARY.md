# Research Summary: Quiver v0.3 -- Explicit Transaction Control

**Domain:** SQLite wrapper library -- transaction management
**Researched:** 2026-02-20
**Overall confidence:** HIGH

## Executive Summary

Quiver v0.3 adds explicit transaction control (`begin_transaction`, `commit`, `rollback`) to enable users to batch multiple write operations into a single SQLite transaction, reducing fsync overhead from N per operation to 1 per batch. This is a well-understood SQLite pattern with minimal technical risk.

The most important finding is that **zero new dependencies are required**. The existing SQLite 3.50.2 provides `sqlite3_get_autocommit()` for nesting detection, the existing C++20 standard library provides `std::chrono::steady_clock` for benchmarking, and the existing GoogleTest 1.17.0 provides the test framework. The methods `begin_transaction()`, `commit()`, and `rollback()` already exist as private methods on `Database` -- the primary code change is moving them to public and making `TransactionGuard` nest-aware.

The critical design decision is how `TransactionGuard` detects an active user transaction. Two approaches emerged in research: (1) querying `sqlite3_get_autocommit()` directly, which is always authoritative but cannot distinguish user vs internal transactions, or (2) maintaining an explicit `user_transaction_active_` flag on `Impl`, which is simpler to reason about but risks getting out of sync if SQLite auto-rolls-back. Both approaches are viable; the flag approach is simpler for this single-threaded use case but should include a safety check against `sqlite3_get_autocommit()` in `commit()`/`rollback()`.

The benchmark should use file-based databases (not `:memory:`) because the performance benefit comes entirely from reduced fsync calls. A simple `std::chrono::steady_clock` measurement in a GoogleTest case is sufficient -- Google Benchmark is overkill for a single proof-of-concept test.

## Key Findings

**Stack:** No new dependencies. SQLite 3.50.2 (`sqlite3_get_autocommit`), C++20 (`std::chrono::steady_clock`), GoogleTest 1.17.0 are all already available.

**Architecture:** Methods already exist as private. Move to public, add flag/autocommit check to `TransactionGuard`, expose through C API and all bindings. ~8 modified files, 1 new file, 4 new test files.

**Critical pitfall:** `TransactionGuard` must become a complete no-op (no BEGIN, no COMMIT, no ROLLBACK) when inside a user transaction. If the guard issues ROLLBACK on exception while nested, it destroys the entire user transaction including prior successful operations.

## Implications for Roadmap

Based on research, suggested phase structure:

1. **C++ Core + Tests** - Modify `TransactionGuard`, move methods to public, add `in_transaction()`, write core tests
   - Addresses: begin/commit/rollback public API, nest-aware TransactionGuard
   - Avoids: Pitfall 2 (guard rollback in user txn) by implementing no-op pattern first

2. **C API + Tests** - Add 4 flat C functions, write C API tests
   - Addresses: FFI surface for all bindings
   - Avoids: Pitfall 5 (stale bindings) by establishing the API before bindings

3. **Bindings (parallelizable)** - Julia, Dart, Lua, each with tests
   - Addresses: All binding requirements
   - Avoids: Pitfall 6 (happy-path-only) by including error-path tests

4. **Benchmark** - Performance comparison test
   - Addresses: Proving the feature's value with measurable speedup
   - Avoids: Pitfall 1 (in-memory DB) and Pitfall 7 (flaky assertions) by using file-based DB with loose assertions

**Phase ordering rationale:**
- C++ core must be correct before C API wraps it (correctness flows outward)
- C API must exist before bindings can call it (FFI generators read C headers)
- Bindings are independent of each other (Julia, Dart, Lua can be done in parallel)
- Benchmark only depends on C++ core but is logically last (proves the completed feature)

**Research flags for phases:**
- Phase 1: The `user_transaction_active_` flag vs `sqlite3_get_autocommit()` decision should be finalized during implementation. Both work; the flag is recommended in ARCHITECTURE.md, the autocommit query is recommended in STACK.md. Either is correct.
- Phase 3: Standard pattern, unlikely to need deeper research. Follow existing binding conventions mechanically.
- Phase 4: May need adjustment based on CI hardware characteristics. Keep benchmark assertions loose.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Zero new dependencies. All APIs verified against official SQLite docs. |
| Features | HIGH | Well-understood feature set. Table stakes for any transaction API. |
| Architecture | HIGH | Methods already exist as private. TransactionGuard pattern is straightforward. |
| Pitfalls | HIGH | SQLite auto-rollback behavior verified with official docs. Benchmark pitfalls are well-documented. |

## Gaps to Address

- **Flag vs autocommit decision:** ARCHITECTURE.md recommends `user_transaction_active_` flag; STACK.md recommends `sqlite3_get_autocommit()`. Both are valid. The implementer should choose one and add a safety check for the other. This is a design decision, not a research gap.
- **`BEGIN DEFERRED` vs `BEGIN IMMEDIATE`:** Current implementation uses `BEGIN TRANSACTION` (which is `BEGIN DEFERRED`). This is correct for the motivating use case. If users report `SQLITE_BUSY` contention in multi-connection scenarios, `BEGIN IMMEDIATE` can be added as a follow-up.
- **Binding-level RAII wrappers:** Julia `transaction(db) do ... end`, Dart `db.transaction(() { ... })` -- these are convenience methods that could be deferred to a follow-up. Research indicates they are "nice to have" but not blockers for v0.3.

---
*Research summary for: Quiver v0.3 -- Explicit Transaction Control*
*Researched: 2026-02-20*
