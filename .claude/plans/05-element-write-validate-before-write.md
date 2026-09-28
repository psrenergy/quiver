# 05 — create_element/update_element: validate every array before the first write

**Batch** 1 · **Severity** m · **Breaking** no · **Size** M · **Layers** C++ core (`src/database_impl.h`, `src/database_create.cpp`, `src/database_update.cpp`), tests in C++ / C API / Lua / Julia / Dart / Python / JS, docs (root `AGENTS.md`, `src/AGENTS.md`, `CHANGELOG.md`)
**Depends on** none · **Overlaps with** 53 (drops the `Database& db` parameter from every Impl helper, including the ones this plan adds or renames), 55 (turns `type_validator->validate_array` into a free function; the call moves into the new `validate_group_columns`), 56 (changes `resolve_fk_label`, and may add a `caller` parameter that `resolve_scalar_fk_labels` then has to pass through), 57 (replaces the table lookup at the top of `update_group_rows`), 37 (adds tests to the same Dart/Julia transaction test files). Only file overlap with 37. For 53/55/56/57, this plan lands first and those plans re-anchor on the names introduced here.

## Why

`create_element` and `update_element` write the element's scalar row first, and only then route, type-check and length-check its arrays. Group tables are also checked one at a time, each just before its own DELETE. `Impl::TransactionGuard` does nothing when a transaction is already open (`src/database_impl.h`, `TransactionGuard` constructor, currently ~L409):

```cpp
explicit TransactionGuard(Impl& impl) : impl_(impl) {
    if (sqlite3_get_autocommit(impl_.db)) {
```

So inside a caller-owned transaction or a dry run, a rejected call leaves its earlier writes in place, and the caller's `commit()` keeps them. The common way to hit this is a Lua `pcall` inside `db:transaction`.

The current order:

- `src/database_update.cpp`, `Database::update_element`: `Impl::TransactionGuard txn(*impl_);` (~L25), then `execute(sql, parameters);` for the scalar UPDATE (~L50), then `impl_->insert_group_data("update_element", collection, id, resolved.arrays, true, *this);` (~L54).
- `src/database_create.cpp`, `Database::create_element`: `execute(sql, parameters);` (~L42), then `impl_->insert_group_data("create_element", collection, element_id, resolved.arrays, false, *this);` (~L47).
- `src/database_impl.h`, `insert_group_data` (~L292-347). This is where an unknown array throws (`"' does not match any vector, set, or time series table in collection '"`, ~L313). Its last loop calls `insert_rows_into_group_table` once per table, and that helper validates (`type_validator->validate_array(...)`, ~L256) and then runs the table's DELETE and INSERTs before the next table is validated.

**Reproduced against the current Debug build** (`build/bin/quiver_cli.exe --schema tests/schemas/valid/collections.sql`, Lua script; each call is wrapped in `pcall` between `db:begin_transaction()` and `db:commit()`):

| Call | Error (correct) | State after commit (wrong) |
|---|---|---|
| `update_element("Collection", id, { some_integer = 2, tag = { 1.5 } })` (element had `some_integer = 1`) | `Cannot update_element: type mismatch for array 'tag' index 0: expected TEXT, got REAL` | `some_integer = 2` |
| `create_element("Collection", { label = "X", typo = { 1 } })` | `Cannot create_element: array 'typo' does not match any vector, set, or time series table in collection 'Collection'` | element `X` exists (2 elements) |
| `update_element("Collection", id, { tag = { "new" }, value_int = { 1.5 } })` (element had `tag = {"keep"}`, `value_int = {7}`) | `Cannot update_element: type mismatch for array 'value_int' index 0: expected INTEGER, got REAL` | `tag = {"new"}`. `Collection_set_tags` sorts before `Collection_vector_values`, so it was rewritten before the vector table was checked. |
| `create_element("Collection", { label = "Y", typo = { 1 } })` inside `db:begin_dry_run()` | same "does not match" error | `Y` is readable for the rest of the dry run |

The code already states the invariant it breaks. Both writers carry the comment `// Pre-resolve pass: resolve all FK labels before any writes`, and the group writers already follow it: root `AGENTS.md` says "validation precedes the DELETE so a rejected write cannot leave a cleared group even when `TransactionGuard` owns no transaction (inside a dry run or a caller-owned transaction)", and `Database.UpdateGroupTypeErrorInsideDryRunKeepsExistingRows` pins that. The time-series writers (`update_time_series_group`, `upsert_time_series_row`, `update_time_series_files`) also validate before opening their guard. `create_element` and `update_element` are the only writers left that don't.

There is a second, smaller defect. Arrays are routed twice, once in `resolve_element_fk_labels` (`schema->find_all_tables_for_column`, ~L184) and again in `insert_group_data` (~L311). The first pass resolves FK labels against the **first** matching table only, and its justification is wrong:

```cpp
// Find the first table match for FK resolution
// (FK columns have unique names per schema design, so first match is correct;
```

`SchemaValidator::validate_no_duplicate_attributes` exempts FK columns (`src/schema_validator.cpp`, the `fk_cols.count(col_name) > 0` skip), `tests/schemas/valid/relations.sql` shares `parent_ref` between `Child_vector_refs` and `Child_set_parents`, and `insert_group_data`'s own comment (~L318-322) says the same.

## Constraints and decisions

