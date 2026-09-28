# 52 — Lua tests for the six untested metadata getters (+ C++ `list_vector_groups`/`list_set_groups`)

**Batch** 5 · **Severity** medium (bound, documented to agents, untested) · **Breaking** no · **Size** S · **Layers** C++ Lua tests, C++ core tests (test-only)
**Depends on** none · **Overlaps with** 49 (refactors these exact Lua builders — landing 52 first gives 49 a safety net), 07 (list-groups throw on unknown collection; its tests live in the same C++ file)

## Why

`src/lua_runner.cpp` binds six metadata functions (`bind_database`, currently ~L663-670):
`get_scalar_metadata`, `get_vector_metadata`, `get_set_metadata`, `list_scalar_attributes`,
`list_vector_groups`, `list_set_groups`. Each has its own sol2 marshaller
(`scalar_metadata_lua`, `get_*_metadata_lua`, `list_*_metadata_lua`, ~L1583-1711).
`scalar_metadata_lua` maps `data_type` to `"integer"`/`"real"`/`"text"`/`"date_time"` and omits
missing default/FK fields.

`grep -rn "db:get_\|db:list_" tests bindings/*/test*` finds only `db:get_time_series_metadata`,
`db:list_time_series_groups` and `db:list_time_series_files_columns`
(`tests/test_lua_runner_time_series.cpp`). **No test in any layer calls the six from Lua.** The
time-series test reaches `scalar_metadata_lua` but only asserts `value_columns[1].name`. The table
shape is promised to an LLM in `bindings/js/src/lua-api.ts` (~L514-535:
`data_type = "integer", -- "integer" | "real" | "text" | "date_time"`).

In the C++ core, `list_set_groups` has no test of its own, and `list_vector_groups` is checked once
with only `EXPECT_FALSE(...empty())` (`tests/test_database_lifecycle.cpp`, ~L518). The C API
(`tests/test_c_api_database_metadata.cpp`, ~L97/~L114) and every FFI binding do test them.

Root rule: tests must exist in every layer the behaviour is visible in.

## Constraints and decisions

- Test-only. No production code.
- Do not assert `x == nil` as the only proof of an absent field: that passes vacuously if the whole
  table is wrong. Pair every nil check with a positive check on the same table. Cover the
  `default_value` "has value" branch through `basic.sql` (`integer_attribute INTEGER DEFAULT 6`).
- Use the fixtures and schemas that already exist: `LuaRunnerTest` (`tests/test_lua_runner.h`,
  `collections_schema` member) and `VALID_SCHEMA("relations.sql")` / `VALID_SCHEMA("basic.sql")`.
  Assertions are Lua `assert(...)`, in the file's style.

## Changes

None to production code.

## Tests

### New Lua tests — append to `tests/test_lua_runner_time_series.cpp` under a `// Metadata getters` banner

(Or create `tests/test_lua_runner_metadata.cpp`. If you do, register it in `tests/CMakeLists.txt`
next to the other `test_lua_runner_*.cpp` entries, and add it to the Lua per-area list in
`tests/AGENTS.md`.)

