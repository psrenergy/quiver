# Changelog

All notable changes to Quiver are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Entries that require
callers to change something are prefixed **BREAKING** and say what to do.

## [0.13.0] — unreleased

### Changed

- **BREAKING** **Lua table arguments are type-checked.** A value other than a table passed where
  a Lua method takes a table now raises `Cannot <op>: <argument> must be a table, got <type>`:
  an element table (`create_element`, `update_element`, `update_element_by_label`,
  `quiver.metadata_from_element`), the columns of `db:update_vector_group`,
  `db:update_set_group`, `db:update_time_series_group` and their `_by_label` forms, the row of
  `upsert_time_series_row`, the `paths` of `update_time_series_files`, the `dims` of `file:read`,
  the `data` and `dims` of `file:write`, the `labels` of `expr:select_agents` and every options
  table. A non-table column inside a group writer's columns raises `Cannot <op>: column '<name>'
  must be an array of values, got <type>`, and a userdata as an element attribute value raises
  `Cannot <op>: attribute '<name>' must be a value or a table, got userdata`. Release builds used
  to read a non-table there as a table: a userdata such as `db` passed as a group writer's columns
  was read as an empty payload and cleared the group. The existing `must be a table` messages
  (options tables, `header`, `enum_labels`, `quiver.metadata` fields, `expr:rename_agents`,
  `w:write_row`) now end in `, got <type>` too. Pass a table, e.g. `{ column = { values... } }`
  for the group writers.
- **BREAKING** **A wrong-typed optional Lua argument throws instead of being ignored.** The
  `params` of `db:query_string` / `query_integer` / `query_float`, the metadata of
  `db:open_file`, the `aggregate` flag of `db:bin_to_csv`, the `allow_nulls` flag of `file:read`
  and the parameter of `expr:aggregate` / `expr:aggregate_agents` now raise `Cannot <op>: <argument>
  must be <a table | a BinaryMetadata | a boolean | a number>, got <type>` for a value of the wrong
  type, where they used to fall back to the default (`db:query_integer("SELECT 1", 5)` ran with no
  parameters). Pass `nil` or omit the argument to get the default.
- **BREAKING** **An empty array in Lua `update_element` clears the group.** `update_element` /
  `update_element_by_label` with `{ column = {} }` now clear the group holding that column, as the
  C++, Python and JS bindings already did; Lua used to skip the empty array. A misspelled empty
  column now throws `Cannot update_element: array '<name>' does not match any vector, set, or time
  series table ...` instead of being ignored. A `read_vectors_by_id` -> `update_element` round trip
  of a column that read back empty or all-NULL now clears that group when it is the group's only
  column in the call, and throws `... must have the same length` when another column of the group
  is non-empty. A column name shared by several groups clears every one of them. `create_element`
  still skips an empty array. To leave a group untouched, omit its column.

### Fixed

- **A non-string key in a Lua table argument is a Pattern 1 error.** A number or boolean key in an
  element table, a time-series row, a `file:read`/`file:write` `dims` table or the `paths` of
  `update_time_series_files` now raises `Cannot <op>: <attribute|column|dimension> name must be a
  string, got <type>`. Release builds used to spell a number key as text (`column '1' not
  found ...`) and could crash on a boolean key.
- **`db:transaction` / `db:dry_run` check their argument before opening anything.** A value other
  than a function now raises `Cannot transaction: fn must be a function, got <type>` (or `Cannot
  dry_run: ...`) before a transaction or dry run is opened. Release builds used to open the scope
  first and then fail to call the value (inside an already-open transaction they reported `Cannot
  begin_transaction: transaction already active` instead); Debug builds reported sol2's raw
  argument text. A table with a `__call` metamethod is no longer accepted: pass a function.
- **`db:transaction` rolls back when its commit fails.** If the COMMIT at the end of the block
  fails (for example on a deferred foreign key), the block is now rolled back and the commit error
  is rethrown. The transaction used to be left open, so a host that committed afterwards wrote the
  failed block.

## [0.12.8] — 2026-10-01

### Changed

- **`create_element`/`update_element` errors name the operation.** A string written to a non-FK
  INTEGER column now reports `Cannot create_element: type mismatch for column 'x': expected
  INTEGER, got TEXT` (was `Cannot resolve attribute: ...`; `update_vector_group` /
  `update_set_group` name themselves the same way), and an unknown attribute reports
  `Cannot create_element: column 'x' not found in table 'T'` (was `Column 'x' not found in table
  'T'`). The accepted values are unchanged.
- **Missing-group errors use one pattern.** The time-series operations now report `Time series
  group not found: 'g' in collection 'c'` (was `Time series group 'g' not found for collection
  'c'`), matching `get_time_series_metadata`; a missing vector/set attribute reports `Vector
  attribute not found: 'a' in collection 'c'` (was `Vector attribute 'a' not found for collection
  'c'`); and `read_time_series_files` / `update_time_series_files` on a collection with no files
  table report `Time series files table not found: c_time_series_files` (was `... not found for
  collection 'c'`), matching `list_time_series_files_columns`.
- **`import_csv` types each column by its declared type alone.** A `date_`-named column that is
  not TEXT (`date_x INTEGER`, or a foreign key such as `date_id`) used to be parsed as a
  timestamp, so every number or label in it was rejected as `Timestamp ... is not valid`. It now
  imports like any other column of its type. A group-table cell naming a missing element now
  reports the same `Could not find an existing element ... Create the element before referencing
  it.` text as the scalar path; it lacked that last sentence before.
- **Schema errors name the offending column.** A column type Quiver does not support now fails
  with `Failed to validate schema: column 'payload' in table 'Items' has unsupported type 'BLOB'`
  (was `Unknown data type: BLOB`), and an unsafe table name with `Failed to validate schema:
  invalid table name '...'` (was `Cannot query columns: invalid table name: ...`).
- **Migration and schema-file errors name the method you called.** `from_migrations`,
  `validate_migrations` and `from_schema` now report e.g. `Failed to validate_migrations: down
  migration 2: ...` and `Cannot from_schema: schema file is empty: ...` instead of the private
  helper names `migrate_up` / `migrate_down` / `apply_schema`.
- **BREAKING (C++ only) — `data_type_from_string` (`quiver/data_type.h`) returns
  `std::optional<DataType>`.** It returns `std::nullopt` for an unsupported type instead of
  throwing `Unknown data type: ...`. *Adapt:* check the optional before using it.

### Added

- **Linux ARM64 (`aarch64`, e.g. NVIDIA DGX Spark) is a published platform.** PyPI ships a
  `manylinux_aarch64` wheel, and the Julia artifact and npm package carry `linux-aarch64` native
  libraries. They need glibc 2.28 or newer (Linux x86_64 stays at 2.17).

### Removed

- **BREAKING (C++ only) — unused `Row`/`Result` members removed:** `Row::size`, `column_count`,
  `empty`, `at`, `begin`, `end` and `Result::Result()`, `column_count`, `at`. They were reachable
  only from the installed headers, and no binding used them. `quiver/database.h` no longer includes
  `quiver/result.h`. *Adapt:* use `operator[]`, `is_null` and the `get_*` getters, construct an
  empty result as `Result({}, {})`, and read the column count as `columns().size()`. Include
  `quiver/result.h` directly if you need the type.
- **BREAKING (C++ only) — `Schema::get_data_type(table, column)` removed.** Its only caller was
  `TypeValidator`, which now reports an unknown column itself (see Changed). The public static
  `TypeValidator::validate_value` also no longer accepts a string for an INTEGER column; no
  `Database` method passes it one, since FK labels are resolved to ids first. *Adapt:* use
  `schema.get_table(table)->get_data_type(column)`, which returns `std::optional<DataType>`.
- **BREAKING (C++ only) — `quiver/schema.h`, `quiver/schema_validator.h` and
  `quiver/type_validator.h` are no longer installed.** They were internal (no binding and no C
  API used them), and `TypeValidator` is now three internal free functions. This supersedes the
  *Adapt* of the entry above: `Schema` is no longer reachable from outside the library either.
  *Adapt:* use `Database::get_*_metadata` / `list_*` for schema introspection.

### Fixed

- **`summarize_collection()`'s value distribution counts only integer cells.** In a non-STRICT
  INTEGER column a TEXT or REAL cell used to appear as a bogus code (`'abc'` as `0`, `1.5` as `1`)
  and count toward the 64-code cutoff.

## [0.12.7] — 2026-10-01

### Changed

- **BREAKING — Lua: `db:open_file` handles are closed when `run()` returns.** A binary file a
  script left open (for example in a global, without `f:close()`) used to stay open, so a writer
  kept its path blocked for reading and writing in the whole process. Readers and writers now
  follow the rule CSV writers already did. *Adapt:* reopen the file in each `run()` instead of
  reusing a handle kept in a global.

- **BREAKING — Lua: `db:export_csv`/`db:import_csv` options, `quiver.metadata{...}` and
  `expr:rename_agents` reject unknown keys and wrong types.** A misspelled key (`date_format`,
  `dimension_size`) or a wrong-typed value (`unit = 5`, `labels = "v1"`, a boolean rename target)
  used to be ignored or silently replaced by a default, and in Release some became empty strings.
  They now throw a `Cannot <op>: ...` error, like the `read_csv`/`write_csv` options already did.
  *Adapt:* fix the key or value the error names.

### Fixed

- **JS: the agent-facing Lua reference (`LUA_DB_API_REFERENCE`) no longer promises a rollback.**
  A failed script keeps every write that finished before the error; only `db:transaction` /
  `db:dry_run` undo their block. The CSV section now says that `import_csv` replaces the target
  table (and that `group = ""` is the scalar table), and that `upsert_time_series_row` and
  `update_time_series_files` replace the whole row.
- **Lua: an array of row tables passed to a group writer throws one clear error in every build**
  (`Cannot <method>: column names must be strings; pass { column = { values... } }, not an array
  of row tables`). It used to raise sol2's raw `stack index -1, expected string, received number`
  in Debug and a misleading `column '1' must be an array of values` in Release; a boolean key
  became column `''`. The agent-facing reference also now documents boolean cells, the real
  transaction/dry-run error texts, that `query_*` do not convert types, the `nil` holes in bulk
  reads and the trailing-`nil` query-parameter limit.
- **Lua: conversion errors name the method the script called.** An unsupported value passed to
  `db:create_element`, `db:update_element`(`_by_label`), `db:upsert_time_series_row`(`_by_label`),
  `db:query_*` or `quiver.metadata_from_element` now reports e.g. `Cannot create_element: attribute
  'x' has unsupported Lua type`, instead of an internal helper name (`table_to_element`,
  `lua_table_to_value_map`, `lua_table_to_values`). The same holds for an element array with a
  `nil` hole (`Cannot update_element: array 'x' has a nil hole ...`).

## [0.12.6] — 2026-09-30

### Added

- **JS: `LOG_LEVEL_*`, `DATA_TYPE_*` and the `DatabaseOptions` type are exported from
  `quiverdb`.** `mod.ts` now re-exports `src/index.ts` whole, so the package root can no longer
  drift from it. The `consoleLevel` values `DatabaseOptions` documents were previously not
  reachable from outside the package, and `ScalarMetadata.dataType` had no named constants.
- **Julia: `Element` accepts `nothing` and any `AbstractString` scalar.** `update_element!(db, c,
  id; attr = nothing)` (and the `create_element!` / `update_element_by_label!` keyword forms) now
  write SQL NULL, as every other binding already could; previously it raised a `MethodError`. A
  `SubString` scalar is accepted too, not only `String`.

### Changed

- **BREAKING — Python and Dart type a numeric group column or element array from every cell.**
  Python took a column's type from its first non-`None` cell and ran every other cell through
  `int()` or `float()`, so `update_vector_group(..., {"score": [1, 2.5]})` stored `[1.0, 2.0]` with
  no error. The same happened in `update_set_group`, `update_time_series_group` and their
  `_by_label` forms, and in `create_element`/`update_element` arrays that start with a `bool`. A
  `str` cell among numbers was parsed (`[1, "7"]` stored `7`). A float anywhere in a numeric column
  now makes it FLOAT, so `[1, 2.5]` stores `1.0` and `2.5`; bool/int columns stay INTEGER. A cell
  that does not fit its column — a `str` among numbers, a number among strings, a `str` among
  `datetime`s, any unsupported type — raises
  `TypeError: Unsupported value type <T> in cell <i> of column '<name>'` before the call. Dart
  applies the same whole-column rule in the group writers and `Element.set`, where `[1, 2.5]` used
  to throw `ArgumentError`. JS and Julia already behaved this way.

  *Adapt:* pass numbers, not numeric strings. A float anywhere in a list written to an INTEGER
  column is now rejected by the core instead of being truncated, including a whole-number one
  (`[65, 70.0]`); pass ints, or round the values yourself if you intended truncation.

- **BREAKING — Python: `collection`, `id`, `group` and `label` are positional-only on every
  `**kwargs` method.** `create_element`, `update_element`, `upsert_time_series_row` and
  `upsert_time_series_row_by_label` now mark their leading parameters positional-only, as
  `update_element_by_label` already did. An attribute with the same name as one of those parameters
  now reaches the core instead of failing with `TypeError: got multiple values for argument
  '<name>'` before the call. As a result, `db.update_element("C", eid, **db.read_scalars_by_id("C",
  eid))` works: the dict holds `id`, and it is written back as-is.

  *Adapt:* pass those parameters positionally. `update_element(collection="C", id=1, x=2)` now
  raises `TypeError`; write `update_element("C", 1, x=2)`.

- **BREAKING — JavaScript: a numeric array or group column with a non-number cell throws.** When a
  column's first non-null cell is a number, a `bigint` or a boolean, every other non-null cell must
  be one of those too. This applies to `createElement` / `updateElement` / `updateElementByLabel`
  arrays and to the six group writers (`updateTimeSeriesGroup`, `updateVectorGroup`,
  `updateSetGroup` and their `ByLabel` forms). The binding used to type the column from one cell
  and convert the rest with no error: in a nullable REAL column, `[1.5, "abc"]` stored
  `[1.5, NULL]` and `[1.5, "2"]` stored `[1.5, 2.0]`. It now throws
  `Cannot <method>: numeric column '<name>' has unsupported value type string in cell 1`. String
  columns are unchanged. `createElement` and `updateElement` now also map a boolean array cell to
  1/0 one cell at a time, as the group writers already did, so `[true, 5, false, 7]` stores
  `[1, 5, 0, 7]` instead of `[1, 1, 0, 1]`.

  *Adapt:* make every cell of a numeric column a number (or a `bigint` or a boolean); convert
  strings with `Number(...)` before the call.

- **Dart: the group writers' jagged-column `ArgumentError` names the offending column.** The six
  columnar writers (`updateVectorGroup`, `updateSetGroup`, `updateTimeSeriesGroup` and their
  `ByLabel` forms) now throw `All column lists must have the same length, got <n> for '<name>'`,
  Python's message; the error type is unchanged.

- **BREAKING — Dart: `Element.set` (and so `createElement` / `updateElement` /
  `updateElementByLabel`) no longer accepts a nested `Map` value.** It used to flatten the map and
  ignore its key, so `{'some_group': {'date_time': [...], 'value': [...]}}` behaved exactly like
  passing the columns flat, and a misspelled key was accepted silently. A `Map` now throws
  `ArgumentError` ("Unsupported type ... for '<name>'"). *Adapt:* pass the columns flat, or use
  `updateTimeSeriesGroup` / `updateVectorGroup` / `updateSetGroup` to write one named group.

- **BREAKING — Julia/Python: two date-time readers are renamed to the plural form.**
  `read_vector_date_time_by_id` → `read_vector_date_times_by_id` and `read_set_date_time_by_id` →
  `read_set_date_times_by_id`. They return a list, and the naming rule makes a list-returning
  reader plural (Dart already spelled them `readVectorDateTimesById` / `readSetDateTimesById`).
  `read_scalar_date_time_by_id` is unchanged. *Adapt:* rename the calls; there is no alias.

### Fixed

- **Python: a `datetime` is accepted on every write path, and an aware one is stored as its UTC
  instant.** `create_element`, `update_element` and `update_element_by_label` (scalar and list
  attributes) and `upsert_time_series_row` / `upsert_time_series_row_by_label` raised `TypeError`
  for a `datetime`, although every reader returns one, so a value read back could not be written
  back. The group writers and `read_time_series_row` did take one but formatted its wall clock and
  dropped the offset, while the readers stamp UTC: `10:00+03:00` was stored as `10:00` and read
  back as `10:00Z`, three hours off, and `read_time_series_row` looked up the wrong instant the
  same way. An aware value is now converted to UTC (`07:00`); a naive one is written as given.
  Rows written earlier from an aware non-UTC value keep the wall-clock time they were stored with.
- **Python: a `LuaRunner` whose construction fails is silent when it is garbage-collected.**
  `LuaRunner(db)` on a closed `Database` raised `QuiverError: Null argument: db` as it should, but
  the half-built object's `__del__` then emitted a spurious `ResourceWarning: LuaRunner was not
  closed explicitly` and printed `Exception ignored in … AttributeError: 'LuaRunner' object has no
  attribute '_ptr'`. A runner now counts as closed until its native handle exists.
- **Julia: `scalar_relation_map` / `set_relation_map` read in bulk.** They issued one query per
  element and a linear search per relation; they now make two bulk reads and a dictionary lookup,
  so they scale linearly. Results are unchanged.
- **JS: `bigint` is accepted by the group writers and as a query parameter.** `updateVectorGroup`,
  `updateSetGroup`, `updateTimeSeriesGroup` (and their `ByLabel` forms) and every `query*` method
  now take a `bigint` cell or parameter and write it as an exact int64, as `createElement` and
  `upsertTimeSeriesRow` already did. Previously the group writers threw `unsupported value type
  bigint` and the query methods `Unsupported query parameter type at index <i>: bigint`. A
  numeric group column or `createElement` / `updateElement` array may mix `bigint` with numbers
  and booleans: it is INTEGER unless a cell is fractional, which makes it FLOAT and converts a
  `bigint` through `Number()`. An element array led by a `bigint` now gets the same per-cell check
  as any other numeric array: `[7n, "12"]` used to store `12`, and `[7n, 1.5]` threw a raw
  `RangeError`.
- **Julia: `read_time_series_group` no longer leaks when decoding fails.** A dimension value that
  is not a valid date (possible in a database written before the DATE_TIME write gate, or by raw
  SQL) raised before the C result was freed.
- **Dart: the group readers no longer leak when decoding fails.** `readTimeSeriesGroup`,
  `readVectorGroupById` and `readSetGroupById` freed the C result only on success; a date value
  outside the accepted grammar (possible in a database written before the DATE_TIME write gate,
  or by raw SQL) leaked it on every call.

## [0.12.5] — 2026-09-29

### Added

- **JS: `readVectorGroupById()` / `readSetGroupById()`.** The whole-group readers Julia, Dart and
  Python already had. Each returns one record per row, `Record<string, number | string | null>[]`,
  read from the named group's own table in one statement: a SQL NULL cell is `null` in its row,
  and a DATE_TIME cell stays an ISO 8601 string, as in every JS reader. Prefer them to zipping
  `readVectorFloatsById` and the other per-column readers, which resolve a column *name*: when two
  groups of one kind share a column name (legal for a foreign key), the zip pairs another group's
  values with this one's.

### Changed

- **BREAKING — `read_time_series_row()` returns null, not `0` / `NaN`, for an element with no
  data.** The C++ core and Lua always did. The C API collapsed the missing value into a sentinel
  (`0` for an INTEGER column, `NaN` for a REAL one), so Julia, Dart, Python and JS returned a `0`
  that could not be told apart from a stored `0`, and a `NaN` their own docs did not mention.
  `quiver_database_read_time_series_row` now takes a `uint8_t** out_mask` out-parameter between
  `out_values` and `out_count`, filled for every data type (`out_mask[i] == 0` = no data at or
  before `date_time`) and freed with `quiver_database_free_mask`. Every binding maps it to
  `nothing` / `null` / `None`. In Julia the result is now `Vector{Union{Nothing, T}}` for every
  column type, `T` from the attribute's type (`Int64`, `Float64` or `String`), including an empty
  result. It used to be `Vector{Int64}` / `Vector{Float64}` for numeric columns.

  *Adapt:* C callers pass `&out_mask` and free it with `quiver_database_free_mask`. Replace
  `isnan(x)` / `x == 0` no-data checks with a null check (`x === nothing`, `x == null`,
  `x is None`). Julia code typed on `Vector{Float64}` / `Vector{Int64}` must accept the `Union`
  element type (`something.(v, NaN)` gives back the old `Vector{Float64}` for a REAL column).

- **BREAKING — C API: one `quiver_database_query_*` function per type.** `quiver_database_query_string`,
  `quiver_database_query_integer` and `quiver_database_query_float` now take the parameter arrays
  (`param_types`, `param_values`, `param_count`) that the `quiver_database_query_*_params` forms
  took, and those three `_params` functions are gone, so each C++ `query_*` method maps to exactly
  one C function. A parameter the C API cannot convert now names the function called
  (`Cannot query_integer: unknown parameter type 999`) instead of `Cannot query: …`. The Julia,
  Dart, Python and JS query methods are unchanged.

  *Adapt:* in direct C API calls, pass `NULL, NULL, 0` after `sql` for a query without parameters,
  and drop the `_params` suffix from a parameterized call.

### Removed

- **BREAKING — `quiver_clear_last_error`, the C element accessors, and C++ `Element::has_scalars` /
  `has_arrays`.** `quiver_clear_last_error`, `quiver_element_has_scalars`,
  `quiver_element_has_arrays`, `quiver_element_scalar_count` and `quiver_element_array_count` are
  removed from the C API, along with the two C++ methods behind them. Nothing called them: no binding
  read an element back or cleared the error message. Julia's generated `Quiver.C` wrappers and the
  internal Dart and Python declarations for them are gone too. No binding's public API changes. The
  `quiver_get_last_error` header comment is corrected. It used to say the message is empty when no
  error occurred, but a successful call never reset it, so after a failure every later successful
  call still reported the old message.

  *Adapt:* in C, delete calls to `quiver_clear_last_error` and read `quiver_get_last_error` only
  after a call returns `QUIVER_ERROR`. To inspect an element, use `quiver_element_to_string`. In C++,
  replace `element.has_scalars()` / `element.has_arrays()` with `!element.scalars().empty()` /
  `!element.arrays().empty()`.

### Fixed

- **Julia and Python: `read_vector_group_by_id` / `read_set_group_by_id` read the group they are
  given.** Both built their rows from one per-column read per column, and a per-column read
  resolves the column *name*: when two groups of one kind share a column name (legal for a foreign
  key), the column came from whichever group's table sorts first, so the rows paired another
  group's values with this group's or raised `BoundsError` / `IndexError`. They now call the native
  C reader, as Dart does: one statement over the named group's own table, so the rows no longer mix
  separate snapshots either. A group with no such shared name reads back as before.
- **A vector or set group named after another group's column no longer hides that column.** The
  per-column readers (`read_{vector,set}_{integers,floats,strings}` and their `_by_id` forms, in
  every layer) took the group named after the column even when that group did not hold it, and
  threw `Cannot read_vector_floats_by_id: column 'cost' not found in table 'Child_vector_cost'`.
  They now fall through to the group that holds the column.
- **Julia: `create_element!` / `update_element!` take a nullable boolean read.**
  `read_vector_booleans` / `read_set_booleans` (and their `_by_id` forms) on a nullable column
  return `Vector{Union{Nothing, Bool}}` since 0.12.4, which no `Element` method accepted
  (`MethodError`). It now round-trips like the other nullable reads, and a real `nothing` cell
  raises the `ArgumentError` naming the column.
- **JS: an element array refuses a `null` cell in any position.** A string array decided by its
  first cell: `["a", null]` stored a SQL NULL while `[null, "a"]` threw, and `["a", undefined]`
  stored the text `"undefined"`. Every array now throws on a `null` or `undefined` cell, as in
  Python, Julia and Lua. *Adapt:* write NULL cells with `updateVectorGroup` / `updateSetGroup`.
- The element-array null-cell error in Julia, Python and JS also names `update_time_series_group`,
  which writes NULL cells too.
- **Julia, Dart, Python, JS: `update_time_series_files` with an empty map validates the
  collection.** The four bindings returned before calling the core when the map was empty, so
  `update_time_series_files("NoSuchCollection", {})`, or the same call on a collection with no
  `_time_series_files` table, succeeded silently where C++, the C API and Lua raised. The empty
  map now reaches the core in every binding and raises the core's error there too:
  `Cannot update_time_series_files: collection not found: <collection>` for an unknown
  collection, and the files-table-not-found error for a collection without one. On a collection
  that has the table it still changes nothing. A caller that made this call on a collection
  without a files table should check `has_time_series_files` first.
- **The C API group readers no longer truncate a REAL cell in an INTEGER column.**
  `quiver_database_read_vector_group_by_id`, `quiver_database_read_set_group_by_id` and
  `quiver_database_read_time_series_group` turned a stored `1.5` into `1` and reported it
  present, and an out-of-range REAL such as `1e300` was undefined behaviour. The cell is now
  absent (mask 0), the same as in the per-column integer readers, so the binding group readers
  built on these functions (Dart, and since this release Julia, Python and JS) return null for it.
  Only a non-STRICT table can hold such a value (e.g. written through raw SQL); STRICT schemas, as
  the conventions use, are unaffected.

## [0.12.4] — 2026-09-29

### Changed

- **BREAKING — a binary file's time coordinate names a calendar cell, and a week starts on the day of
  `initial_datetime`.** Each inner time value is its position inside the parent's period (day of
  month, year or week; hour of day, month, year or week), and the date `bin_to_csv` writes — and
  `csv_to_bin` checks — is the start of that cell. Two things change for callers:
  - When the finest time dimension is monthly or yearly and `initial_datetime` falls mid-period, rows
    are labelled from the period start: a monthly file from `2025-01-15` reads
    `2025-01-01, 2025-02-01, …` (it read `2025-01-15, 2025-02-15, …`; from `2025-01-31` it read
    `2025-01-31, 2025-03-03, 2025-03-31, 2025-05-01`). An hourly cell starts on the hour, so a start
    of `…T06:30:00` labels its first row `…T06:00:00`.
  - Under a weekly dimension, a week is seven days counted from the day of `initial_datetime`, not
    from January 1: a daily child's `initial_value` is always 1 (it was 4 for a file starting
    Saturday 2025-03-15) and an hourly child's is the hour of day + 1. Before this release such a
    file could hold only some of its cells: `write` (and so Julia `write!`, Lua `file:write` and the
    C API) accepted hours 1–24 of every week of a weekly × hourly file, and of a weekly × daily file
    only the weeks whose start on the old January-1 week grid fell on the 1st of a month;
    `csv_to_bin` and `Expression::save` stopped at the first cell they could not write. Unless
    `initial_datetime` is day 1, 8, 15, … of its year, every such stored cell now names a moment
    `(day of year − 1) mod 7` days later: rewrite those files from the source data.

  C++ only: `TimeProperties::datetime_to_int` is removed, and `TimeProperties::add_offset_from_int`
  now returns the start of the `value`-th period counted from the one holding its base, ignoring
  `initial_value`.

  *Adapt:* re-run `bin_to_csv` on such files before editing and re-importing their CSVs. C++ code
  that called `datetime_to_int` has no replacement: the coordinate is the position, and
  `BinaryFile::read`/`write` validate it.

- **BREAKING — `bin_to_csv` writes values at full precision, and `csv_to_bin` rejects a data cell
  that is not a whole number.** `bin_to_csv` wrote each value with 6 significant digits, so
  `1.23456789` became `1.23457` and a bin → csv → bin round trip silently changed the data. It now
  writes the shortest text that reads back to the same double, as `export_csv()` does, so round
  values may also change notation (`200000` → `2e+05`). `csv_to_bin` used `std::stod`, which reads
  the longest valid prefix, so a cell `9.99abc` was stored as `9.99` and a trailing space was
  ignored. The whole cell must now parse (leading whitespace is still skipped), and a bad cell
  reports `Cannot csv_to_bin: invalid float value '<v>' for label '<label>'` instead of the bare
  `stod` / `invalid stod argument` text. `null` still reads as a missing value.

  *Adapt:* regenerate golden files and byte-for-byte comparisons over `bin_to_csv` output; fix CSV
  files that relied on a truncated cell; update any matcher on the old `stod` messages.

- **Expressions: a binary operation accepts two single-label operands whatever their labels are
  called.** `+ - * /`, the comparisons and `&&`/`||` (Julia and Lua `&`/`|`) threw `Cannot apply:
  labels have same size 1 but different content` when each operand carried one label with a
  different name, while `ifelse` over the same operands worked. So
  `e:aggregate_agents("max") - e:aggregate_agents("min")` failed, and so did combining conditions
  on two single-label files, e.g. `(demand > x) & (price < y)`. Binary operations now follow the
  `ifelse` rule: operands with more than one label must carry the same label set, a single label
  broadcasts, and when every operand has a single label the result takes the left operand's (for
  `ifelse`, the `then` operand's). Every expression that built before builds the same output. A
  label-set mismatch in a binary operation now reports `Cannot apply: labels are incompatible
  across operands (non-singleton label sets must match)`, replacing `labels have same size N but
  different content` and `labels have incompatible sizes N vs M`.

- **BREAKING — expressions: one aggregation operation enum.** `quiver_expression_aggregate_agents`
  now takes `quiver_expression_aggregate_operation_t`, the same enum as `quiver_expression_aggregate`;
  `quiver_expression_aggregate_agents_operation_t` and its `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_*`
  constants are removed (their values were identical). In C++, `ExpressionAggregateAgents::Operation`
  is now an alias of `ExpressionAggregate::Operation`, so C++ code compiles unchanged and can pass
  either spelling to either method. Lua takes the operation as a string and is unaffected.

  *Adapt:* in C and Julia, replace `QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_<OP>` with
  `QUIVER_EXPRESSION_AGGREGATE_OPERATION_<OP>` — in Julia,
  `Quiver.aggregate_agents(e, Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN)`.

- **BREAKING — vector and set reads preserve NULL cells.** All twelve readers
  (`read_{vector,set}_{integers,floats,strings}` and their `_by_id` forms, plus the C API and
  binding equivalents) dropped SQL NULL cells, so `[0.10, NULL, 0.30]` read back as
  `[0.10, 0.30]` and two per-column reads of one nullable group paired the wrong values together.
  Cells are now positional: the inner element type is nullable (`std::optional<T>` in C++,
  `nothing`/`None`/`null` in the bindings, a `nil` hole in Lua) — in Julia only for a nullable
  column: a `NOT NULL` one keeps its concrete `Vector{Int64}`, while a nullable one now reads as
  `Vector{Union{Nothing, Int64}}` even when it holds no NULL. The C API numeric readers gained a
  per-cell presence mask — `uint8_t*** out_masks` on the four bulk readers (freed by the new
  `quiver_database_free_masks`) and `uint8_t** out_mask` on the four numeric `_by_id` readers
  (freed by `quiver_database_free_mask`); the string readers keep their signatures and mark a NULL
  with a `nullptr` entry, which they never returned before.

  *Adapt:* unwrap the inner values (`*v` / `v.value()`, `v === null` checks, `t[i] == nil` in Lua)
  and, in C, pass and free the new mask out-parameters and NULL-check every `char*` a string reader
  returns before using it. Inner lists that used to be short are now full length, so a length read
  as "number of non-null values" must count the non-null cells. To write a read back through
  `create_element` / `update_element`, mind the layer. Lua, Python and Julia element arrays refuse a
  NULL cell (Lua: an array with a `nil` hole, or any non-integer key such as a `table.pack`
  result's `n`, throws instead of being cut short or skipped; Python and Julia: an error naming the
  column), and so do JS numeric and boolean arrays (a `null` throws instead of being stored as 0 /
  `false`); write those NULL cells with `update_vector_group` / `update_set_group`. C++ (a
  `std::vector<Value>` holding `nullptr`), the C API (the `has_value` mask) and Dart (`List<T?>`)
  write a NULL cell as SQL NULL. Julia's `create_element!` / `update_element!` take a nullable
  read as it is when it holds no `nothing`. In C++, map `std::nullopt` to `nullptr` into a
  `std::vector<Value>`: `Element::set` has no `std::vector<std::optional<T>>` overload.

### Removed

- **BREAKING (C++ only) — `TimeProperties::set_initial_value()`.** `BinaryMetadata::derive_initial_values()`
  is now the one place a time dimension's `initial_value` is computed, and nothing else called the
  setter. No C API function or binding exposed it.

  *Adapt:* after changing a `BinaryMetadata`'s `dimensions` or `initial_datetime`, call
  `derive_initial_values()` instead of setting each value by hand.

- **BREAKING — the C API's incremental binary-metadata builders, and the C++
  `BinaryMetadata::add_dimension` / `add_time_dimension` behind them.** `quiver_binary_metadata_create`,
  `quiver_binary_metadata_set_initial_datetime`, `quiver_binary_metadata_set_unit`,
  `quiver_binary_metadata_set_version`, `quiver_binary_metadata_set_labels`,
  `quiver_binary_metadata_add_dimension` and `quiver_binary_metadata_add_time_dimension` are gone.
  No binding called them — Julia's `Metadata(; kwargs...)` and Lua's `quiver.metadata{...}` already
  build through `from_element` — and they were the one construction path that never derived a time
  dimension's `initial_value` from `initial_datetime`: it stayed 0, so a traversal of builder-made
  metadata started at coordinate 0. Julia and Lua code is unaffected; only the generated low-level
  `Quiver.C` wrappers for these seven symbols disappear.

  *Adapt:* build the metadata in one call — in C with `quiver_binary_metadata_from_toml` (a TOML
  string with `version`, `dimensions`, `dimension_sizes`, `time_dimensions`, `frequencies`,
  `initial_datetime`, `unit` and `labels`) or `quiver_binary_metadata_from_element` (an element
  carrying the same keys); in C++ with `BinaryMetadata::from_toml_content` or
  `BinaryMetadata::from_element`.

### Fixed

- **Binary files accept every cell of every time layout their metadata accepts.** `read` and `write`
  (and so `bin_to_csv`, `csv_to_bin` and `Expression::save`, in Julia and Lua too) rejected valid
  cells for five of the eight parent/child layouts, with `Invalid values for time dimensions:
  dimension 'hour' has value 25 but the resulting datetime implies 1`: hourly under monthly, yearly
  or weekly past the first day, daily under yearly past January, and daily under weekly in any week
  whose start on the old January-1 grid was not the 1st of a month. A non-midnight
  `initial_datetime` with an hourly dimension under a monthly or yearly one rejected the file's own
  first cell, and a start on the 29th-31st broke yearly + monthly layouts (yearly + monthly from
  January 31 rejected February; yearly + monthly + daily from 2024-02-29 rejected 2025-03-01).
- **Binary metadata with an invalid frequency layout reports why.** `from_toml_content` and
  `from_element` (so Julia `Metadata` and Lua `quiver.metadata`) computed initial values before
  validating, so frequencies `["monthly", "yearly"]` failed with `YEARLY frequency not implemented.
  This function should only be used for inner time dimensions.` and `["daily", "daily"]` with
  `Invalid parent frequency daily for DAILY dimension.`. They now report `Time dimension frequencies
  must be ordered from lowest to highest frequency.` and `Time dimension frequencies must be unique.
  Duplicate: daily`.
- **Expressions: aggregating away the outermost time dimension no longer shifts the result.** For
  `year × month` data starting 2025-03-01, `aggregate("year", ...)` kept the month's start at 3 in
  memory while the saved file re-read it as 1. The file came back shifted by two months, the
  January and February sums were never computed, and the in-memory result could not be combined
  with its own saved output (`incompatible TimeProperties`). The output now starts where the first
  reduced period starts: its `initial_datetime` becomes 2025-01-01 and output month *m* is calendar
  month *m*, in memory and on disk. The same holds for every frequency: a `day × hour` file from
  06:00 aggregated over `day` starts at 00:00. Reducing `year` over `year × month × day` data whose
  first year is a leap year still fails at 29 February, as it already did for a 1 January start.
  Affects C++, the C API, Julia and Lua.
- **Binary files with three or more time dimensions that start mid-period no longer skip cells.**
  For a `yearly × monthly × daily` file starting `2025-03-15`, the traversal behind an expression
  `save`, `bin_to_csv` and `csv_to_bin` resumed every later March at day 15, so 2026-03-01..14 were
  never visited. A saved expression left them NaN, `bin_to_csv` left their rows out, and
  `aggregate("day")` summed March 2026 from the 15th only. This affected C++, the C API, Julia and
  Lua. A time dimension now resumes at its starting value only while every enclosing time
  dimension is still in the starting period. Files with one or two time dimensions are
  unaffected. A CSV that `bin_to_csv` wrote for an affected file lacks those rows, so `csv_to_bin`
  now rejects it: convert the `.qvr` again.
- **Binary metadata factories reject mismatched or malformed fields instead of reading out of
  bounds.** `from_element` and `from_toml_content` (C API `quiver_binary_metadata_from_element` /
  `_from_toml`, Julia `Metadata(; ...)` / `from_element` / `from_toml_content`, Lua
  `quiver.metadata{}` / `quiver.metadata_from_toml` / `quiver.metadata_from_element`, and every
  `.toml` sidecar read by `open_file(path, 'r')`) indexed `dimension_sizes` and `frequencies`
  without checking their lengths. So `dimensions = {"a", "b"}, dimension_sizes = {3}`, or
  `time_dimensions` without `frequencies`, read past the end of an array: undefined behaviour,
  and an abort in a Debug build. They now throw `Cannot <op>: dimension_sizes count (1) does not
  match dimensions count (2)` or `Cannot <op>: frequencies count (0) does not match
  time_dimensions count (1)`. The same checks reject surplus entries (more sizes than dimensions,
  or frequencies beyond `time_dimensions`), which used to be ignored. A TOML array entry of the
  wrong type (`dimensions = ["a", 2]`) used to be dropped silently, which shifted every later
  dimension onto the wrong size. It is now `Cannot from_toml_content: array 'dimensions' must
  contain strings`, and a non-array value is `key '<k>' must be an array`. A missing or
  non-string `version`, `unit` or `initial_datetime` used to throw a bare `bad_optional_access`.
  It now names the key: `missing key 'unit'` or `key 'unit' must be a string`. The two
  time-dimension errors are now Pattern 1 and name the factory that was called (`Cannot
  from_element: time dimension 'x' is not in dimensions`). Before, they read `Error building
  metadata from toml: ...`, even from `from_element`.
- **`csv_to_bin` reads numbers the same way in every host locale and on every platform.** Under a
  decimal-comma C locale (e.g. Python's `locale.setlocale(locale.LC_ALL, "")` on a pt-BR machine,
  then `db:csv_to_bin` through a `LuaRunner`) a data cell `1.5` was read as `1`, and on Linux and
  macOS a subnormal value such as `1e-310`, which `bin_to_csv` writes, was rejected. It now uses
  the same number parser as `import_csv()`.
- **`csv_to_bin()` checks every data row's width against the header.** A row missing a dimension
  cell (`1` under the header `row,col,val1,val2`) was read past its end — an assertion abort in a
  debug build, a comparison against arbitrary memory in a release one. A data row must now have
  exactly as many fields as the header or `csv_to_bin` throws `Cannot csv_to_bin: line N has X
  fields, expected Y`, and a trailing comma counts as an extra field. That message also replaces
  the `Data length X does not match expected length Y` a short or long row used to raise from the
  binary writer, and it is a `std::runtime_error` like every other `csv_to_bin` failure, not a
  `std::invalid_argument`. A file that ends before its last row throws `Cannot csv_to_bin: file ends before
  line N`, and a header with too few columns now reports the same `Unexpected header in CSV file:
  ...` as any other header mismatch instead of `CSV header has N columns, expected M`.
- **Bulk vector and set reads keep an element whose id is -1.** The six bulk readers used -1 as
  their "no element yet" marker, so when -1 was a collection's smallest id (`create_element`
  accepts an explicit `id`) that element was left out — every later element then sat one slot off
  `read_element_ids` — or, when it had group rows, they were appended to an empty result
  (undefined behaviour; a Debug build aborts).

## [0.12.3] — 2026-09-28

### Changed

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
  an existing table, so an existing database needs a migration that rebuilds the table: create the
  new table, copy only the rows whose element still exists (`INSERT INTO <new> SELECT ... FROM
  <old> WHERE id IN (SELECT id FROM <Collection>)` — the rows the old behaviour orphaned would fail
  the new key), drop the old one, rename.

- **BREAKING — `list_vector_groups()`, `list_set_groups()` and `list_time_series_groups()` throw
  for an unknown collection.** They returned an empty list for a name that is not a table, so a
  mistyped collection looked the same as a collection with no groups, while
  `list_scalar_attributes()` on the same name threw. All four now raise
  `Cannot <operation>: collection not found: <name>`, in the C API, Lua and every binding. The
  `read_vectors_by_id` / `read_sets_by_id` composites (Julia, Dart, Python, JS, Lua) are built on
  them and now raise `Cannot list_vector_groups: …` / `Cannot list_set_groups: …` instead of
  returning an empty map. An existing collection with no groups still returns an empty list.

  *Adapt:* a caller that used an empty result to mean "no such collection" must catch the error
  instead.

### Fixed

- **A rejected `create_element()` / `update_element()` no longer leaves part of its write behind
  inside a transaction or dry run.** Both wrote the element's scalar row before routing and
  validating its arrays, and rewrote each group table before checking the next. Inside a
  caller-owned transaction (for example a Lua `pcall` inside `db:transaction`) or a dry run, a call
  rejected for an unknown array, a type mismatch or unequal lengths still left the new element, the
  updated scalars or an already-rewritten group in place for the commit. Every array is now routed,
  FK-resolved and validated before the first write, in every binding; an array whose column name
  several groups share is FK-resolved against each group it is written to, not only the first. A
  failure only SQLite can detect — a duplicate value in a set, a NULL in a NOT NULL group column, a
  CHECK constraint, a foreign-key violation — still happens mid-write, and inside a caller-owned
  transaction the call's earlier writes stay; outside one the call is rolled back as before.

## [0.12.2] — 2026-09-27

### Changed

- **BREAKING — `read_time_series_row()` rejects a group with more than one dimension column.** In a
  group keyed by `date_time` plus another dimension such as `block` (every primary-key column except
  `id` is a dimension), each date holds one row per block, so there is no single value per element.
  The read used to pick one of those rows by accident. It could return null although another block
  held a value at that date (block 1 `10.0` and block 2 `NULL` read back as null), and with several
  non-null blocks it returned whichever row came last. It now throws `Cannot read_time_series_row:
  group '<g>' of collection '<c>' has more than one dimension column` in every binding, even when
  the collection is empty. Single-dimension groups are unchanged.

  *Adapt:* read a multi-dimension group with `read_time_series_group` and choose the block yourself.

### Fixed

- **`update_time_series_group()` writes a value column that only a later row names.** The C++
  method (and `update_time_series_group_by_label()`) built its INSERT column list from the first
  row's keys, so a column that appeared only from the second row on passed validation and was
  then silently dropped, reading back as NULL. It now uses the union of every row's keys, as
  `update_vector_group()` / `update_set_group()` already do; a row that omits such a column writes
  NULL for it. The C API, Lua and the bindings always pass every column in every row and were not
  affected.

## [0.12.1] — 2026-09-27

### Changed

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

## [0.12.0] — 2026-09-27

### Changed

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
  A CSV that repeats a label is rejected before anything is written, and an import whose deletions
  would cascade into an element the CSV keeps (a cycle of `ON DELETE CASCADE` relations through
  another collection) is refused and rolled back.

  *Adapt:* keep every element you mean to keep in the CSV, since omitting one now also removes
  what depends on it through `ON DELETE CASCADE`; if an import is refused for cascading into a
  kept element, re-point that element's relation first. In a schema with a `UNIQUE` column other
  than `label` (a self-reference aside), an import that hands one of that column's values from an
  element it keeps to a row listed before it in the CSV (any swap does) now fails and rolls back;
  route it through a temporary value.

## [0.11.0] — 2026-09-26

### Changed

- **BREAKING — bundled SQLite 3.50.2 → 3.53.4, now built thread-safe.** Two SQLite changes reach
  callers through `query_*` / Lua SQL and user schemas. Inside SQL, a REAL converted to text now
  renders up to 17 significant digits instead of 15 — `CAST(1.1+2.2 AS TEXT)` was `3.3` and is now
  `3.3000000000000003`, and `||`, `printf('%s', …)`, `quote()` and `json_*` change the same way. A
  STRICT table's generated column whose value does not match its declared type now rejects the
  write (`cannot store REAL value in INTEGER column …`). Quiver's own typed reads, CSV export and
  `describe`/`summarize` are unaffected. The bump also brings the 3.50.3 AND-optimizer
  wrong-answer fix and the WAL-reset corruption fix. SQLite is now compiled with
  `SQLITE_THREADSAFE=1` (serialized) instead of `0`, so separate `Database` handles are safe to use
  from different threads.

  *Adapt:* compare converted floats numerically, or format explicitly with `format('%.15g', x)`;
  declare a generated column with the type its expression produces, or `CAST` inside the
  expression.
- **BREAKING — `import_csv()` parses with the same CSV reader as `db:read_csv`.** Quiver no longer
  links rapidcsv. Import used to pre-process each file as text before parsing, and that caused
  several bugs, now fixed:
  - A `sep=X` first line is used as the real delimiter, including after a UTF-8 BOM and for tab or
    `|`. Previously every `;` was rewritten to `,`, so a quoted `"x;y"` was stored as `x,y` and an
    unquoted `,` split its cell.
  - Without a `sep=` line, a file is read as semicolon-delimited when its *header line* holds `;`
    and no `,`.
  - Blank lines after a `sep=` line or the header are skipped (so a `\r\r\n` line ending, which a
    doubled Windows text-mode conversion writes, imports), and a lone CR ends a line.
  - Quoted multi-line cells are no longer cut by Excel's trailing-column cleanup.

  Import is stricter in three places. A numeric cell must parse whole: `1.5` and `12abc` are
  rejected for an INTEGER column, and `9.99abc` or `1,5` for a REAL column (they used to be
  truncated); in a group import these are now caught before anything is deleted. A quoted field
  with text after its closing quote, or never closed, is rejected before anything is deleted:
  `malformed quoted field on line N` / `unterminated quoted field on line N`. A single record over
  10 MB is rejected. Some errors are now reported differently:

  | Case | Before | Now |
  | --- | --- | --- |
  | Missing file | `could not open file: <p>` | `file not found: <p>` (also `path is a directory: <p>` and `cannot access file '<p>': …`) |
  | 0-byte file | `CSV file is empty.` | `file '<p>' is empty` |
  | Only a `sep=` line, or only a BOM | `CSV file is empty.` | `header row N not found in file '<p>'` |
  | Blank first line | `CSV file does not contain a 'label' column.` (collection) or a column-mismatch error (group) | `header row N not found in file '<p>'` |
  | Unreadable file (e.g. another process holds a lock on it) | `could not open file: <p>`, or `CSV file is empty.` | `cannot read file '<p>'` |
  | UTF-16/32 file | `CSV file does not contain a 'label' column.` (collection) or a column-mismatch error (group) | `cannot read file '<p>': …` |
  | Non-numeric or out-of-range REAL cell in a group import | the bare `std::stod` text (`invalid stod argument` on MSVC) | `Invalid float value '<v>' for column '<c>'.` |
  | Non-integer cell in a group INTEGER column with no enum labels | `Invalid enum value '<v>' for column '<c>'.` | `Invalid integer value '<v>' for column '<c>'.` |

  *Adapt:* update any matcher on the old messages; fix files that relied on truncated numbers or
  stray quotes.

- **BREAKING — `export_csv()` quotes a cell for `"` or CR, and no longer for a space.** Export now
  uses `db:write_csv`'s emitter: a cell is quoted if and only if it contains the separator, `"`, CR
  or LF. The old rule quoted a cell containing a space but not one containing a quote, so `"x"` was
  written raw and read back as `x`. A single-column row whose only cell is empty is written as `""`
  instead of a blank line. Parsed values are unchanged, or now correct.

  *Adapt:* regenerate golden files and any byte-for-byte comparisons over exported CSVs.

