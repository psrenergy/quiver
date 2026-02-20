# Phase 14: Verification and Cleanup - Research

**Researched:** 2026-02-19
**Domain:** Test coverage (Lua), dead code removal, documentation update, full suite verification
**Confidence:** HIGH

## Summary

Phase 14 is a cleanup and verification phase. The codebase is in better shape than the roadmap anticipated: the old single-column C API time series functions have **already been removed** during Phase 11. The C API header (`include/quiver/c/database.h`) only contains the new multi-column functions (`quiver_database_read_time_series_group`, `quiver_database_update_time_series_group`, `quiver_database_free_time_series_data`). No old `quiver_database_read_time_series_floats_by_id` or similar functions exist anywhere in the codebase. The Julia and Dart FFI bindings were already regenerated in Phases 12 and 13, and they only reference the new multi-column signatures.

The remaining work divides into three areas: (1) adding Lua multi-column time series tests using `mixed_time_series.sql`, matching Julia/Dart test depth, (2) performing a dead code sweep and any remaining cleanup, and (3) updating CLAUDE.md to document the multi-column time series API patterns. The final gate is `scripts/build-all.bat` passing all 4 test suites.

**Primary recommendation:** Focus on Lua test additions first (BIND-05), then CLAUDE.md updates, then full build verification. The "old API removal" success criterion is already satisfied -- verify and document this fact rather than trying to remove things that are already gone.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Use the same multi-column mixed-type schema (INTEGER + REAL + TEXT value columns) used by Julia and Dart tests
- Match Julia/Dart coverage depth: update, read back with type checking, multi-row, error cases -- full parity
- Add tests to the existing Lua test file, not a new dedicated file
- Also verify composite read helpers (`read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`) alongside time series
- Remove old public C API functions AND all internal helpers, types, and converters that become unused
- Delete old single-column C API tests -- Phase 11 already added multi-column coverage
- Remove old test schemas from `tests/schemas/valid/` that only served the single-column interface
- Regenerate both Julia and Dart FFI bindings after removing old C API functions to drop stale signatures
- Perform a full dead code clean sweep beyond just time series -- remove any confirmed unused functions, stale includes, etc.
- Leave untracked debug files (debug1.jl, debug1.sql, debug2.jl) as-is
- Update CMakeLists.txt, test configs, and any other build files to remove references to deleted files
- Final test gate is a single `scripts/build-all.bat` run -- if it passes, we're green
- Remove all references to old single-column time series API patterns that no longer exist from CLAUDE.md
- Add new documentation for the multi-column time series API patterns (columnar interface, C API signatures, binding usage)
- Add multi-column time series examples to the cross-layer naming table
- Remove specific test counts from documentation -- they become stale quickly

### Claude's Discretion
- Order of operations (which cleanup step happens first)
- Exact dead code detection strategy
- How to structure new CLAUDE.md time series documentation sections

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BIND-05 | Lua multi-column time series operations have test coverage (implementation already works via sol2) | Lua already has `read_time_series_group` and `update_time_series_group` bound via sol2 in `lua_runner.cpp` (lines 278-285). Existing tests use `collections.sql` (single-column). New tests must use `mixed_time_series.sql` (4-column: date_time/temperature/humidity/status) to match Julia/Dart coverage depth. Existing Lua test file is `tests/test_lua_runner.cpp` (1545 lines, 62+ tests). |
| MIGR-02 | All existing 1,213+ tests continue passing throughout migration | All 4 test suites must pass via `scripts/build-all.bat`. Current test counts are approximately: C++ ~392, C API ~257, Julia ~351+, Dart ~227+. The total has grown beyond 1,213 due to new tests added in Phases 11-13. |
| MIGR-03 | Old C API functions removed only after all bindings migrated to new interface | **Already satisfied.** Old single-column C API functions were removed during Phase 11. Bindings were migrated in Phases 12 (Julia) and 13 (Dart). No old function signatures remain in headers, implementation, or FFI bindings. |
</phase_requirements>

## Standard Stack

