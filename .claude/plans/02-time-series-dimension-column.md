# 02 — One definition of the time-series dimension column (PK-based)

**Batch** 1 · **Severity** m · **Breaking** yes, narrowly: for schemas whose time-series table keeps its date column *outside* the `PRIMARY KEY` (metadata, readers, `list_time_series_groups`, CSV import/export and Lua's `db:update_time_series_group` now throw for such a group). `describe` text also changes for multi-dimension groups (display only). · **Size** M · **Layers** C++ core (`src/database_internal.h`, `src/database_describe.cpp`), tests in C++, C API, Lua, Julia, Dart, Python, JS; docs
**Depends on** none · **Overlaps with** 03, 04, 06, 57, 58, 67, 75, 76 (details under "Overlaps" at the end of "Constraints and decisions")

## Why

"Which column is a time series' dimension?" has two answers today, and the readers and the writers each use a different one.

`src/database_internal.h` `find_dimension_column` (currently ~L93-103) walks `table_def.columns`:

```cpp
inline std::string find_dimension_column(const TableDefinition& table_def) {
    for (const auto& [col_name, col] : table_def.columns) {
        if (col_name == "id")
            continue;
        if (col.type == DataType::DateTime || is_date_time_column(col_name)) {
            return col_name;
        }
    }
    throw std::runtime_error("Dimension column not found: time series table '" + table_def.name + "'");
}
```

`TableDefinition::columns` is a `std::map<std::string, ColumnDefinition>` (`include/quiver/schema.h` ~L42), so this returns the **alphabetically first** date-like column. It never looks at `primary_key`. Its sibling `find_dimension_columns` (currently ~L105-125) returns "every PK column except `id`, in declaration order" (`column_order` + `col->primary_key`).

The singular backs the readers: `get_time_series_metadata` (`src/database_time_series.cpp` ~L81, which becomes `GroupMetadata::dimension_column`), `read_time_series_group` (~L104, `ORDER BY dim_col` at ~L122) and `read_time_series_row` (~L283, `MAX(dim_col) ... WHERE dim_col <= ?` at ~L298-300, and the `attribute == dim_col` rejection at ~L286). The plural backs the writers: `update_time_series_group` (~L152) and `upsert_time_series_row` (~L235).

Nothing rejects a second `date_` column. `SchemaValidator` has no time-series checks, the duplicate-attribute check skips `date_` columns of time-series tables, `schema.cpp` (~L381) types every TEXT `date_*` column as `DATE_TIME`, and `docs/rules.md` tells users to prefix date attributes with `date_`. So `date_approved`, `date_end`, `date_commissioning` and similar are realistic value columns. Any of them sorts before `date_time`.

**Reproduction.** Run against the current `build/bin/quiver_cli.exe` with this table:

```sql
CREATE TABLE Plant_time_series_events (
    id INTEGER NOT NULL REFERENCES Plant(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    date_approved TEXT,
    value REAL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

The rows written were (date_time, date_approved, value) = (2024-01-01, 2024-03-01, 1.0), (2024-02-01, NULL, 2.0) and (2024-03-01, 2024-01-15, 3.0). The script returned:

```
{"dim":"date_approved", "value_columns":["date_time","value"], "value_order":[2,3,1], "row":[3],
 "describe":"... - events: [date_time], [date_approved], value(REAL)\n"}
```

Each field is wrong:
- The metadata reports `date_approved` as the dimension and lists `date_time` as a value column.
- `read_time_series_group` orders the rows by `date_approved`, with the NULL first.
- `read_time_series_row(..., "value", "2024-02-15T00:00:00")` answers along `date_approved` and returns 3.0. The correct answer is 2.0, the latest `date_time` at or before the query date.
- `read_time_series_row(..., "date_approved", ...)` throws `Time series attribute not found`, while `date_time` is accepted as an attribute.
- `describe` brackets `date_approved` as a dimension.

The Lua write `db:update_time_series_group("Plant","events",id,{date_time={...3 dates}, date_approved={"2024-03-01T00:00:00", nil, "2024-01-15T00:00:00"}, value={...}})` throws `Cannot update_time_series_group: dimension column 'date_approved' has nil at index 2`. The cause is that `time_series_rows_from_lua` (`src/lua_runner.cpp` ~L2137-2147) treats `metadata.dimension_column` as the dense row-count authority.

The same wrong answer reaches every FFI binding. `quiver_database_read_time_series_group` (`src/c/database_time_series.cpp` ~L150-152) puts `metadata.dimension_column` first, typed STRING. Julia (`bindings/julia/src/database_read.jl` ~L650-678), Python (`bindings/python/src/quiverdb/database.py` ~L1451-1470) and Dart (`bindings/dart/lib/src/database_read.dart` ~L1223-1245) parse the column named `dimension_column` into DateTime without checking its mask. From reading the code, a NULL `date_approved` cell makes that parse hit a NULL `char*`: `unsafe_string(C_NULL)` in Julia, `ffi.string(NULL)` in Python, and `toDartString()` on `nullptr` in Dart. Each of those throws. JS gets the columns swapped and in the wrong order. `export_csv` orders by the wrong column (`src/database_csv_export.cpp` ~L271).

There is a second, smaller inconsistency: a table whose date column is *outside* the key. Take `Meter_time_series_blocks(id, block INTEGER NOT NULL, date_time TEXT, value REAL, PRIMARY KEY (id, block))`, also reproduced with the CLI. Today its metadata reports `date_time` as the dimension, while `upsert_time_series_row` keys on `block` alone. `describe` prints `block(INTEGER), [date_time], value(REAL)`.

Principles violated: a single definition of one concept (root AGENTS.md "Intelligence: logic resides in C++" plus "Simple solutions"), and "Declaration order everywhere / nothing reports alphabetical order" (`src/AGENTS.md`, Core Internals).

## Constraints and decisions

- **Maintainer decision (binding):** keep the full predicate `col.type == DataType::DateTime || is_date_time_column(col_name)`. Apply it as a filter over `find_dimension_columns`, which returns the PK columns in declaration order. Do **not** narrow it to `DataType::DateTime` alone.
- **Maintainer decision (binding):** the regression tests use a **new** schema file under `tests/schemas/valid/`. Do not modify shared schemas such as `nullable_time_series.sql`.
- Root AGENTS.md "Status: WIP project - breaking changes acceptable" and "Changelog: ... Prefix a breaking one **BREAKING** and say what a caller must do". 0.12.0 is unreleased and already a minor bump, so there is no manifest bump.
- Root AGENTS.md "Error Messages": the existing Pattern 2 text `Dimension column not found: time series table '<table>'` is reused unchanged. No new message.
- Root AGENTS.md "Self-Updating": update `src/AGENTS.md` (nearest to `database_internal.h` and `database_describe.cpp`), root AGENTS.md (Schema Conventions / Time Series Tables, which states the dimension rule), and `tests/AGENTS.md` (schema list).
- Root AGENTS.md "Time-series group data is column-oriented", and the design decision on time-series NULL masks ("Lua ... the dimension column(s) are the row-count authority"). Both are unchanged, and this fix is what makes them true for a table with a second `date_` column.
- `src/AGENTS.md` Lua bullet: `time_series_rows_from_lua` discovers dimensions through the public metadata, as "`dimension_column` plus any `value_columns` with `primary_key` set". No Lua code change is needed. With the fix, a non-key `date_` column is an ordinary value column, not primary-key, so Lua lets it be sparse.

Alternatives considered and rejected:
- `find_dimension_columns(t).front()`: rejected because for `PRIMARY KEY (id, block, date_time)` with `block` declared first it returns `block`. The date filter is required.
- A DATE_TIME-only predicate (the policy verifier's variant): rejected by maintainer decision. It would also silently drop a non-TEXT `date_` key column.
- Adding a nullable `date_approved` to `nullable_time_series.sql` (the policy verifier's variant): rejected by maintainer decision. That schema is shared by nulls tests in five layers that assert its exact column list.
- Making the writers also require a date dimension, so they throw for the Meter case too: rejected as scope creep. The writers need only the key, and the only callers they could break are schemas that are already off-convention. Noted under "Out of scope".
- Rejecting a no-date-in-key table in `SchemaValidator`: out of scope (no plan owns it). It would move the failure to load time, and it is a separate validator design choice.
- Leaving `describe`'s name-based bracketing (`print_group_columns`) alone: rejected because it is the third definition of the same concept, and both verifiers flagged it. The fix is one token. It is the only display change.

Overlaps (all later plans; this plan runs first in numeric order):
- **03** (`update_time_series_group` INSERT from the union of row keys): edits the same file, `src/database_time_series.cpp`, but a different block, plus the `src/AGENTS.md` Lua bullet. This plan does **not** edit that bullet or `update_time_series_group`.
- **04** (`read_time_series_row` multi-dim guard + ON-clause filter): edits `read_time_series_row`, which keeps calling `internal::find_dimension_column`. It also edits `docs/time_series.md` ~L52-55; this plan edits ~L23-26 only. **Constraint for 04:** keep resolving the date axis through `find_dimension_column`. The test `Database.GetTimeSeriesMetadataDateColumnOutsidePrimaryKeyThrows` added here asserts that `read_time_series_row` on `Meter.blocks` throws. A plain `find_dimension_columns(...).front()` would make it return values ordered by `block`.
- **06** (SchemaValidator parent FK for set/time-series tables): the new schema here already declares `id ... REFERENCES <Parent>(id) ON DELETE CASCADE ON UPDATE CASCADE` on both time-series tables, so it keeps validating after 06.
- **57** (`group_table_name` lifted out of `database_describe.cpp`, `require_group_table`): touches `database_describe.cpp` and `get_time_series_metadata`, not `print_group_columns`' condition or `find_dimension_column`.
- **58** (import/export share a group lookup): notes that "Export still needs `internal::find_dimension_column(*table_def)`". It will inherit the PK-based definition from here.
- **67** (moves C++ describe content tests into `tests/test_database_describe.cpp`): the test added here goes into that same file. There is no conflict, but expect a textual neighbour.
- **75 / 76** (`tests/AGENTS.md` / `src/AGENTS.md` fixes): they edit different sentences of the same files.

## Changes

No change to `src/database_time_series.cpp`, the C API (`src/c/`, `include/quiver/c/`), any FFI declaration (Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py`, JS `loader.ts`), any binding wrapper, or `src/lua_runner.cpp`. All of them consume `find_dimension_column` directly or through `get_time_series_metadata`, so fixing the one helper fixes every caller. **Do not run any FFI generator.**