### Fixed

- **`import_csv()` reads numbers the same way in every host locale and on every platform.** Under a
  decimal-comma C locale (e.g. Python's `locale.setlocale(locale.LC_ALL, "")` on a pt-BR machine) a
  REAL cell `9.99` was read as `9`, and on Linux and macOS a subnormal value such as `1e-310`, which
  `export_csv()` writes, was rejected.
- **`import_csv()` imports a `vector_index` column of a set or time-series group as its declared
  type.** Only a vector group's `vector_index` is the structural index; elsewhere the name was
  forced to INTEGER, so a TEXT cell `12abc` was stored as `12` and `abc` failed with a bare `stoll`
  error.
- **`export_csv()` reports a failed write.** A full disk or a locked file used to leave an empty or
  cut-short CSV behind a successful return; it now throws `Failed to export_csv: could not write
  file: <p>`.
- **Julia: updating `Artifacts.toml` now invalidates the package precompile cache.** An artifact-only
  update could leave the cached native library hash pointing at the previous release. Julia now
  tracks `Artifacts.toml` as a precompile dependency and refreshes the hash when it changes.

## [0.10.9] — 2026-09-25

### Changed

- **BREAKING — vector and set bulk reads return one entry per element.** The six bulk readers
  (`read_vector_{integers,floats,strings}`, `read_set_{integers,floats,strings}`, and their C API
  and binding equivalents) skipped elements that had no group rows, re-indexing every entry after
  the gap so one element's values were read as another's. They now return one entry per element,
  positionally aligned with `read_element_ids()` and empty where an element has no rows. No
  signature changed in any layer; the outer length and the position of every entry did.

  *Adapt:* callers that zipped a bulk read against `read_element_ids()` were misaligned and are now
  correct; callers that read the outer length as "elements with data" must skip empty entries.

- **BREAKING — a set group's rows read back in one consistent order.**
  `read_set_{integers,floats,strings}_by_id` had no `ORDER BY`, so each took the order of whichever
  index SQLite chose for it — reading two columns of one set group could return their rows in
  different orders and pair the wrong values together. Every set reader now orders by `rowid`,
  matching `read_set_group_by_id`.

  *Adapt:* a set's rows are no longer sorted by value; they come back in the order they were
  written. Treat the order as unspecified but consistent across every reader of the group.

## [0.10.8] — 2026-09-20

### Added

- **`describe()` and `describe_collection()` now render an attribute's meaning, not just its
  declaration, when the database was opened with `from_migrations`.** Each scalar attribute line
  gains zero to three semicolon-delimited clauses read from a `ui/` TOML sidecar that sits beside
  the migrations directory: an English `label`, an `enum` code-to-label list, and — in
  `describe_collection()` only — a `tooltip`. Worked example:
  `- initial_volume_type (INTEGER) NOT NULL; label "Initial Volume Unit"; enum {0: "Per Unit", 2: "Volume"}`.
  A label or tooltip that merely restates the attribute name is suppressed, so only genuinely new
  information is added. A database with no `ui/` sidecar, or with a broken one (missing directory,
  empty file, invalid TOML, wrong-shaped entry), renders exactly as it did before this change and
  `from_migrations` never fails because of it — a warning is logged and the affected collection or
  vocabulary is simply left undescribed. The feature reaches every binding and Lua with no
  additional code on their side, since `describe`/`describe_collection` already return a plain
  string. Deliberately not included: no C API symbol, no structured getter, no validation of the
  sidecar against the schema, and English only — a database opened with `from_schema` is
  unaffected. `summarize_collection()`'s integer value distribution now carries the same enum
  labels: each observed code is annotated with its label, `values {0 "Per Unit": 2, 1: 1}`. A code
  the vocabulary does not cover stays bare, and a column with more than 64 distinct codes still
  renders no distribution clause at all.

## [0.10.7] — 2026-09-17

### Changed

- **The agent-facing Lua API reference now redirects a model to the file, instead of only telling
  it what it lacks.** `LUA_DB_API_REFERENCE`'s `Standard library` bullet used to state only that
  the Lua sandbox has no `io`, which correctly told a model it cannot open a file — and then led it
  to conclude it must paste the file's contents into the script as literals. The correction sits at
  that exact sentence: no `io`, but data files are read with `db:read_csv` / `db:read_csv_stream`.
  The `CSV file reading` section also gained one worked example covering both real, dirty Maranhão
  fixture shapes (a junk title row and units row around the header, apostrophe thousands
  separators, quoted commas, English month names), including the `tonumber`/`gsub` parenthesis
  trap: `gsub` returns two values, so `tonumber(v:gsub("'", ""))` silently passes the replacement
  count as `tonumber`'s base argument and returns `nil`; the fix is `tonumber((v:gsub(...)))`.

### Added

- **A Lua script can now read a CSV file off disk.** `db:read_csv(path, opts)` reads the whole
  file and returns `{ header = {...}, rows = {{...}, ...} }`, with every cell arriving as a string
  and no numeric or date inference; `db:read_csv_stream(path, on_row, opts)` reads the same file
  row by row through the same parser, so a large file can be processed with bounded memory. Both
  are sandboxed to the database directory like every other Lua file operation, and both take the
  same optional options table — `separator` (a single-character string, defaulting to `,`) and
  `header_row` (see below) are its two keys today. This is Lua-only, with no C++/C API/FFI
  counterpart.
- **`db:read_csv`/`db:read_csv_stream` accept a `header_row` option** naming which line is the
  header, 1-based, defaulting to `1`. `header_row = 0` declares the file has no header at all:
  `csv.header` is absent (`nil`) and `csv.rows[1]` is the file's first line — useful for a file
  with a junk title row and/or a units row around the real header. A `header_row` past the end of
  the file throws, as does a value that isn't a non-negative integer.
- **A Lua script can now write a CSV file to disk.** `db:write_csv(path, opts)` returns a handle;
  `w:write_row(row)` appends one row and `w:close()` finishes it — streaming-only, with no
  whole-file form. The same two options as the reader, `separator` and `header`, are all it takes.
  Opening `db:write_csv` truncates an existing file at the target path (no overwrite guard). The
  writer is hand-rolled RFC-4180 emission over `std::ofstream`, with no new dependency; numbers are
  formatted via `std::to_chars`'s shortest round-trip form, and a `nil` cell and an empty-string
  cell are indistinguishable after the round trip since CSV has no null. With a `header`, its
  length is the row width: a shorter `write_row` pads with empty cells and a longer one throws,
  naming the row's ordinal and both counts; omitting `header` disables the check. A writer still
  open when the script's `run()` call returns is flushed automatically, so the file is complete
  and re-readable even without an explicit `w:close()`.

### Fixed

- **Lua: a CSV writer held in a global was never flushed, leaving a 0-byte file.** The promise
  that a writer the script never closed is still complete when `run()` returns was implemented as
  a forced garbage collection, which only finalizes objects the script made *unreachable*.
  `w = db:write_csv(path)` without `local` — Lua's default spelling — is a GC root, so its rows
  stayed in the stream buffer and the file was empty (or truncated mid-record) for the host and
  for any later `run()`. `LuaRunner::run` now closes every writer the run handed out, explicitly
  and regardless of reachability. A writer does not outlive its `run()`: reusing the handle from a
  later script reports `Cannot write_row: writer for '...' is already closed`.
- **Lua: `w:close()` left the writer un-closeable after a flush failure.** It threw before marking
  the writer closed and before releasing the handle, so every later `close()` raised the same
  error instead of the documented no-op, and `w:write_row` then reported "failed to write" rather
  than "already closed".
- **BREAKING — Lua: `separator` no longer accepts a quote, CR, LF or NUL** in `db:read_csv`,
  `db:read_csv_stream` or `db:write_csv`. They are one byte but cannot be delimiters, and
  `db:write_csv(path, { separator = '"' })` silently produced a file `db:read_csv` refused to
  open. They are now rejected up front:
  `Cannot <op>: option 'separator' must not be a quote, carriage return, newline or NUL`. Callers
  passing one of those four bytes must pick a real delimiter.
- **Lua: a sparse row or `header` key allocated without bound.** `w:write_row({[1e9] = "x"})` and
  `db:write_csv(p, { header = {[1e9] = "x"} })` build a dense vector up to the largest integer
  key, so a single stray key asked for tens of gigabytes and surfaced as a raw `bad allocation`
  with no `Cannot ...:` prefix. A key past 1,000,000 is now a precondition failure naming it.
- **Lua: a non-string key in a CSV options table surfaced as a raw Lua value.**
  `db:read_csv(p, { [true] = 1 })` (and the `db:write_csv` equivalent) converted the key
  unchecked, so the script received a bare `true`/table as the error in Release and a sol2 panic
  in Debug. Now `Cannot <op>: option key must be a string`.
- **BREAKING — Lua: two `db:write_csv` writers open on the same path at once are now refused**
  (`Cannot write_csv: file is already open for writing: <path>`). Each opened with truncation and
  wrote from offset 0, so the second silently discarded everything the first had buffered — only
  the second writer's rows survived, with no error. Close the first writer before reopening its
  path; reopening a *closed* path still truncates, unchanged.
- **Lua: a non-function `on_row` reached `db:read_csv_stream`'s caller as a raw sol2 message.**
  `db:read_csv_stream(p, "oops")` reported `stack index 3, expected function, received string`
  (and, for some argument types, escaped `pcall` entirely). Now
  `Cannot read_csv_stream: on_row must be a function`.
- **Lua: `w:write_row(<userdata>)` wrote a spurious empty record.** sol2's table check for the row
  parameter also admits userdata, so `w:write_row(db)` appended `""` instead of throwing; the
  argument's type is now checked (`Cannot write_row: row must be a table`), which also replaces
  sol2's raw "stack index 2, expected table" for a string/number/nil argument.
- **Lua: a `header` table with a bad key blamed the value.** `{ header = { name = "a" } }` reported
  `option 'header' entry must be a string` although every entry was one; a bad key now reports
  `option 'header' key must be a positive integer`.
- **Lua: a csv-parser failure raised while fetching the first data row reached scripts unwrapped.**
  `for_each_row` wrapped `++it` but not the initial `begin()`, which parses too.
- **Lua: a path the OS refuses to resolve reached scripts as a raw `std::filesystem` message.**
  Every file-touching Lua operation — `db:read_csv`, `db:read_csv_stream`, `db:write_csv`,
  `db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`, `db:export_csv`, `db:import_csv`,
  `db:validate_migrations`
  and `expr:save` — resolves its path through one shared gate, and that gate used throwing
  `std::filesystem` overloads without catching them. Any OS failure that is not a plain "does not
  exist" therefore surfaced unprefixed: on Windows, `db:read_csv("NUL")` (or any reserved device
  name, in any case, in any directory) raised
  `weakly_canonical: The parameter is incorrect.: "..."` instead of a `Cannot read_csv: ...`
  message, breaking the guarantee that no standard-library text reaches a script unwrapped. Such
  a failure is now reported as `Cannot <operation>: cannot resolve path '<path>': <reason>`. The
  three CSV precondition checks were hardened the same way and now report
  `Cannot <operation>: cannot access file '<path>': <reason>` when the OS refuses the query,
  keeping the existing not-found / is-a-directory / is-empty messages unchanged.

