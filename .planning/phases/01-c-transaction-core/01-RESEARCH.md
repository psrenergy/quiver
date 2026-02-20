# Phase 1: C++ Transaction Core - Research

**Researched:** 2026-02-20
**Domain:** Explicit user-controlled transactions in C++ SQLite wrapper with existing internal RAII transactions
**Confidence:** HIGH

## Summary

Phase 1 adds public `begin_transaction()`, `commit()`, `rollback()`, and `in_transaction()` methods to the `Database` class, and makes the internal `TransactionGuard` nest-aware so that existing write methods work correctly inside an explicit user transaction.

The core mechanism is `sqlite3_get_autocommit()`, which queries SQLite's actual transaction state. When the user has called `begin_transaction()`, all internal `TransactionGuard` instances detect the active transaction via `sqlite3_get_autocommit()` and become complete no-ops (no BEGIN, no COMMIT, no ROLLBACK). This avoids the "cannot start a transaction within a transaction" error that would otherwise occur when calling any write method inside a user transaction.

The implementation modifies 3 existing files (`database.h`, `database_impl.h`, `database.cpp`), adds 1 new test file (`test_database_transaction.cpp`), and registers it in `tests/CMakeLists.txt`. No new dependencies. No new types. No SAVEPOINT complexity.

**Primary recommendation:** Use `sqlite3_get_autocommit()` exclusively for state detection (per user decision). Modify TransactionGuard with an `owns_transaction_` bool that tracks whether the guard issued BEGIN. Move the private `begin_transaction`/`commit`/`rollback` to public, add precondition checks and `in_transaction()`. Switch internal callers (`migrate_up`, `apply_schema`) to call `impl_->` directly to bypass public precondition checks.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Nesting model
- Binary flag approach using `sqlite3_get_autocommit()` -- no ref-counting, no depth tracking
- TransactionGuard checks autocommit state: if already in transaction, becomes complete no-op (no begin, no commit, no rollback on destruction)
- When TransactionGuard is a no-op and the inner operation throws, the guard silently skips rollback -- exception propagates to the caller who controls the explicit transaction

#### Error behavior
- Strict enforcement: all misuse throws, no silent no-ops
- `begin_transaction()` when already active: `"Cannot begin_transaction: transaction already active"`
- `commit()` without active transaction: `"Cannot commit: no active transaction"`
- `rollback()` without active transaction: `"Cannot rollback: no active transaction"`
- If SQLite auto-rolls-back after a failed statement, subsequent `rollback()` throws (no active transaction) -- caller must handle the original error correctly
- Dangling transactions are caller's responsibility -- Database does not auto-rollback on close. Bindings add RAII wrappers in Phase 3

#### Error check placement
- Error checks (autocommit state validation) live in the public `Database` methods, not in `Impl`
- `Impl` methods stay low-level pass-throughs to SQLite -- no validation logic there
- Clean separation: `Database` = contract enforcement, `Impl` = raw SQLite operations

#### API surface
- `in_transaction()` added now (not deferred) -- returns `bool`, wraps `sqlite3_get_autocommit()`
- Full stack propagation: C++ in Phase 1, C API in Phase 2, all bindings in Phase 3

### Claude's Discretion
- Whether `with_transaction(lambda)` helper on Impl also becomes nest-aware, based on actual usage in codebase
- Test organization and edge case coverage
- Exact logging (spdlog) messages for transaction operations

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TXN-01 | Caller can wrap multiple write operations in a single transaction via `begin_transaction` / `commit` / `rollback` on the public C++ API | Move existing private methods to public. Add precondition checks using `sqlite3_get_autocommit()`. TransactionGuard becomes no-op so internal write methods work inside user transaction. |
| TXN-02 | Existing write methods (create_element, update_*, delete_element) work correctly inside an explicit transaction without "cannot start a transaction within a transaction" errors | TransactionGuard constructor checks `sqlite3_get_autocommit(impl_.db)`. If zero (transaction active), sets `owns_transaction_ = false` and skips BEGIN. Commit and destructor skip when not owning. 12+ call sites automatically become nest-aware. |
| TXN-03 | Misusing transaction API (double begin, commit/rollback without begin) throws Quiver-pattern error messages | Public `Database::begin_transaction()` checks `!sqlite3_get_autocommit()` and throws "Cannot begin_transaction: transaction already active". Public `commit()`/`rollback()` check `sqlite3_get_autocommit()` and throw "Cannot commit/rollback: no active transaction". |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SQLite3 | 3.50.2 (already linked) | `sqlite3_get_autocommit()` for transaction state detection | Function exists since SQLite 3.0. No upgrade needed. Authoritative source of truth for transaction state. |
| GoogleTest | 1.17.0 (already linked) | Test framework for transaction tests | Already the project's test framework. No new dependency. |
| spdlog | 1.17.0 (already linked) | Logging for transaction lifecycle events | Already the project's logging framework. |

