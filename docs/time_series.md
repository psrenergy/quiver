# Time Series

It is possible to store time series data in your database. Time series in `Quiver` are very flexible. You can have missing values, and you can have sparse data.

There is a specific table format that must be followed. Consider the following example:

```sql
CREATE TABLE Resource (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Resource_time_series_group1 (
    id INTEGER,
    date_time TEXT NOT NULL,
    some_vector1 REAL,
    some_vector2 REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;
```

A time series table is named `{Collection}_time_series_{group}` and must be indexed by a
dimension column whose name starts with `date_` (usually `date_time`), stored as ISO 8601
text (`YYYY-MM-DDTHH:MM:SS`). The bindings convert their native datetime types to and from
this format automatically.

Notice that in this example, there are two value columns `some_vector1` and `some_vector2`. You can have as many value columns as you want. You can also separate the time series data into different tables, by creating a table `Resource_time_series_group2` for example.

It is also possible to add more dimensions to your time series, such as `block` and `scenario`.

```sql
CREATE TABLE Resource_time_series_group2 (
    id INTEGER,
    date_time TEXT NOT NULL,
    block INTEGER NOT NULL,
    some_vector3 REAL,
    some_vector4 REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time, block)
) STRICT;
```

## Rules

Time series in `Quiver` are very flexible. You can have missing values (`NULL`), and you can have sparse data.

When reading a single time series row at a given `date_time` (`read_time_series_row`), the
value returned for each element is the **last non-null value at or before** the queried
date. If an element has no data at or before that date, the entry is null (`nothing` in
Julia, `None` in Python, `null` in Dart/JS, `nil` in Lua).

For example, if you have the following data for the attribute `some_vector1`:

| **Date** | **Resource 1** | **Resource 2** |
|:--------:|:--------------:|:--------------:|
|   2020   |      1.0       |    missing     |
|   2021   |    missing     |      1.0       |
|   2022   |      3.0       |    missing     |

1. Querying at `2020` returns `[1.0, nothing]`.
2. Querying at `2021` returns `[1.0, 1.0]`.
3. Querying at `2022` returns `[3.0, 1.0]`.

### NULL cells in a group

`read_time_series_group` and `update_time_series_group` round-trip `NULL` value cells, so a
read → modify → write cycle preserves them. A `NULL` value surfaces per binding as `nothing`
(Julia), `None` (Python), `null` (Dart/JS), or `nil` (Lua); value columns come back null-padded
to the same length as the dimension column (in Julia they are typed `Vector{Union{T, Nothing}}`).
An all-`NULL` value column reads back as a full-length column of nulls — except in **Lua**, where
a `nil` cannot occupy an array slot: a `NULL` cell is a hole and an all-`NULL` column is an empty
table (with its key present). In Lua, take the row count from the dimension column
(`#ts.date_time`), never from a value column.

When writing, the dimension column(s) must be present and fully populated (they are primary-key
columns and cannot be `NULL`). Value columns may carry nulls; in Lua a value column may also be
shorter than, or sparser than, the dimension column — every missing cell is written as `NULL`.
A column longer than the dimension column is an error. (The FFI bindings still require every
column to have equal length — pass explicit nulls.)

## Inserting data

Time series data is column-oriented in every binding: a map of column name to a list of
values, one list entry per row. You can pass the data directly when creating an element:

```julia
using Dates
using Quiver

db = Quiver.from_schema(db_path, path_schema)

Quiver.create_element!(db, "Configuration"; label = "Toy Case")

Quiver.create_element!(
    db,
    "Resource";
    label = "Resource 1",
    date_time = [DateTime(2000), DateTime(2001), DateTime(2002)],
    some_vector1 = [1.0, 2.0, 3.0],
    some_vector2 = [4.0, 5.0, 6.0],
)
```

Or replace the whole group for an existing element with `update_time_series_group!`:

```julia
Quiver.update_time_series_group!(
    db,
    "Resource",
    "group1",
    id;
    date_time = [DateTime(2000), DateTime(2001)],
    some_vector1 = [10.0, 11.0],
    some_vector2 = [20.0, 21.0],
)
```

It is also possible to insert (or upsert) a single row. Calling it again with the same
dimension values overwrites the value columns:

```julia
Quiver.add_time_series_row!(
    db,
    "Resource",
    "group1",
    id;
    date_time = DateTime(2000),
    some_vector1 = 10.0,
)
```

## Reading data

### Reading a whole group

`read_time_series_group` returns the group's data column-oriented, sorted by the dimension
column. The dimension column is returned as native datetimes.

```julia
data = Quiver.read_time_series_group(db, "Resource", "group1", id)
# data["date_time"]    -> [DateTime(2000), DateTime(2001)]
# data["some_vector1"] -> [10.0, 11.0]
```

### Reading a single row

`read_time_series_row` returns one value per element of the collection (ordered by element
id) using the "last non-null value at or before `date_time`" rule described above:

```julia
values = Quiver.read_time_series_row(
    db,
    "Resource",
    "group1",
    "some_vector1";
    date_time = DateTime(2020),
)
```

## Updating data

`update_time_series_group!` replaces **all** rows of the group for the element. To change a
single row, use `add_time_series_row!` with the row's exact dimension values — existing
value columns at that dimension key are overwritten:

```julia
Quiver.add_time_series_row!(
    db,
    "Resource",
    "group2",
    id;
    date_time = DateTime(2000),
    block = 1,
    some_vector3 = 10.0,
)
```

## Deleting data

Updating a group with no data clears every row of that group for the element:

```julia
Quiver.update_time_series_group!(db, "Resource", "group1", id)
```

Reading a cleared group returns empty data.