- **Maintainer decision (binding):** do NOT use SAVEPOINTs. They were rejected on purpose in the v0.3 research: `git show f92af8d:.planning/research/PITFALLS.md`, "Pitfall 4: SAVEPOINT Complexity Leaking Into the Design". That document chose the no-op nested guard instead. `TransactionGuard` is not touched by this plan.
- **Maintainer decision (binding):** use the facts-verifier variant. Split `insert_group_data` into a no-write half (route + FK-resolve against **each** matched table + validate) and a write half. `resolve_element_fk_labels` then handles scalars only, which deletes the first-match loop and its wrong comment.
- **Maintainer decision (binding):** do not move `validate_scalar` in `update_element`. It stays inside the guard, where it already runs before the UPDATE. (The original proposal wanted it moved out of the guard. That change has no effect, so it is dropped.)
- **Maintainer decision (binding):** state plainly that SQLite constraint failures during an INSERT can still leave partial writes inside a caller-owned transaction. This is a documented limitation. Reproduced: `update_element(id, { some_integer = 5, tag = { "a", "a" } })` in a transaction throws `Failed to execute statement: UNIQUE constraint failed: Collection_set_tags.id, Collection_set_tags.tag`, and after commit `some_integer = 5` and `tag = {"a"}`. That stays true after this plan. A NULL cell in a NOT NULL group column behaves the same way: `TypeValidator::validate_value` accepts NULL for every type, and SQLite rejects it at the INSERT. So does a foreign-key violation.
- Root `AGENTS.md`, "Design Decisions" → "Dry runs live on `Database`": the public begin/commit/rollback are absorbed during a dry run. Untouched. This plan changes no transaction semantics, only write order.
- Root `AGENTS.md`, Core API → whole-group writers: validation precedes the DELETE, and a named column with no rows is an error. Untouched. `update_group_rows` keeps its checks and only moves `validate_group_columns` ahead of its guard.
- Root `AGENTS.md`, "*Not yet fixed:*" array fan-out (`matches.size() > 1` writes every matching table and logs a warning). Not fixed here. The fan-out and its warning stay exactly as they are, and `Database.UpdateElementSharedColumnNameWritesEveryMatchingGroup` must keep passing.
- `src/AGENTS.md`, "Group inserts are unified": one shared insert helper and one routing map, and "don't re-grow per-type copies". Kept: there is still one validator and one inserter for vector, set and time-series tables.
- Error messages (root `AGENTS.md`, "C++ Error Message Patterns"): no message text changes. Every throw moves as-is.
- Rejected alternatives:
  - SAVEPOINT in the nested `TransactionGuard` (both verifiers' corrected proposal). Rejected by the maintainer, see above.
  - Keeping the first-match FK resolution and only fixing its comment (policy verifier). Rejected by the maintainer's "facts-verifier variant". Per-table resolution also falls out of the split for free, because each table gets its own resolved vector.
  - Validating inside `insert_rows_into_group_table` and calling it twice (a dry pass, then a real pass). Rejected: it validates everything twice and hides the write/no-write contract behind a flag.
  - Moving the prepare step inside the guard in `update_element`. Not needed: prepare only reads (FK label SELECTs), which is what the existing pre-pass already does before the guard.

Known behaviour consequences, none pinned by any test (I grepped every `Cannot update_element` / `Cannot create_element` / `does not match any` / `must have the same length` assertion in `tests/` and `bindings/`):

1. When one `update_element` call has both an invalid array and a scalar of the wrong type, the array error is now reported first. Scalar types are still checked inside the guard, but array validation now runs before the guard. `create_element` still reports the scalar type error first.
2. An array whose column name is an FK in one matched group table and a plain INTEGER in another (legal, since only FK copies are exempt from the duplicate check) now resolves a string label per table. The plain-INTEGER table then throws `Cannot resolve attribute: ...`. Before, the result depended on which table `find_all_tables_for_column` returned first. No schema in `tests/schemas/` has this shape.

## Changes

### 1. `src/database_impl.h`: replace `ResolvedElement` with `GroupColumns`

Current (~L55-58):

```cpp
struct ResolvedElement {
    std::map<std::string, Value> scalars;
    std::map<std::string, std::vector<Value>> arrays;
};
```

New (same place):

```cpp
// One group table's share of an element write: the element's arrays that route to it, each
// FK-resolved against that table. Built by Impl::prepare_group_data, written by
// Impl::insert_group_data.
struct GroupColumns {
    GroupTableType type;
    std::map<std::string, std::vector<Value>> columns;
};
```

`ResolvedElement` has no other users (grep: only `database_impl.h`, `database_create.cpp` and `database_update.cpp` through `resolve_element_fk_labels`).

### 2. `src/database_impl.h`: `resolve_element_fk_labels` becomes `resolve_scalar_fk_labels`

Delete the whole `resolve_element_fk_labels` function (~L173-211). It starts with `ResolvedElement resolve_element_fk_labels(const std::string& collection, const Element& element, Database& db) {` and contains the `// Find the first table match for FK resolution` loop. Replace it with:

```cpp
    // Resolve FK labels among an element's scalars against the collection table. Arrays are
    // resolved by prepare_group_data, against each group table they are written to.
    std::map<std::string, Value>
    resolve_scalar_fk_labels(const std::string& collection, const std::map<std::string, Value>& scalars, Database& db) {
        const auto& collection_def = *schema->get_table(collection);
        std::map<std::string, Value> resolved;
        for (const auto& [name, value] : scalars) {
            resolved[name] = resolve_fk_label(collection_def, name, value, db);
        }
        return resolved;
    }
```

Why: arrays are now resolved during routing (step 5), so this function is scalar-only. The wrong "FK columns have unique names" comment is deleted with the loop.

### 3. `src/database_impl.h`: extract `validate_group_columns` from `insert_rows_into_group_table`

Current, at the top of `insert_rows_into_group_table` (~L245-262):

```cpp
        const char* noun = group_table_noun(type);

        // Validate types and verify same-length arrays *before* the DELETE: TransactionGuard
        // no-ops inside a caller-owned transaction (or a dry run), so throwing after the DELETE
        // would leave the group cleared with nothing to roll it back.
        // num_rows is seeded from the first column rather than the first non-empty one: `columns`
        // is name-sorted, so an empty alphabetically-first column used to leave it at 0 for a
        // later column to set, skipping the check and indexing the empty vector below.
        size_t num_rows = columns.empty() ? 0 : columns.begin()->second->size();
        for (const auto& [col_name, values_ptr] : columns) {
            if (!values_ptr->empty()) {
                type_validator->validate_array(caller, table_name, col_name, *values_ptr);
            }
            if (values_ptr->size() != num_rows) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": " + noun + " columns in table '" +
                                         table_name + "' must have the same length");
            }
        }
```

Add this new member function directly **above** `insert_rows_into_group_table`:

```cpp
    // Types and equal lengths of the columns bound for one group table. Every writer runs this for
    // every table it will touch *before* its first write: TransactionGuard no-ops inside a
    // caller-owned transaction (or a dry run), so a throw after any write would leave that write in
    // place with nothing to roll it back.
    // num_rows is seeded from the first column rather than the first non-empty one: `columns` is
    // name-sorted, so an empty alphabetically-first column used to leave it at 0 for a later column
    // to set, skipping the check and indexing the empty vector on insert.
    void validate_group_columns(const char* caller,
                                const std::string& table_name,
                                GroupTableType type,
                                const std::map<std::string, std::vector<Value>>& columns) const {
        const size_t num_rows = columns.empty() ? 0 : columns.begin()->second.size();
        for (const auto& [col_name, values] : columns) {
            if (!values.empty()) {
                type_validator->validate_array(caller, table_name, col_name, values);
            }
            if (values.size() != num_rows) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": " + group_table_noun(type) +
                                         " columns in table '" + table_name + "' must have the same length");
            }
        }
    }
```

Keep the `if (!values.empty())` guard. `TypeValidator::validate_array` calls `schema_.get_data_type(table, column)` even for an empty vector, and that throws for a column the routed table lacks.

### 4. `src/database_impl.h`: `insert_rows_into_group_table` becomes write-only

Replace the whole function (~L238-290, currently `void insert_rows_into_group_table(const char* caller, ... const std::map<std::string, const std::vector<Value>*>& columns, ...)`) with:

```cpp
    // DELETE (when replacing) and INSERT one element's rows in one group table; vector tables get a
    // 1-based vector_index. Callers run validate_group_columns first, so every column has the same
    // length. What can still throw here is only what SQLite checks (UNIQUE, NOT NULL, foreign
    // keys), after this call's earlier writes - a documented limit inside a caller-owned
    // transaction (root AGENTS.md design decisions).
    void insert_rows_into_group_table(const std::string& table_name,
                                      GroupTableType type,
                                      const std::map<std::string, std::vector<Value>>& columns,
                                      int64_t element_id,
                                      bool delete_existing,
                                      Database& db) {
        if (delete_existing) {
            db.execute("DELETE FROM " + table_name + " WHERE id = ?", {element_id});
        }

        const size_t num_rows = columns.empty() ? 0 : columns.begin()->second.size();
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto sql = "INSERT INTO " + table_name + " (id";
            std::string placeholders = "?";
            std::vector<Value> parameters = {element_id};

            if (type == GroupTableType::Vector) {
                sql += ", vector_index";
                placeholders += ", ?";
                parameters.emplace_back(static_cast<int64_t>(row_idx + 1));
            }

            for (const auto& [col_name, values] : columns) {
                sql += ", " + col_name;
                placeholders += ", ?";
                parameters.push_back(values[row_idx]);
            }

            sql += ") VALUES (" + placeholders + ")";
            db.execute(sql, parameters);
        }
        logger->debug("Inserted {} {} rows into {}", num_rows, group_table_noun(type), table_name);
    }
```

Why: the `caller` parameter only fed the validation messages, so it goes. Columns are now owned vectors (`std::map<std::string, std::vector<Value>>`) instead of pointers: each table gets its own FK-resolved copy (step 5), and `update_group_rows` already owns its transposed columns (step 8).

### 5. `src/database_impl.h`: `insert_group_data` splits into `prepare_group_data` + `insert_group_data`

Replace the whole current `insert_group_data` (~L292-347, starting `void insert_group_data(const char* caller,` and ending with the `insert_rows_into_group_table(` loop) with:

```cpp
    // The no-write half of create_element / update_element's arrays: route each array to the group
    // table(s) holding its column, FK-resolve it against each of them, and validate every table.
    // Callers run it before their scalar INSERT/UPDATE (validate_group_columns says why) and hand
    // the result to insert_group_data.
    std::map<std::string, GroupColumns> prepare_group_data(const char* caller,
                                                           const std::string& collection,
                                                           const std::map<std::string, std::vector<Value>>& arrays,
                                                           bool delete_existing,
                                                           Database& db) {
        std::map<std::string, GroupColumns> tables;

        for (const auto& [array_name, values] : arrays) {
            // Empty array handling: create skips silently, update still routes (for DELETE)
            if (values.empty() && !delete_existing) {
                continue;
            }

            auto matches = schema->find_all_tables_for_column(collection, array_name);
            if (matches.empty()) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": array '" + array_name +
                                         "' does not match any vector, set, or time series table in collection '" +
                                         collection + "'");
            }

            // A column name shared by several group tables (legal for FK columns, which
            // validate_no_duplicate_attributes exempts) cannot say which group was meant, so the
            // array lands in all of them - rewriting groups the caller never named. Kept for
            // compatibility, but say so: update_vector_group / update_set_group take
            // (collection, group) and name exactly one table.
            if (matches.size() > 1) {
                std::string table_list;
                for (const auto& match : matches) {
                    table_list += (table_list.empty() ? "" : ", ") + match.table_name;
                }
                logger->warn("{}: array '{}' matches {} group tables ({}) and will be written to all of "
                             "them; use update_vector_group/update_set_group to target one group",
                             caller,
                             array_name,
                             matches.size(),
                             table_list);
            }

            // Resolved per table: a shared FK column name may point at a different target in each
            // group that holds it.
            for (const auto& match : matches) {
                const auto& table_def = *schema->get_table(match.table_name);
                auto& entry = tables[match.table_name];
                entry.type = match.type;
                auto& resolved = entry.columns[array_name];
                resolved.reserve(values.size());
                for (const auto& value : values) {
                    resolved.push_back(resolve_fk_label(table_def, array_name, value, db));
                }
            }
        }

        for (const auto& [table_name, entry] : tables) {
            validate_group_columns(caller, table_name, entry.type, entry.columns);
        }
        return tables;
    }

    // The write half: add (create) or replace (update) the element's rows in every prepared table.
    void insert_group_data(const std::map<std::string, GroupColumns>& tables,
                           int64_t element_id,
                           bool delete_existing,
                           Database& db) {
        for (const auto& [table_name, entry] : tables) {
            insert_rows_into_group_table(table_name, entry.type, entry.columns, element_id, delete_existing, db);
        }
    }
```

Notes:
- The unknown-array throw and the fan-out warning are moved verbatim.
- `*schema->get_table(match.table_name)` is dereferenced without a null check. `find_all_tables_for_column` (`src/schema.cpp`) only returns tables that exist (`has_table(vt)` / iteration over `tables_`), so the old `if (td)` check was dead code.
- Validation runs after **all** routing and resolution, so no table is written until every table has passed.

### 6. `src/database_create.cpp`: `Database::create_element`

Current (~L14-20):

```cpp
    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    // Validate resolved scalar types
    for (const auto& [name, value] : resolved.scalars) {
        impl_->type_validator->validate_scalar("create_element", collection, name, value);
    }
```

New:

```cpp
    // Resolve and validate every scalar and array before the INSERT: TransactionGuard no-ops inside
    // a caller-owned transaction or a dry run, so a throw after it would leave the element behind.
    auto resolved = impl_->resolve_scalar_fk_labels(collection, scalars, *this);
    for (const auto& [name, value] : resolved) {
        impl_->type_validator->validate_scalar("create_element", collection, name, value);
    }
    auto groups = impl_->prepare_group_data("create_element", collection, element.arrays(), false, *this);
```

In the INSERT builder loop (~L30), change `for (const auto& [name, value] : resolved.scalars) {` to `for (const auto& [name, value] : resolved) {`.

Current (~L46-47):

```cpp
    // Delegate group insertion to shared helper (empty arrays are skipped silently)
    impl_->insert_group_data("create_element", collection, element_id, resolved.arrays, false, *this);
```

New:

```cpp
    // prepare_group_data already dropped empty arrays; everything left was validated above.
    impl_->insert_group_data(groups, element_id, false, *this);
```

`scalars` is the existing local `const auto& scalars = element.scalars();` (~L9).

### 7. `src/database_update.cpp`: `Database::update_element`

Current (~L22-25):

```cpp
    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    Impl::TransactionGuard txn(*impl_);
```

New:

```cpp
    // Resolve every FK label and validate every array before the UPDATE: TransactionGuard no-ops
    // inside a caller-owned transaction or a dry run, so a throw after it would leave the scalar
    // update behind. (Scalar types are checked below, still ahead of the UPDATE.)
    auto resolved = impl_->resolve_scalar_fk_labels(collection, scalars, *this);
    auto groups = impl_->prepare_group_data("update_element", collection, arrays, true, *this);

    Impl::TransactionGuard txn(*impl_);
```

In the scalar block, replace every `resolved.scalars` with `resolved`. There are three: `if (!resolved.scalars.empty()) {` (~L28), the `validate_scalar` loop (~L30), and the SET-builder loop (~L39). **Leave the `validate_scalar` loop where it is**, inside the guard and before `execute(sql, parameters);` (maintainer decision).

Current (~L53-54):

```cpp
    // Delegate group insertion to shared helper (delete_existing=true for updates)
    impl_->insert_group_data("update_element", collection, id, resolved.arrays, true, *this);
```

New:

```cpp
    // Replace every routed group (delete_existing=true: an empty array clears its group)
    impl_->insert_group_data(groups, id, true, *this);
```

`scalars` and `arrays` are the existing locals at ~L13-14.

### 8. `src/database_update.cpp`: `Database::Impl::update_group_rows`

Current tail (~L188-203):

```cpp
    // Resolve FK labels before any writes, so a failed lookup cannot leave a cleared group.
    auto columns = transpose_group_rows(rows, names);
    for (auto& [col_name, values] : columns) {
        for (auto& value : values) {
            value = resolve_fk_label(*table_def, col_name, value, db);
        }
    }

    std::map<std::string, const std::vector<Value>*> column_ptrs;
    for (const auto& [col_name, values] : columns) {
        column_ptrs[col_name] = &values;
    }

    TransactionGuard txn(*this);
    insert_rows_into_group_table(caller, table_name, type, column_ptrs, id, true, db);
    txn.commit();
```

New:

```cpp
    // Resolve FK labels and validate before any writes, so a failed lookup or a bad cell cannot
    // leave a cleared group.
    auto columns = transpose_group_rows(rows, names);
    for (auto& [col_name, values] : columns) {
        for (auto& value : values) {
            value = resolve_fk_label(*table_def, col_name, value, db);
        }
    }
    validate_group_columns(caller, table_name, type, columns);

    TransactionGuard txn(*this);
    insert_rows_into_group_table(table_name, type, columns, id, true, db);
    txn.commit();
```

This deletes the `column_ptrs` block. Validation moves ahead of the guard, which changes nothing observable: it runs no SQL, and before this plan it already ran before the DELETE. The comment above `transpose_group_rows` ("...the column-shaped form insert_rows_into_group_table expects") stays accurate. No other file calls these helpers. I grepped `insert_group_data|resolve_element_fk_labels|insert_rows_into_group_table|ResolvedElement` across `src/`, `tests/`, `include/` and `bindings/`: only `database_impl.h`, `database_create.cpp` and `database_update.cpp`.

### 9. C API, FFI declarations, bindings, Lua

No code change. No C API signature changes, so no generator run and no edits to `_c_api.py`, `loader.ts` or `bindings.dart`. Every binding and `src/lua_runner.cpp` reaches the fix through `Database::create_element` / `update_element`. Only tests are added (next section).

## Tests

None of the tests below exist today. Each one fails before the fix: the rejected call's earlier write is still visible (reproduced through `quiver_cli.exe`, see Why). After the fix, each passes. No existing test needs changing. All existing assertions stay valid, including `UpdateGroupTypeErrorInsideDryRunKeepsExistingRows`, `UpdateElementEmptyFirstColumnOfMultiColumnGroupThrows`, `UpdateElementSharedColumnNameWritesEveryMatchingGroup`, `CreateElementWithEmptyArraySkipsSilently`, `UpdateElementEmptyArrayClearsRows`, `ResolveFkLabelMissingTarget` (`EXPECT_STREQ "Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'"`), the vector/set/time-series FK label test in `test_database_create.cpp` (`parent_ref` resolved through both `Child_vector_refs` and `Child_set_parents`), and `UpdateElementInvalidDateTimeArray`.

Do **not** add a test that pins the SQLite-constraint limitation (`tag = {"a","a"}`) as expected behaviour. It is documented, not desired.

### C++: `tests/test_database_update.cpp`

Insert directly after `TEST(Database, UpdateGroupTypeErrorInsideDryRunKeepsExistingRows)` (currently ~L1373-1385), before the `// Update by label tests` banner:

```cpp
// TransactionGuard no-ops inside a caller-owned transaction, so update_element must route and
// validate every array before its scalar UPDATE - a throw after it left the UPDATE for the
// caller's commit.
TEST(Database, UpdateElementRejectedArrayInsideTransactionKeepsScalar) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
    auto id = db.create_element(
        "Collection", quiver::Element().set("label", std::string("Item 1")).set("some_integer", int64_t{1}));

    db.begin_transaction();
    try {
        db.update_element(
            "Collection", id, quiver::Element().set("some_integer", int64_t{2}).set("tag", std::vector<double>{1.5}));
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot update_element: type mismatch for array 'tag' index 0: expected TEXT, got REAL");
    }
    db.commit();

    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", id);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 1);
}

// Each group table used to be validated just before its own DELETE, and tables are written in
// name order, so Collection_set_tags was already rewritten when Collection_vector_values failed.
TEST(Database, UpdateElementRejectedArrayKeepsEarlierGroup) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
    auto id = db.create_element("Collection",
                                quiver::Element()
                                    .set("label", std::string("Item 1"))
                                    .set("tag", std::vector<std::string>{"keep"})
                                    .set("value_int", std::vector<int64_t>{7}));

    db.begin_transaction();
    EXPECT_THROW(db.update_element("Collection",
                                   id,
                                   quiver::Element()
                                       .set("tag", std::vector<std::string>{"new"})
                                       .set("value_int", std::vector<double>{1.5})),
                 std::runtime_error);
    db.commit();

    EXPECT_EQ(db.read_set_strings_by_id("Collection", "tag", id), (std::vector<std::string>{"keep"}));
    EXPECT_EQ(db.read_vector_integers_by_id("Collection", "value_int", id), (std::vector<int64_t>{7}));
}
```

Before the fix: the first test reads `2` and the second reads `{"new"}`.

### C++: `tests/test_database_create.cpp`

Insert directly after `TEST(Database, ScalarFkResolutionFailureCausesNoPartialWrites)` (currently ~L682-695):

```cpp
// TransactionGuard no-ops inside a dry run, so an array rejected after the INSERT used to leave the
// element readable for the rest of the dry run (a Lua script that pcall'd the error saw it).
TEST(Database, CreateElementRejectedArrayInsideDryRunLeavesNoElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", std::string("Config")));

    db.begin_dry_run();
    try {
        db.create_element("Collection",
                          quiver::Element().set("label", std::string("X")).set("typo", std::vector<int64_t>{1}));
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(),
                     "Cannot create_element: array 'typo' does not match any vector, set, or time series table in "
                     "collection 'Collection'");
    }
    EXPECT_EQ(db.number_of_elements("Collection"), 0);
    db.end_dry_run();
}
```

Before the fix: `number_of_elements` is `1`.

### C API: `tests/test_c_api_database_transaction.cpp`

Append at the end of the file, after `TEST(DatabaseCApi, InTransactionReflectsState)`:

```cpp
// The core validates every array before update_element's first write, so a rejected call inside a
// caller-owned transaction leaves nothing for the commit to persist.
TEST(DatabaseCApi, TransactionRejectedUpdateElementWritesNothing) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    quiver_element_destroy(config);

    quiver_element_t* item = nullptr;
    ASSERT_EQ(quiver_element_create(&item), QUIVER_OK);
    quiver_element_set_string(item, "label", "Item 1");
    quiver_element_set_integer(item, "some_integer", 1);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", item, &id), QUIVER_OK);
    quiver_element_destroy(item);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "some_integer", 2);
    const double tags[] = {1.5};
    ASSERT_EQ(quiver_element_set_array_float(update, "tag", tags, 1, nullptr), QUIVER_OK);
    EXPECT_EQ(quiver_database_update_element(db, "Collection", id, update), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(),
                 "Cannot update_element: type mismatch for array 'tag' index 0: expected TEXT, got REAL");
    quiver_element_destroy(update);

    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);

    int64_t value = 0;
    int has_value = 0;
    ASSERT_EQ(quiver_database_read_scalar_integer_by_id(db, "Collection", "some_integer", id, &value, &has_value),
              QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 1);

    quiver_database_close(db);
}
```

### Lua: `tests/test_lua_runner_transaction.cpp`

Insert directly after `TEST_F(LuaRunnerTest, TransactionBlockRollbackOnError)` (currently ~L104-123):

```cpp
// A script that catches an error inside db:transaction commits whatever the failed call left
// behind, so a rejected update_element must leave nothing.
TEST_F(LuaRunnerTest, TransactionBlockCaughtRejectedUpdateWritesNothing) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    auto id = db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{1}));
    ASSERT_EQ(id, 1);

    quiver::LuaRunner lua(db);
    lua.run(R"(
        db:transaction(function(db)
            local ok, err = pcall(function()
                db:update_element("Collection", 1, { some_integer = 2, tag = { 1.5 } })
            end)
            assert(ok == false, "expected update_element to fail")
            assert(err:find("type mismatch for array 'tag'", 1, true) ~= nil, "unexpected error: " .. tostring(err))
        end)
    )");

    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", id);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 1);
}
```

### Julia: `bindings/julia/test/test_database_transaction.jl`

Insert a new `@testset` after the `@testset "Multi-operation batch"` block, before the closing `end` of `@testset "Transaction"`:

```julia
    @testset "Rejected update_element inside a transaction writes nothing" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 1)

        Quiver.begin_transaction!(db)
        exc = @test_throws Quiver.DatabaseException Quiver.update_element!(
            db, "Collection", id;
            some_integer = 2,
            tag = [1.5],
        )
        @test occursin("type mismatch for array 'tag'", exc.value.msg)
        Quiver.commit!(db)

        @test Quiver.read_scalar_integer_by_id(db, "Collection", "some_integer", id) == 1

        Quiver.close!(db)
    end
```

### Dart: `bindings/dart/test/database_transaction_test.dart`

Insert a new `test(...)` after `test('multi-operation batch', ...)`, inside `group('Transaction', ...)`:

```dart
    test('rejected updateElement inside a transaction writes nothing', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        final id = db.createElement('Collection', {'label': 'Item 1', 'some_integer': 1});

        db.beginTransaction();
        expect(
          () => db.updateElement('Collection', id, {
            'some_integer': 2,
            'tag': [1.5],
          }),
          throwsA(
            isA<DatabaseException>().having(
              (e) => e.message,
              'message',
              contains("type mismatch for array 'tag'"),
            ),
          ),
        );
        db.commit();

        expect(db.readScalarIntegerById('Collection', 'some_integer', id), equals(1));
      } finally {
        db.close();
      }
    });
```

### Python: `bindings/python/tests/test_database_transaction.py`

Change the import `from quiverdb import Database` to `from quiverdb import Database, QuiverError`. Add to `class TestExplicitTransaction`, after `test_in_transaction_false_after_commit`:

```python
    def test_rejected_update_writes_nothing(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="Config")
        item_id = collections_db.create_element("Collection", label="Item 1", some_integer=1)

        collections_db.begin_transaction()
        with pytest.raises(QuiverError, match="type mismatch for array 'tag'"):
            collections_db.update_element("Collection", item_id, some_integer=2, tag=[1.5])
        collections_db.commit()

        assert collections_db.read_scalar_integer_by_id("Collection", "some_integer", item_id) == 1
```

### JS: `bindings/js/test/database-transaction.test.ts`

The file already uses `SCHEMA_PATH` = `valid/all_types.sql`, where `AllTypes_set_tags.tag` is `TEXT NOT NULL`, and it imports `QuiverError`. Add inside `describe("transaction control", ...)`, after `test("inTransaction returns false after commit", ...)`:

```ts
  test("a rejected updateElement inside a transaction writes nothing", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item 1", some_integer: 1 });
      db.beginTransaction();
      expect(() => db.updateElement("AllTypes", id, { some_integer: 2, tag: [1.5] })).toThrow(
        /type mismatch for array 'tag'/,
      );
      db.commit();
      expect(db.readScalarIntegerById("AllTypes", "some_integer", id)).toEqual(1);
    } finally {
      db.close();
    }
  });
```

No new schema files. `tests/AGENTS.md` needs no change: no new files, and the file-per-area map is unchanged.

## Docs and changelog

### Root `AGENTS.md`, "Design Decisions"

Insert a new bullet directly after the "**Dry runs live on `Database`, not on `LuaRunner`.**" bullet. That bullet ends "...a script that wants a change count already has `SELECT total_changes()`."

```markdown
- **Writers check everything before their first write, and there are no SAVEPOINTs.**
  `Impl::TransactionGuard` no-ops inside a caller-owned transaction or a dry run, so it cannot roll
  back a failed call there. Every writer therefore resolves and validates all of its input before
  its first INSERT/UPDATE/DELETE. The group and time-series writers always did; `create_element` /
  `update_element` used to write the scalar row before checking their arrays (a Lua `pcall` inside
  `db:transaction` committed half a call), and now route, FK-resolve and validate every array in
  `Impl::prepare_group_data` first. The documented limit is what only SQLite checks: a UNIQUE,
  NOT NULL or foreign-key failure on an INSERT (a set written `{"a", "a"}`, a NULL cell in a
  NOT NULL group column) still throws after the call's earlier writes, and inside a caller-owned
  transaction those writes stay for the commit. Autocommit calls are still all-or-nothing. A
  SAVEPOINT per nested guard would close that gap and was rejected in the v0.3 research
  (`git show f92af8d:.planning/research/PITFALLS.md`, Pitfall 4) as the nesting complexity the
  no-op guard exists to avoid.
```

### `src/AGENTS.md`, "Transactions" section

Old sentence (~L315):

> Internally, `Impl::TransactionGuard` is nest-aware RAII: if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`), the guard becomes a no-op. This allows write methods (`create_element`, etc.) to work both standalone and inside explicit transactions without double-beginning.

New:

> Internally, `Impl::TransactionGuard` is nest-aware RAII: if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`), the guard becomes a no-op. This allows write methods (`create_element`, etc.) to work both standalone and inside explicit transactions without double-beginning. A no-op guard cannot roll anything back, so every writer finishes its checks before its first write (see "Group writes" under Core Internals). Only a failure SQLite alone detects mid-write (UNIQUE / NOT NULL / foreign key) can leave a call's earlier writes inside a caller-owned transaction. That is a documented limit, and there are no SAVEPOINTs (root design decisions).

