# Phase 2: Create Element FK Resolution - Research

**Researched:** 2026-02-23
**Domain:** C++ internal extension -- add pre-resolve pass to `create_element` for scalar, vector, and time series FK label resolution; unify existing set FK resolution into the same pass
**Confidence:** HIGH

## Summary

Phase 2 extends the `create_element` method so that FK label resolution (string label to integer ID) works for all four column types: scalar, vector, set, and time series. Phase 1 delivered the `resolve_fk_label` helper on `Database::Impl` and wired it into the set path only. This phase adds a dedicated pre-resolve pass that runs before any SQL writes, resolving all FK labels across all column types in one unified step.

The current `create_element` flow in `src/database_create.cpp` has a clear 4-phase structure: (1) validate + insert scalars into the collection table, (2) insert vector rows, (3) insert set rows (with inline `resolve_fk_label` call), (4) insert time series rows. The user decision mandates a new "Phase 0" step that resolves all FK labels before any of these writes begin. This means producing a parallel data structure of resolved values (scalars map + arrays map) while leaving the original `Element` input immutable. The existing set FK inline resolution at line 149 migrates into this pre-resolve pass for uniformity.

All building blocks exist. The `resolve_fk_label` helper works for any `TableDefinition` (collection or group table). The `Schema` class provides `get_table()` for collection-level FK metadata (scalars) and group-level FK metadata (vectors, sets, time series). The `find_all_tables_for_column()` routing already groups array columns by target table. The test schema `relations.sql` needs extension with vector FK and time series FK columns.

**Primary recommendation:** Add a pre-resolve method (e.g., `resolve_element_fk_labels`) on `Database::Impl` that takes the collection name and Element, returns a pair of resolved maps (`map<string, Value>` for scalars, `map<string, vector<Value>>` for arrays), then refactor `create_element` to call it before the transaction/write section.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Fail-fast on first bad label** -- stop at the first unresolvable FK label anywhere across all column types, throw immediately
- **Keep existing error format** -- `"Failed to resolve label 'X' to ID in table 'Y'"` with no additional column context (matches ERR-01)
- **Same non-FK INTEGER guard everywhere** -- string passed to any non-FK INTEGER column (scalar, vector, set, or time series) gets a clear Quiver error, not raw SQLite STRICT mode error
- **Dedicated first pass** -- new step before the existing 4-phase insert flow: iterate all element data, resolve every FK label, store resolved values. Then the 4 insert phases use already-resolved values
- **Separate resolved map** -- pre-resolve pass produces a parallel data structure with resolved values; Element input stays unchanged (immutable input)
- **Unify all paths** -- move the existing set FK inline resolution (from Phase 1) into the dedicated first pass too. All 4 column types use the same pre-resolved map. Consistent and clean
- **Schema-driven discovery** -- pre-resolve pass inspects schema to find FK columns automatically. User doesn't need to know which columns are FKs

### Claude's Discretion
- Exact data structure for the resolved-values map
- How the pre-resolve pass discovers and iterates FK columns (implementation of schema scanning)
- Internal method signatures and code organization

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CRE-01 | User can pass a target label (string) for a scalar FK column in `create_element` and it resolves to the target's ID | Pre-resolve pass calls `resolve_fk_label` for each scalar in the Element against the collection table's FK metadata. The collection `TableDefinition` has `foreign_keys` populated for columns like `Child.parent_id`. Resolved value (int64_t) replaces the string in the resolved scalars map before SQL INSERT. See Architecture Pattern 1. |
| CRE-02 | User can pass target labels for vector FK columns in `create_element` and they resolve to target IDs | Pre-resolve pass iterates array columns, routes each to its target table(s) via `find_all_tables_for_column`, then calls `resolve_fk_label` for each value in the array against the vector table's FK metadata. `Child_vector_refs.parent_ref` has FK to `Parent(id)`. See Architecture Pattern 2. |
| CRE-04 | User can pass target labels for time series FK columns in `create_element` and they resolve to target IDs | Same routing pattern as vectors -- time series tables can have FK columns beyond the dimension column. Pre-resolve pass handles them identically. Schema extension needed in `relations.sql` to add a time series table with FK column. See Architecture Pattern 2 and Schema Extension. |
</phase_requirements>

## Standard Stack

### Core

No new libraries or dependencies. Phase 2 uses only existing infrastructure:

