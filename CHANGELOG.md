# Changelog

All notable changes to Quiver are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Entries that require
callers to change something are prefixed **BREAKING** and say what to do.

## [0.10.0] — unreleased

### Added

- **Whole-group writers: `update_vector_group()` / `update_set_group()`.** Replace all of an
  element's rows in one *named* group; passing no columns clears the group. These are the write
  counterpart of `read_vector_group_by_id()` / `read_set_group_by_id()`, and the unambiguous
  alternative to passing arrays through `create_element()` / `update_element()` — those route an
  array by *column name*, which names more than one table when two groups of a collection share a
  column (legal: the schema validator exempts foreign-key columns), whereas `(collection, group)`
  names exactly one.

  Available in C++, the C API (columnar arrays plus a per-cell NULL mask, same shape as
  `quiver_database_update_time_series_group`), Dart (`updateVectorGroup` / `updateSetGroup`), and
  Python (`update_vector_group` / `update_set_group`). **Julia, JS, and Lua parity is still
  outstanding.**

- **Dart: `quiver_log_level_t` is exported** from `quiverdb.dart`. The `consoleLevel` values the
  factory constructors document were previously not reachable from outside the package.

### Changed

- **BREAKING — `export_csv()` writes foreign keys as labels, not ids.** A foreign-key column is
  now exported as the referenced element's `label`; self-references are excluded, mirroring what
  `import_csv()` already did. Export previously wrote the raw integer id while import resolved by
  label, so **any table with a relation could not round-trip** — import rejected its own
  exporter's output with
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

- **`query_float()` widens INTEGER results.** An INTEGER result is returned as a double instead of
  reported as "no value", matching the library's int64-for-REAL typing policy. `SELECT COUNT(*)`
  and `SUM(int_col)` — which SQLite answers as INTEGER — now return a value where they previously
  read back empty. Not marked breaking: it only turns an absent value into a present one, so
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

- **`from_migrations()` loads schema metadata when the migrations directory is empty.** Opening an
  existing database against a migrations directory with no versioned subdirectories previously
  returned a handle whose every metadata call failed with "no schema loaded". Note the
  consequence: a schema-validation error now surfaces at open time instead of at the first
  metadata call.

[0.10.0]: https://github.com/psrenergy/quiver/compare/v0.9.16...v0.10.0