No new libraries or dependencies needed. This phase operates entirely within the existing codebase.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| GoogleTest | existing | C++ test framework | Already used for all C++ and C API tests |
| sol2 | existing | Lua/C++ binding | Already powers LuaRunner, time series already bound |
| spdlog | existing | Logging | Already used throughout |

## Architecture Patterns

### Existing Lua Time Series Test Structure

The Lua test file (`tests/test_lua_runner.cpp`) uses the `LuaRunnerTest` fixture class with `collections_schema` member. The existing time series tests (lines 1348-1544) demonstrate the patterns:

```cpp
// Existing single-column time series test pattern
TEST_F(LuaRunnerTest, ReadTimeSeriesGroupByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    // Setup data via C++, test via Lua
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"value", 1.5}},
    };
    db.update_time_series_group("Collection", "data", id, rows);

    quiver::LuaRunner lua(db);
    std::string script = R"(
        local rows = db:read_time_series_group("Collection", "data", )" + std::to_string(id) + R"()
        assert(#rows == ..., "...")
    )";
    lua.run(script);
}
```

### New Multi-Column Test Pattern (Based on Julia/Dart)

New tests should use `mixed_time_series.sql` (Sensor_time_series_readings: date_time TEXT, temperature REAL, humidity INTEGER, status TEXT). The Lua binding returns time series data as an array of tables (row-oriented, not columnar like Julia/Dart). Each row is a Lua table with column names as keys.

```cpp
// Multi-column mixed-type test pattern for Lua
TEST_F(LuaRunnerTest, MultiColumnTimeSeriesUpdateAndRead) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql"));
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Sensor", { label = "Sensor 1" })

        db:update_time_series_group("Sensor", "readings", 1, {
            { date_time = "2024-01-01T10:00:00", temperature = 20.5, humidity = 65, status = "ok" },
            { date_time = "2024-01-01T11:00:00", temperature = 21.0, humidity = 70, status = "warn" },
        })

        local rows = db:read_time_series_group("Sensor", "readings", 1)
        assert(#rows == 2, "Expected 2 rows")
        assert(rows[1].date_time == "2024-01-01T10:00:00", "...")
        assert(rows[1].temperature == 20.5, "...")
        assert(rows[1].humidity == 65, "...")
        assert(rows[1].status == "ok", "...")
    )");
}
```

### Lua vs Julia/Dart Return Format Difference

**Critical difference:** Lua's `read_time_series_group` returns **row-oriented** data (array of tables), while Julia and Dart return **column-oriented** data (Dict/Map of column -> array). This is because Lua binds directly to the C++ layer via sol2, which uses `vector<map<string, Value>>` row format. Julia/Dart go through the C API which returns columnar typed arrays.

This means:
- Lua: `rows[1].temperature == 20.5` (row[1], field "temperature")
- Julia: `result["temperature"][1] == 20.5` (column "temperature", row 1)
- Dart: `result['temperature']![0] == 20.5` (column "temperature", row 0)

### Test Categories to Add (Matching Julia/Dart Depth)

Based on Julia/Dart test coverage analysis:

1. **Multi-column update and read** -- 4-column mixed types, verify each column type
2. **Multi-column read empty** -- read before any update
3. **Multi-column replace** -- two updates, verify second replaces first
4. **Multi-column clear** -- update with empty rows
5. **Multi-column ordering** -- insert out of order, verify dimension-column ordering on read
6. **Multi-row verification** -- verify multiple rows with all column types
7. **Error cases** -- missing group, invalid collection (may already be covered)
8. **Composite read helpers** -- `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` (user decision)

### CLAUDE.md Update Patterns

Current CLAUDE.md references time series in:
- Architecture section (`src/c/database_time_series.cpp` description) -- correct, no change needed
- Schema conventions (time series table naming) -- correct, no change needed
- Core API section (Database Class methods) -- needs multi-column API detail
- Cross-layer naming table -- already has `read_time_series_group` row, needs `update_time_series_group` row
- Binding convenience methods -- may need update
- C API patterns -- needs multi-column function signature documentation
- Test organization section -- mentions test counts "1,213+" that should be removed per user decision

