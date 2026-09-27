# 06 — SchemaValidator: enforce parent FK for set and time-series tables

**Batch** 1 · **Severity** medium · **Breaking** yes. It affects anyone whose schema, migrations or existing database file has a `<Collection>_set_<g>` or `<Collection>_time_series_<g>` table in one of three states: the collection does not exist, `id` has no foreign key to `<Collection>(id)`, or that key is not `ON DELETE CASCADE ON UPDATE CASCADE`. It also affects any time-series table with another foreign key that is not `ON UPDATE CASCADE` plus `ON DELETE CASCADE`/`SET NULL`. Those schemas are now rejected in every layer. · **Size** M · **Layers** C++ core (fix), C API (test only), Julia / Dart / Python / JS (tests only), docs, CHANGELOG. No FFI or signature change. Lua: no change (see Tests).

**Depends on** none. The edits are anchored on code this plan's predecessors (01–05) do not touch.
**Overlaps with**
- **02**: its notes say it adds a new schema under `tests/schemas/valid/`. That schema must satisfy the rules added here. Verification step 1 checks every schema added since `58dfe7a`.
- **01**: no shared file. Plan 01 keeps FKs on during `import_csv` so a dropped element cascades into its groups. This plan makes sure every set and time-series table has a cascade to rely on.
- **55** (batch 6): later moves `include/quiver/schema_validator.h` into `src/`. Edit the header where it is now. Plan 55 carries the edits along.
- **59** (batch 6): later replaces FK loops with `TableDefinition::get_foreign_key` elsewhere, and explicitly leaves the `schema_validator.cpp` loops alone. The parent-FK loop moved here keeps its current predicate for that reason (see Constraints).
- **62** (batch 6): later adds `invalid/unsupported_type.sql`, a test in `tests/test_schema_validator.cpp`, and a name in the `invalid/` list in `tests/AGENTS.md`. Both files are edited here first. Plan 62 appends to them and can reuse the `<gmock/gmock.h>` include added here.
- **73** (batch 7): owns every other `docs/rules.md` / `docs/attributes.md` fix: the vector-relation examples that lack the parent FK (`HydroPlant_vector_gaugingstations`), the FK prose, the Configuration example and the migration sections. This plan edits only the two `HydroPlant_set_gaugingstations` examples and the three "must contain a Column named `id`" bullets (vector, set, time series) in each file. That split is what the maintainer asked for ("coordinate with plan 73").
- **75** (batch 7): later fixes other claims in `tests/AGENTS.md`. This plan edits only the `invalid/` bullet.

## Why

`SchemaValidator` checks parent integrity only for vector tables. `validate_vector_table` (`src/schema_validator.cpp`, currently ~L103-155) makes two checks. The collection named by the table prefix must exist:

```cpp
    // Get parent collection
    auto parent = schema_.get_parent_collection(name);
    if (std::find(collections_.begin(), collections_.end(), parent) == collections_.end()) {
        validation_error("Vector table '" + name + "' references non-existent collection '" + parent + "'");
    }
```

And `id` must have a CASCADE/CASCADE foreign key to that collection (~L140-154):

```cpp
    // Must have FK to parent with ON DELETE CASCADE ON UPDATE CASCADE
    auto has_parent_fk = false;
    for (const auto& fk : table->foreign_keys) {
        if (fk.from_column == "id" && fk.to_table == parent) {
```

`validate_set_table` (~L157-201) checks UNIQUE coverage only. Time-series tables get no per-table validation at all. The dispatch in `validate()` (~L28-39) has no branch for them, and the comment there describes the `_time_series_files` table instead:

```cpp
        } else if (schema_.is_time_series_files_table(name)) {
            validate_time_series_files_table(name);
        }
        // Time series tables have minimal validation (just file paths)
```

`validate_foreign_keys` (~L294-361) also exempts time-series tables from the FK-action rule every other table follows:

```cpp
            } else if (!schema_.is_time_series_table(table_name)) {
                // Relation FK - check for valid combinations
```

`Database::delete_element` (`src/database_delete.cpp`) is a bare `DELETE FROM <collection>`, and `src/database.cpp` opens every connection with `PRAGMA foreign_keys = ON`. So a group table's rows are removed only by an `ON DELETE CASCADE` on its `id`. Reproductions, confirmed against the current build with `quiver_cli`, use the two fixture files this plan adds (their full SQL is in Tests):

- `quiver_cli --schema tests/schemas/invalid/set_no_parent_fk.sql :memory: repro_set.lua` with
  ```lua
  local id = db:create_element("Collection", { label = "a", tag = { "x", "y" } })
  db:delete_element("Collection", id)
  return db:query_integer("SELECT COUNT(*) FROM Collection_set_tags")
  ```
  prints `2`: the schema validates, and deleting the element leaves two orphan set rows. After the fix, the CLI exits 1 with `Failed to validate schema: Set table 'Collection_set_tags' must have foreign key to parent collection 'Collection'`.
- `quiver_cli --schema tests/schemas/invalid/time_series_fk_actions.sql :memory: repro_ts.lua` with
  ```lua
  local id = db:create_element("Collection", { label = "a" })
  db:update_time_series_group("Collection", "data", id, { date_time = { "2024-01-01" }, value = { 1.5 } })
  db:delete_element("Collection", id)
  ```
  fails with `Failed to execute statement: FOREIGN KEY constraint failed` from `delete_element`, because the `id` FK has no actions (SQLite reports `NO ACTION`). After the fix, the schema is rejected with `Time series table 'Collection_time_series_data' FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE`.
- A set table named after a nonexistent collection (`Ghost_set_tags`) validates today. So does a time-series *relation* FK with no actions (`parent_id INTEGER REFERENCES Parent(id)`), and deleting the referenced `Parent` then fails with the same `FOREIGN KEY constraint failed`. Both confirmed.

Principles violated: the root `AGENTS.md` Schema Conventions say *"Always use `ON DELETE CASCADE ON UPDATE CASCADE` for parent references"*, and its set and time-series examples declare that FK. The core, where the logic is supposed to live, enforces it for one of three group kinds.

