# Phase 7: Dart FK Tests - Research

**Researched:** 2026-02-24
**Domain:** Dart FFI binding tests for FK label-to-ID resolution
**Confidence:** HIGH

## Summary

Phase 7 adds 16 FK resolution tests to the Dart bindings (9 create + 7 update), mirroring the same tests already implemented in C++ (Phase 2-3), C API (Phase 5), and Julia (Phase 6). This is a test-only phase -- no new binding code or FK resolution logic is needed. The Dart `createElement` and `updateElement` methods already accept `Map<String, Object?>` values, and strings in FK integer columns trigger the C API's FK resolution automatically.

The research confirms all needed Dart API methods exist and work correctly. The `Element.set()` method handles `String` values (routed to `quiver_element_set_string`) and `List<String>` values (routed to `quiver_element_set_array_string`), which is exactly how FK labels are passed. The test infrastructure (in-memory databases, `try/finally` with `db.close()`, `throwsA(isA<DatabaseException>())`) is well-established across 14 existing Dart test files.

**Primary recommendation:** Directly mirror the Julia Phase 6 tests, translating Julia kwargs to Dart Map literals and Julia `@test_throws` to Dart `throwsA(isA<DatabaseException>())`. Every test is self-contained with its own in-memory database.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Same structure across all binding layers (Julia, Dart, Lua) -- mirror C API phase 5 exactly
- 2 plans: 07-01 for create tests (9 tests), 07-02 for update tests (7 tests)
- Create tests appended to `bindings/dart/test/database_create_test.dart`
- Update tests appended to `bindings/dart/test/database_update_test.dart`
- Test names use Dart conventions: `group('FK Resolution - Create')` with `test('resolves set FK labels to IDs')`
- Pass string labels through Dart Map API (e.g., `{'parent_id': 'Parent 1'}`) -- same pattern as C API where string labels in FK integer columns trigger resolution
- Read back resolved integer IDs using typed readers (`readScalarIntegerById`, `readVectorIntegersById`, etc.) to verify resolution worked
- Use existing `relations.sql` schema from `tests/schemas/valid/` -- same schema as C++ and C API FK tests
- Use `throwsA(isA<DatabaseException>())` for error cases -- matches existing Dart test patterns
- Verify zero partial writes on resolution failure (read back and confirm no rows were inserted)
- Update failure tests verify original values preserved by reading back after failed update
- In-memory databases (`Database.fromSchema(':memory:', schemaPath)`) for all tests
- `try/finally` with `db.close()` for cleanup in each test -- matches all existing Dart tests
- For update tests: create initial element with integer IDs, then update FK columns using string labels, read back to verify resolution

### Claude's Discretion
- Exact test group/test naming within Dart conventions
- Helper setup code structure within each test
- Assertion ordering within each test case

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DART-01 | Dart FK resolution tests for createElement cover all 9 test cases mirroring C++ create path | All 9 test cases mapped below with exact Dart API calls; `createElement` accepts `Map<String, Object?>` which routes strings through `Element.setString()` -> `quiver_element_set_string` triggering FK resolution |
| DART-02 | Dart FK resolution tests for updateElement cover all 7 test cases mirroring C++ update path | All 7 test cases mapped below with exact Dart API calls; `updateElement` uses same `Map<String, Object?>` -> Element builder pattern; all read-back methods verified to exist |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `package:test` | (project dep) | Dart test framework | Already used by all 14 existing Dart test files |
| `package:path` | (project dep) | Cross-platform path construction | Already used by all existing Dart test files |
| `package:quiver_db` | (project lib) | The Dart binding under test | Project's own library |

### Supporting
No additional libraries needed. This is pure test code using existing dependencies.

## Architecture Patterns

### Dart Test File Structure
```
bindings/dart/test/
  database_create_test.dart   # Existing create tests + FK create tests (Plan 07-01)
  database_update_test.dart   # Existing update tests + FK update tests (Plan 07-02)
```