### Supporting
No new libraries needed for this phase.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `sqlite3_get_autocommit()` | `bool user_transaction_active_` flag on Impl | Flag is simpler to reason about but gets out of sync if SQLite auto-rolls-back due to `SQLITE_FULL`/`SQLITE_IOERR`. User locked the decision to use `sqlite3_get_autocommit()`. |
| `sqlite3_get_autocommit()` | `sqlite3_txn_state()` (since 3.34.0) | Provides NONE/READ/WRITE granularity. Overkill for binary active/inactive detection. |
| No-op TransactionGuard | SAVEPOINT-based nesting | SAVEPOINT adds naming complexity, RELEASE vs ROLLBACK TO semantics, and partial-state confusion for zero benefit. Explicitly out of scope. |

## Architecture Patterns

### Existing File Structure (No New Source Files)
```
include/quiver/database.h           # Move 3 methods from private to public, add in_transaction()
src/database_impl.h                 # Modify TransactionGuard (add owns_transaction_ member)
src/database.cpp                    # Add precondition checks to begin/commit/rollback, add in_transaction()
tests/test_database_transaction.cpp # NEW: transaction-specific tests
tests/CMakeLists.txt                # Register new test file
```

### Pattern 1: TransactionGuard Nest-Awareness via sqlite3_get_autocommit()
**What:** TransactionGuard constructor queries `sqlite3_get_autocommit(impl_.db)`. If zero (already in transaction), it sets `owns_transaction_ = false` and skips BEGIN. Commit() and destructor check the flag before issuing COMMIT/ROLLBACK.
**When to use:** Every TransactionGuard construction. This is the universal mechanism.
**Example:**
```cpp
// Source: SQLite docs https://sqlite.org/c3ref/get_autocommit.html + codebase analysis
class TransactionGuard {
    Impl& impl_;
    bool committed_ = false;
    bool owns_transaction_ = false;

public:
    explicit TransactionGuard(Impl& impl) : impl_(impl) {
        if (sqlite3_get_autocommit(impl_.db)) {
            // No active transaction -- we own this one
            impl_.begin_transaction();
            owns_transaction_ = true;
        }
        // else: already in a transaction, become a no-op
    }

    void commit() {
        if (owns_transaction_) {
            impl_.commit();
        }
        committed_ = true;
    }

    ~TransactionGuard() {
        if (!committed_ && owns_transaction_) {
            impl_.rollback();
        }
    }

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
};
```

**Key property:** When no user transaction exists, behavior is identical to today's code. Zero impact on existing code paths.

### Pattern 2: Public Transaction Methods with Precondition Checks
**What:** Public `Database::begin_transaction()`, `commit()`, `rollback()` check `sqlite3_get_autocommit()` before delegating to `impl_->`. The `in_transaction()` method wraps the same check.
**When to use:** All 4 new public methods.
**Example:**
```cpp
// Source: Codebase convention (CLAUDE.md error patterns) + user decision (CONTEXT.md)
void Database::begin_transaction() {
    if (!sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot begin_transaction: transaction already active");
    }
    impl_->begin_transaction();
    impl_->logger->debug("User transaction started");
}

bool Database::in_transaction() const {
    return !sqlite3_get_autocommit(impl_->db);
}

void Database::commit() {
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot commit: no active transaction");
    }
    impl_->commit();
    impl_->logger->debug("User transaction committed");
}

void Database::rollback() {
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot rollback: no active transaction");
    }
    impl_->rollback();
    impl_->logger->debug("User transaction rolled back");
}
```