### 1. `src/database_internal.h`: define the singular in terms of the plural

Current (~L93-125):

```cpp
// Find the dimension/ordering column in a time series table
inline std::string find_dimension_column(const TableDefinition& table_def) {
    for (const auto& [col_name, col] : table_def.columns) {
        if (col_name == "id")
            continue;
        if (col.type == DataType::DateTime || is_date_time_column(col_name)) {
            return col_name;
        }
    }
    throw std::runtime_error("Dimension column not found: time series table '" + table_def.name + "'");
}

// Find all dimension columns for a time series table: every PK column except
// "id", returned in declaration order (column_order is populated from
// PRAGMA table_info, which reports columns in declaration order). Throws if
// the table has no dimension columns — mirrors find_dimension_column's
// contract so callers don't repeat the empty check.
inline std::vector<std::string> find_dimension_columns(const TableDefinition& table_def) {
    ...
}
```

New: swap the order, because the singular now calls the plural and must come after it. Keep `find_dimension_columns`' body byte-for-byte, reword its comment, and replace the singular's body:

```cpp
// Find all dimension columns for a time series table: every PK column except
// "id", returned in declaration order (column_order is populated from
// PRAGMA table_info, which reports columns in declaration order). These are the
// columns the writers key a row on. Throws if the table has none, so callers
// don't repeat the empty check.
inline std::vector<std::string> find_dimension_columns(const TableDefinition& table_def) {
    std::vector<std::string> dim_cols;
    for (const auto& col_name : table_def.column_order) {
        if (col_name == "id") {
            continue;
        }
        const auto* col = table_def.get_column(col_name);
        if (col && col->primary_key) {
            dim_cols.push_back(col_name);
        }
    }
    if (dim_cols.empty()) {
        throw std::runtime_error("Dimension column not found: time series table '" + table_def.name + "'");
    }
    return dim_cols;
}

// Find the date dimension of a time series table: the first of find_dimension_columns
// that holds dates. Built on the primary key, so the readers (metadata, ORDER BY, the
// read_time_series_row axis) agree with the writers; a date_ value column outside the
// key is never picked. Throws if the key holds no date column.
inline std::string find_dimension_column(const TableDefinition& table_def) {
    for (const auto& col_name : find_dimension_columns(table_def)) {
        if (table_def.columns.at(col_name).type == DataType::DateTime || is_date_time_column(col_name)) {
            return col_name;
        }
    }
    throw std::runtime_error("Dimension column not found: time series table '" + table_def.name + "'");
}
```

