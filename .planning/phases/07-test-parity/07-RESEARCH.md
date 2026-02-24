# Phase 7: Test Parity - Research

**Researched:** 2026-02-24
**Domain:** Cross-language test coverage audit and gap-fill
**Confidence:** HIGH

## Summary

Phase 7 is a pure test infrastructure phase -- no new features or API changes. The goal is to audit every functional area across all 6 layers (C++, C API, Julia, Dart, Python, Lua) and fill gaps so each layer has test coverage for every operation it exposes. The research below inventories every test file and identifies what operations are tested vs. missing in each layer.

The codebase already has substantial test coverage. C++ and Lua are the most comprehensive. Julia and Dart are well-covered but missing some edge cases. The C API has good breadth but notable metadata gaps. Python has structural coverage but is split into 3 read files instead of 1, and is missing dedicated relation and convenience helper tests. The work is mechanical: build a coverage matrix, identify gaps, write gap-fill tests.

**Primary recommendation:** Build the coverage matrix first (audit task), then fill gaps layer-by-layer in cascade order: C++ -> C API -> Julia -> Dart -> Python -> Lua. Each gap-fill test should be a single happy-path test added to the existing test file for that functional area.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- C++ is the reference layer -- all other layers must cover what C++ covers
- Audit C++ itself first against the public API surface, filling any gaps before using it as baseline
- Bidirectional: if any binding layer tests an operation C++ doesn't, add it to C++ too
- Cascade order: C++ -> C API -> Julia -> Dart -> Python -> Lua (each layer builds on the previous)
- New test schemas in `tests/schemas/` are allowed if needed for gap-fill
- Happy path only -- one test per missing operation confirming it works with valid inputs
- Do not replicate error path tests from other layers; existence of a happy-path test is sufficient
- Use same test data (schemas, element values) as existing tests when possible for cross-layer consistency
- Add gap-fill tests to existing test files for that functional area; only create new files for completely untested areas
- C API gets a full mirror of C++ test file structure: one test file per functional area
- Python is treated identically to Julia and Dart -- no special focus
- Lua is included in the parity audit alongside the other 5 layers
- Coverage matrix: rows = operations, columns = layers, cells = check/missing
- Matrix lives inside the PLAN.md files -- audit results feed directly into task breakdown
- Audit checks test existence only (is there a test for this operation?), not test quality
- Final validation step: all 6 test suites (C++, C API, Julia, Dart, Python, Lua) must run with zero failures

### Claude's Discretion
- Whether to compare test scenarios vs operations (user deferred this choice)
- Exact grouping of operations in the coverage matrix
- Which convenience methods are equivalent across layers for comparison purposes

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TEST-01 | Python test suite matching Julia/Dart test file structure (one file per functional area) | Python currently has 3 separate read files (scalar, vector, set) vs Julia/Dart having 1 read file. Needs consolidation or acceptance as-is since content is complete. Missing: dedicated relations test, convenience helper tests (read_all_*, read_*_group_by_id) |
| TEST-02 | Audit C++ test coverage and fill gaps where other layers have tests C++ lacks | C++ has comprehensive coverage. Main gaps: no dedicated metadata test file (metadata tests live in test_schema_validator.cpp which is fine), no `read_vector_strings` bulk test, no `read_set_integers`/`read_set_floats` bulk test, no `update_set_integers`/`update_set_floats` typed tests. Convenience methods N/A for C++ |
| TEST-03 | Audit C API test coverage and fill gaps | C API is missing: `get_vector_metadata`, `get_set_metadata`, `list_vector_groups`, `list_set_groups` tests, `describe` test. Has no dedicated metadata test file. Some read types only have null-argument tests but no happy-path tests (e.g., read_vector_strings, read_set_integers, read_set_floats bulk reads) |
| TEST-04 | Audit Julia test coverage and fill gaps | Julia is well-covered. Minor gaps: no `read_vector_strings` bulk test, no `read_set_integers`/`read_set_floats` tests (schema only has string sets). Convenience methods well-tested |
| TEST-05 | Audit Dart test coverage and fill gaps | Dart mirrors Julia closely. Similar minor gaps in typed variant coverage for less-common type combinations |
</phase_requirements>