| Component | Location | Purpose | Status |
|-----------|----------|---------|--------|
| `resolve_fk_label` | `src/database_impl.h:48-81` | Per-value FK label resolution (from Phase 1) | Exists, proven, no changes needed |
| `TableDefinition::foreign_keys` | `include/quiver/schema.h:43` | FK metadata for any table (collection or group) | Exists, populated at schema load |
| `Schema::get_table()` | `src/schema.cpp:49-55` | Lookup table definition by name | Exists |
| `Schema::find_all_tables_for_column()` | `src/schema.cpp:243-270` | Route array column to vector/set/time series tables | Exists, already used in `create_element` |
| `TypeValidator` | `src/type_validator.cpp` | Type checking (allows string for INTEGER for FK resolution) | Exists, no changes needed |
| `Database::Impl::TransactionGuard` | `src/database_impl.h:134-162` | Nest-aware RAII transaction | Exists |

### Alternatives Considered

| Instead of | Could Use | Why Rejected |
|------------|-----------|--------------|
| Dedicated pre-resolve method on `Impl` | Inline resolution per-path (current set approach) | User decision locks "dedicated first pass" and "unify all paths" -- inline per-path resolution is explicitly rejected |
| Returning resolved `map`+`map` pair | Mutating the Element in-place | User decision locks "Element input stays unchanged (immutable input)" |
| New method on `Database` (public) | Method on `Impl` (internal) | Resolution is internal implementation. No public API change needed. Bindings/C API are unaffected per Out of Scope in REQUIREMENTS.md |

## Architecture Patterns

### Current `create_element` Flow

The current flow in `src/database_create.cpp` lines 1-200:

```
create_element(collection, element)
  1. Validate scalars exist (line 12-14)
  2. Validate scalar types via TypeValidator (lines 17-19)
  3. Begin transaction (line 21)
  4. INSERT scalars into collection table (lines 24-42)
  5. Route arrays to vector/set/time_series table maps (lines 55-80)
  6. INSERT vector rows (lines 83-118)
  7. INSERT set rows -- with resolve_fk_label per value (lines 121-156)
  8. INSERT time series rows -- NO FK resolution (lines 159-193)
  9. Commit transaction (line 195)
```

### Target Flow After Phase 2

```
create_element(collection, element)
  0. PRE-RESOLVE PASS (new) -- resolve ALL FK labels, produce resolved maps
  1. Validate resolved scalars exist (unchanged check)
  2. Validate resolved scalar types via TypeValidator (unchanged)
  3. Begin transaction
  4. INSERT resolved scalars into collection table
  5. Route resolved arrays to vector/set/time_series table maps
  6. INSERT vector rows from resolved arrays
  7. INSERT set rows from resolved arrays (no more inline resolve_fk_label)
  8. INSERT time series rows from resolved arrays
  9. Commit transaction
```

### Pattern 1: Scalar FK Resolution

**What:** The collection table itself can have FK columns (e.g., `Child.parent_id REFERENCES Parent(id)`). When a user calls `element.set("parent_id", "Parent Label")`, the TypeValidator allows the string (it permits string for INTEGER), and the pre-resolve pass resolves it to the integer ID before any SQL.

**Schema basis:** `Child` table in `relations.sql` has:
```sql
parent_id INTEGER,
FOREIGN KEY (parent_id) REFERENCES Parent(id) ON DELETE SET NULL ON UPDATE CASCADE
```

**Resolution flow:**
```cpp
// In pre-resolve pass:
const auto* collection_def = impl_->schema->get_table(collection);  // "Child" table
for (const auto& [name, value] : element.scalars()) {
    resolved_scalars[name] = impl_->resolve_fk_label(*collection_def, name, value, *this);
}
```

The `resolve_fk_label` helper already handles this: it checks `collection_def->foreign_keys` for `parent_id`, finds the FK to `Parent(id)`, executes `SELECT id FROM Parent WHERE label = ?`, and returns the resolved int64_t.

**Key insight:** Scalar FK resolution uses the **collection** table's FK metadata, not a group table. The existing `resolve_fk_label` works unchanged because it takes a generic `TableDefinition&`.

### Pattern 2: Vector and Time Series FK Resolution