Secondary defects fixed in the same change:
- The vector-only branch in `validate_foreign_keys` (~L317-322) repeats what `validate_vector_table` already checked, with a different message. It can fire first only when `id` carries two FKs.
- Every `validate_*` function starts with a dead `if (!table) return;` / `continue;` guard: ~L67-70, ~L104-107, ~L158-161, ~L204-207, ~L232-235, ~L259-262, ~L296-299. Every name comes from `schema_.table_names()`, so `get_table` cannot miss.
- `if (!col) continue;` in `validate_foreign_keys` (~L304-306) is dead too. SQLite rejects an FK on an unknown column at `CREATE TABLE` (`unknown column "bogus" in foreign key definition`, confirmed), and `PRAGMA foreign_key_list` reports `from` with the table's own spelling of the column (confirmed: `FOREIGN KEY (ID)` is reported as `id`).
- The header comment (`include/quiver/schema_validator.h` ~L12-17) says duplicate attributes are checked only against "vector tables". The check covers sets and time series as well.
- `tests/schemas/invalid/duplicate_attribute_time_series.sql` is a SQL syntax error: `label TEXT UNIQUE NOT NULL,` right before `)`. Its FKs also reference a nonexistent `Resource`. `InvalidDuplicateAttributeTimeSeries` therefore passes on the parse error (`Failed to execute SQL: near ")": syntax error`, confirmed), not on duplicate detection. With the new time-series parent check it would pass for a second wrong reason, so the fixture is fixed here.

## Constraints and decisions

- **Maintainer notes (binding):** BREAKING, because schemas without the parent FK are now rejected. Check that every existing `tests/schemas/valid/*.sql` and binding test schema still validates, and fix any that do not. Fix the `docs/rules.md` and `docs/attributes.md` set examples, and coordinate with plan 73.
- **Existing schemas: all pass (verified).** Every git-tracked `.sql` was loaded into SQLite and checked against the new rules: parent exists, `id` FK to the parent is CASCADE/CASCADE, and every FK is `ON UPDATE CASCADE` with `ON DELETE CASCADE|SET NULL`. That covers all 13 `valid/` files, `migrations/1..3` applied cumulatively, and `issues/issue52`, `issues/issue70`. All pass, including `relations.sql`'s time-series relation `sponsor_id` (`SET NULL`/`CASCADE`, nullable). No test builds a group table inline: the inline schemas in `test_migrations.cpp` and `test_database_ui_metadata.cpp` have collections only. No binding has its own `.sql`. So no fixture other than `duplicate_attribute_time_series.sql` changes.
- **Error messages** (root `AGENTS.md`, "C++ Error Message Patterns"): every validator message goes through `validation_error`, i.e. Pattern 3 `Failed to validate schema: <reason>`. The three vector messages are kept word for word. The only change is that the word "Vector" becomes the `kind` argument.
- **Lazy schema loading** (root Design Decision): `Database(path)` / `open()` validates on the first metadata/CRUD call. An existing database *file* with a non-conforming set or time-series table therefore opens, and its first use then fails. The CHANGELOG says so. Do not change the loading design.
- **"Clean code over defensive code … Delete unused code"** (root Principles): the dead guards are deleted, not kept "just in case".
- **Tests at every layer** (root `AGENTS.md` / task rules): the rule is visible wherever a schema is opened: C++, the C API, and all four bindings. Lua has no schema-opening entry point. Its only route to the validator is `db:validate_migrations`, which calls the same `migrate_up → load_schema_metadata → SchemaValidator::validate` as C++ `validate_migrations`, and the Lua binding (`src/lua_runner.cpp`, `validate_migrations` lambda) forwards the exception unchanged. No Lua test is added.
- **Alternatives rejected:**
  - A separate `validate_time_series_table` plus set-specific parent code: duplicates the vector block three times. One helper, called for all three kinds, is the whole fix.
  - Using `TableDefinition::get_foreign_key("id")` in the helper. `PRAGMA foreign_key_list` lists FKs in *reverse* declaration order (confirmed), so with two FKs on `id`, "first FK on `id`" is not "the parent FK". The helper keeps the current predicate (`from_column == "id" && to_table == parent`), and plan 59 leaves these loops alone for the same reason.
  - Comparing `to_table` case-insensitively. SQLite would accept `REFERENCES collection(id)`, but the vector check has always been exact. Changing that is a separate decision.
  - Calling the helper *before* each table's structural checks. That would make `validate_vector_table`'s `must have 'id' column` message unreachable, because a table with no `id` has no `id` FK either. Structural checks run first and the parent check last.
  - Keeping the `!is_time_series_table` exemption in `validate_foreign_keys`. It has been there since the initial commit (`04efb1f`) with no documented reason. With it, a time-series relation FK can still be `NO ACTION`, and deleting the referenced element fails exactly like the bug above. Deleting it leaves one FK-action rule for every table. It is breaking only for schemas that are broken the same way.

## Changes

### 1. `src/schema_validator.cpp`

Before editing, run `git log --oneline 58dfe7a..HEAD -- src/schema_validator.cpp`. If an earlier plan touched this file, apply the steps below to the quoted code wherever it now sits.

**1a. `SchemaValidator::validate()` — the table dispatch loop (currently ~L28-39).**

Current:
```cpp
    for (const auto& name : schema_.table_names()) {
        if (schema_.is_collection(name)) {
            validate_collection(name);
        } else if (schema_.is_vector_table(name)) {
            validate_vector_table(name);
        } else if (schema_.is_set_table(name)) {
            validate_set_table(name);
        } else if (schema_.is_time_series_files_table(name)) {
            validate_time_series_files_table(name);
        }
        // Time series tables have minimal validation (just file paths)
    }
```
New (the stale comment is deleted):
```cpp
    for (const auto& name : schema_.table_names()) {
        if (schema_.is_collection(name)) {
            validate_collection(name);
        } else if (schema_.is_vector_table(name)) {
            validate_vector_table(name);
            validate_group_parent(name, "Vector");
        } else if (schema_.is_set_table(name)) {
            validate_set_table(name);
            validate_group_parent(name, "Set");
        } else if (schema_.is_time_series_table(name)) {
            validate_group_parent(name, "Time series");
        } else if (schema_.is_time_series_files_table(name)) {
            validate_time_series_files_table(name);
        }
    }
```
`Schema::is_time_series_table` already excludes `_time_series_files` tables (`src/schema.cpp`), so the two time-series branches never both match.

**1b. `SchemaValidator::validate_collection` — delete the dead guard (currently ~L67-70).**

