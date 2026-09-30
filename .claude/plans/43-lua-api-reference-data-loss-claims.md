# 43 — Lua reference: fix the rollback, `import_csv`-replaces-table and `update_time_series_files` claims

**Batch** 5 · **Severity** high (the text leads an agent into silent data loss) · **Breaking** no (documentation) · **Size** S · **Layers** `bindings/js/src/lua-api.ts` (the agent-facing `LUA_DB_API_REFERENCE`, shipped on npm)
**Depends on** 01 recommended first (it changes what `import_csv` does to dropped elements and rewrites the transaction-precondition sentence in the same section) · **Overlaps with** 01 (same CSV section of lua-api.ts), 44 (other corrections in the same file — land 43 first, 44 after), 45 (adds a binary-handle sentence to the same file)

## Why

`LUA_DB_API_REFERENCE` in `bindings/js/src/lua-api.ts` is interpolated into an LLM system prompt
downstream. Three of its claims are false in a way that makes an agent destroy data.

1. **A failed script does not roll back.** Critical rules (currently ~L86-92):
   ```
   - **Type coercion.** ... Other type mismatches raise a
     validation error and roll the whole script back.
   - **Errors abort the script.** Any error thrown by a \`db:\` call stops the script and surfaces as
     \`Failed to run Lua script: <message>\`. Validation failures roll back whatever the current
     transaction covered.
   ```
   and the time-series group rules (~L454):
   ```
   **Rules** (validation throws, rolling the script back):
   ```
   Nothing wraps `LuaRunner::run` in a transaction (`src/lua_runner.cpp`, `LuaRunner::run`, currently
   ~L2300-2311: it calls `safe_script` and encodes the return). Outside a transaction each `db:` write
   commits on its own, so writes that finished before the error **stay**. Only `db:transaction(fn)`
   and `db:dry_run(fn)` undo their block when `fn` errors (lua_runner.cpp `bind_database`, the
   `transaction`/`dry_run` lambdas, ~L552-569). A `db:begin_transaction()` still open when the script
   errors stays open. `tests/test_lua_runner_csv_import.cpp` (~L177-182) shows the host-side
   consequence.

2. **The CSV section hides that `import_csv` replaces the table** (~L587-611):
   ```
   ## CSV import / export

   Export a time-series group to a CSV file, or import one from a CSV file. ...
   ```
   Three things are wrong here:
   - `group` can name a vector, set or time-series group. The section says "time-series" only.
   - `group = ""` means the collection's own scalar table (`src/database_csv_import.cpp`,
     `import_csv`: `if (!group.empty()) { ... vector ... set ... time series ... }`). The section
     never mentions it.
   - `import_csv` **replaces** the target table. At HEAD it runs `DELETE FROM <table>` for every
     row, then inserts the file's rows. After plan 01 the scalar path deletes every element absent
     from the file (cascading to its group rows and inbound relations), and the group path still
     deletes all of the group's rows. An agent reading "import one from a CSV file" will import a
     partial file and wipe the rest.
   - Also, `date_time_format`'s comment says "for the dimension column". It applies to every
     date-time column.