### `src/AGENTS.md`, "Core Internals Worth Knowing", first bullet

Replace the whole bullet that starts `- **Group inserts are unified** (\`database_impl.h\`): one \`insert_rows_into_group_table(caller,` (~L375-384) with:

```markdown
- **Group writes are unified, and checked before anything is written** (`database_impl.h`): one
  `validate_group_columns(caller, table, type, columns)` checks types and equal lengths, and one
  `insert_rows_into_group_table(table, type, columns, id, delete_existing, db)` does the DELETE and
  INSERTs, for vector, set and time-series tables alike. For `create_element` / `update_element`,
  `prepare_group_data` routes each array to its table(s) through a single
  `table_name -> GroupColumns` map, FK-resolves it against **each** table it is written to (a
  shared FK column name may target a different collection per group), and validates every table;
  the caller runs it **before** its scalar INSERT/UPDATE and hands the result to
  `insert_group_data` afterwards. `update_group_rows` calls `validate_group_columns` before its
  guard the same way. Don't re-grow per-type copies, and don't move a check back past the first
  write: `TransactionGuard` no-ops when a transaction is already open (dry run, caller-owned), so a
  throw after any write leaves it in place for the caller's commit. What remains is only what
  SQLite checks at INSERT time (UNIQUE, NOT NULL, foreign keys) - documented, not fixed (root
  design decisions). `num_rows` is seeded from the **first** column rather than the first
  non-empty one, because `columns` is name-sorted and an empty alphabetically-first column
  otherwise left it at 0 for a later column to overwrite — skipping the check and indexing past
  the end of the empty vector.
```