```cpp
TEST_F(LuaRunnerTest, GetScalarMetadataForeignKey) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"));
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local m = db:get_scalar_metadata("Child", "parent_id")
        assert(m.name == "parent_id", "name")
        assert(m.data_type == "integer", "data_type: " .. tostring(m.data_type))
        assert(m.not_null == false, "not_null")
        assert(m.primary_key == false, "primary_key")
        assert(m.is_foreign_key == true, "is_foreign_key")
        assert(m.references_collection == "Parent", "references_collection")
        assert(m.references_column == "id", "references_column")
        assert(m.default_value == nil, "no default")

        local l = db:get_scalar_metadata("Child", "label")
        assert(l.data_type == "text" and l.not_null == true, "label is TEXT NOT NULL")
        assert(l.is_foreign_key == false and l.references_collection == nil, "label is not an FK")
    )");
}

TEST_F(LuaRunnerTest, GetScalarMetadataDefaultValue) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local m = db:get_scalar_metadata("Configuration", "integer_attribute")
        assert(m.data_type == "integer", "data_type")
        assert(m.default_value == "6", "default_value: " .. tostring(m.default_value))
        assert(db:get_scalar_metadata("Configuration", "float_attribute").data_type == "real", "real")
    )");
}

TEST_F(LuaRunnerTest, GetVectorAndSetMetadata) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local v = db:get_vector_metadata("Collection", "values")
        assert(v.group_name == "values", "vector group_name")
        assert(#v.value_columns == 2, "two vector value columns, got " .. #v.value_columns)
        assert(v.value_columns[1].name == "value_int" and v.value_columns[1].data_type == "integer", "value_int")
        assert(v.value_columns[2].name == "value_float" and v.value_columns[2].data_type == "real", "value_float")

        local s = db:get_set_metadata("Collection", "tags")
        assert(s.group_name == "tags", "set group_name")
        assert(#s.value_columns == 1 and s.value_columns[1].name == "tag", "tag column")
        assert(s.value_columns[1].data_type == "text", "tag is text")
    )");
}

TEST_F(LuaRunnerTest, ListScalarAttributesAndGroups) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local names = {}
        for i, a in ipairs(db:list_scalar_attributes("Collection")) do names[i] = a.name end
        assert(table.concat(names, ",") == "id,label,some_integer,some_float",
               "declaration order, got " .. table.concat(names, ","))

        local vg = db:list_vector_groups("Collection")
        assert(#vg == 1 and vg[1].group_name == "values", "vector groups")
        assert(vg[1].value_columns[1].name == "value_int", "vector columns in declaration order")

        local sg = db:list_set_groups("Collection")
        assert(#sg == 1 and sg[1].group_name == "tags", "set groups")
    )");
}
```

Before committing, verify these against the schemas and the core:
- `collections.sql`: `Collection(id, label, some_integer INTEGER, some_float REAL)`,
  `Collection_vector_values(id, vector_index, value_int INTEGER, value_float REAL)`,
  `Collection_set_tags(id, tag TEXT)`. They match at HEAD.
- Whether `list_scalar_attributes` includes `id`. Run
  `./build/bin/quiver_tests.exe --gtest_filter=*ListScalarAttributes*` and read the C++ test, or
  check `Database::list_scalar_attributes` in `src/database_metadata.cpp`. Adjust the expected
  string (`"id,label,..."` vs `"label,..."`) to the real, declaration-ordered result.
- `default_value` for `DEFAULT 6` is the string `"6"`. `ScalarMetadata::default_value` is
  `std::optional<std::string>`; check `include/quiver/attribute_metadata.h`.
- Whether the schema needs a Configuration row. It does not for metadata reads.

### New C++ core test — `tests/test_database_metadata.cpp`

```cpp
TEST(Database, ListVectorAndSetGroups) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto vectors = db.list_vector_groups("Collection");
    ASSERT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0].group_name, "values");
    ASSERT_EQ(vectors[0].value_columns.size(), 2);
    EXPECT_EQ(vectors[0].value_columns[0].name, "value_int");
    EXPECT_EQ(vectors[0].value_columns[1].name, "value_float");
    EXPECT_TRUE(vectors[0].dimension_column.empty());

    auto sets = db.list_set_groups("Collection");
    ASSERT_EQ(sets.size(), 1);
    EXPECT_EQ(sets[0].group_name, "tags");
    ASSERT_EQ(sets[0].value_columns.size(), 1);
    EXPECT_EQ(sets[0].value_columns[0].name, "tag");
}
```

## Docs and changelog

- If you created `test_lua_runner_metadata.cpp`, add it to the Lua per-area file list in
  `tests/AGENTS.md` (~L33-35). Plan 75 also edits that list; add the name in whichever lands later.
- No CHANGELOG entry (test-only).

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.GetScalarMetadata*:LuaRunnerTest.GetVectorAndSetMetadata:LuaRunnerTest.ListScalarAttributesAndGroups:Database.ListVectorAndSetGroups`
3. `./build/bin/quiver_tests.exe` (full suite)

## Acceptance criteria

- [ ] Every one of the six Lua getters is called by at least one test, with positive assertions on
      `data_type`, FK fields and order.
- [ ] `Database.ListVectorAndSetGroups` exists and passes.

## Pitfalls

- If plan 49 has landed, vector and set tables must **not** have a `dimension_column` key. Add
  `assert(v.dimension_column == nil)` next to a positive check to pin that.
- `ipairs` on these results is safe: they are dense arrays, unlike nullable bulk reads.

## Out of scope

- Time-series metadata (already tested).
- Any change to the getters (plan 49).
