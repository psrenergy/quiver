# Feature Landscape

**Domain:** FK Label Resolution in SQLite Wrapper Library (Quiver)
**Researched:** 2026-02-23
**Confidence:** HIGH -- based on direct codebase analysis + ORM ecosystem patterns

## Ecosystem Context: How ORMs Handle FK Resolution

FK label resolution (accepting a human-readable name instead of a numeric ID) is not a standard feature in most ORMs. The dominant patterns in the wild are:

1. **Object assignment (ActiveRecord, Django, SQLAlchemy):** Callers assign a related *object* (not a label). The ORM extracts the PK from the object. In Rails: `child.parent = parent_object`. In Django: `Book.objects.create(author=author_instance)`. The caller already has the object in memory.

2. **ID assignment (all ORMs):** Callers provide the raw FK ID directly. In Django: `Book.objects.create(author_id=42)`. In SQLAlchemy: `book.author_id = 42`.

3. **Natural key serialization (Django only):** Django's `natural_key()` is used exclusively for *serialization/deserialization* (fixtures, data loading), not for general CRUD. It maps a tuple of fields to an object, e.g., `get_by_natural_key("Physics")`.

Quiver's approach -- accepting a string label and transparently resolving it to an integer FK ID at write time -- is **novel for a low-level wrapper**. It sits between the "caller does lookup manually" approach and a full ORM's object graph. This is appropriate because Quiver's Element builder uses `Value` variants (not object references), and the `label TEXT UNIQUE NOT NULL` convention guarantees every collection has a natural key available.

**Key insight:** Because Quiver already has `label UNIQUE NOT NULL` on every collection, FK-by-label is a natural extension of the API. ORMs don't do this because they lack a universal natural key convention. Quiver has one.

## Table Stakes

Features that must work for the milestone to be complete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Scalar FK resolution in `create_element` | Core use case: `element.set("parent_id", "Parent 1")` instead of looking up ID manually | Low | Apply existing set FK pattern to scalar path; requires detecting FK columns in the scalar insert loop (lines 29-38 of database_create.cpp) |
| Vector FK resolution in `create_element` | Consistency: vector FK columns like `parent_ref` in `Child_vector_refs` must resolve labels | Low | Apply existing set FK pattern to vector insert loop (lines 102-117); iterate `table_def->foreign_keys` per value |
| Scalar FK resolution in `update_element` | Symmetric with create: `update_element` scalars must also accept labels for FK columns | Low | Same pattern in update's scalar UPDATE path (lines 21-44 of database_update.cpp); resolve before building SET clause |
| Vector FK resolution in `update_element` | Symmetric with create: vector FK updates must resolve labels | Low | Same pattern in update's vector INSERT path (lines 88-100) |
| Set FK resolution in `update_element` | `update_element` set path currently lacks FK resolution (unlike `create_element` set path) | Low | Copy the proven pattern from create's set path (lines 151-167 of database_create.cpp) into update's set path (lines 118-131) |
| Error on missing target label | Fail-fast: `"Failed to resolve label 'X' to ID in table 'Y'"` when label doesn't exist | Low | Already implemented in set FK path; use same error message pattern |
| NULL FK value passthrough | FK columns are nullable (`parent_id INTEGER` without NOT NULL); `set_null("parent_id")` or omitting the field must continue working | Low | `nullptr_t` variant skips FK resolution naturally; `std::holds_alternative<std::string>(val)` guard already prevents resolution of non-string values |
| TypeValidator already permits strings for INTEGER columns | `validate_value` at line 43-48 of type_validator.cpp allows `std::string` for `DataType::Integer` -- comment says "FK label resolution" | None | Already handled. No changes needed to TypeValidator. |
| Binding parity (C API) | C API `quiver_database_create_element` / `quiver_database_update_element` already pass through Element's Value variant | None | No C API changes needed -- string values flow through existing marshaling |
| Binding parity (Julia, Dart, Lua) | All bindings already accept string values in Element builder for any column | None | No binding changes needed -- strings are already a valid Value type |

## Differentiators