## Current Test Landscape

### Test File Inventory

| Functional Area | C++ | C API | Julia | Dart | Python | Lua |
|-----------------|-----|-------|-------|------|--------|-----|
| Lifecycle | `test_database_lifecycle.cpp` | `test_c_api_database_lifecycle.cpp` | `test_database_lifecycle.jl` | `database_lifecycle_test.dart` | `test_database_lifecycle.py` | via `test_lua_runner.cpp` |
| Element | `test_element.cpp` | `test_c_api_element.cpp` | `test_element.jl` | `element_test.dart` | `test_element.py` | N/A (no standalone Element) |
| Create | `test_database_create.cpp` | `test_c_api_database_create.cpp` | `test_database_create.jl` | `database_create_test.dart` | `test_database_create.py` | via `test_lua_runner.cpp` |
| Read | `test_database_read.cpp` | `test_c_api_database_read.cpp` | `test_database_read.jl` | `database_read_test.dart` | 3 files: `test_database_read_scalar.py`, `test_database_read_vector.py`, `test_database_read_set.py` | via `test_lua_runner.cpp` |
| Update | `test_database_update.cpp` | `test_c_api_database_update.cpp` | `test_database_update.jl` | `database_update_test.dart` | `test_database_update.py` | via `test_lua_runner.cpp` |
| Delete | `test_database_delete.cpp` | `test_c_api_database_delete.cpp` | `test_database_delete.jl` | `database_delete_test.dart` | `test_database_delete.py` | via `test_lua_runner.cpp` |
| Query | `test_database_query.cpp` | `test_c_api_database_query.cpp` | `test_database_query.jl` | `database_query_test.dart` | `test_database_query.py` | via `test_lua_runner.cpp` |
| Time Series | `test_database_time_series.cpp` | `test_c_api_database_time_series.cpp` | `test_database_time_series.jl` | `database_time_series_test.dart` | `test_database_time_series.py` | via `test_lua_runner.cpp` |
| Transaction | `test_database_transaction.cpp` | `test_c_api_database_transaction.cpp` | `test_database_transaction.jl` | `database_transaction_test.dart` | `test_database_transaction.py` | via `test_lua_runner.cpp` |
| CSV | `test_database_csv.cpp` | `test_c_api_database_csv.cpp` | `test_database_csv.jl` | `database_csv_test.dart` | `test_database_csv.py` | via `test_lua_runner.cpp` |
| Metadata | `test_schema_validator.cpp` (co-located) | co-located in `test_c_api_database_read.cpp` + `test_c_api_database_time_series.cpp` | `test_database_metadata.jl` | `metadata_test.dart` | `test_database_metadata.py` | via `test_lua_runner.cpp` |
| Errors | `test_database_errors.cpp` | null-arg tests throughout | error cases in each file | error cases in each file | error cases in each file | error cases in runner |
| Schema Validator | `test_schema_validator.cpp` | N/A | `test_schema_validator.jl` | `schema_validation_test.dart` | N/A | N/A |
| Migrations | `test_migrations.cpp` + `test_database_lifecycle.cpp` | lifecycle tests | lifecycle tests | lifecycle tests | lifecycle tests | N/A |
| Issues | `test_issues.cpp` | N/A | N/A | `issues_test.dart` | N/A | N/A |
| LuaRunner | `test_lua_runner.cpp` | `test_c_api_lua_runner.cpp` | `test_lua_runner.jl` | `lua_runner_test.dart` | N/A | N/A |
| Row/Result | `test_row_result.cpp` | N/A | N/A | N/A | N/A | N/A |

### Test Counts by Layer