Why: this gives one rule in one place and deletes the alphabetical map walk. `columns.at` cannot throw here, because every name in `column_order` is a key of `columns` (both are filled from the same `PRAGMA table_info` loop in `schema.cpp`). For every existing test schema the result is unchanged: each one has `PRIMARY KEY (id, date_time)`, `(id, date_recorded)` or `(id, date_time, block)`, and I checked all 16 `*_time_series_*` tables under `tests/schemas/`.

Callers verified (grep `find_dimension_column` over `src/`): `database_time_series.cpp` `get_time_series_metadata` (~L81), `read_time_series_group` (~L104) and `read_time_series_row` (~L283) use the singular; `update_time_series_group` (~L152) and `upsert_time_series_row` (~L235) use the plural. No test includes `database_internal.h`.

### 2. `src/database_describe.cpp` `print_group_columns`: bracket the PK dimensions, not every `date_` column

Current (~L20-38):

```cpp
// Print a group's value columns in declaration order; time series dimension
// columns are bracketed, vector tables hide their structural vector_index.
void print_group_columns(std::ostream& out, const TableDefinition& table, GroupTableType type) {
    ...
        if (type == GroupTableType::TimeSeries && is_date_time_column(col_name)) {
            out << "[" << col_name << "]";
```

New (comment plus one condition; the rest of the function is unchanged):

```cpp
// Print a group's value columns in declaration order; a time series' dimension
// columns (its primary key minus id -- the set find_dimension_columns returns) are
// bracketed, vector tables hide their structural vector_index.
void print_group_columns(std::ostream& out, const TableDefinition& table, GroupTableType type) {
    ...
        if (type == GroupTableType::TimeSeries && col.primary_key) {
            out << "[" << col_name << "]";
```

Why: `id` is already skipped at the top of the loop, so `col.primary_key` is exactly `find_dimension_columns`' membership test. Calling the helper would cost a vector per table and would throw on a PK-less table, which `describe` must still render. Effect: `Plant.events` renders `[date_time], date_approved(DATE_TIME), value(REAL)`; `multi_dim_time_series.sql`'s `load` renders `[date_time], [block], load(REAL), flag(INTEGER)` (previously `block(INTEGER)`); a PK-less time-series table brackets nothing. Existing pins (`[date_time]` in `tests/test_database_describe.cpp` `DescribeCollection`, `tests/test_database_lifecycle.cpp` `DescribeTimeSeriesWithDimensionColumn` including `[date_recorded]`, `tests/test_c_api_database_metadata.cpp` `DescribeCollectionReturnsText`, `tests/test_lua_runner_describe.cpp` `DescribeCollection`) all use PK date columns and stay green. No binding describe test pins a `date_`/`block` bracket (grepped).

### 3. New schema `tests/schemas/valid/time_series_date_columns.sql`

This is a new file. Write it with LF endings; `.gitattributes` `* text=auto` handles the working tree.

```sql
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

-- Plant.events: date_time is the only date column in the primary key, so it is the dimension.
-- date_approved is a nullable value column that also starts with date_ and sorts before
-- date_time; it must never be taken for the dimension.
CREATE TABLE Plant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Plant_time_series_events (
    id INTEGER NOT NULL REFERENCES Plant(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    date_approved TEXT,
    value REAL,
    PRIMARY KEY (id, date_time)
) STRICT;

-- Meter.blocks: the date column sits outside the primary key, so the group has no date
-- dimension and its metadata and reads throw "Dimension column not found".
CREATE TABLE Meter (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Meter_time_series_blocks (
    id INTEGER NOT NULL REFERENCES Meter(id) ON DELETE CASCADE ON UPDATE CASCADE,
    block INTEGER NOT NULL,
    date_time TEXT,
    value REAL,
    PRIMARY KEY (id, block)
) STRICT;
```

I verified that this exact file loads through `from_schema` (it passes `SchemaValidator`) with the current CLI. It needs no CMake or binding registration, since every suite resolves `tests/schemas/` at runtime and the Julia publish workflow copies the whole directory. Only the tests below use it.

## Tests

The rows used in every layer (value, date_approved, date_time) are chosen so that ordering or walking by the wrong column is visible:

| date_time | date_approved | value |
|---|---|---|
| 2024-01-01T00:00:00 | 2024-03-01T00:00:00 | 1.5 |
| 2024-02-01T00:00:00 | NULL | 2.5 |
| 2024-03-01T00:00:00 | 2024-01-15T00:00:00 | 3.5 |

Ordered by `date_approved`, the values come back as 2.5, 3.5, 1.5. At 2024-02-15 the `date_time` axis gives 2.5, and the `date_approved` axis gives 3.5.

No existing test changes: every existing `dimension_column`/`dimensionColumn` assertion (C++ and C API time-series metadata, the Lua time-series tests, Dart, JS, Python) expects `date_time` on schemas where it is the PK date column.

### C++ core: `tests/test_database_time_series_metadata.cpp` (append at end of file)

```cpp
// time_series_date_columns.sql: Plant_time_series_events has a nullable date_approved value column
// that sorts before its primary-key date column date_time. The dimension comes from the key.
TEST(Database, GetTimeSeriesMetadataDateValueColumnIsNotTheDimension) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("time_series_date_columns.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_time_series_metadata("Plant", "events");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    ASSERT_EQ(metadata.value_columns.size(), 2);
    EXPECT_EQ(metadata.value_columns[0].name, "date_approved");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::DateTime);
    EXPECT_FALSE(metadata.value_columns[0].primary_key);
    EXPECT_EQ(metadata.value_columns[1].name, "value");

    auto groups = db.list_time_series_groups("Plant");
    ASSERT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].dimension_column, "date_time");
}

// Meter_time_series_blocks keys on (id, block); its date_time is a value column, so the group has
// no date dimension. Metadata and the readers refuse it instead of ordering by a column the writers
// never key on.
TEST(Database, GetTimeSeriesMetadataDateColumnOutsidePrimaryKeyThrows) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("time_series_date_columns.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto id = db.create_element("Meter", quiver::Element().set("label", std::string("Meter 1")));

    try {
        db.get_time_series_metadata("Meter", "blocks");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Dimension column not found: time series table 'Meter_time_series_blocks'");
    }
    EXPECT_THROW(db.list_time_series_groups("Meter"), std::runtime_error);
    EXPECT_THROW(db.read_time_series_group("Meter", "blocks", id), std::runtime_error);
    EXPECT_THROW(db.read_time_series_row("Meter", "blocks", "value", "2024-01-01T00:00:00"), std::runtime_error);
}
```

Before the fix, the first test fails on `dimension_column` (`"date_approved"`) and on `value_columns[0].name` (`"date_time"`). The second fails at `FAIL()`, because the metadata used to return `date_time`.

