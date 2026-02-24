# Phase 8: Lua FK Tests - Research

**Researched:** 2026-02-24
**Domain:** Lua scripting layer tests for FK label-to-ID resolution
**Confidence:** HIGH

## Summary

Phase 8 adds 16 FK resolution tests to the existing `tests/test_lua_runner.cpp` file (9 create + 7 update), completing the final binding layer in the v1.1 FK test coverage milestone. This is a test-only phase -- no new Lua bindings, no FK resolution code changes, and no new schema files. The FK resolution logic lives entirely in the C++ `Database::create_element` and `Database::update_element` methods, which the Lua `table_to_element` helper already feeds correctly by mapping Lua string values to `Element::set(key, string)` and Lua string arrays to `Element::set(key, vector<string>)`.

The key Lua-specific distinction from other bindings (Julia, Dart) is that Lua exposes TWO update paths: the bulk `update_element(collection, id, {key=val})` method (which routes through `table_to_element` -> `Element` -> `Database::update_element` with FK resolution) AND individual typed methods (`update_scalar_integer`, `update_vector_integers`, `update_set_integers`, `update_time_series_group`) which take already-resolved typed values and bypass FK resolution entirely. The CONTEXT.md decision to cover "both update paths" means some FK update tests use `update_element` (string labels resolved) and some use individual typed methods (integer values passed directly, no resolution).

**Primary recommendation:** Add a `LuaRunnerFkTest` fixture class using `relations.sql` schema at the end of `test_lua_runner.cpp`. Write operations happen in Lua via `lua.run()`, verification happens in C++ with `EXPECT_EQ`. Error cases use `EXPECT_THROW` on `lua.run()` without message inspection.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Write operations happen in Lua scripts via `lua.run()`, verification happens in C++ with `EXPECT_EQ` reading back values through the C++ Database API
- Error cases use `EXPECT_THROW` on `lua.run()` -- no message content verification (just confirm exception is thrown)
- "No partial writes" test verifies element count is zero (no Child elements exist at all after failed create), not just that FK columns are empty
- FK update tests cover BOTH update paths: individual typed methods (`update_scalar_integer`, `update_vector_integers`, etc.) AND bulk `update_element(collection, id, {key=val})`
- Claude's discretion on how to split the 7 update test cases across the two paths
- "Failure preserves existing" test: Lua calls update with bad FK label (expect throw), then C++ reads back to confirm original values unchanged
- All FK tests added to existing `test_lua_runner.cpp` (not a new file)
- FK tests grouped together at the end of the file with a clear comment block separator
- Separate test fixture `LuaRunnerFkTest` with `relations.sql` loaded in `SetUp()` -- clean separation from existing `LuaRunnerTest` fixture

### Claude's Discretion
- Exact split of 7 update tests between individual typed methods and bulk `update_element`
- Test naming convention (follow existing `LuaRunnerTest` naming patterns)
- Helper setup code (e.g., creating Parent elements before FK resolution tests)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LUA-01 | Lua FK resolution tests for create_element cover all 9 test cases mirroring C++ create path | All 9 test cases mapped below; `table_to_element` correctly routes Lua strings to `Element::set(key, string)` and Lua string arrays to `Element::set(key, vector<string>)`, which triggers FK resolution in `Database::create_element`. Verification via C++ read-back methods confirmed available. |
| LUA-02 | Lua FK resolution tests for update_element cover all 7 test cases mirroring C++ update path | All 7 test cases mapped below; `update_element` from Lua routes through same `table_to_element` -> `Database::update_element` with FK resolution. Individual typed methods (`update_scalar_integer`, etc.) available for non-FK update path testing. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Google Test | (project dep) | C++ test framework | Already used by all existing tests including `test_lua_runner.cpp` |
| quiver::LuaRunner | (project lib) | Lua scripting API under test | Existing class that exposes all Database methods to Lua via sol2 |
| quiver::Database | (project lib) | C++ Database API for verification reads | Already used in all existing LuaRunnerTest tests |

### Supporting
No additional libraries needed. This is pure test code using existing dependencies.

