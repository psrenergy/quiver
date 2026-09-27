# 53 — Move `Database::execute` into `Impl` (const) and drop the `Database&` back-references

**Batch** 6 · **Severity** medium · **Breaking** no (private members only) · **Size** M · **Layers** C++ core only (+ src/AGENTS.md)
**Depends on** none (do it before 54/55, which trim `database.h`'s private section further) · **Overlaps with** 05 (`insert_group_data`/`resolve_element_fk_labels` signatures in `database_impl.h` — if 05 landed, apply the same `Database& db` removal to its new `prepare_group_data`), 54 (`Row`/`Result` members; 54 forward-declares `Result` in `database.h`), 57/59/60 (other `database_impl.h` / `database.cpp` edits), 76 (src/AGENTS.md `query_int_rows` sentence — becomes moot here)

## Why

`execute`, the one parameterized-statement runner, is a **private member of `Database`**
(`include/quiver/database.h`, currently ~L263-267):
```cpp
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Internal helper for executing raw SQL (for migrations)
    void execute_raw(const std::string& sql);

    // Internal method for parameterized queries
    Result execute(const std::string& sql, const std::vector<Value>& parameters = {});
```
Its body only touches `impl_->db` (`src/database.cpp`, `Result Database::execute`, ~L132).
Because the helpers that need it live on `Database::Impl`, eight of them take a
`Database& db` back-reference just to call `db.execute(...)` (`src/database_impl.h`):
`require_element(..., Database& db) const` (~L100), `static lookup_id_by_label(..., Database& db)`
(~L108), `resolve_label(..., Database& db) const` (~L117), `resolve_fk_label(..., Database& db)`
(~L143), `resolve_element_fk_labels(..., Database& db)` (~L173), `update_group_rows(...,
Database& db)` (declared ~L236), `insert_rows_into_group_table(..., Database& db)` (~L244),
`insert_group_data(..., Database& db)` (~L297). Every call site passes `*this`.

And because `execute` is non-const, the const readers grew **two more statement runners**:
- `query_int_rows(sqlite3* db, ...)`, an inline free function in `src/database_impl.h` (~L27),
  used by `number_of_elements` (`src/database_read.cpp` ~L226) and four `summarize_collection` /
  describe queries (`src/database_describe.cpp` ~L273, ~L284, ~L290, ~L336);
- a hand-written `sqlite3_prepare_v2`/`step` in `Database::current_version() const`
  (`src/database.cpp` ~L223), which also reads with 32-bit `sqlite3_column_int`.

Principles: simple over complex (one statement runner), readability (`db` inside `Impl` should mean
the `sqlite3*`, not a back-pointer to the owner).

## Constraints and decisions

- Move **`execute` and `execute_raw` only**. Moving `set_version` / `migrate_up` / `migrate_down` /
  `apply_schema` into `Impl` is a separate cleanup — **not part of this plan** (maintainer note).
- `Impl::execute` is **const**: it changes no `Impl` state (the `sqlite3*` member is usable in a
  const method), so the const helpers (`require_element`, `resolve_label`) and const `Database`
  readers can call it.
- The summarize histogram cannot port verbatim: `SchemaValidator` does not require `STRICT`, so in a
  non-STRICT table an INTEGER column can hold TEXT/REAL; `get_integer(0)` then returns
  `std::nullopt` (dereferencing it is UB), and `execute` throws on a BLOB where `query_int_rows`
  silently read 0. Guard the histogram query with `WHERE typeof(col) = 'integer'` (or use
  `.value_or(0)` on the key). COUNT-only queries port directly.
- Keep `execute`'s parameter-count validation and string trimming exactly as they are (root design
  decisions "`query_*` validate parameter count" and "`Database::execute` trims every bound
  string").

## Changes

### 1. `include/quiver/database.h`

Delete the two private declarations `execute_raw` and `execute` (and their comments). Keep
`set_version`, `migrate_up`, `migrate_down`, `apply_schema`. If nothing else in the header needs
`Result`, replace `#include "quiver/result.h"` with a forward declaration `class Result;` — plan 54
does exactly that; if 54 has not landed, leave the include (Database's public API does not return
`Result`, so the forward declaration is enough; check with `grep -n "Result" include/quiver/database.h`).

### 2. `src/database_impl.h` — add the runners to `Impl`

Move the body of `Database::execute` (`src/database.cpp` ~L132-~L220, from `sqlite3_stmt* raw_stmt =
nullptr;` to the end of the function) into:
```cpp
    // The one parameterized-statement runner. const: it only uses the sqlite3 handle, so const
    // readers (require_element, resolve_label, number_of_elements, current_version, describe*)
    // go through it too.
    Result execute(const std::string& sql, const std::vector<Value>& parameters = {}) const {
        ...body moved verbatim, `impl_->db` becomes `db`...
    }

    void execute_raw(const std::string& sql) const {
        ...body of Database::execute_raw moved verbatim, `impl_->db` becomes `db`...
    }
```
Place both right after the `sqlite3* db` member / constructor and before the first helper that uses
them. If the body needs includes that `database_impl.h` lacks (`<sqlite3.h>` is already there; check
`quiver/result.h`, `utils/string.h` for `trim`), add them.

Delete `query_int_rows` (~L26-~L50).

### 3. `src/database_impl.h` — drop the back-references

For each of the eight helpers: delete the `Database& db` parameter and change `db.execute(` to
`execute(`. `lookup_id_by_label` stops being `static` and becomes a `const` member. Inside `Impl`,
`db` now always means the `sqlite3*` member again. Then fix every call site (`grep -n "\*this" src/database*.cpp src/database_impl.h`),
e.g. `impl_->require_element(collection, id, *this)` → `impl_->require_element(collection, id)`,
`impl_->resolve_label(collection, label, "update_element_by_label", *this)` →
`impl_->resolve_label(collection, label, "update_element_by_label")`. There are about 15 such call
sites across `database_create.cpp`, `database_update.cpp`, `database_delete.cpp`,
`database_time_series.cpp`.

### 4. `src/database*.cpp` — Database methods call `impl_->execute`

In every `src/database_*.cpp` and `src/database.cpp`, replace calls to the member `execute(` /
`execute_raw(` with `impl_->execute(` / `impl_->execute_raw(`. Counts at HEAD
(`grep -c "execute(" src/database_*.cpp`): read 21, time_series 8, csv_export 5, csv_import 4,
query 3, create 1, delete 1, update 1, plus `database.cpp` (migrations, `apply_schema`,
`set_version`, transactions). Free helpers that take `Database&` and call `db.execute` (e.g.
`build_label_to_id_map(Database& db, ...)` in `database_csv_import.cpp`, `get_db_columns(...)`)
must change to take `const Database::Impl&` or be given the `Result`; the simplest is to pass
`*impl_` and call `impl.execute(...)` — check each with `grep -n "Database& db" src/*.cpp`. Delete
`Database::execute` and `Database::execute_raw` from `src/database.cpp`.

### 5. `number_of_elements` and `current_version`

`src/database_read.cpp` (~L226):
```cpp
    return query_int_rows(impl_->db, "SELECT COUNT(*) FROM \"" + collection + "\"")[0][0];
```
→
```cpp
    return *impl_->execute("SELECT COUNT(*) FROM \"" + collection + "\"")[0].get_integer(0);
```
`src/database.cpp`, `current_version() const`: replace the hand-written prepare/step with
```cpp
    return *impl_->execute("PRAGMA user_version")[0].get_integer(0);
```
(int64 read; fixes the 32-bit `sqlite3_column_int`.) Check `Result`'s indexing API
(`include/quiver/result.h`: `operator[](size_t)` returning `const Row&`) and `Row::get_integer(size_t)`
returning `std::optional<int64_t>`.

### 6. `src/database_describe.cpp` — the four `query_int_rows` calls

Port each to `impl_->execute(sql)`, reading `*row.get_integer(i)` for COUNT results. For the value
histogram (~L284-~L300, the `distinct` / per-code counts), add `AND typeof(<col>) = 'integer'` to its
WHERE so a non-STRICT table's TEXT/REAL cells are skipped instead of dereferencing `nullopt`. Keep
the output identical for STRICT schemas (all test schemas are STRICT).

## Tests

No behaviour change for STRICT schemas; the existing suites are the net:
`./build/bin/quiver_tests.exe` (all), especially `DatabaseDescribe*`, `*Summarize*`,
`*CurrentVersion*`, `*NumberOfElements*`, `*Migration*`, and the C API suite.

Add one regression test for the histogram guard in `tests/test_database_describe.cpp`:
```cpp
TEST(DatabaseDescribe, SummarizeSkipsNonIntegerCellsInIntegerColumn) {
    // A non-STRICT collection can hold TEXT in an INTEGER column; summarize must not crash.
    // Build the schema inline in a temp file with test_utils helpers, or add a small non-STRICT
    // schema under tests/schemas/valid/ (e.g. non_strict_integer.sql) with Configuration + one
    // collection `Items(id, label, code INTEGER)` WITHOUT `STRICT`.
    ...
    db.query_string("INSERT INTO Items (label, code) VALUES ('a', 'not-a-number')");
    db.query_string("INSERT INTO Items (label, code) VALUES ('b', 1)");
    EXPECT_NO_THROW(db.summarize_collection("Items"));
}
```
Check first that `SchemaValidator` accepts a non-STRICT table (the finding says it does not require
STRICT; confirm with `grep -n "STRICT" src/schema_validator.cpp`). If it rejects it, drop this test
and keep the `typeof` guard anyway as documented defense for pre-existing databases — or skip the
guard; state which.

## Docs and changelog

- `src/AGENTS.md`:
  - the `describe*` bullet: delete "These const methods run their own read-only SQL via an
    anon-namespace `query_int_rows` helper that prepares/steps directly on `impl_->db` (the
    `current_version() const` pattern — `execute()` is non-const)." → "They run their SQL through
    `Impl::execute`, which is const."
  - "Group inserts are unified" bullet: `insert_rows_into_group_table(caller, table, type, columns,
    id, delete_existing, db)` → drop `, db`.
  - "Label→id resolution has one query" bullet: `Impl::lookup_id_by_label(table, label, db)` →
    `Impl::lookup_id_by_label(table, label)`.
  - "`execute` validates parameter count (`database.cpp`)" → "(`Impl::execute`, `database_impl.h`)".
- No CHANGELOG entry (internal), unless the summarize guard changes visible output for non-STRICT
  tables (it only prevents UB) — then one `### Fixed` line.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. `scripts/test-all.bat` (bindings exercise every reader)
4. `scripts/format.bat`; `scripts/tidy.bat` if it works (plan 79 fixes it)

## Acceptance criteria

- [ ] `Database` has no `execute`/`execute_raw` members; `Impl::execute` is const.
- [ ] No `Database& db` parameter remains on an `Impl` helper; no `*this` passed to one.
- [ ] `query_int_rows` and the hand-written `current_version` statement are gone.
- [ ] All suites green; src/AGENTS.md updated.

## Pitfalls

- `execute` returns a `Result` holding copied values, so no lifetime issue moving it.
- Free functions in `database_csv_import.cpp`/`_export.cpp` that take `Database&` only to call
  `execute` must switch to the `Impl` — they cannot reach a private `Impl` member through `Database`
  otherwise. `Database::Impl` is visible to them because they include `database_impl.h`.
- Plan 05 (if landed) added `prepare_group_data(..., Database& db)`; strip it too.

## Out of scope

- Moving migrations/apply_schema/set_version into `Impl`.
- Removing `Row`/`Result` public members (plan 54) or headers (plan 55).