## [0.10.6] — 2026-09-11

### Fixed

- **Dart: every DateTime reader threw on valid values whose local wall-clock time the platform
  considers nonexistent.** `stringToDateTime` validated by re-serializing a *local*
  `DateTime.parse` and comparing it with the input, so a value inside a DST gap — on Windows the
  historical Brazilian rules put one at midnight of 2019-01-01 — came back shifted by an hour and
  was rejected as `Cannot convert "2019-01-01T00:00:00" to a date time in
  'Consumption.date_time': expected a valid YYYY-MM-DD[THH:MM:SS]`, taking down
  `readTimeSeriesGroup`, `readScalarDateTimes`, `queryDateTime` and the rest with it. The
  fields are now range-checked in UTC (which has no gaps) and the local `DateTime` built from
  them; the accepted grammar is unchanged and now pinned by `test/date_time_test.dart`. A value
  inside a real DST gap still reads an hour later, since that local time does not exist — but it
  reads.

## [0.10.5] — 2026-09-09

### Changed

- **The Dart binding's native build now works on macOS.** `quiverdb`'s native-assets hook
  previously could not configure, compile, or register its libraries there.
- **macOS builds now target macOS 13.3 as their minimum, deterministically.** libc++ marks the
  floating-point `std::to_chars` (used by `database_csv_export.cpp` and `lua_runner.cpp`)
  unavailable below 13.3, so that is the core's real floor and `cmake/Platform.cmake` now sets
  it for every macOS build. Previously no build path set one, so clang stamped the *builder's*
  OS version into the shipped dylibs and the published Julia/JS/S3 natives silently required
  whatever macOS the CI runner image was — usually much newer than 13.3. A higher explicit
  `CMAKE_OSX_DEPLOYMENT_TARGET` is respected; a lower one is raised to 13.3, which is what the
  code actually requires.
