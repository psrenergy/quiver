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
| number (float)     | REAL         | Integer values are accepted for REAL columns.  |
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

## Database info

\`\`\`lua
db:is_healthy()        -- boolean
db:current_version()   -- integer (current migration version)
db:path()              -- string (database file path)
db:describe()          -- prints schema info to stdout, returns nothing
\`\`\`

---

## Transactions

\`\`\`lua
db:begin_transaction()
db:commit()
db:rollback()
db:in_transaction()    -- boolean
\`\`\`

Plus a block wrapper that begins a transaction, runs your function, and commits — or rolls back
and re-raises if the function errors. It returns the function's first return value (or \`nil\`):

\`\`\`lua
local new_id = db:transaction(function(db)
    local id = db:create_element("Collection", { label = "Item" })
    db:update_element("Collection", id, { value = 10 })
    return id
end)
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

### Read one row across all elements

Returns an array with one value per element, using "last non-null value at or before
\`date_time\`" semantics. Elements with no matching data come back as \`nil\`.

\`\`\`lua
local row = db:read_time_series_row(collection, group, attribute, date_time)
-- e.g. db:read_time_series_row("Items", "data", "value", "2024-01-04T00:00:00")
\`\`\`

### Replace a whole group

Column-oriented input — each value must be an **array**. **All columns must have the same
length.** Passing an empty table \`{}\` (no columns) clears the group.

Anything that would transpose to zero rows throws instead of silently clearing: passing a scalar
where an array is expected (\`{ date_time = "...", value = 5 }\` — the \`add_time_series_row\`
shape) raises \`Cannot update_time_series_group: column '...' must be an array of values\`, and
named columns that are all empty raise \`Cannot update_time_series_group: columns [...] contain no
rows; pass an empty table {} to clear the group\`. Only the explicit \`{}\` clears.

\`\`\`lua
db:update_time_series_group("Items", "data", id, {
    date_time = { "2024-01-01T00:00:00", "2024-01-02T00:00:00", "2024-01-03T00:00:00" },
    value     = { 10.5, 20.0, 30.0 },
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

Integer values are accepted for REAL columns (converted on insert). Unsupported Lua types in a
column throw \`Cannot update_time_series_group: column '...' has unsupported Lua type\`.

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

## CSV import / export

\`\`\`lua
db:export_csv(collection, group, path, options)   -- options optional
db:import_csv(collection, group, path, options)   -- options optional
\`\`\`

The optional \`options\` table:

\`\`\`lua
{
    date_time_format = "%Y-%m-%d",     -- string
    enum_labels = {                    -- { attribute = { locale = { label = int } } }
        status = {
            en = { active = 1, inactive = 0 },
        },
    },
}
\`\`\`

Note: \`import_csv\` refuses to run inside an open transaction (it toggles \`PRAGMA foreign_keys\`,
a no-op mid-transaction) and will throw \`Cannot import_csv: transaction already active\`.

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

-- update inside a transaction block
db:transaction(function(db)
    db:update_element("Collection", item1, { some_integer = 999 })
end)

-- time series
db:update_time_series_group("Collection", "data", item1, {
    date_time = { "2024-01-01T00:00:00", "2024-01-02T00:00:00" },
    value     = { 10.5, 20.0 },
})
db:add_time_series_row("Collection", "data", item1, {
    date_time = "2024-01-03T00:00:00",
    value     = 30.0,
})
local ts = db:read_time_series_group("Collection", "data", item1)

-- delete
db:delete_element("Collection", item2)
\`\`\`

---

## What Lua does *not* expose

- **DateTime wrappers.** Lua uses a string-based datetime surface (ISO 8601 strings); there are
  no \`read_scalar_date_time_by_id\`-style parsing helpers as in Julia/Dart/Python.
- **The binary and expression subsystems** (\`.qvr\` files, lazy expressions) are Julia-only and
  are not bound in Lua.
- **\`read_element_by_id\` has no \`_by_id\` single-scalar variants** (e.g. \`read_scalar_integer_by_id\`);
  use the composite by-id readers or the bulk readers instead.`;
