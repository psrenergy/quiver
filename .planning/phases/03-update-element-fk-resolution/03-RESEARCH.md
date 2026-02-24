# Phase 3: Update Element FK Resolution - Research

**Researched:** 2026-02-23
**Domain:** C++ internal extension -- wire existing pre-resolve FK pass into `update_element` for scalar, vector, set, and time series FK label resolution
**Confidence:** HIGH

## Summary

Phase 3 adds FK label resolution to the `update_element` method so that users can pass target labels (strings) for FK columns in an update Element and have them automatically resolve to target IDs. All infrastructure already exists from Phase 2: the `resolve_element_fk_labels` method on `Database::Impl`, the `ResolvedElement` struct, and the `resolve_fk_label` per-value helper. This phase is purely a wiring task.

The current `update_element` in `src/database_update.cpp` reads from `element.scalars()` and `element.arrays()` directly. The change is: (1) add a single call to `impl_->resolve_element_fk_labels(collection, element, *this)` at the top of the method (before the transaction), (2) switch all downstream code to read from `resolved.scalars` and `resolved.arrays` instead of the raw Element. This mirrors exactly what Phase 2 did for `create_element` in `src/database_create.cpp`.

Tests follow the Phase 2 test structure exactly: per-type FK tests (scalar, vector, set, time series), combined all-types test, no-FK regression test, and error tests. All tests go in `tests/test_database_relations.cpp` in a new section. The existing `relations.sql` schema has all needed FK columns (scalar `parent_id`, vector `parent_ref`, set `mentor_id`, time series `sponsor_id`).

**Primary recommendation:** Add one call to `resolve_element_fk_labels` at the top of `update_element` (before the transaction guard), then use resolved values for all downstream SQL. Mirror Phase 2's test structure for update_element FK resolution tests.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Same fail-fast** -- stop at the first unresolvable FK label and throw immediately, identical to create_element
- **Same bare error format** -- `"Failed to resolve label 'X' to ID in table 'Y'"` with no method-specific context (matches ERR-01, same as create)
- **Same non-FK INTEGER guard** -- string passed to a non-FK INTEGER column in update_element gets the same clear Quiver error, not raw SQLite STRICT mode error
- **Partial-update scope** -- only resolve FK columns present in the Element being passed; columns not in the update are untouched
- **Same function, both paths** -- update_element calls the exact same pre-resolve function that create_element uses. One function to maintain, guaranteed consistency
- **Same ResolvedElement type** -- pre-resolve output is a ResolvedElement, same as create
- **Full schema scan** -- pre-resolve scans all collection tables (same as create), even though update is partial. Cost is negligible and keeps the code simple
- **Minimal wiring** -- implementation is: (1) add one call to pre_resolve at the top of update_element, (2) use resolved values downstream. Keep changes small
- **Same relations.sql schema** -- reuse the extended schema from Phase 2, test FK columns via update_element
- **Mirror Phase 2's test structure exactly** -- per-type FK tests (scalar, vector, set, time series), combined all-types test, no-FK regression test, and error tests
- **Tests in existing file** -- add to test_database_update.cpp, not a separate file

### Claude's Discretion
- Verification depth in tests (read-back resolved IDs vs no-throw sufficient)
- Any minor refactoring needed to consume ResolvedElement in update_element's downstream code

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UPD-01 | User can pass a target label for a scalar FK column in `update_element` and it resolves to the target's ID | Pre-resolve pass calls `resolve_fk_label` for each scalar in the Element against the collection table's FK metadata. Identical to CRE-01 but via `update_element`. Only one line of wiring needed (call `resolve_element_fk_labels`). See Architecture Pattern 1. |
| UPD-02 | User can pass target labels for vector FK columns in `update_element` and they resolve to target IDs | Pre-resolve pass resolves vector array values against group table FK metadata. The routing via `find_all_tables_for_column` already exists in both `update_element` and `resolve_element_fk_labels`. See Architecture Pattern 2. |
| UPD-03 | User can pass target labels for set FK columns in `update_element` and they resolve to target IDs | Pre-resolve pass resolves set array values. Set FK resolution was unified into the pre-resolve pass in Phase 2, so update_element gets it for free. See Architecture Pattern 2. |
| UPD-04 | User can pass target labels for time series FK columns in `update_element` and they resolve to target IDs | Pre-resolve pass resolves time series array values against `Child_time_series_events` FK metadata. Same mechanism as vectors/sets. See Architecture Pattern 2. |
</phase_requirements>

