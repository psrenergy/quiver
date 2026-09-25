# Changelog

All notable changes to Quiver are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Entries that require
callers to change something are prefixed **BREAKING** and say what to do.

## [0.10.9] — unreleased

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

[0.10.9]: https://github.com/psrenergy/quiver/compare/v0.10.8...HEAD
[0.10.8]: https://github.com/psrenergy/quiver/compare/v0.10.7...v0.10.8
[0.10.7]: https://github.com/psrenergy/quiver/compare/v0.10.6...v0.10.7
[0.10.6]: https://github.com/psrenergy/quiver/compare/v0.10.5...v0.10.6
[0.10.5]: https://github.com/psrenergy/quiver/compare/v0.10.4...v0.10.5
[0.10.4]: https://github.com/psrenergy/quiver/compare/v0.10.3...v0.10.4
[0.10.3]: https://github.com/psrenergy/quiver/compare/v0.10.2...v0.10.3
[0.10.2]: https://github.com/psrenergy/quiver/compare/v0.10.1...v0.10.2
[0.10.1]: https://github.com/psrenergy/quiver/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/psrenergy/quiver/compare/v0.9.16...v0.10.0
