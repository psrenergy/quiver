# C++ Core (`src/` + `include/quiver/`)

This file covers the C++ library: public headers in `include/quiver/**` and implementation in
`src/**`, including the Lua, binary, and expression subsystems. The C API (`src/c/`,
`include/quiver/c/`) has its own `src/c/CLAUDE.md`. Cross-cutting rules (naming, error message
patterns, schema conventions, design decisions) live in the root `CLAUDE.md`.

## File Map

```
include/quiver/           # C++ public headers
  database.h              # Database class - main API
  attribute_metadata.h    # ScalarMetadata, GroupMetadata types
  options.h               # DatabaseOptions, CSVOptions types and factories
  element.h               # Element builder for create operations
  lua_runner.h            # Lua scripting support
  schema.h                # Schema/TableDefinition introspection, group table name helpers
  schema_validator.h      # SchemaValidator - schema convention checks
  type_validator.h        # TypeValidator - value-vs-column type checks
  value.h                 # Value variant (nullptr/int64/double/string)
  data_type.h             # DataType enum, data_type_to_string, is_date_time_column
  row.h / result.h        # Row and Result query-result types
  migration.h / migrations.h  # Migration (version dir, up/down sql) and Migrations discovery
  export.h / quiver.h     # Export macro, umbrella header
include/quiver/binary/      # Binary subsystem headers (binary file I/O)
  binary_file.h               # BinaryFile class (Pimpl) - open_file, read, write, get_metadata
  csv_converter.h             # CSVConverter class - bin_to_csv, csv_to_bin
  iteration.h                 # first_dimensions, next_dimensions, dimension_sizes_at_values
  binary_metadata.h           # BinaryMetadata struct - dimensions, labels, serialization
  dimension.h                 # Dimension struct (name, size, optional TimeProperties)
  time_properties.h           # TimeFrequency enum, TimeProperties struct
  time_constants.h            # Time dimension size constraints
include/quiver/expression/  # Expression subsystem headers (lazy expressions on .qvr files)
  expression.h                # Expression value type, + - * / operator overloads, save engine
  expression_node.h           # ExpressionNode base + concrete node classes + BroadcastOperand
src/                      # C++ implementation
  database.cpp            # Lifecycle, factories, transactions, execute, migrate_up
  database_impl.h         # Database::Impl - schema/type validators, label + FK resolution, group inserts, TransactionGuard
  database_internal.h     # internal:: helpers - read templates, value_matches_type, metadata converters
  database_create.cpp / database_read.cpp / database_update.cpp / database_delete.cpp
  database_metadata.cpp / database_query.cpp / database_time_series.cpp / database_describe.cpp
  database_csv_export.cpp / database_csv_import.cpp
  schema.cpp              # Schema introspection (from_database), table classification, group_names
  schema_validator.cpp    # Schema convention validation
  type_validator.cpp      # Scalar/array type validation (caller-threaded Pattern 1 messages)
  element.cpp / row.cpp / result.cpp / migration.cpp / migrations.cpp
  lua_runner.cpp          # LuaRunner (sol2) - all Lua bindings
  csv_read.h / csv_read.cpp  # Internal CSV reader (csv-parser, Pimpl'd) behind db:read_csv /
                              # db:read_csv_stream -- no include/quiver/ counterpart (see below)
  csv_write.h / csv_write.cpp  # Internal CSV writer (hand-rolled, NOT Pimpl'd) behind db:write_csv
                                # -- same no-include/quiver/-counterpart posture as csv_read
  ui_metadata.h / ui_metadata.cpp  # Internal ui/ TOML sidecar reader behind describe/describe_collection
                                # -- same no-include/quiver/-counterpart posture as csv_read
  cli/main.cpp            # quiver_cli CLI entry point
  utils/string.h          # String utilities: new_c_str, trim
  utils/datetime.h        # ISO 8601 parse/format helpers
  utils/number.h          # quiver::utils::append_number -- std::to_chars shortest round-trip
src/binary/                 # Binary C++ implementation
  binary_file.cpp             # BinaryFile class (Pimpl impl) + write registry
  binary_utils.h              # Shared file-extension constants
  csv_converter.cpp           # CSVConverter implementation
  iteration.cpp               # first_dimensions/next_dimensions impls + dimension_sizes_at_values
  binary_metadata.cpp         # BinaryMetadata factories, serialization, validation
  time_properties.cpp         # TimeFrequency string conversion
src/expression/             # Expression C++ implementation
  expression.cpp              # Expression class, operator overloads, save engine
  expression_helpers.h        # Shared inline helpers (validation, broadcast metadata/operands, aggregation accumulators, percentile)
  expression_file.cpp         # ExpressionFile (leaf reading from .qvr)
  expression_scalar.cpp       # ExpressionScalar (constant broadcast)
  expression_binary.cpp       # ExpressionBinary (Add/Sub/Mul/Div)
  expression_unary.cpp        # ExpressionUnary (Negate/Abs/Sqrt/Log/Exp)
  expression_ternary.cpp      # ExpressionTernary (IfElse)
  expression_aggregate.cpp    # ExpressionAggregate (dimension-axis Sum/Mean/Min/Max/Percentile)
  expression_aggregate_agents.cpp  # ExpressionAggregateAgents (label-axis reduction)
  expression_select_agents.cpp     # ExpressionSelectAgents (label-axis projection)
  expression_rename_agents.cpp     # ExpressionRenameAgents (label-axis rename)
```

`csv_read.h`/`csv_read.cpp` is the first `.cpp` in `src/` with no `include/quiver/` public
counterpart — every other internal helper here (`utils/string.h`, `database_internal.h`,
`binary/binary_utils.h`) is header-only inline, and every other `QUIVER_SOURCES` entry implements
a public header. It stays internal because there is no FFI consumer for it (Julia/Dart/Python/JS
already have native CSV libraries; Lua needs this precisely because `io` is deliberately absent),
so the root CLAUDE.md rule "bind every public method down to every binding" never fires — no
documented exception needed. `Reader` is Pimpl'd specifically so csv-parser's headers never have
to be included by `lua_runner.cpp`, which already needs `/bigobj` on MSVC for sol2's template
depth. Three `csv::CSVFormat` settings are pinned in exactly one place (`make_format`, in
`csv_read.cpp`) because every one of the library defaults is wrong for this reader:
`variable_columns(KEEP_NON_EMPTY)` (the default `IGNORE_ROW` silently discards any row whose field
count differs from the header), the header row (with no header pinned, csv-parser guesses one and
pops every record up to the guessed index — silently eating a one-cell title line above the real
header; driven by `Options.header_row`, 1-based at the Lua boundary, `0` = `no_header()`, default
`1` — Phase 2's `header_row` option, D-20), and never calling `chunk_size(...)` (with
`CSV_ENABLE_THREADS` forced off, the read window is csv-parser's own fixed default, unmultiplied by
worker count). **Call order in `make_format` is load-bearing**: the header mode must be set before
`variable_columns()`, because `CSVFormat::header_row(row < 0)` (i.e. `no_header()`) overwrites
`variable_column_policy` to plain `KEEP` as a side effect
(`build/_deps/csv_parser-src/include/internal/csv_format.cpp:44`) — reordering silently
reintroduces phantom blank-line rows for every no-header read. `Reader`'s constructor also
synthesizes the "header row not found" error csv-parser never raises itself: a header row past EOF
returns an empty header with zero rows in total silence, so the check is gated on the caller's
original request (`header_row != 0`) rather than header emptiness alone, since `header_row = 0`
also yields an empty header by design.