## Standard Stack

### Core

No new libraries or dependencies. Phase 3 uses only existing infrastructure:

| Component | Location | Purpose | Status |
|-----------|----------|---------|--------|
| `resolve_element_fk_labels` | `src/database_impl.h:90-132` | Unified pre-resolve pass for all FK types (from Phase 2) | Exists, proven, no changes needed |
| `ResolvedElement` | `src/database_impl.h:18-21` | Struct holding resolved scalars and arrays maps | Exists, no changes needed |
| `resolve_fk_label` | `src/database_impl.h:55-88` | Per-value FK label resolution (from Phase 1) | Exists, no changes needed |
| `TypeValidator` | `src/type_validator.cpp` | Type checking for resolved values | Exists, no changes needed |
| `Database::Impl::TransactionGuard` | `src/database_impl.h:185-213` | Nest-aware RAII transaction | Exists, no changes needed |

### Alternatives Considered

None. The user decision locks "same function, both paths" -- there is no alternative to explore. The infrastructure is proven from Phase 2.

## Architecture Patterns

### Current `update_element` Flow

The current flow in `src/database_update.cpp` lines 7-163:

```
update_element(collection, id, element)
  1. Validate collection exists (line 9)
  2. Check element has at least one attribute (lines 11-16)
  3. Begin transaction (line 18)
  4. If scalars present: validate types, build UPDATE SQL, execute (lines 21-44)
  5. Route arrays to vector/set/time_series table maps (lines 47-72)
  6. Update vector tables: DELETE + INSERT per row (lines 75-101)
  7. Update set tables: DELETE + INSERT per row (lines 105-131)
  8. Update time series tables: DELETE + INSERT per row (lines 134-160)
  9. Commit transaction (line 162)
```

### Target Flow After Phase 3

```
update_element(collection, id, element)
  0. PRE-RESOLVE PASS (new) -- resolve all FK labels in element
  1. Validate collection exists (unchanged)
  2. Check element has at least one attribute (unchanged, uses element not resolved)
  3. If resolved scalars present: validate types, build UPDATE SQL, execute
  4. Route resolved arrays to vector/set/time_series table maps
  5-8. Same as before, but reading from resolved values
  9. Commit transaction
```

### Pattern 1: Wiring Pre-Resolve into update_element

**What:** Add a single call to the existing `resolve_element_fk_labels` before the transaction, then switch all downstream code to use resolved values.

**Where the call goes:** After `require_collection` but before the transaction guard, mirroring `create_element`:

```cpp
void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update_element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    Impl::TransactionGuard txn(*impl_);

    // All downstream code uses resolved.scalars and resolved.arrays
    // instead of element.scalars() and element.arrays()
    ...
}
```

**Key insight:** The emptiness check (line 14) must still use the raw Element, because it checks whether the user provided any attributes at all. The pre-resolve pass always produces a ResolvedElement with the same keys as the input Element, so checking resolved.scalars.empty() would work too, but checking the raw Element is clearer (the check is about user intent, not resolved values).

### Pattern 2: Switching Downstream Code to Resolved Values

**Scalar update section:** Currently reads from `element.scalars()`. After wiring, reads from `resolved.scalars`:

```cpp
// Before:
for (const auto& [name, value] : scalars) {
    impl_->type_validator->validate_scalar(collection, name, value);
}
// ...
for (const auto& [name, value] : scalars) {
    sql += name + " = ?";
    params.push_back(value);
}

// After:
for (const auto& [name, value] : resolved.scalars) {
    impl_->type_validator->validate_scalar(collection, name, value);
}
// ...
for (const auto& [name, value] : resolved.scalars) {
    sql += name + " = ?";
    params.push_back(value);
}
```

