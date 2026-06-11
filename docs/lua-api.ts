// Agent-facing reference for the Lua `db` API available inside run_lua scripts.
//
// Authority: quiver/src/lua_runner.cpp `bind_database()` (read-only sibling repo) — extracted by
// hand, NOT imported. The shipped quiverdb native binding is the runtime truth; this is docs.
//
// Drift guard: test/lua-api.test.ts regex-extracts every `db:<name>` token below and probes
// `type(db[name]) == "function"` against a real database through the Lua worker, so a quiverdb
// upgrade that renames or drops a method fails `bun test`.
//
// NOTE: read_time_series_row is bound in quiver's repo HEAD (lua_runner.cpp) but NOT in the shipped
// quiverdb 0.9.5 native binding the agent runs against (verified: the live probe finds no such
// function), so it is intentionally omitted here — re-add it as a `db:read_time_series_row` token
// only after a quiverdb release whose binding exposes it.
//
// FORMAT CONVENTION (load-bearing for the drift test): every method must appear at least once as
// the literal token `db:<snake_case_name>`. Methods the agent must NOT call (the transaction
// methods and export_csv/import_csv) are still listed as `db:<name>` tokens — annotated DO NOT
// CALL — so the drift probe and the >=45-method count keep covering them. Keep SYSTEM_PROMPT under
// the 15000-char bound asserted in test/lua-api.test.ts when editing.
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
| integer            | INTEGER      | Lua integers map to int64.                     |
| number (float)     | REAL         | REAL columns reject bare integers: write 75.0, not 75. |
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

- **STRICT typing.** A REAL column rejects integer literals — write \`75.0\`, not \`75\`; an INTEGER
  column rejects floats. A mismatch raises a validation error and rolls the whole script back.
- **One managed transaction.** run_lua wraps the whole script in a single transaction (see
  Transactions below) — never call the transaction methods yourself.
- **No file access.** Never call \`db:export_csv\` / \`db:import_csv\` (paths are unconfined; disabled
  here). Only the \`base\`, \`string\`, and \`table\` libraries are loaded — there is NO \`math\` (use
  \`//\` for integer division), and no \`os\`, \`io\`, \`require\`, \`load\`, or \`dofile\`.
- **Output.** Scripts cannot return values to the tool — use \`print()\` for anything you need to
  see (it is captured). Arrays are 1-indexed (iterate with \`ipairs\`); reading a NULL yields \`nil\`,
  writing \`nil\` stores NULL.

---

## Database info

\`\`\`lua
db:is_healthy()        -- boolean
db:current_version()   -- integer (current migration version)
db:path()              -- string (database file path)
db:describe()          -- prints schema info to stdout, returns nothing
\`\`\`

---

## Transactions — DO NOT CALL

run_lua already wraps your whole script in ONE transaction (commit on success; rollback on any
error or timeout-kill). These exist but you must **never** call them — a mid-script commit defeats
the all-or-nothing guarantee and a later timeout-kill would no longer roll back partial writes:

\`\`\`lua
db:begin_transaction()   -- DO NOT CALL
db:commit()              -- DO NOT CALL
db:rollback()            -- DO NOT CALL
db:in_transaction()      -- DO NOT CALL
db:transaction(fn)       -- DO NOT CALL (the tool's wrapper already provides the transaction)
\`\`\`

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

### Read a whole group

\`\`\`lua
local ts = db:read_time_series_group(collection, group, id)
-- ts = { date_time = { "2024-01-01T00:00:00", ... }, value = { 10.5, 20.0, ... } }
-- returns an empty table {} if the element has no rows
\`\`\`

### Replace a whole group

**ROW-oriented input:** an array of row tables, each with the dimension column plus its value
column(s). (Reads above are column-oriented; writes are NOT — this asymmetry is real, and passing
column-oriented data here silently writes nothing.) Passing an empty table \`{}\` clears the group.

\`\`\`lua
db:update_time_series_group("Items", "data", id, {
    { date_time = "2024-01-01T00:00:00", value = 10.5 },
    { date_time = "2024-01-02T00:00:00", value = 20.0 },
    { date_time = "2024-01-03T00:00:00", value = 30.0 },
})

db:update_time_series_group("Items", "data", id, {})   -- clears the group
\`\`\`

### Append/upsert a single row

\`\`\`lua
db:add_time_series_row("Items", "data", id, {
    date_time = "2024-01-04T00:00:00",
    value     = 40.0,
})
\`\`\`

Write REAL-column values as float literals (40.0, not 40); STRICT validation rejects an integer for
a REAL column. Unsupported Lua types in a column throw \`Cannot update_time_series_group: column
'...' has unsupported Lua type\`.

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
    dimension_column = "date_time",   -- time-series groups only; absent/"" otherwise
}
\`\`\`

---

## Query (parameterized SQL)

Positional \`?\` placeholders; \`params\` is an optional 1-indexed array. Each returns the first
column of the first row as the requested type, or \`nil\` if there is no result.

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

## CSV import / export — DO NOT CALL

\`\`\`lua
db:export_csv(collection, group, path, options)   -- DO NOT CALL
db:import_csv(collection, group, path, options)   -- DO NOT CALL
\`\`\`

**Never call \`db:export_csv\` or \`db:import_csv\`.** Their path argument is NOT confined to the
workspace, so they could read or overwrite arbitrary files (including the user's original study).
They are disabled in this sandbox and raise an error if called. Edit data with the CRUD and
time-series methods instead.

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

-- update (run_lua already runs the whole script in one transaction)
db:update_element("Collection", item1, { some_integer = 999 })

-- delete
db:delete_element("Collection", item2)
\`\`\`

---

## What Lua does *not* expose

DateTime wrapper helpers (Lua uses ISO 8601 strings), the binary/expression subsystems (\`.qvr\`
files and lazy expressions — Julia-only), and \`_by_id\` single-scalar variants (use the composite
by-id readers or the bulk readers instead).`;