**What:** Vector and time series group tables can have FK columns (e.g., `Child_vector_refs.parent_ref REFERENCES Parent(id)`). The pre-resolve pass must route each array column to its target table, then resolve each value in the array.

**Resolution flow:**
```cpp
// In pre-resolve pass (for arrays):
for (const auto& [array_name, values] : element.arrays()) {
    auto matches = impl_->schema->find_all_tables_for_column(collection, array_name);
    for (const auto& match : matches) {
        const auto* table_def = impl_->schema->get_table(match.table_name);
        // Resolve each value in the array against this table's FK metadata
        for (size_t i = 0; i < values.size(); ++i) {
            resolved_arrays[array_name][i] = impl_->resolve_fk_label(*table_def, array_name, values[i], *this);
        }
    }
}
```

**Complication -- shared columns:** A column like `date_time` can appear in multiple time series tables (e.g., `Sensor_time_series_temperature` and `Sensor_time_series_humidity`). It is routed to all of them. For FK resolution, this means the same array column could be resolved against multiple tables. However, FK resolution is idempotent for non-FK columns (returns value unchanged), so resolving `date_time` (a TEXT column) against any table just passes it through. For actual FK columns that are unique to one table, the resolution happens correctly.

**Potential issue:** If the same column name appears in two tables with different FK definitions, the resolution could produce different results. In practice, this does not happen in Quiver schemas because: (a) FK columns have unique names per the `Child_set_mentors` pattern from Phase 1, and (b) shared columns like `date_time` are never FKs.

**Simplification:** For array resolution, we can resolve each value once using the first table match that contains the column. Since `resolve_fk_label` returns unchanged values for non-FK columns, resolving against the "wrong" table for non-FK columns is harmless. For FK columns, the column name is unique to one table (by schema design), so the first match is the correct one.

### Pattern 3: Pre-Resolve Method Design

**Recommended data structure:**
```cpp
struct ResolvedElement {
    std::map<std::string, Value> scalars;
    std::map<std::string, std::vector<Value>> arrays;
};
```

Or simply return a pair of maps. The struct approach is cleaner and self-documenting.

**Recommended method signature:**
```cpp
// In database_impl.h, inside struct Database::Impl:
ResolvedElement resolve_element_fk_labels(
    const std::string& collection,
    const Element& element,
    Database& db);
```

**Why on `Impl`:** Same reasoning as `resolve_fk_label` -- needs schema access (via `impl_->schema`) and database access (via `db.execute()` through `resolve_fk_label`).

**Why this method is independently testable:** It takes an Element input and returns a ResolvedElement output. Tests can call `create_element` with FK labels and verify the stored integer IDs, confirming the pre-resolve pass worked. No need to expose the method publicly -- it is tested through its effect on `create_element`.

### Pattern 4: Migrating Set FK Inline Resolution

**Current state:** `database_create.cpp` line 149 calls `resolve_fk_label` inline during set row insertion:
```cpp
auto val = impl_->resolve_fk_label(*table_def, col_name, (*values_ptr)[row_idx], *this);
```

**After migration:** The pre-resolve pass resolves all set array values before the transaction begins. The set insertion loop uses pre-resolved values directly, just like vector and time series loops:
```cpp
// Set insertion loop (after migration):
for (const auto& [col_name, values_ptr] : columns) {
    set_sql += ", " + col_name;
    set_placeholders += ", ?";
    set_params.push_back((*values_ptr)[row_idx]);  // already resolved
}
```

This makes all four insert paths structurally identical (none of them call `resolve_fk_label`), with all resolution happening in the pre-resolve pass.

### Schema Extension: Test Schema Changes

The existing `relations.sql` needs extension for Phase 2 tests. Per CONTEXT.md decisions:
- **Extend existing relations.sql** -- add scalar FK, vector FK, and time series FK columns
- **Parent/Child only** -- no new tables beyond the existing pattern
- **Use existing columns where possible** -- `Child.parent_id` is already a scalar FK

Required additions to `relations.sql`:

1. **Vector FK column already exists:** `Child_vector_refs.parent_ref` has `FOREIGN KEY (parent_ref) REFERENCES Parent(id)`. However, the column name `parent_ref` also exists in `Child_set_parents`, which causes dual-routing via `find_all_tables_for_column`. This was the exact problem discovered in Phase 1 (see 01-01-SUMMARY.md). For vector FK testing, either use a uniquely-named column or accept the dual-routing.

