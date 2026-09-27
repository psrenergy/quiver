# 57 — One group-table name helper and one `require_group_table`; Pattern 2 for every miss

**Batch** 6 · **Severity** low · **Breaking** no (two error texts change to the documented pattern) · **Size** M · **Layers** C++ core, C++ tests (+ src/AGENTS.md, CHANGELOG)
**Depends on** 53 (`require_element(..., db)` loses its `db` argument; keep it if 53 has not landed) · **Overlaps with** 02/03/04 (edit `database_time_series.cpp` bodies — apply this plan's lookup replacement to whatever shape they left), 07 (`list_*_groups` use `require_collection`; not touched here), 55 (if landed, `schema.h`/`schema.cpp` live in `src/`), 58 (import/export share a different lookup, `Schema::find_group_table`, added there)

## Why

The same group-table lookup, and the same Pattern 2 "not found" message, are written out in several
places, with a comment asking maintainers to keep the copies in sync. The time-series side also has
an unreachable fallback plus six unreachable null checks.

1. **Vector/set.** `Impl::update_group_rows` (`src/database_update.cpp`, ~L152-160):
   ```cpp
    const auto is_vector = type == GroupTableType::Vector;
    const auto table_name =
        is_vector ? Schema::vector_table_name(collection, group) : Schema::set_table_name(collection, group);
    const auto* table_def = schema->get_table(table_name);
    if (!table_def) {
        // Same wording as get_vector_metadata / get_set_metadata for the same condition (Pattern 2).
        throw std::runtime_error(std::string(is_vector ? "Vector" : "Set") + " group not found: '" + group +
                                 "' in collection '" + collection + "'");
    }
   ```
   `get_vector_metadata` (`src/database_metadata.cpp`, ~L18-27) and `get_set_metadata` repeat the
   lookup with their own copy of the message.
2. **The type→table-name switch** also lives in an anonymous namespace in
   `src/database_describe.cpp` (`group_table_name(collection, group, GroupTableType)`, ~L40-51), and
   a fourth infix switch lives in `Schema::group_names` (`src/schema.cpp`, ~L238-250).
3. **Time series.** `get_time_series_metadata` (`src/database_time_series.cpp`, ~L71-77) throws the
   Pattern 2 message `Time series group not found: 'g' in collection 'c'`. The four ops
   `read_time_series_group`, `update_time_series_group`, `upsert_time_series_row` and
   `read_time_series_row` instead call `schema->find_time_series_table(collection, group)`
   (~L99, ~L147, ~L228, ~L278). That function (`src/schema.cpp`, ~L189-214):
   - checks `has_table(collection + "_time_series_" + group)` and returns it;
   - otherwise loops over `table_names()` looking for the same name, which is **unreachable**,
     because it can only match what `has_table` just ruled out;
   - then throws the ad-hoc `Time series group 'g' not found for collection 'c'`.

   Each caller follows with an unreachable `if (!table_def) throw "Time series table not found: ..."`
   (~L101-103, ~L149-151, ~L230-232, ~L280-282).
4. **Time-series files.** `find_time_series_files_table` (`schema.cpp` ~L215-222) throws
   `Time series files table not found for collection 'c'`, while `list_time_series_files_columns`
   uses `Time series files table not found: <table>`. `read_time_series_files` /
   `update_time_series_files` (~L345, ~L392) follow the lookup with two more unreachable null checks.
5. `Schema::find_vector_table` / `find_set_table` (`schema.cpp` ~L151-186) throw
   `Vector attribute 'a' not found for collection 'c'` / `Set attribute ...`. Neither uses the
   `{Entity} not found: {identifier}` shape.

Principles: delete duplication and dead code; one condition, one message (root "C++ Error Message
Patterns", Pattern 2).

## Constraints and decisions

- **Maintainer notes (binding):**
  - Add `Schema::group_table_name`, lifted from `database_describe.cpp`, and
    `Impl::require_group_table`. Use them in `get_*_metadata`, `update_group_rows` and the four
    time-series ops.
  - Delete `Schema::find_time_series_table` and `find_time_series_files_table`, with their fallback
    and the six unreachable null checks.
  - The `Schema::find_{vector,set}_table` misses become Pattern 2
    `Vector/Set attribute not found: ...`.
  - **Do not merge the metadata builders.** Each `get_*_metadata` keeps its own column loop.
  - Add a CHANGELOG line for the changed time-series message.
  - Use `group_table_name` in `Schema::group_names`' infix switch too.
- The vector/set messages stay **byte-identical**. `tests/test_database_update.cpp` (~L1363,
  ~L1369) pins them with `EXPECT_STREQ`.
- `find_vector_table`/`find_set_table` resolve an **attribute** (a column name) as well as a group
  name, so their Pattern 2 entity is "Vector attribute" / "Set attribute", not "group".
- Leave CSV import/export's own three-way lookup to plan 58.

## Changes

### 1. `Schema::group_table_name` (`include/quiver/schema.h` + `src/schema.cpp`)

Next to `vector_table_name` / `set_table_name` / `time_series_table_name`, declare
`static std::string group_table_name(const std::string& collection, const std::string& group, GroupTableType type);`
and define it with the switch body from `database_describe.cpp` (~L40-51). Delete describe's
anonymous-namespace copy and call `Schema::group_table_name` at its two call sites (~L227, ~L335).
If `GroupTableType` is not visible in `schema.h`, it is declared there already; check with
`grep -n "enum class GroupTableType" include/quiver/*.h src/*.h`.

In `Schema::group_names` (~L238-250), replace the type→infix switch with a prefix computed from the
helper, e.g. `const auto prefix = group_table_name(collection, "", type);` (for `"Items"`/Vector
that gives `"Items_vector_"`). Check that `group_names` really builds `collection + infix` and
matches by prefix before replacing it.

### 2. `Impl::require_group_table` (`src/database_impl.h`, next to `require_collection`)

```cpp
    // The one lookup behind every group-addressed operation: the collection must exist, then the
    // {collection}_{vector|set|time_series}_{group} table. One Pattern 2 message per kind.
    const TableDefinition& require_group_table(const std::string& collection,
                                               const std::string& group,
                                               GroupTableType type,
                                               const char* operation) const {
        require_collection(collection, operation);
        if (const auto* table_def = schema->get_table(Schema::group_table_name(collection, group, type))) {
            return *table_def;
        }
        const char* kind = type == GroupTableType::Vector ? "Vector" : type == GroupTableType::Set ? "Set" : "Time series";
        throw std::runtime_error(std::string(kind) + " group not found: '" + group + "' in collection '" + collection + "'");
    }
```

### 3. Callers

- `update_group_rows` (`database_update.cpp` ~L152-160): replace the block with
  `const auto& table_def = require_group_table(collection, group, type, caller);` and delete the
  sync comment and the `is_vector` ternary. Then use `table_def.` wherever the old code used
  `table_def->`, and `table_def.name` where it used `table_name`. Check that `TableDefinition` has
  a `name` member (`grep -n "struct TableDefinition" -A8 include/quiver/schema.h`).
- `get_vector_metadata` / `get_set_metadata` / `get_time_series_metadata`: replace their lookup and
  throw with `const auto& table_def = impl_->require_group_table(collection, group_name, GroupTableType::Vector, "get_vector_metadata");`
  (and the Set and TimeSeries equivalents). Keep each one's own column loop.
- The four time-series ops: replace
  `auto ts_table = impl_->schema->find_time_series_table(collection, group); const auto* table_def = impl_->schema->get_table(ts_table); if (!table_def) { throw ... }`
  with `const auto& table_def = impl_->require_group_table(collection, group, GroupTableType::TimeSeries, "<op>");`,
  using `table_def.name` where `ts_table` was used in SQL. If a function calls `require_collection`
  separately first, delete that call, because `require_group_table` does it.

### 4. Time-series files

Delete `Schema::find_time_series_files_table`. In `read_time_series_files` / `update_time_series_files`
(~L345, ~L392), write the same shape `list_time_series_files_columns` uses:
`const auto tsf = Schema::time_series_files_table_name(collection); const auto* table_def = impl_->schema->get_table(tsf); if (!table_def) throw std::runtime_error("Time series files table not found: " + tsf);`.
Use whatever the existing name helper is (`grep -n "files_table_name\|_time_series_files" include/quiver/schema.h src/database_time_series.cpp`).
Delete the now-duplicate null checks.

### 5. Delete `Schema::find_time_series_table`

Remove it from `schema.h` and `schema.cpp`. Check with
`grep -rn "find_time_series_table\|find_time_series_files_table" src/ include/ tests/`: no hits.

### 6. `find_vector_table` / `find_set_table` messages (`schema.cpp` ~L163, ~L186)

`"Vector attribute '" + attribute + "' not found for collection '" + collection + "'"` becomes
`"Vector attribute not found: '" + attribute + "' in collection '" + collection + "'"`. Do the
same for `Set`. Keep their fallback loops, which are reachable: they match a column name inside
any vector/set table of the collection.

## Tests

- Existing vector/set tests pinning `Vector group not found: ...` / `Set group not found: ...` must
  pass unchanged (`grep -rn "group not found" tests/`).
- Search for the old time-series and attribute texts and update every hit:
  `grep -rn "not found for collection\|Time series table not found" tests/ bindings/`.
- Add to `tests/test_database_time_series_group.cpp`:
  ```cpp
  TEST(Database, TimeSeriesOpsReportMissingGroupAsPattern2) {
      auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"),
                                              {.read_only = false, .console_level = quiver::LogLevel::Off});
      db.create_element("Configuration", quiver::Element().set("label", "Config"));
      auto id = db.create_element("Collection", quiver::Element().set("label", "A"));
      try {
          db.update_time_series_group("Collection", "no_such_group", id, {});
          FAIL() << "expected a throw";
      } catch (const std::runtime_error& e) {
          EXPECT_STREQ(e.what(), "Time series group not found: 'no_such_group' in collection 'Collection'");
      }
  }
  ```
  If the file uses a different fixture or helper for exact-message assertions, use that.
- Add one pin for the vector attribute message wherever `read_vector_*` errors are tested:
  `read_vector_integers("Collection", "no_such_attr")` throws a message containing
  `"Vector attribute not found: 'no_such_attr' in collection 'Collection'"`.

## Docs and changelog

- `src/AGENTS.md`, "Table classification has one source" bullet: add "and one name builder,
  `Schema::group_table_name(collection, group, GroupTableType)`; every group-addressed operation
  resolves its table through `Impl::require_group_table`, which owns the Pattern 2 miss."
- `CHANGELOG.md`, under `## [0.11.0] — unreleased` → `### Changed`:
  ```markdown
  - **Missing-group errors use one pattern.** The time-series operations now report `Time series
    group not found: 'g' in collection 'c'` (was `Time series group 'g' not found for collection
    'c'`), matching `get_time_series_metadata`; a missing vector/set attribute reports `Vector
    attribute not found: 'a' in collection 'c'` (was `Vector attribute 'a' not found for collection
    'c'`).
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. `scripts/test-all.bat` (bindings may substring-match the old texts)
4. `scripts/format.bat`

## Acceptance criteria

- [ ] One `Schema::group_table_name` and one `Impl::require_group_table`. The describe copy and the
      sync comment are gone.
- [ ] `find_time_series_table`, `find_time_series_files_table` and all unreachable null checks are
      gone.
- [ ] The vector/set messages are unchanged, and the time-series and attribute messages follow
      Pattern 2. All suites are green.

## Pitfalls

- `require_group_table` returns a reference, and callers that held `const TableDefinition*` must
  switch `->` to `.`. Either that or take `&require_group_table(...)`; pick one style per function.
- `Schema::group_names` must still exclude `_time_series_files`. The prefix `Items_time_series_`
  also matches `Items_time_series_files`, so keep its existing exclusion.

## Out of scope

- Merging the `get_*_metadata` column loops.
- The CSV import/export lookup (plan 58).
