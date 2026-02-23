# Architecture Patterns

**Domain:** FK label resolution integration into create_element/update_element
**Researched:** 2026-02-23
**Confidence:** HIGH (based entirely on source code analysis of existing codebase)

## Current Architecture (Relevant Subset)

### Data Flow: create_element

```
Element (scalars + arrays)
  |
  v
validate_scalar() for each scalar value       <-- TypeValidator checks type
  |
  v
INSERT INTO collection (scalar columns)
  |
  v
Route arrays to vector/set/time_series maps   <-- Schema::find_all_tables_for_column
  |
  v
For each vector table:
  validate_array() -> INSERT rows with vector_index
  |
For each set table:
  validate_array() -> FK resolution (if string for FK col) -> INSERT rows
  |
For each time_series table:
  validate_array() -> INSERT rows
```

### Data Flow: update_element

```
Element (scalars + arrays)
  |
  v
validate_scalar() for each scalar value       <-- TypeValidator checks type
  |
  v
UPDATE collection SET ... WHERE id = ?
  |
  v
Route arrays to vector/set/time_series maps   <-- Schema::find_all_tables_for_column
  |
  v
For each vector table:
  DELETE existing -> INSERT new rows           <-- NO FK resolution
  |
For each set table:
  DELETE existing -> INSERT new rows           <-- NO FK resolution (gap)
  |
For each time_series table:
  DELETE existing -> INSERT new rows
```

### Key Observation: The Set FK Pattern Already Works

The existing FK resolution in `create_element` for set tables (lines 151-167 of `database_create.cpp`) provides the exact pattern to replicate:

```cpp
// For each value being inserted into a set row:
for (const auto& fk : table_def->foreign_keys) {
    if (fk.from_column == col_name && std::holds_alternative<std::string>(val)) {
        const std::string& label = std::get<std::string>(val);
        auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
        auto lookup_result = execute(lookup_sql, {label});
        if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
            throw std::runtime_error("Failed to resolve label '" + label +
                                     "' to ID in table '" + fk.to_table + "'");
        }
        val = lookup_result[0].get_integer(0).value();
        break;
    }
}
```

This pattern: (1) checks if the column has an FK, (2) checks if the value is a string (label), (3) looks up the ID, (4) replaces the string value with the integer ID, (5) throws on missing target. It needs to be **extracted and reused**, not duplicated further.

## Recommended Architecture

### New Component: `resolve_fk_label` Helper on Database::Impl

Add a private helper method to `Database::Impl` that encapsulates the label-to-ID lookup. This is the single point of FK resolution logic.

```cpp
// In database_impl.h, inside struct Database::Impl:

// Resolves a string label to an integer ID via FK lookup.
// Returns the resolved Value (int64_t) if the column is an FK and value is a string.
// Returns the original value unchanged if column is not an FK or value is not a string.
// Throws if the label cannot be found in the target table.
Value resolve_fk_label(const TableDefinition& table_def,
                       const std::string& column,
                       const Value& value,
                       Database& db);
```

**Why on Impl, not a free function:** It needs access to `Database::execute()` (which is private) to run the lookup query. Placing it on Impl and passing a `Database&` reference follows the existing pattern where Impl helpers collaborate with the Database outer class.

**Alternative considered: free function in database_create.cpp.** Rejected because `update_element` (in `database_update.cpp`) needs the same function. The helper must be shared across translation units, so it belongs in `database_impl.h`.

### Implementation

```cpp
// In database_impl.h, inside struct Database::Impl:
Value resolve_fk_label(const TableDefinition& table_def,
                       const std::string& column,
                       const Value& value,
                       Database& db) {
    if (!std::holds_alternative<std::string>(value)) {
        return value;  // Not a string, nothing to resolve
    }

    for (const auto& fk : table_def.foreign_keys) {
        if (fk.from_column == column) {
            const std::string& label = std::get<std::string>(value);
            auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
            auto lookup_result = db.execute(lookup_sql, {label});
            if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
                throw std::runtime_error("Failed to resolve label '" + label +
                                         "' to ID in table '" + fk.to_table + "'");
            }
            return lookup_result[0].get_integer(0).value();
        }
    }

    return value;  // Column is not an FK, return as-is
}
```

### Component Boundaries