### `src/AGENTS.md`, `_by_label` sentence (~L452)

Old: ``*element* validation — the empty-element throw, `TypeValidator`, `insert_group_data` — reports``
New: ``*element* validation — the empty-element throw, `TypeValidator`, `prepare_group_data` — reports``

### `CHANGELOG.md`

Under `## [0.12.0] — unreleased` → `### Fixed`, add as the **first** bullet:

```markdown
- **A rejected `create_element()` / `update_element()` no longer leaves part of its write behind
  inside a transaction or dry run.** Both wrote the element's scalar row before routing and
  validating its arrays, and rewrote each group table before checking the next. Inside a
  caller-owned transaction (for example a Lua `pcall` inside `db:transaction`) or a dry run, a call
  rejected for an unknown array, a type mismatch or unequal lengths still left the new element, the
  updated scalars or an already-rewritten group in place for the commit. Every array is now routed,
  FK-resolved and validated before the first write, in every binding. A failure only SQLite can
  detect — a duplicate value in a set, a NULL in a NOT NULL group column, a foreign-key violation
  — still happens mid-write, and inside a caller-owned transaction the call's earlier writes stay;
  outside one the call is rolled back as before.
```

No other docs: `docs/*.md` and the READMEs make no atomicity claim (I grepped `atomic|partial|all-or-nothing|rolled back`), and the Lua reference (`bindings/js/src/lua-api.ts`) says nothing about per-call atomicity, so nothing there goes stale.