- **New CMake option `QUIVER_UNVERSIONED_SHARED` (default OFF).** Turning it on builds the
  shared libraries as plain `libquiver.dylib` / `libquiver.so` real files instead of a versioned
  real file plus unversioned symlinks. Only the Dart hook sets it — the published Julia, JS and
  Python natives keep their versioned install names, so nothing else changes.

## [0.10.4] — 2026-09-04

### Added

- **Booleans are accepted on every write path, in every layer.** A native boolean now maps to
  INTEGER 1/0 wherever an integer is accepted — element scalars and arrays on
  `create_element`/`update_element`, query parameters, the vector/set/time-series group writers,
  and `upsert_time_series_row` — so it also reaches a REAL column through the existing
  int-for-REAL coercion. Previously **Lua rejected a boolean everywhere**
  (`Cannot table_to_element: attribute 'flag' has unsupported Lua type`, and its siblings for
  arrays, query parameters, group cells, and row upserts), forcing scripts to write
  `flag = true and 1 or 0`; and Dart and JavaScript rejected one in the **group and row writers**
  while accepting it on the element and query paths. Julia and Python already accepted booleans
  throughout (`Bool <: Integer`; `bool` is an `int` subclass) and gain test coverage that pins it.
  `db:update_relation` still refuses a boolean by design — only `nil` may clear a relation.

  Booleans are **not** readable as booleans from Lua: a stored flag comes back as `0`/`1` from
  `read_scalar_integers`, and the boolean readers remain a Julia/Dart/Python/JS convenience. Note
  `nil ~= 0` is `true` in Lua, so compare a possibly-NULL flag with `== 1`, not `~= 0`.