| Component | Responsibility | Changes Needed |
|-----------|---------------|----------------|
| `Database::Impl` (database_impl.h) | New `resolve_fk_label()` helper | ADD method |
| `TypeValidator` (type_validator.cpp) | Already allows string->INTEGER | NONE (already done, line 44) |
| `Schema` / `TableDefinition` (schema.h) | FK lookup via `table_def->foreign_keys` | NONE |
| `database_create.cpp` | Apply FK resolution to scalar and vector paths; refactor set path to use helper | MODIFY |
| `database_update.cpp` | Apply FK resolution to scalar, vector, and set paths in `update_element` | MODIFY |
| C API (database_create.cpp, database_update.cpp) | Thin wrappers, no FK awareness | NONE |
| Julia/Dart/Lua bindings | Callers pass strings for FK columns naturally | NONE |
| `Element` class | Already stores `Value` (variant including string) for scalars | NONE |

### Data Flow After Changes

**create_element:**

```
Element (scalars + arrays)
  |
  v
validate_scalar() for each scalar       <-- String->INTEGER still passes (line 44)
  |
  v
resolve_fk_label() for each scalar      <-- NEW: resolves "Parent 1" -> 1
  |
  v
INSERT INTO collection (scalar columns)  <-- Now has integer IDs, not labels
  |
  v
Route arrays to vector/set/time_series
  |
For each vector table:
  validate_array() -> resolve_fk_label() per value -> INSERT    <-- NEW
For each set table:
  validate_array() -> resolve_fk_label() per value -> INSERT    <-- REFACTORED (uses helper)
For each time_series table:
  validate_array() -> INSERT                                     <-- UNCHANGED
```

**update_element:**

```
Element (scalars + arrays)
  |
  v
validate_scalar() for each scalar       <-- String->INTEGER still passes
  |
  v
resolve_fk_label() for each scalar      <-- NEW
  |
  v
UPDATE collection SET ... WHERE id = ?   <-- Now has integer IDs
  |
  v
Route arrays to vector/set/time_series
  |
For each vector table:
  DELETE -> resolve_fk_label() per value -> INSERT    <-- NEW
For each set table:
  DELETE -> resolve_fk_label() per value -> INSERT    <-- NEW
For each time_series table:
  DELETE -> INSERT                                     <-- UNCHANGED
```

## Integration Points

### 1. Scalar FK Resolution in create_element (database_create.cpp)

**Where:** After `validate_scalar()` loop (line 19), before SQL building (line 24).

**What:** Iterate scalars, call `resolve_fk_label()` for each, collect resolved values.

```cpp
// After validation, resolve FK labels to IDs
const auto* table_def = impl_->schema->get_table(collection);
std::map<std::string, Value> resolved_scalars;
for (const auto& [name, value] : scalars) {
    resolved_scalars[name] = impl_->resolve_fk_label(*table_def, name, value, *this);
}
// Use resolved_scalars instead of scalars for SQL building
```

**Dependency:** `table_def` is already retrieved via `impl_->schema->get_table(collection)`. The `require_collection` call on line 9 guarantees it exists.

### 2. Vector FK Resolution in create_element (database_create.cpp)

**Where:** Inside the vector table insertion loop (line 102-117), when building `vec_params`.

**What:** Before pushing each value to `vec_params`, pass it through `resolve_fk_label()`.

```cpp
// line 111 changes from:
vec_params.push_back((*values_ptr)[row_idx]);
// to:
vec_params.push_back(impl_->resolve_fk_label(*table_def, col_name, (*values_ptr)[row_idx], *this));
```

### 3. Set FK Resolution in create_element -- REFACTOR (database_create.cpp)

**Where:** Lines 151-167 (existing inline FK resolution).

**What:** Replace the inline 17-line FK resolution block with a single call to the shared helper.

```cpp
// line 149-167 collapses to:
auto val = impl_->resolve_fk_label(*table_def, col_name, (*values_ptr)[row_idx], *this);
set_params.push_back(val);
```

### 4. Scalar FK Resolution in update_element (database_update.cpp)

**Where:** After `validate_scalar()` loop (line 25), before SQL building (line 28).

**What:** Same pattern as create_element scalar resolution.

### 5. Vector FK Resolution in update_element (database_update.cpp)

**Where:** Inside vector table insertion loop (line 93-97), when building `params`.

**What:** Same pattern as create_element vector resolution.

