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
  database_impl.h         # Database::Impl - schema/type validators, FK resolution, group inserts, TransactionGuard
  database_internal.h     # internal:: helpers - read templates, value_matches_type, metadata converters
  database_create.cpp / database_read.cpp / database_update.cpp / database_delete.cpp
  database_metadata.cpp / database_query.cpp / database_time_series.cpp / database_describe.cpp
  database_csv_export.cpp / database_csv_import.cpp
  schema.cpp              # Schema introspection (from_database), table classification, group_names
  schema_validator.cpp    # Schema convention validation
  type_validator.cpp      # Scalar/array type validation (caller-threaded Pattern 1 messages)
  element.cpp / row.cpp / result.cpp / migration.cpp / migrations.cpp
  lua_runner.cpp          # LuaRunner (sol2) - all Lua bindings
  cli/main.cpp            # quiver_cli CLI entry point
  utils/string.h          # String utilities: new_c_str, trim
  utils/datetime.h        # ISO 8601 parse/format helpers
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

Static methods for database creation:
```cpp
static Database from_schema(const std::string& db_path, const std::string& schema_path, const DatabaseOptions& options = {});
static Database from_migrations(const std::string& db_path, const std::string& migrations_path, const DatabaseOptions& options = {});
```
`schema_path` is a `.sql` file; `migrations_path` is a directory of numbered version
subdirectories with `up.sql`/`down.sql`.

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
  attributes. Don't re-grow per-type copies.
- **Table classification has one source** (`schema.cpp`): `Schema::group_names(collection,
  GroupTableType)` and `is_group_table(table, type)` are the only way to enumerate/classify
  `_vector_` / `_set_` / `_time_series_` tables (`group_names` excludes `_time_series_files`).
  All list/metadata/describe call sites use them — never hand-roll prefix scans.
- **Declaration order everywhere**: metadata and list functions iterate `column_order`
  (declaration order), matching the `describe(ostream&)` dump and CSV export. Nothing reports alphabetical order.
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
- **`update_element` / `delete_element` verify the id exists** (`SELECT 1 ... WHERE id = ?` via
  `execute`) and throw Pattern 2 `"Element not found: ..."` — no silent no-op.
- **`execute` validates parameter count** (`database.cpp`): `sqlite3_bind_parameter_count` must
  equal `parameters.size()`, else it throws — the single guard for every `query_*` and internal
  parameterized statement.
- **Utilities**: `quiver::string::new_c_str` / `trim` in `src/utils/string.h`; ISO 8601
  (`YYYY-MM-DDTHH:MM:SS`) parse/format helpers in `src/utils/datetime.h`;
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
  `db:export_csv`, `db:import_csv`, `expr:save`). It rejects `:memory:` databases, resolves
  relative paths against the database file's directory (bare-filename db paths fall back to the
  CWD at call time, mirroring `create_database_logger`), canonicalizes via `weakly_canonical`,
  and requires strict containment (candidate == root is rejected — the binary subsystem appends
  `.qvr`/`.toml` by string concatenation). The resolved absolute path is what's forwarded
  downstream, so the process CWD is irrelevant to Lua file I/O. Pattern 1 messages thread the
  public operation name. This is LuaRunner policy only — the C++/Julia surfaces stay unsandboxed.
- **Enabled standard libraries**: `base`, `string`, `table`, `math`, `coroutine`, and `utf8`
  (pure computation only). `os`, `io`, `package`/`require`, and `debug` stay unloaded — scripts
  cannot reach the shell, the process, the environment, or the filesystem outside the db sandbox.
- `dofile` and `loadfile` are nil'd out after `open_libraries` (no loading Lua source from disk);
  string-form `load` stays available.
- `parse_csv_options(table)` is the single CSVOptions parser shared by `export_csv`/`import_csv`.
- `to_lua_table<T>` overloads (flat + nested) are the only vector→table marshalers.
- `describe` / `describe_collection` / `summarize_collection` are bound as plain lambdas returning
  the C++ `std::string` text report (`db:describe()` returns a string — it does not print).
- Lua→C++ converters **throw on unsupported value types** (booleans, functions, ...) — never
  skip silently; a skipped positional query parameter would shift the rest and bind NULL to the
  trailing placeholder.
- `update_time_series_group_lua` transpose: the **dimension column(s) are the row-count
  authority**, discovered via public `get_time_series_metadata` (`dimension_column` plus any
  `value_columns` with `primary_key` set — the multi-dim case; `Database` exposes no Schema
  accessor so `internal::find_dimension_columns` is unreachable here). Dimension columns must be
  present and dense; value columns may be shorter, sparse, or empty — missing indices become
  `Value{nullptr}` (rows stay uniform: the core builds its INSERT list from `rows[0]`).
  Longer-than-dimension throws; a non-array column or non-positive-integer key throws; named
  columns with a zero-length dimension still throw — only a genuinely empty `{}` (no columns)
  clears (the anti-silent-clear trap is preserved). Column extents come from a `pairs` walk, never
  `sol::table::size()` (`lua_rawlen` returns an arbitrary border on a table with holes). The read
  path emits NULL cells as `nil` holes (an all-NULL column is an empty table with the key present),
  so read → modify → write round-trips; `#ts.<dimension>` is the trustworthy row count.
- Script errors surface as `"Failed to run Lua script: ..."` (root Pattern 3).

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
- Free functions in `quiver::` for element-wise comparison: `gt / lt / gte / lte / eq / neq`, each × {expr,expr | expr,double | double,expr}. Produce `1.0`/`0.0` per element; **a NaN operand propagates as NaN** (so `ifelse(cmp, …)` yields NaN). Free functions (not operators) so the surface is identical in C++ and Lua, where comparison metamethods can't return an `Expression`. They reuse `ExpressionBinary` (so they inherit unit-match + shape validation; the result carries the broadcast unit).
- Free functions in `quiver::` for boolean logic on nonzero-is-true operands: `and_ / or_` (binary, same three combos) and `not_` (unary). Produce `1.0`/`0.0`, **NaN propagates**, and the result is **unitless** — `and_`/`or_` skip unit-match validation (only shapes must broadcast) so conditions on different-unit variables compose; `not_` emits a unitless result too. Named (not operators) because `and`/`or`/`not` are reserved in C++/Lua.
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