### Pattern 1: Dart FK Create Test
**What:** Test that passes string labels in Map values and reads back resolved integer IDs
**When to use:** Every FK create test
**Example:**
```dart
// Source: Existing pattern from database_create_test.dart
group('FK Resolution - Create', () {
  test('resolves set FK labels to IDs', () {
    final db = Database.fromSchema(
      ':memory:',
      path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
    );
    try {
      db.createElement('Configuration', {'label': 'Test Config'});
      db.createElement('Parent', {'label': 'Parent 1'});
      db.createElement('Parent', {'label': 'Parent 2'});

      // Pass string labels for FK integer column
      db.createElement('Child', {
        'label': 'Child 1',
        'mentor_id': ['Parent 1', 'Parent 2'],
      });

      // Read back resolved integer IDs
      final result = db.readSetIntegersById('Child', 'mentor_id', 1);
      expect(result..sort(), equals([1, 2]));
    } finally {
      db.close();
    }
  });
});
```

### Pattern 2: Dart FK Update Test
**What:** Create element with initial FK values, update with string labels, verify resolution
**When to use:** Every FK update test
**Example:**
```dart
test('resolves scalar FK labels to IDs', () {
  final db = Database.fromSchema(
    ':memory:',
    path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
  );
  try {
    db.createElement('Configuration', {'label': 'Test Config'});
    db.createElement('Parent', {'label': 'Parent 1'});
    db.createElement('Parent', {'label': 'Parent 2'});
    db.createElement('Child', {'label': 'Child 1', 'parent_id': 'Parent 1'});

    // Update with string label
    db.updateElement('Child', 1, {'parent_id': 'Parent 2'});

    // Verify resolved to integer ID
    final parentId = db.readScalarIntegerById('Child', 'parent_id', 1);
    expect(parentId, equals(2));
  } finally {
    db.close();
  }
});
```

### Pattern 3: Dart Error Assertion
**What:** Verify exception thrown and no side effects
**When to use:** Error and failure tests
**Example:**
```dart
test('throws on missing FK target label', () {
  final db = Database.fromSchema(
    ':memory:',
    path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
  );
  try {
    db.createElement('Configuration', {'label': 'Test Config'});

    expect(
      () => db.createElement('Child', {
        'label': 'Child 1',
        'mentor_id': ['Nonexistent Parent'],
      }),
      throwsA(isA<DatabaseException>()),
    );
  } finally {
    db.close();
  }
});
```

### Pattern 4: Dart Time Series FK in createElement
**What:** Pass time series columns through the Map for createElement. Time series columns (dimension + value columns) are passed as string arrays in the top-level map.
**When to use:** Time series FK create and update tests
**Example:**
```dart
// Create with time series FK
db.createElement('Child', {
  'label': 'Child 1',
  'date_time': ['2024-01-01', '2024-01-02'],
  'sponsor_id': ['Parent 1', 'Parent 2'],
});

// Read back via readTimeSeriesGroup
final result = db.readTimeSeriesGroup('Child', 'events', 1);
expect(result['sponsor_id'], equals([1, 2]));
```

**IMPORTANT:** The Dart `readTimeSeriesGroup` returns `Map<String, List<Object>>`. The dimension column (`date_time`) is automatically parsed to `List<DateTime>` objects, while INTEGER columns like `sponsor_id` are returned as `List<int>`. This differs from Julia which returns raw values.

### Anti-Patterns to Avoid
- **Inspecting error message content:** Per locked decisions, use `throwsA(isA<DatabaseException>())` only -- do not match on message substrings.
- **Shared database across tests:** Each test creates its own `:memory:` database for isolation.
- **Forgetting `try/finally`:** Every test must wrap db usage in `try/finally` with `db.close()`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FK resolution | Custom label lookup | Pass string labels in Map -- C API does resolution | Resolution logic lives in C++ layer; bindings just pass strings through |
| Test assertions | Custom matchers | `throwsA(isA<DatabaseException>())` | Standard Dart test pattern, matches existing tests |
| Path construction | String concatenation | `path.join(testsPath, 'schemas', 'valid', ...)` | Cross-platform, matches all existing tests |