### Anti-Patterns to Avoid
- **Do not add new test files** -- add to existing `tests/test_lua_runner.cpp`
- **Do not try to remove non-existent old functions** -- they're already gone
- **Do not remove `multi_time_series.sql`** -- it's still used by create tests (C++, C API, Julia, Dart, Lua)
- **Do not regenerate FFI bindings** -- already done in Phases 12/13, and no C API changes in this phase

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lua time series binding | New Lua binding code | Existing sol2 bindings in `lua_runner.cpp` | Already fully implemented -- `read_time_series_group` and `update_time_series_group` are already bound (lines 278-285) |
| Dead code detection | Manual file-by-file review | Grep-based approach on function names | More reliable than reading every file |
| FFI regeneration | Running generators | Skip entirely | Already done in Phases 12/13, no C API header changes in Phase 14 |

**Key insight:** This phase is almost entirely additive (new tests, updated docs) rather than subtractive. The "removal" work was already completed in earlier phases.

## Common Pitfalls

### Pitfall 1: Trying to Remove Already-Removed Code
**What goes wrong:** Planner creates tasks to remove old single-column C API functions that don't exist.
**Why it happens:** The roadmap was written before Phase 11 execution, which replaced (rather than added alongside) the old functions.
**How to avoid:** Verify the codebase state first. The old functions are confirmed absent from `include/quiver/c/database.h`, `src/c/database_time_series.cpp`, and all FFI binding files.
**Warning signs:** Grep for old function names returns zero results.

### Pitfall 2: Lua Row-Oriented vs Binding Column-Oriented API
**What goes wrong:** Tests assume Lua returns columnar data like Julia/Dart.
**Why it happens:** Lua binds directly to C++ (sol2), which returns `vector<map<string, Value>>`. Julia/Dart go through the C API which returns columnar typed arrays.
**How to avoid:** Lua tests must use `rows[i].field_name` access pattern, not `result["column"][i]`.
**Warning signs:** Lua test assertions don't match the Lua return format.

### Pitfall 3: Lua Number Type Ambiguity
**What goes wrong:** Lua numbers are all doubles by default (in Lua 5.3+ there are integers, but sol2 type detection can be tricky). An INTEGER column value might be returned as a float in Lua.
**Why it happens:** `value_to_lua_object` in `lua_runner.cpp` uses `std::visit` and converts based on C++ variant type, which should correctly distinguish int64_t and double. But Lua comparison `65 == 65.0` is true in Lua, so this may not cause test failures.
**How to avoid:** Use exact value comparisons in Lua assertions. Since the C++ layer stores integers as `int64_t` and sol2 correctly maps these, the distinction should be preserved. Verify with `type(rows[1].humidity)` assertions if needed.
**Warning signs:** Type assertions fail even though values match.

### Pitfall 4: Test Count Expectation
**What goes wrong:** MIGR-02 says "1,213+ tests" but the actual count has grown significantly with tests added in Phases 11-13.
**Why it happens:** Test count was snapshotted at v1.0 milestone.
**How to avoid:** Don't assert a specific test count. Verify all suites pass via `scripts/build-all.bat` exit code. The user decision explicitly says to remove specific test counts from documentation.
**Warning signs:** Counting tests manually and getting confused by the discrepancy.

### Pitfall 5: Forgetting multi_time_series.sql vs mixed_time_series.sql
**What goes wrong:** Confusing the two schemas or accidentally removing the wrong one.
**Why it happens:** Similar names.
**How to avoid:** `multi_time_series.sql` has multiple single-column time series groups (temperature, humidity as separate tables). `mixed_time_series.sql` has one multi-column group (readings: temperature REAL + humidity INTEGER + status TEXT). Both are actively used. Neither should be removed.
**Warning signs:** Removing a schema that still has test references.

## Code Examples

