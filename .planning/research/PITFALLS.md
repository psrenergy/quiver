# Domain Pitfalls

**Domain:** FK label resolution in a multi-layer SQLite wrapper (C++ -> C API -> Julia/Dart/Lua)
**Researched:** 2026-02-23
**Overall confidence:** HIGH (based on direct codebase analysis, not external sources)

## Critical Pitfalls

Mistakes that cause rewrites, data corruption, or multi-layer breakage.

### Pitfall 1: TypeValidator Allows Strings for ALL INTEGER Columns, Not Just FK Columns

**What goes wrong:** The `TypeValidator::validate_value()` at `src/type_validator.cpp:42-48` already permits `std::string` for `DataType::Integer` columns with the comment "FK label resolution." This is a blanket bypass -- it allows string values for ANY integer column, not just FK columns. After adding FK label resolution to `create_element` and `update_element`, non-FK integer columns (like `some_integer` in `collections.sql`) will pass type validation when given a string, then fail at SQLite INSERT time with a confusing type mismatch error instead of a clean TypeValidator error.

**Why it happens:** The TypeValidator has no FK awareness. It only knows `(table, column) -> DataType`. The FK metadata lives in `Schema::TableDefinition::foreign_keys`, which TypeValidator does not consult.

**Consequences:**
- A user passes `element.set("some_integer", std::string("not_a_fk"))` for a non-FK INTEGER column.
- TypeValidator says OK (string allowed for INTEGER).
- No FK resolution runs (column is not a FK).
- SQLite INSERT fails with `SQLITE_MISMATCH` because the schema is STRICT and a TEXT value was bound to an INTEGER column.
- Error message is a raw SQLite error, not the clean Quiver error pattern.

**Prevention:** FK resolution must happen between validation and INSERT. Two approaches:
1. **(Recommended)** Keep the TypeValidator blanket string-for-INTEGER rule, but resolve labels immediately after validation and before building SQL. If a string is passed for an INTEGER column that is NOT a FK, the resolution step should throw `"Cannot create_element: attribute 'some_integer' is INTEGER but received string and is not a foreign key"`. This way TypeValidator stays simple (no FK awareness), and resolution is the gatekeeper.
2. (Alternative) Make TypeValidator FK-aware by injecting schema FK metadata. Rejected because it adds complexity to a deliberately simple class.

**Detection:** Test case: pass a string value for a non-FK INTEGER column. If it produces a SQLite error instead of a Quiver error, the pitfall is present.

---

### Pitfall 2: Scalar FK Resolution Missing from create_element While Set FK Resolution Exists

**What goes wrong:** The existing code at `src/database_create.cpp:151-167` resolves FK labels for set tables. But the scalar path (lines 17-39) and vector path (lines 83-117) do NOT resolve FK labels. If a developer copies the set FK pattern to scalars/vectors but places resolution AFTER the value is already bound into the SQL params vector, the string value gets inserted directly into SQLite, violating the STRICT INTEGER constraint.

**Why it happens:** The set FK resolution happens inline during the per-row insertion loop, checking `table_def->foreign_keys` and converting values in-place. The scalar path has no equivalent loop structure -- it builds params from `scalars()` directly. The vector path similarly builds params directly from `(*values_ptr)[row_idx]`.

**Consequences:** If the resolution logic is placed wrong (e.g., after `params.push_back(value)` in the scalar section), the original string value is what gets bound to the SQL statement. SQLite STRICT mode will reject it.

**Prevention:** For scalars, resolve FK labels in a separate pass AFTER TypeValidator but BEFORE building the INSERT SQL:
```cpp
// After validation loop, before INSERT building:
auto resolved_scalars = resolve_fk_labels(collection, scalars);
// Then use resolved_scalars to build INSERT
```
For vectors, apply the same pattern as the existing set FK resolution -- check `table_def->foreign_keys` inside the per-row loop and mutate `val` before pushing to params.