### Fixed

- **JavaScript: `upsertTimeSeriesRow` wrote a boolean as FLOAT into an INTEGER column.**
  `Number.isInteger(true)` is `false`, so a boolean fell through to the float branch and was
  coerced to `1.0` with no error — the core then rejected the row for a type mismatch, or a REAL
  column silently received a float where an integer was intended. Booleans now take the INTEGER
  branch. Only callers passing booleans to that method were affected.
- **Dart: the group writers' unsupported-type error named neither the column nor the type's
  context.** `_marshalGroupColumn` threw `Unsupported value type: <T>`; it now reports
  `Unsupported value type <T> for column '<name>'`, per the rule that a pre-FFI marshalling error
  names the offending column. Affects every unsupported type, not just booleans.
- **Lua: a mixed integer/boolean array silently stored 0 in release builds.** `{1, true}`
  dispatched on the first cell as an integer and then read later cells through an unchecked sol2
  getter, which throws only when `SOL_SAFE_GETTER` is enabled (debug) and yielded `0` in release.
  Cells are now coerced individually.

## [0.10.3] — 2026-09-03

### Changed

- **BREAKING — a `DATE_TIME` value is validated when it is written.** A string bound to a
  `date_`-prefixed column must be ISO 8601: `YYYY-MM-DD`, optionally followed by `THH:MM:SS` or
  ` HH:MM:SS`. Anything else now throws `Cannot <operation>: invalid DATE_TIME value for column
  '<c>': '<value>' (expected YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS)`. The value is still stored
  verbatim — the core validates, it never normalizes.

  Previously the only requirement was *being a string*, so `date_initial = "2005-01"` was accepted
  and then failed on the read, deep inside a binding's date parser, with an error naming neither
  the write nor the column. Because the composite readers (`read_scalars_by_id`,
  `read_element_by_id`, `read_vector_group_by_id`, …) all funnel through that parser, one bad cell
  made the whole element unreadable.

  Every field is fixed-width and zero-padded, the year is `0001`-`9999`, and the calendar day must
  exist. Rejected: `"2005"`, `"2005-01"`, `"not-a-date"`, `""`, an impossible calendar day
  (`"2024-02-31"`), trailing garbage (`"2024-01-15junk"`), an unpadded or short field
  (`"2024-1-5"`, `"24-01-15"`, `"2024-01-15T1:30:00"`), a truncated time (`"2024-01-15T10:30"`),
  and a leap second (`"2024-01-15T10:30:60"`). Leading and trailing whitespace is trimmed before
  validating, matching what actually gets stored. The accepted band is the intersection of what
  Python's `fromisoformat`, Julia's `DateTime` and Dart's `DateTime.parse` handle, so a value the
  core stores is a value every binding can read. Applies to `create_element`, `update_element`,
  the vector/set/time-series group writers, `upsert_time_series_row`, their `_by_label` forms,
  `import_csv`, and every binding and Lua.

  *Adapt:* write a full calendar date. `"2005-01"` becomes `"2005-01-01"`. To put a
  non-conforming value in a date column deliberately — reproducing legacy data in a test, say —
  use raw SQL through `query_string`/`query_integer`, and read it back as a string: the native
  DateTime readers now hold the same grammar (see the next entry).

