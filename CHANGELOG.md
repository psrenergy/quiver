# Changelog

All notable changes to Quiver are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Entries that require
callers to change something are prefixed **BREAKING** and say what to do.

## [0.10.2] — unreleased

### Added

- **`delete_element_by_label(collection, label)`.** Deletes an element addressed by its `label`
  instead of its id, for callers that already know the name and would otherwise round-trip through
  a query to find it. It resolves the label and then delegates to `delete_element`, so `ON DELETE
  CASCADE` cleanup is identical. Available in every layer, under the usual per-layer spelling.

  A label is unique **per collection, not per database**: one naming an element of a different
  collection does not resolve. A miss throws `Element not found: label '<label>' in collection
  '<c>'` and deletes nothing (no silent no-op, matching `delete_element` / `update_element`).
  Naming a table with no `label` column throws `Cannot delete_element_by_label: column 'label' not
  found in table '<t>'`.

### Changed

- **BREAKING — an unnamed column is no longer a written column for `update_time_series_files()`
  and `upsert_time_series_row()`.** Both each rebuilt the whole unit they addressed, so a column
  the caller did not mention was silently reset to NULL: updating only `data_file` wiped
  `metadata_file`, and upserting a row on an existing dimension key nulled its other value columns.
  Both now write only the columns you name and leave the rest untouched. A *named* column still
  takes the value you give it, explicit NULL included; when there is nothing to preserve — no
  existing row — the unnamed columns are NULL as before.

  *Adapt:* if you relied on omission to clear a column, name it explicitly with a null value
  (`nothing` / `None` / `null` / a NULL `column_data` pointer through the C API). **Lua cannot do
  this**: `{ x = nil }` is `{}` in Lua, so omission is the only signal it has and it now means
  *preserve* — the same limitation Lua already had on `create_element` / `update_element` scalars.
  Lua writers keyed by *position* are unaffected; a `nil` cell in a group column still writes NULL.

  *Everything else is unaffected:* `create_element` / `update_element` still route array
  attributes into their group table by a delete-and-reinsert, so naming one column of a
  multi-column group still nulls the others, and `update_vector_group` / `update_set_group` still
  write NULL wherever a column or cell across the caller's rows is unnamed. Partial, patch-style
  group writes are not supported anywhere else.

- **`upsert_time_series_row()` now handles primary-key conflicts only.** It uses
  `INSERT … ON CONFLICT(id, <dimensions…>) DO UPDATE` instead of `INSERT OR REPLACE`, which fired on
  *any* uniqueness conflict. A second UNIQUE constraint on a time-series table (unusual, and not
  required by the schema rules) now raises instead of replacing.

  A row that names *only* dimension columns also changes behaviour: there is nothing left to write,
  so the statement becomes `DO NOTHING` and the call now succeeds without touching the row, where
  `INSERT OR REPLACE` used to replace it and raise a NOT NULL constraint error. Name at least one
  value column if you mean to write something.

- **BREAKING — `quiver_database_upsert_time_series_row()` takes a per-cell NULL mask.** A
  `const uint8_t* const* column_has_value` parameter now sits between `column_data` and
  `column_count`, in the same position and shape as
  `quiver_database_update_{time_series,vector,set}_group`.

  *Adapt:* C and FFI callers must pass the new argument. `NULL` for the whole parameter, or for an
  individual column's entry, means that scope is present and behaves exactly as before; the two
  pointer spellings of NULL stay valid — a NULL `column_data[c]`, and a NULL `char*` entry for a
  STRING/DATE_TIME column. Every language binding's signature only *widens* to admit a null cell
  (Dart `Map<String, Object>` → `Map<String, Object?>`, JS `row` gains `| null`, Python/Julia
  accept `None`/`nothing`), so existing calls keep compiling.

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

[0.10.2]: https://github.com/psrenergy/quiver/compare/v0.10.1...v0.10.2
[0.10.1]: https://github.com/psrenergy/quiver/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/psrenergy/quiver/compare/v0.9.16...v0.10.0
