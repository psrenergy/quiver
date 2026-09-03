# Changelog

All notable changes to Quiver are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Entries that require
callers to change something are prefixed **BREAKING** and say what to do.

## [0.10.3] — unreleased

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
  use raw SQL through `query_string`/`query_integer`; the readers' lenient fallbacks are unchanged.

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

- **Julia scoped resource factories.** `open`, `from_schema`, `from_migrations`, and
  `Binary.open_file` take a callback-first argument, so Julia `do` syntax releases the handle at the
  block's `end` on both the normal and the exceptional exit, and returns the callback's result. The
  finalizer already released eventually — what is new is *prompt, deterministic* release, which is
  what frees an OS file handle on Windows. Caveat: a `LuaRunner` built inside the block must not
  outlive it (it borrows the database), and an uncommitted transaction still open at the block's
  `end` is rolled back — use `transaction(db) do db ... end` inside.

- **Boolean convenience readers for INTEGER-backed values.** Julia, Python, Dart, and JavaScript
  now expose scalar, vector, and set boolean readers in both bulk and by-id forms, plus a boolean
  query helper. They compose the existing integer APIs, preserve scalar NULLs, and convert only
  `0`/`1` to `false`/`true`; any other integer raises the binding's native conversion error.

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

[0.10.3]: https://github.com/psrenergy/quiver/compare/v0.10.2...v0.11.0
[0.10.2]: https://github.com/psrenergy/quiver/compare/v0.10.1...v0.10.2
[0.10.1]: https://github.com/psrenergy/quiver/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/psrenergy/quiver/compare/v0.9.16...v0.10.0