| Layer | Test Files | Approximate Test Count |
|-------|-----------|----------------------|
| C++ | 12 core files | ~280 tests |
| C API | 11 files | ~220 tests |
| Julia | 12 files | ~160 testsets |
| Dart | 14 files | ~160 tests |
| Python | 13 files | ~120 tests |
| Lua | 1 file (test_lua_runner.cpp) | ~110 tests |

### Schemas Used by Tests

| Schema | Location | Used By |
|--------|----------|---------|
| basic.sql | `tests/schemas/valid/` | Lifecycle, scalar read/write, query, metadata tests |
| collections.sql | `tests/schemas/valid/` | Vector, set, time series, convenience method tests |
| relations.sql | `tests/schemas/valid/` | FK, relation, create/update FK resolution tests |
| csv_export.sql | `tests/schemas/valid/` | CSV export/import tests |
| mixed_time_series.sql | `tests/schemas/valid/` | Multi-column time series tests |
| multi_time_series.sql | `tests/schemas/valid/` | Multi-group time series tests (shared dimension) |
| 8 invalid schemas | `tests/schemas/invalid/` | Schema validation tests |
| migrations 1-3 | `tests/schemas/migrations/` | Migration lifecycle tests |
| issue52, issue70 | `tests/schemas/issues/` | Regression tests |

## Architecture Patterns

### Recommended Audit Structure

The audit should compare operations at the granularity of "does a test exist that exercises this operation with valid inputs?" not "do we have the same number of assertions."

**Operation grouping for the coverage matrix:**

1. **Lifecycle**: from_schema, from_migrations, close, path, current_version, is_healthy, describe
2. **Create**: create_element (scalars), create with vectors, create with sets, create with time series, create with FK labels, create with multi-time-series
3. **Read Scalar**: read_scalar_integers/floats/strings (bulk), read_scalar_integer/float/string_by_id, read_element_ids, read empty
4. **Read Vector**: read_vector_integers/floats/strings (bulk), read_vector_integers/floats/strings_by_id, read empty
5. **Read Set**: read_set_integers/floats/strings (bulk), read_set_integers/floats/strings_by_id, read empty
6. **Update Scalar**: update_scalar_integer/float/string
7. **Update Vector**: update_vector_integers/floats/strings, update to empty, update from empty
8. **Update Set**: update_set_integers/floats/strings, update to empty, update from empty
9. **Update Element**: update_element (scalars), update_element with arrays, update_element with time series, update_element with FK labels
10. **Delete**: delete_element, delete with cascade, delete non-existent
11. **Query**: query_string/integer/float (simple), query with params (string/integer/float/null/multiple)
12. **Time Series**: read/update time series group, read/update multi-column, clear, ordering, time series files (has/list/read/update)
13. **Transaction**: begin/commit, begin/rollback, double-begin error, commit-without-begin, in_transaction, transaction block
14. **CSV**: export scalar/vector/set/time-series, enum labels, date formatting, import round-trip
15. **Metadata**: get_scalar_metadata, get_vector_metadata, get_set_metadata, get_time_series_metadata, list_scalar_attributes, list_vector_groups, list_set_groups, list_time_series_groups, FK metadata
16. **Convenience** (bindings only): read_all_scalars_by_id, read_all_vectors_by_id, read_all_sets_by_id, read_vector_group_by_id, read_set_group_by_id, datetime wrappers, transaction block

### Test Pattern: Happy Path Gap-Fill

Per user decision, gap-fill tests should follow this pattern:
```
1. Set up a database with an existing schema (reuse existing schemas when possible)
2. Populate test data (create elements as needed)
3. Call the operation under test
4. Assert the result matches expected output
```

No error-path tests needed for gap-fill. One test per missing operation.

### File Placement Rules

- Add gap-fill tests to the existing test file for that functional area
- Only create new files for completely untested areas
- New C API test files follow pattern: `test_c_api_database_{area}.cpp`
- Binding test files follow existing naming conventions per language

## Identified Gaps

### C++ Gaps (vs. Public API Surface)

The C++ layer is the reference. Gaps identified by comparing `database.h` public methods against test coverage:

| Operation | Status | Notes |
|-----------|--------|-------|
| `read_vector_strings` (bulk) | **MISSING** | No bulk string vector read test |
| `read_set_integers` (bulk) | **MISSING** | No bulk integer set read test |
| `read_set_floats` (bulk) | **MISSING** | No bulk float set read test |
| `read_set_strings_by_id` | Tested | In test_database_read.cpp |
| `read_set_integers_by_id` | **MISSING** | No by-id integer set read test |
| `read_set_floats_by_id` | **MISSING** | No by-id float set read test |
| `update_set_integers` | **MISSING** | No typed integer set update test |
| `update_set_floats` | **MISSING** | No typed float set update test |
| `update_vector_strings` | **MISSING** | No typed string vector update test |
| `read_vector_strings_by_id` | **MISSING** | No by-id string vector read test |
| `describe()` | **UNTESTED** | Called in some tests' setup but no dedicated test |
| `list_vector_groups` | **MISSING** | Tested in schema_validator but not via Database directly |
| `list_set_groups` | **MISSING** | Tested in schema_validator but not via Database directly |

Note: `read_scalar_relation` and `update_scalar_relation` do NOT exist in C++ or C API. They are Python-only convenience methods that reference non-existent C API symbols (`quiver_database_read_scalar_relation`, `quiver_database_update_scalar_relation`). These symbols are not exported from the DLL. This is a pre-existing issue outside Phase 7 scope.

### C API Gaps (vs. C++ + C API Header)

| Operation | Status | Notes |
|-----------|--------|-------|
| `quiver_database_get_vector_metadata` | **MISSING** | Declared in header, no test |
| `quiver_database_get_set_metadata` | **MISSING** | Declared in header, no test |
| `quiver_database_list_vector_groups` | **MISSING** | Declared in header, no test |
| `quiver_database_list_set_groups` | **MISSING** | Declared in header, no test |
| `quiver_database_describe` | **MISSING** | Declared in header, no test |
| `read_vector_strings` (bulk happy-path) | **MISSING** | Only null-arg tests exist |
| `read_set_integers` (bulk happy-path) | **MISSING** | Only null-arg tests exist |
| `read_set_floats` (bulk happy-path) | **MISSING** | Only null-arg tests exist |
| `read_set_integers_by_id` (happy-path) | **MISSING** | Only null-arg test exists |
| `read_set_floats_by_id` (happy-path) | **MISSING** | Only null-arg test exists |
| `read_vector_strings_by_id` (happy-path) | **MISSING** | Only null-arg test exists |
| `update_vector_strings` (happy-path) | **MISSING** | Only null-arg tests exist |
| `update_set_integers` (happy-path) | **MISSING** | Only null-arg tests exist |
| `update_set_floats` (happy-path) | **MISSING** | Only null-arg tests exist |
| Dedicated metadata test file | **MISSING** | Metadata tests scattered across read and time_series files |

### Julia Gaps

| Operation | Status | Notes |
|-----------|--------|-------|
| `read_vector_strings` (bulk) | **MISSING** | No string vector bulk read (collections.sql has no string vectors) |
| `read_set_integers` (bulk) | **MISSING** | No integer set bulk read (collections.sql has string sets only) |
| `read_set_floats` (bulk) | **MISSING** | No float set bulk read |
| `update_vector_strings` | **MISSING** | No string vector update test |
| `update_set_integers` | **MISSING** | No integer set update test |
| `update_set_floats` | **MISSING** | No float set update test |
| `read_set_integers_by_id` | **MISSING** | No by-id integer set read |
| `read_set_floats_by_id` | **MISSING** | No by-id float set read |
| `read_vector_strings_by_id` | **MISSING** | No by-id string vector read |
| `describe` | **MISSING** | No test for describe() |
| `import_csv` | Tested | In test_database_csv.jl |
| `read_set_group_by_id` | **MISSING** | Convenience method not tested |

### Dart Gaps