**Detection:** Integration test: `create_element` with a scalar FK column set to a string label. Verify the stored value is the resolved integer ID, not the string.

---

### Pitfall 3: update_element Scalar Path Passes Unresolved Labels Directly to UPDATE SQL

**What goes wrong:** In `src/database_update.cpp:21-44`, the scalar update builds `UPDATE ... SET col = ? WHERE id = ?` and pushes values directly from `element.scalars()`. If a user sets a FK column to a string label, the unresolved string goes into the UPDATE params. Unlike `create_element` which at least has the set FK resolution pattern to copy from, `update_element` has zero FK resolution anywhere in its code.

**Why it happens:** `update_element` was written for scalar/vector/set data without FK awareness. The relation path (`update_scalar_relation`) was a separate method with its own label resolution.

**Consequences:**
- SQLite STRICT mode rejects the string for an INTEGER FK column.
- Even if STRICT mode were off, the FK column would contain a string label instead of an integer ID, corrupting relational integrity silently.

**Prevention:** Add FK resolution to `update_element`'s scalar path identically to the `create_element` approach -- resolve between validation and SQL building. The vector and set paths in `update_element` (lines 75-131) need the same treatment as their `create_element` counterparts.

**Detection:** Test: `update_element` with a FK scalar column set to a target label. Verify `read_scalar_integer_by_id` returns the resolved ID.

---

### Pitfall 4: Inconsistent Resolution Placement Creates Partial-Write on Error

**What goes wrong:** If FK resolution for scalars happens during INSERT/UPDATE execution (inline), but the resolution fails (target label not found) AFTER some scalars have already been written, the element is in a partially-written state. While the `TransactionGuard` will rollback, this is only safe if the guard owns the transaction. If the user has an explicit `begin_transaction()` active, the `TransactionGuard` becomes a no-op (see `database_impl.h:105-109`), and the partial write persists until the user commits or rolls back.

**Why it happens:** The nest-aware `TransactionGuard` correctly avoids double-beginning. But this means that within a user transaction, a failed FK resolution in `create_element` does NOT automatically rollback -- the exception propagates and the user's transaction continues with partial data.

**Consequences:** Inside `db.begin_transaction()` / `db.commit()`:
1. `create_element("Child", child1)` succeeds
2. `create_element("Child", child2_with_bad_fk_label)` throws on FK resolution
3. User catches exception, calls `db.commit()` -- child1 was inserted but child2 was not
4. This is CORRECT behavior (user controls transaction), but ONLY if the failed `create_element` wrote nothing. If resolution fails mid-INSERT (after scalar row is written but before vector FK is resolved), the scalar row persists.

**Prevention:** Resolve ALL FK labels (scalar, vector, set) BEFORE any SQL execution, not inline during execution. This ensures resolution failure means zero writes attempted. The pattern should be:
```
1. Validate types
2. Resolve all FK labels (throw if any fail)
3. Build and execute SQL with resolved values
```
This makes the operation atomic even within a user transaction: either all resolution succeeds and all writes happen, or no writes happen at all.

**Detection:** Test within explicit transaction: create_element with valid scalars + vector with bad FK label. Verify that the scalar row was NOT written after the exception.

---

### Pitfall 5: Self-Referential FK Resolution Race in create_element

**What goes wrong:** The `relations.sql` schema has `Child.sibling_id REFERENCES Child(id)`. When creating a new Child element with `sibling_id` set to a label, the resolution does `SELECT id FROM Child WHERE label = ?`. But the element being created has not been inserted yet (we resolve BEFORE INSERT per Pitfall 4). If the user passes the element's own label as the FK target, the resolution will fail with "label not found" even though the target will exist after the current INSERT completes.

**Why it happens:** Self-referential FKs create a chicken-and-egg problem: the target must exist before resolution, but the target IS the element being created.

**Consequences:** Users cannot create elements with self-references in a single `create_element` call. They must:
1. `create_element("Child", child)` without the self-FK
2. `update_scalar_relation("Child", "sibling_id", "Child 1", "Child 1")`