2. **Time series FK table needed:** No time series table exists in `relations.sql` currently. Need to add one, e.g.:
```sql
CREATE TABLE Child_time_series_events (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    parent_ref INTEGER,
    FOREIGN KEY (parent_ref) REFERENCES Parent(id) ON DELETE SET NULL ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;
```

**Column naming consideration:** The `parent_ref` column name already exists in `Child_vector_refs` and `Child_set_parents`. Adding it to a time series table creates triple-routing. Options:
- Use a unique column name for the time series FK (e.g., `sponsor_id`) to avoid routing conflicts -- follows the Phase 1 pattern with `mentor_id`.
- Accept the routing and verify that pre-resolve correctly handles multi-table routing for the same column.

**Recommendation:** Use unique column names for test FK columns to isolate each test path, following the established Phase 1 pattern.

### Anti-Patterns to Avoid

- **Resolving inside the transaction:** The user decision locks "pre-resolve pass before any SQL writes." All resolution must complete before `TransactionGuard txn(*impl_)`. If resolution throws, zero partial writes exist.
- **Modifying the Element:** The user decision locks "Element input stays unchanged." The pre-resolve pass returns a new data structure; it never calls `element.set()` or modifies `scalars_`/`arrays_`.
- **Resolving only FK columns:** The pre-resolve pass should copy ALL values (FK and non-FK) into the resolved map. Non-FK values pass through `resolve_fk_label` unchanged. This avoids needing conditional logic to decide whether to read from the Element or the resolved map.
- **Separate resolution passes per type:** One pass resolves everything (scalars + arrays). Fail-fast means the first bad label anywhere stops the entire operation.
- **Keeping inline resolution for sets:** The decision explicitly says "move existing set FK inline resolution into the dedicated first pass too." The set loop must use pre-resolved values.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Per-value FK resolution | New FK lookup logic | `impl_->resolve_fk_label()` from Phase 1 | Already handles all 4 value variants, tested, proven |
| FK column discovery | Custom PRAGMA queries or column iteration | `TableDefinition::foreign_keys` (already populated) | Schema loads FK metadata once; `foreign_keys` vector is ready |
| Array-to-table routing | Manual table name construction | `Schema::find_all_tables_for_column()` | Already used by `create_element` for the insert routing; reuse for resolution routing |
| Type validation | Manual type checking in pre-resolve | `TypeValidator::validate_scalar/validate_array` | Already called in the existing flow; pre-resolve pass runs `resolve_fk_label` which includes the non-FK INTEGER guard |

**Key insight:** The pre-resolve pass is a composition of existing primitives (`resolve_fk_label` + `find_all_tables_for_column` + `get_table`). No new primitives needed.

## Common Pitfalls

### Pitfall 1: Scalar FK Resolution -- Using Wrong TableDefinition

**What goes wrong:** Scalar FK columns (like `Child.parent_id`) live on the collection table, not a group table. Using a group table's FK metadata for scalar resolution finds nothing (group tables' FKs are the `id REFERENCES Child(id)` parent link, not the user-data FK).

**Why it happens:** The existing code only resolves FKs for group tables (set path). Extending to scalars requires using the collection table's own `TableDefinition`.

**How to avoid:** For scalar resolution, call `impl_->schema->get_table(collection)` to get the collection's `TableDefinition` (which contains `parent_id`'s FK metadata). For array resolution, use the group table's `TableDefinition` (which contains the group table's FK metadata).

**Warning signs:** Scalar FK labels passing through unresolved, causing a SQLite STRICT mode error on INSERT (string in INTEGER column).

### Pitfall 2: Pre-Resolve Ordering vs. Type Validation

**What goes wrong:** The current flow validates scalar types before insertion (line 17-19). If the pre-resolve pass runs before type validation, the TypeValidator sees the original string value for FK columns (which is allowed), but after resolution the value is int64_t (also allowed). If type validation runs after resolution, string values for non-FK INTEGER columns would already be caught by `resolve_fk_label`'s guard.

**Why it matters:** The order determines which error surfaces first for edge cases.

**How to avoid:** The pre-resolve pass should run before type validation. `resolve_fk_label` already contains the non-FK INTEGER guard, so it catches bad strings before TypeValidator sees them. TypeValidator then validates the resolved values (all int64_t for FK columns), which always passes.