**Array routing section:** Currently stores `const std::vector<Value>*` pointers into `element.arrays()`. After wiring, stores pointers into `resolved.arrays`:

```cpp
// Before:
for (const auto& [attr_name, values] : arrays) {
    // ...
    vector_table_columns[match.table_name][attr_name] = &values;
    // ...
}

// After:
for (const auto& [attr_name, values] : resolved.arrays) {
    // ...
    vector_table_columns[match.table_name][attr_name] = &values;
    // ...
}
```

**Important pointer lifetime:** The routing maps store `const std::vector<Value>*` pointers. These point into `resolved.arrays` values. The `resolved` variable must outlive these pointers (it does, since `resolved` is a local variable in `update_element` and the routing maps are also local).

### Pattern 3: Test Structure (Mirroring Phase 2)

Per the locked decision, Phase 3 tests mirror Phase 2's test structure exactly:

| Phase 2 Test (create_element) | Phase 3 Test (update_element) | Schema Used |
|-------------------------------|-------------------------------|-------------|
| `CreateElementScalarFkLabel` | `UpdateElementScalarFkLabel` | relations.sql |
| `CreateElementVectorFkLabels` | `UpdateElementVectorFkLabels` | relations.sql |
| (set FK tested via `ResolveFkLabelInSetCreate`) | `UpdateElementSetFkLabels` | relations.sql |
| `CreateElementTimeSeriesFkLabels` | `UpdateElementTimeSeriesFkLabels` | relations.sql |
| `CreateElementAllFkTypesInOneCall` | `UpdateElementAllFkTypesInOneCall` | relations.sql |
| `CreateElementNoFkColumnsUnchanged` | `UpdateElementNoFkColumnsUnchanged` | basic.sql |
| `ScalarFkResolutionFailureCausesNoPartialWrites` | `UpdateElementFkResolutionFailurePreservesExisting` | relations.sql |

**Key difference from create tests:** Update tests must first create an element (with initial values), then update it using FK labels, then verify the updated values. Error tests must verify that a failed update preserves the pre-existing state.

### Anti-Patterns to Avoid

- **Modifying the Element:** Same as Phase 2 -- the pre-resolve pass returns a new `ResolvedElement`; the input `Element` is unchanged.
- **Resolving inside the transaction:** The pre-resolve call must be before `TransactionGuard txn(*impl_)`. If resolution throws, zero database changes occur.
- **Duplicating the pre-resolve logic:** The user decision explicitly locks "same function, both paths." Do not copy or create a variant of `resolve_element_fk_labels`.
- **Changing the emptiness check:** The `if (scalars.empty() && arrays.empty())` check on the raw Element must remain. Do not change it to check `resolved.scalars.empty()`.
- **Forgetting to switch ALL downstream references:** There are multiple places that read from `scalars` and `arrays` local variables. All must switch to `resolved.scalars` and `resolved.arrays`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FK resolution for update_element | New resolution logic | `impl_->resolve_element_fk_labels()` from Phase 2 | One function to maintain, guaranteed consistency (locked decision) |
| Per-value FK lookup | Inline SQL queries | `resolve_fk_label` called by `resolve_element_fk_labels` | Already handles all value variants, tested, proven |
| Error messages | Custom error strings for update path | Same error format from `resolve_fk_label` | ERR-01 format is locked: "Failed to resolve label 'X' to ID in table 'Y'" |

**Key insight:** This phase adds zero new functions. It is purely wiring an existing function into an existing method.

## Common Pitfalls

### Pitfall 1: Stale Pointer After Switching to Resolved Arrays

**What goes wrong:** The current code declares `const auto& arrays = element.arrays()` at line 12 and stores `&values` pointers from iterating `arrays`. If the code switches to iterating `resolved.arrays` for routing but still uses the `arrays` local variable elsewhere, the pointers point to the wrong data (original Element arrays vs. resolved arrays).