`csv_write.h`/`csv_write.cpp` is `csv_read`'s deliberate non-Pimpl counterpart (D-37): it depends
on nothing that must be kept out of the sol2 translation unit (no csv-parser, no third-party
headers), so hiding its `std::ofstream` member behind a Pimpl the way `Reader` hides csv-parser
would be cargo cult. It backs `db:write_csv` alone, with the same no-`include/quiver/`-header,
no-`QUIVER_API`, no-C-API posture as `csv_read`. Numeric cell formatting reuses
`quiver::utils::append_number` (`src/utils/number.h`) via `std::to_chars`'s shortest round-trip
form with no synthetic decimal point, so a whole float and the equal integer write identical text
(D-34); a `nil` cell and an empty-string cell are structurally indistinguishable after a CSV round
trip and that is stated, not fixed — CSV has no null (D-40). FMT-07's row-width enforcement (a
short `write_row` pads to the header's length, a long one throws) lives entirely in the Lua-layer
`CsvWriter` wrapper in `src/lua_runner.cpp`, not here: this file's `Writer` gained no header-width
state and no signature change for it, and padding happens before the cell vector ever reaches
`write_row`/`append_record`, so `append_record`'s `lone_empty_cell` predicate sees the final,
already-padded cell count.

`ui_metadata.h`/`ui_metadata.cpp` is the `ui/` TOML sidecar reader behind `describe()` and
`describe_collection()` (Phase 1 of the "UI Metadata in describe" milestone): same
no-`include/quiver/`-header, no-`QUIVER_API`, no-C-API-symbol, no-FFI-binding posture as
`csv_read` — `describe*` already return a plain `std::string` through the C API, so there is no
FFI consumer for a structured getter, and toml++ is linked PRIVATE on `quiver`
(`src/CMakeLists.txt`), so no `toml::` symbol may appear outside this `.cpp`; `ui_metadata.cpp` must
be listed in `QUIVER_SOURCES` for exactly that reason. The load happens once, in `from_migrations`
(`database.cpp`) right after `migrate_up` returns, and deliberately **not** on
`Impl::require_schema` — `migrate_up` early-returns before reaching the schema-load path on every
re-open of an already-up-to-date study, which is the common case for a real PSR run. A database
opened with `from_schema` never populates it, so its `Database::Impl::ui_metadata` stays
default-constructed (empty), and `describe`/`describe_collection` render exactly as before.

