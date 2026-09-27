# 01 — import_csv: keep foreign keys ON so dropped elements cascade

**Batch** 1 · **Severity** h · **Breaking** yes. It affects callers whose scalar-import CSV omits an element that other rows depend on through `ON DELETE CASCADE`: those rows are now deleted, as `delete_element` would delete them. It also affects schemas with a `UNIQUE` column other than `label` whose values are swapped between two kept elements in one import: that import now fails and rolls back. · **Size** M · **Layers** C++ core (fix + tests), C API (tests), Lua (test), Julia/Dart/Python/JS (one test each), docs (root `AGENTS.md`, `src/AGENTS.md`, `tests/AGENTS.md`, `bindings/js/src/lua-api.ts`, `CHANGELOG.md`), one new test schema
**Depends on** none · **Overlaps with** 58 (same function: runs later and refactors the validation and write passes and the two catch tails; it must keep everything this plan adds); 60 (turns the hand-rolled `impl_->begin_transaction/commit/rollback` into `TransactionGuard`, which would change the precondition rationale written here again); 43 (edits the same `import_csv` paragraphs of `bindings/js/src/lua-api.ts`); 53 (moves `Database::execute` into `Impl`, and this plan adds two more `execute(...)` calls in `import_csv`); 22 (the new C API tests call `quiver_database_query_integer`); 06 (enforcing CASCADE parent FKs on set and time-series tables removes one fail-closed case listed under Pitfalls); 61 (the same file's includes: this plan adds a second use of the already-included `<set>`); 75 (`tests/AGENTS.md` schema list); 69 (C API test hygiene: the new C API tests already free with the right functions)

## Why

A scalar `import_csv` (empty `group`) switches foreign keys off, deletes every row of the collection and re-inserts the CSV's rows. SQLite runs `ON DELETE CASCADE` / `SET NULL` only while `foreign_keys` is ON. Only labels present in the CSV are re-inserted with their old id. So for every element the CSV **omits**, the collection row disappears but:

- its vector, set and time-series rows stay behind, and are still readable by the old id (`read_vector_floats_by_id` does a bare `SELECT ... WHERE id = ?`);
- every relation to it keeps the deleted id. `export_csv` then writes that id as a bare number, because no label exists for it, and `import_csv` rejects the file it just wrote;
- in a collection without `AUTOINCREMENT` (for example `Configuration`, which is `id INTEGER PRIMARY KEY`), a new label can take `max(rowid)+1` and inherit the orphaned rows.

Turning `PRAGMA foreign_keys = ON` back on does not re-check existing rows. The schema's own CASCADE / SET NULL declarations and the Schema Conventions rule ("Always use `ON DELETE CASCADE ON UPDATE CASCADE` for parent references") are silently bypassed.

The current code is in `src/database_csv_import.cpp`, `Database::import_csv`. There are three toggle sites:

```cpp
// header-only branch (currently ~L363-374)
auto row_count = csv.rows.size();
if (row_count == 0) {
    execute_raw("PRAGMA foreign_keys = OFF");
    try {
        execute_raw("DELETE FROM " + table_name);
        execute_raw("PRAGMA foreign_keys = ON");
    ...
// scalar path (currently ~L454-459, ~L562-566)
        // Data import: disable FK → DELETE → INSERT → enable FK
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + collection);
...
// group path (currently ~L706-711, ~L775-779)
        execute_raw("PRAGMA foreign_keys = OFF");
```

Only CSV labels found in `existing_label_to_id` (captured at ~L391) get their old id back (~L474-480).

**Reproduced on the current build** (HEAD 58dfe7a), with `build/bin/quiver_cli.exe --schema tests/schemas/valid/csv_export.sql repro.db repro.lua`. The script creates `Item1` (vector `{1.5, 2.5}`, set `{"red"}`) and `Item2`, then imports a CSV that holds only `Item2`:

```
{"items":1,"orphan_set":1,"orphan_vec":2,"read_vec":{"measurement":[1.5,2.5]}}
```

With `relations.sql`, the script creates Parent A, Parent B and `Child 1` with `parent_id = B`, then imports a Parent CSV that holds only `Parent A`. `Child.parent_id` is still `2`. The script then runs `export_csv("Child")` followed by `import_csv` of that file, which throws:

```
Cannot import_csv: Could not find an existing element from collection Parent with label 2.
```

## Constraints and decisions

- **Maintainer decisions for this item (binding):**
  - Write every row with **one** `INSERT ... ON CONFLICT(id) DO UPDATE SET col = excluded.col`, binding the preserved id or NULL. **Never** use `INSERT OR REPLACE`.
  - Reject duplicate labels in the validation pass.
  - **Do not lift** the "refuses inside a transaction" precondition, because that decision is still pending with the user. Do update its stated rationale in the root `AGENTS.md`, `src/AGENTS.md` and `bindings/js/src/lua-api.ts`.
  - Plan 58 later refactors the same file, so keep this change minimal and self-contained.
- Root `AGENTS.md` Design Decision, which stays in force (only the parenthetical reason changes): "**`import_csv` refuses to run inside an open transaction** (`PRAGMA foreign_keys` is a no-op mid-transaction, so nesting is unsupportable) — Pattern 1 precondition, not a silent rollback." Once the PRAGMA is gone, the precondition still guards something real. Import opens its own transaction with `impl_->begin_transaction()` (a raw `BEGIN`), and its catch calls `impl_->rollback()`. Inside a caller's transaction that `BEGIN` throws, and the catch's `ROLLBACK` would discard the caller's work. That is the new stated reason.
- Root dry-run decision: "`import_csv` still throws its `transaction already active` precondition". This stays true and is not edited.
- Root Principles: clean over defensive, simple over abstract, delete rather than deprecate. So the zero-row special case is deleted, because the general path handles zero rows, and the group path's needless toggle goes too.
- Error messages use the three root patterns. The duplicate check reuses the existing message `Cannot import_csv: There are duplicate entries in the CSV file.` (Pattern 1). Every layer's tests match it by the substring `duplicate entries`.
- `TransactionGuard` stays out, and so do the merged catch tails and `convert_cell`. Those belong to plans 60 and 58.

**Corrections to the finding's proposal, and why:**
1. There are **three** toggle sites, not two. The group path also switches foreign keys off. It never needed to: every parent id and FK cell there is resolved to an existing element before the write, and nothing references a group table. It is removed too. Only then is the PRAGMA premise of the precondition actually gone.
2. **Duplicate labels.** Today a repeated label is caught only by accident: two INSERTs with the same preserved id collide on the rowid, and the resulting `UNIQUE constraint` error is rewritten to "duplicate entries". Under an upsert, the second row silently overwrites the first. I verified this in SQLite (`dup existing label -> ('Y',)`). So the check moves into the validation pass, as both verifiers and the maintainer require.
3. **Ordering, which the verifiers missed.** They proposed "delete omitted, then write". With an `ON DELETE CASCADE` **self**-reference, which the validator allows for relation FKs, deleting an omitted parent X while a kept row Y still points at it deletes Y and Y's group rows. The upsert then re-inserts Y by its preserved id, so the row comes back but its group rows are gone. That is the same silent loss this item fixes. I verified it in SQLite: `delete-first: [(2, 'Y', None)] []`, meaning the vector is gone. The fix is one step before the deletes: set every self-FK column to NULL. The existing second pass already restores the self-FK columns from labels.
4. **Tests in every layer.** The behaviour is visible through every binding's `import_csv`, and the 0.10.9 bulk-read fix set the precedent of adding one test per binding. So Lua, Julia, Dart, Python and JS each get one compact test. The verifiers asked only for C++ and C API tests.

**Alternatives considered and rejected:**
- `INSERT OR REPLACE`. Its implicit delete fires the ON DELETE actions on the rows being kept, which wipes their group rows. The maintainer ruled it out.
- A separate `UPDATE` for existing labels plus an `INSERT` for new ones. That is two statements and a branch where one upsert does the job, and the maintainer chose the upsert.
- Keeping FK OFF and deleting the orphans by hand. That hand-rolls CASCADE / SET NULL for every referencing table, when the schema already declares it.
- Write first, then delete the omitted elements. This avoids the self-FK hazard without a clearing step. But a new or renamed row that takes an omitted element's value in another `UNIQUE` column would then collide with the not-yet-deleted row. Deleting first frees those values.
- Lifting the transaction precondition. That decision is pending with the user.
- Keeping the header-only fast path (a plain `DELETE FROM` would now cascade correctly). It duplicates the general path.

## Changes

All code changes are in **`src/database_csv_import.cpp`**, function `Database::import_csv`. No public header, C API, FFI declaration, binding wrapper or Lua binding changes. The Julia generator, Python `_c_api.py`, JS `loader.ts` and Dart `bindings.dart` are untouched.

### Step 1 — precondition comment (currently ~L309-313)

Current:
```cpp
    // Import toggles PRAGMA foreign_keys, which is a no-op inside a transaction,
    // and manages its own transaction — so it cannot run inside an explicit one.
    if (in_transaction()) {
```
New:
```cpp
    // Import manages its own transaction (a raw BEGIN, and a ROLLBACK on any error), so inside a
    // caller's transaction its BEGIN would fail and that ROLLBACK would discard the caller's work.
    if (in_transaction()) {
```
Why: the PRAGMA premise is gone after this change. The check itself stays, per the maintainer decision.

### Step 2 — delete the zero-row special case (currently ~L363-374)

Current:
```cpp
    auto row_count = csv.rows.size();
    if (row_count == 0) {
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            execute_raw("DELETE FROM " + table_name);
            execute_raw("PRAGMA foreign_keys = ON");
        } catch (...) {
            execute_raw("PRAGMA foreign_keys = ON");
            throw;
        }
        return;
    }
```
New:
```cpp
    auto row_count = csv.rows.size();
```
Why: a header-only CSV now takes the general path. In the scalar path it deletes every existing element by id, so the cascades fire, inside the one transaction. In the group path it deletes the group table's rows and inserts none. Every loop below handles `row_count == 0`.

### Step 3 — the `existing_label_to_id` comment (currently ~L389-391)

Current:
```cpp
        // Capture label -> id before the delete so re-inserted elements keep their
        // ids; otherwise foreign keys in other tables would silently re-point.
        auto existing_label_to_id = build_label_to_id_map(*this, collection);
```
New:
```cpp
        // Capture label -> id before any write: it decides which elements are updated in place (keeping
        // their ids, so relations to them hold) and which ones the CSV omits and are deleted.
        auto existing_label_to_id = build_label_to_id_map(*this, collection);
```

### Step 4 — duplicate-label check at the top of the validation pass (currently ~L407-409)

Current:
```cpp
        // Validation pass: check all cells before mutating
        for (size_t row = 0; row < row_count; ++row) {
            for (const auto& col_name : db_cols) {
```
New:
```cpp
        // Validation pass: check all cells before mutating. A label may appear only once: an existing
        // label is written in place by its id below, so a repeat would silently overwrite the first row.
        std::set<std::string> csv_labels;
        for (size_t row = 0; row < row_count; ++row) {
            if (!csv_labels.insert(csv.rows[row][csv_col_index.at("label")]).second) {
                throw std::runtime_error("Cannot import_csv: There are duplicate entries in the CSV file.");
            }
            for (const auto& col_name : db_cols) {
```
The rest of the per-cell validation loop body is unchanged. `std::set` is already included (`<set>`, used by `validate_columns_match`), so no include is added. Labels are trimmed on both sides: `read_csv_file` trims cells, and `Database::execute` trims every bound string. So `csv_labels` and `existing_label_to_id` compare like with like. An empty label still fails first with `Column label cannot be NULL.`, in that row's own column loop, before any second row is reached.

### Step 5 — the write block head (currently ~L454-469)

Current:
```cpp
        // Data import: disable FK → DELETE → INSERT → enable FK
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + collection);

            // Build INSERT statement; id is inserted explicitly (it is not one of db_cols).
            std::string insert_cols = "id";
            std::string insert_placeholders = "?";
            for (const auto& col : db_cols) {
                insert_cols += ", " + col;
                insert_placeholders += ", ?";
            }
            auto insert_sql =
                "INSERT INTO " + collection + " (" + insert_cols + ") VALUES (" + insert_placeholders + ")";
```
The group path has a block that starts with the same two lines but continues `DELETE FROM " + table_name`. Include the `collection` line in the old string so it is unique.

New:
```cpp
        std::vector<std::string> self_fk_cols;
        for (const auto& [col_name, fk] : fk_map) {
            if (fk.to_table == collection) {
                self_fk_cols.push_back(col_name);
            }
        }

        // Data import, in one transaction and with foreign keys on throughout, so every ON DELETE
        // action fires exactly as it does for delete_element.
        try {
            impl_->begin_transaction();

            // Clear self-references first: an ON DELETE CASCADE self-reference from a kept element to
            // an omitted one would otherwise delete the kept element along with it. The second pass
            // below restores them from the CSV's labels.
            for (const auto& col_name : self_fk_cols) {
                execute("UPDATE " + collection + " SET " + col_name + " = NULL");
            }

            // Delete the elements the CSV omits: their group rows cascade away and every relation to
            // them follows its ON DELETE action (SET NULL clears it, CASCADE deletes the row).
            for (const auto& [label, id] : existing_label_to_id) {
                if (!csv_labels.contains(label)) {
                    execute("DELETE FROM " + collection + " WHERE id = ?", {id});
                }
            }

            // Write every row by id (id is not one of db_cols): an existing label's id updates its row
            // in place, keeping its group rows and every relation pointing at it; a new label's NULL id
            // gets a fresh one. Never INSERT OR REPLACE: its implicit delete would cascade as well.
            std::string insert_cols = "id";
            std::string insert_placeholders = "?";
            std::string update_assignments;
            for (const auto& col : db_cols) {
                insert_cols += ", " + col;
                insert_placeholders += ", ?";
                if (!update_assignments.empty()) {
                    update_assignments += ", ";
                }
                update_assignments += col + " = excluded." + col;
            }
            auto insert_sql = "INSERT INTO " + collection + " (" + insert_cols + ") VALUES (" + insert_placeholders +
                              ") ON CONFLICT(id) DO UPDATE SET " + update_assignments;
```
The per-row parameter loop that follows (currently ~L471-529) is **unchanged**. It already binds the preserved id or `nullptr` first, writes `nullptr` for self-FK cells, and ends with `execute(insert_sql, parameters);`.

`db_cols` always contains `label` (checked at ~L346), so `update_assignments` is never empty. `validate_columns_match` forces the CSV to carry every column, so updating every column is well defined. The conflict target `id` is each collection's `INTEGER PRIMARY KEY`. The upsert never changes `id`, so `ON UPDATE CASCADE` never fires.

### Step 6 — the second pass head (currently ~L531-539)

Current:
```cpp
            // Second pass: resolve self-referencing FKs
            std::vector<std::string> self_fk_cols;
            for (const auto& [col_name, fk] : fk_map) {
                if (fk.to_table == collection) {
                    self_fk_cols.push_back(col_name);
                }
            }

            if (!self_fk_cols.empty()) {
```
New (the computation moved up in Step 5):
```cpp
            // Second pass: resolve self-referencing FKs, now that every CSV row exists
            if (!self_fk_cols.empty()) {
```
The body of that `if` is unchanged.

### Step 7 — both catch tails (scalar ~L562-566, group ~L775-779)

This exact block occurs **twice**. Change both, for example with Edit `replace_all`.

Current:
```cpp
            impl_->commit();
            execute_raw("PRAGMA foreign_keys = ON");
        } catch (const std::exception& e) {
            impl_->rollback();
            execute_raw("PRAGMA foreign_keys = ON");
```
New:
```cpp
            impl_->commit();
        } catch (const std::exception& e) {
            impl_->rollback();
```
The `UNIQUE constraint` → "duplicate entries" mapping below each tail stays as it is. On the scalar path it is now reachable only from a `UNIQUE` column other than `label`. Plan 58 owns merging the tails.

### Step 8 — group path head (currently ~L706-711)

Current:
```cpp
        // Data import: disable FK → DELETE → INSERT → enable FK
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + table_name);
```
New:
```cpp
        // Data import: DELETE → INSERT in one transaction. Every id and FK cell was resolved to an
        // existing element above, so the inserts need no foreign-key relaxation.
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + table_name);
```

### Resulting scalar write block, for review

After Steps 5-7, the scalar branch reads, in order:
1. The validation pass, with `csv_labels`.
2. `self_fk_cols`.
3. `try { begin;`
4. Clear the self-FK columns.
5. Delete the omitted elements.
6. Build the upsert SQL.
7. The unchanged row loop.
8. The unchanged self-FK pass.
9. `commit; } catch { rollback; UNIQUE→duplicate mapping; throw; }`

Afterwards, `grep -n "PRAGMA" src/database_csv_import.cpp` must print nothing.

### Step 9 — test comment (`tests/test_database_transaction.cpp`, `TEST(DatabaseDryRun, ImportCsvStillRefusesToNest)`, currently ~L342-343)

Current:
```cpp
    // import_csv toggles PRAGMA foreign_keys, which is a no-op mid-transaction — the dry run's
    // transaction is a real one, so the existing precondition still fires.
```
New:
```cpp
    // import_csv manages its own transaction and refuses to nest; the dry run's transaction is a
    // real one, so that precondition still fires.
```

## Tests

### New schema: `tests/schemas/valid/csv_import_self_cascade.sql` (LF line endings)

```sql
-- Schema: a self-reference with ON DELETE CASCADE
-- Tests: import_csv deletes an omitted element without cascading through a stale self-reference
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Node (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    node_parent INTEGER,
    FOREIGN KEY (node_parent) REFERENCES Node(id) ON DELETE CASCADE ON UPDATE CASCADE
) STRICT;

CREATE TABLE Node_vector_weights (
    id INTEGER NOT NULL REFERENCES Node(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    weight REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
```
I checked that this schema passes `SchemaValidator`: `quiver_cli --schema` on a scratch copy loaded it, and `node_parent` reports `is_foreign_key = true`. No CMake registration is needed, because `VALID_SCHEMA` resolves from the source path. List the file in `tests/AGENTS.md` (see Docs).

### C++ core — `tests/test_database_csv_import.cpp`

**Change the existing test** `TEST(DatabaseCSV, ImportCSV_Scalar_HeaderOnly_ClearsTable)` (currently ~L209-227). Replace
```cpp
    quiver::Element e1;
    e1.set("label", std::string("Item1")).set("name", std::string("Alpha"));
    db.create_element("Items", e1);
```
with
```cpp
    quiver::Element e1;
    e1.set("label", std::string("Item1"))
        .set("name", std::string("Alpha"))
        .set("measurement", std::vector<double>{1.1, 2.2})
        .set("tag", std::vector<std::string>{"red"});
    db.create_element("Items", e1);
```
and after `EXPECT_TRUE(names.empty());` add:
```cpp
    // Deleted with foreign keys on, so the element's group rows went with it.
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Items_vector_measurements"), 0);
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Items_set_tags"), 0);
```
This fails before the fix, with counts 2 and 1.

**Append at the end of the file** (after `ImportCSV_InsideTransactionThrows`). `expect_import_error` (defined ~L783) and `make_relations_db` (top of file) already exist.

```cpp
// ============================================================================
// import_csv: an element the CSV omits is deleted with foreign keys ON
// ============================================================================

// Import used to switch foreign keys off, delete every row and re-insert the CSV's, so an element the
// CSV left out lost only its collection row: its group rows stayed behind, still readable by id.
TEST(DatabaseCSV, ImportCSV_Scalar_OmittedElement_DeletesItsGroupRows) {
    auto db = make_db();

    auto dropped = db.create_element("Items",
                                     quiver::Element()
                                         .set("label", std::string("Dropped"))
                                         .set("name", std::string("Alpha"))
                                         .set("measurement", std::vector<double>{1.5, 2.5})
                                         .set("tag", std::vector<std::string>{"red"}));
    std::vector<std::map<std::string, quiver::Value>> readings = {
        {{"date_time", std::string("2024-01-01T00:00:00")}, {"temperature", 20.0}, {"humidity", int64_t{50}}}};
    db.update_time_series_group("Items", "readings", dropped, readings);
    auto kept = db.create_element("Items",
                                  quiver::Element()
                                      .set("label", std::string("Kept"))
                                      .set("name", std::string("Beta"))
                                      .set("measurement", std::vector<double>{9.5}));

    auto csv_path = temp_csv("ImportOmittedElement");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta2,,,,\n");
    db.import_csv("Items", "", csv_path.string());
    fs::remove(csv_path);

    EXPECT_EQ(db.read_element_ids("Items"), (std::vector<int64_t>{kept}));
    // The kept element was updated in place: new scalar value, same id, group rows intact.
    EXPECT_EQ(db.read_scalar_string_by_id("Items", "name", kept), "Beta2");
    EXPECT_EQ(db.read_vector_floats_by_id("Items", "measurement", kept), (std::vector<double>{9.5}));
    // The dropped element's group rows went with it (ON DELETE CASCADE).
    for (const std::string table : {"Items_vector_measurements", "Items_set_tags", "Items_time_series_readings"}) {
        EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM " + table + " WHERE id = ?", {dropped}), 0) << table;
    }
}

// With foreign keys off, deleting an omitted element fired no ON DELETE action: every relation to it
// kept the deleted id, which export_csv then wrote as a bare number that import_csv rejected.
TEST(DatabaseCSV, ImportCSV_Scalar_OmittedParent_AppliesOnDeleteActions) {
    auto db = make_relations_db();
    auto id_a = db.create_element("Parent", quiver::Element().set("label", std::string("Parent A")));
    auto id_b = db.create_element("Parent", quiver::Element().set("label", std::string("Parent B")));
    auto child =
        db.create_element("Child", quiver::Element().set("label", std::string("Child 1")).set("parent_id", id_b));
    db.update_vector_group("Child", "refs", child, {{{"parent_ref", id_b}}});                          // SET NULL
    db.update_set_group("Child", "parents", child, {{{"parent_ref", id_a}}, {{"parent_ref", id_b}}});  // CASCADE
    std::vector<std::map<std::string, quiver::Value>> events = {
        {{"date_time", std::string("2024-01-01T00:00:00")}, {"sponsor_id", id_b}}};  // SET NULL
    db.update_time_series_group("Child", "events", child, events);

    auto csv_path = temp_csv("ImportOmittedParent");
    write_csv_file(csv_path.string(), "sep=,\nlabel\nParent A\n");
    db.import_csv("Parent", "", csv_path.string());

    EXPECT_EQ(db.read_element_ids("Parent"), (std::vector<int64_t>{id_a}));
    EXPECT_FALSE(db.read_scalar_integer_by_id("Child", "parent_id", child).has_value());
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = ? AND parent_ref IS NULL", {child}),
              1);
    EXPECT_EQ(db.read_set_integers_by_id("Child", "parent_ref", child), (std::vector<int64_t>{id_a}));
    EXPECT_EQ(
        db.query_integer("SELECT COUNT(*) FROM Child_time_series_events WHERE id = ? AND sponsor_id IS NULL", {child}),
        1);

    // No dangling id is left for export_csv to write, so the Child table round-trips again - and the
    // re-import updates Child 1 in place, keeping its vector row.
    db.export_csv("Child", "", csv_path.string());
    EXPECT_NO_THROW(db.import_csv("Child", "", csv_path.string()));
    fs::remove(csv_path);
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", {child}), 1);
}

// An existing label is written in place by its id, so a repeat would let the last row win silently;
// the validation pass rejects it before anything is written.
TEST(DatabaseCSV, ImportCSV_Scalar_RepeatedExistingLabel_Throws) {
    auto db = make_db();
    db.create_element("Items", quiver::Element().set("label", std::string("Item1")).set("name", std::string("Alpha")));

    auto csv_path = temp_csv("ImportRepeatedExistingLabel");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\n"
                   "Item1,Beta,,,,\n"
                   "Item1,Gamma,,,,\n");

    expect_import_error(db, "", csv_path, "Cannot import_csv: There are duplicate entries in the CSV file.");
    fs::remove(csv_path);

    EXPECT_EQ(db.read_scalar_string_by_id("Items", "name", 1), "Alpha");
}

// Leaf points at Root through an ON DELETE CASCADE self-reference. Deleting the omitted Root while
// Leaf still pointed at it would delete Leaf too, and re-inserting Leaf by its preserved id would
// bring the row back without its vector - so import clears self-references before deleting.
TEST(DatabaseCSV, ImportCSV_Scalar_OmittedElement_DoesNotCascadeThroughSelfReference) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("csv_import_self_cascade.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto root = db.create_element("Node", quiver::Element().set("label", std::string("Root")));
    auto leaf = db.create_element("Node",
                                  quiver::Element()
                                      .set("label", std::string("Leaf"))
                                      .set("node_parent", root)
                                      .set("weight", std::vector<double>{1.5, 2.5}));

    auto csv_path = temp_csv("ImportSelfCascade");
    write_csv_file(csv_path.string(), "sep=,\nlabel,node_parent\nLeaf,\n");
    db.import_csv("Node", "", csv_path.string());
    fs::remove(csv_path);

    EXPECT_EQ(db.read_element_ids("Node"), (std::vector<int64_t>{leaf}));
    EXPECT_FALSE(db.read_scalar_integer_by_id("Node", "node_parent", leaf).has_value());
    EXPECT_EQ(db.read_vector_floats_by_id("Node", "weight", leaf), (std::vector<double>{1.5, 2.5}));
}
```

What each new test proves, and when it fails:

| Test | Fails before this plan? | Why |
| --- | --- | --- |
| `ImportCSV_Scalar_HeaderOnly_ClearsTable` (extended) | yes | orphan counts are 2 and 1 |
| `ImportCSV_Scalar_OmittedElement_DeletesItsGroupRows` | yes | the dropped id still has 2 / 1 / 1 group rows |
| `ImportCSV_Scalar_OmittedParent_AppliesOnDeleteActions` | yes | `parent_id` is still `id_b`, the vector ref is not NULL, the set still holds `id_b`, the sponsor is not NULL, and the re-import throws `...with label 2` |
| `ImportCSV_Scalar_RepeatedExistingLabel_Throws` | no (today the rowid collision is remapped to the same message) | guards the upsert: it fails if Step 4 is dropped, because the import then succeeds and `name` is `Gamma` |
| `ImportCSV_Scalar_OmittedElement_DoesNotCascadeThroughSelfReference` | no (FK off never cascades) | guards the ordering: it fails if the self-FK clearing loop in Step 5 is dropped or moved after the deletes, because the vector comes back empty |

Existing tests that must stay green unchanged, each of which I traced against the new code:
- `ImportCSV_Scalar_PreservesIdsForForeignKeys` and `ImportCSV_Scalar_MixedNewAndExistingLabels`. `AUTOINCREMENT` still gives `Parent New` an id greater than `id_c`; I checked this in SQLite and got `[(2,'Parent B'),(3,'Parent C'),(4,'Parent New')]`.
- `ImportCSV_Scalar_DuplicateEntries_Throws`, which is now caught by Step 4.
- `ImportCSV_Scalar_SelfReferenceFK_RoundTrip` and `_ReImport`.
- `ImportCSV_Group_HeaderOnly_ClearsGroup`.
- `ImportCSV_Group_DuplicateEntries_Throws`, which still goes through the group catch mapping.
- `ImportCSV_LockedFile_RejectedBeforeDelete`.
- `ImportCSV_InsideTransactionThrows`.
- `ExportImportCSV_*` in `tests/test_database_csv_export.cpp`.
- `DatabaseDryRun.ImportCsvStillRefusesToNest`.

### C API — `tests/test_c_api_database_csv_import.cpp` (append at the end of the file)

```cpp
// ============================================================================
// CSV Import: an element the CSV omits is deleted with foreign keys ON
// ============================================================================

TEST(DatabaseCApiCSV, ImportCSV_Scalar_OmittedElement_DeletesItsGroupRows) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* dropped = nullptr;
    ASSERT_EQ(quiver_element_create(&dropped), QUIVER_OK);
    quiver_element_set_string(dropped, "label", "Dropped");
    quiver_element_set_string(dropped, "name", "Alpha");
    double dropped_values[] = {1.5, 2.5};
    quiver_element_set_array_float(dropped, "measurement", dropped_values, 2, nullptr);
    const char* dropped_tags[] = {"red"};
    quiver_element_set_array_string(dropped, "tag", dropped_tags, 1, nullptr);
    int64_t dropped_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", dropped, &dropped_id), QUIVER_OK);
    quiver_element_destroy(dropped);

    quiver_element_t* kept = nullptr;
    ASSERT_EQ(quiver_element_create(&kept), QUIVER_OK);
    quiver_element_set_string(kept, "label", "Kept");
    quiver_element_set_string(kept, "name", "Beta");
    double kept_values[] = {9.5};
    quiver_element_set_array_float(kept, "measurement", kept_values, 1, nullptr);
    int64_t kept_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", kept, &kept_id), QUIVER_OK);
    quiver_element_destroy(kept);

    auto csv_path = temp_csv("ImportOmittedElement");
    write_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta,,,,\n");
    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_OK);
    fs::remove(csv_path);

    // The kept element was updated in place, so its vector survives.
    double* values = nullptr;
    size_t value_count = 0;
    ASSERT_EQ(quiver_database_read_vector_floats_by_id(db, "Items", "measurement", kept_id, &values, &value_count),
              QUIVER_OK);
    ASSERT_EQ(value_count, 1u);
    EXPECT_EQ(values[0], 9.5);
    quiver_database_free_float_array(values);

    // The dropped element's group rows went with it.
    for (const char* sql : {"SELECT COUNT(*) FROM Items_vector_measurements WHERE id NOT IN (SELECT id FROM Items)",
                            "SELECT COUNT(*) FROM Items_set_tags WHERE id NOT IN (SELECT id FROM Items)"}) {
        int64_t orphans = -1;
        int has_value = 0;
        ASSERT_EQ(quiver_database_query_integer(db, sql, &orphans, &has_value), QUIVER_OK);
        EXPECT_EQ(has_value, 1);
        EXPECT_EQ(orphans, 0) << sql;
    }

    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_Scalar_OmittedParent_ClearsRelation) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("relations.sql").c_str(), &options, &db), QUIVER_OK);

    const char* parent_labels[] = {"Parent A", "Parent B"};
    int64_t parent_ids[] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        quiver_element_t* parent = nullptr;
        ASSERT_EQ(quiver_element_create(&parent), QUIVER_OK);
        quiver_element_set_string(parent, "label", parent_labels[i]);
        ASSERT_EQ(quiver_database_create_element(db, "Parent", parent, &parent_ids[i]), QUIVER_OK);
        quiver_element_destroy(parent);
    }

    quiver_element_t* child = nullptr;
    ASSERT_EQ(quiver_element_create(&child), QUIVER_OK);
    quiver_element_set_string(child, "label", "Child 1");
    quiver_element_set_integer(child, "parent_id", parent_ids[1]);
    int64_t child_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Child", child, &child_id), QUIVER_OK);
    quiver_element_destroy(child);

    auto csv_path = temp_csv("ImportOmittedParent");
    write_csv_file(csv_path.string(), "sep=,\nlabel\nParent A\n");
    auto import_options = quiver_csv_options_default();
    ASSERT_EQ(quiver_database_import_csv(db, "Parent", "", csv_path.string().c_str(), &import_options), QUIVER_OK);
    fs::remove(csv_path);

    // Parent B was deleted with foreign keys on, so Child.parent_id's ON DELETE SET NULL fired.
    int64_t parent_id = -1;
    int has_value = 1;
    ASSERT_EQ(
        quiver_database_read_scalar_integer_by_id(db, "Child", "parent_id", child_id, &parent_id, &has_value),
        QUIVER_OK);
    EXPECT_EQ(has_value, 0);

    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ImportCSV_Scalar_RepeatedExistingLabel_ReturnsError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ImportRepeatedExistingLabel");
    write_csv_file(csv_path.string(),
                   "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Beta,,,,\nItem1,Gamma,,,,\n");
    auto import_options = quiver_csv_options_default();
    EXPECT_EQ(quiver_database_import_csv(db, "Items", "", csv_path.string().c_str(), &import_options), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot import_csv: There are duplicate entries in the CSV file.");
    fs::remove(csv_path);

    // Rejected before anything was written.
    char* name = nullptr;
    int has_value = 0;
    ASSERT_EQ(quiver_database_read_scalar_string_by_id(db, "Items", "name", id1, &name, &has_value), QUIVER_OK);
    ASSERT_EQ(has_value, 1);
    EXPECT_STREQ(name, "Alpha");
    quiver_database_free_string(name);

    quiver_database_close(db);
}
```
The first two tests fail before the fix: the orphan count is 2, and `has_value` is 1. The third passes both before and after; it is a guard.

### Lua — `tests/test_lua_runner_csv_import.cpp`

Insert after `TEST_F(LuaRunner_ImportCSV, VectorTrailingEmptyColumns)` (currently ends ~L167), before `InsideTransactionThrows`:
```cpp
TEST_F(LuaRunner_ImportCSV, OmittedElementDeletesItsGroupRows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Items", { label = "Dropped", name = "Alpha", measurement = {1.5, 2.5}, tag = {"red"} })
        db:create_element("Items", { label = "Kept", name = "Beta", measurement = {9.5} })
    )");

    write_lua_csv_file((sandbox / "subset.csv").string(),
                       "sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta,,,,\n");

    lua.run(R"(
        db:import_csv("Items", "", "subset.csv")
        assert(db:number_of_elements("Items") == 1)
        local vec = db:query_integer("SELECT COUNT(*) FROM Items_vector_measurements WHERE id NOT IN (SELECT id FROM Items)")
        local set = db:query_integer("SELECT COUNT(*) FROM Items_set_tags WHERE id NOT IN (SELECT id FROM Items)")
        assert(vec == 0, "orphaned vector rows: " .. vec)
        assert(set == 0, "orphaned set rows: " .. set)
    )");

    EXPECT_EQ(db.read_vector_floats_by_id("Items", "measurement", 2), (std::vector<double>{9.5}));
}
```
This fails before the fix: `orphaned vector rows: 2` is raised as a `Failed to run Lua script` exception.

### Julia — `bindings/julia/test/test_database_csv_import.jl`

Insert before `@testset "Import inside transaction throws" begin` (currently ~L370):
```julia
    @testset "Scalar import deletes an omitted element's group rows" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items"; label = "Dropped", name = "Alpha", measurement = [1.5, 2.5], tag = ["red"])
            kept = Quiver.create_element!(db, "Items"; label = "Kept", name = "Beta", measurement = [9.5])

            open(csv_path, "w") do f
                return write(f, "sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta,,,,\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path)

            @test Quiver.read_element_ids(db, "Items") == [kept]
            @test Quiver.read_vector_floats_by_id(db, "Items", "measurement", kept) == [9.5]
            orphans(table) = Quiver.query_integer(db, "SELECT COUNT(*) FROM $table WHERE id NOT IN (SELECT id FROM Items)")
            @test orphans("Items_vector_measurements") == 0
            @test orphans("Items_set_tags") == 0
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end
```

### Dart — `bindings/dart/test/database_csv_import_test.dart`

Insert before `test('import inside transaction throws', () {` (currently ~L398):
```dart
    test('scalar import deletes an omitted element\'s group rows', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_omitted.csv';
      try {
        db.createElement('Items', {
          'label': 'Dropped',
          'name': 'Alpha',
          'measurement': [1.5, 2.5],
          'tag': ['red'],
        });
        final kept = db.createElement('Items', {
          'label': 'Kept',
          'name': 'Beta',
          'measurement': [9.5],
        });

        File(csvPath).writeAsStringSync('sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta,,,,\n');

        db.importCSV('Items', '', csvPath);

        expect(db.readElementIds('Items'), [kept]);
        expect(db.readVectorFloatsById('Items', 'measurement', kept), [9.5]);
        int? orphans(String table) =>
            db.queryInteger('SELECT COUNT(*) FROM $table WHERE id NOT IN (SELECT id FROM Items)');
        expect(orphans('Items_vector_measurements'), 0);
        expect(orphans('Items_set_tags'), 0);
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });
```

### Python — `bindings/python/tests/test_database_csv_import.py`

Insert before `class TestImportCSVInsideTransaction:` (currently ~L180):
```python
class TestImportCSVOmittedElement:
    """A scalar import deletes the elements the CSV omits, together with their group rows."""

    def test_omitted_element_group_rows_are_deleted(self, csv_db: Database, tmp_path):
        csv_db.create_element("Items", label="Dropped", name="Alpha", measurement=[1.5, 2.5], tag=["red"])
        kept = csv_db.create_element("Items", label="Kept", name="Beta", measurement=[9.5])

        csv_path = tmp_path / "subset.csv"
        csv_path.write_text("sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta,,,,\n")

        csv_db.import_csv("Items", "", str(csv_path))

        assert csv_db.read_element_ids("Items") == [kept]
        assert csv_db.read_vector_floats_by_id("Items", "measurement", kept) == [9.5]
        orphans = "SELECT COUNT(*) FROM {} WHERE id NOT IN (SELECT id FROM Items)"
        assert csv_db.query_integer(orphans.format("Items_vector_measurements")) == 0
        assert csv_db.query_integer(orphans.format("Items_set_tags")) == 0
```

### JS — `bindings/js/test/database-csv.test.ts`

Insert before the `// Import inside an explicit transaction` banner (currently ~L304-306):
```ts
// ============================================================================
// Import of a CSV that omits an element
// ============================================================================

describe("CSV import omitting an element", () => {
  test("the omitted element's group rows are deleted", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("import_omitted");
    try {
      db.createElement("Items", { label: "Dropped", name: "Alpha", measurement: [1.5, 2.5], tag: ["red"] });
      const kept = db.createElement("Items", { label: "Kept", name: "Beta", measurement: [9.5] });
      writeFileSync(csvPath, "sep=,\nlabel,name,status,price,date_created,notes\nKept,Beta,,,,\n");

      db.importCsv("Items", "", csvPath);

      expect(db.readElementIds("Items")).toEqual([kept]);
      expect(db.readVectorFloatsById("Items", "measurement", kept)).toEqual([9.5]);
      const orphans = (table: string) =>
        db.queryInteger(`SELECT COUNT(*) FROM ${table} WHERE id NOT IN (SELECT id FROM Items)`);
      expect(orphans("Items_vector_measurements")).toBe(0);
      expect(orphans("Items_set_tags")).toBe(0);
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });
});
```
Use `9.5`, not `9`: a JS integer array would marshal as INTEGER cells. Every binding test fails before the fix, because the orphan counts are 2 and 1.

## Docs and changelog

### Root `AGENTS.md`

(a) Design Decisions (currently ~L145-146). Replace
```
- **`import_csv` refuses to run inside an open transaction** (`PRAGMA foreign_keys` is a no-op
  mid-transaction, so nesting is unsupportable) — Pattern 1 precondition, not a silent rollback.
```
with
```
- **`import_csv` refuses to run inside an open transaction** — Pattern 1 precondition, not a silent
  rollback. Import opens its own transaction (a raw `BEGIN`, rolled back on any error), so nested
  inside a caller's transaction its `BEGIN` would fail and that `ROLLBACK` would discard the
  caller's work. The original reason (import toggled `PRAGMA foreign_keys`, a no-op
  mid-transaction) is gone: import now keeps foreign keys on throughout. Whether to let it nest
  instead is an open decision for the maintainer.
```

(b) Core API → Database Class, the CSV bullet. After the line ending `significant digits.` (currently ~L628, end of the "Export and import are symmetric on foreign keys" paragraph), add a continuation paragraph with the same two-space indent:
```
  **A scalar `import_csv` makes the collection match the CSV by label, with foreign keys on**: an
  element whose label is in the file is updated in place (its id, group rows and inbound relations
  survive), a new label is inserted, and an element the file omits is deleted exactly as
  `delete_element` would delete it — its group rows cascade away and every relation to it follows
  its `ON DELETE` action (`SET NULL` clears it, `CASCADE` deletes the referencing row, which can be
  an element of another collection). A label may appear only once. Import used to switch foreign
  keys off and delete-then-reinsert every row, which orphaned the omitted elements' group rows and
  left relations pointing at deleted ids. Mechanism and ordering: `src/AGENTS.md`.
```

### `src/AGENTS.md`

(a) Transactions section (currently ~L336-338). Replace
```
The one write path that cannot nest is `import_csv` (it toggles `PRAGMA foreign_keys`, which is a
no-op inside a transaction) — it throws `"Cannot import_csv: transaction already active"` as a
precondition instead of silently destroying the caller's transaction.
```
with
```
The one write path that cannot nest is `import_csv`: it opens its own transaction with a raw
`impl_->begin_transaction()` (not `TransactionGuard`) and rolls back on any error, so inside a
caller's transaction the `BEGIN` would fail and the `ROLLBACK` would discard the caller's work. It
throws `"Cannot import_csv: transaction already active"` as a precondition instead. (It used to
toggle `PRAGMA foreign_keys`, a no-op mid-transaction, which was the original reason; it no longer
does. Whether it should nest instead is an open decision.)
```

(b) Core Internals Worth Knowing. Insert a new bullet immediately before `- **Label→id resolution has one query**` (currently ~L393):
```
- **`import_csv`'s scalar path keeps foreign keys ON** (`database_csv_import.cpp`) and runs, in one
  transaction and in this order, every step load-bearing: (1) set every self-FK column to NULL, so
  that (2) deleting each existing element whose label the CSV omits (`DELETE ... WHERE id = ?`,
  firing CASCADE / SET NULL exactly as `delete_element`) cannot cascade through a stale
  `ON DELETE CASCADE` self-reference into an element the CSV keeps; (3) write every CSV row with
  `INSERT ... ON CONFLICT(id) DO UPDATE SET col = excluded.col`, binding the preserved id (NULL for
  a new label), so a kept element is updated in place and keeps its group rows and inbound
  relations; (4) the self-FK label pass. Never `INSERT OR REPLACE`: its implicit delete fires the
  ON DELETE actions and wipes the group rows of the very elements being kept. Deleting before
  writing also frees an omitted element's values in any other `UNIQUE` column; a *swap* of such
  values between two kept elements still fails (row-by-row UPDATEs) and rolls the import back. A
  repeated label is rejected in the validation pass, since the upsert would otherwise let the last
  row win silently. The group path needs nothing special: it deletes and re-inserts one group
  table whose ids and FK cells are all resolved to existing elements.
```

### `tests/AGENTS.md` — Schemas list (currently ~L162-170)

Replace
```
- `valid/` — `all_types.sql`, `basic.sql`, `collections.sql`, `composite_helpers.sql`,
  `csv_export.sql`, `csv_group_vector_index.sql`, `describe_multi_group.sql`,
  `mixed_time_series.sql`, `multi_column_groups.sql`, `multi_dim_time_series.sql`,
  `multi_time_series.sql`, `nullable_time_series.sql`, `relations.sql`
```
with
```
- `valid/` — `all_types.sql`, `basic.sql`, `collections.sql`, `composite_helpers.sql`,
  `csv_export.sql`, `csv_group_vector_index.sql`, `csv_import_self_cascade.sql`,
  `describe_multi_group.sql`, `mixed_time_series.sql`, `multi_column_groups.sql`,
  `multi_dim_time_series.sql`, `multi_time_series.sql`, `nullable_time_series.sql`, `relations.sql`
```
and insert this sub-bullet right after the `csv_group_vector_index.sql` sub-bullet (the one ending `dereferenced it\n    unvalidated.`):
```
  - `csv_import_self_cascade.sql` is the one schema with an `ON DELETE CASCADE` **self**-reference
    (`Node.node_parent`), plus a vector group. `import_csv` clears self-references before deleting
    the elements a CSV omits; without that, deleting an omitted parent cascades into a kept child
    and its vector rows (`ImportCSV_Scalar_OmittedElement_DoesNotCascadeThroughSelfReference`).
```

### `bindings/js/src/lua-api.ts` (a TS template string: keep the `\`` escapes)

(a) The dry-run rules (currently ~L226-227). Replace
```
- \`db:import_csv\` cannot run inside a dry run (it toggles a pragma that is a no-op mid-transaction)
  and throws \`Cannot import_csv: transaction already active\`.
```
with
```
- \`db:import_csv\` cannot run inside a dry run (it manages its own transaction, and a dry run is a
  real one) and throws \`Cannot import_csv: transaction already active\`.
```
(b) The CSV import / export precondition (currently ~L613-615). Replace
```
**Precondition:** \`db:import_csv\` cannot run inside an open transaction (it toggles
\`PRAGMA foreign_keys\`, a no-op mid-transaction) — it throws \`Cannot import_csv: transaction already
active\`. Call it outside any \`db:transaction\` / \`db:begin_transaction\` block.
```
with
```
**Precondition:** \`db:import_csv\` cannot run inside an open transaction (it opens and commits its
own) — it throws \`Cannot import_csv: transaction already active\`. Call it outside any
\`db:transaction\` / \`db:begin_transaction\` block.
```
Do not add replace-semantics prose here; plan 43 owns that claim. `lua-api-sync.test.ts` is unaffected, because no `db:` name changes.

### `CHANGELOG.md` — under `## [0.12.0] — unreleased` → `### Changed`

Append after the `export_csv()` quoting entry (the one ending `*Adapt:* regenerate golden files and any byte-for-byte comparisons over exported CSVs.`, currently ~L65) and before `### Fixed`:
```
- **BREAKING — `import_csv()` into a collection deletes the elements the CSV omits the way
  `delete_element()` does.** A scalar import makes the collection match the CSV by label. It used
  to switch foreign keys off, delete every row and re-insert the CSV's, so an element the CSV left
  out lost only its collection row: its vector, set and time-series rows stayed behind (still
  readable by its old id), and every relation to it kept pointing at the deleted id, which
  `export_csv()` then wrote as a bare number that `import_csv()` rejected. Foreign keys now stay on
  for the whole import. An element whose label is in the CSV is updated in place, keeping its id,
  group rows and inbound relations; a new label is inserted; an omitted element is deleted, so its
  group rows go with it and each relation to it follows the schema's `ON DELETE` action (`SET NULL`
  clears it, `CASCADE` deletes the referencing row, which can be an element of another collection).
  A CSV that repeats a label is rejected before anything is written.

  *Adapt:* keep every element you mean to keep in the CSV, since omitting one now also removes
  what depends on it through `ON DELETE CASCADE`. In a schema with a `UNIQUE` column other than
  `label`, an import that swaps that column's values between two elements it keeps now fails and
  rolls back; route the swap through a temporary value.
```

No other docs mention the PRAGMA or the omitted-element behaviour. I checked `docs/*.md` (only migration examples use `PRAGMA foreign_keys`) and the binding READMEs and docstrings (no import semantics are described). The binding `AGENTS.md` files need no change, because no binding code changes.

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`):

1. `cmake --build build --config Debug`
2. `grep -n "PRAGMA" src/database_csv_import.cpp` → no output.
3. `./build/bin/quiver_tests.exe --gtest_filter='DatabaseCSV.*:DatabaseDryRun.*:LuaRunner_ImportCSV.*'` → all pass, including the new `DatabaseCSV.ImportCSV_Scalar_OmittedElement_DeletesItsGroupRows`, `ImportCSV_Scalar_OmittedParent_AppliesOnDeleteActions`, `ImportCSV_Scalar_RepeatedExistingLabel_Throws`, `ImportCSV_Scalar_OmittedElement_DoesNotCascadeThroughSelfReference`, the extended `ImportCSV_Scalar_HeaderOnly_ClearsTable`, and `LuaRunner_ImportCSV.OmittedElementDeletesItsGroupRows`. (Baseline before the change: the same filter ran 84 tests, all green.)
4. `./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApiCSV.*'` → all pass, including `ImportCSV_Scalar_OmittedElement_DeletesItsGroupRows`, `ImportCSV_Scalar_OmittedParent_ClearsRelation` and `ImportCSV_Scalar_RepeatedExistingLabel_ReturnsError`.
5. Optional proof that the tests bite: `git stash push src/database_csv_import.cpp`, rebuild, rerun steps 3 and 4. The HeaderOnly, OmittedElement (C++, C API, Lua) and OmittedParent tests must fail; RepeatedExistingLabel and the self-cascade test pass on the old code, as they are guards. Then `git stash pop` and rebuild.
6. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe` (full suites) → green.
7. Binding suites (from cmd or PowerShell): `bindings\julia\test\test.bat`, `bindings\dart\test\test.bat`, `bindings\js\test\test.bat`, `bindings\python\tests\test.bat` → green, including each new test.
8. No generator run: no C API signature changed.
9. `scripts\format.bat`, then `git diff --stat`. Only the files this plan touches may appear. Check that no `.bat` file was touched.
10. `scripts\test-all.bat` → the six suites PASS. The CLI smoke test (step 7) currently FAILS on HEAD, because `example/example1.lua` was deleted; that is pre-existing and fixed by plan 65. Do not treat it as a regression from this plan.

## Acceptance criteria

- [ ] `src/database_csv_import.cpp` contains no `PRAGMA foreign_keys` toggle (all three sites removed) and no zero-row early return.
- [ ] The scalar path runs clear self-FKs → delete omitted ids → `INSERT ... ON CONFLICT(id) DO UPDATE SET col = excluded.col` per row → self-FK pass, in one transaction; there is no `INSERT OR REPLACE` anywhere.
- [ ] A repeated label throws `Cannot import_csv: There are duplicate entries in the CSV file.` from the validation pass, before any write.
- [ ] The `in_transaction()` precondition and its message are unchanged, and its comment states the new reason.
- [ ] New schema `tests/schemas/valid/csv_import_self_cascade.sql` exists (LF) and is listed in `tests/AGENTS.md`.
- [ ] The five C++ tests, three C API tests, one Lua test and one test in each of Julia / Dart / Python / JS are added as specified; every existing test is still green.
- [ ] Root `AGENTS.md`, `src/AGENTS.md`, `tests/AGENTS.md`, `bindings/js/src/lua-api.ts`, `tests/test_database_transaction.cpp` (comment) and `CHANGELOG.md` are edited as specified.
- [ ] `scripts/format.bat` leaves no diff beyond this plan's files.

## Pitfalls

- **The order inside the transaction is the fix.** If the self-FK clear is moved after the deletes, `ImportCSV_Scalar_OmittedElement_DoesNotCascadeThroughSelfReference` fails. If the delete is moved after the writes, a new row that takes an omitted element's value in another `UNIQUE` column fails against the not-yet-deleted row. `self_fk_cols` must therefore be computed before the `try` (Step 5), not in the second pass as today.
- **Two new tests pass on the old code by design.** `ImportCSV_Scalar_RepeatedExistingLabel_Throws` (C++ and C API) guards the upsert. `..._DoesNotCascadeThroughSelfReference` guards the ordering. Don't delete them as redundant.
- **Step 7's old string occurs twice.** Replace both occurrences. Step 5's and Step 8's old strings share their first four lines, so match on the `DELETE FROM " + collection` / `+ table_name` line to keep each edit unique.
- **The scalar catch still rewrites any `UNIQUE constraint` error to "duplicate entries".** After this change that is reachable only from a non-`label` `UNIQUE` column (including the swap case above). The message is then imprecise, but leave it: plan 58 owns the shared tail.
- **Fail-closed FK cases that did not fail before.** The validator exempts time-series tables from the FK-action rule, so a time-series table can hold a value FK with `NO ACTION` / `RESTRICT` pointing at the collection being imported. Deleting an omitted element that such a row references now fails with `Failed to execute statement: FOREIGN KEY constraint failed`, and the import rolls back. That matches what `delete_element` does. Plan 06 enforces CASCADE for set and time-series *parent* FKs. No test schema has such a column.
- **Cascade cycles between collections** (A → B CASCADE and B → A CASCADE) can reach rows the CSV keeps, exactly as `delete_element` can. Out of scope.
- **Trimming:** don't add trimming to `csv_labels` or the delete loop. Cells are trimmed in `read_csv_file`, and stored labels are trimmed by `Database::execute`.
- **gtest printing:** assert a missing optional with `EXPECT_FALSE(x.has_value())`, not `EXPECT_EQ(x, std::nullopt)`.
- **Line endings:** the new `.sql` file must be LF. `scripts/format.bat` must not touch `.bat` files, which are CRLF in the working tree. If any shows in `git diff`, restore it.
- **`lua-api.ts` is a template string.** Keep every backtick escaped (`\``). No sol2 or Lua binding code changes, so there is no Debug/Release sol2 concern here.
- **Leftover temp files:** the C++ OmittedParent test reuses one temp path for the Parent CSV and the Child export, and removes it after the re-import. Keep that `fs::remove` after the second `import_csv`.
- **Other plans:** plan 53 moves `execute` into `Impl`, so it must also rewrite the two new `execute(...)` calls. Plan 22 changes the C API query entry points, so it must also update the two new `quiver_database_query_integer` calls in `tests/test_c_api_database_csv_import.cpp`. Plan 58 must preserve the order, the upsert and the duplicate check.

## Out of scope

- Lifting the "refuses inside a transaction" precondition. The user's decision is pending; only its rationale text changes here.
- Replacing import's hand-rolled `impl_->begin_transaction/commit/rollback` with `TransactionGuard`, and `Impl::exec` (plan 60).
- The `convert_cell` pass, merging the two catch tails, and the shared import/export group lookup (plan 58).
- Documenting import's replace-the-table semantics in the agent-facing Lua reference (plan 43).
- Enforcing CASCADE parent FKs on set and time-series tables (plan 06).
- Allowing `UNIQUE` value swaps between kept elements in one import. This would need deferred constraints or a two-phase write, and no schema in the repo has such a column.
- Noticed but owned by no plan: the root `AGENTS.md` sentence "(self-references are excluded on both sides, since the target rows are the ones being rewritten)" in the export/import FK paragraph is stale. Export now writes a self-reference's label (`ExportImportCSV_SelfForeignKeyRoundTrips`). It is left untouched here.