**Why it happens:** The local variables `scalars` and `arrays` are references to the raw Element data. After adding the pre-resolve pass, these variables become misleading -- they hold unresolved values while the code should use resolved values.

**How to avoid:** After the pre-resolve call, all downstream code should iterate `resolved.scalars` and `resolved.arrays` directly. The cleanest approach is to either: (a) remove the `scalars`/`arrays` local variables entirely and use `resolved.scalars`/`resolved.arrays` everywhere, or (b) keep them only for the emptiness check. Option (a) is cleanest but changes more lines. Option (b) is minimal but requires care.

**Warning signs:** Tests pass for non-FK schemas but fail for FK schemas because the SQL receives unresolved string values instead of integer IDs.

### Pitfall 2: Emptiness Check vs. Resolved Check

**What goes wrong:** The emptiness check `if (scalars.empty() && arrays.empty())` uses raw Element data. If someone changes it to check `resolved.scalars.empty()`, it would still work because `resolve_element_fk_labels` preserves all keys. But conceptually, the check is about user intent (did they provide any attributes?) not about resolved state.

**How to avoid:** Keep the emptiness check using raw Element accessors. Add a comment if clarity is needed.

### Pitfall 3: Forgetting the Scalar Validation Switch

**What goes wrong:** The scalar type validation loop (line 23-25) currently validates against `scalars` (raw Element values). After resolution, FK values change from string to int64_t. Type validation must run on resolved values (same as Phase 2's create_element refactor).

**Why it matters:** Validating raw Element values would fail for FK columns because TypeValidator sees a string for an INTEGER column. While TypeValidator currently allows string-for-INTEGER (to support FK labels), the resolved values are the ones that should be validated for correctness.

**How to avoid:** Switch the validation loop to iterate `resolved.scalars` instead of `scalars`. This matches how `create_element` was refactored in Phase 2.

### Pitfall 4: Update Error Test Needs Existing State

**What goes wrong:** Create-path error tests just need to verify zero partial writes (nothing was created). Update-path error tests need to verify the **existing** state is preserved: the element existed before the failed update, and it still has its original values after the failed update.

**Why it matters:** A failed update that corrupts existing data is worse than a failed create that leaves nothing.

**How to avoid:** Error tests for update must: (1) create an element with initial values, (2) attempt an update with bad FK labels, (3) verify the element still has its original values. The pre-resolve pass runs before the transaction, so no SQL writes happen on failure. The existing state should be intact.

### Pitfall 5: Test Setup Complexity for Update Tests

**What goes wrong:** Each update FK test needs a 3-step setup: (a) create target entities (parents), (b) create the source entity with initial values, (c) update with FK labels. If the initial creation uses FK labels too (which it can after Phase 2), the test conflates create-path and update-path resolution.

**How to avoid:** For clarity, create initial entities with raw integer IDs (or no FK values), then update using string labels. This isolates the update-path resolution. Alternatively, create initial entities with FK labels (using create_element, which is already proven by Phase 2 tests), then update with different FK labels and verify the new values. The latter is simpler and acceptable since Phase 2 tests already validate create-path resolution.

## Code Examples

### Example 1: Complete Refactored update_element

```cpp
void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update_element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    Impl::TransactionGuard txn(*impl_);

    // Update scalars if present (using resolved values)
    if (!resolved.scalars.empty()) {
        for (const auto& [name, value] : resolved.scalars) {
            impl_->type_validator->validate_scalar(collection, name, value);
        }

        auto sql = "UPDATE " + collection + " SET ";
        std::vector<Value> params;
        auto first = true;
        for (const auto& [name, value] : resolved.scalars) {
            if (!first) { sql += ", "; }
            sql += name + " = ?";
            params.push_back(value);
            first = false;
        }
        sql += " WHERE id = ?";
        params.emplace_back(id);
        execute(sql, params);
    }

    // Route resolved arrays to their tables
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> vector_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> set_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> time_series_table_columns;

    for (const auto& [attr_name, values] : resolved.arrays) {
        // ... same routing logic, but iterating resolved.arrays
    }

    // ... rest of update logic unchanged (vector/set/time_series INSERT loops)

    txn.commit();
}
```

### Example 2: Test -- Scalar FK Update

```cpp
TEST(Database, UpdateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with parent_id pointing to Parent 1 (via FK label)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));
    db.create_element("Child", child);

    // Update: change parent_id to Parent 2 via FK label
    quiver::Element update;
    update.set("parent_id", std::string("Parent 2"));
    db.update_element("Child", 1, update);

    // Verify: parent_id is now 2
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);
}
```

### Example 3: Test -- Error Preserves Existing State

```cpp
TEST(Database, UpdateElementFkResolutionFailurePreservesExisting) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent and child
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));
    db.create_element("Child", child);

    // Attempt update with nonexistent FK label
    quiver::Element update;
    update.set("parent_id", std::string("Nonexistent Parent"));
    EXPECT_THROW(db.update_element("Child", 1, update), std::runtime_error);

    // Verify: existing value preserved
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);  // Still points to Parent 1
}
```

## State of the Art

No external technology changes. This is purely wiring an existing internal function into a second call site.

| Current State | After Phase 3 | Impact |
|--------------|---------------|--------|
| `update_element` has no FK resolution | `update_element` pre-resolves FK labels via same function as `create_element` | Users can update FK columns by label, same as create |
| FK columns in update require raw integer IDs | FK columns accept string labels, auto-resolved | Uniform UX across create and update paths |

## Files That Will Be Modified

| File | What Changes |
|------|-------------|
| `src/database_update.cpp` | Add `resolve_element_fk_labels` call, switch downstream code to use resolved values |
| `tests/test_database_update.cpp` | Add FK resolution test section: per-type, combined, no-FK regression, error |

**No changes to:** `database_impl.h` (pre-resolve function already exists), `database.h` (public API unchanged), `element.h` (Element immutable), `relations.sql` (schema already has all needed FK columns), C API files, binding files, `type_validator.cpp`.

## Open Questions

1. **Test file placement: update tests vs relations tests**
   - What we know: CONTEXT.md says "add to test_database_update.cpp, not a separate file." Phase 2 tests went into `test_database_relations.cpp`.
   - What's unclear: The locked decision says test_database_update.cpp. This is the correct location since these tests exercise `update_element` behavior.
   - Recommendation: Follow the locked decision -- add tests to `test_database_update.cpp`.

2. **Verification depth: read-back IDs vs no-throw**
   - What we know: CONTEXT.md marks this as Claude's discretion.
   - Recommendation: Read-back resolved integer IDs (like Phase 2 tests do). This is a stronger assertion than "no throw" and confirms the resolved values actually reached the database. The pattern is already established in Phase 2 tests.

## Sources

### Primary (HIGH confidence -- direct source code analysis)

- `src/database_update.cpp:1-355` -- complete `update_element` implementation (the method being modified)
- `src/database_impl.h:18-132` -- `ResolvedElement` struct and `resolve_element_fk_labels` method (the function being wired in)
- `src/database_create.cpp:1-200` -- `create_element` implementation showing Phase 2's pre-resolve wiring pattern (the reference implementation)
- `tests/test_database_update.cpp:1-647` -- existing update tests (the file being extended)
- `tests/test_database_relations.cpp:255-420` -- Phase 2 FK resolution tests (the test pattern to mirror)
- `tests/schemas/valid/relations.sql` -- FK test schema with all needed columns (Parent, Child, vector/set/time series FK tables)
- `include/quiver/element.h:1-47` -- Element class API (input type)
- `.planning/phases/03-update-element-fk-resolution/03-CONTEXT.md` -- user decisions constraining this phase

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all infrastructure exists from Phase 2 and is verified by direct source reading; zero new components
- Architecture: HIGH -- exact wiring pattern established by Phase 2's create_element refactor; this is a direct copy of that pattern
- Pitfalls: HIGH -- all pitfalls derived from direct code reading and understanding of the pointer/reference semantics in update_element's routing maps

**Research date:** 2026-02-23
**Valid until:** Indefinite (internal codebase extension, no external dependency drift)