## Architecture Patterns

### Test File Structure
```
tests/
  test_lua_runner.cpp   # Existing file (~2124 lines)
    class LuaRunnerTest     # Existing fixture (collections.sql)
    ... existing tests ...
    // FK resolution tests  # NEW: comment block separator
    class LuaRunnerFkTest   # NEW: fixture with relations.sql
    ... 16 FK tests ...     # NEW: 9 create + 7 update
```

### Pattern 1: Lua FK Create Test (write in Lua, verify in C++)
**What:** Lua script creates element with string FK labels, C++ reads back resolved integer IDs
**When to use:** Every FK create test
**Example:**
```cpp
TEST_F(LuaRunnerFkTest, CreateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Parent", { label = "Parent 1" })
        db:create_element("Child", {
            label = "Child 1",
            parent_id = "Parent 1"   -- string label, not integer ID
        })
    )");

    // Verify: C++ reads back resolved integer
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);
}
```

### Pattern 2: Lua FK Error Test (EXPECT_THROW on lua.run)
**What:** Lua script attempts invalid FK operation, exception propagates to C++
**When to use:** Missing target label, non-FK integer column with string
**Example:**
```cpp
TEST_F(LuaRunnerFkTest, CreateElementMissingFkTarget) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    EXPECT_THROW({
        lua.run(R"(
            db:create_element("Child", {
                label = "Child 1",
                mentor_id = {"Nonexistent Parent"}
            })
        )");
    }, std::runtime_error);
}
```

### Pattern 3: Lua FK Update Test (bulk update_element path)
**What:** Create initial element, then Lua updates FK column with string label via `update_element`
**When to use:** FK resolution through bulk update path
**Example:**
```cpp
TEST_F(LuaRunnerFkTest, UpdateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Parent", { label = "Parent 1" })
        db:create_element("Parent", { label = "Parent 2" })
        db:create_element("Child", { label = "Child 1", parent_id = "Parent 1" })
        db:update_element("Child", 1, { parent_id = "Parent 2" })
    )");

    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);
}
```

### Pattern 4: Lua FK Update Test (individual typed method path)
**What:** Create element, then Lua updates using individual typed methods (no FK resolution -- uses integer IDs directly)
**When to use:** Verifying individual typed methods still work alongside FK resolution
**Example:**
```cpp
TEST_F(LuaRunnerFkTest, UpdateVectorIntegersIndividualMethod) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Parent", { label = "Parent 1" })
        db:create_element("Parent", { label = "Parent 2" })
        db:create_element("Child", {
            label = "Child 1",
            parent_ref = {"Parent 1"}
        })
        -- Individual typed method: pass integer IDs directly
        db:update_vector_integers("Child", "parent_ref", 1, {2, 1})
    )");

    auto refs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(refs.size(), 2);
    EXPECT_EQ(refs[0], 2);
    EXPECT_EQ(refs[1], 1);
}
```

### Pattern 5: Time Series FK in Lua
**What:** Pass time series columns as parallel arrays in Lua table. `read_time_series_group` returns array of row tables.
**When to use:** Time series FK create and update tests
**Example:**
```cpp
TEST_F(LuaRunnerFkTest, CreateElementTimeSeriesFkLabels) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Parent", { label = "Parent 1" })
        db:create_element("Parent", { label = "Parent 2" })
        db:create_element("Child", {
            label = "Child 1",
            date_time = {"2024-01-01", "2024-01-02"},
            sponsor_id = {"Parent 1", "Parent 2"}
        })
    )");

    // read_time_series_group returns vector<map<string, Value>>
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 2);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[1].at("sponsor_id")), 2);
}
```