- **BREAKING — the native DateTime readers hold the same grammar as the write gate.**
  `read_scalar_date_times` / `read_vector_date_times` / `read_set_date_times`, their `_by_id`
  forms, `query_date_time`, and the DATE_TIME cells of the group readers now reject any value
  outside `YYYY-MM-DD[THH:MM:SS | ' HH:MM:SS']` (fixed-width, year `0001`-`9999`, a real calendar
  day) with a message naming the offending column: `Cannot convert "<value>" to a date time in
  '<collection>.<attribute>': expected a valid YYYY-MM-DD[THH:MM:SS]`. `ArgumentError` in Julia and
  Dart, `ValueError` in Python — the same exception types the boolean wrappers raise.

  Each host parser was wider than the core's grammar in a *different* direction, so the same stored
  bytes read back three different ways. Julia's `dateformat` treats field widths as maxima and fills
  missing trailing components, so it silently fabricated dates: `"2024"` and `"2024-01"` both read
  as 2024-01-01, and `"20240115"` as **year 20240115**. Python's `_parse_datetime` stamped
  `tzinfo=utc` onto an already-offset value instead of converting it, so
  `"2024-01-15T10:30:00+03:00"` came back as `10:30Z` — three hours wrong, no error. Dart's
  `DateTime.parse` rolled an out-of-range field over rather than rejecting it (`"2024-02-31"` read
  as March 2), and returned `isUtc = true` for `Z`/offset forms, so one list could mix flags — and
  Dart's `==` compares `isUtc` as well as the instant, making same-moment values compare unequal
  and dedupe to two in a `Set` while `compareTo` read 0 and hid it.

  This is only reachable through a column the write gate does not cover: it fires on
  `date_`-prefixed columns, so a plain `TEXT` column was the way in. `read_scalar_date_times` on a
  `TEXT` column holding `"20240115"` returned year 20240115 in Julia and 2024-01-15 in
  Python/Dart, neither of them erroring.

  *Adapt:* nothing, if your dates go through a `date_`-prefixed column — the write gate already
  guaranteed conforming values there. If you point a DateTime reader at a plain `TEXT` column
  holding something else, read it with the string reader (`read_scalar_strings` and friends) and
  parse it yourself.

