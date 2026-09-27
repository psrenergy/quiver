# 50 — Lua: `read_vectors_by_id` / `read_sets_by_id` use `to_lua_table`

**Batch** 5 · **Severity** low · **Breaking** no (identical output) · **Size** S · **Layers** C++ Lua runner only
**Depends on** none · **Overlaps with** 49 (neighbouring functions), 07 (these composites call `list_vector_groups`/`list_set_groups`, which 07 makes throw on an unknown collection — no code change here)

## Why

`src/lua_runner.cpp`, `read_vectors_by_id_lua` (currently ~L1801-1837) and `read_sets_by_id_lua`
(~L1839-1874), are near-clones. Each hand-writes the vector→table conversion that the file's
`to_lua_table` overloads already do. `src/CLAUDE.md` says "`to_lua_table<T>` overloads (flat +
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

None. `src/CLAUDE.md` already states that `to_lua_table` is the only marshaler, and this makes it
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