The sibling directory is `fs::weakly_canonical(migrations_path).parent_path() / "ui"` — raw
`parent_path()` was tried and is provably wrong two ways: a trailing separator on the migrations
path yields `<migrations>/ui`, which never exists, and a bare relative migrations path yields
`./ui` against whatever the process CWD happens to be at call time, not the sibling directory a
caller means. `fs::weakly_canonical` normalizes both away before `parent_path()` ever runs (the
same idiom `src/lua_runner.cpp`'s `resolve_sandboxed_path` already uses).

A `ui/*.toml` collection file self-selects by shape, never by filename: a top-level string `id`
plus an `attribute` array are both required, which is what excludes `main.toml` (no `id`), every
theme file (`id` but no `attribute` array), and any non-`.toml` file, with the scan staying
non-recursive so a `themes/` subdirectory is never walked into. `enum.toml` has no wrapper key of
its own — every one of its top-level keys IS a vocabulary name, discovered by iterating the whole
top-level table rather than reading one fixed array key, and an attribute joins a vocabulary by
its own `enum` value, never its `id` (several attributes commonly share one vocabulary, e.g.
`bool`).

The whole load is a **nested try/catch that warns and degrades, and never throws** — explicitly
not `src/binary/binary_metadata.cpp`'s posture, which throws on a parse error or a bare
`.value()` unwrap with no test for either. One outer catch covers directory iteration and path
resolution and yields a fully empty `UiMetadata` on failure; one inner catch per collection file (and
a separate one around `enum.toml`) means a single malformed `ui/*.toml` costs only that
collection's metadata, not every other collection's. An absent or empty `ui/` directory is the
ordinary case and logs nothing — an empty directory yields zero `directory_iterator` entries, so
the per-file loop body never runs. A directory that cannot be iterated (path resolution or the
iteration itself failing) logs one warning from the outer catch and degrades to an empty
`UiMetadata`; a file within it that cannot be opened (`ifstream::is_open()` is checked before
reading, and a failed open throws so it lands in the same catch as a parse failure) or that fails
to parse logs its own warning through the per-database logger and costs only that file —
`from_migrations` still succeeds in every case.

Rendering lives in `database_describe.cpp`, not here: `write_collection_section` gained a nullable
`const UiMetadata*` and a `bool with_tooltip` parameter and appends zero to three `"; keyword body"`
clauses (`label`, `enum`, `tooltip`, in that fixed order) after each scalar's existing
name/type/flags line — `describe()` passes `with_tooltip = false`, `describe_collection()` passes
`true`, so `describe()`'s line is always a strict prefix of `describe_collection()`'s by
construction. A label or tooltip whose `squash` (ASCII-lowercase, digits and letters only —
spelled as an explicit ASCII test, never `std::tolower(char)`, which is undefined behavior on a
negative `char` and would be hit by real non-ASCII corpus strings) matches the attribute name's
(or, for tooltip, the raw label's) squash is suppressed as redundant; the enum clause is never
suppressed, since it cannot be re-derived from the column name. `normalize_ui_text` maps every
byte below `0x20` and `0x7F` to a space before collapsing runs and trimming — a deliberate
superset of "just collapse newlines" that also neutralizes ESC, so a sidecar string can never emit
an ANSI escape sequence into a terminal rendering the report. An attribute with `hide = true` in
its sidecar still renders every clause: `describe*` describes the schema, not the UI's visibility
policy.

`summarize_collection()`'s integer value histogram (`src/database_describe.cpp`, inside the
`kMaxDistributionCardinality`-bounded branch) also annotates each *observed* code with its enum
label: `values {0 "Per Unit": 2, 1: 1}`. The `ui_metadata.find(collection, scalar.name)` lookup
sits immediately before `"; values {"` is written, not at the top of the per-scalar loop, so a
collection of TEXT/REAL/PK scalars pays zero two-level map lookups. **D-09 (deliberate divergence
from D-06):** here a label that normalizes to empty drops only the *annotation* and keeps the
*entry* — unlike the `enum {}` clause above, where the entry IS the vocabulary and an
empty-normalizing label drops the whole thing. In the histogram the entry is an observed row
count, and dropping it would destroy data. Two known limits, recorded rather than fixed: (1)
`ui_metadata` is populated only by `from_migrations` (see the early-return trap above), so
`Database::open("study.db").summarize_collection(...)` still shows bare codes — this must not be
"fixed" by hooking the load onto `load_schema_metadata` / `require_schema`, since `migrate_up`
early-returns before it on the open-an-existing-study path; and (2) `kMaxDistributionCardinality`
(64) still suppresses the whole `; values {}` clause above 64 distinct codes, so the richest enum
column renders no labels at all — `describe_collection()`'s `; enum {...}` is the fallback, and 64
labelled entries is already a ~2 KB single line, so no truncation is proposed (truncation needs its
own ellipsis convention). One more honest note: `quote_ui_text` gives a parse-level guarantee, not
substring immunity, and `summarize_collection` additionally emits `  Vectors:` / `  Sets:` /
`  Time Series:` headers — so a naive header-count test written against it on a `from_migrations`
database would be breakable by a label containing that text. The remedy, if it ever bites, is
asserting on report structure (line prefix + indentation), never a substring blocklist.

Three guards in the Lua layer's decoders (`src/lua_runner.cpp`) exist because a script is
untrusted input, in the same spirit as the JSON encoder's two caps below:
- `csv_max_integer_key` is the single max-integer-key walk behind both `csv_row_cells_from_lua`
  and `csv_header_from_lua` (so the key rule and its message live once), and it caps the result at
  1,000,000. Both callers materialize a **dense** vector up to that key, so `{[1e9] = "x"}` — the
  same sparseness hazard the encoder note below names — allocated tens of gigabytes, or reached
  the script as a raw `std::bad_alloc` with no Pattern 1 prefix.
- `csv_options_entries` checks each option key's Lua *type* before converting it. sol2's
  `std::string` getter is `lua_tolstring`, which answers `nullptr` for a boolean/table/function
  key: unchecked in Release (`SOL_SAFE_GETTER` is off there) and a raw sol2 panic in Debug, so
  `{ [true] = 1 }` reached the script as a bare Lua value rather than a message.
- `csv_separator_from_lua` rejects `"`, CR, LF and NUL in addition to the multi-byte check. They
  are one byte but cannot be delimiters: csv-parser refuses a delimiter that overlaps its quote
  character, so `db:write_csv` with `separator = '"'` silently produced a file `db:read_csv`
  could not open.
`w:write_row` also checks its argument is a table: sol2's check for a `const sol::table&`
parameter is a loose one that accepts **userdata** too, and iterating a userdata yields no keys,
so `w:write_row(db)` appended a spurious empty record instead of throwing. For the same reason
`db:read_csv_stream`'s `on_row` is a `sol::object` with an explicit `sol::type::function` check
rather than a typed `sol::protected_function` parameter — the typed one surfaced sol2's own
"stack index 3, expected function" text.

`Impl::open_writers` is also the concurrency guard: it records each writer's **resolved** path, and
`db:write_csv` refuses a path some live, unclosed writer already holds (Pattern 1, mirroring
`db:open_file`'s process-global write registry in `src/binary/binary_file.cpp`). Two writers on one
path each open with `ios::trunc` and write from offset 0, so the second silently discarded
everything the first had buffered. Reopening a path whose previous writer was **closed** is still
the documented truncate (WRITE-08) — the guard checks `is_closed()`, which is what keeps
`ReopeningSamePathTruncatesExistingContent` green.

## Pimpl vs Value Types

Pimpl is used only for classes that hide private dependencies (e.g., `Database`, `LuaRunner` hide sqlite3/lua headers):
```cpp
// database.h (public)
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// database.cpp (private)
struct Database::Impl {
    sqlite3* db;
    // all implementation details
};
```

Binary subsystem: `BinaryFile` uses Pimpl (hides file I/O dependencies). `CSVConverter` is a plain class composing a `BinaryMetadata` and the CSV `iostream` (no Pimpl, no inheritance). `BinaryMetadata`, `Dimension`, `TimeProperties` are plain value types.

Expression subsystem: `Expression` is a plain value type wrapping `shared_ptr<ExpressionNode>` — no Pimpl. `ExpressionNode` is an abstract base with virtual `metadata()` / `compute_row()`; concrete subclasses are exposed via `QUIVER_API` and use Rule of Zero. Polymorphism is justified by the recursive tree shape (operand-owning nodes hold child `shared_ptr<ExpressionNode>`).

Classes with no private dependencies (`Element`, `Row`, `Migration`, `Migrations`, `GroupMetadata`, `ScalarMetadata`, `CSVOptions`, `Dimension`, `TimeProperties`, `Expression`) are plain value types — direct members, no Pimpl, Rule of Zero (compiler-generated copy/move/destructor). `BinaryMetadata` is the one deviation: it user-declares its default constructor and destructor (defaulted out-of-line), which suppresses compiler-generated moves — moves silently fall back to copies.

## Transactions

Public API exposes explicit transaction control:
```cpp
db.begin_transaction();
// multiple write operations...
db.commit();   // or db.rollback();
bool active = db.in_transaction();
```

Internally, `Impl::TransactionGuard` is nest-aware RAII: if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`), the guard becomes a no-op. This allows write methods (`create_element`, etc.) to work both standalone and inside explicit transactions without double-beginning.

```cpp
// Internal RAII guard (nest-aware)
{
    TransactionGuard guard(impl);
    // operations...
    guard.commit();
}
```

**Dry runs** (`begin_dry_run` / `end_dry_run` / `in_dry_run`, `Impl::dry_run` flag) sit on top of
the same machinery: `begin_dry_run` opens a real transaction and sets the flag; `end_dry_run`
clears the flag and calls `impl_->rollback()` **directly** — the public `rollback()` is a no-op
while the flag is set, so going through it would silently do nothing. The public
`begin_transaction`/`commit`/`rollback` each start with a one-line `if (impl_->dry_run) return;`.
`TransactionGuard` needs no flag awareness: it checks `sqlite3_get_autocommit` and already no-ops
because the dry run holds a real transaction. `end_dry_run` guards its rollback on
`sqlite3_get_autocommit` because a caller can end the transaction out from under it with a bare
`COMMIT` through `query_*`. Rationale and the documented consequences: root design decisions.

The one write path that cannot nest is `import_csv` (it toggles `PRAGMA foreign_keys`, which is a
no-op inside a transaction) — it throws `"Cannot import_csv: transaction already active"` as a
precondition instead of silently destroying the caller's transaction.

## Move Semantics

Delete copy, default move for resource types:
```cpp
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&&) = default;
Database& operator=(Database&&) = default;
```

## Factory Methods

Static methods for database creation and migration validation:
```cpp
static Database from_schema(const std::string& db_path, const std::string& schema_path, const DatabaseOptions& options = {});
static Database from_migrations(const std::string& db_path, const std::string& migrations_path, const DatabaseOptions& options = {});
static void validate_migrations(const std::string& migrations_path);
```
`schema_path` is a `.sql` file; `migrations_path` is a directory of numbered version
subdirectories with `up.sql`/`down.sql`.
`validate_migrations` validates that directory in an in-memory database by executing every up migration
and then every down migration, and finally rejects any table left behind; the direction-specific
execution helpers remain private.

## Logging

spdlog with debug/info/warn/error levels, through a **per-database logger instance** — never the
`spdlog::` global functions. `create_database_logger` (`database.cpp`) builds a uniquely named
logger (`quiver_database_<id>`) with stderr + file sinks, stored as `Impl::logger`:
```cpp
impl_->logger->debug("Opening database: {}", path);
```

## Core Internals Worth Knowing

- **Group inserts are unified** (`database_impl.h`): one `insert_rows_into_group_table(caller,
  table, type, columns, id, delete_existing, db)` helper serves vector, set, and time-series
  writes; a single routing map (`table_name -> {type, columns}`) classifies an element's array
  attributes. Don't re-grow per-type copies. Two load-bearing details: validation (types +
  same-length) runs **before** the DELETE, because `TransactionGuard` no-ops when a transaction is
  already open (dry run, caller-owned) and a throw after the DELETE would leave the group cleared
  with nothing to roll back; and `num_rows` is seeded from the **first** column rather than the
  first non-empty one, because `columns` is name-sorted and an empty alphabetically-first column
  otherwise left it at 0 for a later column to overwrite — skipping the check and indexing past the
  end of the empty vector.
- **`Impl::update_group_rows`** (`database_update.cpp`) is the shared body of
  `update_vector_group`/`update_set_group`. It validates the **union of every row's keys** (not
  `rows[0]`, which dropped later-row-only columns and skipped validating them) against the group
  table, rejects the derived `id`/`vector_index`, and calls `require_element` — all before
  `transpose_group_rows`, so a named-but-empty column list cannot fall through to the DELETE.
- **`Impl::require_element`** (`database_impl.h`) is the single `SELECT 1 ... WHERE id = ?` guard
  behind the Pattern 2 `"Element not found: ..."` message, shared by `update_element`,
  `delete_element`, and both group writers.
- **Label→id resolution has one query** (`database_impl.h`): `Impl::lookup_id_by_label(table,
  label, db)` is the only `SELECT id ... WHERE label = ?`, shared by `Impl::resolve_label`
  (Pattern 2, backs every `_by_label` form) and `Impl::resolve_fk_label` (Pattern 3) — the two
  report a miss differently, so the throw stays with each caller. `resolve_label` calls
  `require_column(collection, "label")` because `require_collection` only checks `has_table`, so a
  group table would otherwise reach the SELECT and leak a raw `no such column: label` prepare
  error; it takes the operation name so the message reports the caller's public method, not the
  delegate's.
- **Table classification has one source** (`schema.cpp`): `Schema::group_names(collection,
  GroupTableType)` and `is_group_table(table, type)` are the only way to enumerate/classify
  `_vector_` / `_set_` / `_time_series_` tables (`group_names` excludes `_time_series_files`).
  All list/metadata/describe call sites use them — never hand-roll prefix scans.
- **Declaration order everywhere**: metadata and list functions iterate `column_order`
  (declaration order), matching the `describe(ostream&)` dump and CSV export. Nothing reports alphabetical order.
- **`number_of_elements` lives in `database_read.cpp`** alongside the other element-level reads
  (`read_element_ids`). `number_of_elements(c)
  const` validates through `Impl::require_collection` and directly executes `SELECT COUNT(*)`
  against the quoted collection. The value is queried on demand and is not cached. `describe()`,
  `describe_collection(c)`, and `summarize_collection(c)` reuse the public operation; the
  collection-specific report methods intentionally retain their own validation so
  errors name the operation the caller invoked.
- **`describe*` return text reports** (`database_describe.cpp`): `describe()` (whole-DB overview),
  `describe_collection(c)` (one collection's structure), `summarize_collection(c)` (per-scalar
  null/non-null counts + low-cardinality integer distributions [threshold `kMaxDistributionCardinality`]
  + per-group empty/non-empty counts) all build an `std::ostringstream` and return `std::string`. These
  const methods run their own read-only SQL via an anon-namespace `query_int_rows` helper that
  prepares/steps directly on `impl_->db` (the `current_version() const` pattern — `execute()` is
  non-const). All three are bound 1:1 across the C API and every binding as string getters.
- **`TypeValidator` threads the caller's name** (`type_validator.cpp`): call sites pass
  `"create_element"` / `"update_element"` so messages read `"Cannot create_element: type
  mismatch for column ..."` (root Pattern 1).
- **One scalar typing policy** shared by `value_matches_type` (`database_internal.h`, time-series
  writes) and `TypeValidator::validate_value` (`type_validator.cpp`, scalar create/update): int64
  matches `INTEGER` or `REAL` (int-for-REAL coercion), double matches `REAL` only (a float into an
  `INTEGER` column is rejected), string matches `TEXT`/`INTEGER`(FK label)/`DATE_TIME`. Keep the two
  in sync (root design decision).
- **DATE_TIME content is checked by both halves of that policy, through one predicate**:
  `datetime::is_valid_iso8601` (`utils/datetime.h`). `TypeValidator::validate_value` calls it in its
  string branch (covering scalar create/update and every vector/set array write, so it inherits the
  validate-before-DELETE ordering below); `validate_time_series_row` (`database_time_series.cpp`)
  calls it in a **separate** guard next to `value_matches_type`. Do not "restore symmetry" by moving
  the check into `value_matches_type`: that function decides the *variant's shape*, and TEXT into a
  DATE_TIME column is the correct shape — routing a content failure through its `bool` would emit
  `column 'date_time' has type DATE_TIME but received TEXT`, which is a lie. The two guards phrase
  their own messages; the rule itself lives in exactly one function.
  `parse_datetime_import` (`database_csv_import.cpp`) is the **third** gate and needs to exist:
  `import_csv` writes through a raw `INSERT` and never reaches `TypeValidator`, and its
  custom-`date_time_format` branch parses with the caller's `get_time` format, which cannot see an
  impossible calendar day (`"%d/%m/%Y"` on `31/02/2024`). It therefore runs `is_valid_iso8601` on
  the string it canonicalizes, so import is held to the same grammar as the other writers.
- **`update_element` / `delete_element` / the vector+set group writers verify the id exists** (via
  `Impl::require_element`) and throw Pattern 2 `"Element not found: ..."` — no silent no-op.
  The two time-series writers do not: `upsert_time_series_row` always writes one row, so a bad id
  always fails at the foreign key; `update_time_series_group` fails there too when it has rows to
  write, but with no rows it deletes nothing and returns silently.
  Every `_by_label` form resolves the label via `Impl::resolve_label`, and is a one-line
  delegation to its id counterpart (the root `_by_label` rule), so `update_element_by_label`'s
  *element* validation — the empty-element throw, `TypeValidator`, `insert_group_data` — reports
  `Cannot update_element: ...` and the group/row writers' column validation reports
  `Cannot update_{vector,set,time_series}_group: ...` / `Cannot upsert_time_series_row: ...`,
  naming the operation that validated.
- **`update_relation` is a validated `update_element`** (`database_update.cpp`): the derived column
  is checked against the schema through the `TableDefinition::get_foreign_key` accessor added for
  it in `schema.cpp`, then written as a one-attribute `Element` (`std::nullopt` becomes
  `Element::set_null`). It resolves no label of its own — binding the target label to an INTEGER FK
  column is already `Impl::resolve_fk_label`'s job, and the missing-id check is `require_element`'s
  — so failures past the derivation report `Cannot update_element: ...`.

- **Schema metadata loads lazily** (`Impl::require_schema`): the `Database(path, options)`
  constructor does not read it, so the first metadata/CRUD call does. `schema` and `type_validator`
  are `mutable` (const readers trigger the load) and `load_schema_metadata()` is `const` and
  publishes **neither** member until `SchemaValidator::validate()` passes — assigning `schema`
  first would leave a half-loaded state (schema set, `type_validator` null) alive after a failed
  lazy load, crashing the next call. Rationale in the root design decisions.
- **`Row::get_float` widens an int64** (`row.cpp`): the one place the int64-for-REAL policy is
  implemented for reads, since `read_column_values<double>`,
  `read_column_values_nullable<double>`, `read_single_value<double>` and `query_float` all funnel
  through it. Don't re-add a widening branch at a call site.
- **Two column readers in `database_internal.h`**: `read_column_values<T>` drops NULLs (dense —
  used by vector/set `_by_id` and `read_element_ids`, whose columns are NOT NULL / PK by
  convention); `read_column_values_nullable<T>` keeps them as `std::optional<T>` and backs only the
  three `read_scalar_*` bulk readers (one entry per element, `ORDER BY rowid`). The Lua scalar
  readers consume the optional vector directly via a `to_lua_table(vector<optional<T>>)` overload
  that emits `nil` holes (root scalar-NULL design decision).
- **`scalar_metadata_from_column` reports an INTEGER PRIMARY KEY as `not_null`**
  (`database_internal.h`): a rowid-alias PK is never NULL, but SQLite's `PRAGMA table_info` leaves
  the `notnull` flag unset, so the public `ScalarMetadata.not_null` ORs in `primary_key && type ==
  Integer`. The raw `ColumnDefinition.not_null` stays the literal PRAGMA value — `csv_import`
  (empty-cell rejection) and `schema_validator` read it directly and must not see PK flip. This is
  what lets Julia's nullability-aware readers return a concrete `Vector{Int64}` for `id`.
- **`execute` validates parameter count** (`database.cpp`): `sqlite3_bind_parameter_count` must
  equal `parameters.size()`, else it throws — the single guard for every `query_*` and internal
  parameterized statement.
- **Utilities**: `quiver::string::new_c_str` / `trim` in `src/utils/string.h`; ISO 8601
  parse/format helpers in `src/utils/datetime.h` — `parse_iso8601` accepts `YYYY-MM-DD` with an
  optional `THH:MM:SS`/` HH:MM:SS`, every field fixed-width and zero-padded, year `0001`-`9999`,
  the calendar day must exist, no leap second, and the whole string must be consumed. It is a
  **hand-rolled shape-mask match, deliberately not `std::get_time`** — the accepted length is 10 or
  19 and every character is checked against `"dddd-dd-ddTdd:dd:dd"` (`d` = digit, `T` = `T` or a
  space, everything else literal), so the length check alone is what forbids a partial or trailing
  anything. get_time instead: its field widths are
  *maxima*, so it also matches `2024-1-5`, `24-01-15` and `T1:30:00`, and on MSVC it does not set
  failbit when the input ends mid-format, so `T10:30` passed on Windows and failed on Linux. Every
  one of those is rejected by Python's `fromisoformat` or Dart's `DateTime.parse`, i.e. get_time
  cannot express the intersection band the write gate promises. (It was also locale-sensitive, and
  a literal space in its format means *skip zero or more spaces*, which accepts
  `2024-01-0110:30:00`.) `parse_iso8601` fills `tm_wday`/`tm_yday` too — nothing else does, and
  `format_datetime` feeds the `tm` to `strftime`, so `%a`/`%A`/`%j`/`%U`/`%W` would otherwise
  report every date as a Sunday on day 001. `is_valid_iso8601` trims before parsing, because
  `Database::execute` trims every bound string and the gate must judge the value that is stored.
  `format_utc` always writes the full `T` form;
  use `is_date_time_column` (`data_type.h`) for `date_`-prefix checks (one legacy hand-rolled
  `starts_with("date_")` remains in `schema_validator.cpp`).

## LuaRunner

Executes Lua scripts with database access (sol2). The `db` userdata exposes the same API as the
other bindings (root cross-layer tables):
```cpp
LuaRunner lua(db);
lua.run(R"(
    db:create_element("Collection", { label = "Item", value = 42 })
    local values = db:read_scalar_integers("Collection", "value")
)");
```

Implementation conventions in `lua_runner.cpp`:
- **Filesystem sandbox**: `resolve_sandboxed_path(db, operation, path)` is the single gate for
  every file-touching Lua operation (`db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`,
  `db:export_csv`, `db:import_csv`, `db:validate_migrations`, `db:read_csv`, `db:read_csv_stream`,
  `db:write_csv`, `expr:save`). It rejects `:memory:`
  databases, resolves relative paths against the database file's directory (bare-filename db paths fall back to the
  CWD at call time, mirroring `create_database_logger`), canonicalizes via `weakly_canonical`,
  and requires strict containment (candidate == root is rejected — the binary subsystem appends
  `.qvr`/`.toml` by string concatenation). The resolved absolute path is what's forwarded
  downstream, so the process CWD is irrelevant to Lua file I/O. Pattern 1 messages thread the
  public operation name. This is LuaRunner policy only — the C++/Julia surfaces stay unsandboxed.
  **The `current_path`/`weakly_canonical` block is wrapped in a `try`/`catch` that re-throws as
  `"Cannot <op>: cannot resolve path '<p>': <os reason>"`** — those throwing overloads raise
  `std::filesystem_error` for any OS failure that is not a plain "does not exist", and a Windows
  device name (`NUL`, `nul`, any case, any directory) is exactly such a case. Unwrapped, the raw
  `weakly_canonical: The parameter is incorrect.: ...` reached the script with no Pattern 1 prefix
  at all, breaking LUA-08 for **every** operation in the list above, not just the one it was found
  through. Because this is the single gate they all share, the guard belongs here and nowhere else;
  the deliberate `:memory:` and containment throws stay outside the `try` so they are not
  double-wrapped. Covered by `LuaRunner_ReadCsv.DeviceNamePathIsReportedWithPrefix` and
  `LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` (the latter spanning `open_file`/`bin_to_csv`/
  `csv_to_bin`, so the shared fix cannot regress to a per-caller patch).