- **BREAKING — `import_csv` rejects a timestamp it cannot canonicalize.** Import writes through a
  raw `INSERT` and never reaches the validator above, and with a `date_time_format` set it parsed
  the cell with the caller's format — which range-checks month and day separately and so cannot see
  that February has no 31st. `date_time_format = "%d/%m/%Y"` on a cell `31/02/2024` stored
  `"2024-02-31T00:00:00"`, a value `create_element` refuses and no binding's date parser can read.
  Import now validates the canonical string it produces, so it is held to the same grammar as every
  other writer.

  *Adapt:* fix the offending cell. An import that used to "succeed" on such a row was writing data
  you could not read back.

- **`parse_iso8601` accepts a date-only value, and now requires the whole string.** The core's one
  ISO 8601 parser (`src/utils/datetime.h`) previously demanded the time part, which had two
  consequences beyond the validation above:

  - **`import_csv` now accepts a date-only cell** when no `date_time_format` is given, storing it
    canonicalized as `<date>T00:00:00`. This fixes a round-trip bug: `export_csv` writes the stored
    value verbatim, so a database holding `"2024-01-01"` exported a file its own importer rejected
    with `Cannot import_csv: Timestamp 2024-01-01 is not valid`.
  - **`export_csv` with `date_time_format` set now formats a date-only value** instead of passing
    it through raw, and `BinaryMetadata`'s `initial_datetime` accepts a date-only value (read back
    as midnight UTC, re-serialized in full `T` form).

  The whole-string requirement is a tightening: `"2024-01-15T10:30:00.123"`, `"…Z"` and any other
  trailing text used to parse (the parser stopped as soon as the format was satisfied) and are now
  rejected everywhere `parse_iso8601` is used — which includes two *ingest* paths with no lenient
  fallback. `import_csv` with no `date_time_format` now fails on a cell carrying a `Z` or
  fractional seconds (the shape most external exporters write), and `BinaryMetadata`'s
  `initial_datetime` now fails on a `.toml` sidecar carrying one, making the `.qvr` unopenable.
  `export_csv` with `date_time_format` passes such a legacy cell through raw instead of formatting
  it.

  *Adapt:* rewrite the offending cell to `YYYY-MM-DDTHH:MM:SS`, or pass a matching
  `date_time_format` to `import_csv`.

  The parser is now a hand-rolled fixed-width scan rather than `std::get_time`. get_time's field
  widths are maxima, so it also accepted `"2024-1-5"`, `"24-01-15"` and `"+2024-01-15"`, and on
  MSVC it did not fail on a truncated time — so `"2024-01-15T10:30"` validated on Windows and would
  not on Linux. It also left `tm_wday`/`tm_yday` unset, so `export_csv` with a `date_time_format`
  containing `%a`/`%A`/`%j`/`%U`/`%W` reported every date as a Sunday on day 001; those now
  format correctly.

### Added

- **Bulk DateTime convenience readers.** Julia, Python, and Dart now expose native-DateTime
  readers for scalar, vector, and set attributes (`read_scalar_date_times` /
  `readScalarDateTimes`, and their vector/set counterparts). They compose the existing string
  readers and parsers. Scalar reads preserve SQL NULLs positionally; vector and set reads retain
  the existing group-reader behavior of omitting NULL cells and elements that own no rows.
  JavaScript remains deliberately string-based, and the core, C API, and Lua surfaces are
  unchanged.

- **Julia scoped resource factories.** `open`, `from_schema`, `from_migrations`, and
  `Binary.open_file` take a callback-first argument, so Julia `do` syntax releases the handle at the
  block's `end` on both the normal and the exceptional exit, and returns the callback's result. The
  finalizer already released eventually — what is new is *prompt, deterministic* release, which is
  what frees an OS file handle on Windows. Caveat: a `LuaRunner` built inside the block must not
  outlive it (it borrows the database), and an uncommitted transaction still open at the block's
  `end` is rolled back — use `transaction(db) do db ... end` inside.

- **Boolean convenience readers for INTEGER-backed values.** Julia, Python, Dart, and JavaScript
  now expose scalar, vector, and set boolean readers in both bulk and by-id forms, plus a boolean
  query helper. They compose the existing integer APIs and convert only `0`/`1` to
  `false`/`true`; any other integer raises the binding's native conversion error
  (`ArgumentError` in Julia and Dart, `ValueError` in Python, `RangeError` in JavaScript), naming
  the offending `collection.attribute`. The scalar readers preserve NULLs positionally, one entry
  per element; the vector and set readers do not — like every other group reader they drop NULL
  cells and omit elements that own no rows, so they are not aligned with `read_element_ids`.
  Lua is deliberately excluded (it has a native boolean; see the design decisions).

- **Dart and JavaScript accept a `bool` wherever an integer is accepted.** `createElement` /
  `updateElement` (scalars and arrays) and query parameters now take a boolean and store it as
  INTEGER `1`/`0`, matching Julia and Python. Previously they threw `Unsupported type bool`, so a
  value read through the new boolean readers could not be written back. JavaScript's
  `ScalarValue`, `ArrayValue` and `QueryParam` were widened accordingly.

- **`update_relation(collection_from, collection_to, relation_type, id, target_label)` and
  `update_relation_by_label(..., label, target_label)`.** Points one element's scalar foreign-key
  relation at another element, named by the target's `label`; no target label clears it. The
  column is derived by the naming convention — `lowercase(collection_to) + "_" + relation_type`,
  so `update_relation("Child", "Parent", "id", child, "Parent A")` writes `Child.parent_id` — and
  must be a foreign key to `collection_to`, otherwise Pattern 1 `Cannot update_relation: ...`. The
  write delegates to `update_element`, so the target-label resolution and the missing-source-id
  check are that method's. A relation that lives in a group needs the matching group writer
  instead.

  Available in **every layer**: C++, the C API, Julia (`update_relation!`), Dart
  (`updateRelation`), Python (`update_relation`), JS (`updateRelation`), and Lua
  (`db:update_relation`), each with its `_by_label` form. `target_label` is a required parameter
  that accepts the language's null (`nothing`/`null`/`None`) to clear the relation; in Lua a `nil`
  or omitted argument clears it.

- **Migration round-trip validation: `Database::validate_migrations()` / `quiver_database_validate_migrations()`.** Validates a migrations directory in an in-memory database by applying every up migration, then every down migration, and finally checking that no table survives.

  Available in **every layer**: C++, the C API, Julia (`validate_migrations`), Dart
  (`Database.validateMigrations`), Python (`Database.validate_migrations`), JS (`Database.validateMigrations`),
  and Lua (`db:validate_migrations`) — the Lua binding is db-scoped and sandboxed to the database
  directory, like the other file-touching Lua operations.

  A directory with no numbered migration subdirectories throws `Cannot validate_migrations: no
  migrations found in <path>` rather than passing vacuously. A `down.sql` that runs but forgets a
  `DROP` throws `Failed to validate_migrations: down migrations left tables behind: <names>`.

## [0.10.2] — 2026-08-27

### Added

- **`upsert_time_series_row_by_label(collection, group, label, row)`.** Inserts or replaces a
  single time-series row addressed by `label` instead of id — the label-addressed counterpart of
  `upsert_time_series_row`. It resolves the label and then delegates, so the dimension-column
  rules, the type validation, and the upsert-on-PK semantics are identical. Available in every layer,
  under the usual per-layer spelling. Label resolution and its miss semantics are
  `update_element_by_label`'s, below.

- **`update_time_series_group_by_label(collection, group, label, rows)`.** Replaces all of an
  element's rows in one named time-series group, addressed by `label` instead of id — the
  label-addressed counterpart of `update_time_series_group`. It resolves the label and then
  delegates, so the dimension-column rules, the type validation, the NULL cells, and "no columns
  clears the group" are identical. Available in every layer, under the usual per-layer spelling.
  Label resolution and its miss semantics are `update_element_by_label`'s, below.

- **`update_vector_group_by_label` / `update_set_group_by_label(collection, group, label, rows)`.**
  Replaces all of an element's rows in one named vector or set group, addressed by `label` instead
  of id — the label-addressed counterpart of `update_vector_group` / `update_set_group`. Each
  resolves the label and then delegates, so the column validation, the FK-label resolution, the
  NULL cells, and "no columns clears the group" are identical. Available in every layer, under the
  usual per-layer spelling. Label resolution and its miss semantics are
  `update_element_by_label`'s, below.