Features that go beyond minimum requirements but add significant value.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Extract shared `resolve_fk_value` helper | Eliminates 5x code duplication across create/update scalar/vector/set paths | Low | Private helper: `Value resolve_fk_value(const TableDefinition& table, const std::string& col, Value val)`. Returns val unchanged if not an FK or not a string, otherwise resolves. ~15 lines. |
| Remove `update_scalar_relation` if redundant | API simplification -- one fewer method to maintain across all 5 layers (C++, C, Julia, Dart, Lua) | Medium | Must verify no use case remains. `update_scalar_relation` uses label-to-label lookup (from_label to to_label). `update_element` uses id-to-label. They serve different access patterns. See analysis below. |

### Analysis: Should `update_scalar_relation` be removed?

**Current `update_scalar_relation` signature:** `(collection, attribute, from_label, to_label)` -- locates source row by `from_label`, target row by `to_label`, updates FK column.

**Current `update_element` signature:** `(collection, id, element)` -- locates source row by `id`, FK values resolved from label in Element.

**Overlap:** If caller has the element's ID (common after `create_element` returns it), `update_element` with FK resolution covers the same use case.

**Gap:** If caller only has the source element's *label* (not its ID), `update_scalar_relation` is more convenient -- no need to query for the ID first. However, this is a thin convenience; callers could use `query_integer("SELECT id FROM X WHERE label = ?", {"label"})` to get the ID.

**Recommendation:** Defer removal. Mark `update_scalar_relation` as a candidate for future deprecation, but do not remove it in this milestone. The label-to-label access pattern is genuinely different and removing it would be a breaking change for existing users.

## Anti-Features

Features to explicitly NOT build.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| FK resolution cache | Premature optimization; SQLite page cache handles repeated lookups; element counts are 10s-1000s not millions | Trust SQLite's built-in page caching within the transaction |
| Batch label resolution (single query for all labels in an array) | More complex SQL (`WHERE label IN (...)`), harder to produce clear per-label error messages, marginal perf gain | Resolve one label at a time inside the wrapping transaction |
| FK resolution in typed update methods (`update_scalar_integer`, `update_vector_integers`, etc.) | These methods accept typed values (`int64_t`, `double`); callers who use them already have IDs. Adding string overloads would break the type contract. | Only resolve in `create_element` / `update_element` which accept the `Value` variant |
| Reverse FK resolution (ID-to-label at write time) | Different concern entirely; `read_scalar_relation` already handles the read direction | Keep existing `read_scalar_relation` unchanged |
| FK resolution in time series columns | No current schema uses FK in time series tables; PROJECT.md explicitly marks this out of scope | Defer to future milestone if any schema requires it |
| New `Schema::is_fk_column()` method | The detection pattern is 2 lines: iterate `foreign_keys`, compare `from_column`. Used 5 times but always with immediate access to `fk.to_table` for the lookup query. A helper would split detection from usage, making the code less readable. | Keep inline FK detection. If a helper is extracted, make it `resolve_fk_value` which combines detection + resolution. |
| FK resolution in query methods (`query_string`, etc.) | Queries are raw SQL; users who write SQL can JOIN themselves. Adding implicit resolution would be surprising. | Keep query methods as pure SQL execution |
| "Find or create" semantics for FK targets | Some ORMs auto-create missing related objects. This is implicit and dangerous for a low-level wrapper -- silent data creation is worse than a clear error. | Throw on missing target label (fail-fast) |
| Cascading label updates (re-resolve when target label changes) | SQLite's `ON UPDATE CASCADE` handles FK integrity at the ID level. Label changes don't affect stored IDs. | Not applicable -- FK stores the integer ID, which doesn't change when labels change |

## Edge Cases

Detailed analysis of edge cases that the implementation must handle correctly.

### 1. NULL FK Values

**Scenario:** `element.set_null("parent_id")` or simply omitting `parent_id` from the Element.

**Expected behavior:** NULL is stored directly. No FK resolution attempted.