### 6. Set FK Resolution in update_element (database_update.cpp)

**Where:** Inside set table insertion loop (line 122-125), when building `params`.

**What:** Same pattern. Currently `update_element` has NO FK resolution for sets (gap identified in PROJECT.md).

## TypeValidator Interaction

**No changes needed.** The `TypeValidator::validate_value()` already permits strings for INTEGER columns (line 43-44 of type_validator.cpp):

```cpp
} else if constexpr (std::is_same_v<T, std::string>) {
    // String can go to TEXT, INTEGER (FK label resolution), or DATE_TIME (stored as TEXT)
    if (expected_type != DataType::Text && expected_type != DataType::Integer &&
        expected_type != DataType::DateTime) {
```

The comment explicitly states "FK label resolution" as the reason strings are allowed for INTEGER columns. This was added as preparation for exactly this milestone.

**Validation order:** `validate_scalar()` / `validate_array()` runs FIRST, confirming the value is an acceptable type for the column. Then `resolve_fk_label()` runs, converting the validated string to an integer. This keeps validation and resolution as separate concerns.

## Schema FK Lookup

**No changes needed.** The existing `TableDefinition::foreign_keys` vector already contains all FK definitions for every table (populated by `Schema::query_foreign_keys()` which calls `PRAGMA foreign_key_list`). The `resolve_fk_label()` helper just iterates this vector to find a matching `from_column`.

For scalar FK columns, the FK definition lives on the collection table itself (e.g., `Child.parent_id -> Parent.id`). For vector/set FK columns, the FK definition lives on the group table (e.g., `Child_set_parents.parent_ref -> Parent.id`). In both cases, `table_def->foreign_keys` has the information.

## C API Transparency

**The C API and all bindings require ZERO changes.** The FK resolution happens entirely inside the C++ layer:

1. **Element already accepts strings for any scalar** -- `Element::set(name, string)` stores a `Value` (variant). The C API `quiver_element_set_string()` calls this.
2. **Element already accepts string arrays** -- `Element::set(name, vector<string>)` stores `vector<Value>`. The C API element setters call this.
3. **Resolution happens inside `create_element` / `update_element`** -- these are the C++ methods that the C API wraps. The C API just calls `db->db.create_element(collection, element->element)`.
4. **Error propagation already works** -- if `resolve_fk_label()` throws, the C API catch block captures it via `quiver_set_last_error(e.what())`.

**Binding impact:** Julia, Dart, and Lua bindings already pass strings through the Element builder. When a user calls `element.set("parent_id", "Parent 1")`, it flows through C API -> C++ `create_element` -> `resolve_fk_label()` -> SQL INSERT. No binding code changes needed.

## Patterns to Follow

### Pattern 1: Resolve-Then-Build
**What:** Resolve FK labels to IDs before building SQL statements, not during.
**When:** Any write path that touches FK columns.
**Why:** Keeps SQL building logic clean; all values are in their final form when the INSERT/UPDATE is constructed.

```cpp
// Resolve scalars before building SQL
for (const auto& [name, value] : scalars) {
    resolved[name] = impl_->resolve_fk_label(*table_def, name, value, *this);
}
// Build SQL using resolved values only
```

### Pattern 2: Conditional Resolution (No-Op for Non-FK)
**What:** `resolve_fk_label()` returns the value unchanged if the column has no FK or the value is not a string.
**When:** Always -- call it on every value, let it decide.
**Why:** Callers do not need to check whether a column is an FK. This eliminates conditional branching at call sites.

### Pattern 3: Value Mutation via Copy
**What:** `resolve_fk_label()` returns a new `Value`, it does not mutate the input.
**When:** Always.
**Why:** The Element's data should not be modified. The resolution produces a local copy for the SQL parameters.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Duplicating FK Resolution Inline
**What:** Copy-pasting the 17-line FK resolution block from set path into vector/scalar paths.
**Why bad:** Already happened once (set path in create_element). Duplicating it 5 more times makes 6 copies to maintain.
**Instead:** Extract to `resolve_fk_label()` on Impl, call from all 6 sites.