- **`update_element_by_label(collection, label, element)`.** Updates an element addressed by its
  `label` instead of its id, the label-addressed counterpart of `update_element`. It resolves the
  label and then delegates to `update_element`, so the attributes written, the FK-label
  resolution, and the group-replacement semantics are identical. Available in every layer, under
  the usual per-layer spelling.

  Label resolution and its miss semantics are `delete_element_by_label`'s, below — with the
  messages naming `update_element_by_label`. Passing `label` among the attributes **renames** the
  element, after which only the new label resolves. Because the label form delegates, failures
  that validate the *element* (an empty element, a type mismatch) report `Cannot update_element:
  ...` — the operation that actually validated. Python's `collection` and `label` are
  positional-only (`/`) so that `label=` in `**kwargs` renames rather than colliding with the
  parameter.

- **`delete_element_by_label(collection, label)`.** Deletes an element addressed by its `label`
  instead of its id, for callers that already know the name and would otherwise round-trip through
  a query to find it. It resolves the label and then delegates to `delete_element`, so `ON DELETE
  CASCADE` cleanup is identical. Available in every layer, under the usual per-layer spelling.

  A label is unique **per collection, not per database**: one naming an element of a different
  collection does not resolve. A miss throws `Element not found: label '<label>' in collection
  '<c>'` and deletes nothing (no silent no-op, matching `delete_element` / `update_element`).
  Naming a table with no `label` column throws `Cannot delete_element_by_label: column 'label' not
  found in table '<t>'`.

### Fixed

- `quiver_database_upsert_time_series_row` now writes SQL NULL for a NULL `column_data[c]` or a
  NULL `char*` cell instead of dereferencing it — it shares the group writers' decoder. Reachable
  only from direct C API callers (every binding rejects a null cell before the FFI call).

## [0.10.1] — 2026-08-14

No library changes — release tooling only: the **Bump Version** workflow
(`.github/workflows/bump-version.yml`) plus `scripts/assert_version.py bump major|minor|patch`,
and the PyPI publish action pinned to `pypa/gh-action-pypi-publish@v1.14.2`. Published artifacts
are functionally identical to 0.10.0.

## [0.10.0] — 2026-08-14

### Added

- **`number_of_elements(collection)`.** Returns the current number of rows in a
  collection's main table with `COUNT(*)`, without materializing and transferring every element
  ID. An empty collection returns `0`; deleting any element decreases the count regardless of ID
  gaps. The C API symbol is `quiver_database_number_of_elements` and writes an `int64_t` scalar to
  caller-owned storage.

  Available in **every layer**: C++, the C API, Julia (`number_of_elements`), Dart
  (`numberOfElements`), Python (`number_of_elements`), JS (`numberOfElements`), and Lua
  (`db:number_of_elements`). Every binding calls the scalar C entry point directly, so the count
  never travels as an array of ids.

- **Whole-group writers: `update_vector_group()` / `update_set_group()`.** Replace all of an
  element's rows in one *named* group; passing no columns clears the group. These are the write
  counterpart of `read_vector_group_by_id()` / `read_set_group_by_id()`, and the unambiguous
  alternative to passing arrays through `create_element()` / `update_element()` — those route an
  array by *column name*, which names more than one table when two groups of a collection share a
  column (legal: the schema validator exempts foreign-key columns), whereas `(collection, group)`
  names exactly one.

  Available in **every layer**: C++, the C API (columnar arrays plus a per-cell NULL mask, same
  shape as `quiver_database_update_time_series_group`), Julia (`update_vector_group!` /
  `update_set_group!`), Dart (`updateVectorGroup` / `updateSetGroup`), Python
  (`update_vector_group` / `update_set_group`), JS (`updateVectorGroup` / `updateSetGroup`), and
  Lua (`db:update_vector_group` / `db:update_set_group`).

  Passing no columns clears the group; naming a column whose value list is empty is an **error**,
  so a typo'd column name cannot silently wipe a group. `id` and `vector_index` are rejected —
  they are derived from the element and the row's position. A missing element id throws
  `Element not found: <id> in collection '<c>'`, like `update_element` / `delete_element`. Foreign-key
  columns accept a label string. In Lua the row count is the largest index any column reaches, so
  short or sparse columns write NULL in the gaps and a read's `nil` holes round-trip.

- **Dart: `quiver_log_level_t` is exported** from `quiverdb.dart`. The `consoleLevel` values the
  factory constructors document were previously not reachable from outside the package.

### Changed

- **BREAKING — `export_csv()` writes foreign keys as labels, not ids.** A foreign-key column is
  now exported as the referenced element's `label`, **including self-references** — `import_csv()`
  does not skip those either, it defers them to a second pass and looks them up by label there too.
  Export previously wrote the raw integer id while import resolved by label, so **any table with a
  relation could not round-trip** — import rejected its own exporter's output with
  `Cannot import_csv: Could not find an existing element from collection <target> with label <id>`.

  *Adapt:* re-export any stored CSVs, and update tooling that parsed exported foreign-key ids to
  read labels instead.

- **BREAKING — `export_csv()` writes floats at full precision.** Floats now use the shortest
  representation that round-trips exactly (`std::to_chars`) instead of `%g`, which silently
  truncated to 6 significant digits: `1234567.89` exported as `1.23457e+06` and an
  export → edit → import cycle lost precision.

  *Adapt:* regenerate golden files and any byte-for-byte comparisons over exported CSVs.

- **BREAKING — `list_time_series_files_columns()` returns declaration order.** Columns come back
  in schema declaration order instead of alphabetical, matching every other list/metadata call in
  the library. `read_time_series_files()` is **unaffected** — it returns a key-sorted map, so its
  observable order never depended on this.

  *Adapt:* only positional consumers of the returned list are affected; lookups by name are not.

- **Every float read widens INTEGER values.** The int64-for-REAL typing policy now lives in
  `Row::get_float`, the single extractor behind `query_float()`, `read_scalar_floats()`,
  `read_scalar_float_by_id()`, `read_vector_floats_by_id()` and `read_set_floats_by_id()`, so all of
  them return a double where they previously reported "no value": `SELECT COUNT(*)` and
  `SUM(int_col)` (SQLite answers those as INTEGER), and an integer stored in a REAL column (SQLite
  keeps it INTEGER). Not marked breaking: it only turns an absent value into a present one, so
  existing null handling still compiles and simply stops firing. `query_integer()` still does not
  narrow a REAL; that direction is lossy.

- **`create_element()` / `update_element()` warn on ambiguous array routing.** When an array's
  column name matches more than one group table in the collection, the array is still written to
  all of them (unchanged behaviour), but the operation now logs a warning naming the tables. Use
  `update_vector_group()` / `update_set_group()` to target a single group.

- **Dart: `hooks` dependency widened** from `^2.1.0` to `>=2.0.2 <3.0.0`. `^2.1.0` requires
  `meta ^1.19.0`, which cannot resolve against the `meta` version the Flutter SDK pins — the
  binding was not consumable from a Flutter app. This one *removes* a restriction rather than
  adding one.

### Fixed

- **`open()` on an existing database now works.** `Database(path)` / `quiver_database_open` — and
  therefore `open()` in all five bindings — never read the schema, so an opened database answered
  every metadata and CRUD call with `Cannot <op>: no schema loaded`. Schema metadata is now loaded
  on first use. `from_migrations()` against a migrations directory with no versioned subdirectories
  is covered by the same change (and still returns a usable handle for an empty database, rather
  than throwing a validation error at open). A database that is not a quiver database now reports
  the validator's actual reason instead of "no schema loaded".

- **Out-of-bounds read writing a group with two value columns.** The core's column map is
  name-ordered, so an empty *alphabetically first* column left the row count at 0 for a later
  column to overwrite, skipping the same-length check and then indexing the empty vector — a heap
  read past the end, bound straight into SQLite. Reachable through `create_element()` /
  `update_element()` on any group with more than one value column. The length mismatch is now
  always reported.

- **A failed group write can no longer leave the group cleared.** Type validation ran *after* the
  DELETE, and the internal transaction guard is a no-op when a transaction is already open (inside
  `begin_dry_run()` or a caller-owned transaction), so a rejected write silently emptied the group.
  Validation now precedes the DELETE.

- **`update_vector_group()` / `update_set_group()` validate what the caller actually passed.**
  A column present only in a later row is now written instead of dropped (and an unknown one in a
  later row is rejected instead of ignored); `id` and `vector_index` are rejected instead of being
  duplicated in the INSERT, where SQLite keeps the first occurrence and discarded the caller's
  value; a nonexistent element id throws `Element not found` instead of succeeding silently (clear)
  or surfacing a raw `FOREIGN KEY constraint failed` (write); and the not-found message is
  `Vector group not found: ...` / `Set group not found: ...`, matching what `get_vector_metadata()`
  reports for the same condition.

- **A named column with no rows no longer clears a group.** Through the C API (and therefore
  Julia/Python/Dart/JS) `{"typo": []}` reached the core as an empty update and wiped the group while
  reporting success — for `update_time_series_group` too. It is now rejected; clearing is spelled
  "no columns".

- **A NULL string cell in a group update is SQL NULL, not undefined behaviour.** The C API built a
  `std::string` from a NULL `char*` when the presence mask was dense, which is exactly what
  `read_time_series_group` emits for a NULL STRING cell — so feeding a read result back with the
  mask stripped was UB. A NULL entry, or a NULL per-column data pointer, is now SQL NULL.

[0.13.0]: https://github.com/psrenergy/quiver/compare/v0.12.9...v0.13.0
[0.12.8]: https://github.com/psrenergy/quiver/compare/v0.12.7...v0.12.8
[0.12.7]: https://github.com/psrenergy/quiver/compare/v0.12.6...v0.12.7
[0.12.6]: https://github.com/psrenergy/quiver/compare/v0.12.5...v0.12.6
[0.12.5]: https://github.com/psrenergy/quiver/compare/v0.12.4...v0.12.5
[0.12.4]: https://github.com/psrenergy/quiver/compare/v0.12.3...v0.12.4
[0.12.3]: https://github.com/psrenergy/quiver/compare/v0.12.2...v0.12.3
[0.12.2]: https://github.com/psrenergy/quiver/compare/v0.12.1...v0.12.2
[0.12.1]: https://github.com/psrenergy/quiver/compare/v0.12.0...v0.12.1
[0.12.0]: https://github.com/psrenergy/quiver/compare/v0.11.0...v0.12.0
[0.11.0]: https://github.com/psrenergy/quiver/compare/v0.10.9...v0.11.0
[0.10.9]: https://github.com/psrenergy/quiver/compare/v0.10.8...v0.10.9
[0.10.8]: https://github.com/psrenergy/quiver/compare/v0.10.7...v0.10.8
[0.10.7]: https://github.com/psrenergy/quiver/compare/v0.10.6...v0.10.7
[0.10.6]: https://github.com/psrenergy/quiver/compare/v0.10.5...v0.10.6
[0.10.5]: https://github.com/psrenergy/quiver/compare/v0.10.4...v0.10.5
[0.10.4]: https://github.com/psrenergy/quiver/compare/v0.10.3...v0.10.4
[0.10.3]: https://github.com/psrenergy/quiver/compare/v0.10.2...v0.10.3
[0.10.2]: https://github.com/psrenergy/quiver/compare/v0.10.1...v0.10.2
[0.10.1]: https://github.com/psrenergy/quiver/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/psrenergy/quiver/compare/v0.9.16...v0.10.0
