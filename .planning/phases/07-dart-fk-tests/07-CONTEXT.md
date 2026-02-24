# Phase 7: Dart FK Tests - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

16 FK resolution tests (9 create, 7 update) through Dart bindings, mirroring the C++ FK tests end-to-end. Test-only — no new binding methods or FK resolution code changes.

</domain>

<decisions>
## Implementation Decisions

### Test organization
- Same structure across all binding layers (Julia, Dart, Lua) — mirror C API phase 5 exactly
- 2 plans: 07-01 for create tests (9 tests), 07-02 for update tests (7 tests)
- Create tests appended to `bindings/dart/test/database_create_test.dart`
- Update tests appended to `bindings/dart/test/database_update_test.dart`
- Test names use Dart conventions: `group('FK Resolution - Create')` with `test('resolves set FK labels to IDs')`

### FK label passing
- Pass string labels through Dart Map API (e.g., `{'parent_id': 'Parent 1'}`) — same pattern as C API where string labels in FK integer columns trigger resolution
- Read back resolved integer IDs using typed readers (`readScalarIntegerById`, `readVectorIntegersById`, etc.) to verify resolution worked
- Use existing `relations.sql` schema from `tests/schemas/valid/` — same schema as C++ and C API FK tests

### Error assertions
- Use `throwsA(isA<DatabaseException>())` for error cases — matches existing Dart test patterns
- Verify zero partial writes on resolution failure (read back and confirm no rows were inserted)
- Update failure tests verify original values preserved by reading back after failed update

### Test lifecycle
- In-memory databases (`Database.fromSchema(':memory:', schemaPath)`) for all tests
- `try/finally` with `db.close()` for cleanup in each test — matches all existing Dart tests
- For update tests: create initial element with integer IDs, then update FK columns using string labels, read back to verify resolution

### Claude's Discretion
- Exact test group/test naming within Dart conventions
- Helper setup code structure within each test
- Assertion ordering within each test case

</decisions>

<specifics>
## Specific Ideas

- "I want all the layers with the same test organization" — uniform structure across C API, Julia, Dart, Lua phases
- Fully mirror Julia phase decisions — same 16 test cases, adapted to Dart API conventions (camelCase, Map<String, dynamic>, throwsA)
- Use existing `relations.sql` schema from `tests/schemas/valid/` — same schema as all other FK test phases

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 07-dart-fk-tests*
*Context gathered: 2026-02-24*