**Current handling:** The `Value` variant holds `nullptr_t`. The guard `std::holds_alternative<std::string>(val)` is false, so the FK resolution block is skipped. This works correctly today in the set path and will work in all new paths.

**Risk:** None. The pattern is proven.

### 2. Self-Referential FKs

**Scenario:** `Child.sibling_id` references `Child(id)`. Creating or updating a Child that references another Child by label.

**Example:**
```cpp
Element child2;
child2.set("label", "Child 2").set("sibling_id", "Child 1");  // Self-ref FK
db.create_element("Child", child2);
```

**Expected behavior:** Resolve "Child 1" to its ID in the `Child` table, store that ID in `sibling_id`.

**Current handling:** The FK resolution pattern is table-agnostic -- it uses `fk.to_table` from the schema's `foreign_keys` list. For self-references, `fk.to_table` equals the collection name. The lookup SQL `SELECT id FROM Child WHERE label = ?` works correctly.

**Risk:** LOW. The only subtle case is creating an element that references *itself* (circular). This is impossible because the element doesn't exist yet at INSERT time, so the label lookup would fail with "Failed to resolve label 'X' to ID in table 'Y'", which is the correct behavior.

### 3. Integer Value for FK Column

**Scenario:** Caller passes `element.set("parent_id", int64_t{42})` -- already has the ID.

**Expected behavior:** Store 42 directly. No FK resolution.

**Current handling:** The guard `std::holds_alternative<std::string>(val)` is false for `int64_t`, so FK resolution is skipped. The integer flows directly into the INSERT/UPDATE SQL.

**Risk:** None.

### 4. String That Looks Like a Number for FK Column

**Scenario:** Caller passes `element.set("parent_id", std::string("42"))` for an INTEGER FK column.

**Expected behavior:** Treat "42" as a label, attempt to look up a row with `label = '42'` in the target table. If found, use that row's ID. If not found, throw.

**Current handling:** This is correct behavior. The string type triggers FK resolution. If someone has an element labeled "42", it resolves correctly. If not, it fails clearly.

**Risk:** LOW. This is a rare edge case but the behavior is correct and consistent.

### 5. FK Target Has No Matching Label

**Scenario:** `element.set("parent_id", "Nonexistent Parent")` where no Parent with that label exists.

**Expected behavior:** Throw `"Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'"`.

**Current handling:** The existing set FK pattern (database_create.cpp:158-160) handles this correctly.

**Risk:** None.

### 6. Multiple FK Columns in Same Table

**Scenario:** `Child` has both `parent_id` (references Parent) and `sibling_id` (references Child). Element sets both.

**Expected behavior:** Each FK column is resolved independently against its own target table.

**Current handling:** The loop iterates over `table_def->foreign_keys` for each column. Each column matches at most one FK entry. The `break` after finding a match ensures correct 1:1 mapping.

**Risk:** None.

### 7. Vector/Set with Mixed FK and Non-FK Columns

**Scenario:** `Child_vector_refs` has `parent_ref` (FK to Parent) and potentially other non-FK value columns in the same row.

**Expected behavior:** Only FK columns are resolved; non-FK columns pass through unchanged.

**Current handling:** The FK check `fk.from_column == col_name` only triggers for columns that are actually foreign keys. Other columns skip the resolution block entirely.

**Risk:** None.

### 8. Cascading Deletes/Updates (SQLite Level)

**Scenario:** A Parent is deleted. Children with `ON DELETE SET NULL` have their FK set to NULL. Children with `ON DELETE CASCADE` are deleted.

**Expected behavior:** This is entirely handled by SQLite's FK constraint enforcement. Quiver's FK resolution only runs at INSERT/UPDATE time. Cascading effects are SQLite's responsibility.

**Risk:** None for FK resolution. But worth noting: if a caller creates elements, then deletes a parent, then reads, the FK columns will reflect the cascade action. This is standard database behavior.

### 9. Transaction Atomicity

**Scenario:** Element has 3 set FK values. The first two resolve successfully, the third fails.

**Expected behavior:** The entire `create_element` / `update_element` call rolls back. No partial writes.