### C++ core: `tests/test_database_time_series_group.cpp` (append at end of file)

```cpp
// read_time_series_group orders by the primary-key date column, not by a date_ value column that
// sorts before it (date_approved's order is NULL, 2024-01-15, 2024-03-01).
TEST(Database, ReadTimeSeriesGroupOrdersByPrimaryKeyDateColumn) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("time_series_date_columns.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto id = db.create_element("Plant", quiver::Element().set("label", std::string("Plant 1")));

    db.update_time_series_group("Plant",
                                "events",
                                id,
                                {{{"date_time", std::string("2024-01-01T00:00:00")},
                                  {"date_approved", std::string("2024-03-01T00:00:00")},
                                  {"value", 1.5}},
                                 {{"date_time", std::string("2024-02-01T00:00:00")},
                                  {"date_approved", nullptr},
                                  {"value", 2.5}},
                                 {{"date_time", std::string("2024-03-01T00:00:00")},
                                  {"date_approved", std::string("2024-01-15T00:00:00")},
                                  {"value", 3.5}}});

    auto rows = db.read_time_series_group("Plant", "events", id);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-01-01T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[1].at("date_time")), "2024-02-01T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[2].at("date_time")), "2024-03-01T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_approved")), "2024-03-01T00:00:00");
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1].at("date_approved")));
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 1.5);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1].at("value")), 2.5);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2].at("value")), 3.5);
}
```

Before the fix, `rows[0].at("date_time")` is `"2024-02-01T00:00:00"`, because the NULL `date_approved` sorts first.

### C++ core: `tests/test_database_time_series_row.cpp` (append at end of file)

```cpp
// read_time_series_row walks the primary-key date column. date_approved is an ordinary attribute;
// the dimension itself is not one.
TEST(Database, ReadTimeSeriesRowUsesPrimaryKeyDateColumn) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("time_series_date_columns.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto id = db.create_element("Plant", quiver::Element().set("label", std::string("Plant 1")));

    db.update_time_series_group("Plant",
                                "events",
                                id,
                                {{{"date_time", std::string("2024-01-01T00:00:00")},
                                  {"date_approved", std::string("2024-03-01T00:00:00")},
                                  {"value", 1.5}},
                                 {{"date_time", std::string("2024-02-01T00:00:00")},
                                  {"date_approved", nullptr},
                                  {"value", 2.5}},
                                 {{"date_time", std::string("2024-03-01T00:00:00")},
                                  {"date_approved", std::string("2024-01-15T00:00:00")},
                                  {"value", 3.5}}});

    // Latest date_time at or before 2024-02-15 is 2024-02-01. (Walking date_approved instead picks
    // 2024-01-15, which is the 2024-03-01 row: 3.5.)
    auto values = db.read_time_series_row("Plant", "events", "value", "2024-02-15T00:00:00");
    ASSERT_EQ(values.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(values[0]), 2.5);

    // Last non-null date_approved at or before 2024-02-15 (by date_time) is the 2024-01-01 row's.
    auto approved = db.read_time_series_row("Plant", "events", "date_approved", "2024-02-15T00:00:00");
    ASSERT_EQ(approved.size(), 1);
    EXPECT_EQ(std::get<std::string>(approved[0]), "2024-03-01T00:00:00");

    try {
        db.read_time_series_row("Plant", "events", "date_time", "2024-02-15T00:00:00");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Time series attribute not found: 'date_time' in group 'events' of collection 'Plant'");
    }
}
```

Before the fix, the first read returns 3.5, the `date_approved` read throws `Time series attribute not found`, and the `date_time` read succeeds (the CLI reproduction confirmed the last two).

### C++ core: `tests/test_database_describe.cpp` (append at end of file; it already has the `open` and `contains` helpers)

```cpp
// Bracketed columns are exactly the time series' primary-key dimensions: a date_ value column is
// not bracketed, and a non-date key column (block) is.
TEST(DatabaseDescribe, TimeSeriesBracketsPrimaryKeyDimensionsOnly) {
    auto plant = open(VALID_SCHEMA("time_series_date_columns.sql")).describe_collection("Plant");
    EXPECT_TRUE(contains(plant, "- events: [date_time], date_approved(DATE_TIME), value(REAL)\n")) << plant;

    auto resource = open(VALID_SCHEMA("multi_dim_time_series.sql")).describe_collection("Resource");
    EXPECT_TRUE(contains(resource, "- load: [date_time], [block], load(REAL), flag(INTEGER)\n")) << resource;
}
```

Before the fix, the report shows `[date_approved]` and `block(INTEGER)`.

### C API: `tests/test_c_api_database_time_series_metadata.cpp` (append at end of file)

```cpp
TEST(DatabaseCApi, GetTimeSeriesMetadataDateValueColumnIsNotTheDimension) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series_date_columns.sql").c_str(), &options, &db),
        QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_group_metadata_t metadata;
    ASSERT_EQ(quiver_database_get_time_series_metadata(db, "Plant", "events", &metadata), QUIVER_OK);
    EXPECT_STREQ(metadata.dimension_column, "date_time");
    ASSERT_EQ(metadata.value_column_count, 2);
    EXPECT_STREQ(metadata.value_columns[0].name, "date_approved");
    EXPECT_EQ(metadata.value_columns[0].data_type, QUIVER_DATA_TYPE_DATE_TIME);
    EXPECT_EQ(metadata.value_columns[0].primary_key, 0);
    EXPECT_STREQ(metadata.value_columns[1].name, "value");
    quiver_database_free_group_metadata(&metadata);

    // A group with no date in its key reports the core's message through the one error channel.
    quiver_group_metadata_t meter{};
    EXPECT_EQ(quiver_database_get_time_series_metadata(db, "Meter", "blocks", &meter), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Dimension column not found: time series table 'Meter_time_series_blocks'");

    quiver_database_close(db);
}
```

### C API: `tests/test_c_api_database_time_series_group.cpp` (append at end of file)

This pins the C-layer logic itself: column 0 of the read is built from `metadata.dimension_column`.