### Anti-Patterns to Avoid
- **Inspecting error message content:** Per locked decisions, use `EXPECT_THROW(..., std::runtime_error)` only -- do not match on `e.what()` substrings.
- **Verifying in Lua instead of C++:** The locked pattern is "Lua writes, C++ reads." All assertions go through `EXPECT_EQ` / `ASSERT_EQ`, not Lua `assert()`.
- **Creating a new test file:** Per locked decisions, all FK tests go in the existing `test_lua_runner.cpp`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FK resolution | Custom Lua-side label lookup | Pass string labels in Lua table -- C++ layer does resolution | Resolution logic lives in C++ `Database::create_element` / `update_element`; Lua passes strings through `table_to_element` |
| Element construction | Manual SQL or C API calls from Lua | `db:create_element("Child", {parent_id = "Parent 1"})` | `table_to_element` correctly maps Lua types to Element attributes |
| Time series verification | Lua-side readback and assertion | C++ `db.read_time_series_group()` with `std::get<int64_t>()` | Keeps verification in C++ as per locked decisions |

## Common Pitfalls

### Pitfall 1: Individual typed update methods do NOT support FK resolution
**What goes wrong:** Trying to pass a string label to `update_scalar_integer` -- it takes `int64_t`, not `string`
**Why it happens:** Only `update_element` routes through `Element` builder with FK resolution. Individual typed methods (`update_scalar_integer`, `update_vector_integers`, `update_set_integers`) take already-resolved typed values directly.
**How to avoid:** FK resolution update tests MUST use `update_element`. Individual typed method tests pass integer IDs, not string labels.
**Warning signs:** Lua runtime error about wrong argument type

### Pitfall 2: Set ordering in read-back
**What goes wrong:** Set read results may not be in insertion order
**Why it happens:** SQLite sets have no guaranteed ordering
**How to avoid:** Sort before comparing when reading back set integers. Vectors preserve insertion order.
**Warning signs:** Tests pass intermittently

### Pitfall 3: Time series group name vs table name
**What goes wrong:** Using `"Child_time_series_events"` instead of `"events"` as the group name
**Why it happens:** The table is `Child_time_series_events` but the API group name is `events` (suffix after `_time_series_`)
**How to avoid:** Always use `"events"` as the group name
**Warning signs:** "not found" errors from `read_time_series_group`

### Pitfall 4: Lua number type ambiguity
**What goes wrong:** Lua represents all numbers as `double` by default; `table_to_element` checks `is<int64_t>()` before `is<double>()`
**Why it happens:** `sol2` can distinguish integer-valued doubles from non-integer doubles, but edge cases exist
**How to avoid:** When passing integer IDs directly in Lua (for individual typed method tests), use whole numbers: `{1, 2}` not `{1.0, 2.0}`. sol2's `is<int64_t>()` check handles this correctly for whole numbers.
**Warning signs:** Values stored as floats instead of integers

### Pitfall 5: "No partial writes" means zero Child elements, not just empty FK columns
**What goes wrong:** Checking only that FK columns are empty/null instead of checking no Child row exists
**Why it happens:** Misreading the locked decision
**How to avoid:** Per CONTEXT.md: verify element count is zero -- check `db.read_scalar_strings("Child", "label").size() == 0`
**Warning signs:** Test passes but doesn't actually verify atomicity

### Pitfall 6: Time series update via update_element requires both dimension and value columns
**What goes wrong:** Omitting `date_time` when updating time series FK via `update_element`
**Why it happens:** Time series data is row-based; the dimension column is mandatory
**How to avoid:** Always include `date_time` alongside `sponsor_id` in time series updates
**Warning signs:** "Missing dimension column" or similar runtime error

## Code Examples

### Complete Create Test List (all 9 tests)

The 9 create tests mirror C++ `test_database_create.cpp` FK tests exactly:

| # | Test Name (suggested) | Schema | Lua Operation | C++ Verification |
|---|----------------------|--------|---------------|------------------|
| 1 | CreateElementSetFkLabels | relations.sql | `mentor_id = {"Parent 1", "Parent 2"}` | `read_set_integers("Child", "mentor_id")` -> sorted [1, 2] |
| 2 | CreateElementMissingFkTarget | relations.sql | `mentor_id = {"Nonexistent Parent"}` | `EXPECT_THROW(..., std::runtime_error)` |
| 3 | CreateElementStringForNonFkInteger | relations.sql | `score = {"not_a_label"}` | `EXPECT_THROW(..., std::runtime_error)` |
| 4 | CreateElementScalarFkLabel | relations.sql | `parent_id = "Parent 1"` | `read_scalar_integers("Child", "parent_id")` -> [1] |
| 5 | CreateElementVectorFkLabels | relations.sql | `parent_ref = {"Parent 1", "Parent 2"}` | `read_vector_integers_by_id("Child", "parent_ref", 1)` -> [1, 2] |
| 6 | CreateElementTimeSeriesFkLabels | relations.sql | `date_time = {...}, sponsor_id = {"Parent 1", "Parent 2"}` | `read_time_series_group("Child", "events", 1)` -> sponsor_id [1, 2] |
| 7 | CreateElementAllFkTypes | relations.sql | scalar + set + vector + time series FKs in single create | All read-back methods verify resolution |
| 8 | CreateElementNoFkUnchanged | basic.sql | No FK columns, integer/float/string values | Values pass through unmodified |
| 9 | CreateElementFkResolutionNoPartialWrites | relations.sql | `parent_id = "Nonexistent"` | `read_scalar_strings("Child", "label").size() == 0` |

### Complete Update Test List (all 7 tests)

Recommended split of 7 update tests across the two paths:

**Via `update_element` (bulk, with FK resolution) -- 5 tests:**

| # | Test Name (suggested) | Schema | Lua Operation | C++ Verification |
|---|----------------------|--------|---------------|------------------|
| 1 | UpdateElementScalarFkLabel | relations.sql | `update_element("Child", 1, {parent_id = "Parent 2"})` | `read_scalar_integers` -> [2] |
| 2 | UpdateElementSetFkLabels | relations.sql | `update_element("Child", 1, {mentor_id = {"Parent 2"}})` | `read_set_integers` -> [2] |
| 3 | UpdateElementAllFkTypes | relations.sql | Update all FK types to Parent 2 in single `update_element` | All read-back methods verify |
| 4 | UpdateElementFkFailurePreservesExisting | relations.sql | `update_element("Child", 1, {parent_id = "Nonexistent"})` | `EXPECT_THROW`, then read confirms original [1] |
| 5 | UpdateElementNoFkUnchanged | basic.sql | `update_element` with non-FK scalars | Values update correctly |

**Via individual typed methods (no FK resolution, integer IDs directly) -- 2 tests:**

| # | Test Name (suggested) | Schema | Lua Operation | C++ Verification |
|---|----------------------|--------|---------------|------------------|
| 6 | UpdateVectorFkViaTypedMethod | relations.sql | `update_vector_integers("Child", "parent_ref", 1, {2, 1})` | `read_vector_integers_by_id` -> [2, 1] |
| 7 | UpdateTimeSeriesFkViaTypedMethod | relations.sql | `update_time_series_group("Child", "events", 1, rows)` | `read_time_series_group` -> sponsor_id [2, 1] |

**Rationale for the split:** The 5 `update_element` tests cover FK resolution for scalar, set, combined, failure, and no-FK scenarios. The 2 individual typed method tests demonstrate that the alternative update path (vector and time series) works with pre-resolved integer IDs. This gives meaningful coverage of both paths without duplicating all 7 tests across both paths.

### Key Lua API Methods Used

**Write path (Lua):**
- `db:create_element("Child", { parent_id = "Parent 1", ... })` -- table with string labels for FK columns
- `db:update_element("Child", 1, { parent_id = "Parent 2" })` -- table with string labels for FK columns
- `db:update_vector_integers("Child", "parent_ref", 1, {2, 1})` -- individual typed method with integer IDs
- `db:update_time_series_group("Child", "events", 1, rows)` -- individual typed method for time series