Current:
```cpp
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Must have 'id' column as primary key
```
New:
```cpp
    const auto* table = schema_.get_table(name);

    // Must have 'id' column as primary key
```

**1c. New function `SchemaValidator::validate_group_parent`.** Insert it directly above `void SchemaValidator::validate_vector_table(`. It holds the parent-exists and parent-FK checks moved out of `validate_vector_table`, unchanged except that "Vector" becomes `kind`, and a `return` replaces the `has_parent_fk` flag:
```cpp
// A vector, set or time series table belongs to the collection its name starts with, and its `id`
// must reference that collection with ON DELETE CASCADE ON UPDATE CASCADE: delete_element is a bare
// DELETE on the collection and relies on the cascade to remove the element's group rows.
void SchemaValidator::validate_group_parent(const std::string& name, const std::string& kind) {
    const auto parent = schema_.get_parent_collection(name);
    if (std::find(collections_.begin(), collections_.end(), parent) == collections_.end()) {
        validation_error(kind + " table '" + name + "' references non-existent collection '" + parent + "'");
    }

    for (const auto& fk : schema_.get_table(name)->foreign_keys) {
        if (fk.from_column == "id" && fk.to_table == parent) {
            if (fk.on_delete != "CASCADE" || fk.on_update != "CASCADE") {
                validation_error(kind + " table '" + name +
                                 "' FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE");
            }
            return;
        }
    }
    validation_error(kind + " table '" + name + "' must have foreign key to parent collection '" + parent + "'");
}
```

**1d. `SchemaValidator::validate_vector_table` — remove the guard and the two moved blocks (currently ~L103-155).** The full new body:
```cpp
void SchemaValidator::validate_vector_table(const std::string& name) {
    const auto* table = schema_.get_table(name);

    // Must have 'id' column
    const auto* id_col = table->get_column("id");
    if (!id_col) {
        validation_error("Vector table '" + name + "' must have 'id' column");
    }

    // id should NOT be primary key alone (composite PK required)
    if (id_col && id_col->primary_key) {
        auto pk_count = 0;
        for (const auto& [_, col] : table->columns) {
            if (col.primary_key) {
                pk_count++;
            }
        }
        if (pk_count == 1) {
            validation_error("Vector table '" + name +
                             "' must have composite primary key (id, vector_index), not just 'id'");
        }
    }

    // Must have 'vector_index' column
    if (!table->has_column("vector_index")) {
        validation_error("Vector table '" + name + "' must have 'vector_index' column");
    }
}
```
This deletes the `if (!table) { return; }` guard, the `// Get parent collection` block (`auto parent = …` plus its `std::find` check), and the whole `// Must have FK to parent with ON DELETE CASCADE ON UPDATE CASCADE` block through the final `must have foreign key to parent collection` check. Everything else is unchanged.

**1e. `SchemaValidator::validate_set_table` — delete the dead guard (currently ~L158-161).**

Current:
```cpp
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Get FK column names
```
New:
```cpp
    const auto* table = schema_.get_table(name);

    // Get FK column names
```

**1f. `SchemaValidator::validate_time_series_files_table` — delete the dead guard (currently ~L204-207).**

Current:
```cpp
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Get parent collection
    auto parent = schema_.get_time_series_files_parent_collection(name);
```
New:
```cpp
    const auto* table = schema_.get_table(name);

    // Get parent collection
    auto parent = schema_.get_time_series_files_parent_collection(name);
```

**1g. `SchemaValidator::validate_no_duplicate_attributes` — delete two dead guards (currently ~L232-235 and ~L259-262).**

Current (first):
```cpp
        const auto* col_table = schema_.get_table(collection);
        if (!col_table) {
            continue;
        }

        std::set<std::string> attributes;
```
New:
```cpp
        const auto* col_table = schema_.get_table(collection);

        std::set<std::string> attributes;
```
Current (second):
```cpp
            const auto* group_table = schema_.get_table(table_name);
            if (!group_table) {
                continue;
            }

            // Get FK column names
```
New:
```cpp
            const auto* group_table = schema_.get_table(table_name);

            // Get FK column names
```
Leave the rest of this function alone, including `is_ts && col_name.starts_with("date_")`.

**1h. `SchemaValidator::validate_foreign_keys` — one FK-action rule for every table; delete the dead guards (currently ~L294-361).** The full new function. The naming-rule block at the end is unchanged:
```cpp
void SchemaValidator::validate_foreign_keys() {
    for (const auto& table_name : schema_.table_names()) {
        const auto* table = schema_.get_table(table_name);

        for (const auto& fk : table->foreign_keys) {
            const auto* col = table->get_column(fk.from_column);

            // Rule: FK columns with ON DELETE SET NULL cannot have NOT NULL constraint
            if (fk.on_delete == "SET NULL" && col->not_null) {
                validation_error("Foreign key column '" + fk.from_column + "' in table '" + table_name +
                                 "' has ON DELETE SET NULL but NOT NULL constraint");
            }

            // Rule: every FK uses ON UPDATE CASCADE and ON DELETE SET NULL or CASCADE. A group table's
            // parent FK is held to ON DELETE CASCADE by validate_group_parent, which runs first.
            if (fk.on_update != "CASCADE") {
                validation_error("Foreign key '" + fk.from_column + "' in table '" + table_name +
                                 "' must use ON UPDATE CASCADE");
            }
            if (fk.on_delete != "SET NULL" && fk.on_delete != "CASCADE") {
                validation_error("Foreign key '" + fk.from_column + "' in table '" + table_name +
                                 "' must use ON DELETE SET NULL or ON DELETE CASCADE");
            }

            // Rule: FK column names should follow pattern <collection>_id or <collection>_<relation>
            // Skip vector table id column and set table columns
            if (!schema_.is_vector_table(table_name) || fk.from_column != "id") {
                if (!schema_.is_set_table(table_name) && !schema_.is_time_series_table(table_name)) {
                    // Check if FK column name ends with _id or matches target_relation pattern
                    auto target = fk.to_table;
                    auto target_lower = target;
                    std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(), ::tolower);
                    auto col_lower = fk.from_column;
                    std::transform(col_lower.begin(), col_lower.end(), col_lower.begin(), ::tolower);

                    // Expected pattern: <target>_id or <target>_<something>
                    auto valid_name = (col_lower == target_lower + "_id") ||
                                      (col_lower.starts_with(target_lower + "_")) ||
                                      (col_lower.find("_id") != std::string::npos);

                    if (!valid_name) {
                        validation_error("Foreign key column '" + fk.from_column + "' in table '" + table_name +
                                         "' should follow naming pattern '<collection>_id'");
                    }
                }
            }
        }
    }
}
```
What changed relative to today:
- The `if (!table) { continue; }` guard is gone.
- The `// Find the column for this FK` comment and the `if (!col) { continue; }` guard are gone (dead: see Why).
- The three-line `// Rule: Valid FK action combinations …` comment is replaced.
- The `if (schema_.is_vector_table(table_name) && fk.from_column == "id") { … parent FK must use … }` branch is gone.
- `else if (!schema_.is_time_series_table(table_name)) { … }` became two unconditional `if`s with the same messages.

