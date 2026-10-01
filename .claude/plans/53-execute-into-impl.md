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

- [x] `Database` has no `execute`/`execute_raw` members; `Impl::execute` is const.
- [x] No `Database& db` parameter remains on an `Impl` helper; no `*this` passed to one.
- [x] `query_int_rows` and the hand-written `current_version` statement are gone.
- [x] All suites green; src/AGENTS.md updated.

## Pitfalls

- `execute` returns a `Result` holding copied values, so no lifetime issue moving it.
- Free functions in `database_csv_import.cpp`/`_export.cpp` that take `Database&` only to call
  `execute` must switch to the `Impl` — they cannot reach a private `Impl` member through `Database`
  otherwise. `Database::Impl` is visible to them because they include `database_impl.h`.
- Plan 05 (if landed) added `prepare_group_data(..., Database& db)`; strip it too.

## Out of scope

- Moving migrations/apply_schema/set_version into `Impl`.
- Removing `Row`/`Result` public members (plan 54) or headers (plan 55).

## Implementation notes

Implemented on `rs/plan53` at `3ca11b8`. `git merge master` was a no-op.

### What changed

- **`include/quiver/database.h`.** The private `execute` / `execute_raw` are gone.
  `#include "quiver/result.h"` stays, because plan 54 owns the forward declaration.
- **`src/database_impl.h`.**
  - It includes `quiver/result.h` itself, which plan 54 relies on.
  - `query_int_rows` is deleted.
  - `Impl` declares `Result execute(sql, parameters = {}) const` and `void execute_raw(sql) const`.
  - Nine helpers lost `Database& db`: `require_element`, `lookup_id_by_label` (now a `const`
    member, no longer `static`), `resolve_label`, `resolve_fk_label`, `resolve_scalar_fk_labels`,
    `update_group_rows`, `insert_rows_into_group_table`, `prepare_group_data`, `insert_group_data`.
- **`src/database.cpp`.**
  - `Database::Impl::execute` and `Database::Impl::execute_raw` are defined here, side by side.
  - `current_version()` is `*impl_->execute("PRAGMA user_version")[0].get_integer(0)`.
- **Call sites.**
  - 47 member `execute(` calls became `impl_->execute(`: read 21, time_series 8, csv_export 5,
    csv_import 6, query 3, create 1, delete 1, update 1, and `current_version`.
  - 4 `execute_raw(` calls became `impl_->execute_raw(`: three in migrations and `apply_schema`, one
    in import.
  - 17 `, *this)` arguments were dropped: create 3, delete 2, update 10, time_series 2.
- **`number_of_elements`.** It goes through `impl_->execute`.
- **`summarize_collection`.**
  - All four counts go through `impl_->execute`.
  - The distinct pre-check and the histogram both filter `WHERE typeof(col) = 'integer'`, which
    replaces `IS NOT NULL`.
  - The loop uses `Result::row_count()`, not `size()`, and avoids `Row::size/at/is_null`, which plan
    54 deletes.

### Deviations and drift

- **`Impl::execute` / `execute_raw` are defined out of line in `database.cpp`, not inline in
  `database_impl.h`.** The user approved this when the plan was reviewed. It follows the precedent
  `Impl::update_group_rows` set in `database_update.cpp`. The body stayed in place (the diff is the
  signature plus `impl_->db` → `db`), the header did not gain `utils/string.h`, and the
  `database.cpp … execute` file-map line in `src/AGENTS.md` stays true.
- **9 helpers, not 8.** `resolve_element_fk_labels` is now `resolve_scalar_fk_labels`, and plan 05
  added `prepare_group_data`.
- **Left unchanged:**
  - `build_label_to_id_map(Database& db, …)` in `database_csv_import.cpp` calls the public
    `read_scalar_*`, not `execute`.
  - `get_db_columns` takes a `Result`.
  - `TransactionGuard txn(*this)` in `Impl::update_group_rows` passes the Impl's own `*this`.
- **The test was renamed and strengthened.** It is `DatabaseDescribe.SummarizeDistributionSkipsNonIntegerCells`
  and asserts `code: 3 non-null, 0 null; values {1: 1}\n`. The spec's `EXPECT_NO_THROW` would
  already have passed at HEAD: `query_int_rows` never crashed, it silently read TEXT `'not-a-number'`
  as code `0` and REAL `1.5` as `1` (`values {1: 1, 1: 1, 0: 1}`). The test failed at HEAD and
  passes after the fix.