```cpp
TEST(DatabaseCApi, ReadTimeSeriesGroupDimensionIsThePrimaryKeyDateColumn) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("time_series_date_columns.sql").c_str(), &options, &db),
        QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    quiver_element_set_string(element, "label", "Plant 1");
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Plant", element, &id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);

    // A NULL char* entry in a string column is a SQL NULL cell (no mask needed).
    const char* col_names[] = {"date_time", "date_approved", "value"};
    int col_types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* date_times[] = {"2024-01-01T00:00:00", "2024-02-01T00:00:00", "2024-03-01T00:00:00"};
    const char* approved[] = {"2024-03-01T00:00:00", nullptr, "2024-01-15T00:00:00"};
    double values[] = {1.5, 2.5, 3.5};
    const void* col_data[] = {date_times, approved, values};
    ASSERT_EQ(
        quiver_database_update_time_series_group(db, "Plant", "events", id, col_names, col_types, col_data, nullptr, 3, 3),
        QUIVER_OK);

    char** out_col_names = nullptr;
    int* out_col_types = nullptr;
    void** out_col_data = nullptr;
    uint8_t** out_col_has_value = nullptr;
    size_t col_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_time_series_group(db,
                                                     "Plant",
                                                     "events",
                                                     id,
                                                     &out_col_names,
                                                     &out_col_types,
                                                     &out_col_data,
                                                     &out_col_has_value,
                                                     &col_count,
                                                     &row_count),
              QUIVER_OK);
    ASSERT_EQ(col_count, 3);
    ASSERT_EQ(row_count, 3);

    EXPECT_STREQ(out_col_names[0], "date_time");
    EXPECT_STREQ(out_col_names[1], "date_approved");
    EXPECT_STREQ(out_col_names[2], "value");
    EXPECT_EQ(out_col_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(out_col_types[1], QUIVER_DATA_TYPE_DATE_TIME);
    EXPECT_EQ(out_col_types[2], QUIVER_DATA_TYPE_FLOAT);

    // Rows in date_time order; the dimension is dense, date_approved's NULL is an ordinary masked cell.
    auto** dims = static_cast<char**>(out_col_data[0]);
    EXPECT_STREQ(dims[0], "2024-01-01T00:00:00");
    EXPECT_STREQ(dims[1], "2024-02-01T00:00:00");
    EXPECT_STREQ(dims[2], "2024-03-01T00:00:00");
    EXPECT_EQ(out_col_has_value[0][1], 1);
    EXPECT_EQ(out_col_has_value[1][1], 0);
    auto** approved_out = static_cast<char**>(out_col_data[1]);
    EXPECT_STREQ(approved_out[0], "2024-03-01T00:00:00");
    EXPECT_STREQ(approved_out[2], "2024-01-15T00:00:00");
    auto* values_out = static_cast<double*>(out_col_data[2]);
    EXPECT_DOUBLE_EQ(values_out[0], 1.5);
    EXPECT_DOUBLE_EQ(values_out[1], 2.5);
    EXPECT_DOUBLE_EQ(values_out[2], 3.5);

    quiver_database_free_time_series_data(
        out_col_names, out_col_types, out_col_data, out_col_has_value, col_count, row_count);
    quiver_database_close(db);
}
```

Before the fix, `out_col_names[0]` is `"date_approved"` and `out_col_has_value[0][0] == 0`.

### Lua: `tests/test_lua_runner_time_series.cpp`

Insert right after `TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupDimensionNilThrows)` (currently ~L237-252).

```cpp
// A date_ value column outside the primary key is a value column, so it may hold nil like any other;
// only the key's date column (date_time) is the dense row-count authority.
TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupSparseDateValueColumn) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("time_series_date_columns.sql"));
    int64_t id = db.create_element("Plant", quiver::Element().set("label", "Plant 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local id = )" + std::to_string(id) +
                         R"(
        db:update_time_series_group("Plant", "events", id, {
            date_time = { "2024-01-01T00:00:00", "2024-02-01T00:00:00", "2024-03-01T00:00:00" },
            date_approved = { "2024-03-01T00:00:00", nil, "2024-01-15T00:00:00" },
            value = { 1.5, 2.5, 3.5 },
        })
        local meta = db:get_time_series_metadata("Plant", "events")
        assert(meta.dimension_column == "date_time", "Expected dimension_column 'date_time', got " .. tostring(meta.dimension_column))
        local data = db:read_time_series_group("Plant", "events", id)
        assert(#data.date_time == 3, "Expected 3 rows, got " .. #data.date_time)
        assert(data.date_time[1] == "2024-01-01T00:00:00", "Expected rows in date_time order")
        assert(data.date_time[3] == "2024-03-01T00:00:00", "Expected rows in date_time order")
        assert(data.date_approved[1] == "2024-03-01T00:00:00", "Expected date_approved[1]")
        assert(data.date_approved[2] == nil, "Expected date_approved[2] to be nil, SQL NULL")
        assert(data.value[2] == 2.5, "Expected value[2] == 2.5")
    )";
    lua.run(script);
}
```

Before the fix, `lua.run` throws `Cannot update_time_series_group: dimension column 'date_approved' has nil at index 2` (reproduced with the CLI).

### Julia: `bindings/julia/test/test_database_time_series_group.jl`

Add a new `@testset` inside the outer `@testset "Time Series Group" begin ... end`, just before its closing `end`, which is the second-to-last `end` of the file.

```julia
    @testset "Date value column is not the dimension" begin
        # time_series_date_columns.sql: date_approved is a nullable value column that sorts
        # before the primary-key date column date_time.
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series_date_columns.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        id = Quiver.create_element!(db, "Plant"; label = "Plant 1")

        metadata = Quiver.get_time_series_metadata(db, "Plant", "events")
        @test metadata.dimension_column == "date_time"
        @test [c.name for c in metadata.value_columns] == ["date_approved", "value"]

        Quiver.update_time_series_group!(db, "Plant", "events", id;
            date_time = ["2024-01-01T00:00:00", "2024-02-01T00:00:00", "2024-03-01T00:00:00"],
            date_approved = ["2024-03-01T00:00:00", nothing, "2024-01-15T00:00:00"],
            value = [1.5, 2.5, 3.5],
        )

        result = Quiver.read_time_series_group(db, "Plant", "events", id)
        @test result["date_time"] == [DateTime(2024, 1, 1), DateTime(2024, 2, 1), DateTime(2024, 3, 1)]
        @test result["date_approved"] == ["2024-03-01T00:00:00", nothing, "2024-01-15T00:00:00"]
        @test result["value"] == [1.5, 2.5, 3.5]

        Quiver.close!(db)
    end
```

Before the fix, the metadata assertions fail, and `read_time_series_group` errors: it parses `date_approved` as the DateTime dimension, and `unsafe_string` on its NULL cell throws `ArgumentError`.

### Dart: `bindings/dart/test/database_time_series_group_test.dart`

Add a new `group` at the end of `main()`, after the `group('Time Series Update By Label', ...)` block and before `main`'s closing `}`.