**Recommendation:** Move type validation to after resolution, or keep it in place since `resolve_fk_label`'s guard catches the critical case. Either ordering works; the key is that the pre-resolve pass runs before the transaction.

### Pitfall 3: Array Column Routing Duplication

**What goes wrong:** The pre-resolve pass needs to route array columns to tables (to get the right `TableDefinition` for FK lookup). The existing insert flow already does this routing (lines 55-80). If both the pre-resolve pass and the insert flow call `find_all_tables_for_column`, the routing runs twice.

**Why it happens:** The pre-resolve pass and the insert flow serve different purposes (resolution vs. insertion) but both need the same routing information.

**How to avoid:** Two approaches:
1. **Accept the duplication:** `find_all_tables_for_column` is cheap (iterates in-memory maps). The clarity of separating resolution from insertion outweighs the minor redundancy.
2. **Share the routing:** The pre-resolve pass produces the routing maps as a side effect, and the insert flow reuses them. This is more efficient but couples the pre-resolve output to the insert flow's data structures.

**Recommendation:** Accept the duplication for clarity. The pre-resolve method should be a clean, self-contained function. The existing insert routing remains unchanged.

### Pitfall 4: Shared Column Names Across Multiple Tables

**What goes wrong:** A column name like `parent_ref` might exist in both `Child_vector_refs` and `Child_set_parents`. The `find_all_tables_for_column` returns both matches. The pre-resolve pass resolves the column's values against both tables' FK metadata.

**Why it happens:** Quiver allows the same column name in multiple group tables for the same collection.

**How to avoid:** For FK resolution, the column only needs to be resolved once (FK metadata is the same if the column has the same FK in both tables). In practice, the Phase 1 workaround was to use unique column names. For the pre-resolve pass, resolve each unique `(array_name)` once using the first table match that has FK metadata for that column. Since `resolve_fk_label` is idempotent for non-FK columns, resolving against any table produces the correct result.

**Warning signs:** Test failures when using shared column names across vector and set tables.

### Pitfall 5: Empty Arrays After Resolution

**What goes wrong:** The existing code throws for empty arrays (line 57: "empty array not allowed"). If the pre-resolve pass creates the resolved arrays map before this check, empty arrays could bypass the check.

**How to avoid:** Keep the empty array check in the insert flow (after resolution), since it is a validation concern, not a resolution concern. The pre-resolve pass simply resolves whatever values exist; if the array is empty, it produces an empty resolved array, and the insert flow's check catches it.

### Pitfall 6: Transaction Atomicity with Pre-Resolve

**What goes wrong:** The pre-resolve pass runs before the transaction. If it makes SQL calls (`SELECT id FROM ... WHERE label = ?` inside `resolve_fk_label`), these reads happen outside the transaction. In theory, between resolution and insertion, the target row could be deleted.

**Why this is acceptable:** SQLite in WAL mode with single-writer guarantees that reads see committed state. The pre-resolve reads and the subsequent transaction writes are in the same thread/connection. The window for concurrent modification is negligible in Quiver's single-connection model. The user decision explicitly places resolution before the transaction.

## Code Examples

### Example 1: Pre-Resolve Method

```cpp
// In database_impl.h (or a new section of database_create.cpp):

struct ResolvedElement {
    std::map<std::string, Value> scalars;
    std::map<std::string, std::vector<Value>> arrays;
};

// On Database::Impl:
ResolvedElement resolve_element_fk_labels(
    const std::string& collection,
    const Element& element,
    Database& db)
{
    ResolvedElement resolved;
    const auto* collection_def = schema->get_table(collection);

    // Resolve scalars against collection table FK metadata
    for (const auto& [name, value] : element.scalars()) {
        resolved.scalars[name] = resolve_fk_label(*collection_def, name, value, db);
    }

    // Resolve arrays against their respective group table FK metadata
    for (const auto& [array_name, values] : element.arrays()) {
        auto matches = schema->find_all_tables_for_column(collection, array_name);
        // Find the first table that has this column for FK resolution
        const TableDefinition* resolve_table = nullptr;
        for (const auto& match : matches) {
            const auto* td = schema->get_table(match.table_name);
            if (td) {
                resolve_table = td;
                break;
            }
        }

        std::vector<Value> resolved_values;
        resolved_values.reserve(values.size());
        for (const auto& val : values) {
            if (resolve_table) {
                resolved_values.push_back(
                    resolve_fk_label(*resolve_table, array_name, val, db));
            } else {
                resolved_values.push_back(val);
            }
        }
        resolved.arrays[array_name] = std::move(resolved_values);
    }

    return resolved;
}
```