### Pattern 3: Internal Callers Bypass Public Checks
**What:** `migrate_up()` and `apply_schema()` currently call `this->begin_transaction()` / `this->commit()` / `this->rollback()` (the public methods). After adding precondition checks, these internal callers must switch to `impl_->begin_transaction()` / `impl_->commit()` / `impl_->rollback()` to bypass the contract enforcement.
**Why:** The public methods enforce "no double begin" / "no commit without begin" for user transactions. Internal methods like `migrate_up` are called during factory construction before any user interaction, so the checks would pass today. But conceptually, internal operations should bypass user-facing contract enforcement. If a user were to call `from_schema()` while a transaction is active (which would be a programming error), the internal `impl_->` calls would let SQLite handle the double-begin error natively.
**Example:**
```cpp
// database.cpp -- migrate_up (switch from public to impl)
void Database::migrate_up(const std::string& migrations_path) {
    // ...
    for (const auto& migration : pending) {
        // ...
        impl_->begin_transaction();  // was: begin_transaction();
        try {
            execute_raw(up_sql);
            set_version(migration.version());
            impl_->commit();          // was: commit();
        } catch (const std::exception& e) {
            impl_->rollback();         // was: rollback();
            // ...
        }
    }
}
```

### Anti-Patterns to Avoid
- **SAVEPOINT for nesting:** Adds massive complexity (name management, RELEASE vs ROLLBACK TO, partial state) with zero benefit. User explicitly excluded this.
- **`bool user_transaction_active_` flag:** Gets out of sync if SQLite auto-rolls-back on `SQLITE_FULL`/`SQLITE_IOERR`/`SQLITE_NOMEM`. `sqlite3_get_autocommit()` is always authoritative. User locked the `sqlite3_get_autocommit()` decision.
- **TransactionGuard rolling back in no-op mode:** When the guard does not own the transaction, it must not touch it at all. Exception propagates to the user who controls the outer transaction.
- **Silent no-ops for commit/rollback without transaction:** User decided on strict enforcement. All misuse throws.
- **Auto-rollback on Database close:** User explicitly deferred this to bindings layer (Phase 3). C++ API does not auto-rollback on destruction.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Transaction state detection | Manual `bool` flag | `sqlite3_get_autocommit()` | SQLite is the single source of truth. Flag becomes stale after auto-rollback on catastrophic errors. |
| Nested transaction support | SAVEPOINT-based nesting with name management | No-op TransactionGuard | SAVEPOINT adds 10x complexity for zero practical benefit in a single-connection library. |
| Transaction timeout/retry | SQLITE_BUSY retry loop with backoff | Nothing | Single-connection library -- SQLITE_BUSY cannot occur. |

**Key insight:** The existing codebase already has 95% of the transaction infrastructure. The entire Phase 1 is a surgical modification of 3 files plus a new test file. The complexity is in getting the edge cases right, not in building new systems.

## Common Pitfalls

### Pitfall 1: Double BEGIN Crashes All Write Methods Inside User Transaction
**What goes wrong:** User calls `begin_transaction()`, then calls `create_element()`. The internal `TransactionGuard` constructor issues `BEGIN TRANSACTION;`, which SQLite rejects with "cannot start a transaction within a transaction". There are 12+ call sites that will all fail.
**Why it happens:** The existing `TransactionGuard` unconditionally issues BEGIN. SQLite flatly rejects nested BEGIN.
**How to avoid:** TransactionGuard checks `sqlite3_get_autocommit()` before issuing BEGIN. If already in transaction, becomes no-op.
**Warning signs:** Any test that calls `begin_transaction()` then any write method crashes immediately.

### Pitfall 2: TransactionGuard Rollback Destroys User's Prior Work
**What goes wrong:** Inside a user transaction, operation B fails after operation A succeeded. TransactionGuard destructor issues ROLLBACK, destroying operation A's data.
**Why it happens:** SQLite ROLLBACK terminates the entire transaction, not just the "inner" scope. There is only one transaction.
**How to avoid:** When TransactionGuard does not own the transaction (`owns_transaction_ == false`), its destructor is a no-op. Exception propagates to the user who decides whether to rollback or continue.
**Warning signs:** User calls begin, creates element A, creates element B (fails), and element A is silently lost.