**Prevention:** This is expected behavior, not a bug. Document it clearly: "Self-referential FK labels are resolved against existing data. To set a self-reference, create the element first, then update the FK column." The existing `update_scalar_relation` test at line 31 of `test_database_relations.cpp` demonstrates this two-step pattern already.

**Detection:** Test that creating a Child with `sibling_id` pointing to its own label throws the expected resolution error. Add to docs/error messages.

---

### Pitfall 6: update_element's Typed Update Methods Bypass FK Resolution Entirely

**What goes wrong:** The codebase has TWO paths for updating data:
1. `update_element(collection, id, element)` -- generic, takes Element builder
2. `update_scalar_integer(collection, attribute, id, value)` / `update_vector_strings(...)` / `update_set_strings(...)` -- typed, take raw values

The typed methods at `database_update.cpp:166-353` accept raw typed values and write them directly. They have no FK awareness at all. If `update_scalar_relation` is removed (per PROJECT.md: "Remove update_scalar_relation if redundant after above changes"), users lose the only way to update FK columns outside of `update_element`.

**Why it happens:** `update_scalar_integer` accepts `int64_t` -- it is the correct type for FK columns after resolution. But there is no `update_scalar_by_label` method. The typed string methods (`update_vector_strings`, `update_set_strings`) accept strings, but these are the stored values, not labels-to-resolve.

**Consequences:** If `update_scalar_relation` is removed prematurely, users who want to update a single FK column must construct an Element with just that column and call `update_element`. This works but is verbose compared to `update_scalar_relation("Child", "parent_id", "Child 1", "Parent 2")`.

**Prevention:** Do NOT remove `update_scalar_relation` until `update_element` fully supports FK label resolution for all column types. Even then, consider keeping it as a convenience method since it uses label-to-label lookup (from_label -> to_label) which is a different access pattern than `update_element`'s id-based lookup.

**Detection:** After implementing FK resolution in `update_element`, verify the same operation can be done both ways and produces identical results.

## Moderate Pitfalls

### Pitfall 7: Binding-Layer Type Ambiguity -- String Could Mean Label or Value

**What goes wrong:** In Dart's `Element.set()` (`bindings/dart/lib/src/element.dart:57`), a `String` value is routed to `setString()` which calls `quiver_element_set_string`. In Julia's `Base.setindex!` (`bindings/julia/src/element.jl:36`), strings are similarly routed. For a FK INTEGER column like `parent_id`, the user would write:
- Dart: `createElement('Child', {'label': 'Child 1', 'parent_id': 'Parent 1'})`
- Julia: `create_element!(db, "Child", label="Child 1", parent_id="Parent 1")`

Both pass the string through Element's string setter, which stores it as `std::string` in the `Value` variant. This is correct for FK resolution. BUT if a user passes an integer for a FK column:
- Dart: `createElement('Child', {'label': 'Child 1', 'parent_id': 42})`
- Julia: `create_element!(db, "Child", label="Child 1", parent_id=42)`

This stores `int64_t(42)` in the variant. TypeValidator accepts it (INTEGER for INTEGER). No FK resolution runs. The raw ID 42 is inserted. This is ALSO correct -- power users should be able to pass raw IDs.

The pitfall is that there is no way to distinguish "user passed ID intentionally" from "user passed ID because they didn't know about label resolution." This is fundamentally acceptable but must be documented.

**Prevention:** Document the dual-input contract clearly: FK columns accept either a string label (resolved to ID) or an integer ID (passed through directly). This is the natural behavior of the `Value` variant dispatch. No code changes needed, just clear documentation.

---

### Pitfall 8: Per-Row Label Lookups in Vectors/Sets Are N+1 Query Performance Traps

**What goes wrong:** The existing set FK resolution at `database_create.cpp:156-157` executes `SELECT id FROM {table} WHERE label = ?` for EVERY row in the array. A vector FK column with 1000 elements means 1000 separate SELECT queries. SQLite prepared statements help, but the overhead is still O(N) queries.