**Read-back verification (C++):**
- `db.read_scalar_integers("Child", "parent_id")` -> `vector<int64_t>`
- `db.read_scalar_integer_by_id("Child", "parent_id", 1)` -> `optional<int64_t>`
- `db.read_vector_integers_by_id("Child", "parent_ref", 1)` -> `vector<int64_t>`
- `db.read_set_integers("Child", "mentor_id")` -> `vector<vector<int64_t>>`
- `db.read_time_series_group("Child", "events", 1)` -> `vector<map<string, Value>>` (use `std::get<int64_t>(row.at("sponsor_id"))`)
- `db.read_scalar_strings("Child", "label")` -> `vector<string>` (for zero-writes check)
- `db.read_scalar_floats("Configuration", "float_attribute")` -> `vector<double>` (for no-FK check)
- `db.read_scalar_float_by_id(...)`, `db.read_scalar_string_by_id(...)` (for no-FK update check)

### Schema Reference (relations.sql)

```
Parent(id, label)
Child(id, label, parent_id FK->Parent, sibling_id FK->Child)
Child_vector_refs(id, vector_index, parent_ref FK->Parent)
Child_set_parents(id, parent_ref FK->Parent)
Child_set_mentors(id, mentor_id FK->Parent)
Child_set_scores(id, score) -- NOT a FK, just integer
Child_time_series_events(id, date_time, sponsor_id FK->Parent)
```

Group names derived from table naming convention:
- Vector: `refs` (from `Child_vector_refs`)
- Set: `parents`, `mentors`, `scores` (from `Child_set_*`)
- Time series: `events` (from `Child_time_series_events`)

### LuaRunnerFkTest Fixture

```cpp
class LuaRunnerFkTest : public ::testing::Test {
protected:
    void SetUp() override { relations_schema = VALID_SCHEMA("relations.sql"); }
    std::string relations_schema;
};
```

### Test Naming Convention
Existing `LuaRunnerTest` uses `TEST_F(LuaRunnerTest, CamelCaseDescription)` pattern. Follow same convention: `TEST_F(LuaRunnerFkTest, CreateElementScalarFkLabel)`, etc.

### How `table_to_element` Routes FK Labels

The Lua `table_to_element` function (lines 402-442 of `src/lua_runner.cpp`) handles:
1. `parent_id = "Parent 1"` -> `val.is<std::string>()` -> `element.set(k, val.as<std::string>())` -- scalar FK label
2. `mentor_id = {"Parent 1", "Parent 2"}` -> `val.is<sol::table>()`, first element `is<std::string>()` -> `element.set(k, vector<string>)` -- set/vector/time series FK labels
3. `some_integer = 42` -> `val.is<int64_t>()` -> `element.set(k, val.as<int64_t>())` -- non-FK integer passthrough

This means FK resolution works identically to C++/C API/Julia/Dart -- the Lua layer is transparent.

## Open Questions

None. All API methods, test patterns, schemas, and the two update paths are fully understood. The Lua tests are a direct translation of the C++ tests with Lua syntax for writes and C++ assertions for reads.

## Sources

### Primary (HIGH confidence)
- `tests/test_lua_runner.cpp` -- existing Lua test file (2124 lines, ~60 tests, fixture patterns, naming conventions)
- `src/lua_runner.cpp` -- Lua binding implementation (table_to_element at line 402, update_element at line 117, individual typed update methods at lines 178-241, time series at lines 266-272)
- `tests/test_database_create.cpp` -- C++ FK create tests (lines 341-567, all 9 test cases)
- `tests/test_database_update.cpp` -- C++ FK update tests (lines 650-878, all 7 test cases)
- `tests/schemas/valid/relations.sql` -- FK test schema (62 lines)
- `tests/schemas/valid/basic.sql` -- no-FK test schema (13 lines)
- `tests/test_utils.h` -- VALID_SCHEMA macro definition
- `.planning/phases/07-dart-fk-tests/07-RESEARCH.md` -- prior phase research (pattern reference)
- `include/quiver/database.h` -- C++ API signatures confirming update_element takes Element (FK resolution) vs update_scalar_integer takes int64_t (no FK resolution)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- exact same test framework and patterns already in use in test_lua_runner.cpp
- Architecture: HIGH -- all API methods verified in source code, both update paths inspected
- Pitfalls: HIGH -- based on actual codebase inspection of type signatures and routing logic

**Research date:** 2026-02-24
**Valid until:** indefinite (test-only phase, no external dependencies)
