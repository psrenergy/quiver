# Architecture Research

**Domain:** Explicit transaction control for SQLite wrapper library
**Researched:** 2026-02-20
**Confidence:** HIGH

## System Overview

Current architecture with transaction integration points marked:

```
Bindings Layer (Julia, Dart, Lua)
  |  Thin wrappers calling C API
  |
C API Layer (src/c/)
  |  Flat functions, quiver_error_t returns, thread-local errors
  |  NEW: quiver_database_begin_transaction / commit / rollback
  |
C++ Public API (include/quiver/database.h)
  |  Database class with Pimpl
  |  CHANGE: begin_transaction / commit / rollback become public
  |  NEW: in_transaction() query method
  |
C++ Implementation (src/database_impl.h)
  |  Database::Impl owns sqlite3*, Schema, TypeValidator
  |  CHANGE: TransactionGuard checks user transaction state
  |  NEW: bool user_transaction_active_ flag on Impl
  |
SQLite (sqlite3*)
  |  sqlite3_get_autocommit() for state detection
```

## Integration Design

### Core Mechanism: User Transaction Flag on Impl

Add a `bool user_transaction_active_` member to `Database::Impl`. This flag distinguishes user-initiated transactions from internal ones. The flag is set by the public `begin_transaction()`, cleared by `commit()`/`rollback()`, and checked by `TransactionGuard`.

This is preferred over relying solely on `sqlite3_get_autocommit()` because:
1. `sqlite3_get_autocommit()` cannot distinguish between user transactions and internal TransactionGuard transactions
2. A flag gives explicit control and clear semantics
3. It avoids edge cases with `BEGIN DEFERRED` where autocommit may not flip immediately

```cpp
// database_impl.h -- modified Impl struct
struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    std::unique_ptr<Schema> schema;
    std::unique_ptr<TypeValidator> type_validator;
    bool user_transaction_active_ = false;  // NEW

    // ... existing helpers unchanged ...

    // Existing begin_transaction/commit/rollback unchanged
    // They still execute raw SQL against sqlite3*
};
```

### TransactionGuard Becomes Nest-Aware

The only modification to `TransactionGuard`: check `user_transaction_active_` on construction. If a user transaction is already active, the guard becomes a no-op (does not BEGIN, does not COMMIT/ROLLBACK on destruction).

```cpp
class TransactionGuard {
    Impl& impl_;
    bool committed_ = false;
    bool is_noop_ = false;  // NEW

public:
    explicit TransactionGuard(Impl& impl) : impl_(impl) {
        if (impl_.user_transaction_active_) {
            is_noop_ = true;  // User transaction covers us
        } else {
            impl_.begin_transaction();
        }
    }

    void commit() {
        if (!is_noop_) {
            impl_.commit();
        }
        committed_ = true;
    }

    ~TransactionGuard() {
        if (!committed_ && !is_noop_) {
            impl_.rollback();
        }
    }

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
};
```

**Key property:** When `user_transaction_active_` is false, behavior is identical to today. No existing code paths change.

### Public API on Database Class

Move `begin_transaction()`, `commit()`, `rollback()` from private to public in `database.h`. Add `in_transaction()` query. The existing private implementations delegate to `Impl`; the only new logic is the flag management.

```cpp
// database.h -- new public methods
class Database {
public:
    // ... existing public methods ...

    // Transaction control
    void begin_transaction();
    bool in_transaction() const;
    void commit();
    void rollback();

    // ... rest unchanged, remove these from private section ...
};
```

```cpp
// database.cpp -- modified implementations
void Database::begin_transaction() {
    if (impl_->user_transaction_active_) {
        throw std::runtime_error(
            "Cannot begin_transaction: a transaction is already active");
    }
    impl_->begin_transaction();
    impl_->user_transaction_active_ = true;
}

bool Database::in_transaction() const {
    return impl_->user_transaction_active_;
}

void Database::commit() {
    if (!impl_->user_transaction_active_) {
        throw std::runtime_error(
            "Cannot commit: no active transaction");
    }
    impl_->commit();
    impl_->user_transaction_active_ = false;
}

void Database::rollback() {
    if (!impl_->user_transaction_active_) {
        throw std::runtime_error(
            "Cannot rollback: no active transaction");
    }
    impl_->rollback();
    impl_->user_transaction_active_ = false;
}
```

**Error messages follow established patterns:** "Cannot {operation}: {reason}"

**Note:** The internal callers in `database.cpp` (`migrate_up`, `apply_schema`) use `impl_->begin_transaction()` / `impl_->commit()` / `impl_->rollback()` directly on the Impl, bypassing the flag. This is correct -- those are internal operations that should not interact with user transaction state.

### Component Responsibilities

