# 44 — Lua reference: boolean rule, quoted errors, query conversion, NULL shapes, nil params; one stable key-type error

**Batch** 5 · **Severity** medium · **Breaking** no · **Size** M · **Layers** `bindings/js/src/lua-api.ts` (prose), `src/lua_runner.cpp` (one check), C++ Lua tests
**Depends on** 43 (same file; land 43 first — it rewrites the "nil → NULL is accepted by" parenthetical this plan extends) · **Overlaps with** 46/47 (touch the Lua converters `lua_table_to_values` and `columns_to_cpp_rows` near `collect_group_columns`; this plan edits only `collect_group_columns`), 48 (strict decoders, other functions), 45 (binary sentence in the same file)

## Why

After plan 43 fixes the three data-loss claims, `LUA_DB_API_REFERENCE`
(`bindings/js/src/lua-api.ts`) still has these inaccuracies. The sync test checks names only, so
none of them is caught automatically.

1. **The boolean rule contradicts itself.** The time-series writer rules (~L466-467) say:
   `Integer values are accepted for REAL columns (converted on insert). Booleans, functions, and
   other unsupported Lua types throw \`column '...' has unsupported Lua type\`.`
   The code writes a boolean as 1/0: `columns_to_cpp_rows` in `src/lua_runner.cpp` (~L2036-2038)
   goes through `is_lua_boolean`, and `tests/test_lua_runner_create.cpp` (~L226-238) pins it. The
   same document already says so in its type table (~L54): `| boolean | INTEGER 1/0 | ... |`.
2. **Error text the build never emits.**
   - The Transactions caveat (~L190-194) quotes `cannot start a transaction within a transaction`.
     `Database::begin_transaction` actually throws `Cannot begin_transaction: transaction already
     active` (`src/database.cpp`, ~L321-322). `db:transaction(fn)` calls it, so it fails with the
     same text under a host's plain transaction.
   - The group-writer "DO NOT pass an array of row tables" note (~L449-452) quotes sol2's Debug-only
     `stack index -1, expected string, received number`. In Release (`SOL_SAFE_GETTER` off) the key
     conversion is unchecked. The integer key becomes a garbage or empty column name, and the call
     fails later, or a boolean key silently becomes column `''`.
   - Dry runs (~L226-233) say only a host *dry run* blocks `db:dry_run`. Under a host *plain*
     transaction, `db:begin_dry_run()`/`db:dry_run(fn)` throw
     `Cannot begin_dry_run: transaction already active` (`src/database.cpp`, `begin_dry_run`,
     ~L358-359). `db:in_transaction()` is true in both cases, so it is the one check that covers
     both.
   - The encoder error list (~L134-141) omits the unsupported-key-type error (a table key that is
     neither an integer nor a string, e.g. a boolean key). Find its exact text with
     `grep -n "unsupported table key\|unsupported key" src/lua_runner.cpp`.
