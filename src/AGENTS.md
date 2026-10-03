# C++ Core (`src/` + `include/quiver/`)

This file covers the C++ library: public headers in `include/quiver/**` and implementation in
`src/**`, including the Lua, binary, and expression subsystems. The C API (`src/c/`,
`include/quiver/c/`) has its own `src/c/AGENTS.md`. Cross-cutting rules (naming, error message
patterns, schema conventions, design decisions) live in the root `AGENTS.md`.

## File Map

```
include/quiver/           # C++ public headers
  database.h              # Database class - main API
  attribute_metadata.h    # ScalarMetadata, GroupMetadata types
  options.h               # DatabaseOptions, CSVOptions types and factories
  element.h               # Element builder for create operations
  lua_runner.h            # Lua scripting support
  value.h                 # Value variant (nullptr/int64/double/string)
  data_type.h             # DataType enum, data_type_to_string, is_date_time_column
  row.h / result.h        # Row and Result query-result types
  migration.h / migrations.h  # Migration (version dir, up/down sql) and Migrations discovery
  export.h / quiver.h     # Export macro, umbrella header
include/quiver/binary/      # Binary subsystem headers (binary file I/O)
  binary_file.h               # BinaryFile class (Pimpl) - open_file, read, write, get_metadata
  csv_converter.h             # CSVConverter class - bin_to_csv, csv_to_bin
  iteration.h                 # first_dimensions, next_dimensions, dimension_sizes_at_values, dimension_start_at_values
  binary_metadata.h           # BinaryMetadata struct - dimensions, labels, serialization
  dimension.h                 # Dimension struct (name, size, optional TimeProperties)
  time_properties.h           # TimeFrequency enum, TimeProperties struct
  time_constants.h            # Time dimension size constraints
include/quiver/expression/  # Expression subsystem headers (lazy expressions on .qvr files)
  expression.h                # Expression value type, + - * / operator overloads, save engine
  expression_node.h           # ExpressionNode base + concrete node classes + BroadcastOperand
src/                      # C++ implementation
  database.cpp            # Lifecycle, factories, transactions, execute, migrate_up
  database_impl.h         # Database::Impl - lazy schema load, label + FK resolution, group inserts, TransactionGuard
  database_internal.h     # internal:: helpers - read templates, value_matches_type, metadata converters, time-series dimension lookup
  database_create.cpp / database_read.cpp / database_update.cpp / database_delete.cpp
  database_metadata.cpp / database_query.cpp / database_time_series.cpp / database_describe.cpp
  database_csv_export.cpp / database_csv_import.cpp
  schema.h / schema.cpp   # Schema/TableDefinition introspection (from_database), table classification,
                          # group_names, group table name helpers
  schema_validator.h / schema_validator.cpp  # SchemaValidator - schema convention checks
  type_validator.h / type_validator.cpp      # Scalar/array type validation (free functions,
                                             # caller-threaded Pattern 1 messages)
  element.cpp / row.cpp / result.cpp / migration.cpp / migrations.cpp
  lua_runner/             # LuaRunner (sol2): every Lua binding, one file per domain
    lua_runner.cpp        # LuaRunner::Impl (ctor order, the one Database usertype), RunHandles bodies, run()/GcGuard
    internal.h            # quiver::lua_internal: RunHandles, binder decls, converters, read adapters, option walk, group-decoder decls
    return_json.cpp       # run()'s JSON encoder
    path_policy.cpp       # resolve_sandboxed_path, the single filesystem gate
    db_core.cpp           # bind_core: info, transactions, dry runs, count, describe, query, migrations, export/import_csv
    db_read.cpp           # bind_read: bulk + by-id readers
    db_write.cpp          # bind_write: element CRUD, relations, vector/set group writers; table_to_element, group decoder
    db_metadata.cpp       # bind_metadata: get_*_metadata, list_* groups
    db_time_series.cpp    # bind_time_series: time-series read/write/upsert, time-series files
    csv.cpp               # bind_csv: read_csv, read_csv_stream, write_csv, CsvWriter
    binary.cpp            # bind_binary: BinaryMetadata/BinaryFile/Expression, quiver.*, open_file/bin_to_csv/csv_to_bin
  ui_metadata.h / ui_metadata.cpp  # Internal ui/ TOML sidecar reader behind describe/describe_collection
                                # -- same no-include/quiver/-counterpart posture as csv_read
  cli/main.cpp            # quiver_cli CLI entry point
  utils/string.h          # String utilities: new_c_str, trim
  utils/datetime.h        # ISO 8601 parse/format helpers
  utils/number.h          # quiver::utils::append_number (std::to_chars shortest round-trip) and
                          # parse_float (its whole-cell, host-locale-proof reader)
src/csv/                    # Standalone CSV file reader/writer (see below)
  csv_read.h / csv_read.cpp   # Internal CSV reader (csv-parser, Pimpl'd) behind db:read_csv /
                              # db:read_csv_stream and import_csv -- no include/quiver/ counterpart
  csv_write.h / csv_write.cpp # Internal CSV writer (hand-rolled, NOT Pimpl'd) behind db:write_csv;
                              # its append_record also emits export_csv -- same posture as csv_read
src/binary/                 # Binary C++ implementation
  binary_file.cpp             # BinaryFile class (Pimpl impl) + write registry
  binary_utils.h              # Shared file-extension constants, day_of_year, position_in_parent
  csv_converter.cpp           # CSVConverter implementation
  iteration.cpp               # first_dimensions/next_dimensions impls + dimension_sizes_at_values/dimension_start_at_values
  binary_metadata.cpp         # BinaryMetadata factories, serialization, validation
  time_properties.cpp         # TimeFrequency string conversion, add_offset_from_int
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

`src/csv/` is grouped by format, not by consumer: its classes never see a sol2 type, and their
callers are Lua (`db:read_csv*`, `db:write_csv`) and `Database::import_csv` / `export_csv` alike. It holds only the
standalone reader/writer — `database_csv_{import,export}.cpp` stay with the `database_*` family,
parsing through `csv_read::Reader` and emitting through `csv_write::append_record` (root design
decision "One CSV parser, one CSV emitter"), and `binary/csv_converter.cpp` stays with `binary/`. A future format helper (e.g. a Lua JSON reader)
gets a sibling folder (`src/json/`). The `run()` JSON encoder now lives in
`src/lua_runner/return_json.cpp`.

`csv/csv_read.h`/`.cpp` was the first `.cpp` in `src/` with no `include/quiver/` public
counterpart — the header-only internal helpers here (`utils/string.h`, `database_internal.h`,
`binary/binary_utils.h`) have no `.cpp` at all. `csv/csv_write.cpp` and `ui_metadata.cpp`
(below) share csv_read's posture, and so do `schema.cpp`, `schema_validator.cpp` and
`type_validator.cpp` since their headers moved into `src/`. It stays
internal because its public surface is already bound: `import_csv`
parses through it, and the only other caller is Lua, which needs it because `io` is deliberately
absent (Julia/Dart/Python/JS already have native CSV libraries), so the root AGENTS.md rule "bind
every public method down to every binding" never fires — no documented exception needed. Import
passes its one unsandboxed path as both `resolved_path` and `original_path`, with `"import_csv"` as
the operation (from Lua it arrives already sandbox-resolved, so those errors quote the absolute
path). `Reader` is Pimpl'd specifically so csv-parser's headers never have
to be included by any `src/lua_runner/` TU (all are sol2 TUs; `/bigobj` is target-wide for sol2's template
depth). Three `csv::CSVFormat` settings are pinned in exactly one place (`make_format`, in
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
also yields an empty header by design. csv-parser's quote rules are not configurable at all, yet
import's `require_well_formed_quotes` (`database_csv_import.cpp`) hand-copies them to guard its
DELETE; `LuaRunner_ReadCsv.StrayQuotesTokenizeAsTheImportPrePassAssumes` pins the parser side, so
re-check both on any csv-parser `GIT_TAG` bump.

`csv/csv_write.h`/`.cpp` is `csv_read`'s deliberate non-Pimpl counterpart (D-37): it depends
on nothing that must be kept out of the sol2 translation unit (no csv-parser, no third-party
headers), so hiding its `std::ofstream` member behind a Pimpl the way `Reader` hides csv-parser
would be cargo cult. `Writer` backs `db:write_csv`; the free `append_record` it emits through is
also `export_csv`'s emitter (`database_csv_export.cpp` builds the whole file with it, then writes
it in one shot, so `Writer`'s truncate-at-open and its `Cannot write_csv` messages stay out of
export). Same no-`include/quiver/`-header, no-`QUIVER_API`, no-C-API posture as `csv_read`. Numeric cell formatting reuses
`quiver::utils::append_number` (`src/utils/number.h`) via `std::to_chars`'s shortest round-trip
form with no synthetic decimal point, so a whole float and the equal integer write identical text
(D-34); a `nil` cell and an empty-string cell are structurally indistinguishable after a CSV round
trip and that is stated, not fixed — CSV has no null (D-40). FMT-07's row-width enforcement (a
short `write_row` pads to the header's length, a long one throws) lives entirely in the Lua-layer
`CsvWriter::write_row` in `src/lua_runner/csv.cpp`, not here: this file's `Writer` gained no header-width
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
same idiom `src/lua_runner/path_policy.cpp`'s `resolve_sandboxed_path` already uses).

A `ui/*.toml` collection file self-selects by shape, never by filename: a top-level string `id`
plus an `attribute` array are both required, which is what excludes `main.toml` (no `id`), every
theme file (`id` but no `attribute` array), and any non-`.toml` file, with the scan staying
non-recursive so a `themes/` subdirectory is never walked into. `enum.toml` has no wrapper key of
its own — every one of its top-level keys IS a vocabulary name, discovered by iterating the whole
top-level table rather than reading one fixed array key, and an attribute joins a vocabulary by
its own `enum` value, never its `id` (several attributes commonly share one vocabulary, e.g.
`bool`).

The whole load is a **nested try/catch that warns and degrades, and never throws** — explicitly
not `src/binary/binary_metadata.cpp`'s posture, which throws on a parse error, a missing or
non-string `version`/`unit`/`initial_datetime`, a non-array value, or a wrong-typed array entry
(an absent array still reads as empty). One outer catch covers directory iteration and path
resolution and yields a fully empty `UiMetadata` on failure; one inner catch per collection file (and
a separate one around `enum.toml`) means a single malformed `ui/*.toml` costs only that
collection's metadata, not every other collection's. An absent or empty `ui/` directory is the
ordinary case and logs nothing — an empty directory yields zero `directory_iterator` entries, so
the per-file loop body never runs. A directory that cannot be iterated (path resolution or the
iteration itself failing) logs one warning from the outer catch and degrades to an empty
`UiMetadata`; a file that cannot be read or parsed logs its own warning through the per-database
logger and costs only that file — `from_migrations` still succeeds in every case. Both passes go
through one `parse_toml_file(path)` helper so their failure behaviour cannot drift: it throws
Pattern 3 on a failed open **and on a failed read** (`ifstream::bad()` after the slurp), the second
because a truncated TOML prefix is usually still syntactically valid, so a half-read sidecar would
otherwise have parsed as a complete one and silently lost every attribute past the cut. Two things
in that loop are deliberately spelled the way they are: `directory_entry::is_regular_file` takes
the **`std::error_code` overload**, because that test sits outside the per-file `try` and the
throwing overload would send one unstattable entry into the *outer* catch, discarding every
collection already parsed; and a second file declaring an `id` some earlier file already claimed
logs a `Duplicate UI metadata for collection` warning before replacing it, since selection is by
shape rather than filename and `directory_iterator` order is unspecified — last-in still wins, but
the collision is diagnosable instead of varying silently by filesystem.

Rendering lives in `database_describe.cpp`, not here: `write_collection_section` gained a
`const UiMetadata&` and a `bool with_tooltip` parameter and appends zero to three `"; keyword body"`
clauses (`label`, `enum`, `tooltip`, in that fixed order) after each scalar's existing
name/type/flags line — `describe()` passes `with_tooltip = false`, `describe_collection()` passes
`true`, so `describe()`'s line is always a prefix of `describe_collection()`'s by
construction (identical whenever the tooltip clause is suppressed, so "prefix", not "strict
prefix"). Only **main-table scalars** are annotated: `print_group_columns` takes no `UiMetadata`,
so a sidecar entry for a vector/set/time-series column is parsed and then never rendered anywhere.
A label or tooltip whose `squash` (ASCII-lowercase, digits and letters only —
spelled as an explicit ASCII test, never `std::tolower(char)`, which is undefined behavior on a
negative `char` and would be hit by real non-ASCII corpus strings) matches the attribute name's
(or, for tooltip, the label's) squash is suppressed as redundant; the enum clause is never
suppressed, since it cannot be re-derived from the column name. Both comparisons go through one
`is_redundant(a, b)` predicate that requires the squash to be **non-empty**: squash drops every
byte outside `a-z0-9`, so a symbol-only (`"%"`, `"(-)"`) or non-Latin (`"Начальный объём"`) string
squashes to `""` and used to compare equal to an *absent* label's `""` — silently deleting a
tooltip that restates nothing. That is the one direction in which squash's "drop non-ASCII" bias
suppresses rather than prints, and it is the reason the predicate exists rather than three
open-coded `squash(a) == squash(b)` tests. The raw-vs-normalized distinction D-05 once drew is
unobservable and is not spelled: `normalize_ui_text` only rewrites bytes `squash` discards anyway,
so `squash(normalize(x)) == squash(x)`. `normalize_ui_text` maps every
byte below `0x20` and `0x7F` to a space before collapsing runs and trimming (via
`quiver::string::trim`) — **and** the two-byte UTF-8 encoding of the C1 block,
`0xC2 0x80`-`0xC2 0x9F`. The C1 half is not optional: U+009B is CSI and U+009D is OSC, the 8-bit
forms of `ESC [` and `ESC ]` that xterm and the Linux console honour by default, so a C0-only
gate still let a sidecar string colour a terminal or retitle its window. Every *other* byte at
0x80 or above is a UTF-8 lead or continuation byte and is never touched, which is what keeps the
text intact — do not widen this into "drop two-byte sequences" (`ã`, `µ` and `°` all start with
the same `0xC2`/`0xC3` lead bytes). An attribute with `hide = true` in
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
count, and dropping it would destroy data. Three known limits, recorded rather than fixed: (1)
`ui_metadata` is populated only by `from_migrations` (see the early-return trap above), so
`Database::open("study.db").summarize_collection(...)` still shows bare codes — this must not be
"fixed" by hooking the load onto `load_schema_metadata` / `require_schema`, since `migrate_up`
early-returns before it on the open-an-existing-study path. Note the sharpest edge of that limit:
`from_migrations` **throws** on `options.read_only`, so a read-only handle — the natural mode for
an inspection tool, and the mode PSR's `load_study` defaults to — can never carry the sidecar at
all. Closing it needs a dedicated lazy `require_ui_metadata()` that remembers the migrations path
(orthogonal to schema loading, and not the forbidden `require_schema` hook), or persisting the
sidecar into the database file at `from_migrations` time; both are design changes, not cleanups.
(1b) group columns are never annotated — see the render paragraph above; and (2)
`kMaxDistributionCardinality`
(64) still suppresses the whole `; values {}` clause above 64 distinct codes, so the richest enum
column renders no labels at all — `describe_collection()`'s `; enum {...}` is the fallback, and 64
labelled entries is already a ~2 KB single line, so no truncation is proposed (truncation needs its
own ellipsis convention). One more honest note: `quote_ui_text` gives a parse-level guarantee, not
substring immunity, and `summarize_collection` additionally emits `  Vectors:` / `  Sets:` /
`  Time Series:` headers — so a naive header-count test written against it on a `from_migrations`
database would be breakable by a label containing that text. The remedy, if it ever bites, is
asserting on report structure (line prefix + indentation), never a substring blocklist.

Three guards in the Lua layer's decoders (`src/lua_runner/`: `csv.cpp`, `internal.h`, `db_write.cpp`) exist because a script is
untrusted input, in the same spirit as the JSON encoder's two caps below:
- `csv_max_integer_key` is the single max-integer-key walk behind both `csv_row_cells_from_lua`
  and `csv_header_from_lua` (so the key rule and its message live once), and it caps the result at
  1,000,000. Both callers materialize a **dense** vector up to that key, so `{[1e9] = "x"}` — the
  same sparseness hazard the encoder note below names — allocated tens of gigabytes, or reached
  the script as a raw `std::bad_alloc` with no Pattern 1 prefix.
- `option_entries` (which also owns the options-must-be-a-table check; what nil means stays with
  each caller) and `collect_group_columns` check each key's Lua *type* before converting it.
  sol2's `std::string` getter is `lua_tolstring`, which answers `nullptr` for a
  boolean/table/function key and spells a number key as text: unchecked in Release
  (`SOL_SAFE_GETTER` is off there) and a raw sol2 panic in Debug, so `{ [true] = 1 }` reached the
  script as a bare Lua value rather than a message. For the six group writers the check makes an
  array of row tables (`{ { date_time = ... } }`) throw one Pattern 1 message in every build,
  where Release used to report a misleading `column '1' must be an array of values`.
- `csv_separator_from_lua` rejects `"`, CR, LF and NUL in addition to the multi-byte check. They
  are one byte but cannot be delimiters: csv-parser refuses a delimiter that overlaps its quote
  character, so `db:write_csv` with `separator = '"'` silently produced a file `db:read_csv`
  could not open.
Every table argument of a bound function is a `sol::object` checked by `require_table`
(`internal.h`) in the decoder that first walks it, never a typed `sol::table` parameter: sol2's
check for one is loose (it accepts a **userdata**, which iterates as no keys, so
`w:write_row(db)` once appended an empty record and a userdata group payload cleared the group)
and, in Release, absent, so a number or string reached `lua_next` unchecked. The message names the
operation, the argument and its Lua type (`Cannot write: data must be a table, got number`).
`db:read_csv_stream`'s `on_row` stays the model for the function case: a `sol::object` with an
explicit `sol::type::function` check rather than a typed `sol::protected_function` parameter,
whose check surfaced sol2's own "stack index 3, expected function" text.

`RunHandles::open_writers` (`src/lua_runner/internal.h`) is also the concurrency guard: it records each writer's **resolved** path, and
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

Internally, `Impl::TransactionGuard` is nest-aware RAII: if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`), the guard becomes a no-op. This allows write methods (`create_element`, etc.) to work both standalone and inside explicit transactions without double-beginning. A no-op guard cannot roll anything back, so every writer that can run inside a caller's transaction finishes its checks before its first write (see "Group writes" under Core Internals). Only a failure SQLite alone detects mid-write (UNIQUE / NOT NULL / CHECK / foreign key) can leave a call's earlier writes inside a caller-owned transaction. That is a documented limit, and there are no SAVEPOINTs (root design decisions).

```cpp
// Internal RAII guard (nest-aware)
{
    TransactionGuard guard(impl);
    // operations...
    guard.commit();
}
```

Every internal write that owns a transaction (`create_element`, `update_element`, the group and
time-series writers, the migrations, `apply_schema`, `import_csv`) uses `Impl::TransactionGuard`.
Where the writer has a handler (the migrations, `apply_schema`, `import_csv`), the guard is
declared inside the `try`, so the rollback runs before the `catch`. `Impl::execute_raw(sql, what)`
is the one `sqlite3_exec` runner (`BEGIN`, `COMMIT`, `PRAGMA user_version`, multi-statement
scripts). Two calls keep their own: `Impl::rollback`, which logs instead of throwing, and the
constructor's unchecked `PRAGMA foreign_keys = ON`.

**Dry runs** (`begin_dry_run` / `end_dry_run` / `in_dry_run`, `Impl::dry_run` flag) sit on top of
the same machinery: `begin_dry_run` opens a real transaction and sets the flag; `end_dry_run`
clears the flag and calls `impl_->rollback()` **directly** — the public `rollback()` is a no-op
while the flag is set, so going through it would silently do nothing. The public
`begin_transaction`/`commit`/`rollback` each start with a one-line `if (impl_->dry_run) return;`.
`TransactionGuard` needs no flag awareness: it checks `sqlite3_get_autocommit` and already no-ops
because the dry run holds a real transaction. `end_dry_run` guards its rollback on
`sqlite3_get_autocommit` because a caller can end the transaction out from under it with a bare
`COMMIT` through `query_*`. Rationale and the documented consequences: root design decisions.

The one write path that cannot nest is `import_csv`: it must own its transaction, because inside a
caller's its `TransactionGuard` would no-op, and a failure partway through would leave import's
earlier writes (the DELETEs included) in the caller's transaction for its commit. It throws
`"Cannot import_csv: transaction already active"` as a precondition instead. (It used to open a
raw `BEGIN`, and before that to toggle `PRAGMA foreign_keys`, a no-op mid-transaction, which was
the original reason. Whether it should nest instead is an open decision.)

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
execution helpers remain private. Their errors name the public caller, not the helper:
`migrate_up` takes it as `operation` (`from_migrations` or `validate_migrations`), while
`migrate_down` and `apply_schema` write their only caller's name in directly.

## Logging

spdlog with debug/info/warn/error levels, through a **per-database logger instance** — never the
`spdlog::` global functions. `create_database_logger` (`database.cpp`) builds a uniquely named
logger (`quiver_database_<id>`) with stderr + file sinks, stored as `Impl::logger`:
```cpp
impl_->logger->debug("Opening database: {}", path);
```

## Core Internals Worth Knowing

- **Group writes are unified, and checked before anything is written** (`database_impl.h`): one
  `validate_group_columns(caller, table, type, columns)` checks types and equal lengths, and one
  `insert_rows_into_group_table(table, type, columns, id, delete_existing)` does the DELETE and
  INSERTs, for vector, set and time-series tables alike. For `create_element` / `update_element`,
  `prepare_group_data` routes each array to its table(s) through a single
  `table_name -> GroupColumns` map, FK-resolves it against **each** table it is written to (a
  shared FK column name may target a different collection per group), and validates every table;
  the caller runs it **before** its scalar INSERT/UPDATE and hands the result to
  `insert_group_data` afterwards. `update_group_rows` calls `validate_group_columns` before its
  guard the same way. Don't re-grow per-type copies, and don't move a check back past the first
  write: `TransactionGuard` no-ops when a transaction is already open (dry run, caller-owned), so a
  throw after any write leaves it in place for the caller's commit. What remains is only what
  SQLite checks at INSERT time (UNIQUE, NOT NULL, CHECK, foreign keys) - documented, not fixed (root
  design decisions). `num_rows` is seeded from the **first** column rather than the first
  non-empty one, because `columns` is name-sorted and an empty alphabetically-first column
  otherwise left it at 0 for a later column to overwrite — skipping the check and indexing past
  the end of the empty vector.
- **`Impl::update_group_rows`** (`database_update.cpp`) is the shared body of
  `update_vector_group`/`update_set_group`. It validates the **union of every row's keys** (not
  `rows[0]`, which dropped later-row-only columns and skipped validating them) against the group
  table, rejects the derived `id`/`vector_index`, and calls `require_element` — all before
  `transpose_group_rows`, so a named-but-empty column list cannot fall through to the DELETE.
  `update_time_series_group` (`database_time_series.cpp`) builds its INSERT column list from the
  same union (minus the primary dimension column, bound first); it used to take only the first
  row's keys and silently drop a value column that a later row named.
- **`Impl::require_element`** (`database_impl.h`) is the single `SELECT 1 ... WHERE id = ?` guard
  behind the Pattern 2 `"Element not found: ..."` message, shared by `update_element`,
  `delete_element`, and both group writers.
- **`import_csv`'s scalar path keeps foreign keys ON** (`database_csv_import.cpp`) and runs, in one
  transaction and in this order, every step load-bearing: (1) set every self-FK column to NULL, so
  that (2) deleting each existing element whose label the CSV omits (`DELETE ... WHERE id = ?`,
  firing CASCADE / SET NULL exactly as `delete_element`) cannot cascade through a stale
  `ON DELETE CASCADE` self-reference into an element the CSV keeps; (3) check, with one
  `read_element_ids`, that every kept element survived the deletes, and otherwise throw
  `Cannot import_csv: Deleting the elements the CSV omits would also delete element '<label>'
  through an ON DELETE CASCADE chain.` — a CASCADE cycle through another collection can still
  reach one, and the upsert would silently re-insert it by its preserved id without its group
  rows; (4) write every CSV row with `INSERT ... ON CONFLICT(id) DO UPDATE SET col = excluded.col`,
  binding the preserved id (NULL for a new label), so a kept element is updated in place and keeps
  its group rows and inbound relations; (5) the self-FK label pass. Never `INSERT OR REPLACE`: its
  implicit delete fires the ON DELETE actions and wipes the group rows of the very elements being
  kept. Clearing *every* FK column in (1), rather than refusing in (3), was implemented and
  reverted: blanking a relation to another collection on every row breaks any `CHECK` that
  involves it (`kind = 0 OR bus_id IS NOT NULL`), even for an unchanged re-import, while the
  upsert writes each row whole. A kept row whose CSV cell names an element the deletions cascade
  away fails the import on its foreign key. Deleting before writing also frees an omitted
  element's values in any other `UNIQUE` column; handing such a value from a kept element to a row
  written before it (any swap does) still fails (row-by-row UPDATEs) and rolls the import back,
  except in a self-FK column, which (1) cleared. A repeated label is rejected in the conversion
  pass, since the upsert would otherwise let the last row win silently. The group path needs
  nothing special: it deletes and re-inserts one group table whose ids and FK cells are all
  resolved to existing elements. A time-series group still needs its date dimension
  (`find_dimension_column`, as export and the readers do), which is also what refuses group
  `files`: that name is the `_time_series_files` table, and a header-only CSV would clear it. Both
  paths then share one write tail, with one transaction and one catch.
- **Label→id resolution has one query** (`database_impl.h`): `Impl::lookup_id_by_label(table,
  label)` is the only `SELECT id ... WHERE label = ?`, shared by `Impl::resolve_label`
  (Pattern 2, backs every `_by_label` form) and `Impl::resolve_fk_label` (a miss is Pattern 3) — the two
  report a miss differently, so the throw stays with each caller. `resolve_label` calls
  `require_column(collection, "label")` because `require_collection` only checks `has_table`, so a
  group table would otherwise reach the SELECT and leak a raw `no such column: label` prepare
  error; it takes the operation name so the message reports the caller's public method, not the
  delegate's.
- **Table classification has one source** (`schema.cpp`): `Schema::group_names(collection,
  GroupTableType)` and `is_group_table(table, type)` are the only way to enumerate/classify
  `_vector_` / `_set_` / `_time_series_` tables (`group_names` excludes `_time_series_files`),
  and one name builder, `Schema::group_table_name(collection, group, GroupTableType)`; every
  group-addressed operation resolves its table through `Impl::require_group_table`, which owns the
  Pattern 2 miss (`{Vector|Set|Time series} group not found: 'g' in collection 'c'`).
  All list/metadata/describe call sites use them — never hand-roll prefix scans. `group_names` returns an empty
  list for a name that is not a table, so each `list_{vector,set,time_series}_groups` calls
  `Impl::require_collection` first (as `list_scalar_attributes` does); without it a mistyped collection is
  indistinguishable from one that has no groups.
- **Declaration order everywhere**: metadata and list functions iterate `column_order`
  (declaration order), matching the `describe(ostream&)` dump and CSV export. Nothing reports alphabetical order.
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
  + per-group empty/non-empty counts) all build an `std::ostringstream` and return `std::string`. They
  run their SQL through `Impl::execute`, which is const. The distribution counts only cells whose
  `typeof` is `integer`, since a non-STRICT INTEGER column can also hold TEXT/REAL. All three are
  bound 1:1 across the C API and every binding as string getters.
- **`validate_scalar`/`validate_array` thread the caller's name** (`type_validator.cpp`): call sites pass
  `"create_element"` / `"update_element"` so messages read `"Cannot create_element: type
  mismatch for column ..."` (root Pattern 1).
- **One scalar typing policy** shared by `value_matches_type` (`database_internal.h`, time-series
  writes) and `validate_value` (`type_validator.cpp`, scalar create/update): int64
  matches `INTEGER` or `REAL` (int-for-REAL coercion), double matches `REAL` only (a float into an
  `INTEGER` column is rejected), string matches `TEXT` / `DATE_TIME`. `validate_value`
  *calls* `value_matches_type`, so the rule lives in one function. An FK label string never reaches
  `validate_value`: `Impl::resolve_fk_label` turns it into an id first, and rejects a string on a
  non-FK INTEGER column itself (Pattern 1, naming the caller). The time-series writers
  (`update_time_series_group` / `upsert_time_series_row`) resolve no labels, so a label there reaches
  `value_matches_type` and is rejected. `import_csv` is the third enforcer, on CSV text: its
  `parse_integer` (`database_csv_import.cpp`) and `utils::parse_float` (`utils/number.h`, shared
  with `csv_to_bin`) take a cell only if it parses whole, so a policy change must reach them too.
- **DATE_TIME content is checked by both halves of that policy, through one predicate**:
  `datetime::is_valid_iso8601` (`utils/datetime.h`). Both halves call it in a separate guard right
  after the `value_matches_type` shape check: `validate_value` (covering scalar
  create/update and every vector/set array write, so it inherits the check-before-first-write
  ordering of the "Group writes" bullet above) and `validate_time_series_row`
  (`database_time_series.cpp`). Do not fold the check into `value_matches_type`: that function decides the
  *variant's shape*, and TEXT into a DATE_TIME column is the correct shape — routing a content
  failure through its `bool` would emit `column 'date_time' has type DATE_TIME but received TEXT`,
  which is a lie. The two guards phrase their own messages; the rule itself lives in exactly one
  function.
  `parse_datetime_import` (`database_csv_import.cpp`) is the **third** gate and needs to exist:
  `import_csv` writes through a raw `INSERT` and never reaches `validate_value`, and its
  custom-`date_time_format` branch parses with the caller's `get_time` format, which cannot see an
  impossible calendar day (`"%d/%m/%Y"` on `31/02/2024`). It therefore runs `is_valid_iso8601` on
  the string it canonicalizes, so import is held to the same grammar as the other writers.
  Import converts every cell through one `convert_cell` (`database_csv_import.cpp`) before
  writing, so validation and the write cannot disagree. It picks a branch on the column's
  declared type alone (`query_columns` already types a TEXT `date_` column DATE_TIME), and a
  column the Schema does not list (`PRAGMA table_info` omits generated columns, which `SELECT *`
  returns) converts as nullable TEXT.
- **`update_element` / `delete_element` / the vector+set group writers verify the id exists** (via
  `Impl::require_element`) and throw Pattern 2 `"Element not found: ..."` — no silent no-op.
  The two time-series writers do not: `upsert_time_series_row` always writes one row, so a bad id
  always fails at the foreign key; `update_time_series_group` fails there too when it has rows to
  write, but with no rows it deletes nothing and returns silently.
  Every `_by_label` form resolves the label via `Impl::resolve_label`, and is a one-line
  delegation to its id counterpart (the root `_by_label` rule), so `update_element_by_label`'s
  *element* validation — the empty-element throw, `validate_scalar`, `prepare_group_data`'s
  routing/type/length checks — reports `Cannot update_element: ...` and the group/row writers'
  column validation reports `Cannot update_{vector,set,time_series}_group: ...` /
  `Cannot upsert_time_series_row: ...`, naming the operation that validated.
- **`update_relation` is a validated `update_element`** (`database_update.cpp`): the derived column
  is checked against the schema through the `TableDefinition::get_foreign_key` accessor added for
  it in `schema.cpp`, then written as a one-attribute `Element` (`std::nullopt` becomes
  `Element::set_null`). It resolves no label of its own — binding the target label to an INTEGER FK
  column is already `Impl::resolve_fk_label`'s job, and the missing-id check is `require_element`'s
  — so failures past the derivation report `Cannot update_element: ...`.

- **Schema metadata loads lazily** (`Impl::require_schema`): the `Database(path, options)`
  constructor does not read it, so the first metadata/CRUD call does. `schema` is `mutable` (const
  readers trigger the load), and `load_schema_metadata()` publishes it only after
  `SchemaValidator::validate()` passes, so a failed lazy load leaves no half-loaded state for the
  next call: `require_schema` loads only while `schema` is null, so a schema published early would
  never be validated again. Rationale in the root design decisions.
- **Every group table's parent is checked by one helper** (`schema_validator.cpp`,
  `validate_group_parent`, called from `validate()` for vector, set and time-series tables after
  their structural checks): the prefix must name an existing collection and `id` must reference
  it with ON DELETE CASCADE ON UPDATE CASCADE — `delete_element` is a bare `DELETE` on the
  collection and relies on that cascade. `validate_foreign_keys` then applies one action rule to
  every FK in every table. Sets and time series used to skip the parent check, and time series
  the action rule too, so a schema could leave orphan set rows or make `delete_element` fail on a
  time-series table with SQLite's `FOREIGN KEY constraint failed`.
- **`Row::get_float` widens an int64** (`row.cpp`): the one place the int64-for-REAL policy is
  implemented for reads, since `read_column_values_nullable<double>`,
  `read_grouped_values_all<double>`, `read_single_value<double>` and `query_float` all funnel
  through it. Don't re-add a widening branch at a call site.
- **Two column readers in `database_internal.h`**: `read_column_values<T>` drops NULLs (dense —
  used only by `read_element_ids`, whose column is the collection's PK);
  `read_column_values_nullable<T>` keeps them as `std::optional<T>` and backs the three
  `read_scalar_*` bulk readers *and* the six vector/set `_by_id` readers (one entry per element or
  per cell, `ORDER BY rowid` — except the vector `_by_id` readers, which order by `vector_index`).
  The Lua readers consume the optional vector directly via a
  `to_lua_table(vector<optional<T>>)` overload that emits `nil` holes (root NULL design decisions).
  Every per-column vector/set reader finds its table by column **name** (`Schema::find_vector_table`
  / `find_set_table`): the group named after the column only if it holds that column (a group may
  be named after another group's column, which used to throw `column not found`), else the group
  whose table name sorts first among those holding it. A name two groups share therefore reads one
  of them; `read_{vector,set}_group_by_id` take the group and are the reads that cannot be misrouted.
- **`read_grouped_values_all<T>`** (`database_internal.h`) backs the six bulk vector/set readers,
  returns `vector<vector<optional<T>>>`, and parses by position the LEFT JOIN that its neighbour
  `grouped_values_sql` builds for all six. That SELECT is `c.id, g.id, g.<attr>` — three columns,
  not two: `g.id` is a **presence column** that is NULL only when the join found no row, which is
  the one thing that keeps "element with no group rows" (empty inner vector) apart from "row whose
  value is NULL" (`nullopt` cell). Don't "simplify" the SQL back to `SELECT id, value FROM
  <group_table>` (that shape skipped elements) and don't drop `g.id` (that shape collapses the two
  NULL cases back together). The presence test is `!is_null(1)`, not `get_integer(1)`: a group
  whose `id` column is declared REAL or TEXT (the validator does not check its type) stores the id
  as 1.0 / '1', which still matches the join. The element tracker is an `optional<int64_t>`, never
  a sentinel id: an explicit id of -1 is accepted by `create_element`, and a `-1` sentinel dropped
  that element (or appended to an empty result) when it was the smallest id.
- **`scalar_metadata_from_column` reports an INTEGER PRIMARY KEY as `not_null`**
  (`database_internal.h`): a rowid-alias PK is never NULL, but SQLite's `PRAGMA table_info` leaves
  the `notnull` flag unset, so the public `ScalarMetadata.not_null` ORs in `primary_key && type ==
  Integer`. The raw `ColumnDefinition.not_null` stays the literal PRAGMA value — `csv_import`
  (empty-cell rejection) and `schema_validator` read it directly and must not see PK flip. This is
  what lets Julia's nullability-aware readers return a concrete `Vector{Int64}` for `id`.
- **`execute` validates parameter count** (`Impl::execute`, `database.cpp`):
  `sqlite3_bind_parameter_count` must equal `parameters.size()`, else it throws — the single guard
  for every `query_*` and internal parameterized statement.
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
  `Impl::execute` trims every bound string and the gate must judge the value that is stored.
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

Implementation conventions in `src/lua_runner/`:
- **Layout**: `LuaRunner::Impl`'s constructor creates the only Database usertype and hands it to
  the seven binders as `bind`, with the `quiver` table as `ns`. Those parameter names are what the
  sync test's first pass matches (it fails on a `.set_function(` through any other receiver, and
  reads subdirectories too), and a second Database usertype would clear every method bound
  before it. The non-Database usertypes stay variadic, one bound name per line (the sync test's
  second pass). `RunHandles` is an `Impl` member declared before `lua`; closures capture `handles`
  or `db` by reference, never the `Impl` pointer, so a moved runner keeps working. `internal.h`
  holds only templates, `inline` functions and declarations; every other helper is in an anonymous
  namespace nested in `quiver::lua_internal`. A type registered as a usertype stays in the named
  namespace, because sol2 keys usertypes by demangled name and that drops anonymous namespaces.
  Comments must not spell the Database usertype call or the stdlib-opening call, because greps
  count both. Each TU with by-value sol2 parameters gets one
  `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair. A file stays at about 450 lines or
  fewer.
- **Shared helpers**: each repeated binding pattern lives in one helper, and a new method reuses
  it rather than copying a body. The 17 plain forwarders are `&Database::` member pointers (below).
  `bulk_read_lua` / `collection_read_lua` (`internal.h`) adapt the bulk readers and are registered
  under the member's own name. `read_groups_by_id` (`db_read.cpp`) is behind `read_vectors_by_id`
  and `read_sets_by_id`. The `metadata_to_lua` overloads with `list_metadata_lua` /
  `get_metadata_lua` (`db_metadata.cpp`) are behind the four `get_*_metadata` and four `list_*`
  group methods.
  `query_*_lua` return `std::optional` and `read_scalars_by_id` assigns `std::optional` values, so a
  NULL is `nil` and an absent key. `run_in_scope` (`db_core.cpp`) is the one scoped block behind `db:transaction` and
  `db:dry_run`: the two lambdas pass their operation name, and `run_in_scope` checks the argument is
  a function before opening the scope.
  `collect_entries` / `option_table` / `option_entries` (`internal.h`) are the one option walk:
  `option_entries` owns the table check and returns slots that callers bind by name with a
  structured binding, and nil handling stays with each caller. `lua_to_value` is the one
  `Value`-typed write dispatch, CSV cells included (`csv_cell_to_string`); `lua_cell_as<T>` is the
  typed-array one (see the boolean bullet). `CsvWriter::write_row` / `close` are members
  registered by member pointer, and `header_object` (`csv.cpp`) is the one no-header rule for both
  read forms. `RunHandles::add_writer` / `add_binary_file` are the only appenders to the run-handle
  registries by convention (the vectors stay public; prune expired entries, then append), and `close_open_handles` empties both at
  `run()`'s exit. `binop<Op>` (`binary.cpp`) with a transparent functor (`std::plus<>`,
  `std::greater_equal<>`, ...) is every binary Expression operator, metamethods and
  `quiver.gt`/`lt`/`gte`/`lte`/`eq`/`neq` alike. `columns_to_cpp_rows` owns the group decoders'
  no-rows rejection, and `length_mismatch` (`db_time_series.cpp`) is the time-series decoder's one
  length message. `lua_type_error` / `require_table` (`internal.h`) are the one argument
  type-error shape (`Cannot <op>: <what> must be <expected>, got <lua type>`) and the one table
  check: `require_table` tests `get_type()`, never the loose `is<sol::table>()` that accepts a
  userdata, and it sits in the decoder that first walks the argument. `lua_string_key` is the
  one check for a key that names something (an attribute, a column, a dimension) before it is
  converted; the older guarded key checks (`option_entries`, `collect_group_columns`,
  `string_key`) keep their own pinned texts. `optional_from_lua<T>` is the one optional-argument
  decoder (see the optional-argument bullet below).
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
  `bindings/js/test/lua-api-sync.test.ts` parses every `.cpp`/`.h` under `src/lua_runner/` and fails otherwise. That check
  exists because the doc went stale two days after it was written: it said only
  `base`/`string`/`table` were loaded and "there is NO `math`", and #210 added
  `math`/`coroutine`/`utf8` here without touching it.
- **Every optional argument takes `sol::object`, not `sol::optional<T>`, and goes through
  `optional_from_lua<T>(object, operation, what, expected)`** (`internal.h`): `sol::optional<T>`
  yields `nullopt` for a wrong type just as it does for `nil`, so a wrong-typed `aggregate` flag
  or `params` table was silently ignored. `optional_from_lua` has `luaL_opt` semantics: nil or a
  missing argument is absent, anything else must be a `T` or throws `Cannot <op>: <what> must be
  <expected>, got <lua type>`. It backs the `query_*` params, `db:open_file`'s metadata (decoded
  after `resolve_sandboxed_path`, so the order stays mode, path, metadata), `db:bin_to_csv`'s
  aggregate, `file:read`'s allow_nulls and the `aggregate` / `aggregate_agents` parameter. Where
  the decode sat inside one call's argument list, it is hoisted into locals in argument order, so
  which bad argument wins no longer depends on the compiler. `relation_target_from_lua(object,
  caller)` is the relation-specific case, with its own text: `db:update_relation(..., false)`
  used to clear the relation silently; now nil/missing clears and a non-string throws
  `Cannot <caller>: target_label has unsupported Lua type`. Both
  `db:update_relation(..., nil)` and omitting the argument clear; that affordance is sol2's and
  Lua-only (the FFI bindings all require the parameter and take their language's null).
- `parse_csv_options(options, operation)` is the single strict CSVOptions decoder for
  `export_csv`/`import_csv`: `nil` means defaults, any other non-table and any unknown or
  wrong-typed key throws, with the same collect-then-validate walk (`option_entries`, which owns the
  table check while each caller keeps its own nil handling) as the `read_csv`/`write_csv` decoders. `quiver.metadata{...}` and `expr:rename_agents` are decoded
  the same strict way.
- `to_lua_table<T>` overloads (flat + nested) are the only vector→table marshalers.
- The plain forwarders — `is_healthy`, `current_version`, `path`, the transaction and dry-run
  methods (`begin_transaction`, `commit`, `rollback`, `in_transaction`, `begin_dry_run`,
  `end_dry_run`, `in_dry_run`), `number_of_elements`, `describe` / `describe_collection` /
  `summarize_collection`, `delete_element` / `delete_element_by_label` and `has_time_series_files`
  — are bound as `&Database::` member pointers, not lambdas. `db:describe()` and its siblings still
  return the C++ `std::string` text report — they do not print.
- Lua→C++ converters **throw on unsupported value types** (functions, nested tables, ...) — never
  skip silently; a skipped positional query parameter would shift the rest and bind NULL to the
  trailing placeholder.
- **A Lua boolean is INTEGER 1/0 on every write path**, matching the cross-layer policy in the root
  `AGENTS.md`. Every boolean test goes through the one predicate `is_lua_boolean`, and the 1/0
  mapping lives in two converters: `lua_to_value` (the `Value`-typed one, behind
  `table_to_element`'s scalars, `lua_table_to_value_map` (row upsert), `lua_table_to_values` (query
  parameters), `columns_to_cpp_rows` (group cells) and `csv_cell_to_string` (CSV cells, which is
  why `w:write_row` writes a boolean as the text `1`/`0`)) and `lua_cell_as<T>` (typed arrays via
  `lua_table_to_vector`). `table_to_element`'s array dispatch also tests cell 1 with it to pick the
  element type. `relation_target_from_lua` is the deliberate exception:
  only `nil` may clear a relation, so a boolean still throws there. Lua has no boolean *readers*
  (root design decision), so this is a write-side-only asymmetry.
- **`lua_cell_as<T>(object, caller, what)` is the checked Lua-value→T conversion for the typed
  paths**: `lua_table_to_vector` (per array cell),
  `lua_table_to_dim_map` (per binary dimension), `update_time_series_files_lua` (per path), the
  scalar `quiver.metadata` fields (`metadata_string`), `expr:rename_agents` (each key and value)
  and the `export_csv`/`import_csv` `enum_labels` codes (`parse_csv_options`).
  `what` names the offending slot in the Pattern 1 message — `cell #3`, `dimension 'stage'`,
  `path 'data_file'`, `field 'unit'`, `value for 'v1'`, `code for label 'active'` — so one rule
  and one message shape cover them all. Because it maps a boolean to 1/0 for a numeric `T`, an
  `enum_labels` code of `true` is code 1, the same policy as `dimension_sizes = {true}` below. Its
  `Value`-typed sibling is `lua_to_value(object, caller, what)`, with the same message shape.
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
  `collect_group_columns`, which walks `pairs` for exactly that reason. For element arrays that
  would be silent data loss, since a vector/set read hands a NULL cell back as a `nil` hole, so
  `table_to_element` first calls `require_dense_array`, which throws on a hole (or a non-integer
  key) and points at the group writers (before that, a userdata attribute value is rejected as
  `attribute '<name>' must be a value or a table, got userdata`: sol2's loose table test used to
  take it for an array); and the element type still
  comes from cell 1, so `{1, 2.5}` into a REAL column is rejected rather than widened (JS, Python
  and Dart type the whole column and widen it to FLOAT, and a Lua group-writer column converts each
  cell to its own `Value`, so a Lua element array is the one path that refuses it). One
  consequence worth knowing: `quiver.metadata`'s `dimension_sizes` routes through it too (via
  `metadata_array<int64_t>`), so `quiver.metadata{dimension_sizes = {true}}` coerces to a size-1
  dimension rather than erroring. That is consistent with the cross-layer boolean policy, and
  `BinaryMetadata::validate()` still rejects a non-positive size, so `{false}` throws.
- **`SOL_SAFE_NUMERICS=1` (`src/CMakeLists.txt`) is load-bearing for every `src/lua_runner/` TU.** It turns on
  sol2's `SOL_NUMBER_PRECISION_CHECKS`, which is what makes `is<int64_t>()` false for a Lua float.
  Without it that check degrades to "is a number" in release, and the folder-wide
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
  machine. `PRIVATE` on `quiver` is full coverage: the `src/lua_runner/` TUs are the only ones in
  the repo that include sol2, and all of them are in the `quiver` target (no test includes `<sol/sol.hpp>`). Use `sol::lua_nil` and
  `sol::type::lua_nil`, never `sol::nil` / `sol::type::nil`.
- `time_series_rows_from_lua` transpose, shared by `update_time_series_group_lua` and
  `update_time_series_group_by_label_lua` (both one-liners over it). Mirrors `group_rows_from_lua`
  but takes `db`, since the dimension columns come from metadata. The **dimension column(s) are
  the row-count authority**, discovered via public `get_time_series_metadata` (`dimension_column`
  plus any `value_columns` with `primary_key` set — the multi-dim case; `Database` exposes no
  Schema accessor so `internal::find_dimension_columns` is unreachable here). Dimension columns
  must be present and dense; value columns may be shorter, sparse, or empty — missing indices become
  `Value{nullptr}`, so every row carries every named column and an all-nil column such as
  `flag = {}` is still validated and written as NULL, not left to the column DEFAULT.
  Longer-than-dimension throws; a non-array column or non-positive-integer key throws; named
  columns with a zero-length dimension still throw — only a genuinely empty `{}` (no columns)
  clears (the anti-silent-clear trap is preserved). Column extents come from a `pairs` walk, never
  `sol::table::size()` (`lua_rawlen` returns an arbitrary border on a table with holes). The read
  path emits NULL cells as `nil` holes (an all-NULL column is an empty table with the key present),
  so read → modify → write round-trips; `#ts.<dimension>` is the trustworthy row count.
- **`run` returns the script's return value as JSON**, built by the anonymous-namespace
  `append_json` / `append_json_string` / `append_json_double` / `append_json_table` at the top of
  `return_json.cpp`, plus `quiver::utils::append_number` (`src/utils/number.h` — moved out of the Lua binding,
  D-38; `db:write_csv`'s cell formatter and `bin_to_csv` are its other callers). The table check uses `get_type()` rather than
  `is<T>()` on purpose: sol2's `is<sol::table>()` also accepts **userdata**, so `return db` would
  quietly encode as `{}`. The boolean check spells `get_type()` for consistency with
  `is_lua_boolean` in `internal.h`, not out of necessity — sol2's `check<bool>` *is* `lua_isboolean`
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
  `RunHandles::close_open_handles()` and then `impl_->lua.collect_garbage()` exactly once at `run()`'s
  scope exit — covering the normal-return, empty-return, and throw-unwinding paths alike. The
  guard is declared *before* `result`, so C++'s reverse-declaration-order destruction runs both
  *after* `result`'s Lua stack reference is released.
  **The explicit close is the load-bearing half, not the collection.** `collect_garbage()` only
  finalizes *unreachable* objects, so a writer the script assigned to a global
  (`w = db:write_csv(...)` — no `local`, Lua's default spelling) is a GC root and was never
  flushed: the file stayed at 0 bytes, which
  `LuaRunner_WriteCsv.UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns` pins. `db:write_csv`
  therefore hands out a `std::shared_ptr<csv_write::Writer>` and records a `weak_ptr` in
  `RunHandles::open_writers` (declared in `src/lua_runner/internal.h`, bodies in `src/lua_runner/lua_runner.cpp`); `close_open_handles()` locks each one still alive, closes it (swallowing a
  flush failure — a scope-exit guard has no caller to report to, exactly as `~Writer` did), and
  clears the list. A writer therefore does not outlive its `run()`. `db:open_file` handles,
  readers and writers, are recorded the same way (a `weak_ptr` in `RunHandles::open_binary_files`) and
  closed by `close_open_handles()`, so no binary file handle outlives its `run()` either. Both lists
  are appended only through `RunHandles::add_writer` / `add_binary_file`, which first prune the
  entries whose handle the GC has already collected (`expired()`), never a closed-but-alive one, so
  the registry holds only live handles plus any dropped since the last collection, not every handle
  the run ever opened. A writer
  left in a global would otherwise hold its path in the process-wide write registry until the
  `LuaRunner` is destroyed (pinned by `LuaBinaryTest.WriterHeldInAGlobalIsClosedWhenRunReturns`
  and `HandleFromAnEarlierRunIsClosed`). The `collect_garbage()` call
  stays for every other sol2-owned resource; one call was proven sufficient by an executed probe
  against this repo's own vendored sol2/Lua build (RESEARCH.md Q1) — it must not be "hardened"
  into a loop.

## Binary Subsystem

Standalone binary file I/O layer for `.qvr` files with `.toml` metadata sidecars.
Bound in **Julia and Lua** (root design decision); Lua binds these C++ classes directly via sol2 in `src/lua_runner/binary.cpp` (file I/O is db-scoped and sandboxed — `db:open_file`/`db:bin_to_csv`/`db:csv_to_bin`; metadata builders under `quiver.*`; method syntax + string aggregation ops).

- `BinaryFile` class (Pimpl): `open_file(path, mode, metadata?)`, `read(dims, allow_nulls = false)`, `write(data, dims)`, `get_metadata()`, `get_file_path()`
- `CSVConverter` class (composition, no Pimpl): `bin_to_csv(path, aggregate)`, `csv_to_bin(path)` — the only
  entry points; the constructor is private. Writer and reader share one definition of each text shape, so a file
  `bin_to_csv` writes is exactly what `csv_to_bin` checks: the column names (`header_`, built once by the
  constructor — the dimension columns, or a leading `date`/`datetime` column instead of the time dimensions when
  aggregating, then the labels), a coordinate's dimension cells (`dimension_cells`), and the comma split/join
  (`split_fields`/`join_fields`, file-local). `csv_to_bin` checks each data row's field count against `header_`
  before parsing a cell (Pattern 1 `Cannot csv_to_bin: line N has X fields, expected Y`, or `file ends before
  line N`), which is what lets `validate_dimensions` index every dimension cell. `split_fields` keeps a trailing
  empty field (`a,b,` is three): do not swap it for `std::getline(stream, field, ',')`, which drops it and would let
  a trailing-comma row pass the width check. Data cells are written by `utils::append_number` and read by
  `utils::parse_float` (the whole cell must parse, in the "C" locale's format; `null` is NaN), so bin → csv → bin
  is exact in every host locale. A bad cell throws `Cannot csv_to_bin: invalid float value '<v>' for label
  '<label>'`.
- `BinaryMetadata` struct: `dimensions`, `initial_datetime`, `unit`, `labels`, `version`; `number_of_time_dimensions()` is derived from `dimensions` (not stored)
  - Factories: `from_toml_content()` (and `from_toml_file()`, which reads the sidecar and calls it), `from_element()`. Both hand their eight fields to one anonymous-namespace `build_metadata(operation, ...)` in `binary_metadata.cpp`. It rejects a `dimension_sizes`/`dimensions` or `frequencies`/`time_dimensions` count mismatch before indexing either, takes each time dimension's frequency from its matched position in `time_dimensions`, and names the calling factory in its Pattern 1 errors. `from_element` does not go through TOML text. In a TOML document, an absent array reads as empty, an absent or non-string `version`/`unit`/`initial_datetime` throws naming the key, and a wrong-typed array entry throws instead of being skipped. There is no incremental builder: the two factories are the only construction path a binding reaches (C API, Julia, Lua), and they derive every time dimension's `parent_dimension_index` (the previous time dimension) and `initial_value` (from `initial_datetime`). Inside the library, `build_broadcast_metadata` (`expression_helpers.h`) still assembles `dimensions` directly.
  - Serialization: `to_toml()`
  - Initial values: `derive_initial_values()` — the one computation of every time dimension's `initial_value` (from `initial_datetime` and the parent chain). `build_metadata` (behind both factories) calls it after `validate()`; the `ExpressionAggregate` constructor calls it on its output metadata. `to_toml()` never writes the values, so every load re-derives them. It is public for those callers, and deliberately not bound to the C API/Julia/Lua: no binding can mutate a `BinaryMetadata`.
  - Validation: `validate()`, `validate_time_dimension_metadata()`, `validate_time_dimension_sizes()`
- `Dimension` struct: `name`, `size`, optional `TimeProperties`
- `TimeProperties` struct: `frequency`, `initial_value`, `parent_dimension_index`. `initial_value` is the dimension's coordinate at `initial_datetime` (1 for the outermost time dimension). It is **stored**, not derived on read, because `next_dimensions` and `ExpressionAggregate::compute_row` read it per cell (`first_dimensions` once per traversal). Any code that changes `dimensions` or `initial_datetime` must end with `BinaryMetadata::derive_initial_values()`.
- `TimeFrequency` enum: `Yearly`, `Monthly`, `Weekly`, `Daily`, `Hourly`

### Time Coordinates

A coordinate names a **cell**, and its datetime is the cell's start. `TimeProperties::add_offset_from_int(base, value)`
floors `base` to the start of its own period (January 1, the 1st, the day for Weekly and Daily, the hour) and adds
`value - 1` periods. Folded over the time dimensions outermost-first from `initial_datetime` (what
`validate_dimension_values`, `dimension_sizes_at_values` and the CSV datetime column do), every inner step starts
from its parent's period start, so no calendar step overflows (January 31 + one month is not March 3) and
`initial_value` plays no part. `add_offset_from_int(datetime, 1)` is therefore the start of that frequency's period
holding `datetime` (for Weekly, the week that starts on `datetime`'s day, which is on the file's grid only when
that day is a whole number of weeks after `initial_datetime`'s day). Yearly and monthly periods are calendar-aligned; **a week is seven days counted
from the day of `initial_datetime`**, never from January 1, so a weekly file crosses year ends on one grid.
`position_in_parent` (`binary_utils.h`) is the inverse — a datetime's position inside a dimension's parent period —
and the one rule for it: `BinaryMetadata::derive_initial_values()` (called by `build_metadata`, behind both factories, and the
`ExpressionAggregate` constructor) sets each inner `initial_value` to the position of `initial_datetime`
(the outermost gets 1), and `validate_dimension_values` rejects a coordinate whose cell start sits at a different
position than the value given (day 30 of February spills into March). `build_metadata` runs `validate()`
**before** computing initial values, since a position exists only for the eight parent/child layouts it admits:
Monthly under Yearly; Daily under Yearly, Monthly or Weekly; Hourly under Yearly, Monthly, Weekly or Daily. The
`EveryCell*` tests in `tests/test_binary_file.cpp` walk every cell of each pair from a mid-period, non-midnight start.

### Iteration Helpers

Free functions in `quiver::` for traversing the dimension space of a `BinaryMetadata` (declared in `quiver/binary/iteration.h`):

- `first_dimensions(meta)` — initial position; returns `initial_value` for time dims, `1` for non-time dims
- `next_dimensions(meta, current)` — next position via right-to-left cascade; returns `nullopt` at end. Uses `dimension_sizes_at_values` to handle variable-length time dims (Feb=28/29, Jan=31, etc.), then lifts each reset dimension to `dimension_start_at_values` in ascending index order (an ancestor restored earlier in the same pass is what its descendants compare against).
- `dimension_sizes_at_values(meta, values)` — per-dim actual sizes at a coordinate vector. Used by `next_dimensions()` and `ExpressionAggregate` to size variable-length time-dim iteration windows.
- `dimension_start_at_values(meta, values, index)` — where dimension `index` starts at that coordinate: its `initial_value` if it is a time dim and **every** time ancestor on the `parent_dimension_index` chain is at its own `initial_value` (the period the file starts in), else `1`. It is the only place the mid-period start rule is written: `next_dimensions()` and `ExpressionAggregate::compute_row` both call it, so an aggregate reduces over `[dimension_start_at_values, dimension_sizes_at_values]`, exactly the cells the traversal visits — except when the reduced dimension is the outermost time dimension, whose window `[1, size]` also reads the first period's cells before `initial_datetime` (never written, so NaN, and skipped). Checking only the immediate parent (the old rule, which both places had) restarted every later March of a `yearly × monthly × daily` file starting 2025-03-15 at day 15.

Used by `Expression::save()` and `CSVConverter::{bin_to_csv, csv_to_bin}` — single source of truth for `.qvr` traversal.

### Write Registry

In-process path registry prevents reading files that are currently open for writing. A `static std::unordered_set<std::string>` in `binary_file.cpp` tracks canonical paths of files open in write mode. Opening a reader or a second writer on a registered path throws `std::runtime_error`. The registry entry is removed in `BinaryFile::close()` (after flush; the destructor closes). Move semantics work correctly: moved-from objects have null `impl_` and skip unregistration. Not thread-safe or multi-process safe.

### Performance Bottlenecks

Profiled with 480×500×31 dimensions (~7.3M read/write calls). Main hot-path costs:

1. **`unordered_map<string, int64_t>` dims parameter (~40% of total time):** Every read/write constructs a hash map with string keys. Hashing, heap allocation for strings/buckets, and `find()` lookups dominate. Indexed `vector<int64_t>` overloads were prototyped and dropped in favor of API simplicity — the map-based form is the single supported entry point. Hot-path consumers (e.g., `ExpressionFile::compute_row`, `Expression::save`) cache the `unordered_map` across calls to amortize the allocation. Do not reintroduce indexed overloads without revisiting that decision.
2. **`validate_dimension_values` (~19% of total time):** Called on every read/write. Checks dimension count, name existence, bounds, and time-dimension consistency (date arithmetic via `add_offset_from_int`/`position_in_parent`). Could be made opt-in or skippable when callers guarantee correct values (e.g., when iterating via `next_dimensions()`).
3. **`vector<double>` allocation per read (~3%):** Each `read()` allocates a new vector. A `read_into(buffer, dims)` overload writing into caller-provided storage would eliminate this.

## Expression Subsystem

Lazy expressions over `.qvr` binary files. Build a DAG using `+ - * /` operator overloads (binary and unary minus) and unary math free functions, materialize via `save()`. Bound in **Julia and Lua** (root design decision); Lua binds these C++ classes directly via sol2 in `src/lua_runner/binary.cpp` (`quiver.*` namespace + method syntax + string aggregation ops; `expr:save` paths are sandboxed to the database directory).

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
  - Aggregation: `aggregate(dimension, op, [parameter])` collapses a dimension; `aggregate_agents(op, [parameter])` collapses the label axis. `op` is `ExpressionAggregate::Operation` (`Sum / Mean / Min / Max / Percentile`) for both; `ExpressionAggregateAgents::Operation` is an alias of it, not a second enum. `Percentile` requires a `parameter` fraction in `[0, 1]`; nullary ops reject `parameter`.
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
  - `ExpressionBinary`: combines two operands with `ExpressionBinary::Operation::{Add,Subtract,Multiply,Divide,Gt,Lt,Gte,Lte,Eq,Neq,And,Or}` (nested enum). Arithmetic ops compute `lhs op rhs`; the six comparisons and the two logical ops (`And`/`Or`, nonzero-is-true) return `1.0`/`0.0` and propagate a NaN operand as NaN. Logical ops skip unit-match validation and emit a unitless result (a small `is_logical(op)` branch in the constructor); comparisons/arithmetic keep the full unit-match check. Constructor pre-computes broadcast metadata (`build_broadcast_metadata({&lhs, &rhs}, lhs)`, see the broadcast-metadata bullet below) and one `BroadcastOperand` per operand (index translation tables + reusable buffers, built by `make_broadcast_operand` and driven per row by `compute_broadcast_operand_row` — both shared with `ExpressionTernary` via `expression_helpers.h`). The `apply(Operation, double, double)` operation-dispatch is a private static member.
  - `ExpressionUnary`: applies a single-operand function with `ExpressionUnary::Operation::{Negate,Abs,Sqrt,Log,Exp,Not}` (nested enum). For the math ops `metadata()` returns the operand's metadata unchanged (no dimensional analysis — `sqrt(MW)` stays as `MW`); `Not` is logical negation (nonzero→0, 0→1, NaN propagates) and returns a **unitless** boolean via a dedicated `output_meta_` member. Constructor pre-allocates a reusable `operand_row_buf_`. Lets IEEE-754 NaN/inf propagate naturally (`sqrt(-1) → NaN`, `log(0) → -inf`); no NaN special-casing. The `apply(Operation, double)` operation-dispatch is a private static member.
  - `ExpressionTernary`: selects per-element across three operands. `Operation::{IfElse}` (nested enum). For `IfElse`: NaN in `condition` → NaN; `condition != 0` → `then_value`; else `else_value`. Constructor eagerly validates (`then` and `else` units must match; `condition`'s unit is ignored; shapes broadcast across all three pairs), pre-builds broadcast metadata via the same `build_broadcast_metadata({&cond, &then, &else}, then)` and one `BroadcastOperand` per operand (same shared machinery as `ExpressionBinary`). The `apply(Operation, double, double, double)` operation-dispatch is a private static member.
  - `ExpressionAggregate`: collapses a named dimension. `Operation::{Sum,Mean,Min,Max,Percentile}` (nested enum). Constructor eagerly removes the dim from output metadata, rewires child time-dim `parent_dimension_index` transitively (a time dim whose parent was removed re-points to the removed dim's grandparent, or `-1`), and pre-allocates index translation + reusable buffers. When the removed dim is the **outermost time dimension** and a time dim remains, the constructor also floors `initial_datetime` to the start of the removed dim's period holding it (`reduced_dim.time->add_offset_from_int(initial_datetime, 1)`: `year × month` from 2025-03-01 gives 2025-01-01, `day × hour` from 06:00 gives 00:00), then calls `derive_initial_values()`, which gives every remaining time dim 1. `compute_row` forwards the promoted child's coordinate to the operand unchanged, so output month *m* must still mean calendar month *m*. Without the rebase, the in-memory output kept month's start at 3 while the saved file re-read it as 1, shifting the data by two months. The rebase runs before `derive_initial_values()`, which reads the rebased `initial_datetime`. Skips NaN inputs during accumulation; all-NaN range yields NaN.
  - `ExpressionAggregateAgents`: collapses the label axis to a single entry named after the operation (e.g., `"sum"`, `"mean"`, `"percentile"`). Dimensions, `initial_datetime`, `unit` unchanged. Same NaN policy as `ExpressionAggregate`. Shares `ExpressionAggregate`'s operation enum (`using Operation = ExpressionAggregate::Operation;`) and the accumulation helpers in `expression_helpers.h`.
  - `ExpressionSelectAgents`: projects the operand onto a caller-supplied label list. Constructor pre-computes a `selected_indices_` table from operand-label → output-position, copies operand metadata with `labels` replaced, and calls `output_meta_.validate()` (which rejects duplicate output labels). Missing labels throw `"Cannot select_agents: label not found: '<name>'"`. `compute_row` reads the operand row into a reusable buffer and gathers selected columns into `out`.
  - `ExpressionRenameAgents`: rewrites operand labels via a partial `{old: new}` mapping. Constructor builds a rename map (duplicate keys throw), walks operand labels swapping matched names, verifies every key was used (unmatched keys throw), and calls `output_meta_.validate()` (rejects collisions like `val1→val2` when `val2` already exists). `compute_row` forwards directly to the operand — count and order are unchanged so no per-row reshuffle is needed.
- Validation is **eager** at construction for `ExpressionBinary`, `ExpressionTernary`, `ExpressionAggregate`, `ExpressionAggregateAgents`, `ExpressionSelectAgents`, `ExpressionRenameAgents` (units/dim sizes/time-dim properties/label sets/initial datetimes for binary and ternary; dim existence + op/parameter consistency + output metadata validity for aggregations; label existence + uniqueness for label-axis projections). `ExpressionUnary` has no inputs to cross-validate so its constructor just sizes the row buffer. Computation is **lazy**: no I/O until `save()`.
- **One broadcast-metadata builder for every arity**: `build_broadcast_metadata(sources, primary)` (`expression_helpers.h`) builds the output metadata of `ExpressionBinary` (`{lhs, rhs}`, primary `lhs`) and `ExpressionTernary` (`{cond, then, else}`, primary `then`). Source order sets the output dimension order: the union of dimension names, first occurrence first, each sized as the max over the sources that have it, with time properties and parent link from the first source that has it (so `ifelse` output dimensions are condition-first). `version` and `unit` come from the primary (a logical op then clears the unit); `initial_datetime` comes from the first source with a time dimension, else from the primary — the pairwise `validate_shape_compatibility` calls already force every time-bearing source to agree, so only that no-time fallback depends on which operand is primary. The same pairwise checks force a shared time dimension to agree on frequency, `initial_value` and parent name, so each copied `initial_value` already equals what `derive_initial_values()` would compute and the builder does not call it. Labels follow one rule, `broadcast_labels`: every operand with more than one label must carry the same label set, a single-label operand broadcasts whatever its label is called, and when every operand has a single label the output takes the primary's (`{"max"} - {"min"}` is `{"max"}`; `ifelse({"c"}, {"t"}, {"e"})` is `{"t"}`). A mismatch throws `Cannot apply: labels are incompatible across operands (non-singleton label sets must match)`. There used to be a separate two-operand builder whose stricter rule rejected two differently named single labels (so `aggregate_agents("max") - aggregate_agents("min")` threw while `ifelse` over the same operands worked) — don't reintroduce a per-arity copy.
- All operation enums are nested in their owning class: `ExpressionBinary::Operation`, `ExpressionUnary::Operation`, `ExpressionTernary::Operation`, `ExpressionAggregate::Operation`. There is **one** aggregation enum (`Sum / Mean / Min / Max / Percentile`): `ExpressionAggregateAgents::Operation` is `using Operation = ExpressionAggregate::Operation;`, so `aggregate` and `aggregate_agents` take the same type and `aggregation_operation_label` / `validate_aggregation_param` / `aggregation_accumulate` / `aggregation_finalize` (`expression_helpers.h`) are plain functions on it. It used to be two parallel enums with identical values, which doubled the C enum, the C `from_c` switch, the Lua string parser and the Julia constants; do not re-split it. Label-axis projection nodes (`ExpressionSelectAgents`, `ExpressionRenameAgents`) have no operation enum — their behavior is fully specified by the label list / rename map. The C API mirrors this with four enums: `quiver_expression_operation_t` (now `ADD..DIVIDE`, the comparisons `GT/LT/GTE/LTE/EQ/NEQ`, and the logical `AND/OR`), `quiver_expression_unary_operation_t` (math ops plus `NOT`), `quiver_expression_ternary_operation_t`, and `quiver_expression_aggregate_operation_t`, which both `quiver_expression_aggregate` and `quiver_expression_aggregate_agents` take. The one C `from_c` and the one Lua `parse_aggregate_op` take the calling operation's name, so their Pattern 1 messages still read `Cannot aggregate: ...` or `Cannot aggregate_agents: ...`. Comparisons and logical ops reuse the `quiver_expression_apply*` / `quiver_expression_apply_unary` entry points (no new C functions); the Julia FFI enum (`src/c_api.jl`) must carry the same values.
