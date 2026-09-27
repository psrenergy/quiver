# 60 — `Impl::exec` for the `sqlite3_exec` blocks; `TransactionGuard` replaces the hand-rolled transactions

**Batch** 6 · **Severity** low · **Breaking** no (same messages; a failed BEGIN inside migrate/import is now wrapped by that operation's message) · **Size** M · **Layers** C++ core only (+ src/CLAUDE.md)
**Depends on** 53 (moves `execute_raw` into `Impl`), 58 (leaves `import_csv` with **one** write tail — this plan then edits one block instead of two), 01 (removes the `PRAGMA foreign_keys` toggles in import) · **Overlaps with** 63 (rewords the migrate/apply_schema error messages — land 60 first or merge carefully: both touch `migrate_up`/`migrate_down`/`apply_schema`)

## Why

`src/database_impl.h` and `src/database.cpp` spell the `sqlite3_exec` + error-message + free block
out **five times**: in `Impl::begin_transaction`, `Impl::commit` and `Impl::rollback`
(`database_impl.h`, currently ~L368-401), in `Database::execute_raw` (`database.cpp` ~L388-396), and
in `Database::set_version` (`database.cpp` ~L305-315). Each has this shape:

```cpp
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to <what>: " + error);
    }
```

Next to the RAII `Impl::TransactionGuard` (`database_impl.h`, ~L403-431, nest-aware: it no-ops
inside an open transaction), five blocks still hand-roll begin, commit and rollback-in-catch.
These are `migrate_up`, `migrate_down` and `apply_schema` (`database.cpp` ~L425-436, ~L463-474,
~L494-503) and both `import_csv` write blocks (one block after plan 58). Their shape:

```cpp
        impl_->begin_transaction();
        try {
            ...;
            impl_->commit();
        } catch (const std::exception& e) {
            impl_->rollback();
            ...log / rethrow...
        }
```

Principles: simplicity and RAII ("RAII used strictly", root CLAUDE.md).

## Constraints and decisions

- Keep every current message. `tests/test_migrations.cpp` (~L238, ~L255) pins the `execute_raw`
  prefix `Failed to execute SQL: ...`. `set_version` keeps its own wording.
- `rollback` keeps its log-and-continue body. It is called while an exception is already in flight
  and must not throw.
- **Declare the guard inside the `try`**, so its destructor rolls back during unwinding **before**
  the `catch` runs. This matters in `import_csv` if its catch still does anything that must run
  after the rollback, such as a `PRAGMA foreign_keys = ON` if plan 01 has not removed it.
  `PRAGMA foreign_keys` is a no-op inside a transaction.
- A `TransactionGuard` no-ops inside a caller-owned transaction. `migrate_up`/`migrate_down`/
  `apply_schema` run inside `from_migrations`/`from_schema`/`validate_migrations` on a fresh handle
  with no open transaction, so the guard always owns its transaction there. `import_csv` refuses to
  run inside a transaction (precondition), so the same holds.
- One accepted consequence: a failed `BEGIN` now happens inside the `try`, so in
  `migrate_up`/`migrate_down` it is wrapped as `Failed to migrate_up: migration N: Failed to begin
  transaction: ...`. That is harmless.
- Leave the constructor's unchecked `PRAGMA foreign_keys = ON` (`database.cpp`, ~L117) alone.
  Routing it through `exec` would change behaviour on failure.

## Changes

### 1. `Impl::exec` (`src/database_impl.h`, before `begin_transaction`)

```cpp
    // The one sqlite3_exec runner for statements with no parameters or result: throws
    // Pattern 3 "Failed to <what>: <sqlite message>".
    void exec(const std::string& sql, const char* what) const {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error(std::string("Failed to ") + what + ": " + error);
        }
    }
```
Copy the exact fallback text of the current blocks (`"Unknown error"` or whatever they use) so
messages stay byte-identical.

### 2. Route the callers through it

- `Impl::begin_transaction`: `exec("BEGIN TRANSACTION;", "begin transaction");`. Keep the exact
  SQL text and `what` each one uses today, e.g. `BEGIN IMMEDIATE` if that is what the code runs;
  read each body before replacing it. Keep any logging lines.
- `Impl::commit`: `exec("COMMIT;", "commit transaction");`
- `Impl::rollback`: **unchanged** (log-and-continue).
- `execute_raw` (an `Impl` member after plan 53): `exec(sql, "execute SQL");`
- `Database::set_version`: `impl_->exec("PRAGMA user_version = " + std::to_string(version) + ";", "set user_version");`.
  Match its current `what` text.

### 3. `migrate_up` / `migrate_down` / `apply_schema` (`src/database.cpp`)

Each becomes:
```cpp
        try {
            Impl::TransactionGuard txn(*impl_);
            ...the existing body between begin and commit, unchanged...
            txn.commit();
        } catch (const std::exception& e) {
            ...the existing log + rethrow/re-wrap, unchanged, minus the impl_->rollback() call...
        }
```
Check `TransactionGuard`'s constructor argument and its `commit()` spelling
(`grep -n "struct TransactionGuard" -A30 src/database_impl.h`).

### 4. `import_csv`'s write tail (`src/database_csv_import.cpp`)

In the one `try` plan 58 left (or in each of the two blocks if 58 has not landed):
```cpp
    try {
        Impl::TransactionGuard txn(*impl_);
        ...write steps...
        txn.commit();
    } catch (const std::exception& e) {
        // The guard's destructor has already rolled back.
        ...the UNIQUE-constraint -> "duplicate entries" rewrite, then rethrow...
    }
```
Delete `impl_->begin_transaction()`, `impl_->commit()` and `impl_->rollback()` there. If plan 01
has not landed and the catch still runs `execute_raw("PRAGMA foreign_keys = ON")`, keep that line.
It now runs after the guard's rollback, which is the order it needs.

Update the precondition comment at the top of `import_csv` if plan 01 made it cite
`impl_->begin_transaction()`. It now cites the guard. The precondition still holds, because the
guard would silently no-op inside a caller's transaction and import must own its transaction.

## Tests

No behaviour change. These existing tests are the net:
- `tests/test_migrations.cpp`: the pinned `Failed to execute SQL` messages, up/down failures and
  rollback on a failed migration.
- `tests/test_database_transaction.cpp`: `ImportCsvStillRefusesToNest` and the dry-run tests.
- The CSV import suites: a failed import leaves the table unchanged, and the duplicate-entries
  message.

Add one test pinning that a failed migration leaves no partial schema, if none exists
(`grep -n "rollback\|partial" tests/test_migrations.cpp`). A migration directory whose `up.sql`
creates a table and then fails must leave `user_version` and the table absent.

## Docs and changelog

- `src/CLAUDE.md`, Transactions section: add "Every internal write that owns a transaction
  (`create_element` ..., the migrations, `apply_schema`, `import_csv`) uses `Impl::TransactionGuard`,
  declared inside its `try` so the rollback runs before the handler; `Impl::exec` is the one
  `sqlite3_exec` runner."
- No CHANGELOG entry.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=*Migration*:*Transaction*:*DryRun*:*Csv*:*CSV*`, then the full suite
3. `./build/bin/quiver_c_tests.exe`
4. `scripts/format.bat`

## Acceptance criteria

- [ ] `sqlite3_exec` appears once in `database_impl.h`/`database.cpp`, plus any deliberately
      unchecked constructor PRAGMA.
- [ ] No `impl_->begin_transaction()` / `impl_->commit()` / `impl_->rollback()` pairs remain in
      `database.cpp` or `database_csv_import.cpp`. The public `Database::begin_transaction`/`commit`/
      `rollback` keep calling the `Impl` methods.
- [ ] Suites green, messages unchanged.

## Pitfalls

- Do not replace the **public** `Database::begin_transaction`/`commit`/`rollback`. They are the
  user API and must throw on misuse. Only internal hand-rolled blocks change.
- The dry-run `end_dry_run` path calls `impl_->rollback()` directly on purpose (src/CLAUDE.md).
  Leave it.

## Out of scope

- The migration error wording (plan 63).
- SAVEPOINTs (rejected in v0.3).