| Component | Responsibility | What Changes |
|-----------|---------------|--------------|
| `Database::Impl` (database_impl.h) | Owns sqlite3*, executes raw transaction SQL | Add `user_transaction_active_` flag |
| `TransactionGuard` (database_impl.h) | RAII for internal method transactions | Check flag, become no-op when set |
| `Database` (database.h) | Public API surface | Move txn methods from private to public, add `in_transaction()` |
| `Database` (database.cpp) | Public method implementations | Add flag management and precondition checks |
| C API (src/c/database.h, database.cpp) | FFI surface | Add 4 new functions |
| Julia binding (database.jl) | Julia wrapper | Add 4 new functions |
| Dart binding (database.dart) | Dart wrapper | Add 4 new methods |
| Lua binding (lua_runner.cpp) | Lua userdata methods | Add 4 new bindings |

## C API Integration

Three new functions plus one query, all in `src/c/database.cpp` (lifecycle file). They follow the exact same pattern as `quiver_database_describe` -- simple delegation, no output parameters beyond error code.

```c
// include/quiver/c/database.h -- new declarations
QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, int* out_active);
QUIVER_C_API quiver_error_t quiver_database_commit(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_rollback(quiver_database_t* db);
```

```cpp
// src/c/database.cpp -- implementations
QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db) {
    QUIVER_REQUIRE(db);
    try {
        db->db.begin_transaction();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, int* out_active) {
    QUIVER_REQUIRE(db, out_active);
    *out_active = db->db.in_transaction() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_commit(quiver_database_t* db) {
    QUIVER_REQUIRE(db);
    try {
        db->db.commit();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_rollback(quiver_database_t* db) {
    QUIVER_REQUIRE(db);
    try {
        db->db.rollback();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

These belong in `src/c/database.cpp` alongside other lifecycle functions (open, close, describe) because transactions are database-level lifecycle operations, not CRUD.

## Binding Integration Patterns

### Julia Binding

New functions in `bindings/julia/src/database.jl` (lifecycle file, consistent with `close!`, `describe`).

```julia
# bindings/julia/src/database.jl -- new functions
function begin_transaction!(db::Database)
    check(C.quiver_database_begin_transaction(db.ptr))
    return nothing
end

function in_transaction(db::Database)
    out_active = Ref{Cint}(0)
    check(C.quiver_database_in_transaction(db.ptr, out_active))
    return out_active[] != 0
end

function commit!(db::Database)
    check(C.quiver_database_commit(db.ptr))
    return nothing
end

function rollback!(db::Database)
    check(C.quiver_database_rollback(db.ptr))
    return nothing
end
```

FFI declarations in `bindings/julia/src/c_api.jl` (auto-generated by running `bindings/julia/generator/generator.bat`).

**Naming convention:** `begin_transaction!`, `commit!`, `rollback!` use `!` suffix because they mutate database state. `in_transaction` is a query, no `!`.

### Dart Binding

New methods on the `Database` class. These could go in `database.dart` directly (lifecycle) or in a new `database_transaction.dart` part file for organizational clarity. Given existing convention (create/read/update/delete each have their own part file), a dedicated `database_transaction.dart` is consistent.

```dart
// bindings/dart/lib/src/database_transaction.dart
part of 'database.dart';

extension DatabaseTransaction on Database {
  void beginTransaction() {
    _ensureNotClosed();
    check(bindings.quiver_database_begin_transaction(_ptr));
  }

  bool get isInTransaction {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outActive = arena<Int>();
      check(bindings.quiver_database_in_transaction(_ptr, outActive));
      return outActive.value != 0;
    } finally {
      arena.releaseAll();
    }
  }

  void commit() {
    _ensureNotClosed();
    check(bindings.quiver_database_commit(_ptr));
  }

  void rollback() {
    _ensureNotClosed();
    check(bindings.quiver_database_rollback(_ptr));
  }
}
```

Add `part 'database_transaction.dart';` to `database.dart`.

FFI declarations in `bindings/dart/lib/src/ffi/bindings.dart` (auto-generated by running `bindings/dart/generator/generator.bat`).

**Naming convention:** `beginTransaction`, `commit`, `rollback` (camelCase). `isInTransaction` as a getter is idiomatic Dart.

### Lua Binding

New methods on the Database userdata in `src/lua_runner.cpp`, added to the `bind_database()` call.

```cpp
// In bind_database(), add to the new_usertype call:
"begin_transaction",
[](Database& self) { self.begin_transaction(); },
"in_transaction",
[](Database& self) { return self.in_transaction(); },
"commit",
[](Database& self) { self.commit(); },
"rollback",
[](Database& self) { self.rollback(); },
```

**Naming convention:** `begin_transaction`, `in_transaction`, `commit`, `rollback` -- 1:1 match with C++ names, consistent with all other Lua bindings.

## Cross-Layer Naming Summary

| C++ | C API | Julia | Dart | Lua |
|-----|-------|-------|------|-----|
| `begin_transaction()` | `quiver_database_begin_transaction()` | `begin_transaction!()` | `beginTransaction()` | `begin_transaction()` |
| `in_transaction()` | `quiver_database_in_transaction()` | `in_transaction()` | `isInTransaction` (getter) | `in_transaction()` |
| `commit()` | `quiver_database_commit()` | `commit!()` | `commit()` | `commit()` |
| `rollback()` | `quiver_database_rollback()` | `rollback!()` | `rollback()` | `rollback()` |

## Data Flow

### Without User Transaction (Unchanged)

```
caller: db.create_element("Items", elem)
    |
    v