## Verification

From `C:\Development\Quiver\quiver1`:

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter='Database.UpdateElementRejectedArrayInsideTransactionKeepsScalar:Database.UpdateElementRejectedArrayKeepsEarlierGroup:Database.CreateElementRejectedArrayInsideDryRunLeavesNoElement:LuaRunnerTest.TransactionBlockCaughtRejectedUpdateWritesNothing'`. All 4 pass. To confirm they are real regressions, run `git stash push -- src/` (this stashes only the core change and keeps the new tests), rebuild, and rerun the same filter: all 4 fail, with `some_integer` read as 2, the tags read as `{"new"}`, and `number_of_elements` returning 1. Then run `git stash pop` and rebuild.
3. `./build/bin/quiver_tests.exe --gtest_filter='Database.*Element*:Database.*Group*:Database.ResolveFkLabel*:DatabaseTransaction.*:DatabaseDryRun.*:LuaRunnerTest.*:LuaRunnerFkTest.*'`. All pass, including `UpdateGroupTypeErrorInsideDryRunKeepsExistingRows`, `UpdateElementSharedColumnNameWritesEveryMatchingGroup` and `UpdateElementEmptyFirstColumnOfMultiColumnGroupThrows`.
4. `./build/bin/quiver_tests.exe`: full suite passes.
5. `./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.TransactionRejectedUpdateElementWritesNothing'` passes, then `./build/bin/quiver_c_tests.exe` passes in full.
6. `bindings/julia/test/test.bat`: the new "Rejected update_element inside a transaction writes nothing" testset passes.
7. `bindings/dart/test/test.bat`: the new 'rejected updateElement inside a transaction writes nothing' test passes.
8. `bindings/js/test/test.bat`: the new "a rejected updateElement inside a transaction writes nothing" test passes.
9. `bindings/python/tests/test.bat`: `test_rejected_update_writes_nothing` passes.
10. No generator run: no C API header changed.
11. `scripts/format.bat`, then `git diff --stat`. Only the files listed in this plan change, and no `.bat` file appears.
12. `scripts/test-all.bat`: all seven steps pass.

