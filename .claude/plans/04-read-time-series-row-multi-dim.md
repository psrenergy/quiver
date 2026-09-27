# 04 — read_time_series_row: reject multi-dimension groups and filter NULL in the join

**Batch** 1 · **Severity** medium · **Breaking** yes: any caller, in any layer, that calls `read_time_series_row` on a time-series group with more than one dimension column (e.g. `date_time` + `block`) now gets an error instead of a value · **Size** S · **Layers** C++ core (fix), C API (test only), Lua (test + agent reference), Julia / Dart / Python / JS (tests + doc comments), docs, CHANGELOG

**Depends on** none. Plan 02 lands first and redefines `internal::find_dimension_column`. The code below is correct whether or not 02 has landed.
**Overlaps with**
- **02**: changes what `internal::find_dimension_column` returns; this plan keeps calling it from the same line of `read_time_series_row`.
- **17**: later adds an `out_mask` out-param to `quiver_database_read_time_series_row`. It must extend the new C API test added here, and it edits the same paragraphs in `docs/time_series.md`, root `AGENTS.md` (the "Time series row" bullet), the Dart `readTimeSeriesRow` doc comment and the Python `read_time_series_row` docstring.
- **57**: later rewrites the `find_time_series_table` / `get_table` / throw prologue of this function. The guard added here sits just below that prologue.
- **53**: later changes the `execute(sql, {date_time})` call in this function.
- **43 / 44**: later edit other parts of `bindings/js/src/lua-api.ts`.

## Why

`Database::read_time_series_row` promises one value per element: "last non-null value at or before `date_time`" (root `AGENTS.md`, "Time series row" bullet; `docs/time_series.md`, "Rules"; `include/quiver/database.h`). It works that out with a self-join, `src/database_time_series.cpp` (currently ~L296-310):

```cpp
    // For each element, find the most recent non-null value where dim_col <= date_time.
    // Self-join: subquery picks max dim_col per id, outer query gets the value.
    auto sql = "SELECT t.id, t." + attribute + " FROM " + ts_table + " t INNER JOIN (SELECT id, MAX(" + dim_col +
               ") as max_dt FROM " + ts_table + " WHERE " + dim_col + " <= ? AND " + attribute + " IS NOT NULL " +
               "GROUP BY id) latest ON t.id = latest.id AND t." + dim_col + " = latest.max_dt ORDER BY t.id";

    auto query_result = execute(sql, {date_time});

    std::map<int64_t, Value> id_value_map;
    for (size_t i = 0; i < query_result.row_count(); ++i) {
        ...
        id_value_map[*id] = query_result[i][1];   // last joined row per id wins
    }
```

Only the subquery filters out NULLs. The outer join matches every row at `max_dt`, and nothing orders rows that share an id. A group whose primary key has a second dimension (`tests/schemas/valid/multi_dim_time_series.sql`: `PRIMARY KEY (id, date_time, block)`) can hold several rows at one date. The single-column dimension lookup (`internal::find_dimension_column`, currently ~L283) sees only `date_time` and cannot tell.

Reproduction, confirmed against the current build through `quiver_cli` (`--schema tests/schemas/valid/multi_dim_time_series.sql :memory: repro.lua`):

```lua
db:create_element("Configuration", { label = "Config" })
local id = db:create_element("Resource", { label = "R1" })
db:upsert_time_series_row("Resource", "load", id, { date_time = "2024-01-01", block = 1, load = 10.0 })
db:upsert_time_series_row("Resource", "load", id, { date_time = "2024-01-01", block = 2, flag = 5 })
db:read_time_series_row("Resource", "load", "load", "2024-01-01")   --> { nil }   (block 1 holds 10.0)
db:upsert_time_series_row("Resource", "load", id, { date_time = "2024-01-01", block = 2, load = 20.0 })
db:read_time_series_row("Resource", "load", "load", "2024-01-01")   --> { 20.0 }  (an arbitrary block)
```

The same SQL in plain SQLite gives `[(1, 10.0), (1, None)]` for the first read. The loop keeps the NULL row because it comes last. With `AND t.load IS NOT NULL` in the ON clause it gives `[(1, 10.0)]`.

Two things are wrong, and the maintainer has decided to fix both:

1. **A silent wrong answer.** The read returns NULL even though a non-null value exists at that date. The cause is the ON clause.
2. **An ambiguous question.** When a group has more than one dimension, "one value per element at a date" has no single answer. Refusing the ambiguous case (Pattern 1) is better than returning whichever block SQLite happens to emit last.

No test at any layer calls `read_time_series_row` on `multi_dim_time_series.sql`. The C++, C API, Lua, Julia, Dart, Python and JS row-read tests all use `collections.sql`, `mixed_time_series.sql` or `nullable_time_series.sql`, and every one of those has `PRIMARY KEY (id, date_time)`. That is why the bug never showed up.

## Constraints and decisions

- **Maintainer decision (binding):** "BREAKING. Do both the Pattern 1 guard (after find_dimension_columns succeeds) and the ON-clause IS NOT NULL. CHANGELOG under 0.12.0 (already a minor bump; no manifest bump)."
  - "After find_dimension_columns succeeds" means `internal::find_dimension_columns(*table_def)` runs first. It still throws its Pattern 2 `Dimension column not found: time series table '<t>'` for a table with no primary-key dimension. Only after that does the guard test `.size() > 1`.
- **Error patterns** (root `AGENTS.md`, "C++ Error Message Patterns"): the guard is Pattern 1, `Cannot {operation}: {reason}`, with `{operation}` set to the public method, `read_time_series_row`. The wording "group '<g>' of collection '<c>'" matches the attribute-miss message in this same function (`Time series attribute not found: '<a>' in group '<g>' of collection '<c>'`).
- **Error messages live in C++** (root `AGENTS.md`, "Principles"): bindings only pass the message through. No binding crafts its own message or adds a pre-check.
- **Tests at every layer where the behaviour is visible**: C++, C API, Lua, Julia, Dart, Python and JS all surface the new error, so each gets one test.
- **Changelog** (root `AGENTS.md`, "Principles"): a **BREAKING** entry under `## [0.12.0] — unreleased` that says what a caller must do. 0.12.0 is already the minor bump, so no manifest change is needed.
- **Self-Updating**: the contract is written in root `AGENTS.md` ("Core API", "Time series row" bullet). `src/AGENTS.md` does not describe `read_time_series_row`, so the root bullet is the one to edit.
- **Multi-dimension groups remain a supported feature** (`docs/time_series.md`, "It is also possible to add more dimensions ... such as `block`"). Only this single-row reader refuses them. `read_time_series_group`, `update_time_series_group` and `upsert_time_series_row` keep working on them.
- Alternatives considered and rejected:
  - *ON-clause filter only (non-breaking):* this fixes the NULL result, but a multi-dimension read still returns an arbitrary block's value. The maintainer chose to refuse that case.
  - *Guard only; leave the SQL alone (the policy verifier's recommendation):* once the guard is in, the outer join is one-to-one, because `PRIMARY KEY (id, date_time)` allows one row per id and date. The extra filter can then no longer change a result. The maintainer overrode this and wants both, so the SQL states its own intent ("the non-null row at the latest non-null date") and does not rely on the guard that sits above it.
  - *Add a `block`/dimension filter parameter to read per block:* this is a new API, not a fix. Out of scope.
  - *Use `dim_cols.front()` as the dimension:* rejected. `internal::find_dimension_column` stays the only definition of "the date dimension" that `get_time_series_metadata` and `read_time_series_group` also use (plan 02 makes it primary-key based). After plan 02, a lone dimension that is not a date is rejected there, not silently used as the date axis.

## Changes

### 1. `src/database_time_series.cpp` — `Database::read_time_series_row`

**1a. Add the guard in front of the dimension lookup.**

Current (currently ~L278-283):

```cpp
    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }
    auto dim_col = internal::find_dimension_column(*table_def);
```

New:

```cpp
    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }
    // One value per element needs one row per (element, date). A second dimension such as `block`
    // keeps several rows at each date, and picking one of them would be arbitrary.
    if (internal::find_dimension_columns(*table_def).size() > 1) {
        throw std::runtime_error("Cannot read_time_series_row: group '" + group + "' of collection '" + collection +
                                 "' has more than one dimension column");
    }
    auto dim_col = internal::find_dimension_column(*table_def);
```

Notes:
- If plan 02 changed the spelling of the `auto dim_col = ...` line, keep 02's line as it is. Insert the guard block directly above it.
- The guard must come **before** the `read_element_ids(collection)` early return (currently ~L291-294, `if (element_ids.empty()) { return {}; }`). The refusal depends on the schema, not the data, so an empty collection must throw too. The new tests check this.
- `find_dimension_columns` returns every primary-key column except `id`, and throws the Pattern 2 "Dimension column not found" itself when there are none. That is the "after find_dimension_columns succeeds" order the maintainer asked for.

**1b. Filter NULL in the outer join.**

Current (currently ~L296-300):

```cpp
    // For each element, find the most recent non-null value where dim_col <= date_time.
    // Self-join: subquery picks max dim_col per id, outer query gets the value.
    auto sql = "SELECT t.id, t." + attribute + " FROM " + ts_table + " t INNER JOIN (SELECT id, MAX(" + dim_col +
               ") as max_dt FROM " + ts_table + " WHERE " + dim_col + " <= ? AND " + attribute + " IS NOT NULL " +
               "GROUP BY id) latest ON t.id = latest.id AND t." + dim_col + " = latest.max_dt ORDER BY t.id";
```

New:

```cpp
    // For each element, the most recent non-null value where dim_col <= date_time.
    // Self-join: the subquery picks the latest non-null date per id, and the outer query reads the value there.
    // The outer IS NOT NULL repeats the subquery's filter, so the join can only land on a non-null row.
    auto sql = "SELECT t.id, t." + attribute + " FROM " + ts_table + " t INNER JOIN (SELECT id, MAX(" + dim_col +
               ") as max_dt FROM " + ts_table + " WHERE " + dim_col + " <= ? AND " + attribute + " IS NOT NULL " +
               "GROUP BY id) latest ON t.id = latest.id AND t." + dim_col + " = latest.max_dt AND t." + attribute +
               " IS NOT NULL ORDER BY t.id";
```

The rest of the function (the `id_value_map` loop and the per-element result) is unchanged. The SQL was checked in SQLite on both table shapes:
- On the multi-dimension data, the old SQL returns `[(1, 10.0), (1, None)]` and the new SQL returns `[(1, 10.0)]`.
- On a single-dimension table with rows `5.0 @01-01` and `NULL @01-02`, the new SQL still returns `5.0` at `01-02`, which is the behaviour `ReadTimeSeriesRowSkipsNullValues` checks.

### 2. `include/quiver/database.h` — declaration comment

Current (currently ~L172-174):

```cpp
    // Read time series row - returns one value per element for a specific attribute at a given date_time
    // Uses "last non-null value at or before date_time" lookup semantics
    // Returns nullptr Value for elements with no matching data
```

New:

```cpp
    // Read time series row - returns one value per element for a specific attribute at a given date_time
    // Uses "last non-null value at or before date_time" lookup semantics
    // Returns nullptr Value for elements with no matching data
    // Throws for a group with more than one dimension column (e.g. date_time + block): use read_time_series_group
```

### 3. C API, FFI declarations and binding wrappers

No code change. `quiver_database_read_time_series_row` (`src/c/database_time_series.cpp`) catches the exception and hands the message to `quiver_set_last_error`. No signature changes, so there is no generator run and no edit to `c_api.jl`, `bindings.dart`, `_c_api.py` or `loader.ts`. The four FFI wrappers (Julia `read_time_series_row` in `bindings/julia/src/database_read.jl`, Dart `readTimeSeriesRow` in `bindings/dart/lib/src/database_read.dart`, Python `read_time_series_row` in `bindings/python/src/quiverdb/database.py`, JS `readTimeSeriesRow` in `bindings/js/src/time-series.ts`) all call `check(...)`, which raises the binding's exception with that message. Lua's `read_time_series_row_lua` (`src/lua_runner.cpp`) calls `db.read_time_series_row` directly, and sol2 turns the exception into a Lua error. Only doc comments change (see "Docs and changelog").

## Tests

Expected message, written the same way in every layer:

```
Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column
```

Every new test fails before the fix. Today the read returns normally: `[]` on an empty collection, and `[NULL]` / `{nil}` when the repro data is present.

No existing test pins the old multi-dimension behaviour, so no existing assertion changes. The existing single-dimension row-read tests must stay green unchanged: C++ `Database.ReadTimeSeriesRow*`, C API `DatabaseCApi.ReadTimeSeriesRow*`, Lua `LuaRunnerTest.ReadTimeSeriesRow`, and the binding suites. `ReadTimeSeriesRowSkipsNullValues` is what checks that the extra ON-clause filter does not disturb the single-dimension NULL-skip.

The ON-clause filter (step 1b) has **no test that fails without it**. Once the guard is in place, the only case where the outer join returns more than one row per id (a multi-dimension group) is refused before the SQL runs. Do not try to build a failing case for it. The C++ test below checks the repro data, where the guard is what fires.

No new schema file is needed. Every test uses `tests/schemas/valid/multi_dim_time_series.sql`, which is already listed in `tests/AGENTS.md`.

### C++ — `tests/test_database_time_series_row.cpp`

Add after `TEST(Database, ReadTimeSeriesRowSkipsNullValues)`, which is the last test in the "read_time_series_row" section and sits just above the `// upsert_time_series_row tests` banner:

```cpp
TEST(Database, ReadTimeSeriesRowRejectsMultiDimensionGroup) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    const char* expected =
        "Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column";

    // Refused on the schema, not the data: an empty collection already throws.
    try {
        db.read_time_series_row("Resource", "load", "load", "2024-01-01");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), expected);
    }

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    // Two blocks at one date, block 2's load NULL: the read used to return NULL here although block 1 holds 10.0.
    db.upsert_time_series_row(
        "Resource", "load", id, {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 10.0}});
    db.upsert_time_series_row("Resource",
                              "load",
                              id,
                              {{"date_time", std::string("2024-01-01")}, {"block", int64_t{2}}, {"flag", int64_t{5}}});

    try {
        db.read_time_series_row("Resource", "load", "load", "2024-01-01");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), expected);
    }

    // The group itself stays readable through the group reader.
    EXPECT_EQ(db.read_time_series_group("Resource", "load", id).size(), 2u);
}
```

### C API — `tests/test_c_api_database_time_series_row.cpp`

Add after `TEST(DatabaseCApi, ReadTimeSeriesRowGroupNotFound)`, the last test in the file:

```cpp
TEST(DatabaseCApi, ReadTimeSeriesRowRejectsMultiDimensionGroup) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    size_t out_count = 0;
    auto err = quiver_database_read_time_series_row(
        db, "Resource", "load", "load", "2024-01-01", &out_type, &out_values, &out_count);
    EXPECT_EQ(err, QUIVER_ERROR);
    EXPECT_EQ(out_values, nullptr);
    EXPECT_STREQ(quiver_get_last_error(),
                 "Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column");

    quiver_database_close(db);
}
```

(Plan 17 will add an `out_mask` argument to this call when it changes the signature.)

### Lua — `tests/test_lua_runner_time_series.cpp`

Add after `TEST_F(LuaRunnerTest, ReadTimeSeriesRow)`:

```cpp
TEST_F(LuaRunnerTest, ReadTimeSeriesRowRejectsMultiDimensionGroup) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql"));
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua,
        R"(db:read_time_series_row("Resource", "load", "load", "2024-01-01"))",
        "Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column");
}
```

(`expect_lua_error` in `tests/test_lua_runner.h` matches a substring, so the prefix LuaRunner adds to the message is fine.)

### Julia — `bindings/julia/test/test_database_time_series_row.jl`

Add after the `@testset "Read Time Series Row - Group Not Found"` block:

```julia
    @testset "Read Time Series Row - Multi-Dimension Group Rejected" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_dim_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        exc = @test_throws Quiver.DatabaseException Quiver.read_time_series_row(
            db, "Resource", "load", "load"; date_time = DateTime(2024, 1, 1),
        )
        @test exc.value.msg ==
              "Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column"

        Quiver.close!(db)
    end
```

### Dart — `bindings/dart/test/database_time_series_row_test.dart`

Add inside `group('Read Time Series Row', ...)`, after `test('readTimeSeriesRow returns empty list without elements', ...)`:

```dart
    test('readTimeSeriesRow throws on a multi-dimension group', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'multi_dim_time_series.sql'),
      );
      try {
        expect(
          () => db.readTimeSeriesRow('Resource', 'load', 'load', DateTime(2024, 1, 1)),
          throwsA(
            isA<DatabaseException>().having(
              (e) => e.message,
              'message',
              equals(
                "Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column",
              ),
            ),
          ),
        );
      } finally {
        db.close();
      }
    });
```

### Python — `bindings/python/tests/test_database_time_series_row.py`

Add to `class TestReadTimeSeriesRow`, after `test_read_time_series_row_unknown_attribute_raises`. It uses the existing `multi_dim_ts_db` fixture from `bindings/python/tests/conftest.py`:

```python
    def test_read_time_series_row_multi_dimension_group_raises(self, multi_dim_ts_db: Database) -> None:
        """A second dimension (block) leaves no single value per element at a date."""
        with pytest.raises(
            QuiverError,
            match="Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column",
        ):
            multi_dim_ts_db.read_time_series_row("Resource", "load", "load", datetime(2024, 1, 1))
```

(`match` is a regex, but the message has no regex metacharacters, so it can go in as plain text.)

### JS — `bindings/js/test/database-time-series-row.test.ts`

1. Below the `NULLABLE_TS_SCHEMA` constant, add:

```ts
const MULTI_DIM_TS_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "multi_dim_time_series.sql",
);
```

2. Inside `describe("readTimeSeriesRow", ...)`, after `test("returns empty array when the collection has no elements", ...)`:

```ts
  test("throws on a group with more than one dimension column", () => {
    const db = Database.fromSchema(":memory:", MULTI_DIM_TS_SCHEMA);
    try {
      expect(() => db.readTimeSeriesRow("Resource", "load", "load", "2024-01-01T00:00:00")).toThrow(
        "Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column",
      );
    } finally {
      db.close();
    }
  });
```

## Docs and changelog

### Root `AGENTS.md` — "Core API" → "Database Class", the "Time series row" bullet

Old:

```
- Time series row: `read_time_series_row(collection, group, attribute, date_time)` — one value per element using "last non-null value at or before date_time" semantics; null Value for elements with no matching data (bindings surface `nothing`/`null`/`None`/`nil`).
```

New:

```
- Time series row: `read_time_series_row(collection, group, attribute, date_time)` — one value per element using "last non-null value at or before date_time" semantics; null Value for elements with no matching data (bindings surface `nothing`/`null`/`None`/`nil`). Single-dimension groups only: a group with more than one dimension column (every PK column but `id`, e.g. `date_time` + `block`) holds several rows per date, so it throws Pattern 1 `Cannot read_time_series_row: group '<g>' of collection '<c>' has more than one dimension column` even on an empty collection — such a group is read with `read_time_series_group`. The query also filters NULL in its outer join, not only in the latest-date subquery.
```

### `docs/time_series.md` — "## Rules", the `read_time_series_row` paragraph

Old:

```
When reading a single time series row at a given `date_time` (`read_time_series_row`), the
value returned for each element is the **last non-null value at or before** the queried
date. If an element has no data at or before that date, the entry is null (`nothing` in
Julia, `None` in Python, `null` in Dart/JS, `nil` in Lua).
```

New:

```
When reading a single time series row at a given `date_time` (`read_time_series_row`), the
value returned for each element is the **last non-null value at or before** the queried
date. If an element has no data at or before that date, the entry is null (`nothing` in
Julia, `None` in Python, `null` in Dart/JS, `nil` in Lua).

`read_time_series_row` needs a group with a single dimension column. A group with more
dimensions (such as `block` above) holds several rows at each date, so there is no single
value per element, and the call raises an error. Read such a group with
`read_time_series_group` instead.
```

### `bindings/js/src/lua-api.ts` — `### Read one value per element at a date (\`read_time_series_row\`)`

Old (inside the template literal; backticks are escaped):

```
One value per element using **last non-null value at or before \`date_time\`** semantics. Elements
with no matching data yield \`nil\` in the array. \`date_time\` is an ISO 8601 string.
```

New:

```
One value per element using **last non-null value at or before \`date_time\`** semantics. Elements
with no matching data yield \`nil\` in the array. \`date_time\` is an ISO 8601 string. A group with
more than one dimension column (e.g. \`date_time\` + \`block\`) throws — read it with
\`read_time_series_group\`.
```

Keep every backtick escaped as `\``. `test/lua-api-sync.test.ts` checks names only, so this prose change does not affect it.

### Binding doc comments

- `bindings/dart/lib/src/database_read.dart`, the doc comment above `List<Object?> readTimeSeriesRow(`. Old:
  ```dart
  /// Uses "last non-null value at or before [dateTime]" lookup semantics.
  /// Entries are typed by the column (`int`, `double`, or `String`); elements
  /// with no matching data yield `null`.
  ```
  New (append one sentence):
  ```dart
  /// Uses "last non-null value at or before [dateTime]" lookup semantics.
  /// Entries are typed by the column (`int`, `double`, or `String`); elements
  /// with no matching data yield `null`. Throws [DatabaseException] for a group
  /// with more than one dimension column; use [readTimeSeriesGroup] for those.
  ```
- `bindings/python/src/quiverdb/database.py`, the docstring of `def read_time_series_row(`. Old:
  ```python
        Uses "last non-null value at or before date_time" lookup semantics.
        Entries are typed by the column (int, float, or str); elements with no
        matching data yield None.
  ```
  New:
  ```python
        Uses "last non-null value at or before date_time" lookup semantics.
        Entries are typed by the column (int, float, or str); elements with no
        matching data yield None. Raises QuiverError for a group with more than
        one dimension column; use read_time_series_group for those.
  ```
- Julia and JS have no doc comment on this function. Do not add one.

### `CHANGELOG.md` — `## [0.12.0] — unreleased` → `### Changed`

Append as the **last bullet of `### Changed`**, directly above `### Fixed`. Earlier plans may have added bullets of their own; keep them.

```markdown
- **BREAKING — `read_time_series_row()` rejects a group with more than one dimension column.** In a
  group keyed by `date_time` plus another dimension such as `block` (every primary-key column except
  `id` is a dimension), each date holds one row per block, so there is no single value per element.
  The read used to pick one of those rows by accident. It could return null although another block
  held a value at that date (block 1 `10.0` and block 2 `NULL` read back as null), and with several
  non-null blocks it returned whichever row came last. It now throws `Cannot read_time_series_row:
  group '<g>' of collection '<c>' has more than one dimension column` in every binding, even when
  the collection is empty. Single-dimension groups are unchanged.

  *Adapt:* read a multi-dimension group with `read_time_series_group` and choose the block yourself.
```

No manifest version bump (0.12.0 is already the minor bump).

## Verification

From the repo root (`C:\Development\Quiver\quiver1`), in order:

1. `cmake --build build --config Debug`. The build must succeed.
2. `./build/bin/quiver_tests.exe --gtest_filter='Database.ReadTimeSeriesRow*:LuaRunnerTest.ReadTimeSeriesRow*'`. Expect every test to pass, including the new `Database.ReadTimeSeriesRowRejectsMultiDimensionGroup` and `LuaRunnerTest.ReadTimeSeriesRowRejectsMultiDimensionGroup` (two more tests than the same filter matched before this change; at HEAD 58dfe7a it matched 10).
3. `./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadTimeSeriesRow*'`. Expect every test to pass, including the new `DatabaseCApi.ReadTimeSeriesRowRejectsMultiDimensionGroup` (one more test than before this change; at HEAD 58dfe7a the filter matched 7).
4. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`. Both full suites must be green, in particular `Database.UpsertTimeSeriesRow*` and `Database.*TimeSeriesGroup*`, which use the same multi-dimension schema.
5. `bindings/julia/test/test.bat`. The new testset "Read Time Series Row - Multi-Dimension Group Rejected" must pass.
6. `bindings/dart/test/test.bat`. The new test "readTimeSeriesRow throws on a multi-dimension group" must pass.
7. `bindings/python/tests/test.bat`. The new test `test_read_time_series_row_multi_dimension_group_raises` must pass.
8. `bindings/js/test/test.bat`. The new test "throws on a group with more than one dimension column" must pass, and so must `lua-api-sync.test.ts`.
9. `scripts/format.bat`. Then `git diff`: clang-format may reflow the SQL concatenation in step 1b and the new C++ tests, and dart format / ruff / biome may reflow the binding tests. Accept those reflows.
10. `scripts/test-all.bat`. All six test suites must PASS. The seventh step, the CLI smoke test, fails at HEAD 58dfe7a because `example/example1.lua` no longer exists. Plan 65 fixes it, so a FAIL there is expected until 65 lands and is not caused by this change.

## Acceptance criteria

- [ ] `read_time_series_row` calls `internal::find_dimension_columns(*table_def)` and throws `Cannot read_time_series_row: group '<g>' of collection '<c>' has more than one dimension column` when it returns more than one column. The throw comes after the table lookup and before the attribute check and the empty-collection early return.
- [ ] The SQL's outer `ON` clause ends with `AND t.<attribute> IS NOT NULL`.
- [ ] New tests pass: C++ `Database.ReadTimeSeriesRowRejectsMultiDimensionGroup`, C API `DatabaseCApi.ReadTimeSeriesRowRejectsMultiDimensionGroup`, Lua `LuaRunnerTest.ReadTimeSeriesRowRejectsMultiDimensionGroup`, plus the Julia, Dart, Python and JS tests named above. Each asserts the exact message.
- [ ] All existing single-dimension row-read tests pass unchanged.
- [ ] No C API signature, FFI declaration or binding wrapper code changed. Only doc comments changed in Dart and Python.
- [ ] Root `AGENTS.md` "Time series row" bullet, `docs/time_series.md` Rules paragraph, `include/quiver/database.h` comment, and the `lua-api.ts` section are updated as written above.
- [ ] `CHANGELOG.md` has the **BREAKING** entry as the last bullet of `### Changed` under `## [0.12.0] — unreleased`. No manifest version changed.
- [ ] `scripts/format.bat` leaves no diff after it runs a second time.

## Pitfalls

- **Guard placement.** If the guard goes below `if (element_ids.empty()) { return {}; }`, an empty collection returns `[]` and the first half of the C++ test fails. If it goes above the table lookup, a bad group name raises the wrong error. Put it between the `table_def` check and the `dim_col` line.
- **Plan 02 has landed before this one** and may have reworded `find_dimension_column` or the call site. Anchor on the `auto dim_col = ...` line inside `read_time_series_row`, whatever its right-hand side now is, and insert the guard directly above it. Do not replace `find_dimension_column` with `dim_cols.front()`.
- **Do not build a failing test for the ON-clause filter.** With the guard in place no reachable input distinguishes it. `ReadTimeSeriesRowSkipsNullValues` and the other existing tests show it does no harm.
- **Exact-message assertions:**
  - C++ `EXPECT_STREQ`, the C API `EXPECT_STREQ(quiver_get_last_error(), ...)`, Julia `==` and Dart `equals` all require the message character for character. Keep the wording identical in the core and in every test.
  - Lua (`expect_lua_error`), JS (`toThrow(string)`) and Python (`match=`) match a substring or regex search.
- **`lua-api.ts` is a template literal**: an unescaped backtick ends the string and breaks the JS build, plus every consumer of `LUA_DB_API_REFERENCE`.
- **clang-format reflows the SQL string concatenation.** Run `scripts/format.bat` and do not hand-align it. The `.bat` scripts are only run, never edited here, so their CRLF endings are not at risk.
- **Prebuilt binaries**: `build/bin` may be older than HEAD. Always rebuild (step 1) before running the gtest filters.
- **Python fixture**: `multi_dim_ts_db` uses a file-backed database under `tmp_path`. No `Configuration` row is needed for the read, because the guard fires before any data is read.

## Out of scope

- The null mask for `read_time_series_row`'s numeric results in the C API. Today 0 or NaN stand in for "no data". Owned by **plan 17**, which also rewrites the C header comment block for this function and the "no matching data" wording of the doc comments touched here.
- Making `find_dimension_column` primary-key based and fixing the alphabetical scan: **plan 02**.
- The shared group-table lookup helper and Pattern 2 misses in this function's prologue: **plan 57**.
- Moving `execute` into `Impl`: **plan 53**.
- A per-dimension (e.g. per-`block`) row reader, or a dimension-filter parameter on `read_time_series_row`. That is a new API; nobody has asked for it.
- `include/quiver/c/database.h` and `src/c/AGENTS.md`: the C API passes the error through unchanged, and plan 17 rewrites that comment block.