A vector parent FK still reaches these rules and passes, because `validate_group_parent` has already required CASCADE/CASCADE.

### 2. `include/quiver/schema_validator.h`

**2a. Class comment (currently ~L12-17).**

Current:
```cpp
// Validates that a schema follows QUIVER conventions:
// - Configuration table exists
// - Collections have id/label with proper constraints
// - Vector tables have proper structure and FK constraints
// - Set tables have proper UNIQUE constraints
// - No duplicate attributes across collection and its vector tables
```
New:
```cpp
// Validates that a schema follows QUIVER conventions:
// - Configuration table exists
// - Collections have id/label with proper constraints
// - Every vector, set and time series table is named after an existing collection, and its `id`
//   references that collection with ON DELETE CASCADE ON UPDATE CASCADE
// - Vector tables have a composite (id, vector_index) primary key and a vector_index column
// - Set tables have proper UNIQUE constraints
// - Every foreign key uses ON UPDATE CASCADE and ON DELETE SET NULL or CASCADE
// - No duplicate attributes across a collection and its vector, set and time series tables
```

**2b. Private declarations (currently ~L29-37).** Add one line after `validate_collection`:

Current:
```cpp
    void validate_collection(const std::string& name);
    void validate_vector_table(const std::string& name);
```
New:
```cpp
    void validate_collection(const std::string& name);
    void validate_group_parent(const std::string& name, const std::string& kind);
    void validate_vector_table(const std::string& name);
```

### 3. `tests/schemas/invalid/duplicate_attribute_time_series.sql` — fix the fixture

Replace the whole file. It drops the trailing comma after `label TEXT UNIQUE NOT NULL` and changes both `REFERENCES Resource(id)` to `REFERENCES Collection(id)`, so the file breaks exactly one rule (duplicate `some_vector1`):
```sql
-- Invalid: Duplicate attribute name in time series tables
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_group1 (
    id INTEGER,
    date_time TEXT NOT NULL,
    some_vector1 REAL,
    FOREIGN KEY(id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;

CREATE TABLE Collection_time_series_group2 (
    id INTEGER,
    date_time TEXT NOT NULL,
    some_vector1 REAL,
    FOREIGN KEY(id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;
```
Verified against the current build: this file is rejected with `Failed to validate schema: Duplicate attribute 'some_vector1' found in table 'Collection_time_series_group2' (already defined in collection 'Collection')`. It still is after the fix, because its parent FKs are correct.

### 4. New invalid schemas (`tests/schemas/invalid/`)

No CMake registration is needed: tests locate `tests/schemas` from the compiled-in source path (`INVALID_SCHEMA` in `tests/test_utils.h`). Each file breaks exactly one rule. All four are accepted by the current build (verified with `quiver_cli`), so every test that uses them fails before the fix.

**4a. `tests/schemas/invalid/set_no_parent_fk.sql`**
```sql
-- Invalid: Set table whose id has no foreign key to its parent collection
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_set_tags (
    id INTEGER NOT NULL,
    tag TEXT NOT NULL,
    UNIQUE (id, tag)
) STRICT;
```
Expected: `Failed to validate schema: Set table 'Collection_set_tags' must have foreign key to parent collection 'Collection'`.

**4b. `tests/schemas/invalid/set_unknown_parent.sql`.** SQLite allows an FK to a missing table at `CREATE`, so only the validator can catch this.
```sql
-- Invalid: Set table named after a collection that does not exist
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Ghost_set_tags (
    id INTEGER NOT NULL,
    tag TEXT NOT NULL,
    FOREIGN KEY (id) REFERENCES Ghost(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE (id, tag)
) STRICT;
```
Expected: `Failed to validate schema: Set table 'Ghost_set_tags' references non-existent collection 'Ghost'`.

**4c. `tests/schemas/invalid/time_series_fk_actions.sql`**
```sql
-- Invalid: Time series table whose parent FK does not use ON DELETE CASCADE ON UPDATE CASCADE
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_data (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    value REAL,
    FOREIGN KEY (id) REFERENCES Collection(id),
    PRIMARY KEY (id, date_time)
) STRICT;
```
Expected: `Failed to validate schema: Time series table 'Collection_time_series_data' FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE`.

**4d. `tests/schemas/invalid/time_series_relation_fk_actions.sql`.** The parent FK is correct. The relation FK has no actions.
```sql
-- Invalid: Time series relation FK without ON UPDATE CASCADE and ON DELETE SET NULL or CASCADE
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Parent (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_events (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    parent_id INTEGER,
    FOREIGN KEY (id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY (parent_id) REFERENCES Parent(id),
    PRIMARY KEY (id, date_time)
) STRICT;
```
Expected: `Failed to validate schema: Foreign key 'parent_id' in table 'Collection_time_series_events' must use ON UPDATE CASCADE`. `validate_foreign_keys` walks tables alphabetically: `Collection` has no FKs, and `Collection_time_series_events` comes next.

### 5. C API, FFI declarations, binding wrappers, Lua

No change. No signature changed, so there is no generator run, no `_c_api.py` / `loader.ts` / `bindings.dart` edit, and no wrapper edit. Every layer gets the new message through `quiver_get_last_error`.

## Tests

### C++ — `tests/test_schema_validator.cpp` (fixture `SchemaValidatorFixture`)

**Includes (top of file, currently ~L1-4).** Add gmock for `ThrowsMessage`/`HasSubstr`. `quiver_tests` already links `GTest::gmock` (`tests/CMakeLists.txt`).