**Current handling:** Both methods wrap all operations in `TransactionGuard`. If any exception is thrown (including FK resolution failure), the destructor rolls back the transaction.

**Risk:** None. The pattern is proven.

### 10. Vector FK Column: `id` Column vs Data FK Column

**Scenario:** In `Child_vector_refs`, the `id` column is an FK to `Child(id)` (standard parent reference). The `parent_ref` column is a data FK to `Parent(id)`.

**Expected behavior:** Only `parent_ref` should undergo label-to-ID resolution. The `id` column is the element's own ID (passed as a parameter, not through the Element builder).

**Current handling:** The `id` column is never part of the Element's arrays -- it's injected directly from `element_id` in the INSERT SQL. The FK resolution loop only processes columns from the Element's arrays, so `id` is never considered.

**Risk:** None.

## Feature Dependencies

```
Scalar FK in create_element  --> (none, independent)
Vector FK in create_element  --> (none, independent)
Scalar FK in update_element  --> (none, independent)
Vector FK in update_element  --> (none, independent)
Set FK in update_element     --> (none, independent)
Extract shared helper        --> depends on at least 2 paths implemented to identify common pattern
Remove update_scalar_relation --> depends on all FK resolution paths complete + verification
```

All five FK resolution paths are independent of each other. They can be implemented in any order. The implementation pattern is identical in every case:

```
For each column in the row being inserted/updated:
    For each FK definition in the table:
        If FK.from_column == column_name AND value is a string:
            Resolve label to ID via SELECT
            Replace string value with integer ID
            Break (a column has at most one FK)
```

The helper extraction should come after at least two paths are done to confirm the shared pattern. Removal of `update_scalar_relation` is the final step and may be deferred.

## MVP Recommendation

Prioritize:
1. **Scalar FK in create_element** -- highest-value path; most common FK pattern (e.g., `element.set("parent_id", "Parent 1")`)
2. **Scalar FK in update_element** -- symmetric with create; validates the pattern works in both directions
3. **Vector FK in create_element** -- extends pattern to vector tables (already proven in set tables)
4. **Vector FK in update_element** -- symmetric with create
5. **Set FK in update_element** -- completes the matrix (create_element set FK already works)
6. **Extract shared helper** -- consolidate after all 5 paths work

Defer: **Remove update_scalar_relation** -- verify redundancy after all paths work; the label-to-label access pattern is genuinely different from the id-to-label pattern used by `update_element`.

## Sources

- `C:\Development\DatabaseTmp\quiver3\.planning\PROJECT.md` -- active/out-of-scope feature list, key decisions
- `C:\Development\DatabaseTmp\quiver3\src\database_create.cpp` -- existing set FK resolution (lines 151-167), scalar insert (lines 24-41), vector insert (lines 83-118)
- `C:\Development\DatabaseTmp\quiver3\src\database_update.cpp` -- current update_element (lines 7-164), typed update methods (lines 166-353)
- `C:\Development\DatabaseTmp\quiver3\src\database_relations.cpp` -- update_scalar_relation (label-to-label pattern), read_scalar_relation
- `C:\Development\DatabaseTmp\quiver3\src\type_validator.cpp` -- string-to-INTEGER allowance for FK resolution (line 43-48)
- `C:\Development\DatabaseTmp\quiver3\include\quiver\schema.h` -- ForeignKey struct, TableDefinition with foreign_keys vector
- `C:\Development\DatabaseTmp\quiver3\tests\schemas\valid\relations.sql` -- FK schema patterns (scalar, vector, set, self-referential)
- [Basic Relationship Patterns -- SQLAlchemy 2.0](https://docs.sqlalchemy.org/en/20/orm/basic_relationships.html) -- ORM relationship patterns for context
- [Active Record Associations -- Ruby on Rails Guides](https://guides.rubyonrails.org/association_basics.html) -- ActiveRecord object assignment pattern
- [How to Use a Foreign Key in Django](https://www.freecodecamp.org/news/how-to-use-a-foreign-key-in-django/) -- Django FK assignment patterns
