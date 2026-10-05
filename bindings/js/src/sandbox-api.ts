// Agent-facing reference for the Lua `db` API available inside run_lua scripts.
//
// Authority: the binders under `src/sandbox/` (`bind_database` through `bind_expression`,
// this repo) — extracted by hand, NOT imported.
// The shipped quiverdb native binding is the runtime truth; this is docs.
//
// SYNC: `test/sandbox-api-sync.test.ts` derives the bound surface from every `.cpp`/`.h` under
// `src/sandbox/` and checks
// it AUTOMATICALLY — every `db:`/`quiver.*` name is documented, no documented name has been
// removed, and the stdlib sentence matches `open_libraries` exactly. What it CANNOT check, and you
// must still re-diff by hand when the binding changes: arg order, arity, arg types, return shapes,
// and whether the prose is semantically true.
//
// NOTE: the binary/expression subsystems are bound in the native binding and documented below.
// File-touching operations are sandboxed to the database file's directory (the "Filesystem
// sandbox" bullet lists all of them); the pure-metadata builders stay under the quiver.* global.
//
// FORMAT CONVENTION: every db: method appears at least once as the literal token
// `db:<snake_case_name>`, and every quiver.* function as `quiver.<name>`, so coverage is greppable
// (the sync test relies on this).
export const SANDBOX_API_REFERENCE = `
# Quiver Sandbox API Reference

Quiver embeds a Lua sandbox scripting layer that exposes the same database API as the other language
bindings. This document is a complete reference of every method available to a Lua script.

## Running scripts

The database is provided to your script as a global userdata named \`db\`. There are **no
open/close lifecycle methods in Lua** — the \`Sandbox\` opens the database and hands you \`db\`
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
| boolean            | INTEGER 1/0  | SQLite has no boolean type; \`true\` writes 1, \`false\` 0. |
| \`nil\`              | NULL         | In query params (not trailing), ts rows, group cells, relations. |
| table (1-indexed)  | array        | Used for vectors/sets and column-oriented data.|

A boolean is accepted **wherever an integer is** — element scalars and arrays, query parameters,
the group writers, and \`upsert_time_series_row\` — so it also reaches a REAL column through the
usual int-for-REAL coercion. Reading is the asymmetric half: there are no boolean readers in Lua
(unlike Julia/Dart/Python/JS), so a stored flag comes back as \`0\`/\`1\` from
\`read_scalar_integers\`. Compare against 0 rather than truth-testing —
\`read_scalar_integers(c, a)[i] == 1\` — because in Lua a \`nil\` from a NULL cell is *not* equal to
0 and \`nil ~= 0\` evaluates to \`true\`.

The one place a boolean is deliberately refused is \`db:update_relation\`, where the argument is a
target label and only \`nil\` (or omitting it) may clear the relation.

**Unsupported types throw.** Passing a function or a nested table where a scalar is expected raises
an error rather than silently dropping the value. This applies to element attributes, time-series
rows, and query parameters. A skipped positional query parameter would shift every later parameter
and bind NULL to the trailing placeholder, so this is rejected loudly.

Dates are plain strings in ISO 8601 format: \`YYYY-MM-DDTHH:MM:SS\`. (Lua keeps a string-based
datetime surface — there are no DateTime wrapper helpers, unlike Julia/Dart/Python.) The time part
is optional, so \`"2024-01-15"\` is also valid, and a space may replace the \`T\`. Every field is
fixed-width and zero-padded. Anything shorter or malformed — \`"2005"\`, \`"2005-01"\`,
\`"2024-02-31"\`, \`"2024-1-5"\`, \`"2024-01-15T10:30"\` — is **rejected when you write it**, not
silently stored. The value is stored exactly as written; a date-only value is not padded to
midnight.

---

## Critical rules

- **Type coercion.** An integer is accepted for a REAL column (coerced to real on insert); a float
  is rejected for an INTEGER column. A string bound to a \`date_*\` column must parse as ISO 8601
  (\`YYYY-MM-DD\`, optionally \`THH:MM:SS\` or \` HH:MM:SS\`). Other type mismatches raise a
  validation error.
- **An uncaught error aborts the script, and the script is not rolled back.** Any error thrown by
  a \`db:\` call that you do not catch with \`pcall\` stops the script and surfaces as
  \`Failed to run Lua script: <message>\`. Outside a transaction each \`db:\` write commits on its
  own, so the failing call changes nothing in the database, but every write that finished before
  it **stays**. Only \`db:transaction(fn)\` and \`db:dry_run(fn)\` undo their block's database
  writes when \`fn\` errors; nothing undoes a file a \`db:\` call wrote. A
  \`db:begin_transaction()\` (or \`db:begin_dry_run()\`) you opened and never closed stays open
  after the error, and closing it is up to the host. A host that runs your script inside its own
  transaction or dry run may undo the whole run; check \`db:in_transaction()\` if it matters. To
  make several writes all-or-nothing, put them in one \`db:transaction(function(db) ... end)\`
  and do not \`pcall\` inside it: a caught error lets the block commit whatever ran.
- **Standard library.** Loaded standard libraries: base, string, table, math, coroutine, utf8.
  That is the pure-computation set — there is no \`os\`, \`io\`, \`debug\`, or \`package\`/\`require\`,
  and \`dofile\`/\`loadfile\` are removed (string-form \`load\` stays for source text; a precompiled
  binary chunk is refused). Integer division is the Lua 5.4 \`//\` operator — a language operator,
  unrelated to \`math\`. No \`io\` does **not** mean
  a data file on disk is out of reach: read it with \`db:read_csv\` / \`db:read_csv_stream\` (see
  the CSV file reading section below). Never copy, paste, or re-type a data file's contents into
  the script as literals — read the file.
- **Filesystem sandbox.** Every file-touching operation (\`db:export_csv\`, \`db:import_csv\`,
  \`db:open_file\`, \`db:bin_to_csv\`, \`db:csv_to_bin\`, \`db:validate_migrations\`, \`db:read_csv\`,
  \`db:read_csv_stream\`, \`db:write_csv\`, \`save\` on a file or an expression) resolves
  relative paths against the directory containing the database file and rejects anything outside it
  (subdirectories are fine; \`..\` escapes and outside absolute paths throw \`Cannot <op>: path '...' escapes the
  database directory ...\`). On an in-memory database these operations throw
  \`Cannot <op>: database is in-memory, file operations are unavailable\`.
- **What the sandbox does not limit.** The sandbox controls which files a script can touch and
  which standard libraries exist. It does not bound how much work a script does: there is no
  instruction-count limit, no memory cap and no wall-clock timeout (\`while true do end\` runs
  until the host stops it). Globals persist across \`run()\` calls on the same runner, so a global
  one script sets is visible to the next; use \`local\`. A host that runs untrusted scripts has to
  impose those limits outside the library.
- **Output.** A script can \`return\` one value and the host receives it as JSON — prefer this over
  \`print()\` when you need structured data back (\`print()\` still works and is captured). Only the
  **first** returned value is encoded. Arrays are 1-indexed. Iterate with \`ipairs\` only where no
  NULL can appear: a read of a nullable column (a bulk scalar read, a vector/set inner list, a
  time-series value column, \`db:read_time_series_row\`) has \`nil\` holes, and \`ipairs\` stops at
  the first one (see Scalar reads). Reading a NULL yields \`nil\`; writing \`nil\` stores NULL where
  NULL is accepted (query params except a trailing one, ts rows, group cells, relation targets —
  but NOT element scalar attributes; see CRUD).

  \`\`\`lua
  return { ids = db:read_element_ids("Collection"), total = 3 }
  -- host receives: {"ids":[1,2,3],"total":3}
  \`\`\`

  | Returned                   | JSON                                                    |
  | -------------------------- | ------------------------------------------------------- |
  | nothing                    | \`""\` (empty — distinct from \`nil\`)                      |
  | \`nil\`                      | \`null\`                                                  |
  | integer / float / string   | the value; a float keeps its shortest round-trip form   |
  | boolean                    | \`true\` / \`false\`                                        |
  | NaN, \`math.huge\`           | \`null\` (JSON has no NaN/Infinity)                       |
  | table keyed \`1..n\`         | array — an empty table \`{}\` encodes as \`[]\`             |
  | any other table            | object, keys sorted; integer keys stringify              |

  **A table with holes is an object, not an array.** A bulk read of a nullable column returns
  \`nil\` holes (see Scalar reads), so \`return db:read_scalar_integers(c, a)\` encodes as
  \`{"1":10,"3":30}\` — not \`[10,null,30]\` — and the keys sort as text (\`"1","11","2"\`). When the
  host needs positional scalar data, return the ids alongside and fill the holes yourself:
  \`local ids = db:read_element_ids(c); local v = db:read_scalar_integers(c, a); local out = {}; for i = 1, #ids do out[i] = v[i] or false end\`.
  **Inside** a vector/set read an inner list with an interior NULL cell encodes as an object nested
  in the outer array (\`[{"1":10,"3":30},[]]\`), but a trailing NULL cell leaves no trace at all:
  \`[10, NULL]\` encodes as the plain \`[10]\`. The ids count elements, not rows, so they cannot
  restore inner positions — take the row count from a \`NOT NULL\` column of the group (see
  *Vector reads*), or do that read in the host binding.

  Returning a function, a coroutine, or a userdata (including \`db\` itself) raises
  \`Cannot run: script returned an unsupported Lua type\`; nesting deeper than 32 levels raises
  \`Cannot run: script return value nests deeper than 32 levels\` (this is also what stops a
  self-referencing table); a result over 64 MB raises
  \`Cannot run: script return value exceeds ... bytes of JSON\`; a string holding bytes that are not
  valid UTF-8 raises \`Cannot run: script return value contains a string that is not valid UTF-8\`;
  a table where an integer key and a string key spell the same thing (\`{[1] = 'a', ['1'] = 'b'}\`)
  raises \`Cannot run: script returned a table with duplicate key '1'\`; and a table with a key that
  is neither an integer nor a string (a boolean, a float such as \`1.5\`, a table) raises
  \`Cannot run: script returned a table with an unsupported key type\`.
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
db:number_of_elements(collection)  -- integer: how many elements the collection holds right now
db:describe()                      -- string: whole-DB text report (returns it, does NOT print)
db:describe_collection(collection) -- string: one collection's structure (text report)
db:summarize_collection(collection)-- string: per-scalar null/non-null counts, low-cardinality
                                   --         integer value distributions, per-group sizes
db:validate_migrations(path)       -- validate a migrations dir (up then down) in-memory; no return
\`\`\`

All three \`describe*\`/\`summarize*\` methods **return** a string — \`print()\` it to see it.

\`db:validate_migrations(path)\` applies every \`up.sql\` in version order, then every \`down.sql\` in
reverse, against a throwaway in-memory database — nothing in \`db\` itself is touched. The round trip
must end with an empty database; leftover tables are named in the error.

---

## Transactions

\`\`\`lua
db:begin_transaction()   -- start an explicit transaction
db:commit()              -- commit it
db:rollback()            -- roll it back
db:in_transaction()      -- boolean: is a transaction currently open?
db:transaction(fn)       -- run fn(db) inside begin/commit; rollback + rethrow if fn errors or the commit fails
\`\`\`

To make a group of writes atomic, prefer the \`db:transaction\` wrapper:

\`\`\`lua
db:transaction(function(db)
    local id = db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    db:update_element("Collection", id, { some_integer = 99 })
end)   -- both writes commit together; if either throws, both roll back
\`\`\`

**Caveat:** if the host already runs your script inside a plain transaction (not a dry run), an
explicit \`db:begin_transaction()\` will error
(\`Cannot begin_transaction: transaction already active\`) and a mid-script \`db:commit()\` would
prematurely end the host's transaction. \`db:transaction(fn)\` begins one too, so it fails the same
way. When unsure whether a transaction is already open, check \`db:in_transaction()\` first, or just issue writes directly:
outside a transaction each \`db:\` write commits on its own, while inside the host's transaction it
commits or rolls back with the host's.

---

## Dry runs

A dry run executes writes and then throws them away. Use it to check that a sequence works — that
the collections exist, the types match, the foreign keys resolve — before committing to it.

\`\`\`lua
db:dry_run(fn)           -- run fn(db), roll everything back, return fn's result
db:begin_dry_run()       -- start one explicitly
db:end_dry_run()         -- end it, rolling back everything it covered
db:in_dry_run()          -- boolean: is a dry run currently active?
\`\`\`

\`\`\`lua
local preview = db:dry_run(function(db)
    db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    return db:read_element_ids("Collection")   -- reads see the uncommitted writes
end)
-- nothing was kept; preview holds what the reads saw
\`\`\`

Rules worth knowing:

- **Nested transaction control is absorbed.** Inside a dry run, \`db:begin_transaction\`,
  \`db:commit\` and \`db:rollback\` become no-ops, so the \`db:transaction\` pattern above composes
  instead of erroring — \`db:transaction(fn)\` still runs \`fn\` and still performs its writes, only
  its BEGIN/COMMIT are absorbed. The flip side: a nested \`db:rollback\` does **not** partially
  undo — everything is undone when the dry run ends, whatever the nested calls asked for.
- \`db:in_transaction()\` still reports \`true\` during a dry run: a real transaction is open.
- \`db:import_csv\` cannot run inside a dry run (it manages its own transaction, and a dry run is a
  real one) and throws \`Cannot import_csv: transaction already active\`.
- **Dry runs do not nest.** The host may have already opened one around your whole script, in which
  case both \`db:begin_dry_run()\` and \`db:dry_run(fn)\` (which calls it internally) error with
  \`Cannot begin_dry_run: dry run already active\` — check \`db:in_dry_run()\` first and skip the
  wrapper when one is already active. Do **not** call \`db:end_dry_run()\` to get around it: that
  ends the host's dry run, and everything you write afterwards is committed for real.
- **Nor inside a plain transaction.** If the host (or a \`db:transaction\` block) already holds a
  plain transaction, \`db:begin_dry_run()\` and \`db:dry_run(fn)\` throw
  \`Cannot begin_dry_run: transaction already active\`. \`db:in_transaction()\` is true under a host
  dry run *and* under a host transaction, so it is the one check that covers both;
  \`db:in_dry_run()\` covers only the first.
- For a rough sense of how much a run touched, \`db:query_integer("SELECT total_changes()")\` gives
  the number of rows inserted, updated or deleted on this connection.

---

## CRUD

\`\`\`lua
local id = db:create_element(collection, element_table)   -- returns new integer id
db:update_element(collection, id, element_table)
db:update_element_by_label(collection, label, element_table)  -- same update, addressed by label
db:delete_element(collection, id)
db:delete_element_by_label(collection, label)             -- same delete, addressed by label

db:update_relation(collection_from, collection_to, relation_type, id, target_label)
db:update_relation_by_label(collection_from, collection_to, relation_type, label, target_label)
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
- **\`update_element_by_label\` / \`delete_element_by_label\` require an existing label**, unique
  *per collection*, not per database — one naming an element of another collection does not
  resolve. A miss throws \`Element not found: label '<label>' in collection '<collection>'\` and
  changes nothing. Passing \`label = "New name"\` in the element table renames the element, after
  which only the new label resolves. Because the label form delegates to the id form, failures
  that validate the *element* (an empty table, a type mismatch) report
  \`Cannot update_element: ...\`.
- **An empty array clears on update.** On \`update_element\` / \`update_element_by_label\`,
  \`{ col = {} }\` clears the whole group holding \`col\` (all its columns, and every group that
  shares the column name). An empty column beside a non-empty column of the same group throws a
  length error, and a misspelled empty column throws
  \`array '<name>' does not match any vector, set, or time series table ...\`. \`create_element\`
  skips an empty array. To leave a group alone, omit its column: writing back a
  \`read_vectors_by_id\` result clears a group whose read column came back empty or all-NULL.
- **Arrays must be dense.** A vector/set read returns a NULL cell as a \`nil\` hole, but an element
  array cannot carry one: \`create_element\` / \`update_element\` throw \`array '<name>' has a nil
  hole ...\` rather than cut the array short at the hole. Write NULL cells with
  \`update_vector_group\` / \`update_set_group\`, which write a hole as NULL.
- **No \`nil\` scalar attributes.** In Lua a key set to \`nil\` is dropped from the table, so
  \`{ x = nil }\` is identical to \`{}\`; an update/create table that ends up with no attributes
  **throws** (\`...must have at least one scalar attribute\` on create, \`...at least one attribute
  to update\` on update). To leave a column unchanged, omit the key — you cannot set a scalar to
  NULL via the element table. (\`nil\` → NULL is accepted by \`update_relation\` and in query params
  (not a trailing one), and as a cell in the group writers; \`upsert_time_series_row\` and
  \`update_time_series_files\` replace the whole row, so a column you leave out (or set to \`nil\`,
  the same thing) is cleared — see Time series and Time series files.)
- **\`update_relation\` points one scalar foreign-key relation at another element**, named by the
  target's label. The column is derived from the naming convention —
  \`lowercase(collection_to) .. "_" .. relation_type\`, so
  \`db:update_relation("Child", "Parent", "id", id, "Parent A")\` writes \`Child.parent_id\`. A
  \`nil\` or omitted \`target_label\` clears the relation; anything that is not a string throws
  (\`target_label has unsupported Lua type\`). The derived column must exist and be a
  foreign key to \`collection_to\`, otherwise \`Cannot update_relation: ...\`. The write delegates
  to \`update_element\`, so a missing id reports that method's error;
  \`update_relation_by_label\` takes a label in place of the id, with
  \`update_element_by_label\`'s resolution and miss semantics. A relation living in a vector, set
  or time-series group is a list of targets — use that group's writer instead.

---

## Scalar reads (bulk, across all elements)

Each returns a flat array (1-indexed table), one value per element, in id order. A NULL is a
\`nil\` hole at that position, so \`ipairs\` stops at the first one and \`#\` is unreliable. Loop
\`for i = 1, #ids\` over \`local ids = db:read_element_ids(collection)\` and index the result with
\`i\`.

\`\`\`lua
db:read_scalar_integers(collection, attribute)   -- { 42, 37, ... }
db:read_scalar_floats(collection, attribute)      -- { 3.14, 2.71, ... }
db:read_scalar_strings(collection, attribute)     -- { "Item 1", "Item 2", ... }
\`\`\`

---

## Vector reads (bulk)

Each returns an array of arrays — one inner array per element, aligned with
\`db:read_element_ids\`; an element with no rows is \`{}\`. A NULL cell is a \`nil\` hole, so on a
nullable column \`#\` and \`ipairs\` are unreliable on an inner list, and a trailing NULL is
invisible (\`[10, NULL]\` reads as \`{10}\`; a NULL-only row reads as \`{}\`).

\`\`\`lua
db:read_vector_integers(collection, attribute)   -- { {1,2,3}, {10,nil,30}, {}, ... }
db:read_vector_floats(collection, attribute)
db:read_vector_strings(collection, attribute)
\`\`\`

---

## Set reads (bulk)

Same shape and NULL handling as vector reads — an array of arrays with \`nil\` holes.

\`\`\`lua
db:read_set_integers(collection, attribute)
db:read_set_floats(collection, attribute)
db:read_set_strings(collection, attribute)
\`\`\`

---

## Replace a whole vector or set group (column-oriented)

\`update_vector_group\` / \`update_set_group\` replace **all** of one element's rows in one *named*
group, taking the same column-oriented shape as the time-series writer:

\`\`\`lua
db:update_vector_group("Child", "refs", id, { parent_ref = { 1, 2, 3 } })
db:update_set_group("Child", "parents", id, { parent_ref = { 1, 2 } })

db:update_vector_group("Child", "refs", id, {})   -- clears the group

db:update_vector_group_by_label("Child", "refs", "Child 1", { parent_ref = { 1, 2 } })
db:update_set_group_by_label("Child", "parents", "Child 1", { parent_ref = { 1, 2 } })
\`\`\`

Use these instead of routing a group's columns through \`update_element\` whenever a column name is
shared by two groups of the collection (legal for foreign-key columns): \`update_element\` routes an
array **by column name**, so it writes to *every* group table that has that column — silently
rewriting groups you never named. \`(collection, group)\` names exactly one table.

Rules:
- **Row count is the largest index any column reaches.** Shorter or sparse columns write NULL in
  the gaps, so interior \`nil\` holes from a read round-trip. A trailing row that is NULL in every
  column is invisible to a read (see *Vector reads*), so writing a read back drops it.
- **\`{}\` (no columns) clears the group.** Naming a column whose array is empty is an error, not a
  clear — a typo'd column name must not destroy data.
- **\`id\` and \`vector_index\` are managed by the group** (the element and the row's position) and
  are rejected if passed.
- **Foreign-key columns accept a label string** and resolve it to the referenced id, exactly as in
  \`create_element\` / \`update_element\`.
- The element id must exist, same as \`update_element\` / \`delete_element\`; the \`_by_label\` form
  takes a label in its place, with \`update_element_by_label\`'s resolution and miss semantics.

---

## Composite by-id reads (Lua convenience helpers)

\`\`\`lua
db:read_element_ids(collection)                  -- { 1, 2, 3, ... }

db:read_scalars_by_id(collection, id)            -- { attr = value, ... } (missing -> nil)
db:read_vectors_by_id(collection, id)            -- { column = { v1, nil, v3, ... }, ... }
db:read_sets_by_id(collection, id)               -- { column = { v1, nil, v3, ... }, ... }
db:read_element_by_id(collection, id)            -- scalars + vectors + sets merged into one table
\`\`\`

\`read_element_by_id\` merges every scalar, vector, and set for the element into a single table.
Scalar attributes with no value come back as \`nil\`.

**Group columns keep NULL cells as \`nil\` holes**, so cell *i* of every column of one group is
the same row. Zipping them needs the row count: \`#\` of a \`NOT NULL\` column of the group.
There is no row-shaped group read in Lua. The result is keyed by column name, so when two vector
(or two set) groups share a column name, which a foreign key may, that name appears once and is
read from the group it resolves to (the one named after it, else the one whose table name sorts
first) — do not zip it with the other group's columns.

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
with no matching data yield \`nil\` in the array, so the result can hold \`nil\` holes: loop over
\`db:read_element_ids(collection)\` rather than using \`ipairs\`. \`date_time\` is an ISO 8601
string. A group with more than one dimension column (e.g. \`date_time\` + \`block\`) throws — read
it with \`read_time_series_group\`.

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

db:update_time_series_group_by_label("Items", "data", "Item 1", { date_time = { "2024-01-01T00:00:00" }, value = { 10.5 } })
\`\`\`

A read-modify-write looks like this:

\`\`\`lua
local ts = db:read_time_series_group("Items", "data", id)
ts.value[2] = 125.0                                    -- edit the 2nd row's value
db:update_time_series_group("Items", "data", id, ts)   -- write the whole group back
\`\`\`

**DO NOT pass an array of row tables** (\`{ { date_time = ..., value = ... }, ... }\`) — that is the
\`upsert_time_series_row\` shape, not this one. Doing so raises
\`Cannot update_time_series_group: column names must be strings; pass { column = { values... } }, not an array of row tables\`.
Each value of the top-level table must be an **array**, not a scalar.

**Rules** (a violation throws; see "Errors abort the script" for what is kept):
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
- Integer values are accepted for REAL columns (converted on insert), and a boolean is written as
  1/0. A function, a table or another unsupported Lua type throws
  \`column '...' has unsupported Lua type\`.
- The element id must exist; the \`_by_label\` form takes a label in its place, with
  \`update_element_by_label\`'s resolution and miss semantics. Every rule above applies to both.

### Append/upsert a single row (\`upsert_time_series_row\` — ROW-oriented, the one exception)

Unlike the column-oriented group update above, this takes **one row table of scalars** (dimension
column + value column(s)), and upserts that single row:

\`\`\`lua
db:upsert_time_series_row("Items", "data", id, {
    date_time = "2024-01-04T00:00:00",
    value     = 40.0,
})

db:upsert_time_series_row_by_label("Items", "data", "Item 1", {
    date_time = "2024-01-04T00:00:00",
    value     = 40.0,
})
\`\`\`

The element id must exist; the \`_by_label\` form takes a label in its place, with
\`update_element_by_label\`'s resolution and miss semantics.

An existing row with the same dimension key is **replaced whole**: every value column you leave
out (or set to \`nil\`) is reset to NULL (or its DEFAULT). To change one value, pass every value
column of the row.

---

## Time series files

For schemas that reference external time-series files (the \`{Collection}_time_series_files\`
singleton table):

\`\`\`lua
db:has_time_series_files(collection)              -- boolean
db:list_time_series_files_columns(collection)     -- { "data_file", "metadata_file", ... }
db:read_time_series_files(collection)             -- { data_file = "path", metadata_file = nil, ... }
db:update_time_series_files(collection, { data_file = "path/to/data.bin" })  -- metadata_file is cleared
\`\`\`

\`update_time_series_files\` **replaces the whole row**: every column you do not give a string is
set to NULL (or to its DEFAULT, if the schema declares one), and in Lua a \`nil\` value and a
missing key are the same thing. A table with no string values (\`{}\`, or only \`nil\` values)
changes nothing and clears nothing. A value that is not a string throws. To change one path, read
the row with \`db:read_time_series_files(collection)\`, change that one entry, and pass the whole
table back.

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
column of the first row, or \`nil\` when there is no row, the value is NULL, or it is not already
the requested type. Only \`query_float\` converts (it widens an INTEGER), so
\`db:query_string("SELECT COUNT(*) ...")\` and \`db:query_integer("SELECT AVG(x) ...")\` return
\`nil\`; \`CAST\` in the SQL when unsure (\`SELECT CAST(AVG(x) AS INTEGER)\`). The number of
\`params\` must match the number of \`?\` placeholders exactly — a mismatch (too few or too many)
throws rather than binding NULL or ignoring extras. A \`nil\` param binds NULL only in a table
constructor whose last entry is not \`nil\` (\`{ nil, 5 }\`): Lua stores no key for a \`nil\`, so
\`{ 5, nil }\` or \`{ nil }\` comes up short and throws that mismatch. To test for NULL, write
\`IS NULL\` in the SQL.

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

Export one table of \`collection\` to a CSV file, or import one from a CSV file. \`group\` names a
vector, set or time-series group of \`collection\`; pass \`""\` for the collection's own scalar table
(the CSV then holds exactly the table's columns except \`id\`, \`label\` included). \`path\` is
sandboxed: relative paths resolve against the database file's directory and must stay inside it
(see Critical rules); \`options\` is optional.

**\`db:import_csv\` replaces, it does not merge.** A group import deletes every existing row of
that group, for every element, not only the elements named in the file, and then inserts the
file's rows. A scalar import (\`group = ""\`) matches elements by exact label. An element whose
label is in the file keeps its id and has every column overwritten from the file (a blank cell
writes NULL). **Every element whose label is not in the file is deleted** as
\`db:delete_element\` would delete it: its group rows go with it, and every relation pointing at
it from another table follows its \`ON DELETE\` action (\`SET NULL\` clears it, \`CASCADE\`
deletes the row that holds it, a group row or an element of another collection). So editing a
label in the file deletes that element and creates a new one. A header-only CSV empties the
table. To add or change a few elements, use \`db:create_element\` / \`db:update_element\` instead.

\`\`\`lua
db:export_csv(collection, group, path, options)
db:import_csv(collection, group, path, options)
\`\`\`

The optional \`options\` table has two keys:

\`\`\`lua
{
    date_time_format = "%Y-%m-%d",   -- strftime-style format for every date-time column

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

An options value that is not a table, an unknown key or a wrong-typed value throws
(\`Cannot export_csv: unknown option '...'\`).

**Precondition:** \`db:import_csv\` cannot run inside an open transaction (it opens and commits its
own) — it throws \`Cannot import_csv: transaction already active\`. Call it outside any
\`db:transaction\` / \`db:begin_transaction\` block.

---

## CSV file reading

Read a CSV file from disk directly into Lua — the only way to get file data into a script, since
\`io\` is deliberately absent from the sandbox. \`path\` is sandboxed the same way as every other
file-touching operation (see Critical rules).

\`\`\`lua
local csv = db:read_csv(path, { separator = ",", header_row = 1 })   -- { header = {...}, rows = {{...}, ...} }
\`\`\`

Every cell arrives as a **string**, with no numeric or date inference — \`"0012"\` stays \`"0012"\`
and a date stays text. \`csv.header\` is a 1-based array of the file's column names in file order;
\`csv.rows\` is a 1-based array of rows, each a 1-based array of strings, addressed positionally
(\`csv.rows[1][1]\`). Rows are never padded to header width — a short row stays short and a field
past its end is \`nil\`. A 0-byte file throws \`Cannot read_csv: file '<path>' is empty\`; a
header-only file returns a populated \`header\` and an empty \`rows\`.

The options table is optional; its two keys are \`separator\` and \`header_row\`. \`header_row\` is
1-based (like every other index here) and defaults to \`1\`; \`header_row = 0\` declares the file has
no header at all, so \`csv.header\` is absent (\`nil\`, not an empty table) and \`csv.rows[1]\` is the
file's first line — useful for a file with a junk title row and/or a units row around the real
header (skip them by naming the header row and slicing \`csv.rows\` in the script). A \`header_row\`
past the end of the file throws. Passing the separator positionally (\`db:read_csv(path, ";")\`)
throws \`Cannot read_csv: options must be a table, got string\` instead of silently parsing with
a comma; an unknown key, a separator that isn't a single character (or is a quote, CR, LF or NUL — none of
those can be a delimiter), a non-string option key, or a \`header_row\` that isn't a
non-negative integer also throws.

**Reading a file \`db:export_csv\` wrote:** \`export_csv\` emits an Excel-style \`sep=,\` preamble as
line 1, so its real header is line 2 — read it with \`{ header_row = 2 }\`. With the default
\`header_row = 1\` the preamble itself becomes a two-column header and the column names come back
as \`rows[1]\`.

\`db:read_csv_stream\` reads the same file through the same parser, row by row, so the process holds
a bounded window instead of the whole file:

\`\`\`lua
local n = db:read_csv_stream(path, function(row, index, header)
    -- row: same shape as db:read_csv's rows; index: the 1-based ordinal of this DATA row (the
    -- header row is not counted, so skipping index 1 to "skip a header" drops a real record);
    -- header: the same array db:read_csv returns, reachable here so a column can be found by
    -- name before processing row 1.
    return row[1] ~= ""     -- returning false stops the read early; a bare comparison as the
end, { separator = "," })   -- last statement can silently truncate the stream this way
-- n counts rows FED to the callback, not rows it kept -- a filtering callback logging n as
-- "imported" would be wrong.
\`\`\`

A Lua error raised inside the callback propagates to the host verbatim, and the file is closed.

**Worked example**, over two real files that used to be hand-transcribed into scripts instead of
read from disk:

\`\`\`lua
-- File 1: BOM + CRLF, a junk title row above the header, a units row below it, apostrophe
-- thousands separators, and a DD/MM/YYYY date.
local csv = db:read_csv("ma_energia_residencial.csv", { header_row = 2 })
-- header_row = 2 skips the block-title junk row (line 1). rows[1] is the units row that sits
-- BELOW the header (line 3) -- not a reader concern -- so real data starts at rows[2].
for i = 2, #csv.rows do
    local row = csv.rows[i]
    local dd, mm, yyyy = row[5]:match("(%d%d)/(%d%d)/(%d%d%d%d)")
    local date_key = yyyy .. "-" .. mm
    -- gsub returns TWO values (string, replacement count). tonumber(row[6]:gsub("['%s]", ""))
    -- would hand the count to tonumber as its BASE argument and silently return nil, no error.
    -- The parentheses below truncate the call to one value -- this is the correct form.
    local value = tonumber((row[6]:gsub("['%s]", "")))
end

-- File 2: header is line 1 (the default, no header_row needed), a quoted field containing a
-- comma, and English month names -- os is unloaded, so there is no date library to lean on.
local MONTHS = {
    January = 1, February = 2, March = 3, April = 4, May = 5, June = 6,
    July = 7, August = 8, September = 9, October = 10, November = 11, December = 12,
}
local gd = db:read_csv("ma_gd_data.csv")
for i = 1, #gd.rows do
    local row = gd.rows[i]
    -- The date cell is a quoted field containing a comma ("May 1, 2014"); the row still has
    -- exactly 2 fields -- mishandled quoting would have split the date and shifted this value.
    local month_name, _, year = row[1]:match("(%a+) (%d+), (%d+)")
    local date_key = string.format("%d-%02d", tonumber(year), MONTHS[month_name])
    local value = tonumber(row[2])  -- already a plain decimal string, no cleanup needed
end
\`\`\`

---

## CSV file writing

Write a CSV file to disk — the only way to get data out of a script onto disk, since \`io\` is
deliberately absent from the sandbox. Streaming only, with no whole-file counterpart: \`db:write_csv\`
returns a handle, \`w:write_row({...})\` appends one row, \`w:close()\` finishes it. \`path\` is
sandboxed the same way as every other file-touching operation (see Critical rules).

\`\`\`lua
local w = db:write_csv(path, { separator = ",", header = { "name", "note", "active", "score" } })
local rows = {
    { "Alpha", "first", true, 42 },
    { "Beta", nil, false, 3.5 },     -- nil is INTERIOR, not the row's last cell: a TRAILING nil
                                      -- would shorten the row instead of writing an empty cell.
}
for _, row in ipairs(rows) do
    w:write_row(row)
end
w:close()
\`\`\`

The options table is optional; its only two keys are \`separator\` (a single character, default
\`,\`) and \`header\` (column names written as the first record, default none — no header row). A
quote, CR, LF or NUL is rejected as a separator: none of them can be a delimiter, and a file
written with one could not be read back.

Opening \`db:write_csv\` **truncates** an existing file at the target path — there is no overwrite
guard, so a script can destroy an existing file in the case folder (including the database file
itself) by writing to its path. This is documented behaviour, not a bug: reopening the same path
always starts a fresh file. Two writers open on the *same* path at once is refused, though
(\`Cannot write_csv: file is already open for writing: ...\`) — the second would truncate what the
first is still buffering. Close the first writer before reopening its path.

\`write_row\` after \`close\` throws; \`close\` is idempotent (a second call is a no-op, not an error).

A number is written in its shortest round-trip form (\`std::to_chars\`, no synthetic decimal point),
so a whole float like \`2014.0\` and the integer \`2014\` write identical text — a script that needs
a decimal point writes the cell as a string. A boolean writes \`1\`/\`0\`, matching the project-wide
boolean-is-INTEGER write policy.

\`nil\` and an empty string are not always the same thing here. An INTERIOR \`nil\` cell (not a row's
last cell, like \`"Beta"\`'s note above) writes an empty cell, indistinguishable from \`""\` after the
round trip — CSV has no null. A TRAILING \`nil\`, however, is not a cell at all: Lua stores no key
for it, so the row's maximum integer key is lower. **With no \`header\`** that makes the row come
back **one column narrower**, and a script that needs a trailing empty column must write an empty
string there, not \`nil\`. **With a \`header\`** the padding rule below fills the gap, so the row is
header-width either way.

With a \`header\`, its length is the row width: a \`write_row\` shorter than the header pads with
empty cells, and a longer one throws, naming the row's ordinal and both counts. Omitting \`header\`
disables the check entirely — rows of any length are written as-is.

A writer never explicitly closed is still flushed and closed when the script's \`run()\` call
returns — whether or not the script still holds it (a \`local\` that went out of scope and a global
alike) — so the file is complete and re-readable even without a \`w:close()\` call, and no warning
is emitted. The writer does not survive that \`run()\`: using the same handle from a later
\`run()\` throws \`Cannot write_row: writer for '...' is already closed\`.

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
operators are \`+ - * /\` and unary \`-\`, with scalars allowed on either side. A binary file **is** an
expression: every operator, every \`quiver.*\` expression function and every expression method
(\`aggregate\`, \`aggregate_agents\`, \`select_agents\`, \`rename_agents\`, \`save\`, \`get_metadata\`) takes
a file handle directly; \`quiver.expression(f)\` stays as an explicit conversion, and saving from a
file reads it by path and leaves the handle open. A wrong operand raises \`Cannot <op>: operand must
be an expression or a binary file, got <type>\`, and extra arguments to a \`quiver.*\` expression
function (or to an operator metamethod called directly) raise
\`Cannot <op>: too many arguments (expected N, got M)\`; expression methods ignore extra arguments,
like every other method. A file handle does not outlive the \`run()\` that opened it: any handle still open when the script returns
is closed (and a writer flushed), so reopen the file in each script.

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

local e = (r + 10.0) * 2.0                           -- a file is an expression; scalars either side
local per_stage = r:aggregate("stage", "sum")        -- every expression method works on a file too
r:save(copy_path)                                    -- reads the file by path; r stays open
e = quiver.abs(e); e = quiver.sqrt(e)                -- also quiver.log / quiver.exp
local cond_e = quiver.gt(e, 3.0)                     -- also quiver.lt/quiver.gte/quiver.lte/quiver.eq/quiver.neq
                                                     -- all -> 1.0/0.0 per element (NaN operand -> NaN)
cond_e = cond_e & ~quiver.lt(e, 1.0)                 -- boolean logic via & | ~ operators (and/or/not are keywords)
e = quiver.ifelse(cond_e, then_e, else_e)            -- build cond_e with comparison + logical operators
e = e:aggregate("stage", "sum")                      -- sum/mean/min/max/percentile
e = e:aggregate("stage", "percentile", 0.9)          -- percentile needs the fraction
e = e:aggregate_agents("mean")                       -- collapse the label axis
e = e:select_agents({"v2"}); e = e:rename_agents({v1 = "alpha"})
e:save(out_path); e:get_metadata()                   -- save path is sandboxed like db:open_file
\`\`\`

**\`quiver.metadata{...}\` kwargs and defaults:** \`version\` defaults to \`"1"\`; \`initial_datetime\`
and \`unit\` default to \`""\`; \`labels\`, \`dimensions\`, \`dimension_sizes\`, \`time_dimensions\`, and
\`frequencies\` default to empty arrays. Only these eight keys are accepted; an unknown key or a
value of the wrong type throws.

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

DateTime wrapper helpers (Lua uses ISO 8601 strings), boolean *reader* helpers (a stored flag reads
back as \`0\`/\`1\` — writing a boolean is supported), \`_by_id\` single-scalar variants (use the
composite by-id readers or the bulk readers instead), and the row-aligned whole-group readers the
other bindings have (hence the row-count caveat under composite by-id reads). Everything else
the native binding exposes — CRUD, reads, time series, metadata, query, CSV, and the
binary/expression subsystems — is documented above and callable.`;