Current:
```cpp
#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
```
New:
```cpp
#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
```

**Change `InvalidDuplicateAttributeTimeSeries` (currently ~L50-54)** so it passes only for its named reason.

Current:
```cpp
TEST_F(SchemaValidatorFixture, InvalidDuplicateAttributeTimeSeries) {
    EXPECT_THROW(
        quiver::Database::from_schema(":memory:", INVALID_SCHEMA("duplicate_attribute_time_series.sql"), options),
        std::runtime_error);
}
```
New:
```cpp
TEST_F(SchemaValidatorFixture, InvalidDuplicateAttributeTimeSeries) {
    EXPECT_THAT(
        [&] { quiver::Database::from_schema(":memory:", INVALID_SCHEMA("duplicate_attribute_time_series.sql"), options); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
            "Duplicate attribute 'some_vector1' found in table 'Collection_time_series_group2'")));
}
```

**Add four tests directly after `TEST_F(SchemaValidatorFixture, InvalidFkActions)` (currently ~L71-74)**, before the `// Type validation tests` banner:
```cpp
TEST_F(SchemaValidatorFixture, InvalidSetNoParentFk) {
    EXPECT_THAT(
        [&] { quiver::Database::from_schema(":memory:", INVALID_SCHEMA("set_no_parent_fk.sql"), options); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
            "Failed to validate schema: Set table 'Collection_set_tags' must have foreign key to parent collection "
            "'Collection'")));
}

TEST_F(SchemaValidatorFixture, InvalidSetUnknownParent) {
    EXPECT_THAT(
        [&] { quiver::Database::from_schema(":memory:", INVALID_SCHEMA("set_unknown_parent.sql"), options); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::HasSubstr("Set table 'Ghost_set_tags' references non-existent collection 'Ghost'")));
}

TEST_F(SchemaValidatorFixture, InvalidTimeSeriesFkActions) {
    EXPECT_THAT(
        [&] { quiver::Database::from_schema(":memory:", INVALID_SCHEMA("time_series_fk_actions.sql"), options); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
            "Time series table 'Collection_time_series_data' FK to parent must use ON DELETE CASCADE ON UPDATE "
            "CASCADE")));
}

TEST_F(SchemaValidatorFixture, InvalidTimeSeriesRelationFkActions) {
    EXPECT_THAT(
        [&] {
            quiver::Database::from_schema(":memory:", INVALID_SCHEMA("time_series_relation_fk_actions.sql"), options);
        },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
            "Foreign key 'parent_id' in table 'Collection_time_series_events' must use ON UPDATE CASCADE")));
}
```
All four fail on the current code: the schema is accepted, so `ThrowsMessage` reports "does not throw". `InvalidDuplicateAttributeTimeSeries` fails with the *old* fixture, whose message is the syntax error, and passes with the fixed fixture both before and after the validator change. The positive side (a set or time-series table *with* a correct FK still validates) is already pinned by `ValidSchemaCollections` and `ValidSchemaRelations` (`collections.sql` and `relations.sql` have both kinds), and by every other suite that opens a `valid/` schema.

### C API — `tests/test_c_api_database_lifecycle.cpp` (fixture `TempFileFixture`)

Add directly after `TEST_F(TempFileFixture, FromSchemaInvalidPath)` (currently ~L216-220):
```cpp
TEST_F(TempFileFixture, FromSchemaRejectsSetTableWithoutParentFk) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(":memory:", INVALID_SCHEMA("set_no_parent_fk.sql").c_str(), &options, &db),
              QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(),
                 "Failed to validate schema: Set table 'Collection_set_tags' must have foreign key to parent "
                 "collection 'Collection'");
}
```
The file already includes `test_utils.h` (`INVALID_SCHEMA`, `quiet_options`). This test fails before the fix, because `from_schema` returns `QUIVER_OK`.

### Julia — `bindings/julia/test/test_schema_validator.jl`

Add inside `@testset "Invalid Schema"`, after the `@testset "FK Actions"` block (currently ~L54-57) and before that testset's closing `end`:
```julia
    @testset "Set No Parent FK" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "set_no_parent_fk.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("Set table 'Collection_set_tags' must have foreign key to parent collection", exc.value.msg)
    end

    @testset "Set Unknown Parent" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "set_unknown_parent.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("Set table 'Ghost_set_tags' references non-existent collection 'Ghost'", exc.value.msg)
    end

    @testset "Time Series FK Actions" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "time_series_fk_actions.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE", exc.value.msg)
    end

    @testset "Time Series Relation FK Actions" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "time_series_relation_fk_actions.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("Foreign key 'parent_id' in table 'Collection_time_series_events'", exc.value.msg)
    end
```
(`exc.value.msg` is the idiom already used in `test_database_lifecycle.jl`.) `runtests.jl` includes every `test_*.jl`, so no registration is needed.

### Dart — `bindings/dart/test/schema_validation_test.dart`

Add inside `group('Invalid Schemas', …)`, after `test('rejects schema with invalid FK actions', …)` (currently ~L146-154) and before the group's closing `});`:
```dart
    test('rejects schema with set table without parent FK', () {
      expect(
        () => Database.fromSchema(dbPath, path.join(invalidPath, 'set_no_parent_fk.sql')),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            contains("Set table 'Collection_set_tags' must have foreign key to parent collection 'Collection'"),
          ),
        ),
      );
    });

    test('rejects schema with set table of a non-existent collection', () {
      expect(
        () => Database.fromSchema(dbPath, path.join(invalidPath, 'set_unknown_parent.sql')),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            contains("Set table 'Ghost_set_tags' references non-existent collection 'Ghost'"),
          ),
        ),
      );
    });

    test('rejects schema with time series parent FK without CASCADE', () {
      expect(
        () => Database.fromSchema(dbPath, path.join(invalidPath, 'time_series_fk_actions.sql')),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            contains("Time series table 'Collection_time_series_data' FK to parent must use ON DELETE CASCADE"),
          ),
        ),
      );
    });

    test('rejects schema with time series relation FK without ON UPDATE CASCADE', () {
      expect(
        () => Database.fromSchema(dbPath, path.join(invalidPath, 'time_series_relation_fk_actions.sql')),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            contains("Foreign key 'parent_id' in table 'Collection_time_series_events' must use ON UPDATE CASCADE"),
          ),
        ),
      );
    });
```

