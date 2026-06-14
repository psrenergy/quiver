// Agent-facing reference for the Lua `db` API available inside run_lua scripts.
//
// Authority: `src/lua_runner.cpp` `bind_database()` (this repo) — extracted by hand, NOT imported.
// The shipped quiverdb native binding is the runtime truth; this is docs. Whenever the C++ Lua
// binding changes, re-diff every signature below against `bind_database()` (names, arg order, arg
// types, return shapes).
//
// NOTE: the binary/expression subsystems are bound in the native binding and documented below.
// File-touching operations (db:open_file, db:bin_to_csv, db:csv_to_bin, expr:save) are sandboxed
// to the database file's directory; the pure-metadata builders stay under the quiver.* global.
//
// FORMAT CONVENTION: every db: method should appear at least once as the literal token
// `db:<snake_case_name>` so coverage is greppable.
export const LUA_DB_API_REFERENCE = `
# Quiver Lua API Reference

Quiver embeds a Lua scripting layer (via \`LuaRunner\`) that exposes the same database API as the
other language bindings. This document is a complete reference of every method available to a Lua
script.

## Running scripts

The database is provided to your script as a global userdata named \`db\`. There are **no
open/close lifecycle methods in Lua** — the \`LuaRunner\` opens the database and hands you \`db\`
already connected. All methods are called with the colon syntax:

\`\`\`lua
db:create_element("Collection", { label = "Item 1", value = 42 })
local ids = db:read_element_ids("Collection")
\`\`\`

If a script raises an error (including any error thrown by a \`db:\` call), it surfaces to the host
as:

\`\`\`
Failed to run Lua script: <message>
\`\`\`

## Value type mapping

Lua values map to Quiver column values as follows:

| Lua value          | Quiver value | Notes                                          |
| ------------------ | ------------ | ---------------------------------------------- |
| integer            | INTEGER      | Also accepted for REAL columns (coerced to real). |
| number (float)     | REAL         | A float is rejected for an INTEGER column.     |
| string             | TEXT         | Also used for \`date_time\` columns (ISO 8601).  |
| \`nil\`              | NULL         | In query params, file paths, and ts rows.      |
| table (1-indexed)  | array        | Used for vectors/sets and column-oriented data.|

**Unsupported types throw.** Passing a boolean, a function, or a nested table where a scalar is
expected raises an error rather than silently dropping the value. This applies to element
attributes, time-series rows, and query parameters. A skipped positional query parameter would
shift every later parameter and bind NULL to the trailing placeholder, so this is rejected loudly.

Dates are plain strings in ISO 8601 format: \`YYYY-MM-DDTHH:MM:SS\`. (Lua keeps a string-based
datetime surface — there are no DateTime wrapper helpers, unlike Julia/Dart/Python.)

---

## Critical rules

- **Type coercion.** An integer is accepted for a REAL column (coerced to real on insert); a float
  is rejected for an INTEGER column. Other type mismatches raise a validation error and roll the
  whole script back.
- **Errors abort the script.** Any error thrown by a \`db:\` call stops the script and surfaces as
  \`Failed to run Lua script: <message>\`. Validation failures roll back whatever the current
  transaction covered.
- **Standard library.** Only the \`base\`, \`string\`, and \`table\` libraries are loaded — there is NO
  \`math\` (use \`//\` for integer division), and no \`os\`, \`io\`, or \`require\`; \`dofile\` and
  \`loadfile\` are removed (string-form \`load\` is available).
- **Filesystem sandbox.** Every file-touching operation (\`db:export_csv\`, \`db:import_csv\`,
  \`db:open_file\`, \`db:bin_to_csv\`, \`db:csv_to_bin\`, \`expr:save\`) resolves relative paths against
  the directory containing the database file and rejects anything outside it (subdirectories are
  fine; \`..\` escapes and outside absolute paths throw \`Cannot <op>: path '...' escapes the
  database directory ...\`). On an in-memory database these operations throw
  \`Cannot <op>: database is in-memory, file operations are unavailable\`.
- **Output.** Scripts cannot return values to the tool — use \`print()\` for anything you need to
  see (it is captured). Arrays are 1-indexed (iterate with \`ipairs\`); reading a NULL yields \`nil\`,
  writing \`nil\` stores NULL where NULL is accepted (query params, ts rows, file columns — but NOT
  element scalar attributes; see CRUD).
- **Embedding harness may restrict further.** The library itself allows transactions and the
  (sandboxed) CSV/file operations below. A host that runs your script (e.g. a hosted \`run_lua\`
  tool) may disable some of them and report \`disabled in the run_lua sandbox\` — that limit comes
  from the harness, not from quiverdb.

---

## Database info

\`\`\`lua
db:is_healthy()                    -- boolean
db:current_version()               -- integer (current migration version)
db:path()                          -- string (database file path)
db:describe()                      -- string: whole-DB text report (returns it, does NOT print)
db:describe_collection(collection) -- string: one collection's structure (text report)
db:summarize_collection(collection)-- string: per-scalar null/non-null counts, low-cardinality
                                   --         integer value distributions, per-group sizes
\`\`\`

All three \`describe*\`/\`summarize*\` methods **return** a string — \`print()\` it to see it.

---

## Transactions

\`\`\`lua
db:begin_transaction()   -- start an explicit transaction
db:commit()              -- commit it
db:rollback()            -- roll it back
db:in_transaction()      -- boolean: is a transaction currently open?
db:transaction(fn)       -- run fn(db) inside begin/commit; rollback + rethrow if fn errors
\`\`\`

To make a group of writes atomic, prefer the \`db:transaction\` wrapper:

\`\`\`lua
db:transaction(function(db)
    local id = db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    db:update_element("Collection", id, { some_integer = 99 })
end)   -- both writes commit together; if either throws, both roll back
\`\`\`

**Caveat:** if the host already runs your script inside a transaction, an explicit
\`db:begin_transaction()\` will error (\`cannot start a transaction within a transaction\`) and a
mid-script \`db:commit()\` would prematurely end the host's transaction. When unsure whether a
transaction is already open, check \`db:in_transaction()\` first, or just issue writes directly —
each \`db:\` write is durable on its own.

---

## CRUD

\`\`\`lua
local id = db:create_element(collection, element_table)   -- returns new integer id
db:update_element(collection, id, element_table)
db:delete_element(collection, id)
\`\`\`

The element table holds scalar attributes as \`key = value\`, and vector/set attributes as
1-indexed arrays. The element type of an array is inferred from its **first** entry:

\`\`\`lua
local id = db:create_element("Collection", {
    label        = "Item 1",   -- scalar string
    some_integer = 42,         -- scalar integer
    some_float   = 3.14,       -- scalar real
    value_int    = { 1, 2, 3 },          -- vector/set of integers
    tags         = { "a", "b", "c" },    -- vector/set of strings
})
\`\`\`

\`update_element\` only touches the attributes you pass:

\`\`\`lua
db:update_element("Collection", id, { some_integer = 999 })
\`\`\`

Notes:
- **\`update_element\` / \`delete_element\` require an existing id.** Targeting an id that does not
  exist throws \`Element not found: <id> in collection '<collection>'\` (no silent no-op). Use
  \`read_element_ids\` to get valid ids.
- **Empty arrays are skipped.** An attribute whose value is \`{}\` writes no vector/set (the element
  type can't be inferred from an empty array), so it is silently dropped.
- **No \`nil\` scalar attributes.** In Lua a key set to \`nil\` is dropped from the table, so
  \`{ x = nil }\` is identical to \`{}\`; an update/create table that ends up with no attributes
  **throws** (\`element must have at least one attribute\`). To leave a column unchanged, omit the
  key — you cannot set a scalar to NULL via the element table. (\`nil\` → NULL is only accepted by
  \`add_time_series_row\` and \`update_time_series_files\`.)

---

## Scalar reads (bulk, across all elements)

Each returns a flat array (1-indexed table), one value per element, in id order.

\`\`\`lua
db:read_scalar_integers(collection, attribute)   -- { 42, 37, ... }
db:read_scalar_floats(collection, attribute)      -- { 3.14, 2.71, ... }
db:read_scalar_strings(collection, attribute)     -- { "Item 1", "Item 2", ... }
\`\`\`

---

## Vector reads (bulk)

Each returns an array of arrays — one inner array per element.

\`\`\`lua
db:read_vector_integers(collection, attribute)   -- { {1,2,3}, {2,3,4}, ... }
db:read_vector_floats(collection, attribute)
db:read_vector_strings(collection, attribute)
\`\`\`

---

## Set reads (bulk)

Same shape as vector reads — an array of arrays.

\`\`\`lua
db:read_set_integers(collection, attribute)
db:read_set_floats(collection, attribute)
db:read_set_strings(collection, attribute)
\`\`\`

---

## Composite by-id reads (Lua convenience helpers)

\`\`\`lua
db:read_element_ids(collection)                  -- { 1, 2, 3, ... }

db:read_scalars_by_id(collection, id)            -- { attr = value, ... } (missing -> nil)
db:read_vectors_by_id(collection, id)            -- { column = { v1, v2, ... }, ... }
db:read_sets_by_id(collection, id)               -- { column = { v1, v2, ... }, ... }
db:read_element_by_id(collection, id)            -- scalars + vectors + sets merged into one table
\`\`\`

\`read_element_by_id\` merges every scalar, vector, and set for the element into a single table.
Scalar attributes with no value come back as \`nil\`.

---

## Time series

Time-series group data is **column-oriented** in Lua: \`{ column = { v1, v2, ... }, ... }\`. The
dimension (ordering) column is a \`date_*\` text column holding ISO 8601 timestamps.

### Read a whole group (column-oriented)

\`\`\`lua
local ts = db:read_time_series_group(collection, group, id)
-- ts = { date_time = { "2024-01-01T00:00:00", ... }, value = { 10.5, 20.0, ... } }
-- returns an empty table {} if the element has no rows
\`\`\`

A \`NULL\` value cell comes back as a \`nil\` hole (the index is simply absent), and an all-\`NULL\`
value column is an empty table \`{}\` (its key is still present). Because \`nil\` cannot occupy an
array slot, **take the row count from the dimension column** (\`#ts.date_time\`), never from a value
column.

### Read one value per element at a date (\`read_time_series_row\`)

\`\`\`lua
local values = db:read_time_series_row(collection, group, attribute, date_time)
-- { v_elem1, v_elem2, ... } in element-id order
\`\`\`

One value per element using **last non-null value at or before \`date_time\`** semantics. Elements
with no matching data yield \`nil\` in the array. \`date_time\` is an ISO 8601 string.

### Replace a whole group (column-oriented — SAME shape as the read)

\`update_time_series_group\` takes the **exact column-oriented shape \`read_time_series_group\`
returns**: a table mapping each column name to a 1-indexed array of its values. Read → modify →
write round-trips. Passing an empty table \`{}\` clears the group.

\`\`\`lua
db:update_time_series_group("Items", "data", id, {
    date_time = { "2024-01-01T00:00:00", "2024-01-02T00:00:00", "2024-01-03T00:00:00" },
    value     = { 10.5, 20.0, 30.0 },
})

db:update_time_series_group("Items", "data", id, {})   -- clears the group
\`\`\`

A read-modify-write looks like this:

\`\`\`lua
local ts = db:read_time_series_group("Items", "data", id)
ts.value[2] = 125.0                                    -- edit the 2nd row's value
db:update_time_series_group("Items", "data", id, ts)   -- write the whole group back
\`\`\`

**DO NOT pass an array of row tables** (\`{ { date_time = ..., value = ... }, ... }\`) — that is the
\`add_time_series_row\` shape, not this one. Doing so raises
\`stack index -1, expected string, received number\` (the integer array indices 1, 2, 3 are not
column names). Each value of the top-level table must be an **array**, not a scalar.

**Rules** (validation throws, rolling the script back):
- Every column value must be an array — a bare scalar throws \`column '...' must be an array of values\`.
- The **dimension column(s) set the row count** and must be present and fully populated: the
  \`date_*\` ordering column, plus any extra primary-key columns in a multi-dimensional group (e.g.
  \`block\`). A missing one throws \`missing dimension column '...'\`; a \`nil\` inside one throws
  \`dimension column '...' has nil at index N\` (they are primary-key columns and cannot be NULL).
- **Value columns may be shorter, sparser, or absent** relative to the dimension column — every
  missing cell is written as \`NULL\`. So \`value = { 10.0, nil, 30.0 }\` or a too-short \`value = { 10.0 }\`
  both write NULLs for the gaps; this is how you round-trip the \`nil\` holes a read produces. A value
  column **longer** than the dimension column throws \`column '...' has length N but expected M\`.
- Named columns whose dimension transposes to zero rows throw (\`contain no rows; pass an empty
  table {} to clear the group\`) — only a bare \`{}\` clears.
- Integer values are accepted for REAL columns (converted on insert). Booleans, functions, and
  other unsupported Lua types throw \`column '...' has unsupported Lua type\`.

### Append/upsert a single row (\`add_time_series_row\` — ROW-oriented, the one exception)

Unlike the column-oriented group update above, this takes **one row table of scalars** (dimension
column + value column(s)), and upserts that single row:

\`\`\`lua
db:add_time_series_row("Items", "data", id, {
    date_time = "2024-01-04T00:00:00",
    value     = 40.0,
})
\`\`\`

---

## Time series files

For schemas that reference external time-series files (the \`{Collection}_time_series_files\`
singleton table):

\`\`\`lua
db:has_time_series_files(collection)              -- boolean
db:list_time_series_files_columns(collection)     -- { "data_file", "metadata_file", ... }
db:read_time_series_files(collection)             -- { data_file = "path", metadata_file = nil, ... }
db:update_time_series_files(collection, { data_file = "path/to/data.bin", metadata_file = nil })
\`\`\`

In \`update_time_series_files\`, a \`nil\` value clears that column.

---

## Metadata

### Single attribute / group

\`\`\`lua
db:get_scalar_metadata(collection, attribute)        -- scalar metadata table (below)
db:get_vector_metadata(collection, group_name)       -- group metadata table (below)
db:get_set_metadata(collection, group_name)          -- group metadata table
db:get_time_series_metadata(collection, group_name)  -- group metadata table (+ dimension_column)
\`\`\`

### Lists (one entry per attribute/group)

\`\`\`lua
db:list_scalar_attributes(collection)    -- array of scalar metadata tables
db:list_vector_groups(collection)        -- array of group metadata tables
db:list_set_groups(collection)           -- array of group metadata tables
db:list_time_series_groups(collection)   -- array of group metadata tables (+ dimension_column)
\`\`\`

### Scalar metadata table shape

\`\`\`lua
{
    name                   = "value",
    data_type              = "integer",   -- "integer" | "real" | "text" | "date_time"
    not_null               = true,
    primary_key            = false,
    default_value          = nil,         -- string, or nil
    is_foreign_key         = false,
    references_collection  = nil,         -- string, or nil
    references_column      = nil,         -- string, or nil
}
\`\`\`

### Group metadata table shape

\`\`\`lua
{
    group_name      = "data",
    value_columns   = { <scalar metadata table>, ... },
    dimension_column = "date_time",   -- present ONLY for time-series groups
}
\`\`\`

The \`dimension_column\` key is present only in **time-series** group metadata
(\`get_time_series_metadata\` / \`list_time_series_groups\`). Vector and set group metadata omit it
entirely.

---

## Query (parameterized SQL)

Positional \`?\` placeholders; \`params\` is an optional 1-indexed array. Each returns the first
column of the first row as the requested type, or \`nil\` if there is no result. The number of
\`params\` must match the number of \`?\` placeholders exactly — a mismatch (too few or too many)
throws rather than binding NULL or ignoring extras.

\`\`\`lua
db:query_string(sql, params)    -- string or nil
db:query_integer(sql, params)   -- integer or nil
db:query_float(sql, params)     -- number or nil
\`\`\`

Example:

\`\`\`lua
local label = db:query_string(
    "SELECT label FROM Collection WHERE some_integer = ?",
    { 42 }
)
local count = db:query_integer("SELECT COUNT(*) FROM Collection")
\`\`\`

---

## CSV import / export

Export a time-series group to a CSV file, or import one from a CSV file. \`path\` is sandboxed:
relative paths resolve against the database file's directory and must stay inside it (see
Critical rules); \`options\` is optional.

\`\`\`lua
db:export_csv(collection, group, path, options)
db:import_csv(collection, group, path, options)
\`\`\`

The optional \`options\` table has two keys:

\`\`\`lua
{
    date_time_format = "%Y-%m-%d",   -- strftime-style format for the dimension column

    -- enum_labels: write/read integer codes as human labels. Three nested levels:
    --   attribute name -> locale -> { label = integer_id }
    enum_labels = {
        status = {
            en = { active = 1, inactive = 0 },
            pt = { ativo  = 1, inativo  = 0 },
        },
    },
}
\`\`\`

**Precondition:** \`db:import_csv\` cannot run inside an open transaction (it toggles
\`PRAGMA foreign_keys\`, a no-op mid-transaction) — it throws \`Cannot import_csv: transaction already
active\`. Call it outside any \`db:transaction\` / \`db:begin_transaction\` block.

---

## Complete example

\`\`\`lua
-- create
db:create_element("Configuration", { label = "Configuration" })

local item1 = db:create_element("Collection",
    { label = "Item 1", some_integer = 42, some_float = 3.14, value_int = { 1, 2, 3 } })
local item2 = db:create_element("Collection",
    { label = "Item 2", some_integer = 37, some_float = 2.71, value_int = { 2, 3, 4 } })

-- read every element
local ids = db:read_element_ids("Collection")
for _, id in ipairs(ids) do
    local element = db:read_element_by_id("Collection", id)
    print("Element " .. id .. ", label = " .. tostring(element.label))
end

-- update
db:update_element("Collection", item1, { some_integer = 999 })

-- delete
db:delete_element("Collection", item2)
\`\`\`

---

## Binary & expression subsystems

Dense N-dimensional \`float64\` arrays (\`.qvr\` + \`.toml\` sidecar) plus lazy arithmetic over them.
File I/O is db-scoped (\`db:open_file\` / \`db:bin_to_csv\` / \`db:csv_to_bin\`); paths are extensionless
base paths, sandboxed to the database directory (see Critical rules), and \`get_file_path()\` returns
the resolved absolute path. The pure-metadata builders and expression constructors live under the
global \`quiver\` table. Mirrors the Julia surface; aggregation ops are strings (Lua has no enums);
operators are \`+ - * /\` and unary \`-\`, with scalars allowed on either side.

\`\`\`lua
local md = quiver.metadata{
    initial_datetime = "2025-01-01T00:00:00", unit = "MW",
    labels = {"v1", "v2"}, dimensions = {"stage", "block"}, dimension_sizes = {4, 31},
    time_dimensions = {"stage", "block"}, frequencies = {"monthly", "daily"},
}
local f = db:open_file(path, "w", md)               -- mode "r"/"w"; md required for "w"
f:write({1.0, 2.0}, {stage = 1, block = 1})         -- data table, dims table
f:close()
local r = db:open_file(path, "r")
local cell = r:read({stage = 1, block = 1})         -- { v1, v2 }; pass true as 2nd arg to allow NaN
r:get_metadata(); r:get_file_path(); r:is_open()
md:get_unit(); md:get_version(); md:get_initial_datetime()
md:get_labels(); md:get_dimensions(); md:get_number_of_time_dimensions(); md:to_toml()
quiver.metadata_from_toml(text); quiver.metadata_from_element(tbl)
db:bin_to_csv(path)                                  -- aggregate=true by default; pass false to keep time dims as columns
db:csv_to_bin(path)

local e = (quiver.expression(r) + 10.0) * 2.0        -- files auto-wrap; scalars either side
e = quiver.abs(e); e = quiver.sqrt(e)                -- also quiver.log / quiver.exp
local cond_e = quiver.gt(e, 3.0)                     -- gt/lt/gte/lte/eq/neq -> 1.0/0.0 (NaN -> NaN)
cond_e = quiver.and_(cond_e, quiver.not_(quiver.lt(e, 1.0)))  -- and_/or_/not_ (keywords -> trailing _)
e = quiver.ifelse(cond_e, then_e, else_e)            -- build cond_e with comparison + logical functions
e = e:aggregate("stage", "sum")                      -- sum/mean/min/max/percentile
e = e:aggregate("stage", "percentile", 0.9)          -- percentile needs the fraction
e = e:aggregate_agents("mean")                       -- collapse the label axis
e = e:select_agents({"v2"}); e = e:rename_agents({v1 = "alpha"})
e:save(out_path); e:metadata()                       -- save path is sandboxed like db:open_file
\`\`\`

**\`quiver.metadata{...}\` kwargs and defaults:** \`version\` defaults to \`"1"\`; \`initial_datetime\`
and \`unit\` default to \`""\`; \`labels\`, \`dimensions\`, \`dimension_sizes\`, \`time_dimensions\`, and
\`frequencies\` default to empty arrays.

**\`get_dimensions()\` / \`get_metadata()\` dimension shape** — returns an array of dimension tables:

\`\`\`lua
{
    name                   = "stage",
    size                   = 4,
    is_time_dimension      = true,
    frequency              = "monthly",   -- nil for non-time dimensions
    initial_value          = 1,           -- nil for non-time dimensions
    parent_dimension_index = -1,          -- nil for non-time dimensions
}
\`\`\`

---

## What Lua does *not* expose

DateTime wrapper helpers (Lua uses ISO 8601 strings) and \`_by_id\` single-scalar variants (use the
composite by-id readers or the bulk readers instead). Everything else the native binding exposes —
CRUD, reads, time series, metadata, query, CSV, and the binary/expression subsystems — is
documented above and callable.`;