## Acceptance criteria

- [x] `ResolvedElement` and `resolve_element_fk_labels` are gone. `grep -rn "resolve_element_fk_labels\|ResolvedElement\|FK columns have unique names" src/` returns nothing.
- [x] `prepare_group_data`, `validate_group_columns`, `insert_rows_into_group_table` (no `caller` parameter, owned-vector columns), `insert_group_data` (takes the prepared map) and `resolve_scalar_fk_labels` exist in `src/database_impl.h`, with the bodies above.
- [x] `create_element` calls `prepare_group_data` before `Impl::TransactionGuard`. `update_element` calls it before `Impl::TransactionGuard`, and its `validate_scalar` loop has not moved.
- [x] `update_group_rows` calls `validate_group_columns` before its guard and no longer builds `column_ptrs`.
- [x] No error message text changed (`git diff` shows the three throw strings moved verbatim).
- [x] New tests pass in C++ (3), C API (1), Lua (1), Julia, Dart, Python and JS (1 each), and each C++/C/Lua one fails on the pre-change build. *(JS and Python were also shown failing before the fix.)*
- [x] Root `AGENTS.md` has the new design-decision bullet. `src/AGENTS.md` Transactions paragraph, the Core Internals bullet and the `_by_label` sentence are updated. `CHANGELOG.md` has the Fixed entry.
- [x] No SAVEPOINT anywhere in code. *(Checked with `grep -rni savepoint src/ include/ --include=*.h --include=*.cpp`, which is empty. The plan's unscoped grep matches the new `src/AGENTS.md` sentence "there are no SAVEPOINTs"; see deviation 3.)*
- [ ] `scripts/test-all.bat` passes. *(6 of 7 steps PASS. Step 7, the CLI smoke test, FAILS because `example/example1.lua` no longer exists; plan 65 fixes this, and it failed before this change too.)*

## Pitfalls

- **Do not add a SAVEPOINT and do not touch `TransactionGuard`.** That was rejected by the maintainer. Plan 60 owns `TransactionGuard`'s internals.
- **Keep `update_element`'s `validate_scalar` loop inside the guard** (maintainer decision). Only the array preparation moves ahead of the guard.
- **Validate after routing every array, not per array.** The same-length check is per *table* and needs all of that table's columns. Validating inside the routing loop would reject a table before its second column has been added.
- **Keep the empty-array skip in `prepare_group_data`** (`values.empty() && !delete_existing`). `create_element` must silently skip an empty array, even one whose name matches nothing (`CreateElementWithEmptyArraySkipsSilently`, `DatabaseErrors.CreateElementEmptyArraySkipsSilently`). `update_element` must still route it so the DELETE clears the group (`UpdateElementEmptyArrayClearsRows`).
- **Keep `if (!values.empty())` around `validate_array`.** Without it, an empty update array routed to a table that lacks the column throws a raw `Column 'x' not found` from `Schema::get_data_type` instead of clearing.
- **`validate_group_columns` is `const` and `type_validator` is a `mutable std::unique_ptr`.** That compiles as is. Don't make it `static`, because it needs the member.
- **Other plans will re-anchor on these names.** 53 drops `Database& db` from these helpers, 55 replaces `type_validator->validate_array`, and 56 may thread a `caller` into `resolve_fk_label` (and so into `resolve_scalar_fk_labels`). Keep the names exactly as given here so those plans' greps find them.
- **Error precedence changes slightly** (see Constraints). No test pins it. If a binding test that is not listed here starts failing on a mixed scalar-plus-array error, the reason is that the array error is now reported first. Update the assertion, not the order.
- **The Lua test prints `[sol2] An exception occurred: ...` on stderr** when the C++ exception crosses the `pcall`. That is expected noise, not a failure.
- **Line endings:** no `.bat` file is edited. If `scripts/format.bat` or an editor touches one, restore CRLF.
- **Dart map literal:** `{'some_integer': 2, 'tag': [1.5]}` is a `Map<String, Object>`, which passes as the `Map<String, Object?>` parameter. `[1.5]` hits `Element.set`'s `List<double>` case.
- **JS:** `[1.5]` is sent as a float array because `Number.isInteger(1.5)` is false (`setElementArray` in `bindings/js/src/create.ts`). Don't use `[1.0]`: JS would send it as an integer array, the core would reject it with a different message ("expected TEXT, got INTEGER"), and the `/type mismatch for array 'tag'/` regex would still match, hiding a mistake.

## Out of scope

- Closing the SQLite-constraint gap (UNIQUE / NOT NULL / FK after earlier writes). It stays a documented limitation, and SAVEPOINTs are rejected.
- The array fan-out (`matches.size() > 1` writing every matching table). This is the root `AGENTS.md` "*Not yet fixed*" item. The warning and behaviour are unchanged here.
- Adding the `caller` operation name to `resolve_fk_label`'s messages, and the typing-policy unification: plan 56.
- Dropping the `Database& db` back-references: plan 53. `TypeValidator` as free functions: plan 55. The `update_group_rows` table lookup / `require_group_table`: plan 57. `TransactionGuard` / `Impl::exec`: plan 60.
- Lua reference (`bindings/js/src/lua-api.ts`) wording about transactions and rollback: plans 43/44. Nothing there is made stale by this plan.

## Implementation notes

Implemented on `rs/plan5` at HEAD `0813296` (the `bump-version/0.12.3` merge, after plan 04's `ba9669a`). The core code (Changes 1-8) and all nine tests are the plan's, apart from the deviations listed below. A read-only verification pass matched every quoted excerpt, symbol, signature, fixture and test anchor, in all seven layers, before any edit. No C API, FFI, binding or Lua code changed, so the generators were not run.

**Red/green.** The failures before the fix were exactly as predicted:
- `CreateElementRejectedArrayInsideDryRunLeavesNoElement`: `number_of_elements` was `1`.
- `UpdateElementRejectedArrayInsideTransactionKeepsScalar`: `*value` was `2`.
- `UpdateElementRejectedArrayKeepsEarlierGroup`: `read_set_strings_by_id` returned `{ "new" }`.
- `LuaRunnerTest.TransactionBlockCaughtRejectedUpdateWritesNothing`: `*value` was `2`.
- `DatabaseCApi.TransactionRejectedUpdateElementWritesNothing`: `value` was `2`.
- The new JS test failed with `Expected: 1 Received: 2`, and the new Python test failed as well.

After the fix:
- The 5 C++/C API/Lua tests pass.
- Verification step 3's filter: 346/346.
- `quiver_tests`: 1312/1312.
- `quiver_c_tests`: 564/564.
- Julia: 1443/1443.
- Dart: 421/421, after clearing the native cache.
- JS: 210/210.
- Python: 307/307.
- `scripts/test-all.bat`: steps 1-6 PASS (C++ 1312, C API 564, Julia 1443, Dart 421, JS 210, Python 307); step 7, the CLI smoke test, FAILS as it did before this plan (`Script file not found: ...\example\example1.lua`, which is plan 65).

The strengthened `UpdateElementRejectedArrayKeepsEarlierGroup` (see deviation 5) was also re-run against the stashed pre-fix core. It still fails at the `{ "new" }` read, so the pinned error is the pre-fix error too.

**Deviations (all small):**
1. **The CHANGELOG entry sits under a new `## [0.12.3] — unreleased` → `### Fixed` section above `[0.12.2]`.** The plan said `[0.12.0]`. `v0.12.0`, `v0.12.1` and `v0.12.2` are all tagged now, and the manifests are already at 0.12.3 (#310). No manifest bump.
2. **Only line numbers drifted:**
   - `UpdateGroupTypeErrorInsideDryRunKeepsExistingRows` is at L1376 and the banner is a 3-line block. The new tests go above the whole banner.
   - In `src/AGENTS.md`, the "Group inserts are unified" bullet is at L378 and the `_by_label` sentence is at L495.
3. **The SAVEPOINT acceptance grep is scoped to code:** `grep -rni savepoint src/ include/ --include=*.h --include=*.cpp` is empty. The plan's `grep -rni savepoint src/` can never be empty, because the plan's own `src/AGENTS.md` sentence ("there are no SAVEPOINTs") lives under `src/`.
4. **The documented SQLite-only limit is widened from UNIQUE / NOT NULL / foreign key.**
   - CHECK constraints fail mid-write the same way, and root `AGENTS.md` itself recommends `CHECK (col IN (0, 1))`. So do PRIMARY KEY duplicates, raising triggers, and a NOT NULL column the call leaves out. The last one is real: `date_` columns of time-series tables are exempt from the duplicate check, so a `date_time` array fans out to every time-series group of the collection. `update_element("Sensor", id, {date_time = [...], temperature = [...]})` on `multi_time_series.sql` clears `Sensor_time_series_humidity` and then fails its INSERT on `humidity NOT NULL`.
   - CHECK was added in the `insert_rows_into_group_table` comment, in both `src/AGENTS.md` places and in the CHANGELOG. The root bullet names the full list.
   - The root bullet says the group and time-series writers "already do" validate first, not "always did": their validate-before-DELETE was itself a fix.
5. **Fixes from the post-implementation 4-lens review.** It found two confirmed doc inaccuracies, and five nits were fixed along with them.
   - The plan's "every writer" validates first is false for `import_csv`. Its CASCADE-survivor check and its self-FK label pass throw after writes. That is harmless, because it refuses to nest and rolls back its own transaction. The root bullet and the `src/AGENTS.md` Transactions paragraph now say "every writer that can run inside" a caller's transaction, and the root bullet names the exception.
   - The `_by_label` sentence now says "`prepare_group_data`'s routing/type/length checks". Its array FK-resolution errors come from `resolve_fk_label` and carry no `Cannot update_element:` prefix until plan 56 threads `caller` in.
   - `validate_group_columns`' comment named "every writer" as its callers. It now names its two real callers.
   - The CHANGELOG entry gained one sentence on per-table FK resolution, the second user-visible change.
   - `src/AGENTS.md`'s DATE_TIME bullet pointed "below" to the old bullet name. It now points to the "Group writes" bullet above.
   - `UpdateElementRejectedArrayKeepsEarlierGroup` now pins the `value_int` message with the file's try/FAIL/catch idiom instead of a bare `EXPECT_THROW`, so a throw before any write can't pass it.

**Error-precedence changes beyond the plan's "Known behaviour consequences."** No test pins any of them:
- In `create_element`, an array FK-resolution failure now comes after a scalar type error. Before, `resolve_element_fk_labels` resolved arrays first.
- SQLite errors from the scalar INSERT/UPDATE itself now come after every array routing, FK and validation error. These are a duplicate label, a CHECK failure, and the raw `Column 'x' not found` for an unknown scalar.
- Between arrays, the first failing array in name order wins. Before, every array FK error came before any "does not match" error.
- `update_element` can now log the fan-out warning and then throw on a scalar type check.

**For later plans:**
- **Rename map:**
  - `resolve_element_fk_labels` → `resolve_scalar_fk_labels`, which handles scalars only. Plan 56 still threads `caller` into it and into `resolve_fk_label`.
  - The array-validation loop now lives in `validate_group_columns`. That is plan 55's single `validate_array` call site, not `prepare_group_data`.
  - `Database& db` is now on `resolve_scalar_fk_labels`, `prepare_group_data`, `insert_group_data` and `insert_rows_into_group_table`, for plan 53 to drop.
  - Plan 53 edits the `src/AGENTS.md` bullet now titled "Group writes are unified, and checked before anything is written".
  - `update_group_rows`' table lookup, which plan 57 replaces, is untouched.
- **Plan 37's header** says it overlaps with no other plan, but 05 added tests to the same Dart and Julia transaction files. The anchors differ: Dart after 'multi-operation batch', Julia after "Multi-operation batch".
- **Pre-existing gaps, not fixed here** (candidates for a later plan):
  - `update_element(c, id, {id = {}})` silently clears every group of the element. `find_all_tables_for_column("id")` matches every group table, and `prepare_group_data`, unlike `update_group_rows`, does not reject `id` / `vector_index` as array names. A non-empty `vector_index` array duplicates an INSERT column.
  - The `date_time` fan-out into a time-series group whose NOT NULL value column the call omits (deviation 4) deletes that group before failing. `validate_time_series_row` does not catch the equivalent either, because it only checks that the dimension columns are present.
  - A fanned-out array now costs one resolved copy and one `resolve_fk_label` pass per matched table instead of one in total. Single-table arrays cost the same as before.
- **Carried forward:**
  - The Dart hook's native cache must be cleared before a regression run: delete `bindings/dart/.dart_tool/hooks_runner` and `.dart_tool/lib`.
  - `scripts/format.bat`'s biome step again rewrote 22 untouched JS files from CRLF to LF with no content change. Revert them by the names `git status --porcelain` lists; `git diff --name-only` hides CR-only changes.
  - The Debug build's `C4458: declaration of 'db' hides class member` warnings from `database_impl.h` predate this plan, and plan 53 removes them. `prepare_group_data` adds one more.
  - The first `uv run pytest` after a manifest bump rebuilds the editable install in Release, which takes minutes. Python still loads `build/bin` through `PATH`.