### Pitfall 3: Internal Callers (migrate_up, apply_schema) Conflict with Public Precondition Checks
**What goes wrong:** `migrate_up()` calls `this->begin_transaction()` (public). If a user happens to have a transaction open (e.g., they call `from_migrations()` from within a transaction context, which is a bug but possible), the public method throws "transaction already active".
**Why it happens:** Internal callers previously called the same method without precondition checks. Adding checks to the public method affects all callers.
**How to avoid:** Switch `migrate_up()` and `apply_schema()` to call `impl_->begin_transaction()` / `impl_->commit()` / `impl_->rollback()` directly, bypassing public checks. Internal operations are not subject to user-facing contract enforcement.
**Warning signs:** Existing tests for `from_schema()` and `from_migrations()` fail after adding precondition checks.

### Pitfall 4: Auto-Rollback Makes commit()/rollback() Throw Unexpectedly
**What goes wrong:** A statement inside a user transaction triggers `SQLITE_FULL` or `SQLITE_IOERR`. SQLite automatically rolls back the transaction. The user's catch block calls `rollback()`, which throws "Cannot rollback: no active transaction" because SQLite already rolled back.
**Why it happens:** Certain catastrophic SQLite errors cause automatic rollback. After auto-rollback, `sqlite3_get_autocommit()` returns non-zero.
**How to avoid:** This is correct behavior per user decision ("If SQLite auto-rolls-back after a failed statement, subsequent `rollback()` throws -- caller must handle the original error correctly"). Document this clearly. Users should check `in_transaction()` before calling `rollback()` in error recovery paths, or simply catch and ignore the secondary error.
**Warning signs:** Confusing "no active transaction" errors after catastrophic failures.

### Pitfall 5: with_transaction(lambda) Does Not Exist in Codebase
**What goes wrong:** CLAUDE.md documentation mentions `with_transaction(lambda)` as an Impl pattern, but it does not actually exist in the codebase. A planner might create tasks to modify it.
**Why it happens:** Documentation describes patterns that were planned but not implemented.
**How to avoid:** Grep confirmed: `with_transaction` has zero matches in `src/`. The discretion item from CONTEXT.md ("Whether `with_transaction(lambda)` helper on Impl also becomes nest-aware") is moot. There is nothing to modify.
**Warning signs:** Plan references `with_transaction` modifications that do not exist.

### Pitfall 6: Test Schema Must Support Multiple Write Operations
**What goes wrong:** Transaction tests need schemas that support `create_element`, `update_vector_*`, `update_set_*`, `update_time_series_group`, etc. If the test uses `basic.sql` which has no vector/set/time series tables, half the test scenarios cannot be covered.
**Why it happens:** Different schemas support different operations. Transaction tests need a schema that covers the widest range of write operations.
**How to avoid:** Use `collections.sql` (or another schema) that includes vectors, sets, and time series tables. Verify the schema has the tables needed for all planned test cases.
**Warning signs:** Tests compile but fail at runtime because the schema lacks the required tables.

## Code Examples

### Complete TransactionGuard Modification
```cpp
// Source: database_impl.h -- verified against current codebase
class TransactionGuard {
    Impl& impl_;
    bool committed_ = false;
    bool owns_transaction_ = false;  // NEW: tracks whether we issued BEGIN

public:
    explicit TransactionGuard(Impl& impl) : impl_(impl) {
        if (sqlite3_get_autocommit(impl_.db)) {
            impl_.begin_transaction();
            owns_transaction_ = true;
        }
        // else: already in a transaction (user or internal), become a no-op
    }

    void commit() {
        if (owns_transaction_) {
            impl_.commit();
        }
        committed_ = true;
    }

    ~TransactionGuard() {
        if (!committed_ && owns_transaction_) {
            impl_.rollback();
        }
    }

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
};
```

### Complete Database Public Methods
```cpp
// Source: database.h -- move from private to public section
void begin_transaction();
bool in_transaction() const;
void commit();
void rollback();

// Source: database.cpp -- modified implementations
void Database::begin_transaction() {
    if (!sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot begin_transaction: transaction already active");
    }
    impl_->begin_transaction();
    impl_->logger->debug("User transaction started");
}

bool Database::in_transaction() const {
    return !sqlite3_get_autocommit(impl_->db);
}

void Database::commit() {
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot commit: no active transaction");
    }
    impl_->commit();
    impl_->logger->debug("User transaction committed");
}

void Database::rollback() {
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot rollback: no active transaction");
    }
    impl_->rollback();
    impl_->logger->debug("User transaction rolled back");
}
```