```dart
  group('Time Series Dimension Column', () {
    // time_series_date_columns.sql: date_approved is a nullable value column
    // that sorts before the primary-key date column date_time.
    test('a date_ value column is not the dimension', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series_date_columns.sql'),
      );
      try {
        final id = db.createElement('Plant', {'label': 'Plant 1'});

        final meta = db.getTimeSeriesMetadata('Plant', 'events');
        expect(meta.dimensionColumn, equals('date_time'));
        expect(
          meta.valueColumns.map((c) => c.name).toList(),
          equals(['date_approved', 'value']),
        );

        db.updateTimeSeriesGroup('Plant', 'events', id, {
          'date_time': ['2024-01-01T00:00:00', '2024-02-01T00:00:00', '2024-03-01T00:00:00'],
          'date_approved': ['2024-03-01T00:00:00', null, '2024-01-15T00:00:00'],
          'value': [1.5, 2.5, 3.5],
        });

        final result = db.readTimeSeriesGroup('Plant', 'events', id);
        expect(
          result['date_time'],
          equals([DateTime(2024, 1, 1), DateTime(2024, 2, 1), DateTime(2024, 3, 1)]),
        );
        expect(
          result['date_approved'],
          equals(['2024-03-01T00:00:00', null, '2024-01-15T00:00:00']),
        );
        expect(result['value'], equals([1.5, 2.5, 3.5]));
      } finally {
        db.close();
      }
    });
  });
```

Before the fix, the metadata assertions fail, and `readTimeSeriesGroup` throws: it calls `toDartString()` on the NULL `date_approved` pointer, taking it for the dimension.

### Python: `bindings/python/tests/conftest.py` (append at end of file) and `bindings/python/tests/test_database_time_series_group.py` (append at end of file)

`conftest.py`: follow the existing path-fixture plus db-fixture pattern.

```python
@pytest.fixture
def time_series_date_columns_schema_path(schemas_path: Path) -> Path:
    """Return the path to the schema with a date_ value column beside the time series dimension."""
    return schemas_path / "valid" / "time_series_date_columns.sql"


@pytest.fixture
def time_series_date_columns_db(
    time_series_date_columns_schema_path: Path, tmp_path: Path
) -> Generator[Database, None, None]:
    """Create a test database with the time_series_date_columns schema."""
    database = Database.from_schema(str(tmp_path / "ts_date_columns.db"), str(time_series_date_columns_schema_path))
    yield database
    database.close()
```

`test_database_time_series_group.py` (it already defines `_utc`):

```python
# -- Dimension column (time_series_date_columns_db: a date_ value column beside the key) --


class TestTimeSeriesDimensionColumn:
    def test_date_value_column_is_not_the_dimension(self, time_series_date_columns_db: Database) -> None:
        """date_approved sorts before date_time but is not in the primary key, so it is a value column."""
        db = time_series_date_columns_db
        eid = db.create_element("Plant", label="Plant 1")

        meta = db.get_time_series_metadata("Plant", "events")
        assert meta.dimension_column == "date_time"
        assert [c.name for c in meta.value_columns] == ["date_approved", "value"]

        db.update_time_series_group(
            "Plant",
            "events",
            eid,
            {
                "date_time": ["2024-01-01T00:00:00", "2024-02-01T00:00:00", "2024-03-01T00:00:00"],
                "date_approved": ["2024-03-01T00:00:00", None, "2024-01-15T00:00:00"],
                "value": [1.5, 2.5, 3.5],
            },
        )
        result = db.read_time_series_group("Plant", "events", eid)
        assert list(result) == ["date_time", "date_approved", "value"]
        assert result["date_time"] == [_utc(2024, 1, 1), _utc(2024, 2, 1), _utc(2024, 3, 1)]
        assert result["date_approved"] == ["2024-03-01T00:00:00", None, "2024-01-15T00:00:00"]
        assert result["value"] == [1.5, 2.5, 3.5]
```

Before the fix, the metadata assertions fail, and the read raises when `ffi.string` meets the NULL `date_approved` pointer, taken for the dimension.

### JS: `bindings/js/test/database-time-series-group.test.ts`

Add a schema constant after the `NULLABLE_TS_SCHEMA` constant (currently ~L30-39), in the same multi-line `join(...)` form as the others:

```ts
const DATE_COLUMNS_TS_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "time_series_date_columns.sql",
);
```

Then append at the end of the file:

```ts
describe("time series dimension column", () => {
  // time_series_date_columns.sql: date_approved is a nullable value column that
  // sorts before the primary-key date column date_time.
  test("a date_ value column is not the dimension", () => {
    const db = Database.fromSchema(":memory:", DATE_COLUMNS_TS_SCHEMA);
    try {
      const id = db.createElement("Plant", { label: "Plant 1" });

      const meta = db.getTimeSeriesMetadata("Plant", "events");
      expect(meta.dimensionColumn).toEqual("date_time");
      expect(meta.valueColumns.map((c) => c.name)).toEqual(["date_approved", "value"]);

      db.updateTimeSeriesGroup("Plant", "events", id, {
        date_time: ["2024-01-01T00:00:00", "2024-02-01T00:00:00", "2024-03-01T00:00:00"],
        date_approved: ["2024-03-01T00:00:00", null, "2024-01-15T00:00:00"],
        value: [1.5, 2.5, 3.5],
      });

      const result = db.readTimeSeriesGroup("Plant", "events", id);
      expect(Object.keys(result)).toEqual(["date_time", "date_approved", "value"]);
      expect(result.date_time).toEqual([
        "2024-01-01T00:00:00",
        "2024-02-01T00:00:00",
        "2024-03-01T00:00:00",
      ]);
      expect(result.date_approved).toEqual(["2024-03-01T00:00:00", null, "2024-01-15T00:00:00"]);
      expect(result.value).toEqual([1.5, 2.5, 3.5]);
    } finally {
      db.close();
    }
  });
});
```

Before the fix, `dimensionColumn` is `"date_approved"`, the key order is swapped, and the rows come back ordered by `date_approved`.

## Docs and changelog

1. **`src/AGENTS.md`**, File Map line:
   - Old: `  database_internal.h     # internal:: helpers - read templates, value_matches_type, metadata converters`
   - New: `  database_internal.h     # internal:: helpers - read templates, value_matches_type, metadata converters, time-series dimension lookup`