**Why it happens:** The resolution happens inside the per-row insertion loop. Each row independently resolves its label.

**Consequences:** Creating elements with large FK vector/set arrays is slow. For 100 elements with 100 FK references each, that is 10,000 label lookups.

**Prevention:** Two-phase approach:
1. **Phase 1 (now):** Resolve labels in a single batch before the insertion loop. Collect all unique labels, execute ONE `SELECT id, label FROM {table} WHERE label IN (?, ?, ...)` query, build a label-to-id map, then use the map during insertion. This reduces N lookups to 1 lookup per unique target table.
2. **Phase 2 (future, if needed):** Prepare the lookup statement once and reuse it. SQLite prepared statement reuse is negligible overhead for small N but matters at scale.

For the current milestone, batch resolution is worth implementing because the pattern is simple and the set FK code already proves the concept.

**Detection:** Benchmark: create element with 1000-element FK vector. Compare time with batch resolution vs per-row resolution.

---

### Pitfall 9: NULL Handling for Optional FK Columns

**What goes wrong:** FK columns like `parent_id` in `relations.sql` are nullable (`ON DELETE SET NULL` implies nullable). Users can pass `nullptr` (via `element.set_null("parent_id")`) for optional FK columns. The TypeValidator allows NULL for any type (line 29). But if FK resolution code checks `std::holds_alternative<std::string>(val)` to decide whether to resolve, it must also handle the `nullptr` case by passing it through unchanged.

If the resolution code only has branches for `string` (resolve) and `int64_t` (pass through), a `nullptr` value will fall through without matching either branch and be passed directly to the INSERT. This is correct for SQLite (NULL is valid), but only if the column actually allows NULL.

**Why it happens:** The Value variant has four alternatives: `nullptr_t`, `int64_t`, `double`, `std::string`. Resolution logic must handle all four for FK columns.

**Consequences:** If resolution code throws for unexpected types (like double for an FK column), this is correct error handling. But if it silently passes nullptr without checking the column's NOT NULL constraint, users might insert NULL into a NOT NULL FK column and get a confusing SQLite constraint error instead of a clean Quiver error.

**Prevention:** In the resolution step, for each FK column:
- `std::string` -> resolve label to ID
- `int64_t` -> pass through (raw ID)
- `nullptr_t` -> pass through (NULL, will be caught by SQLite NOT NULL if applicable)
- `double` -> throw "Cannot resolve FK: expected string label or integer ID, got REAL"

The existing set FK pattern at `database_create.cpp:153` only checks `std::holds_alternative<std::string>` and passes non-strings through. This is correct because TypeValidator already rejected doubles for INTEGER columns. The only values that reach the FK check for an INTEGER FK column are `string`, `int64_t`, and `nullptr_t` -- and the first is resolved, the other two pass through correctly.

**Detection:** Test: create element with `set_null("parent_id")`. Verify NULL is stored. Test: create element with `set("parent_id", 3.14)` on a FK column. Verify TypeValidator rejects it (it does -- double is allowed for INTEGER columns, so this is actually accepted by the current TypeValidator). This reveals that `double` values for FK columns are a gray area -- `TypeValidator` allows `double` for `INTEGER` but FK resolution does not handle it. Need to decide: reject double for FK INTEGER columns, or truncate to int64_t?

---

### Pitfall 10: Binding Parity -- All Five Layers Must Handle the Same Input Types

**What goes wrong:** The C++ `Element::set(name, string)` stores a string value for any column. The C API `quiver_element_set_string` does the same. But in Julia, `el[name] = value` dispatches on Julia type: `Integer` goes to `set_integer`, `String` goes to `set_string`. In Dart, `Element.set()` similarly dispatches on Dart type. These dispatch rules were designed before FK label resolution existed.