### Example 2: Refactored `create_element` Using Pre-Resolved Values

```cpp
int64_t Database::create_element(const std::string& collection, const Element& element) {
    impl_->logger->debug("Creating element in collection: {}", collection);
    impl_->require_collection(collection, "create_element");

    const auto& scalars = element.scalars();
    if (scalars.empty()) {
        throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    // Validate resolved scalar types
    for (const auto& [name, value] : resolved.scalars) {
        impl_->type_validator->validate_scalar(collection, name, value);
    }

    Impl::TransactionGuard txn(*impl_);

    // Build INSERT SQL using resolved scalars
    auto sql = "INSERT INTO " + collection + " (";
    std::string placeholders;
    std::vector<Value> params;
    auto first = true;
    for (const auto& [name, value] : resolved.scalars) {
        if (!first) { sql += ", "; placeholders += ", "; }
        sql += name;
        placeholders += "?";
        params.push_back(value);
        first = false;
    }
    sql += ") VALUES (" + placeholders + ")";
    execute(sql, params);
    const auto element_id = sqlite3_last_insert_rowid(impl_->db);

    // Route resolved arrays to tables (same routing as before)
    // ... use resolved.arrays instead of element.arrays()
    // ... all insert loops use pre-resolved values, no inline resolve_fk_label calls

    txn.commit();
    return element_id;
}
```

### Example 3: Test -- Scalar FK Resolution via `create_element`

```cpp
TEST(Database, CreateElementWithScalarFkLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child with scalar FK using string label
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));  // string label, not int ID
    db.create_element("Child", child);

    // Verify: read back as integer, should be resolved ID
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);  // Parent 1's auto-increment ID
}
```

### Example 4: Test -- Vector FK Resolution via `create_element`

```cpp
TEST(Database, CreateElementWithVectorFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with vector FK using string labels
    // (uses a uniquely-named FK column to avoid routing conflicts)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_ref", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Verify: read back as integers
    auto refs = db.read_vector_integers("Child", "parent_ref");
    ASSERT_EQ(refs.size(), 1);
    ASSERT_EQ(refs[0].size(), 2);
    EXPECT_EQ(refs[0][0], 1);
    EXPECT_EQ(refs[0][1], 2);
}
```

### Example 5: Test -- Combined All-Types FK Resolution

```cpp
TEST(Database, CreateElementWithAllFkTypes) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with scalar FK + set FK + (optionally vector/time series FK)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));       // scalar FK
    child.set("mentor_id", std::vector<std::string>{"Parent 2"});  // set FK
    // ... additional vector/time series FK columns as schema allows
    db.create_element("Child", child);

    // Verify scalar FK resolved
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    EXPECT_EQ(parent_ids[0], 1);

    // Verify set FK resolved
    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    EXPECT_EQ(mentors[0][0], 2);
}
```

### Example 6: Test -- No-FK Regression

```cpp
TEST(Database, CreateElementWithNoFkColumnsUnchanged) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // basic.sql has no FK columns
    quiver::Element element;
    element.set("label", std::string("Config 1"));
    element.set("integer_attribute", int64_t{42});
    element.set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels[0], "Config 1");
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(integers[0], 42);
}
```

## State of the Art

No external technology changes. This is purely extending an internal pattern.

| Current State | After Phase 2 | Impact |
|--------------|---------------|--------|
| FK resolution only in set path (inline `resolve_fk_label` call) | Unified pre-resolve pass handles all 4 column types | All FK types work in `create_element`; consistent architecture |
| Scalar FK columns require `update_scalar_relation` after creation | User can set scalar FK labels directly in `create_element` | Simpler API usage; FK resolution at creation time |
| Vector/time series FK columns require passing raw integer IDs | User can pass string labels; they resolve automatically | Uniform experience across all column types |
| Resolution happens inside the transaction (set path) | Resolution happens before the transaction | Fail-fast: bad label means zero partial writes |

## Open Questions