Database::create_element()
    |
    v
TransactionGuard(*impl_)
    |-- impl_->user_transaction_active_ == false
    |-- Executes BEGIN TRANSACTION
    |
    v
... SQL operations ...
    |
    v
txn.commit()  -->  COMMIT
    |
    v
return element_id
```

### With User Transaction (New)

```
caller: db.begin_transaction()
    |-- impl_->begin_transaction()  -->  BEGIN TRANSACTION
    |-- impl_->user_transaction_active_ = true
    |
    v
caller: db.create_element("Items", elem1)
    |
    v
Database::create_element()
    |
    v
TransactionGuard(*impl_)
    |-- impl_->user_transaction_active_ == true
    |-- is_noop_ = true  -->  NO BEGIN
    |
    v
... SQL operations ...
    |
    v
txn.commit()  -->  NO-OP (is_noop_)
    |
    v
return element_id

caller: db.update_time_series_group(...)
    |-- Same pattern: TransactionGuard is no-op
    |
    v
caller: db.commit()
    |-- impl_->commit()  -->  COMMIT (single fsync)
    |-- impl_->user_transaction_active_ = false
```

### Error During User Transaction

```
caller: db.begin_transaction()
    |
    v
caller: db.create_element(...)  -->  throws!
    |
    |-- TransactionGuard destructor fires
    |-- is_noop_ == true  -->  NO ROLLBACK (user controls this)
    |
    v
caller catches exception
    |
    v
caller: db.rollback()
    |-- impl_->rollback()  -->  ROLLBACK
    |-- impl_->user_transaction_active_ = false