- **The test schema was reused, not added.** `tests/schemas/valid/non_strict_vector.sql`'s `Items`
  is no longer STRICT and gained a nullable `code INTEGER`. That keeps tests/AGENTS.md's "keep the
  other `valid/` schemas STRICT" rule. `DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn`
  only writes `label`, so it is unaffected.
- **The `current_version` int64 read is tidying, not a fix.** `user_version` is a 32-bit header
  field, so `sqlite3_column_int` lost nothing. It has no CHANGELOG line, and the
  `Failed to read user_version` message is gone (no test pinned it).
- **CHANGELOG.** `v0.12.7` is tagged and the manifests are at 0.12.8, so the one `### Fixed` entry
  (the summarize distribution) opened `## [0.12.8] — unreleased`.
- **Extra docs beyond the plan's list.** Root `AGENTS.md` and `src/AGENTS.md` (the trim sentence),
  the comments in `src/utils/datetime.h` and `tests/test_database_create.cpp`, and
  `tests/AGENTS.md` (the `non_strict_vector.sql` entry) all said `Database::execute`, or described
  the schema; they now say `Impl::execute`. The `src/AGENTS.md` describe bullet also records the
  `typeof` filter.

### Verification

- **Core suites.** `quiver_tests` 1397/1397 and `quiver_c_tests` 571/571, before and after the final
  format pass.
- **`scripts/test-all.bat`** exited 0, with six steps and no CLI smoke step:

  | Suite | Result |
  |---|---|
  | C++ | 1397 |
  | C API | 571 |
  | Julia | 1575 |
  | Dart | 445 |
  | JS | 242 |
  | Python | 350 |

- **`scripts/format.bat`** exited 0.
  - clang-format only rewrapped this plan's new `summarize_collection` lines. An awkward
    `*impl_` / `->execute` wrap was then removed by splitting out `distinct_sql`.
  - Biome again rewrote 30 JS files CRLF→LF. `git diff --ignore-cr-at-eol bindings/` was empty, and
    `git checkout -- bindings/js` reverted them.
- **`scripts/tidy.bat`** cannot run on this machine: it hard-codes
  `C:\Program Files\LLVM\bin\run-clang-tidy`, which is absent (plan 79's area). Instead, Visual
  Studio's bundled `clang-tidy -p build` was run over the 10 changed `.cpp` files. It found two
  warnings, both on untouched lines: `kMaxDistributionCardinality` naming (describe.cpp:18) and a
  signed/unsigned compare (csv_import.cpp:638).
- **MSVC build.** It still reports the existing C4701 on `GroupTableType group_type;` in
  `import_csv`, a declaration this plan did not touch.
- **Greps.**
  - `Database& db` in `database_impl.h` / `database_update.cpp`: none.
  - `*this)` in `src/database_*.cpp`: only `TransactionGuard txn(*this)`.
  - `query_int_rows` / `Database::execute` in `src include tests AGENTS.md`: none.
  - `execute(` / `execute_raw` in `include/quiver/database.h`: none.

### For later plans

- **Where `execute` lives.** `Impl::execute` / `Impl::execute_raw` are *declared* in
  `src/database_impl.h` and *defined* in `src/database.cpp`. Plan 60's `exec` helper and its
  `execute_raw` rewrite anchor on `Database::Impl::execute_raw` in `database.cpp`.
- **How to call it.** Inside `Impl` it is `execute(...)`; from a `Database` method it is
  `impl_->execute(...)` / `impl_->execute_raw(...)`. No `db` or `*this` is passed to any Impl helper
  any more. Plans 56, 57, 58 and 59 should drop those arguments from their excerpts, for example
  `require_element(collection, id)`, `resolve_fk_label(table_def, column, value)` and
  `lookup_id_by_label(table, label)`.
- **Plan 54.** `database.h` no longer references `Result`. Replace its include with nothing or a
  forward declaration; `database_impl.h` already includes `quiver/result.h`.
- **Plan 76.** Step 1 (the `query_int_rows` describe sentence) is done. That bullet now says they
  run through `Impl::execute`, which is const.
- **Plans 56–64.** CHANGELOG entries go under `## [0.12.8] — unreleased`, unless `v0.12.8` gets
  tagged first.