2. **`src/AGENTS.md`**, "Core Internals Worth Knowing": insert a new bullet directly after the bullet that starts `- **Declaration order everywhere**:`.
   ```
   - **One definition of a time series' dimensions** (`database_internal.h`): `find_dimension_columns`
     is every primary-key column except `id`, in declaration order — what `update_time_series_group`
     and `upsert_time_series_row` key a row on. `find_dimension_column` is the first of those that
     holds dates (DATE_TIME-typed or `date_`-named): `GroupMetadata::dimension_column`, the column
     `read_time_series_group` orders by, the axis `read_time_series_row` walks, and (through the
     metadata) the C API's column 0, the bindings' DateTime-parsed column, Lua's row-count authority
     and `export_csv`'s row order. It used to scan the name-sorted `columns` map, so a `date_`
     *value* column sorting before `date_time` (`date_approved`) became the readers' dimension while
     the writers keyed on the primary key. Don't reintroduce a name- or map-order scan, and don't
     "simplify" it to `find_dimension_columns(...).front()` — a key declared `(id, block, date_time)`
     would return `block`. A table whose key holds no date column has no dimension: metadata and
     every reader throw `Dimension column not found`, while the writers, which need only the key,
     still work. `describe`'s brackets (`print_group_columns`) mark the same primary-key set.
   ```
3. **Root `AGENTS.md`**, Schema Conventions, "### Time Series Tables":
   - Old: ``Named `{Collection}_time_series_{name}` with a dimension (ordering) column whose name starts with `date_` (e.g., `date_time`), stored as ISO 8601 text (`YYYY-MM-DDTHH:MM:SS`):``
   - New: ``Named `{Collection}_time_series_{name}` with a dimension (ordering) column: the first primary-key column after `id` whose name starts with `date_` (e.g., `date_time`), stored as ISO 8601 text (`YYYY-MM-DDTHH:MM:SS`). Any other `date_` column is an ordinary value column:``
4. **`tests/AGENTS.md`**, "## Schemas", `valid/` list:
   - Old: `` `multi_time_series.sql`, `nullable_time_series.sql`, `relations.sql` ``
   - New: `` `multi_time_series.sql`, `nullable_time_series.sql`, `relations.sql`, `time_series_date_columns.sql` ``
   - Add a sub-bullet after the `multi_column_groups.sql` sub-bullet:
   ```
     - `time_series_date_columns.sql` pins which column is a time series' dimension.
       `Plant_time_series_events` has a nullable `date_approved` value column that sorts before its
       key column `date_time`, so a lookup that scans columns by name instead of the primary key
       picks the wrong one. `Meter_time_series_blocks` keeps its date column outside the key
       (`PRIMARY KEY (id, block)`), so it has no dimension and its metadata and reads throw — use
       `Meter` only to test that refusal.
   ```
5. **`docs/time_series.md`**, currently ~L23-26 (do not touch ~L52-55, which plan 04 owns):
   - Old:
     ```
     A time series table is named `{Collection}_time_series_{group}` and must be indexed by a
     dimension column whose name starts with `date_` (usually `date_time`), stored as ISO 8601
     text (`YYYY-MM-DDTHH:MM:SS`). The bindings convert their native datetime types to and from
     this format automatically.
     ```
   - New:
     ```
     A time series table is named `{Collection}_time_series_{group}` and must be indexed by a
     dimension column: a column whose name starts with `date_` (usually `date_time`) and that is
     part of the table's `PRIMARY KEY`, stored as ISO 8601 text (`YYYY-MM-DDTHH:MM:SS`). Any other
     `date_` column is an ordinary value column: it may be NULL and `read_time_series_row` can read
     it. The bindings convert their native datetime types to and from this format automatically.
     ```
6. No change to `bindings/js/src/lua-api.ts` (the Lua surface is unchanged, and `dimension_column = "date_time"` there stays correct), the binding READMEs, or any binding `AGENTS.md`. Their "the dimension column's mask is always all 1" sentences become true for this schema; they were never edited.
7. **`CHANGELOG.md`**: append under `## [0.12.0] — unreleased` → `### Changed`, after the last existing bullet of that subsection, which is the `export_csv()` quoting entry ending `*Adapt:* regenerate golden files ...`. Plan 01 may have added bullets before this one, so append after whatever is last in `### Changed`:
   ```
   - **BREAKING — a time series' dimension column is the date column of its primary key.** The
     dimension (`get_time_series_metadata`'s `dimension_column`, the row order of
     `read_time_series_group` and `export_csv`, the axis `read_time_series_row` walks, and the dense
     row-count column of Lua's `db:update_time_series_group`) used to be the alphabetically first
     `date_` column, while `update_time_series_group` and `upsert_time_series_row` keyed on the
     primary key. A `date_` value column sorting before `date_time` (say `date_approved`) was
     therefore taken for the dimension: rows came back ordered by it, `read_time_series_row`
     answered along it, Julia/Python/Dart reads failed on its NULL cells, and a Lua write with a
     `nil` in it threw. The dimension is now the first primary-key column after `id` that is
     DATE_TIME-typed or `date_`-named; any other `date_` column is an ordinary value column.
     `describe` brackets exactly the primary-key columns, so a multi-dimension group now shows
     `[block]` as well.

     *Adapt:* a time-series table whose date column is not in its `PRIMARY KEY` now has no
     dimension — `get_time_series_metadata`, `list_time_series_groups`, `read_time_series_group`,
     `read_time_series_row`, `export_csv`/`import_csv` and Lua's `db:update_time_series_group`
     throw `Dimension column not found: time series table '<table>'` for it. Add the date column to
     the key, e.g. `PRIMARY KEY (id, date_time)`.
   ```

## Verification

From the repo root (PowerShell):