1. **ResolvedElement as struct vs. pair of maps**
   - What we know: User decision says "separate resolved map" with "parallel data structure." Both approaches satisfy this.
   - What's unclear: Whether to define `ResolvedElement` as a named struct in `database_impl.h` or use `std::pair<std::map<...>, std::map<...>>`.
   - Recommendation: Named struct is cleaner and more readable. Place it inside `Database::Impl` or in the `quiver` namespace in `database_impl.h` since it is internal-only.

2. **Vector FK column routing -- shared column names**
   - What we know: `parent_ref` exists in both `Child_vector_refs` and `Child_set_parents`. In `create_element`, `find_all_tables_for_column` routes it to both. The pre-resolve pass needs to resolve against the right table.
   - What's unclear: Whether to resolve against all matching tables or just the first.
   - Recommendation: Resolve against the first matching table for each type. Since the same column in different tables should have the same FK definition (referencing the same target), the resolution result is identical regardless of which table is used. If needed for correctness, resolve against the table that actually matches the routing type (vector table for vector resolution, set table for set resolution).

3. **Where to place the pre-resolve method**
   - What we know: It needs to be on `Database::Impl` for schema/execute access. It is only called from `database_create.cpp` today (Phase 3 will call it from `database_update.cpp` too).
   - What's unclear: Whether to define it in `database_impl.h` (accessible from all translation units) or declare it in `database_impl.h` and define it in `database_create.cpp`.
   - Recommendation: Define it in `database_impl.h` (inline in the struct, like `resolve_fk_label`) so Phase 3 can reuse it from `database_update.cpp` without forward-declaration issues.

## Files That Will Be Modified

| File | What Changes |
|------|-------------|
| `src/database_impl.h` | Add `ResolvedElement` struct and `resolve_element_fk_labels` method to `Database::Impl` |
| `src/database_create.cpp` | Refactor `create_element` to call pre-resolve pass, use resolved values, remove inline set FK resolution |
| `tests/schemas/valid/relations.sql` | Add time series FK table (and possibly vector FK with unique column name) |
| `tests/test_database_relations.cpp` | Add tests for scalar FK, vector FK, time series FK, combined all-types, and no-FK regression |

**No changes to:** `database.h` (public API unchanged), `element.h` (Element immutable), C API files, binding files, `type_validator.cpp`.

## Sources

### Primary (HIGH confidence -- direct source code analysis)

- `src/database_create.cpp:1-200` -- complete `create_element` implementation, the method being modified
- `src/database_impl.h:48-81` -- `resolve_fk_label` helper from Phase 1, the building block for pre-resolve
- `src/database_impl.h:16-163` -- full `Database::Impl` struct including TransactionGuard
- `include/quiver/schema.h:24-104` -- `ForeignKey`, `TableDefinition`, `Schema` classes with FK metadata access
- `src/schema.cpp:243-270` -- `find_all_tables_for_column()` implementation, the array routing mechanism
- `src/database_update.cpp:7-164` -- `update_element` implementation (Phase 3 target, confirms pre-resolve method reusability)
- `include/quiver/element.h:1-47` -- Element class API showing `scalars()` and `arrays()` accessors
- `include/quiver/value.h:10` -- `Value = std::variant<nullptr_t, int64_t, double, std::string>` type definition
- `src/type_validator.cpp:42-48` -- string-for-INTEGER allowance enabling FK label passing
- `tests/schemas/valid/relations.sql` -- current FK test schema (Parent, Child, vectors, sets)
- `tests/test_database_relations.cpp:191-249` -- Phase 1 FK resolution tests as baseline
- `tests/test_database_create.cpp:1-339` -- existing create tests for regression checking
- `.planning/phases/01-foundation/01-01-SUMMARY.md` -- Phase 1 summary documenting `resolve_fk_label` interface and decisions
- `.planning/phases/02-create-element-fk-resolution/02-CONTEXT.md` -- user decisions constraining this phase

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all infrastructure exists and is verified by direct source reading
- Architecture: HIGH -- pre-resolve pattern is a direct composition of proven Phase 1 primitives; `resolve_fk_label` + schema routing are both battle-tested
- Pitfalls: HIGH -- all pitfalls derived from direct code reading and Phase 1 experience (e.g., shared column names, routing duplication)

**Research date:** 2026-02-23
**Valid until:** Indefinite (internal codebase extension, no external dependency drift)