3. **`query_*` do not convert** (~L562-563: "Each returns the first column of the first row as the
   requested type, or `nil` if there is no result"). `Row::get_string` / `get_integer`
   (`src/row.cpp`, ~L29/~L50) return nothing for a value that is not already that type; only
   `get_float` widens an INTEGER. So `db:query_string("SELECT COUNT(*) ...")` and
   `db:query_integer("SELECT AVG(x) ...")` return `nil`, which reads as "no row".
4. **Bulk reads omit the NULL shape.**
   - Scalar reads (~L304-305: "Each returns a flat array ..., one value per element") have `nil`
     holes. The Critical rules even say (~L109) "Arrays are 1-indexed (iterate with `ipairs`)",
     which stops at the first hole.
   - "(see Reading)" (~L129) points at a section that does not exist.
   - The snippet at ~L132 uses an undefined `ids`.
   - Vector/set reads (~L315-329) never say an element with no rows is `{}`, or that NULL cells are
     dropped, so two columns of one group are not aligned.
   - `read_time_series_row` (~L421) has the same holes.
5. **A nil query parameter binds NULL only in the interior of the table.** Lua stores no key for a
   trailing `nil`, so `{ 5, nil }` and `{ nil }` count as one parameter short and hit `execute`'s
   count check (`expected N bound parameter(s) but got M`). The reference says (~L55, ~L110)
   `nil` → NULL "in query params" without that caveat. No Lua test binds a nil parameter.

**Code change, one line of logic:** `collect_group_columns` (`src/lua_runner.cpp`, currently
~L2004-2022) is the single decoder behind all six Lua group writers. It converts each key with
`pair.first.as<std::string>()` before any type check:

```cpp
    static std::vector<GroupColumn> collect_group_columns(const std::string& caller, const sol::table& columns) {
        std::vector<GroupColumn> result;
        for (auto& pair : columns) {
            auto name = pair.first.as<std::string>();
            if (!pair.second.is<sol::table>()) {
```

A key-type check gives one stable Pattern 1 message in Debug and Release, and the reference can
then quote it. That is the same style as the file's `csv_options_entries` / `csv_separator_from_lua`
key checks, which `src/AGENTS.md` describes.

## Constraints and decisions

- **Maintainer notes (binding):**
  - Add the key-type check in `collect_group_columns` and quote its message in the reference.
  - Add Lua tests: `{nil, 5}` binds NULL, and `{5, nil}` throws the count mismatch.
  - Leave `lua_table_to_values` unchanged.
- Pattern 1 message, naming the public operation: `caller` is already the public method name there
  (`update_vector_group`, etc.). Check this with
  `grep -n "collect_group_columns(\"" src/lua_runner.cpp` and the callers of `columns_to_cpp_rows`.
- Prose edits keep the template-literal escaping (`\``) and the `db:<name>` tokens.

## Changes

### 1. `src/lua_runner.cpp` — `collect_group_columns`

Insert as the first statement in the loop, before `auto name = pair.first.as<std::string>();`:

```cpp
            // Check the key's type before converting it: sol2's string getter is unchecked in
            // Release (SOL_SAFE_GETTER off), so an array of row tables ({ {date_time=..}, .. })
            // or a boolean key otherwise became a garbage / empty column name.
            if (pair.first.get_type() != sol::type::string) {
                throw std::runtime_error("Cannot " + caller +
                                         ": column names must be strings -- pass { column = { values... } }, "
                                         "not an array of row tables");
            }
```

### 2. `bindings/js/src/lua-api.ts` — prose

a. **Boolean rule (~L466-467).** Replace
`Integer values are accepted for REAL columns (converted on insert). Booleans, functions, and
other unsupported Lua types throw \`column '...' has unsupported Lua type\`.` with
`Integer values are accepted for REAL columns (converted on insert), and a boolean is written as
1/0. A function, a table or another unsupported Lua type throws \`column '...' has unsupported Lua
type\`.`

b. **Transactions caveat (~L190-194).** Replace the quoted
`\`cannot start a transaction within a transaction\`` with
`\`Cannot begin_transaction: transaction already active\``, and add after the first sentence:
"\`db:transaction(fn)\` begins one too, so it fails the same way."

c. **Row-table note (~L449-452).** Replace
```
\`upsert_time_series_row\` shape, not this one. Doing so raises
\`stack index -1, expected string, received number\` (the integer array indices 1, 2, 3 are not
column names). Each value of the top-level table must be an **array**, not a scalar.
```
with
```
\`upsert_time_series_row\` shape, not this one. Doing so raises
\`Cannot update_time_series_group: column names must be strings -- pass { column = { values... } },
not an array of row tables\`. Each value of the top-level table must be an **array**, not a scalar.
```

d. **Dry runs (~L228-232).** After the "Dry runs do not nest" bullet, add:
```
- **Nor inside a plain transaction.** If the host (or a \`db:transaction\` block) already holds a
  plain transaction, \`db:begin_dry_run()\` and \`db:dry_run(fn)\` throw
  \`Cannot begin_dry_run: transaction already active\`. \`db:in_transaction()\` is true under a host
  dry run *and* under a host transaction, so it is the one check that covers both;
  \`db:in_dry_run()\` covers only the first.
```

e. **Encoder errors (~L134-141).** Append the unsupported-key-type error, quoting the exact text
found by the grep above, e.g. `a table with a key that is neither an integer nor a string raises
\`<exact text>\``.

f. **Query (~L562-563).** Replace "Each returns the first column of the first row as the requested
type, or \`nil\` if there is no result." with:
```
Each returns the first column of the first row, or \`nil\` when there is no row, the value is NULL,
or it is not already the requested type. Only \`query_float\` converts (it widens an INTEGER), so
\`db:query_string("SELECT COUNT(*) ...")\` and \`db:query_integer("SELECT AVG(x) ...")\` return
\`nil\`; \`CAST\` in the SQL when unsure (\`SELECT CAST(AVG(x) AS INTEGER)\`). A \`nil\` param binds
NULL only when a non-nil param follows it (\`{ nil, 5 }\`): Lua stores no key for a trailing
\`nil\`, so \`{ 5, nil }\` or \`{ nil }\` is one parameter short and throws the count mismatch. To
test for NULL, write \`IS NULL\` in the SQL.
```

g. **NULL shapes of bulk reads.**
   - Critical rules (~L109): "Arrays are 1-indexed (iterate with \`ipairs\`)" becomes
     "Arrays are 1-indexed (iterate with \`ipairs\`, except a nullable bulk scalar read or
     \`db:read_time_series_row\`, which can hold \`nil\` holes; see Scalar reads)".
   - ~L129: "(see Reading)" becomes "(see Scalar reads)".
   - ~L132: define `ids` in the snippet. Prefix it with
     `local ids = db:read_element_ids(c); ` and change `for i in ipairs(ids)` to
     `for i = 1, #ids`.
   - Scalar reads (after ~L305 "Each returns a flat array ..."): add "A NULL is a \`nil\` hole at
     that position, so \`ipairs\` and \`#\` stop at the first one. Loop over
     \`local ids = db:read_element_ids(c)\` with \`for i = 1, #ids\` and read \`v[i]\`."
   - Vector reads (~L315 section): add "An element with no rows is \`{}\`. NULL cells are dropped,
     so the inner arrays of two columns of one group are not aligned (see Composite by-id reads)."
     Set reads already say "Same shape as vector reads".
   - `read_time_series_row` (~L421): add "The result can hold \`nil\` holes, so loop over
     \`db:read_element_ids(c)\` rather than using \`ipairs\`."

h. **nil list (~L55, ~L110, and the parenthetical plan 43 left at ~L287-288).**
   - L55 type-table row: "In query params, file paths, ts rows, relations." becomes
     "In query params (not trailing), ts rows, group cells, relations."
   - L110: "writing \`nil\` stores NULL where NULL is accepted (query params, ..." becomes
     "(query params except a trailing one, ...".
   - Plan 43's parenthetical: add "query params (not a trailing one)" to the accepted list.

## Tests

### `tests/test_lua_runner_time_series.cpp` — the new key-type error

Next to the existing `expect_lua_error(lua, script, "must be an array of values");` test (~L137),
add a test in the same style (copy its fixture and element setup):
```cpp
TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupRejectsArrayOfRowTables) {
    // same setup as the neighbouring "must be an array of values" test
    auto script = R"(
        db:update_time_series_group("Collection", "data", 1, {
            { date_time = "2024-01-01T00:00:00", value = 1.0 },
        })
    )";
    expect_lua_error(lua, script, "Cannot update_time_series_group: column names must be strings");
}
```
Also add one for a boolean key on a vector writer in `tests/test_lua_runner_update.cpp`, next to
the `update_set_group ... "must be an array of values"` test (~L346):
```cpp
    expect_lua_error(lua, R"(db:update_vector_group("Child", "refs", 1, { [true] = { 1 } }))",
                     "Cannot update_vector_group: column names must be strings");
