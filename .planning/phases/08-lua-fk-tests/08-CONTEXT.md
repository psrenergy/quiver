# Phase 8: Lua FK Tests - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

16 FK resolution tests (9 create + 7 update) through the Lua scripting layer, mirroring the C++ FK tests. Tests verify that `create_element` and `update_element` / individual update methods correctly resolve FK string labels to integer IDs when called from Lua scripts. No new Lua bindings or FK resolution code — this is test-only.

</domain>

<decisions>
## Implementation Decisions

### Verification layer
- Write operations happen in Lua scripts via `lua.run()`, verification happens in C++ with `EXPECT_EQ` reading back values through the C++ Database API
- Error cases use `EXPECT_THROW` on `lua.run()` — no message content verification (just confirm exception is thrown)
- "No partial writes" test verifies element count is zero (no Child elements exist at all after failed create), not just that FK columns are empty

### Update path coverage
- FK update tests cover BOTH update paths: individual typed methods (`update_scalar_integer`, `update_vector_integers`, etc.) AND bulk `update_element(collection, id, {key=val})`
- Claude's discretion on how to split the 7 update test cases across the two paths
- "Failure preserves existing" test: Lua calls update with bad FK label (expect throw), then C++ reads back to confirm original values unchanged

### Test organization
- All FK tests added to existing `test_lua_runner.cpp` (not a new file)
- FK tests grouped together at the end of the file with a clear comment block separator
- Separate test fixture `LuaRunnerFkTest` with `relations.sql` loaded in `SetUp()` — clean separation from existing `LuaRunnerTest` fixture

### Claude's Discretion
- Exact split of 7 update tests between individual typed methods and bulk `update_element`
- Test naming convention (follow existing `LuaRunnerTest` naming patterns)
- Helper setup code (e.g., creating Parent elements before FK resolution tests)

</decisions>

<specifics>
## Specific Ideas

- Verification pattern is consistent: Lua writes, C++ reads — this proves FK labels pass correctly through the Lua → C++ boundary
- Error tests are simple EXPECT_THROW — no message string matching to avoid brittleness
- "Both update paths" gives Lua-specific coverage that other bindings don't have (Julia/Dart only have one update mechanism each)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 08-lua-fk-tests*
*Context gathered: 2026-02-24*