## Common Pitfalls

### Pitfall 1: Set ordering
**What goes wrong:** Set read results may not be in insertion order
**Why it happens:** SQLite sets have no guaranteed ordering
**How to avoid:** Sort before comparing: `expect(result..sort(), equals([1, 2]))` for sets. Vectors preserve order so don't sort those.
**Warning signs:** Tests pass intermittently

### Pitfall 2: Time series group name vs table name
**What goes wrong:** Using wrong group name when reading time series
**Why it happens:** The table is `Child_time_series_events` but the group name is `events` (the suffix after `_time_series_`)
**How to avoid:** Always use `'events'` as the group name, not the full table name
**Warning signs:** "not found" errors from readTimeSeriesGroup

### Pitfall 3: Time series dimension column returned as DateTime
**What goes wrong:** Comparing DateTime objects with string expectations
**Why it happens:** Dart `readTimeSeriesGroup` parses the dimension column to `DateTime` objects automatically
**How to avoid:** When checking time series results, only check non-dimension columns (like `sponsor_id`) for integer resolution. If checking dimension column, compare with `DateTime` objects.
**Warning signs:** Type mismatch assertion failures

### Pitfall 4: Dart List type inference
**What goes wrong:** Dart infers `List<Object>` instead of `List<String>` for mixed lists
**Why it happens:** `Element.set()` uses runtime type checking with `switch` on value types
**How to avoid:** Explicitly type list literals when needed: `['Parent 1', 'Parent 2']` (Dart infers `List<String>` for string literals, so this is usually not an issue)
**Warning signs:** "Unsupported type" ArgumentError

### Pitfall 5: Integer values in update setup
**What goes wrong:** Creating initial elements with string FK labels instead of integer IDs for update tests
**Why it happens:** Per CONTEXT.md: "For update tests: create initial element with integer IDs, then update FK columns using string labels"
**How to avoid:** Use string labels for initial create too (since FK resolution already works), or use integer IDs directly -- either works. Julia tests use string labels for initial create, which is fine since FK resolution is already proven. The key point is that the UPDATE call uses string labels.
**Warning signs:** N/A -- both approaches work

## Code Examples

### Complete Create Test Group (all 9 tests)

The 9 create tests mirror Julia/C++ exactly, translated to Dart conventions:

| # | Test Name | Schema | What It Tests |
|---|-----------|--------|---------------|
| 1 | resolves set FK labels to IDs | relations.sql | `mentor_id: ['Parent 1', 'Parent 2']` -> `readSetIntegersById` returns [1, 2] |
| 2 | throws on missing FK target label | relations.sql | `mentor_id: ['Nonexistent Parent']` -> `throwsA(isA<DatabaseException>())` |
| 3 | throws on string for non-FK integer column | relations.sql | `score: ['not_a_label']` -> `throwsA(isA<DatabaseException>())` |
| 4 | resolves scalar FK labels to IDs | relations.sql | `parent_id: 'Parent 1'` -> `readScalarIntegers` returns [1] |
| 5 | resolves vector FK labels to IDs | relations.sql | `parent_ref: ['Parent 1', 'Parent 2']` -> `readVectorIntegersById` returns [1, 2] |
| 6 | resolves time series FK labels to IDs | relations.sql | `sponsor_id: ['Parent 1', 'Parent 2']` -> `readTimeSeriesGroup['sponsor_id']` returns [1, 2] |
| 7 | resolves all FK types in one call | relations.sql | scalar + set + vector + time series FK labels in single createElement |
| 8 | non-FK integer columns pass through unchanged | basic.sql | No FK columns, values pass through unmodified |
| 9 | zero partial writes on resolution failure | relations.sql | Failed FK resolution -> no child row created |

### Complete Update Test Group (all 7 tests)