### migrate_up / apply_schema Internal Caller Fix
```cpp
// Source: database.cpp -- switch to impl_-> to bypass public checks
// In migrate_up():
impl_->begin_transaction();  // was: begin_transaction();
try {
    execute_raw(up_sql);
    set_version(migration.version());
    impl_->commit();          // was: commit();
} catch (const std::exception& e) {
    impl_->rollback();         // was: rollback();
    // ...
}

// In apply_schema():
impl_->begin_transaction();  // was: begin_transaction();
try {
    execute_raw(schema_sql);
    impl_->load_schema_metadata();
    impl_->commit();          // was: commit();
} catch (const std::exception& e) {
    impl_->rollback();         // was: rollback();
    // ...
}
```

### Test Pattern: Begin-Write-Commit Lifecycle
```cpp
// Source: project test conventions from test_database_create.cpp, test_database_update.cpp
TEST(DatabaseTransaction, BeginMultipleWritesCommit) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Setup: create initial element
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Begin explicit transaction
    db.begin_transaction();
    EXPECT_TRUE(db.in_transaction());

    // Multiple writes inside single transaction
    quiver::Element elem1;
    elem1.set("label", std::string("Item 1"));
    auto id1 = db.create_element("Collection", elem1);

    quiver::Element elem2;
    elem2.set("label", std::string("Item 2"));
    auto id2 = db.create_element("Collection", elem2);

    db.commit();
    EXPECT_FALSE(db.in_transaction());

    // Verify both elements exist
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
}
```

### Test Pattern: Error Misuse Detection
```cpp
TEST(DatabaseTransaction, DoubleBeginThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    db.begin_transaction();

    try {
        db.begin_transaction();
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot begin_transaction: transaction already active");
    }

    db.rollback();  // cleanup
}

TEST(DatabaseTransaction, CommitWithoutBeginThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    try {
        db.commit();
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot commit: no active transaction");
    }
}
```

## Codebase-Specific Findings

### All TransactionGuard Usage Sites (12 call sites that become nest-aware)
Every one of these constructs a `TransactionGuard` and will automatically become a no-op inside a user transaction:

| File | Method | Line |
|------|--------|------|
| `src/database_create.cpp` | `create_element()` | 21 |
| `src/database_update.cpp` | `update_element()` | 18 |
| `src/database_update.cpp` | `update_vector_integers()` | 218 |
| `src/database_update.cpp` | `update_vector_floats()` | 243 |
| `src/database_update.cpp` | `update_vector_strings()` | 268 |
| `src/database_update.cpp` | `update_set_integers()` | 293 |
| `src/database_update.cpp` | `update_set_floats()` | 317 |
| `src/database_update.cpp` | `update_set_strings()` | 341 |
| `src/database_time_series.cpp` | `update_time_series_group()` | 128 |
| `src/database_time_series.cpp` | `update_time_series_files()` | 280 |

### Write Methods That Do NOT Use TransactionGuard (single-statement operations)
These methods execute a single SQL statement and do not need TransactionGuard:

| File | Method | Reason |
|------|--------|--------|
| `src/database_update.cpp` | `update_scalar_integer/float/string()` | Single UPDATE statement |
| `src/database_delete.cpp` | `delete_element()` | Single DELETE statement (CASCADE handles children) |
| `src/database_relations.cpp` | `update_scalar_relation()` | Two statements but not wrapped in TransactionGuard |

These methods work inside a user transaction without any modification (single statements auto-participate in the active transaction).

### `with_transaction(lambda)` -- Does Not Exist
Confirmed via grep: `with_transaction` has zero matches in `src/`. The CLAUDE.md mentions it as a pattern, but it was never implemented. The discretion item is moot.

### Internal Callers That Need Fixing
Two methods in `database.cpp` call the public `begin_transaction()`/`commit()`/`rollback()`:
1. `migrate_up()` (lines 325, 329, 332) -- used by `from_migrations()` factory
2. `apply_schema()` (lines 359, 363, 365) -- used by `from_schema()` factory