### Anti-Pattern 2: FK Resolution in TypeValidator
**What:** Moving FK resolution logic into `TypeValidator::validate_scalar()`.
**Why bad:** TypeValidator's job is validation (is this type acceptable?), not transformation (convert this value). Mixing concerns makes TypeValidator dependent on `Database::execute()` (SQL execution), breaking its current clean dependency on Schema only.
**Instead:** Keep TypeValidator as pure validation. Resolution is a separate step that happens after validation.

### Anti-Pattern 3: FK Resolution in the C API Layer
**What:** Adding FK resolution logic in `src/c/database_create.cpp` or similar.
**Why bad:** Violates the "intelligence in C++ layer" principle. The C API is a thin wrapper. FK resolution is business logic.
**Instead:** All FK resolution stays in the C++ Database methods.

### Anti-Pattern 4: Separate resolve_fk_labels Function That Mutates Element
**What:** A function that takes `Element&` and modifies its scalars/arrays in-place.
**Why bad:** Element is the caller's data. Mutating it has side effects -- if the caller reuses the element, it now contains IDs instead of labels.
**Instead:** Resolve values into local copies during SQL parameter building.

## Build Order

The changes have a clear dependency chain:

1. **Add `resolve_fk_label()` to `Database::Impl`** (database_impl.h)
   - No test changes yet, just the helper
   - Depends on: nothing new (uses existing Schema FK data and Database::execute)

2. **Refactor set FK resolution in `create_element`** (database_create.cpp)
   - Replace inline 17-line block with call to `resolve_fk_label()`
   - Existing tests must still pass (no behavioral change)
   - Depends on: step 1

3. **Add scalar FK resolution in `create_element`** (database_create.cpp)
   - Add resolution loop after validation, before SQL building
   - Add tests: create Child with `parent_id: "Parent 1"` (string label)
   - Depends on: step 1

4. **Add vector FK resolution in `create_element`** (database_create.cpp)
   - Add `resolve_fk_label()` call in vector row insertion
   - Add tests: create Child with `parent_ref: ["Parent 1", "Parent 2"]` as vector
   - Depends on: step 1

5. **Add scalar FK resolution in `update_element`** (database_update.cpp)
   - Same pattern as create_element scalars
   - Add tests: update Child scalar `parent_id` via `update_element`
   - Depends on: step 1

6. **Add vector FK resolution in `update_element`** (database_update.cpp)
   - Same pattern as create_element vectors
   - Add tests
   - Depends on: step 1

7. **Add set FK resolution in `update_element`** (database_update.cpp)
   - Same pattern as create_element sets
   - Add tests
   - Depends on: step 1

8. **Evaluate `update_scalar_relation` redundancy** (database_relations.cpp)
   - After steps 3+5, `update_element` can set scalar FK by ID
   - `update_scalar_relation` uses label-to-label (from_label -> to_label)
   - `update_element` uses id-to-label (element id + FK label)
   - Decision: keep or remove based on whether label-to-label access pattern is still needed

Steps 3-7 are independent of each other (all depend only on step 1), so they could be implemented in any order. The suggested order (create before update, scalar before vector before set) follows complexity progression.

## Files Modified vs Created

| File | Action | What Changes |
|------|--------|-------------|
| `src/database_impl.h` | MODIFY | Add `resolve_fk_label()` method to Impl struct |
| `src/database_create.cpp` | MODIFY | Add scalar/vector FK resolution; refactor set FK resolution to use helper |
| `src/database_update.cpp` | MODIFY | Add scalar/vector/set FK resolution in `update_element` |
| `tests/test_database_create.cpp` | MODIFY | Add tests for scalar/vector FK create |
| `tests/test_database_update.cpp` | MODIFY | Add tests for scalar/vector/set FK update |

**No new files.** No header changes needed in `database.h`, `schema.h`, `type_validator.h`, `element.h`, or any C API/binding files.

## Sources

- `src/database_create.cpp` -- existing set FK resolution pattern (lines 151-167)
- `src/database_update.cpp` -- current update_element with no FK resolution
- `src/type_validator.cpp` -- already permits string->INTEGER (line 43-44)
- `include/quiver/schema.h` -- ForeignKey struct with from_column, to_table, to_column
- `src/database_impl.h` -- Impl struct where helper will live
- `src/database_relations.cpp` -- existing update_scalar_relation for comparison
- `tests/schemas/valid/relations.sql` -- test schema with scalar, vector, and set FK columns
- `.planning/PROJECT.md` -- milestone scope and constraints