### Existing Lua Time Series Binding (lua_runner.cpp lines 278-285)
```cpp
// Group 8: Time series data
"read_time_series_group",
[](Database& self, const std::string& collection, const std::string& group, int64_t id, sol::this_state s) {
    return read_time_series_group_to_lua(self, collection, group, id, s);
},
"update_time_series_group",
[](Database& self, const std::string& collection, const std::string& group, int64_t id, sol::table rows) {
    update_time_series_group_from_lua(self, collection, group, id, rows);
},
```

### Lua read_time_series_group Return Format (lua_runner.cpp lines 1040-1056)
```cpp
static sol::table read_time_series_group_to_lua(Database& db,
                                                const std::string& collection,
                                                const std::string& group,
                                                int64_t id,
                                                sol::this_state s) {
    sol::state_view lua(s);
    auto rows = db.read_time_series_group(collection, group, id);
    auto t = lua.create_table();
    for (size_t i = 0; i < rows.size(); ++i) {
        auto row = lua.create_table();
        for (const auto& [key, val] : rows[i]) {
            row[key] = value_to_lua_object(lua, val);
        }
        t[i + 1] = row;
    }
    return t;
}
```

### Lua update_time_series_group Input Format (lua_runner.cpp lines 1058-1069)
```cpp
static void update_time_series_group_from_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& group,
                                              int64_t id,
                                              sol::table rows) {
    std::vector<std::map<std::string, Value>> cpp_rows;
    for (size_t i = 1; i <= rows.size(); ++i) {
        sol::table row = rows[i];
        cpp_rows.push_back(lua_table_to_value_map(row));
    }
    db.update_time_series_group(collection, group, id, cpp_rows);
}
```

