# 50 — Lua: `read_vectors_by_id` / `read_sets_by_id` use `to_lua_table`

**Batch** 5 · **Severity** low · **Breaking** no (identical output) · **Size** S · **Layers** C++ Lua runner only
**Depends on** none · **Overlaps with** 49 (neighbouring functions), 07 (these composites call `list_vector_groups`/`list_set_groups`, which 07 makes throw on an unknown collection — no code change here)

## Why

`src/lua_runner.cpp`, `read_vectors_by_id_lua` (currently ~L1801-1837) and `read_sets_by_id_lua`
(~L1839-1874), are near-clones. Each hand-writes the vector→table conversion that the file's
`to_lua_table` overloads already do. `src/AGENTS.md` says "`to_lua_table<T>` overloads (flat +
nested) are the only vector→table marshalers". Current shape:

```cpp
        for (const auto& group : db.list_vector_groups(collection)) {
            for (const auto& col : group.value_columns) {
                auto t = lua.create_table();
                switch (col.data_type) {
                case DataType::Integer: {
                    auto values = db.read_vector_integers_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                case DataType::Real: { ...read_vector_floats_by_id... }
                case DataType::Text:
                case DataType::DateTime: { ...read_vector_strings_by_id... }
                default:
                    throw std::runtime_error("Cannot read_vectors_by_id: unknown data type " + ...);
                }
                result[col.name] = t;
            }
        }
```

`to_lua_table(sol::state_view&, const std::vector<T>&)` (~L1318) produces the same 1-indexed table.

Principle: reuse what exists and delete duplication.

## Constraints and decisions

