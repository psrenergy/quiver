# Phase 6: Julia FK Tests - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

16 FK resolution tests (9 create, 7 update) through Julia bindings, mirroring the C++ FK tests end-to-end. Test-only — no new binding methods or FK resolution code changes.

</domain>

<decisions>
## Implementation Decisions

### Test organization
- Same structure across all binding layers (Julia, Dart, Lua) — mirror C API phase 5 exactly
- 2 plans: 06-01 for create tests (9 tests), 06-02 for update tests (7 tests)
- Create tests appended to `bindings/julia/test/test_database_create.jl`
- Update tests appended to `bindings/julia/test/test_database_update.jl`
- Test names mirror C++ test names adapted to Julia `@testset` conventions

### FK label passing
- Pass string labels through Julia kwargs API (e.g., `parent_id = "Parent 1"`) — same pattern as C API where string labels in FK integer columns trigger resolution
- Read back resolved integer IDs to verify resolution worked

### Error assertions
- Use `@test_throws Quiver.DatabaseException` for error cases — same depth as C API tests
- Verify zero partial writes on resolution failure (check no rows were inserted)
- Mirror the same 9 create test cases and 7 update test cases from C++/C API

### Claude's Discretion
- Exact `@testset` naming within the Julia convention
- Helper setup code structure within each test
- Assertion ordering within each test case

</decisions>

<specifics>
## Specific Ideas

- "I want all the layers with the same test organization" — uniform structure across C API, Julia, Dart, Lua phases
- Use existing `relations.sql` schema from `tests/schemas/valid/` — same schema as C++ and C API FK tests

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 06-julia-fk-tests*
*Context gathered: 2026-02-24*