| # | Test Name | Schema | What It Tests |
|---|-----------|--------|---------------|
| 1 | resolves scalar FK labels to IDs | relations.sql | Update `parent_id: 'Parent 2'` -> readback shows 2 |
| 2 | resolves vector FK labels to IDs | relations.sql | Update `parent_ref: ['Parent 2', 'Parent 1']` -> readback shows [2, 1] |
| 3 | resolves set FK labels to IDs | relations.sql | Update `mentor_id: ['Parent 2']` -> readback shows [2] |
| 4 | resolves time series FK labels to IDs | relations.sql | Update `sponsor_id: ['Parent 2', 'Parent 1']` -> readback shows [2, 1] |
| 5 | resolves all FK types in one call | relations.sql | Update all FK types to Parent 2 in single updateElement |
| 6 | non-FK integer columns pass through unchanged | basic.sql | Update non-FK scalars, values update correctly |
| 7 | failure preserves existing data | relations.sql | Failed FK update -> original values intact |

### Key Dart API Methods Used

**Create path:**
- `db.createElement('Child', {'parent_id': 'Parent 1', ...})` -- Map with string labels for FK columns

**Update path:**
- `db.updateElement('Child', 1, {'parent_id': 'Parent 2'})` -- Map with string labels for FK columns

**Read-back verification (all confirmed to exist in Dart bindings):**
- `db.readScalarIntegers('Child', 'parent_id')` -> `List<int>`
- `db.readScalarIntegerById('Child', 'parent_id', 1)` -> `int?`
- `db.readVectorIntegersById('Child', 'parent_ref', 1)` -> `List<int>`
- `db.readSetIntegersById('Child', 'mentor_id', 1)` -> `List<int>`
- `db.readTimeSeriesGroup('Child', 'events', 1)` -> `Map<String, List<Object>>` (sponsor_id returns `List<int>`)
- `db.readScalarStrings('Child', 'label')` -> `List<String>` (for zero-writes check)
- `db.readScalarFloats('Configuration', 'float_attribute')` -> `List<double>` (for no-FK check)
- `db.readScalarFloatById(...)`, `db.readScalarStringById(...)` (for no-FK update check)

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

## Open Questions

None. All API methods, test patterns, and schemas are fully understood. The Dart tests are a mechanical translation of the Julia tests already completed in Phase 6.

## Sources

### Primary (HIGH confidence)
- `bindings/dart/test/database_create_test.dart` -- existing Dart create test patterns (480 lines, 13 test groups)
- `bindings/dart/test/database_update_test.dart` -- existing Dart update test patterns (972 lines, 15+ test groups)
- `bindings/dart/lib/src/element.dart` -- Element.set() type dispatch confirming String and List<String> handling
- `bindings/dart/lib/src/database_create.dart` -- createElement uses Map -> Element builder -> C API
- `bindings/dart/lib/src/database_update.dart` -- updateElement uses Map -> Element builder -> C API
- `bindings/dart/lib/src/database_read.dart` -- all read-back methods confirmed
- `bindings/dart/lib/src/exceptions.dart` -- DatabaseException class definition
- `bindings/julia/test/test_database_create.jl` -- Julia FK create tests (reference implementation, lines 347-526)
- `bindings/julia/test/test_database_update.jl` -- Julia FK update tests (reference implementation, lines 688-872)
- `tests/schemas/valid/relations.sql` -- FK test schema
- `tests/schemas/valid/basic.sql` -- no-FK test schema
- `.planning/phases/06-julia-fk-tests/06-01-PLAN.md` -- Phase 6 plan 1 (Julia FK create, reference)
- `.planning/phases/06-julia-fk-tests/06-02-PLAN.md` -- Phase 6 plan 2 (Julia FK update, reference)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- exact same test framework and patterns already in use
- Architecture: HIGH -- direct mechanical translation from Julia tests, all API methods verified
- Pitfalls: HIGH -- based on actual codebase inspection, not training data

**Research date:** 2026-02-24
**Valid until:** indefinite (test-only phase, no external dependencies)