1. `cmake --build build --config Debug`: builds without warnings in the two edited files.
2. `.\build\bin\quiver_tests.exe --gtest_filter="Database.GetTimeSeriesMetadata*:Database.ListTimeSeriesGroups*:Database.ReadTimeSeriesGroup*:Database.ReadTimeSeriesRow*:Database.UpdateTimeSeriesGroup*:Database.UpsertTimeSeriesRow*:DatabaseDescribe.*:TempFileFixture.Describe*:LuaRunnerTest.*TimeSeries*:LuaRunnerTest.Describe*"`: all pass, including the new `Database.GetTimeSeriesMetadataDateValueColumnIsNotTheDimension`, `Database.GetTimeSeriesMetadataDateColumnOutsidePrimaryKeyThrows`, `Database.ReadTimeSeriesGroupOrdersByPrimaryKeyDateColumn`, `Database.ReadTimeSeriesRowUsesPrimaryKeyDateColumn`, `DatabaseDescribe.TimeSeriesBracketsPrimaryKeyDimensionsOnly` and `LuaRunnerTest.UpdateTimeSeriesGroupSparseDateValueColumn`.
3. `.\build\bin\quiver_c_tests.exe --gtest_filter="DatabaseCApi.*TimeSeries*:DatabaseCApiMetadata.*"`: all pass, including `DatabaseCApi.GetTimeSeriesMetadataDateValueColumnIsNotTheDimension` and `DatabaseCApi.ReadTimeSeriesGroupDimensionIsThePrimaryKeyDateColumn`.
4. Optional red check: `git stash push src/database_internal.h src/database_describe.cpp`, rebuild, and rerun steps 2-3. Exactly the 8 new C++/C tests should fail, with the Lua one throwing `dimension column 'date_approved' has nil at index 2`. Then run `git stash pop` and rebuild.
5. Full suites: `.\build\bin\quiver_tests.exe` and `.\build\bin\quiver_c_tests.exe`, with zero failures.
6. Bindings, each with zero failures and its new test run:
   - `.\bindings\julia\test\test.bat` (new testset "Date value column is not the dimension")
   - `.\bindings\dart\test\test.bat` ("a date_ value column is not the dimension")
   - `.\bindings\python\tests\test.bat` (`TestTimeSeriesDimensionColumn::test_date_value_column_is_not_the_dimension`)
   - `.\bindings\js\test\test.bat` ("a date_ value column is not the dimension")
7. No generator run is needed, because no C API signature changed.
8. `.\scripts\format.bat`, then `git status` / `git diff --stat`. Only the files listed in this plan may change. Revert any unrelated formatter churn.
9. `.\scripts\test-all.bat`: all seven steps green.

## Acceptance criteria

- [ ] `find_dimension_column` iterates `find_dimension_columns(table_def)`, keeps the predicate `type == DataType::DateTime || is_date_time_column(name)`, and no longer iterates `table_def.columns`.
- [ ] `find_dimension_columns` is defined before `find_dimension_column`, with its body unchanged and its comment no longer saying "mirrors find_dimension_column's contract".
- [ ] `print_group_columns` brackets on `col.primary_key` for time-series tables; its comment says so.
- [ ] `tests/schemas/valid/time_series_date_columns.sql` exists with the `Plant` and `Meter` collections exactly as above. No shared schema is modified.
- [ ] The new tests pass: 4 C++ core tests in 3 files, 1 describe test, 2 C API tests, 1 Lua test, and 1 each in Julia, Dart, Python and JS. All pre-existing tests still pass unchanged.
- [ ] No C API, FFI declaration, binding wrapper or `lua_runner.cpp` change. No generator run.
- [ ] `src/AGENTS.md` (file map plus the new bullet), root `AGENTS.md` (Time Series Tables sentence), `tests/AGENTS.md` (schema list plus sub-bullet) and `docs/time_series.md` (~L23-26) are updated as specified.
- [ ] A CHANGELOG **BREAKING** entry is under `[0.12.0] — unreleased` → `### Changed`, with an *Adapt:* line. No manifest version bump.
- [ ] `scripts/format.bat` leaves no diff outside the touched files, and `scripts/test-all.bat` is green.

## Pitfalls

- **Definition order in `database_internal.h`.** These are non-template inline functions, so `find_dimension_columns` must appear above `find_dimension_column`, or the build fails with "identifier not found".
- **Do not use `.front()`.** `find_dimension_columns(t).front()` passes every existing test, because every existing multi-dim schema declares `date_time` before `block`, yet it is wrong for `PRIMARY KEY (id, block, date_time)`. The date filter is required.
- **Do not drop `is_date_time_column`** from the predicate (maintainer decision), even though it is redundant for TEXT columns (`schema.cpp` already types them DATE_TIME).
- **Line endings.** Working-tree `.sql` files are CRLF through `core.autocrlf=true`, with LF in the index. Writing the new schema with LF is fine, and git may print a CRLF warning. No `.bat` file is touched; if the formatter or an editor rewrites one, restore CRLF.
- **The `Meter` collection is deliberately unreadable.** Do not use `time_series_date_columns.sql` in unrelated tests. Anything that calls `list_time_series_groups("Meter")`, `export_csv`/`import_csv` on `Meter` or `db:update_time_series_group` on `Meter.blocks` throws by design.
- **Binding tests fail differently before the fix.** Julia's testset *errors* (`ArgumentError` from `unsafe_string(C_NULL)`) instead of failing an `@test`. Dart and Python raise inside `readTimeSeriesGroup` / `read_time_series_group`. JS just returns swapped data. All four are real regressions caught by the same test.
- **Dart `DateTime(2024, 1, 1)`** is local time and matches `stringToDateTime`'s output for `2024-01-01T00:00:00`, like the existing group tests. Python's dimension comes back tz-aware UTC, so compare with the file's `_utc(...)` helper, not naive `datetime`.
- **Python line length** (ruff, 120) and **JS line width** (biome, 100): the fixture signature and the long `expect` lines are pre-wrapped above, but still run `scripts/format.bat` and accept its wrapping.
- **Existing describe pins** (`[date_time]`, `[date_recorded]`) stay green because those columns are primary-key members. If a describe test fails, check that the condition is `col.primary_key` and that the `id` skip above it is still in place.
- **Plans 03/04 run later on `src/database_time_series.cpp`.** Nothing is edited there in this plan. Do not "tidy" `update_time_series_group`'s `dim_cols.front()` (plan 03's area) or `read_time_series_row` (plan 04's).
- **No sol2 Debug/Release sensitivity.** No Lua converter code changes, so the Debug build suffices.

## Out of scope

- `update_time_series_group` building its INSERT from `rows[0]`, and the related "rows stay uniform" caveats in `src/AGENTS.md` / `src/c/AGENTS.md`: **plan 03**.
- `read_time_series_row` on multi-dimension groups (Pattern 1 guard, ON-clause `IS NOT NULL`) and its `docs/time_series.md` ~L52-55 text: **plan 04**.
- The `read_time_series_row` C API presence mask: **plan 17**.
- The group-table lookup and message cleanup in `get_time_series_metadata` / `find_time_series_table`: **plan 57**.
- `export_csv`/`import_csv`'s own `is_date_time_column(group_meta.dimension_column)` type-map lines: **plan 58** (which already expects to call `find_dimension_column`).
- Parent-FK validation for time-series tables: **plan 06** (the new schema already complies).
- Not planned anywhere, and deliberately not done:
  - Making the writers reject a key with no date column. They keep keying on the primary key.
  - A `SchemaValidator` check that a time-series key contains a date column. That would move the Meter case's error from first use to load time.
  - Removing the `is_ts && col_name.starts_with("date_")` skip in `schema_validator.cpp`'s duplicate-attribute check.