### Python — `bindings/python/tests/test_database_lifecycle.py`

Add after `test_from_schema_context_manager` (currently ~L17-22). `pytest` and `QuiverError` are already imported, and `schemas_path` is a `conftest.py` fixture:
```python
def test_from_schema_rejects_set_table_without_parent_fk(schemas_path: Path, tmp_path: Path) -> None:
    schema = schemas_path / "invalid" / "set_no_parent_fk.sql"
    message = "Set table 'Collection_set_tags' must have foreign key to parent collection 'Collection'"
    with pytest.raises(QuiverError, match=message):
        Database.from_schema(str(tmp_path / "test.db"), str(schema))
```
The message holds no regex metacharacters, so `match` is a plain substring search.

### JS — `bindings/js/test/database-lifecycle.test.ts`

Add a path constant after `const MIGRATIONS_PATH = …` (currently ~L13):
```ts
const INVALID_SCHEMAS_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "invalid");
```
Add a test directly after `test("fromSchema with invalid schema throws QuiverError", …)` (currently ~L56-60):
```ts
  test("fromSchema rejects a set table without a parent foreign key", () => {
    expect(() => Database.fromSchema(":memory:", join(INVALID_SCHEMAS_PATH, "set_no_parent_fk.sql"))).toThrow(
      "Set table 'Collection_set_tags' must have foreign key to parent collection 'Collection'",
    );
  });
```
(Bun's `toThrow(string)` asserts that the message contains the string.)

### Lua

No test. Lua cannot open a schema, and `db:validate_migrations` reaches the validator through the same C++ path with no Lua-side code in between (see Constraints).

### Existing tests that must stay green

Every suite that opens a `valid/` schema or `migrations/`: `quiver_tests`, `quiver_c_tests`, and all four binding suites. No existing assertion pins a validator message (grep of `tests/` and `bindings/` for `FK to parent`, `non-existent collection`, `foreign key to parent`, `must use ON UPDATE CASCADE`, `Failed to validate schema` finds none). The bare `EXPECT_THROW` / `@test_throws` / `throwsA(isA<DatabaseException>())` tests for the other nine invalid schemas are unaffected, because each of those files still fails on its own rule. `vector_no_index.sql` still reports the composite-PK message, and `set_no_unique.sql` the UNIQUE message.

## Docs and changelog

### `docs/rules.md`

**Vector section.** Keep the existing bullet (currently ~L88)
```md
- The table must contain a Column named `id` and another named `vector_index`. These two columns together should form the Primary Key of the table.
```
and insert this new bullet directly below it:
```md
- The `id` column must reference the Collection's `id` with `ON DELETE CASCADE ON UPDATE CASCADE`, so deleting an element removes its rows.
```

**Set section.** Replace the bullet (currently ~L124)
```md
- The table must contain a Column named `id`.
```
with
```md
- The table must contain a Column named `id` that references the Collection's `id` with `ON DELETE CASCADE ON UPDATE CASCADE`, so deleting an element removes its rows.
```

**Set example (currently ~L130-137).** Replace the `HydroPlant_set_gaugingstations` block with the following. It adds one line, the parent FK:
```sql
CREATE TABLE HydroPlant_set_gaugingstations(
    id INTEGER,
    conversion_factor REAL NOT NULL,
    gaugingstation_id INTEGER,
    FOREIGN KEY (id) REFERENCES HydroPlant(id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY (gaugingstation_id) REFERENCES GaugingStation(id) ON UPDATE CASCADE ON DELETE CASCADE,
    UNIQUE (id, conversion_factor, gaugingstation_id)
) STRICT;
```
Verified: together with `Configuration`, `HydroPlant` and `GaugingStation` collections, this table passes the current validator and meets every new rule.

**Time Series section.** Under `### Time Series`, replace the bullet (currently ~L142; the same text also appears in the set section above, so edit the one under the Time Series heading)
```md
- The table must contain a Column named `id`.
```
with the same sentence as the set bullet:
```md
- The table must contain a Column named `id` that references the Collection's `id` with `ON DELETE CASCADE ON UPDATE CASCADE`, so deleting an element removes its rows.
```
The time-series example below it already declares the FK. Leave it.

Do **not** touch the `HydroPlant_vector_gaugingstations` example, the relation-FK prose, the Configuration example or the migration sections. Plan 73 owns them.

### `docs/attributes.md`

**Vector section.** Below the bullet (currently ~L53)
```md
- The table must contain a Column named `id` and another named `vector_index`. These two columns together should form the Primary Key of the table.
```
insert the same new bullet as in `rules.md`:
```md
- The `id` column must reference the Collection's `id` with `ON DELETE CASCADE ON UPDATE CASCADE`, so deleting an element removes its rows.
```

**Set section.** Replace the bullet (currently ~L89) `- The table must contain a Column named `id`.` with the same new set bullet as in `rules.md`:
```md
- The table must contain a Column named `id` that references the Collection's `id` with `ON DELETE CASCADE ON UPDATE CASCADE`, so deleting an element removes its rows.
```

**Set example (currently ~L96-102).** Replace it with exactly the same `HydroPlant_set_gaugingstations` block as in `rules.md` above. The only difference from today is the added `FOREIGN KEY (id) REFERENCES HydroPlant(id) ON DELETE CASCADE ON UPDATE CASCADE,` line.

This file has no time-series section. Plan 73 owns everything else in it.

### `docs/time_series.md`

The paragraph after the first example (currently ~L23-26) is:
```md
A time series table is named `{Collection}_time_series_{group}` and must be indexed by a
dimension column whose name starts with `date_` (usually `date_time`), stored as ISO 8601
text (`YYYY-MM-DDTHH:MM:SS`). The bindings convert their native datetime types to and from
this format automatically.
```
Replace it with the following, which inserts one sentence and keeps the rest:
```md
A time series table is named `{Collection}_time_series_{group}` and must be indexed by a
dimension column whose name starts with `date_` (usually `date_time`), stored as ISO 8601
text (`YYYY-MM-DDTHH:MM:SS`). Its `id` must reference the collection's `id` with
`ON DELETE CASCADE ON UPDATE CASCADE`, as in the example; a table without that foreign key is
rejected when the schema is loaded. The bindings convert their native datetime types to and from
this format automatically.
```

### Root `AGENTS.md` — "Schema Conventions" → "### Foreign Keys"

Current (currently ~L529-530):
```
### Foreign Keys
Always use `ON DELETE CASCADE ON UPDATE CASCADE` for parent references.
```
New:
```
### Foreign Keys
Always use `ON DELETE CASCADE ON UPDATE CASCADE` for parent references. `SchemaValidator` enforces
it for every vector, set and time-series table: the table's prefix must name an existing
collection, and its `id` must reference that collection with both actions. Every other foreign
key, in any table, must use `ON UPDATE CASCADE` with `ON DELETE CASCADE` or `ON DELETE SET NULL`
(a `SET NULL` column must be nullable).
```

### `src/AGENTS.md` — "Core Internals Worth Knowing"

Insert a new bullet directly after the `- **Schema metadata loads lazily** (\`Impl::require_schema\`): …` bullet, which ends `… Rationale in the root design decisions.`:
```
- **Every group table's parent is checked by one helper** (`schema_validator.cpp`,
  `validate_group_parent`, called from `validate()` for vector, set and time-series tables after
  their structural checks): the prefix must name an existing collection and `id` must reference
  it with ON DELETE CASCADE ON UPDATE CASCADE — `delete_element` is a bare `DELETE` on the
  collection and relies on that cascade. `validate_foreign_keys` then applies one action rule to
  every FK in every table. Sets and time series used to skip both, so a schema could leave orphan
  set rows or make `delete_element` fail with SQLite's `FOREIGN KEY constraint failed`.
```

### `tests/AGENTS.md` — "Schemas (`tests/schemas/`)" → the `invalid/` bullet

Current:
```
- `invalid/` — schemas the validator must reject: `duplicate_attribute_time_series.sql`,
  `duplicate_attribute_vector.sql`, `fk_actions.sql`, `fk_not_null_set_null.sql`,
  `label_not_null.sql`, `label_not_unique.sql`, `label_wrong_type.sql`, `no_configuration.sql`,
  `set_no_unique.sql`, `vector_no_index.sql`
```
New:
```
- `invalid/` — schemas the validator must reject: `duplicate_attribute_time_series.sql`,
  `duplicate_attribute_vector.sql`, `fk_actions.sql`, `fk_not_null_set_null.sql`,
  `label_not_null.sql`, `label_not_unique.sql`, `label_wrong_type.sql`, `no_configuration.sql`,
  `set_no_parent_fk.sql`, `set_no_unique.sql`, `set_unknown_parent.sql`,
  `time_series_fk_actions.sql`, `time_series_relation_fk_actions.sql`, `vector_no_index.sql`.
  Each file must break exactly one rule and be otherwise valid SQL, and a new test should assert
  the message rather than a bare throw: `duplicate_attribute_time_series.sql` once "passed" on a
  trailing-comma syntax error while pointing its FKs at a table that did not exist.
```

### `bindings/js/src/lua-api.ts`, binding READMEs, binding `AGENTS.md` files

No change. None of them describe schema-validation rules (checked: grep for `CASCADE` / `REFERENCES` / `schema valid`).

### `CHANGELOG.md` — `## [0.11.0] — unreleased` → `### Changed`

Append as the last bullet of `### Changed`, before `### Fixed`. Earlier plans may have appended their own bullets, so append after whatever is last:
```
- **BREAKING — set and time-series tables need the same parent foreign key as vector tables.**
  Opening a schema (`from_schema`, `from_migrations`, `validate_migrations`, or the first use of a
  database opened with `open()`) now rejects a `<Collection>_set_<group>` or
  `<Collection>_time_series_<group>` table when `<Collection>` does not exist, when its `id` has no
  foreign key to `<Collection>(id)`, or when that key is not `ON DELETE CASCADE ON UPDATE CASCADE`
  — the rules vector tables already followed. Without the cascade, `delete_element` left the
  element's set rows behind, or failed with `FOREIGN KEY constraint failed` once the element had
  time-series rows. Any other foreign key in a time-series table must now use `ON UPDATE CASCADE`
  with `ON DELETE CASCADE` or `ON DELETE SET NULL`, as in every other table. The errors read
  `Failed to validate schema: Set table '<t>' must have foreign key to parent collection '<c>'`,
  `… references non-existent collection '<c>'`, and
  `… FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE`.

  *Adapt:* declare `FOREIGN KEY (id) REFERENCES <Collection>(id) ON DELETE CASCADE ON UPDATE
  CASCADE` on every set and time-series table, and give each time-series relation key
  `ON UPDATE CASCADE` with `ON DELETE CASCADE` or `SET NULL`. SQLite cannot add a foreign key to
  an existing table, so an existing database needs a migration that rebuilds the table (create the
  new table, copy the rows, drop the old one, rename).
```

## Verification

From the repo root. Bash commands are for Git Bash; the `.bat` lines can be run from PowerShell (`.\bindings\julia\test\test.bat …`) or `cmd /c`.

1. Schemas added by earlier plans:
   `git diff --name-only 58dfe7a -- tests/schemas`
   For every added or changed `.sql` with a `_set_` or `_time_series_` table (not `_time_series_files`), confirm three things: `id` declares `REFERENCES <Collection>(id) ON DELETE CASCADE ON UPDATE CASCADE` with the exact collection spelling, every other FK in it is `ON UPDATE CASCADE` + `ON DELETE CASCADE|SET NULL`, and a `SET NULL` column is nullable. Fix any that do not in this change. Plan 02 is expected to add one.
2. Build: `cmake --build build --config Debug`
3. Targeted C++:
   `./build/bin/quiver_tests.exe --gtest_filter='SchemaValidatorFixture.*'`
   Expected: all pass, including the new `InvalidSetNoParentFk`, `InvalidSetUnknownParent`, `InvalidTimeSeriesFkActions`, `InvalidTimeSeriesRelationFkActions` and the rewritten `InvalidDuplicateAttributeTimeSeries`.
4. Targeted C API:
   `./build/bin/quiver_c_tests.exe --gtest_filter='TempFileFixture.FromSchema*'`
   Expected: `FromSchemaRejectsSetTableWithoutParentFk` passes, along with the existing `FromSchema*` tests.
5. Full core suites (every `valid/` schema and the migrations still load):
   `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`. Expected: 0 failures.
6. Reproduction check (optional, fast): run the two `quiver_cli` commands from "Why" with the two Lua snippets saved in a temp dir. Both must now exit 1 with the `Failed to validate schema: …` messages quoted there.
7. Bindings, targeted then full:
   - `bindings\julia\test\test.bat test_schema_validator.jl`, then `bindings\julia\test\test.bat`
   - `bindings\dart\test\test.bat test/schema_validation_test.dart`, then `bindings\dart\test\test.bat`
   - `bindings\js\test\test.bat`. The script always passes the `test` directory as a filter, so it runs the whole suite either way. Check that `fromSchema rejects a set table without a parent foreign key` is listed as passed.
   - `bindings\python\tests\test.bat -k parent_fk`, then `bindings\python\tests\test.bat`

   Expected: all green, and each new test is listed as passed.
8. `scripts\format.bat`. Then re-run step 3 if clang-format rewrapped the new tests, and check `git diff` shows only formatting changes.
9. `scripts\test-all.bat`. Expected: steps 1–6 pass. Step 7, the CLI smoke test, fails for a reason that has nothing to do with this change: `example\example1.lua` was deleted and plan 65 restores the smoke test. Do not fix it here.

## Acceptance criteria

- [ ] `validate()` calls `validate_group_parent(name, "Vector"|"Set"|"Time series")` for all three group kinds. The stale `// Time series tables have minimal validation` comment is gone.
- [ ] `validate_vector_table` no longer contains the parent-exists or parent-FK code, and every "Vector table" message it still has is unchanged.
- [ ] `validate_foreign_keys` has no vector-only branch and no `is_time_series_table` exemption from the action rule. Its naming-rule block is unchanged.
- [ ] All eight dead guards are deleted: seven `if (!table)` / `if (!col_table)` / `if (!group_table)` and one `if (!col)`.
- [ ] The `schema_validator.h` comment lists the new rules, and `validate_group_parent` is declared.
- [ ] `duplicate_attribute_time_series.sql` is valid SQL, references `Collection(id)`, and its test asserts the duplicate-attribute message.
- [ ] The four new invalid schemas exist, and each is rejected with exactly the message given above.
- [ ] New tests pass in C++ (4), C API (1), Julia (4), Dart (4), Python (1) and JS (1), and each of them fails on the pre-change code.
- [ ] Every `valid/`, `migrations/` and `issues/` schema, and every schema added by plans 01–05, still loads: the full suites are green.
- [ ] Both `HydroPlant_set_gaugingstations` doc examples declare the parent FK. The vector, set and time-series `id` bullets in `rules.md` and `attributes.md`, and the `time_series.md` paragraph, state the rule.
- [ ] Root `AGENTS.md` Foreign Keys, `src/AGENTS.md` Core Internals and the `tests/AGENTS.md` `invalid/` list are updated.
- [ ] A **BREAKING** CHANGELOG bullet is under `0.11.0` → `### Changed`. No manifest version bump.

## Pitfalls

- **Structural checks first, parent check second.** If `validate_group_parent` runs before `validate_vector_table`, a vector table with no `id` reports `must have foreign key to parent collection` instead of `must have 'id' column`. Keep the order from step 1a.
- **Don't swap the loop for `get_foreign_key("id")`.** `PRAGMA foreign_key_list` returns FKs in reverse declaration order (verified), so the first FK on `id` is not guaranteed to be the parent one.
- **`to_table` is compared exactly.** `REFERENCES items(id)` against a collection named `Items` is rejected, as it always was for vectors. Every shipped schema uses the exact spelling. If a user schema trips this, that is expected, not a bug in this change.
- **`validation_error` is not `[[noreturn]]`.** The helper relies on it throwing, like the rest of the file. The `return` inside the loop is what skips the final "must have foreign key" throw. Do not "fix" this by adding a flag back.
- **Line endings.** `.sql` files are CRLF in this working tree (`core.autocrlf=true`, `* text=auto`), and git stores them LF. Writing the new files with LF is fine: git normalizes them. When rewriting `duplicate_attribute_time_series.sql`, replace the whole file rather than patching it with `sed`. `.bat` files must not be touched.
- **Message wrapping in tests.** clang-format may re-wrap the long `HasSubstr` string literals. Adjacent string literals concatenate, so a re-wrap is harmless. Check that no space was lost at a split point: `"… parent collection "` + `"'Collection'"`.
- **`EXPECT_THAT` with a lambda.** Commas inside the lambda body are protected only because they sit inside the `from_schema(...)` parentheses. Do not add a top-level comma to a lambda body passed straight into the macro.
- **Dart native cache.** `dart test` builds its own copy of the native library through the native-assets hook (`bindings/dart/AGENTS.md`). If the new Dart tests still see the old behaviour (the schemas are accepted), clear `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/` and re-run.
- **Existing database files.** `open()` validates lazily. A test or user database created before this change with a non-conforming set or time-series table opens, and then its first metadata or CRUD call fails. No test in the repo keeps such a file. This is the documented breaking behaviour, so do not add a compatibility path.
- **`test-all.bat` step 7** fails until plan 65 lands (missing `example/example1.lua`). It is unrelated to this change.

## Out of scope

- The vector-relation doc examples without a parent FK (`HydroPlant_vector_gaugingstations` in `docs/rules.md` and `docs/attributes.md`), the relation-FK prose, the Configuration example and the migration docs: **plan 73**.
- Moving `schema_validator.h` into `src/` and dropping `QUIVER_API`: **plan 55**.
- Schema-loading error messages for unsupported column types or invalid table names: **plan 62**.
- Replacing the FK loops in database code with `get_foreign_key`: **plan 59**. The validator's loops stay.
- Other time-series structural checks (dimension column present, composite primary key) and the `starts_with("date_")` skip in `validate_no_duplicate_attributes`. No plan owns them. Plan 02 redefines the dimension column in the reader/writer code only.
- Case-insensitive matching of the `REFERENCES` target, and the FK naming rule's set/time-series exemption (`!is_set_table && !is_time_series_table`). Both are unchanged. No plan owns them.
- Converting the other nine bare `EXPECT_THROW` invalid-schema tests (and their Julia/Dart mirrors) to message assertions. No plan owns this. Only the fixture fixed here gets a message assertion.