### mixed_time_series.sql Schema (for Lua test reference)
```sql
CREATE TABLE Sensor_time_series_readings (
    id INTEGER NOT NULL REFERENCES Sensor(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity INTEGER NOT NULL,
    status TEXT NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

### Lua Test Pattern for Composite Read Helpers
```cpp
// read_all_scalars_by_id already exists in lua_runner.cpp
// Test pattern:
lua.run(R"(
    local scalars = db:read_all_scalars_by_id("Collection", 1)
    assert(scalars.label == "Item 1", "...")
    assert(scalars.some_integer == 42, "...")
)");
```

### Julia Multi-Column Test Reference (for parity comparison)
```julia
# Julia: column-oriented
result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
@test result["temperature"] isa Vector{Float64}
@test result["temperature"] == [20.5, 21.3]
@test result["humidity"] isa Vector{Int64}
@test result["humidity"] == [45, 50]
@test result["status"] isa Vector{String}
@test result["status"] == ["normal", "high"]
```

### Equivalent Lua Test (row-oriented)
```lua
-- Lua: row-oriented
local rows = db:read_time_series_group("Sensor", "readings", 1)
assert(#rows == 2, "Expected 2 rows")
assert(rows[1].temperature == 20.5, "Expected 20.5")
assert(rows[1].humidity == 65, "Expected 65")
assert(rows[1].status == "ok", "Expected 'ok'")
assert(rows[2].temperature == 21.0, "Expected 21.0")
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Single-column C API (`read_time_series_floats_by_id`) | Multi-column C API (`read_time_series_group`) | Phase 11 | Old functions already removed, C API only has multi-column |
| Julia single-column time series | Julia kwargs multi-column | Phase 12 | Already migrated and tested |
| Dart single-column time series | Dart Map-based multi-column | Phase 13 | Already migrated and tested |
| Lua tests on collections.sql only | Lua tests on mixed_time_series.sql | Phase 14 (this phase) | New tests needed |

**Already completed:**
- Old single-column C API functions: removed in Phase 11
- Julia FFI regeneration: done in Phase 12
- Dart FFI regeneration: done in Phase 13

## Codebase State Assessment

### What the Roadmap Says vs What Exists

| Roadmap Success Criterion | Current State | Phase 14 Work Needed |
|--------------------------|---------------|---------------------|
| "Lua multi-column time series operations have passing test coverage using mixed-type schema" | Lua has single-column time series tests (collections.sql). No mixed_time_series.sql tests. | **YES** -- add multi-column tests |
| "Old single-column C API time series functions are removed from headers and implementation" | **Already done.** No old functions exist in headers or implementation. | **NO** -- just verify and document |
| "All 1,213+ tests pass across all 4 suites via scripts/build-all.bat" | Unknown -- need to run. Tests have grown beyond 1,213 with Phase 11-13 additions. | **YES** -- run build-all.bat as final gate |

### Files That Need Changes

| File | Change Type | Reason |
|------|-------------|--------|
| `tests/test_lua_runner.cpp` | Add tests | Multi-column time series tests with mixed_time_series.sql |
| `CLAUDE.md` | Update | Add multi-column API docs, remove stale test counts, update time series sections |

### Files That Do NOT Need Changes (Contrary to CONTEXT.md Expectations)

| Expected Change | Why Not Needed |
|----------------|----------------|
| Remove old C API functions from headers | Already removed in Phase 11 |
| Remove old C API function implementations | Already removed in Phase 11 |
| Delete old single-column C API tests | Already replaced with multi-column tests in Phase 11 |
| Remove old test schemas | `multi_time_series.sql` is still used by create tests; `mixed_time_series.sql` is the new schema. No schemas are obsolete. |
| Regenerate Julia/Dart FFI bindings | Already done in Phases 12/13; no C API changes in Phase 14 |
| Update CMakeLists.txt | No files being removed |

### Dead Code Assessment

No dead code related to old single-column time series functions was found. The codebase has already been cleaned. A general dead code sweep should look for:
- Unused includes in modified files
- Functions declared but not called
- Test helpers no longer referenced

Based on the codebase analysis, no obvious dead code was detected in the time-series-related files. The `c_type_name()` helper, `to_c_data_type()`, `convert_group_to_c()`, `strdup_safe()`, and all other helpers are actively used.

## Open Questions

1. **Test count verification**
   - What we know: v1.0 had 1,213 tests. Phases 11-13 added tests. The exact current count is unknown without running the suites.
   - What's unclear: The exact number of tests after Phase 13. The user decision says to remove specific counts from docs.
   - Recommendation: Run `scripts/build-all.bat` and note the actual counts but don't hardcode them in CLAUDE.md.

2. **Composite read helper tests scope**
   - What we know: User locked the decision to "verify composite read helpers alongside time series."
   - What's unclear: Whether `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` already have Lua tests.
   - Recommendation: Check existing tests in `test_lua_runner.cpp` -- they do NOT currently have dedicated tests for these helpers. Add tests.

3. **Dead code beyond time series**
   - What we know: User wants "full dead code clean sweep beyond just time series."
   - What's unclear: Whether any non-time-series dead code exists after v1.0 and v1.1 cleanups.
   - Recommendation: Use grep to check for declared-but-unused functions in public headers vs implementation files. This is likely minimal.

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis of `include/quiver/c/database.h` -- confirmed no old single-column time series functions
- Direct codebase analysis of `src/c/database_time_series.cpp` -- only multi-column implementation
- Direct codebase analysis of `src/lua_runner.cpp` -- existing time series bindings at lines 278-285, helper functions at lines 1040-1069
- Direct codebase analysis of `tests/test_lua_runner.cpp` -- 1545 lines, existing time series tests at lines 1348-1544
- Direct codebase analysis of `tests/schemas/valid/mixed_time_series.sql` -- 4-column schema (date_time, temperature REAL, humidity INTEGER, status TEXT)
- Phase 11 verification report (`.planning/phases/11-c-api-multi-column-time-series/11-VERIFICATION.md`) -- confirms all old functions replaced

### Secondary (MEDIUM confidence)
- Julia test patterns in `bindings/julia/test/test_database_time_series.jl` -- used as reference for coverage depth
- Dart test patterns in `bindings/dart/test/database_time_series_test.dart` -- used as reference for coverage depth

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, all existing tools
- Architecture: HIGH - direct codebase analysis of all relevant files, clear patterns
- Pitfalls: HIGH - identified from concrete codebase analysis and Lua/C++ binding specifics

**Research date:** 2026-02-19
**Valid until:** 2026-03-19 (stable phase, no external dependencies)