| Operation | Status | Notes |
|-----------|--------|-------|
| `readVectorStrings` (bulk) | **MISSING** | No string vector bulk read |
| `readSetIntegers` (bulk) | **MISSING** | No integer set bulk read |
| `readSetFloats` (bulk) | **MISSING** | No float set bulk read |
| `updateVectorStrings` | **MISSING** | No string vector update |
| `updateSetIntegers` | **MISSING** | No integer set update |
| `updateSetFloats` | **MISSING** | No float set update |
| `readSetIntegersById` | **MISSING** | No by-id integer set read |
| `readSetFloatsById` | **MISSING** | No by-id float set read |
| `readVectorStringsById` | **MISSING** | No by-id string vector read |
| `describe` | **MISSING** | No test for describe() |
| `readSetGroupById` | **MISSING** | Convenience method not tested |

### Python Gaps

| Operation | Status | Notes |
|-----------|--------|-------|
| Read file structure | **3 files instead of 1** | `test_database_read_scalar.py`, `test_database_read_vector.py`, `test_database_read_set.py` -- acceptable per user decision to add to existing files |
| `read_vector_strings` (bulk) | **MISSING** | No string vector bulk read |
| `read_vector_strings_by_id` | **MISSING** | No by-id string vector read |
| `read_set_integers` (bulk) | **MISSING** | No integer set bulk read |
| `read_set_floats` (bulk) | **MISSING** | No float set bulk read |
| `read_set_integers_by_id` | **MISSING** | No by-id integer set read |
| `read_set_floats_by_id` | **MISSING** | No by-id float set read |
| `update_vector_strings` | **MISSING** | No string vector update |
| `update_set_integers` | **MISSING** | No integer set update |
| `update_set_floats` | **MISSING** | No float set update |
| `update_element` with time series | **MISSING** | update_element test exists but not with time series |
| `update_element` with FK labels | **MISSING** | No FK-aware update_element tests |
| `create_element` with time series | **MISSING** | No time series in create tests |
| `create_element` with FK labels | **MISSING** | No FK-aware create tests |
| `describe` | Tested | In lifecycle test |
| `import_csv` | Tested | Raises NotImplementedError |
| `read_all_vectors_by_id` | Partially | Tests empty case only |
| `read_all_sets_by_id` | Partially | Tests empty case only |
| `read_scalar_date_time_by_id` | Tested | In read_scalar tests |
| `read_vector_date_time_by_id` | **MISSING** | DateTime vector convenience not tested |
| `read_set_date_time_by_id` | **MISSING** | DateTime set convenience not tested |
| `query_date_time` | Tested | In query tests |
| `update_scalar_relation` | Tested | In read_scalar tests (used for setup) |
| `read_scalar_relation` | Tested | In read_scalar tests |

### Lua Gaps

Lua tests are comprehensive for its API surface (all in `test_lua_runner.cpp`). Key gaps:

| Operation | Status | Notes |
|-----------|--------|-------|
| `read_vector_strings` (bulk) | **MISSING** | No string vector read |
| `read_set_integers` | **MISSING** | No integer set read |
| `read_set_floats` | **MISSING** | No float set read |
| `update_set_integers` | **MISSING** | No integer set update |
| `update_set_floats` | **MISSING** | No float set update |
| `read_set_integers_by_id` | **MISSING** | No by-id integer set |
| `read_set_floats_by_id` | **MISSING** | No by-id float set |
| `read_vector_strings_by_id` | **MISSING** | No by-id string vector |
| `update_vector_strings` | Tested | `UpdateVectorStringsFromLua` exists |
| `describe` | **MISSING** | No describe test from Lua |
| `export_csv` | Tested | Multiple CSV export tests |
| `import_csv` | Tested | Multiple CSV import tests |

## Schema Considerations

Many of the missing tests involve typed operations on types not present in the existing schemas:
- **String vectors**: No schema has a string-typed vector column
- **Integer/float sets**: `collections.sql` only has string sets; `relations.sql` has integer FK sets
- **Float sets**: No schema has float sets