- Replace each case body with one `to_lua_table` call. **Stop there.** Do not merge the two
  functions into a templated or callback-parameterized helper. At about 15 lines each, sharing a
  body would add indirection for little gain (policy note; the root principle is "simple over
  abstract").
- Keep the `default:` throw. It names the public composite.

## Changes — `src/lua_runner.cpp`

`read_vectors_by_id_lua`, new inner loop:
```cpp
        for (const auto& group : db.list_vector_groups(collection)) {
            for (const auto& col : group.value_columns) {
                switch (col.data_type) {
                case DataType::Integer:
                    result[col.name] = to_lua_table(lua, db.read_vector_integers_by_id(collection, col.name, id));
                    break;
                case DataType::Real:
                    result[col.name] = to_lua_table(lua, db.read_vector_floats_by_id(collection, col.name, id));
                    break;
                case DataType::Text:
                case DataType::DateTime:
                    result[col.name] = to_lua_table(lua, db.read_vector_strings_by_id(collection, col.name, id));
                    break;
                default:
                    throw std::runtime_error("Cannot read_vectors_by_id: unknown data type " +
                                             std::to_string(static_cast<int>(col.data_type)));
                }
            }
        }
```
The `auto t = lua.create_table();` before the switch and the `result[col.name] = t;` after it are
deleted.

`read_sets_by_id_lua` is the same with `db.list_set_groups`, `read_set_integers_by_id`,
`read_set_floats_by_id`, `read_set_strings_by_id` and `"Cannot read_sets_by_id: ..."`.

`to_lua_table` is a template taking `sol::state_view&`. `lua` here is a `sol::state_view` local,
so the call binds directly. The `_by_id` readers return `std::vector<T>` (dense), which matches the
flat overload.

## Tests

No behaviour change. The existing composite tests must pass unchanged. Find them with
`grep -rn "read_vectors_by_id\|read_sets_by_id\|read_element_by_id" tests/test_lua_runner*.cpp`.
Also check that at least one existing test reads an **empty** group (an element with no vector
rows), whose table should still be `{}`. If none does, add one to the file that holds the other
composite tests:
```cpp
TEST_F(LuaRunnerTest, ReadVectorsByIdEmptyGroupIsEmptyTable) {
    // open collections_schema as the neighbouring composite tests do; create Configuration + one
    // Collection element with no vector rows (id 1)
    lua.run(R"(
        local v = db:read_vectors_by_id("Collection", 1)
        assert(type(v.value_int) == "table" and next(v.value_int) == nil, "empty group -> {}")
    )");
}
```
(`collections.sql`'s vector group `values` has columns `value_int`, `value_float`.)

## Docs and changelog

None. `src/AGENTS.md` already states that `to_lua_table` is the only marshaler, and this makes it
true.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
3. `bindings/js/test/test.bat test/lua-api-sync.test.ts`
4. `scripts/format.bat`

## Acceptance criteria

- [ ] Neither function contains a hand-written `t[i + 1] = values[i]` loop.
- [ ] The Lua suites pass.

## Pitfalls

- Do not route these through the bulk readers. They read one element by id.

## Out of scope

- Merging the two functions (explicitly rejected).
- `read_scalars_by_id_lua` (already uses `sol::make_object`).

## Implementation notes

Implemented on `rs/plan50`. At planning time the branch sat at master `b4c62bb`. It was fast-forwarded to master `afb9e91` before editing, which brought in plans 43-49 (#355-#361). So `git fetch origin && git merge origin/master` printed "Already up to date." Every anchor was re-grepped after the fast-forward: the two function bodies were unchanged and only their line numbers moved.

**Verdict: implement the leftovers, with the user's agreement.** The plan's core change had **already landed**. Commit `61e6236` ("fix: preserve NULL cells in vector and set reads") replaced all six hand-written `t[i + 1] = values[i]` loops in these two functions with `to_lua_table` calls, so acceptance criterion 1 was met before this plan started. The user was asked, and chose to finish the two remaining pieces:
- collapse each case body to the plan's one-liner;
- add the empty-group test.

### Drift fixed

- **Line numbers.** At `afb9e91` the functions sit at `read_vectors_by_id_lua` L1854 and `read_sets_by_id_lua` L1886, with the `to_lua_table` overloads at L1385 (flat), L1397 (nullable) and L1409 (nested). The plan quoted ~L1801/~L1839/~L1318.
- **Quoted "current shape" was stale.** It did not match the code. The code already had `case X: { auto values = db.read_..._by_id(...); result[col.name] = to_lua_table(lua, values); break; }`, with no `auto t = lua.create_table();` and no `result[col.name] = t;` left to delete. The plan's "new inner loop" was applied verbatim; the edit only removed the braces and the `values` local (-12 lines).
- **Wrong overload named.** The plan says the `_by_id` readers "return `std::vector<T>` (dense), which matches the flat overload". Since `61e6236` they return `std::vector<std::optional<T>>` (`include/quiver/database.h:96-101`, and the set counterparts). So the **nullable** overload binds, and a NULL cell becomes a `nil` hole. The design is unchanged, and `ReadVectorPreservesNullCellsAsNilHoles` / `ReadSetPreservesNullCellsAsNilHoles` already pin the hole behaviour through both composites.
- **Empty-group test was missing, so it was added.** No existing test read an element with no vector rows through `db:read_vectors_by_id`:
  - `ReadVectorsById` uses `basic.sql`, which has *no groups*;
  - `ReadVectorPreservesNullCellsAsNilHoles` creates a no-rows element but reads only element 1 by id.

  `LuaRunnerTest.ReadVectorsByIdEmptyGroupIsEmptyTable` now sits after `ReadVectorsByIdWithData` in `tests/test_lua_runner_read.cpp`. Unlike the plan's sketch, it uses the id returned by `create_element` rather than a hard-coded `1`, and it asserts both `value_int` and `value_float`, which covers the Integer and Real branches. `read_sets_by_id` has no empty-group test: it is the same code path, and the plan named only vectors.

### Results

- **Not fail-first.** This is a refactor with no behaviour change, not a bug fix. The new test was added before the production edit and passed on the old code: `3 tests ... [  PASSED  ] 3 tests` (with `ReadVectorsById` and `ReadVectorsByIdWithData`).
- **Acceptance.** `grep -n "t\[i + 1\] = values\[i\]" src/lua_runner.cpp` prints one line, L1388, which is the body of the flat `to_lua_table` overload itself. Neither composite contains the loop.
- **Green after.**
  - `quiver_tests.exe --gtest_filter=LuaRunner*`: **373/373** (the rebuilt `libquiver.dll` was newer than the edit);
  - full `quiver_tests.exe`: **1390/1390**;
  - `quiver_c_tests.exe`: **571/571**;
  - `bindings/js/test/test.bat test/lua-api-sync.test.ts`: **242 pass, 0 fail**. `test.bat` prepends `test`, so the whole JS suite runs; `bun test test/lua-api-sync.test.ts` alone gives 6/6.
- **`scripts\format.bat`** exits 0. clang-format left both edited files as written, and ruff (35 unchanged) changed nothing. Biome again rewrote 43 JS files CRLF→LF. `git diff --ignore-cr-at-eol bindings/js` was empty (0 lines), and `git checkout -- bindings/js` reverted them. No `.bat` was touched.
- Diff: `src/lua_runner.cpp` +12/-24, test +15. No CHANGELOG or AGENTS.md change: the plan specifies none, and `src/AGENTS.md`'s "`to_lua_table<T>` overloads are the only vector→table marshalers" was already true.
- **Not run:** `scripts/test-all.bat` (Julia, Dart and Python suites). The change is confined to two Lua-runner functions. The README asks for test-all at the end of each batch.

### For later plans

- **Plans 51/52.** These edits touched only the two composites' switch bodies. The operator metamethods (51) and the metadata getters (52) are not near them, so no conflict is expected.
- **Stale comments left alone.** `ReadVectorsById` / `ReadSetsById` still carry a stale comment calling `group_name != column_name` in `collections.sql` "a known limitation of the composite helper". The composites key by column name and work on that schema, as the new test shows. That is out of scope here, and a candidate for a test-cleanup plan (68/71/72 territory).