```
Run both on Debug **and** on a Release test tree. Before the fix, Release produces a different or
no error. Configure the Release tree once with
`cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`
(see tests/AGENTS.md).

### `tests/test_lua_runner_query.cpp` — nil parameters

Add next to the existing query tests, using the file's fixture:
```cpp
TEST_F(LuaRunnerTest, QueryInteriorNilParamBindsNull) {
    // open collections_schema as the neighbouring tests do
    lua.run(R"(
        local r = db:query_integer("SELECT CASE WHEN ? IS NULL THEN ? ELSE -1 END", { nil, 5 })
        assert(r == 5, "interior nil must bind NULL, got " .. tostring(r))
    )");
}

TEST_F(LuaRunnerTest, QueryTrailingNilParamIsACountMismatch) {
    expect_lua_error(lua, R"(db:query_integer("SELECT ? + ?", { 5, nil }))", "bound parameter");
}
```
Take the count-mismatch substring from the actual `execute` message
(`grep -n "bound parameter" src/database.cpp`).

`{ nil, 5 }` relies on `lua_table_to_values` iterating to `t.size()`, and Lua's `#` for a
constructor-built `{nil, 5}` is 2. If the first test turns out non-deterministic, do not change
`lua_table_to_values` (maintainer decision). Instead, reduce the reference sentence to "a `nil`
param cannot be relied on; write `NULL` / `IS NULL` in the SQL", and replace the first test with
the trailing-nil test only.

## Docs and changelog

- `src/AGENTS.md`, in the "Three guards in the Lua layer's decoders" list: add a fourth bullet.
  "`collect_group_columns` checks each column key is a string before converting it (Release sol2
  getters are unchecked), so a table of row tables throws one Pattern 1 message in every build."
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Fixed`:
  ```markdown
  - **Lua: an array of row tables passed to a group writer throws one clear error in every build**
    (`column names must be strings -- pass { column = { values... } } ...`). In Release it used to
    turn the integer keys into bogus column names. The agent-facing reference also now documents
    boolean cells, the real transaction/dry-run error texts, that `query_*` do not convert types,
    the `nil` holes in bulk reads and the trailing-`nil` query-parameter limit.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunner*` (all Lua suites)
3. `cmake --build build-release --config Release` then
   `./build-release/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
4. `bindings/js/test/test.bat test/lua-api-sync.test.ts` and then the full JS suite
5. `scripts/format.bat`

## Acceptance criteria

- [ ] `collect_group_columns` rejects non-string keys with the new Pattern 1 message. The tests
      pass on Debug and Release.
- [ ] Every quoted error in the edited passages exists verbatim in `src/`
      (`grep -rn "<fragment>" src/`).
- [ ] The prose edits a–h are applied, and lua-api-sync and the full JS suite pass.

## Pitfalls

- Escape backticks inside the template literal as `\``.
- `sol::type::string` is the right enum. Do not use `is<std::string>()`, which is true for numbers,
  because Lua coerces them.
- Keep `db:read_element_ids` and every `db:query_*` token.

## Out of scope

- Changing `lua_table_to_values` to walk `pairs`. Maintainer decision: leave it.
- The data-loss claims (plan 43) and the strict option decoders (plan 48).