**Options:**
1. Use existing `relations.sql` which has `Child_set_scores` (integer set) and `Child_set_mentors`/`Child_set_parents` (integer FK sets)
2. A new schema may be needed for float sets and string vectors, OR add columns to an existing schema

Since the user allows new schemas in `tests/schemas/` for gap-fill, the planner should decide whether to extend `collections.sql` or create a new `all_types.sql` schema that includes string vectors, integer sets, and float sets.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Test data setup | Custom per-test DB factories | Reuse existing schema fixtures and conftest/fixture patterns | Consistency across layers, no schema drift |
| Coverage tracking | Manual checklist | Coverage matrix in PLAN.md | User decision: matrix lives in plan files |
| Cross-language comparison | Diff tool | Manual audit by functional area | Test structures differ too much for automated comparison |

**Key insight:** The gap-fill work is entirely mechanical. Every gap is a known operation + known type combination that needs a single happy-path test. No architectural decisions required.

## Common Pitfalls

### Pitfall 1: Schema Missing Required Types
**What goes wrong:** Writing a gap-fill test for `read_set_integers` but no existing schema has an integer set table.
**Why it happens:** Current schemas were designed for specific original tests, not full type coverage.
**How to avoid:** Verify the target schema has the required column type before writing the test. Use `relations.sql` for integer sets (score column). Create a new schema for float sets and string vectors if needed.
**Warning signs:** Test compiles but throws "attribute not found" at runtime.

### Pitfall 2: Python Read File Split
**What goes wrong:** Planner creates a task to "merge 3 Python read files into 1" breaking existing test organization.
**Why it happens:** Julia/Dart have 1 read file; Python has 3.
**How to avoid:** The user's decision says "add gap-fill tests to existing test files." Python's 3-file split is acceptable. Add new tests to the appropriate existing file (e.g., vector tests go to `test_database_read_vector.py`).
**Warning signs:** Task to restructure/merge Python test files.

### Pitfall 3: Testing Non-Existent API Functions
**What goes wrong:** Writing a Python test for `read_scalar_relation` that calls a C API function that doesn't exist in the DLL.
**Why it happens:** The Python cdef declares `quiver_database_read_scalar_relation` and `quiver_database_update_scalar_relation`, but these are NOT exported from the C/C++ libraries.
**How to avoid:** Treat these as Python-only convenience methods. They exist in the Python binding but their C API symbols are phantom declarations. Do not try to add these to C++/C API as gap-fill.
**Warning signs:** Link errors or ABI load failures.

### Pitfall 4: Inconsistent Test Data Across Layers
**What goes wrong:** Gap-fill tests in different layers use different schemas or element values, making cross-layer comparison harder.
**Why it happens:** Each layer's tests evolved independently.
**How to avoid:** For gap-fill tests, use the same schema and similar test data as the closest existing test in the same file.
**Warning signs:** Different schemas referenced in similar tests across layers.

### Pitfall 5: Creating Tests for Binding-Only Convenience Methods in C++
**What goes wrong:** Creating C++ tests for `read_all_scalars_by_id` or `transaction()` block which don't exist in C++.
**Why it happens:** The audit includes convenience methods that only exist in bindings.
**How to avoid:** The coverage matrix must distinguish core API operations (present in C++) from convenience methods (binding-only). Convenience methods are only audited across the binding layers that implement them.
**Warning signs:** Compilation errors trying to call non-existent C++ methods.

## Code Examples

### C++ Gap-Fill Test Pattern (googletest)
```cpp
TEST(Database, ReadSetIntegersByIdBasic) {
    auto db = Database::from_schema(":memory:", SCHEMA_PATH("relations.sql"));
    // Create parent elements
    db.create_element("Parent", Element().set("label", "Parent 1"));
    db.create_element("Parent", Element().set("label", "Parent 2"));
    // Create child with integer set (scores)
    db.create_element("Child", Element()
        .set("label", "Child 1")
        .set("score", std::vector<int64_t>{10, 20, 30}));

    auto result = db.read_set_integers_by_id("Child", "score", 1);
    ASSERT_EQ(result.size(), 3);
}
```