These must switch to `impl_->begin_transaction()` / `impl_->commit()` / `impl_->rollback()` to bypass the public precondition checks.

### Header Visibility: sqlite3.h Access
The public `Database` methods in `database.cpp` need `sqlite3_get_autocommit(impl_->db)`. Since `database.cpp` already includes `database_impl.h`, which includes `<sqlite3.h>`, and `impl_->db` is accessible, this works without any new includes.

The public `database.h` header does NOT need to include `<sqlite3.h>`. The `in_transaction()` method is declared in the header but implemented in the cpp file where sqlite3 is available. This maintains the Pimpl abstraction correctly.

### Test Schema Selection
The `collections.sql` schema supports: scalar attributes, vector tables, set tables, and time series tables. This is the best schema for transaction tests that need to exercise multiple write method types within a single transaction. The `basic.sql` schema only has scalar attributes -- sufficient for simple begin/commit/rollback tests but not for testing TransactionGuard no-op across all write method types.

### CMakeLists.txt Registration
New test file `test_database_transaction.cpp` must be added to the `quiver_tests` target in `tests/CMakeLists.txt`.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Private begin/commit/rollback on Database | Public begin/commit/rollback with precondition checks | This phase | Users can wrap multiple writes in explicit transactions |
| TransactionGuard unconditionally issues BEGIN | TransactionGuard checks autocommit state first | This phase | Write methods work inside user transactions |
| No in_transaction() query | Public in_transaction() wrapping sqlite3_get_autocommit() | This phase | Users and bindings can query transaction state |

## Open Questions

1. **Logging verbosity for no-op TransactionGuard**
   - What we know: When a TransactionGuard becomes a no-op inside a user transaction, it could log at debug/trace level or not at all
   - What's unclear: Whether logging every no-op (potentially hundreds in a batch) creates noise
   - Recommendation: Do not log no-op guards at all. The user knows they started a transaction. Logging would add noise proportional to batch size with zero diagnostic value.

2. **update_scalar_relation() is not wrapped in TransactionGuard**
   - What we know: `update_scalar_relation()` executes two SQL statements (SELECT lookup + UPDATE) without TransactionGuard
   - What's unclear: Whether this is a pre-existing bug or intentional design
   - Recommendation: Outside Phase 1 scope. The method still works inside a user transaction because both statements auto-participate. Not wrapping it internally just means the two statements are individually committed when no user transaction is active, which could leave partial state on failure.

## Sources

### Primary (HIGH confidence)
- [SQLite `sqlite3_get_autocommit()` official docs](https://sqlite.org/c3ref/get_autocommit.html) -- return value semantics, thread safety, auto-rollback detection
- [SQLite Transaction Documentation](https://www.sqlite.org/lang_transaction.html) -- BEGIN/COMMIT/ROLLBACK semantics, nesting prohibition
- Codebase analysis: `src/database_impl.h` (TransactionGuard at line 99-119), `include/quiver/database.h` (private methods at lines 197-199), `src/database.cpp` (implementations at lines 275-283, internal callers at lines 325-332 and 359-365)
- Codebase analysis: all 10 TransactionGuard usage sites across `database_create.cpp`, `database_update.cpp`, `database_time_series.cpp`
- User decisions: `.planning/phases/01-c-transaction-core/01-CONTEXT.md` -- locked `sqlite3_get_autocommit()` approach, error message patterns, check placement

### Secondary (MEDIUM confidence)
- Prior domain research: `.planning/research/PITFALLS.md` -- 9 identified pitfalls with prevention strategies
- Prior domain research: `.planning/research/STACK.md` -- stack analysis confirming no new dependencies
- Prior domain research: `.planning/research/FEATURES.md` -- feature landscape and competitor analysis
- Prior domain research: `.planning/research/ARCHITECTURE.md` -- integration design (note: recommends flag approach which was overridden by user decision to use `sqlite3_get_autocommit()`)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- zero new dependencies, all components already in project
- Architecture: HIGH -- surgical modification of 3 existing files, patterns verified against codebase
- Pitfalls: HIGH -- all 6 pitfalls identified from codebase analysis and prior domain research, with concrete prevention strategies

**Research date:** 2026-02-20
**Valid until:** 2026-03-20 (stable domain, no external dependency versioning concerns)