3. **`update_time_series_files` replaces the whole row** (~L502-505):
   ```
   db:update_time_series_files(collection, { data_file = "path/to/data.bin", metadata_file = nil })
   ...
   In \`update_time_series_files\`, a \`nil\` value clears that column.
   ```
   `Database::update_time_series_files` (`src/database_time_series.cpp`, currently ~L398-438) writes
   the whole singleton row: every column not given a string is cleared. In Lua, `nil` and a missing
   key are the same thing (`pairs` never yields a nil value; `update_time_series_files_lua` in
   `src/lua_runner.cpp` ~L2260-2272). A table with no string values (`{}` or all-nil) returns early
   and **changes nothing**, so `{ metadata_file = nil }` does not clear `metadata_file`. The
   parenthetical at ~L287-288 ("`nil` → NULL is only accepted by `upsert_time_series_row`,
   `update_time_series_files` and `update_relation`") compounds the mistake.

Principle: the reference must be semantically true. Its header says the sync test "CANNOT check
... whether the prose is semantically true", so these edits must be made and checked by hand.

## Constraints and decisions

- Prose only. No change to `src/lua_runner.cpp` or any C++ behaviour.
- Keep the file's escaping: the reference is a JS template literal, so inline backticks are written
  `\``. Keep the FORMAT CONVENTION: every `db:` method keeps at least one literal `db:<name>` token.
- Word the rollback rule so it is true of the library and does not contradict a host that runs the
  script inside its own transaction or dry run (policy note). The library never rolls back a script
  for you. A host may.
- Do not re-word the import precondition paragraph ("cannot run inside an open transaction") beyond
  what plan 01 already did. If plan 01 has landed, that paragraph's rationale is already updated.
  Leave it.

## Changes — `bindings/js/src/lua-api.ts`

### 1. Type-coercion bullet (~L86-89)

End the bullet at "... raise a validation error." Current tail:
```
  (\`YYYY-MM-DD\`, optionally \`THH:MM:SS\` or \` HH:MM:SS\`). Other type mismatches raise a
  validation error and roll the whole script back.
```
New tail:
```
  (\`YYYY-MM-DD\`, optionally \`THH:MM:SS\` or \` HH:MM:SS\`). Other type mismatches raise a
  validation error.
```

### 2. "Errors abort the script" bullet (~L90-92)

Replace the whole bullet with:
```
- **Errors abort the script, and nothing is rolled back for you.** Any error thrown by a \`db:\`
  call stops the script and surfaces as \`Failed to run Lua script: <message>\`. The failing call
  itself writes nothing, but every write that already finished **stays**: outside a transaction each
  \`db:\` write commits on its own. Only \`db:transaction(fn)\` and \`db:dry_run(fn)\` undo their
  block when \`fn\` errors. A \`db:begin_transaction()\` (or \`db:begin_dry_run()\`) you opened and
  never closed stays open after the error, for the host to commit or roll back. A host that runs
  your script inside its own transaction or dry run may undo the whole run; check
  \`db:in_transaction()\` if it matters. To make several writes all-or-nothing, put them in one
  \`db:transaction(function(db) ... end)\`.
```

### 3. Time-series rules header (~L454)

Current: `**Rules** (validation throws, rolling the script back):`
New: `**Rules** (a violation throws; see "Errors abort the script" for what is kept):`

### 4. Time-series files (~L502-505)

Replace the example line
```
db:update_time_series_files(collection, { data_file = "path/to/data.bin", metadata_file = nil })
```
with
```
db:update_time_series_files(collection, { data_file = "path/to/data.bin" })  -- metadata_file is cleared
```
and replace the sentence `In \`update_time_series_files\`, a \`nil\` value clears that column.` with:
```
\`update_time_series_files\` **replaces the whole row**: every column you do not give a string is
set to NULL, and in Lua a \`nil\` value and a missing key are the same thing. A table with no string
values (\`{}\`, or only \`nil\` values) changes nothing and clears nothing. A value that is not a
string throws. To change one path, read the row with \`db:read_time_series_files(collection)\`,
change that one entry, and pass the whole table back.
```

### 5. The "nil → NULL is only accepted by" parenthetical (~L287-288)

Current:
```
  NULL via the element table. (\`nil\` → NULL is only accepted by
  \`upsert_time_series_row\`, \`update_time_series_files\` and \`update_relation\`.)
```
New:
```
  NULL via the element table. (\`nil\` → NULL is accepted by \`upsert_time_series_row\` and
  \`update_relation\`, and as a cell in the group writers; \`update_time_series_files\` clears any
  column you leave out — see Time series files.)
```
(Plan 44 adds the query-parameter case to this list. Keep this sentence short so 44 can append.)

### 6. CSV import / export opening (~L587-589) and `date_time_format` comment (~L600)

Replace the opening paragraph:
```
Export a time-series group to a CSV file, or import one from a CSV file. \`path\` is sandboxed:
relative paths resolve against the database file's directory and must stay inside it (see
Critical rules); \`options\` is optional.
```
with:
```
Export one table of \`collection\` to a CSV file, or import one from a CSV file. \`group\` names a
vector, set or time-series group of \`collection\`; pass \`""\` for the collection's own scalar table
(the CSV then has no \`id\` column and must contain \`label\`). \`path\` is sandboxed: relative paths
resolve against the database file's directory and must stay inside it (see Critical rules);
\`options\` is optional.

**\`db:import_csv\` replaces, it does not merge.** A group import deletes every existing row of
that group, for every element, not only the elements named in the file, and then inserts the file's
rows. A scalar import (\`group = ""\`) keeps the id of every element whose label is in the file,
updates its columns, and **deletes every element whose label is not in the file**, with its group
rows and the relations that pointed at it. A header-only CSV empties the table. To add or change a
few elements, use \`db:create_element\` / \`db:update_element\` instead.
```
If plan 01 has **not** landed, the scalar sentence must describe HEAD instead: "A scalar import
deletes every element and re-inserts the file's rows, keeping the id of each element whose label
reappears; an element missing from the file is gone." Prefer landing 01 first.

In the options example, change
`date_time_format = "%Y-%m-%d",   -- strftime-style format for the dimension column` to
`date_time_format = "%Y-%m-%d",   -- strftime-style format for every date-time column`.
Before editing, confirm the claim against `src/database_csv_export.cpp` (the format is applied
wherever the column's type is DateTime) and `src/database_csv_import.cpp` (`parse_datetime_import`
is called for every DateTime column).

## Tests

- `bindings/js/test/lua-api-sync.test.ts` must still pass. It checks that every `db:` name is
  present, that none is removed, and the stdlib sentence. The edits keep every `db:` token.
- No behavioural test. The claims are about existing, already-tested behaviour:
  - `import_csv` replacement: `tests/test_database_csv_import.cpp`; after 01, its omitted-element
    tests.
  - `update_time_series_files` whole-row write: `tests/test_database_time_series.cpp` /
    `tests/test_lua_runner_time_series.cpp` (`grep -n "update_time_series_files" tests/test_lua_runner_time_series.cpp`).

    If no Lua test pins "an omitted column is cleared", add one to
    `tests/test_lua_runner_time_series.cpp`, following that file's `LuaRunnerTest` fixture style:
    ```cpp
    TEST_F(LuaRunnerTest, UpdateTimeSeriesFilesReplacesTheWholeRow) {
        auto db = quiver::Database::from_schema(":memory:", collections_schema, {.read_only = false, .console_level = quiver::LogLevel::Off});
        quiver::LuaRunner lua(db);
        lua.run(R"(
            db:update_time_series_files("Collection", { data_file = "a.bin", metadata_file = "a.toml" })
            db:update_time_series_files("Collection", { data_file = "b.bin" })
            local f = db:read_time_series_files("Collection")
            assert(f.data_file == "b.bin", "data_file")
            assert(f.metadata_file == nil, "metadata_file must be cleared")
            db:update_time_series_files("Collection", {})
            local g = db:read_time_series_files("Collection")
            assert(g.data_file == "b.bin", "an empty table changes nothing")
        )");
    }
    ```
    Match the fixture's `collections_schema` member and how other tests in that file open the
    database: `sed -n 1,30p tests/test_lua_runner_time_series.cpp`.

## Docs and changelog

- `CHANGELOG.md`, `## [0.12.0] — unreleased` → `### Fixed`:
  ```markdown
  - **JS: the agent-facing Lua reference (`LUA_DB_API_REFERENCE`) no longer promises a rollback.**
    A failed script keeps every write that finished before the error; only `db:transaction` /
    `db:dry_run` undo their block. The CSV section now says that `import_csv` replaces the target
    table (and that `group = ""` is the scalar table), and that `update_time_series_files` replaces
    the whole row.
  ```

## Verification

From the repo root:
1. `bindings/js/test/test.bat test/lua-api-sync.test.ts`
2. If the Lua test was added: `cmake --build build --config Debug` and
   `./build/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.UpdateTimeSeriesFilesReplacesTheWholeRow`
3. `bindings/js/test/test.bat` (full JS suite; `LUA_DB_API_REFERENCE` is a module-level string, so
   an unescaped backtick breaks the whole module).
4. `scripts/format.bat`

## Acceptance criteria

- [ ] No "roll(s)/rolling ... back" claim remains outside `db:transaction`/`db:dry_run`/the Dry runs
      section (`grep -n "roll" bindings/js/src/lua-api.ts`, then review each hit).
- [ ] The CSV section names vector/set/time-series/scalar tables and states the replace semantics.
- [ ] `update_time_series_files` is documented as a whole-row replace.
- [ ] lua-api-sync and the full JS suite pass.

## Pitfalls

- Every inline backtick inside the template literal must be `\``. An unescaped one ends the string
  and breaks every JS test at import.
- Keep the literal `db:import_csv`, `db:export_csv`, `db:update_time_series_files` and
  `db:read_time_series_files` tokens (FORMAT CONVENTION).

## Out of scope

- The other inaccuracies in this file (plan 44).
- Changing `import_csv` or `update_time_series_files` behaviour (plan 01; the files replace
  semantics are unchanged by design).

## Implementation notes

- **Starting point.** Master was already in the branch: `rs/plan43`, `master` and `origin/master`
  were all `b4c62bb`, so there was nothing to merge. Plan 01 had landed (`a464023`), so the
  post-01 scalar-import wording applies. Every quoted excerpt matched the code; only the line
  numbers had drifted.
- **Adversarial check.** Before editing, four read-only agents tried to refute each new sentence
  against the code. The wording changes below came out of that check. They stay inside the claims
  this plan fixes and keep the new prose true:
  - **Errors bullet.**
    - A `db:` error caught with `pcall` does not stop the script, so the bullet now says
      "uncaught" and "that you do not catch with `pcall`".
    - The claim that the failing call writes nothing holds only outside a transaction. Inside
      one, a failure that only SQLite detects (UNIQUE, NOT NULL, CHECK or FK) keeps the call's
      earlier writes (root AGENTS.md, "Writers check everything before their first write").
    - Only database writes are undone. Nothing undoes a file a `db:` call wrote; `db:csv_to_bin`,
      for example, truncates the `.qvr` before it parses the rows.
    - "For the host to commit or roll back" was wrong for a dry run, which cannot be committed.
      It now reads "closing it is up to the host".
    - The all-or-nothing advice now adds "do not `pcall` inside it": a caught error lets
      `db:transaction` commit whatever ran.
  - **CSV, scalar table.** The CSV must hold *exactly* the table's columns except `id`, not just
    "contain `label`" (`validate_columns_match`).
  - **CSV, scalar import.** The paragraph now states:
    - Elements are matched by exact label.
    - A kept element has every column overwritten, and a blank cell writes NULL.
    - `CASCADE` deletes the row that holds the relation: a group row, or an element of another
      collection.
    - Editing a label deletes that element and creates a new one.
    - Only relations "from another table" follow `ON DELETE`: import clears self-references and
      rewrites them from the file, so "exactly as `delete_element` would" was untrue for those.
  - **`update_time_series_files`.** A column left out gets NULL *or its DEFAULT*, because the code
    does a DELETE and then an INSERT of the given columns only.
  - **Nil parenthetical.** `upsert_time_series_row` is also a whole-row replace
    (`INSERT OR REPLACE` lists only the caller's columns). The plan's text listed it as if it did a
    partial update, so it now names it with `update_time_series_files`.
- **Extra scope the maintainer approved:**
  - A warning in the `upsert_time_series_row` section that an existing row is replaced whole, plus
    `LuaRunnerTest.UpsertTimeSeriesRowReplacesTheWholeRow`.
  - L119 (Output bullet): "file columns" dropped from the list of places where `nil` stores NULL.
  - L206-208 (Transactions caveat): "each `db:` write is durable on its own" became "outside a
    transaction each `db:` write commits on its own, while inside the host's transaction it commits
    or rolls back with the host's".
- **Tests.** `UpdateTimeSeriesFilesReplacesTheWholeRow` (the plan's body) and
  `UpsertTimeSeriesRowReplacesTheWholeRow` both pin behaviour that already exists. They pass before
  and after this change, because the bug was in the prose, so there is no failing-first run.
- **CHANGELOG.** The entry went into a new `## [0.12.7] — unreleased` → `### Fixed` section, not
  `[0.12.0]`: v0.12.6 is tagged and the manifests are already at 0.12.7. The plan's entry gained one
  clause about `upsert_time_series_row`. No manifest bump.
- **Verification.**
  - All 364 `LuaRunner*` tests pass, including both new ones.
  - The full JS suite passes, 242/242; `lua-api-sync` passes 6/6.
  - `scripts/format.bat` exits 0. Biome rewrote 30 untouched JS files from CRLF to LF, and those
    were restored.
  - `bunx biome check --line-ending=crlf src/lua-api.ts` is clean.
- **For plan 44** (`bindings/js/src/lua-api.ts` line anchors after this plan):
  - L55: the type-table row still says "file paths". It was left for 44(h), which removes it.
  - L119: the Output bullet now reads `(query params, ts rows,\n  relation targets — ...`. 44(h)
    adds its "except a trailing one" caveat here.
  - L204: the quoted `cannot start a transaction within a transaction` error is unchanged, for
    44(b). The paragraph's last sentence (L206-208) was rewritten, so re-anchor 44(b) on it.
  - L305-308: the nil parenthetical now starts `(\`nil\` → NULL is accepted by \`update_relation\`,
    and as a cell in the group writers; ...`. Append "query params (not a trailing one)" to that
    first list.
  - L480: the time-series rules header. The booleans bullet under it is still wrong, for 44.
- **For plan 45:** the binary section is unchanged.
- **Found and not planned anywhere:**
  - `db:export_csv` silently overwrites an existing file at `path`, with no guard.
  - `update_time_series_group(c, g, missing_id, {})` is a silent no-op, while the reference says
    "the element id must exist". Only with rows does a missing id fail, on the foreign key.
  - `db:transaction` leaves its transaction open if the final COMMIT fails
    (`src/lua_runner.cpp`, the `transaction` lambda calls `commit()` outside the rollback branch).