```

**Critical design point:** When TransactionGuard is a no-op, it does NOT rollback on exception. The user who opened the transaction is responsible for calling `rollback()`. This is the correct behavior -- the user may want to handle the error and continue the transaction with different operations.

## Files Modified vs New

### Modified Files (8)

| File | Change |
|------|--------|
| `src/database_impl.h` | Add `user_transaction_active_` to Impl, modify TransactionGuard |
| `include/quiver/database.h` | Move txn methods from private to public, add `in_transaction()` |
| `src/database.cpp` | Add flag management in begin/commit/rollback |
| `include/quiver/c/database.h` | Add 4 C API function declarations |
| `src/c/database.cpp` | Add 4 C API function implementations |
| `bindings/julia/src/database.jl` | Add 4 Julia wrapper functions |
| `bindings/julia/src/c_api.jl` | Add FFI declarations (via generator) |
| `src/lua_runner.cpp` | Add 4 Lua bindings to bind_database() |

### New Files (1)

| File | Purpose |
|------|---------|
| `bindings/dart/lib/src/database_transaction.dart` | Dart transaction extension methods |

### Auto-Generated Files (1)

| File | How |
|------|-----|
| `bindings/dart/lib/src/ffi/bindings.dart` | `bindings/dart/generator/generator.bat` |

### Test Files (4 new)

| File | Purpose |
|------|---------|
| `tests/test_database_transaction.cpp` | C++ core transaction tests |
| `tests/test_c_api_database_transaction.cpp` | C API transaction tests |
| Julia test additions | Transaction tests in existing test suite |
| Dart test additions | Transaction tests in existing test suite |

## Anti-Patterns

### Anti-Pattern 1: SAVEPOINT for Nesting

**What people do:** Use SQLite SAVEPOINTs for nested transaction support
**Why it's wrong for this project:** Adds complexity for no benefit. The user transaction already provides atomicity. Internal operations do not need their own rollback points within a user transaction -- if any step fails, the entire user transaction should fail.
**Do this instead:** No-op TransactionGuard when user transaction is active.

### Anti-Pattern 2: Checking sqlite3_get_autocommit() Instead of a Flag

**What people do:** Query SQLite autocommit state to detect nesting
**Why it's wrong:** Cannot distinguish between a user-initiated transaction and an internal TransactionGuard transaction. Also, `BEGIN DEFERRED` does not immediately flip autocommit.
**Do this instead:** Explicit `user_transaction_active_` flag on Impl.

### Anti-Pattern 3: TransactionGuard Rolling Back in No-Op Mode

**What people do:** Have the guard rollback on destruction even in no-op mode "for safety"
**Why it's wrong:** The user explicitly started the transaction and must explicitly control its outcome. An internal operation failing should propagate the exception to the user, who then decides to rollback or handle it.
**Do this instead:** No-op guard does nothing on destruction. Exception propagates to caller.

### Anti-Pattern 4: Making rollback() Silently Succeed When No Transaction

**What people do:** Make `rollback()` a no-op when no transaction is active
**Why it's wrong:** Masks caller logic errors. If a caller calls rollback without a transaction, that is a bug.
**Do this instead:** Throw with "Cannot rollback: no active transaction"

## Build Order

The implementation has strict dependencies that dictate build order:

### Phase 1: Core C++ (no downstream dependencies)

**Step 1.1:** Modify `src/database_impl.h`
- Add `bool user_transaction_active_ = false` to `Impl`
- Modify `TransactionGuard` to check the flag

**Step 1.2:** Modify `include/quiver/database.h`
- Move `begin_transaction()`, `commit()`, `rollback()` from private to public
- Add `in_transaction() const`

**Step 1.3:** Modify `src/database.cpp`
- Add flag management logic to the 3 existing method implementations
- Add `in_transaction()` implementation

**Step 1.4:** Add `tests/test_database_transaction.cpp`
- Test begin/commit, begin/rollback
- Test double-begin error
- Test commit-without-begin error
- Test rollback-without-begin error
- Test TransactionGuard no-op within user transaction
- Test that create_element + update_time_series_group in one user transaction produces one commit
- Test error propagation: exception in second operation, then rollback

**Rationale:** Core must be correct before any downstream layer. Tests verify the nesting behavior that all other layers depend on.

### Phase 2: C API (depends on Phase 1)

**Step 2.1:** Add declarations to `include/quiver/c/database.h`

**Step 2.2:** Add implementations to `src/c/database.cpp`

**Step 2.3:** Add `tests/test_c_api_database_transaction.cpp`
- Same test scenarios as C++ but through C API
- Verify error codes and error messages

**Rationale:** C API must exist before bindings. Bindings are generated from C API headers.

### Phase 3: Bindings (depends on Phase 2, can be parallelized)

**Step 3.1 (Julia):**
- Regenerate `bindings/julia/src/c_api.jl` via generator
- Add wrapper functions to `bindings/julia/src/database.jl`
- Add tests to Julia test suite

**Step 3.2 (Dart):**
- Regenerate `bindings/dart/lib/src/ffi/bindings.dart` via generator
- Create `bindings/dart/lib/src/database_transaction.dart`
- Add `part 'database_transaction.dart';` to `database.dart`
- Add tests to Dart test suite

**Step 3.3 (Lua):**
- Add 4 bindings to `bind_database()` in `src/lua_runner.cpp`
- Add tests via LuaRunner in C++ test suite

**Rationale:** All three bindings depend only on the C API (Phase 2). They have no dependencies on each other. Julia and Dart require FFI regeneration. Lua binds directly to the C++ API through sol2.

### Phase 4: Benchmark (depends on Phase 1)

**Step 4.1:** Write C++ benchmark comparing:
- N iterations of `create_element` + `update_time_series_group` without user transaction (2N fsyncs)
- Same operations wrapped in single user transaction (1 fsync)

**Rationale:** Benchmark validates the performance motivation for the feature. Only depends on C++ core.

### Dependency Graph

```
Phase 1: C++ Core + Tests
    |
    v
Phase 2: C API + Tests
    |
    +---> Phase 3.1: Julia Binding + Tests
    |
    +---> Phase 3.2: Dart Binding + Tests
    |
    +---> Phase 3.3: Lua Binding + Tests
    |
    +---> Phase 4: Benchmark
```

## Sources

- [SQLite Test For Auto-Commit Mode](https://sqlite.org/c3ref/get_autocommit.html) -- Official documentation for `sqlite3_get_autocommit()`
- [SQLite Transaction Documentation](https://www.sqlite.org/lang_transaction.html) -- Transaction semantics, DEFERRED behavior
- Existing codebase: `src/database_impl.h`, `include/quiver/database.h`, `src/database.cpp`, `src/c/database.cpp`, `bindings/julia/src/database.jl`, `bindings/dart/lib/src/database.dart`, `src/lua_runner.cpp`

---
*Architecture research for: Explicit transaction control in Quiver SQLite wrapper*
*Researched: 2026-02-20*