If a Julia user writes `create_element!(db, "Child", label="Child 1", parent_id="Parent 1")`, Julia dispatches `"Parent 1"` through `Base.setindex!(el, value::String, name)` which calls `quiver_element_set_string`. This correctly stores a string variant. The C++ resolution code sees the string variant for the FK column and resolves it. This works.

BUT: What about the `update_element!` path? Julia's `update_element!` and Dart's `updateElement` call the same C API `quiver_database_update_element`. The Element construction in bindings is identical for create and update. So IF FK resolution works in `create_element`, it should automatically work in `update_element` too -- the Element arrives with the same variant type. No binding changes needed.

The pitfall is assuming bindings need changes. They do NOT need changes for basic FK resolution because the string-for-FK-column pattern already flows through the Element builder correctly. The resolution happens in C++, not in bindings.

**Prevention:** Do NOT add FK resolution logic to bindings. Keep it exclusively in C++ (`database_create.cpp` and `database_update.cpp`). The bindings are thin wrappers -- they pass values through and let C++ handle semantics. This is already the project philosophy ("Intelligence: Logic resides in C++ layer. Bindings/wrappers remain thin.").

**Detection:** After implementing C++ FK resolution, verify all binding tests pass without binding code changes. If they need changes, something is wrong with the C++ implementation.

---

### Pitfall 11: Error Message Inconsistency Between update_scalar_relation and create_element FK Resolution

**What goes wrong:** The existing `update_scalar_relation` at `database_relations.cpp:33` throws:
```
"Target element not found: 'X' in collection 'Y'"
```
The existing set FK resolution in `create_element` at `database_create.cpp:159` throws:
```
"Failed to resolve label 'X' to ID in table 'Y'"
```
These are different error message patterns for the same conceptual operation (resolving a label to an ID). PROJECT.md specifies the error pattern should be `"Failed to resolve label 'X' to ID in table 'Y'"`.

**Why it happens:** `update_scalar_relation` was written separately from the set FK resolution, and the error messages diverged.

**Consequences:** Bindings or downstream code that parses error messages to distinguish error types will be confused by inconsistent patterns.

**Prevention:** Adopt a single FK resolution helper function:
```cpp
int64_t resolve_label_to_id(const std::string& table, const std::string& label);
```
Used by BOTH `create_element`/`update_element` FK resolution AND `update_scalar_relation`. Single error message format: `"Failed to resolve label 'X' to ID in table 'Y'"`.

**Detection:** Grep for all label resolution error messages and verify consistency.

## Minor Pitfalls

### Pitfall 12: Schema with FK to Non-Label Column

**What goes wrong:** The current FK resolution assumes the target table has a `label` column and resolves via `SELECT id FROM {table} WHERE label = ?`. If a future schema has a FK that references a table without a `label` column, or references a non-`id` column, resolution silently fails.

**Prevention:** The `SchemaValidator::validate_collection` already requires every collection to have a `label TEXT UNIQUE NOT NULL` column. This is enforced at schema load time. So all FK targets that are collections are guaranteed to have `label`. However, FK targets could theoretically be non-collection tables. The SchemaValidator FK validation at `schema_validator.cpp:294-361` does not require FK targets to be collections. Add a check: if a FK column's target table is not a collection (no `label` column), FK label resolution should throw a clear error at resolution time, not produce a confusing SQL error.

---

### Pitfall 13: Label Uniqueness is Enforced by Schema but Not by Resolution Code

**What goes wrong:** Labels are `UNIQUE NOT NULL` in the schema. Resolution returns the first result from `SELECT id FROM {table} WHERE label = ?`. This always returns 0 or 1 rows because of the UNIQUE constraint. If the UNIQUE constraint were somehow absent (schema validator bug, manual schema manipulation), resolution could return multiple rows and silently use the first one.

**Prevention:** The `SchemaValidator::validate_collection` at `schema_validator.cpp:91-100` already enforces UNIQUE. This pitfall is theoretical. No extra code needed, but resolution code should assert single-row result for defensive logging in debug builds.