- **Enabled standard libraries**: `base`, `string`, `table`, `math`, `coroutine`, and `utf8`
  (pure computation only). `os`, `io`, `package`/`require`, and `debug` stay unloaded — scripts
  cannot reach the shell, the process, the environment, or the filesystem outside the db sandbox.
- `dofile` and `loadfile` are nil'd out after `open_libraries` (no loading Lua source from disk);
  string-form `load` stays available.
- **The agent-facing Lua reference lives in `bindings/js/src/lua-api.ts`** (shipped on npm as
  `LUA_DB_API_REFERENCE` and interpolated into an LLM system prompt downstream). Adding or removing
  a `db:`/`quiver.*` binding, or changing the `open_libraries` list, requires updating it —
  `bindings/js/test/lua-api-sync.test.ts` parses `lua_runner.cpp` and fails otherwise. That check
  exists because the doc went stale two days after it was written: it said only
  `base`/`string`/`table` were loaded and "there is NO `math`", and #210 added
  `math`/`coroutine`/`utf8` here without touching it.
- **A nullable argument whose absence *means* something takes `sol::object`, not
  `sol::optional<T>`**: `sol::optional<T>` yields `nullopt` for a wrong type just as it does for
  `nil`, so `db:update_relation(..., false)` silently cleared the relation.
  `relation_target_from_lua(object, caller)` distinguishes the two — nil/missing clears,
  a non-string throws `Cannot <caller>: target_label has unsupported Lua type`. Both
  `db:update_relation(..., nil)` and omitting the argument clear; that affordance is sol2's and
  Lua-only (the FFI bindings all require the parameter and take their language's null).
- `parse_csv_options(table)` is the single CSVOptions parser shared by `export_csv`/`import_csv`.
- `to_lua_table<T>` overloads (flat + nested) are the only vector→table marshalers.
- `describe` / `describe_collection` / `summarize_collection` are bound as plain lambdas returning
  the C++ `std::string` text report (`db:describe()` returns a string — it does not print).
- Lua→C++ converters **throw on unsupported value types** (functions, nested tables, ...) — never
  skip silently; a skipped positional query parameter would shift the rest and bind NULL to the
  trailing placeholder.
- **A Lua boolean is INTEGER 1/0 on every write path**, matching the cross-layer policy in the root
  `CLAUDE.md`. Every boolean test goes through the one predicate `is_lua_boolean`, used by
  `table_to_element` (scalars *and* the array dispatch), `lua_table_to_value_map` (row upsert),
  `lua_table_to_values` (query parameters), `columns_to_cpp_rows` (group cells), and
  `lua_table_to_vector` (per array cell). `relation_target_from_lua` is the deliberate exception:
  only `nil` may clear a relation, so a boolean still throws there. Lua has no boolean *readers*
  (root design decision), so this is a write-side-only asymmetry.
- **`lua_cell_as<T>(object, caller, what)` is the one checked Lua-value→C++ conversion**, and
  every converter routes through it: `lua_table_to_vector` (per array cell),
  `lua_table_to_dim_map` (per binary dimension) and `update_time_series_files_lua` (per path).
  `what` names the offending slot in the Pattern 1 message — `cell #3`, `dimension 'stage'`,
  `path 'data_file'` — so one rule and one message shape cover all three.
- **`lua_table_to_vector<T>(table, caller)` is the only table→vector converter**, and it converts
  and checks **every cell**, not just the one the caller dispatched on. Both halves are
  load-bearing. `table_to_element` picks an array's element type from cell 1 alone, and sol2's
  plain `get<T>` is unchecked whenever `SOL_SAFE_GETTER` is off — which is every **release** build:
  `src/CMakeLists.txt` sets `SOL_SAFE_NUMERICS=1` and `SOL_SAFE_FUNCTION=1`, but `SOL_SAFE_GETTER`
  is left at sol2's default (on in debug, off in release). So a mixed `{1, true}` used to store 0
  and `{"a", true}` an empty string, silently, in release only — a class of bug Debug CI cannot
  see. The converter now coerces a boolean cell to 1/0 for a numeric `T` and raises a Pattern 1
  `"Cannot <caller>: cell #N has unsupported Lua type"` for anything that does not fit, so both the
  int and the float/string paths are covered. Two known limits, both pre-existing: the loop is
  bounded by `t.size()` (`lua_rawlen`), so a table with `nil` holes truncates — unlike
  `collect_group_columns`, which walks `pairs` for exactly that reason; and the element type still
  comes from cell 1, so `{1, 2.5}` into a REAL column is rejected rather than widened (JS scans the
  whole column and accepts it). One consequence worth knowing: `lua_opt_int64_vector` routes
  through it too, so `quiver.metadata{dimension_sizes = {true}}` coerces to a size-1 dimension
  rather than erroring. That is consistent with the cross-layer boolean policy, and
  `BinaryMetadata::validate()` still rejects a non-positive size, so `{false}` throws.
- **`SOL_SAFE_NUMERICS=1` (`src/CMakeLists.txt`) is load-bearing for the whole file.** It turns on
  sol2's `SOL_NUMBER_PRECISION_CHECKS`, which is what makes `is<int64_t>()` false for a Lua float.
  Without it that check degrades to "is a number" in release, and the file-wide
  `is<int64_t>()`-before-`is<double>()` ordering would route every float into the integer branch
  and store `llround(x)`. Do not drop or move those definitions.
- **`SOL_NO_NIL=1` (`src/CMakeLists.txt`) is a portability guard, not a preference.** sol2 does not
  define `sol::nil` on Apple platforms at all: `version.hpp` turns `SOL_NIL` off whenever
  `__MAC_OS_X_VERSION_MAX_ALLOWED`, `__OBJC__` or a `nil` macro is visible, because Objective-C
  already claims the name. `sol::lua_nil` is always defined and is literally the same object of the
  same type (`types.hpp`: `using nil_t = lua_nil_t; inline constexpr const nil_t& nil = lua_nil;`),
  so the two spellings are interchangeable everywhere `sol::nil` exists. Without this define the
  difference is invisible on Windows and Linux and only surfaces as a macOS CI compile error —
  which is exactly how one `sol::nil` in `db:read_csv_stream`'s header argument reddened both macOS
  jobs for three runs while every other platform stayed green. Setting it makes the portable
  spelling the only one that compiles anywhere, so the mistake fails on the developer's own
  machine. `PRIVATE` on `quiver` is full coverage: `lua_runner.cpp` is the only translation unit in
  the repo that includes sol2 (no test includes `<sol/sol.hpp>`). Use `sol::lua_nil` and
  `sol::type::lua_nil`, never `sol::nil` / `sol::type::nil`.
- `time_series_rows_from_lua` transpose, shared by `update_time_series_group_lua` and
  `update_time_series_group_by_label_lua` (both one-liners over it). Mirrors `group_rows_from_lua`
  but takes `db`, since the dimension columns come from metadata. The **dimension column(s) are
  the row-count authority**, discovered via public `get_time_series_metadata` (`dimension_column`
  plus any `value_columns` with `primary_key` set — the multi-dim case; `Database` exposes no
  Schema accessor so `internal::find_dimension_columns` is unreachable here). Dimension columns
  must be present and dense; value columns may be shorter, sparse, or empty — missing indices become
  `Value{nullptr}` (rows stay uniform: the core builds its INSERT list from `rows[0]`).
  Longer-than-dimension throws; a non-array column or non-positive-integer key throws; named
  columns with a zero-length dimension still throw — only a genuinely empty `{}` (no columns)
  clears (the anti-silent-clear trap is preserved). Column extents come from a `pairs` walk, never
  `sol::table::size()` (`lua_rawlen` returns an arbitrary border on a table with holes). The read
  path emits NULL cells as `nil` holes (an all-NULL column is an empty table with the key present),
  so read → modify → write round-trips; `#ts.<dimension>` is the trustworthy row count.
- **`run` returns the script's return value as JSON**, built by the anonymous-namespace
  `append_json` / `append_json_string` / `append_json_double` / `append_json_table` at the top of
  the file, plus `quiver::utils::append_number` (`src/utils/number.h` — moved out of this file,
  D-38; `db:write_csv`'s cell formatter is its third caller). The table check uses `get_type()` rather than
  `is<T>()` on purpose: sol2's `is<sol::table>()` also accepts **userdata**, so `return db` would
  quietly encode as `{}`. The boolean check spells `get_type()` for consistency with
  `is_lua_boolean` in `Impl`, not out of necessity — sol2's `check<bool>` *is* `lua_isboolean`
  (`stack_check_unqualified.hpp`), so `is<bool>()` would be equivalent here. Everything
  else reuses the house `is<int64_t>()`-then-`is<double>()` ordering. Object keys are collected into
  a `vector` and sorted so output is deterministic — Lua's `pairs` order is not, and the tests
  compare exact strings; a `std::map` was used first and dropped (one tree node per entry on a
  large return). Adjacent equal keys after the sort are **rejected**: an integer key and a string
  key spelling the same text (`{[1] = 'a', ['1'] = 'b'}`) are two Lua keys but one JSON key.
  Output accumulates into a plain **`std::string`**, never an `ostringstream`: no locale (`num_put`
  would insert thousands separators into integers under a grouping global locale, producing invalid
  JSON), no per-token sentry, no copy out of the stream, and no `badbit` swallowing a `bad_alloc`.
  Numbers — integers and doubles alike — go through `std::to_chars` (shortest round-trip,
  locale-independent); non-finite becomes `null` since JSON has no NaN/Infinity literal.
  `append_json_string` appends runs of safe bytes whole and **validates UTF-8**: JSON must be UTF-8
  (RFC 8259) but a Lua string is an arbitrary byte array, and this is the only layer that can
  diagnose it (downstream, Python raises `UnicodeDecodeError`, Dart `FormatException`, and JS
  silently substitutes U+FFFD).
- **Two caps guard the encoder against untrusted scripts**, and both are needed:
  `kMaxReturnDepth = 32` bounds nesting (and is the cycle guard — a self-referencing table would
  otherwise recurse until the stack dies), while `kMaxReturnBytes = 64 MiB` bounds output. Depth
  alone is not enough: a table that shares sub-tables (`local t = {1}; for _ = 1, 30 do t = {t, t}
  end`) is only 31 levels deep but expands to 2^30 nodes.
- **A table with holes encodes as an object, not an array.** The array branch requires keys exactly
  `1..n`, so a bulk read of a nullable column — which the scalar-NULL design decision represents as
  `nil` holes — comes back as `{"1":10,"3":30}` with text-sorted keys, not `[10,null,30]`. Recorded
  in the agent-facing Lua reference; changing it is a design decision (array-with-nulls needs a
  sparseness guard against `{[1e9] = 1}`).
- Script errors surface as `"Failed to run Lua script: ..."` (root Pattern 3). Encoder failures
  (unsupported type, unsupported table key, too deep) are Pattern 1 `"Cannot run: ..."` and are
  **not** wrapped in that prefix — they happen after the script already succeeded.
- **A writer left open when the script returns is still flushed.** `LuaRunner::run` declares one
  function-local RAII guard (`GcGuard`) before calling `safe_script`, whose destructor runs
  `Impl::close_open_writers()` and then `impl_->lua.collect_garbage()` exactly once at `run()`'s
  scope exit — covering the normal-return, empty-return, and throw-unwinding paths alike. The
  guard is declared *before* `result`, so C++'s reverse-declaration-order destruction runs both
  *after* `result`'s Lua stack reference is released.
  **The explicit close is the load-bearing half, not the collection.** `collect_garbage()` only
  finalizes *unreachable* objects, so a writer the script assigned to a global
  (`w = db:write_csv(...)` — no `local`, Lua's default spelling) is a GC root and was never
  flushed: the file stayed at 0 bytes, which
  `LuaRunner_WriteCsv.UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns` pins. `db:write_csv`
  therefore hands out a `std::shared_ptr<csv_write::Writer>` and records a `weak_ptr` in
  `Impl::open_writers`; `close_open_writers()` locks each one still alive, closes it (swallowing a
  flush failure — a scope-exit guard has no caller to report to, exactly as `~Writer` did), and
  clears the list. A writer therefore does not outlive its `run()`. The `collect_garbage()` call
  stays for every other sol2-owned resource; one call was proven sufficient by an executed probe
  against this repo's own vendored sol2/Lua build (RESEARCH.md Q1) — it must not be "hardened"
  into a loop.

## Binary Subsystem

Standalone binary file I/O layer for `.qvr` files with `.toml` metadata sidecars.
Bound in **Julia and Lua** (root design decision); Lua binds these C++ classes directly via sol2 in `src/lua_runner.cpp` (file I/O is db-scoped and sandboxed — `db:open_file`/`db:bin_to_csv`/`db:csv_to_bin`; metadata builders under `quiver.*`; method syntax + string aggregation ops).

- `BinaryFile` class (Pimpl): `open_file(path, mode, metadata?)`, `read(dims, allow_nulls = false)`, `write(data, dims)`, `get_metadata()`, `get_file_path()`
- `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)`
- `BinaryMetadata` struct: `dimensions`, `initial_datetime`, `unit`, `labels`, `version`; `number_of_time_dimensions()` is derived from `dimensions` (not stored)
  - Factories: `from_toml_content()`, `from_element()`
  - Serialization: `to_toml()`
  - Builders: `add_dimension()`, `add_time_dimension()` (chains `parent_dimension_index` to the previous time dimension)
  - Validation: `validate()`, `validate_time_dimension_metadata()`, `validate_time_dimension_sizes()`
- `Dimension` struct: `name`, `size`, optional `TimeProperties`
- `TimeProperties` struct: `frequency`, `initial_value`, `parent_dimension_index`
- `TimeFrequency` enum: `Yearly`, `Monthly`, `Weekly`, `Daily`, `Hourly`

### Iteration Helpers

Free functions in `quiver::` for traversing the dimension space of a `BinaryMetadata` (declared in `quiver/binary/iteration.h`):

- `first_dimensions(meta)` — initial position; returns `initial_value` for time dims, `1` for non-time dims
- `next_dimensions(meta, current)` — next position via right-to-left cascade; returns `nullopt` at end. Uses `dimension_sizes_at_values` to handle variable-length time dims (Feb=28/29, Jan=31, etc.)
- `dimension_sizes_at_values(meta, values)` — per-dim actual sizes at a coordinate vector. Used by `next_dimensions()` and `ExpressionAggregate` to size variable-length time-dim iteration windows.

Used by `Expression::save()` and `CSVConverter::{bin_to_csv, csv_to_bin}` — single source of truth for `.qvr` traversal.

### Write Registry

In-process path registry prevents reading files that are currently open for writing. A `static std::unordered_set<std::string>` in `binary_file.cpp` tracks canonical paths of files open in write mode. Opening a reader or a second writer on a registered path throws `std::runtime_error`. The registry entry is removed in `BinaryFile::close()` (after flush; the destructor closes). Move semantics work correctly: moved-from objects have null `impl_` and skip unregistration. Not thread-safe or multi-process safe.

### Performance Bottlenecks

Profiled with 480×500×31 dimensions (~7.3M read/write calls). Main hot-path costs:

1. **`unordered_map<string, int64_t>` dims parameter (~40% of total time):** Every read/write constructs a hash map with string keys. Hashing, heap allocation for strings/buckets, and `find()` lookups dominate. Indexed `vector<int64_t>` overloads were prototyped and dropped in favor of API simplicity — the map-based form is the single supported entry point. Hot-path consumers (e.g., `ExpressionFile::compute_row`, `Expression::save`) cache the `unordered_map` across calls to amortize the allocation. Do not reintroduce indexed overloads without revisiting that decision.
2. **`validate_dimension_values` (~19% of total time):** Called on every read/write. Checks dimension count, name existence, bounds, and time-dimension consistency (date arithmetic via `add_offset_from_int`/`datetime_to_int`). Could be made opt-in or skippable when callers guarantee correct values (e.g., when iterating via `next_dimensions()`).
3. **`vector<double>` allocation per read (~3%):** Each `read()` allocates a new vector. A `read_into(buffer, dims)` overload writing into caller-provided storage would eliminate this.

## Expression Subsystem

Lazy expressions over `.qvr` binary files. Build a DAG using `+ - * /` operator overloads (binary and unary minus) and unary math free functions, materialize via `save()`. Bound in **Julia and Lua** (root design decision); Lua binds these C++ classes directly via sol2 in `src/lua_runner.cpp` (`quiver.*` namespace + method syntax + string aggregation ops; `expr:save` paths are sandboxed to the database directory).

```cpp
auto a = BinaryFile::open_file("a", 'r');
auto b = BinaryFile::open_file("b", 'r');
Expression result = abs((a + b) * 2.0 - sqrt(Expression(a)));
result.save("output");  // writes output.qvr + output.toml
```

- `Expression` value type (header `quiver/expression/expression.h`):
  - Constructors: `Expression(const BinaryFile&)` (implicit, enables `bf_a + bf_b`), `Expression(shared_ptr<ExpressionNode>)`
  - Accessors: `metadata()`
  - Materialize: `save(path)` — iterates via `first_dimensions`/`next_dimensions`, calls `compute_row()` per cell, writes to a new `.qvr`. Throws if `path` collides (after `weakly_canonical`) with any input file in the DAG.
  - Aggregation: `aggregate(dimension, op, [parameter])` collapses a dimension; `aggregate_agents(op, [parameter])` collapses the label axis. `op` is the nested enum `ExpressionAggregate::Operation` (for `aggregate`) or `ExpressionAggregateAgents::Operation` (for `aggregate_agents`), each with `Sum / Mean / Min / Max / Percentile`. `Percentile` requires a `parameter` fraction in `[0, 1]`; nullary ops reject `parameter`.
  - Label-axis projection: `select_agents(labels)` keeps (and may reorder) a chosen subset of operand labels; `rename_agents(mapping)` rewrites labels in place via a partial `{old: new}` map. Both validate eagerly: `select_agents` throws if any requested label is absent; `rename_agents` throws on duplicate keys or unknown keys, and `BinaryMetadata::validate()` rejects renames that produce duplicate output labels.
- Operator overloads (12 binary + 1 unary): `+ - * /` × {expr+expr, expr+double, double+expr}, plus unary `-expr`.
- Free functions in `quiver::` for unary math: `abs(expr)`, `sqrt(expr)`, `log(expr)`, `exp(expr)`.
- Comparison operators in `quiver::` (C++): `> < >= <= == !=`, each defined for all three combos {expr,expr | expr,double | double,expr} (explicit — the compiler does not synthesize C++20 reversed candidates for these non-bool-returning operators). `==`/`!=` return an elementwise mask `Expression`, not `bool` (Eigen-style). Produce `1.0`/`0.0` per element; **a NaN operand propagates as NaN** (so `ifelse(cmp, …)` yields NaN). They reuse `ExpressionBinary`, inheriting unit-match + shape validation and carrying the broadcast unit. **Per-language surface**: C++ uses the operators; Julia overloads `> < >= <=` and keeps `eq`/`neq` named (`==`/`!=` would break `Dict`/`Set`); Lua keeps `quiver.gt/lt/gte/lte/eq/neq` free functions (comparison metamethods coerce to bool).
- Logical operators in `quiver::` (C++) on nonzero-is-true operands: `operator&&` / `operator||` (binary, three combos each) and `operator!` (unary). Produce `1.0`/`0.0`, **NaN propagates**, result is **unitless** — `&&`/`||` skip unit-match validation (only shapes must broadcast) so conditions on different-unit variables compose; `!` emits a unitless result too. Overloading `&&`/`||` drops short-circuit, which is irrelevant for a lazy DAG. **Per-language surface**: C++ `&& || !`; Julia `& | !` (`&&`/`||` are non-overloadable short-circuit syntax, so `&`/`|`; `!` is a real function); Lua `& | ~` (`and`/`or`/`not` are keywords → `__band`/`__bor`/`__bnot` metamethods on the Expression and BinaryFile usertypes).
- Free function `ifelse(cond, then_value, else_value)` selects per-element: NaN cond → NaN; `cond != 0` → `then_value`; else → `else_value`. `then` and `else` units must match; `cond`'s unit is ignored.
- `ExpressionNode` hierarchy (header `quiver/expression/expression_node.h`):
  - `ExpressionNode` (abstract): `metadata()`, `compute_row(dims, out)`, `collect_input_files(out)` (used by `save()` for the output-path collision check and input open/close lifecycle)
  - `ExpressionFile`: lazy reads from a `.qvr`. Caches an open `BinaryFile` and a reusable `unordered_map` across calls (mutable members; not thread-safe per instance).
  - `ExpressionScalar`: broadcasts a constant across the operand's label space.
  - `ExpressionBinary`: combines two operands with `ExpressionBinary::Operation::{Add,Subtract,Multiply,Divide,Gt,Lt,Gte,Lte,Eq,Neq,And,Or}` (nested enum). Arithmetic ops compute `lhs op rhs`; the six comparisons and the two logical ops (`And`/`Or`, nonzero-is-true) return `1.0`/`0.0` and propagate a NaN operand as NaN. Logical ops skip unit-match validation and emit a unitless result (a small `is_logical(op)` branch in the constructor); comparisons/arithmetic keep the full unit-match check. Constructor pre-computes broadcast metadata (`build_broadcast_metadata`) and one `BroadcastOperand` per operand (index translation tables + reusable buffers, built by `make_broadcast_operand` and driven per row by `compute_broadcast_operand_row` — both shared with `ExpressionTernary` via `expression_helpers.h`). The `apply(Operation, double, double)` operation-dispatch is a private static member.
  - `ExpressionUnary`: applies a single-operand function with `ExpressionUnary::Operation::{Negate,Abs,Sqrt,Log,Exp,Not}` (nested enum). For the math ops `metadata()` returns the operand's metadata unchanged (no dimensional analysis — `sqrt(MW)` stays as `MW`); `Not` is logical negation (nonzero→0, 0→1, NaN propagates) and returns a **unitless** boolean via a dedicated `output_meta_` member. Constructor pre-allocates a reusable `operand_row_buf_`. Lets IEEE-754 NaN/inf propagate naturally (`sqrt(-1) → NaN`, `log(0) → -inf`); no NaN special-casing. The `apply(Operation, double)` operation-dispatch is a private static member.
  - `ExpressionTernary`: selects per-element across three operands. `Operation::{IfElse}` (nested enum). For `IfElse`: NaN in `condition` → NaN; `condition != 0` → `then_value`; else `else_value`. Constructor eagerly validates (`then` and `else` units must match; `condition`'s unit is ignored; shapes broadcast across all three pairs), pre-builds broadcast metadata via `build_ternary_broadcast_metadata` and one `BroadcastOperand` per operand (same shared machinery as `ExpressionBinary`). The `apply(Operation, double, double, double)` operation-dispatch is a private static member.
  - `ExpressionAggregate`: collapses a named dimension. `Operation::{Sum,Mean,Min,Max,Percentile}` (nested enum). Constructor eagerly removes the dim from output metadata, rewires child time-dim `parent_dimension_index` transitively (a time dim whose parent was removed re-points to the removed dim's grandparent, or `-1`), and pre-allocates index translation + reusable buffers. Skips NaN inputs during accumulation; all-NaN range yields NaN.
  - `ExpressionAggregateAgents`: collapses the label axis to a single entry named after the operation (e.g., `"sum"`, `"mean"`, `"percentile"`). Dimensions, `initial_datetime`, `unit` unchanged. Same NaN policy as `ExpressionAggregate`. Shares the accumulation templates in `expression_helpers.h` with `ExpressionAggregate`.
  - `ExpressionSelectAgents`: projects the operand onto a caller-supplied label list. Constructor pre-computes a `selected_indices_` table from operand-label → output-position, copies operand metadata with `labels` replaced, and calls `output_meta_.validate()` (which rejects duplicate output labels). Missing labels throw `"Cannot select_agents: label not found: '<name>'"`. `compute_row` reads the operand row into a reusable buffer and gathers selected columns into `out`.
  - `ExpressionRenameAgents`: rewrites operand labels via a partial `{old: new}` mapping. Constructor builds a rename map (duplicate keys throw), walks operand labels swapping matched names, verifies every key was used (unmatched keys throw), and calls `output_meta_.validate()` (rejects collisions like `val1→val2` when `val2` already exists). `compute_row` forwards directly to the operand — count and order are unchanged so no per-row reshuffle is needed.
- Validation is **eager** at construction for `ExpressionBinary`, `ExpressionTernary`, `ExpressionAggregate`, `ExpressionAggregateAgents`, `ExpressionSelectAgents`, `ExpressionRenameAgents` (units/dim sizes/time-dim properties/label sizes/initial datetimes for binary and ternary; dim existence + op/parameter consistency + output metadata validity for aggregations; label existence + uniqueness for label-axis projections). `ExpressionUnary` has no inputs to cross-validate so its constructor just sizes the row buffer. Computation is **lazy**: no I/O until `save()`.
- All operation enums are nested in their owning class: `ExpressionBinary::Operation`, `ExpressionUnary::Operation`, `ExpressionTernary::Operation`, `ExpressionAggregate::Operation`, `ExpressionAggregateAgents::Operation`. The two aggregation enums are parallel types with identical values (`Sum / Mean / Min / Max / Percentile`). Label-axis projection nodes (`ExpressionSelectAgents`, `ExpressionRenameAgents`) have no operation enum — their behavior is fully specified by the label list / rename map. The C API mirrors this with five parallel enums: `quiver_expression_operation_t` (now `ADD..DIVIDE`, the comparisons `GT/LT/GTE/LTE/EQ/NEQ`, and the logical `AND/OR`), `quiver_expression_unary_operation_t` (math ops plus `NOT`), `quiver_expression_ternary_operation_t`, `quiver_expression_aggregate_operation_t`, `quiver_expression_aggregate_agents_operation_t`. Comparisons and logical ops reuse the `quiver_expression_apply*` / `quiver_expression_apply_unary` entry points (no new C functions); the Julia FFI enum (`src/c_api.jl`) must carry the same values.