### C API Gap-Fill Test Pattern
```cpp
TEST(DatabaseCApi, GetVectorMetadata) {
    auto db = open_test_db("collections.sql");
    quiver_group_metadata_t metadata;
    auto err = quiver_database_get_vector_metadata(db, "Collection", "values", &metadata);
    ASSERT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(metadata.group_name, "values");
    EXPECT_EQ(metadata.value_column_count, 2);
    quiver_database_free_group_metadata(&metadata);
    quiver_database_close(db);
}
```

### Julia Gap-Fill Test Pattern
```julia
@testset "Read Vector Strings by ID" begin
    db = Quiver.from_schema(":memory:", schema_path("collections.sql"))
    # ... setup and test
    Quiver.close(db)
end
```

### Python Gap-Fill Test Pattern
```python
def test_read_set_integers_by_id(self, relations_db: Database) -> None:
    relations_db.create_element("Parent", Element().set("label", "P1"))
    relations_db.create_element("Child", Element()
        .set("label", "C1")
        .set("score", [10, 20]))
    result = relations_db.read_set_integers_by_id("Child", "score", 1)
    assert sorted(result) == [10, 20]
```

### Dart Gap-Fill Test Pattern
```dart
test('reads integer set by ID', () {
  final db = Database.fromSchema(':memory:', schemaPath('relations.sql'));
  // ... setup and test
  db.close();
});
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| C API had only CSV tests | Full C API test file structure | Phase 7 (this phase) | C API now mirrors C++ test layout |
| Python tests ad-hoc | Python tests match Julia/Dart structure | Phase 7 (this phase) | Consistent coverage |
| No cross-layer audit | Coverage matrix ensures parity | Phase 7 (this phase) | Gaps systematically identified |

## Open Questions

1. **Python read file split: merge or keep?**
   - What we know: Python has 3 read files (scalar, vector, set); Julia/Dart have 1 each
   - What's unclear: Whether "one file per functional area" means Python should merge these
   - Recommendation: Keep the 3-file split. The user said "add to existing test files." The 3 files ARE the existing structure. TEST-01 says "one test file per functional area" but read-scalar, read-vector, read-set are each a functional area. Confirm with user if ambiguous.

2. **Schema for missing typed operations**
   - What we know: No schema has string vectors or float sets. `relations.sql` has integer sets.
   - What's unclear: Whether to create a new comprehensive schema or extend an existing one
   - Recommendation: Create a single new `tests/schemas/valid/all_types.sql` schema that includes string vectors, integer/float sets to avoid polluting existing schemas. Alternatively, note that some of these type combinations (float set) may simply not be worth testing if no real schema uses them -- the C++ code paths are type-generic.

3. **read_scalar_relation / update_scalar_relation phantom functions**
   - What we know: Python declares and calls C API functions that don't exist in the DLL
   - What's unclear: How existing Python tests pass (likely CFFI ABI mode defers symbol resolution until call)
   - Recommendation: Out of scope for Phase 7. The tests may fail when actually called. Flag as a known issue.

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis: all test files read and test names extracted
- `include/quiver/database.h` -- C++ public API surface (199 lines)
- `include/quiver/c/database.h` -- C API header (478 lines)
- Test schema files in `tests/schemas/valid/`
- `CLAUDE.md` -- project conventions and architecture

### Secondary (MEDIUM confidence)
- Python `_c_api.py` CFFI declarations compared against actual C API exports
- Cross-referencing test patterns between layers to identify equivalence

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no external dependencies, pure test code using existing frameworks (googletest, Julia Test, dart test, pytest)
- Architecture: HIGH -- test patterns well-established across all layers, gap-fill is mechanical
- Pitfalls: HIGH -- identified from actual codebase analysis, not hypothetical

**Research date:** 2026-02-24
**Valid until:** No expiry -- codebase-specific research, valid until API surface changes