---

### Pitfall 14: Lua Binding create_element Uses Lua Tables Which Have Implicit Type Coercion

**What goes wrong:** In Lua, all numbers are doubles (in standard Lua) or can be integers (in LuaJIT/Lua 5.3+). When a Lua user passes `{parent_id = "Parent 1"}`, the string flows correctly. But if they pass `{parent_id = 42}`, sol2 may marshal it as `double` or `int64_t` depending on the value and Lua version. If it arrives as `double`, the TypeValidator allows it (double allowed for INTEGER), and no FK resolution runs (not a string), but the double value may lose precision for large IDs.

**Prevention:** The existing Lua binding `create_element_from_lua` in `lua_runner.cpp` handles type dispatch. Verify that integer values from Lua tables arrive as `int64_t` in the Value variant, not `double`. This is a sol2 configuration concern, not a FK resolution concern.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| TypeValidator modification | Pitfall 1: Strings accepted for all INTEGER columns, not just FK | Resolve after validation, reject string-for-non-FK-INTEGER at resolution step |
| create_element scalar FK | Pitfall 2: Must add resolution BEFORE param building, not inline | Separate resolve pass, then build SQL with resolved values |
| create_element vector FK | Pitfall 8: N+1 lookups per vector element | Batch resolve: collect unique labels, single SELECT, build map |
| update_element all paths | Pitfall 3: Zero FK resolution exists, add to scalar + vector + set | Mirror create_element's approach exactly |
| Transaction safety | Pitfall 4: Resolve ALL labels before ANY writes | Pre-resolve pattern: validate -> resolve -> execute |
| Self-referential FK | Pitfall 5: Cannot resolve label that does not exist yet | Document as expected behavior, two-step pattern |
| update_scalar_relation removal | Pitfall 6: Do not remove until update_element is fully functional | Keep until all paths tested |
| Binding layer changes | Pitfall 10: Bindings do NOT need FK logic | Verify by running existing binding tests after C++ changes |
| Error messages | Pitfall 11: Inconsistent patterns for same error | Extract shared helper function |
| NULL FK values | Pitfall 9: Handle all Value variants in resolution | Check string -> resolve, int64 -> pass, null -> pass, double -> validate already rejects |

## Recommended Resolution Architecture

Based on the pitfalls above, the safest implementation pattern is:

```
create_element / update_element flow:
  1. TypeValidator validates all values (existing code)
  2. NEW: resolve_fk_labels() pass
     - For each scalar: check if column has FK, if value is string, resolve
     - For each array column: check if table has FK on that column, if values contain strings, batch resolve
     - Throws on: missing label, string for non-FK INTEGER column
     - Passes through: int64_t (raw ID), nullptr (NULL), non-FK values
  3. Build and execute SQL with resolved values (existing code, using resolved values)
```

This pattern:
- Keeps TypeValidator simple (no FK awareness)
- Ensures all resolution happens before any writes
- Makes the operation atomic within user transactions (fail before any writes)
- Requires no binding layer changes
- Uses a shared helper function for consistent error messages

## Sources

- All findings based on direct codebase analysis
- `src/type_validator.cpp` -- TypeValidator implementation
- `src/database_create.cpp` -- create_element with existing set FK resolution
- `src/database_update.cpp` -- update_element without any FK resolution
- `src/database_relations.cpp` -- update_scalar_relation with separate resolution
- `src/database_impl.h` -- TransactionGuard nest-aware behavior
- `include/quiver/schema.h` -- ForeignKey struct, TableDefinition
- `src/schema_validator.cpp` -- Schema validation rules
- `tests/schemas/valid/relations.sql` -- FK schema patterns
- `bindings/julia/src/element.jl` -- Julia type dispatch
- `bindings/dart/lib/src/element.dart` -- Dart type dispatch
- `.planning/PROJECT.md` -- Milestone context and constraints
