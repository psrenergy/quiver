# Phase 1: Foundation - Research

**Researched:** 2026-02-23
**Domain:** C++ internal refactoring -- extract shared FK label resolution helper, refactor existing set FK path, add non-FK INTEGER guard
**Confidence:** HIGH

## Summary

Phase 1 is a focused extraction and refactoring task. The existing inline FK label resolution code in `database_create.cpp:151-167` (set table path) already implements the correct algorithm: check if a column has an FK definition, check if the value is a string, look up the target table's `label` column to resolve to an integer ID, throw on missing label. The task is to extract this into a reusable `resolve_fk_label` method on `Database::Impl`, refactor the set path to use it, and add a guard that rejects strings passed to non-FK INTEGER columns with a clear Quiver error.

No new libraries, no new files (only modifications to existing files), no C API changes, no binding changes. Every building block exists: `TableDefinition::foreign_keys` provides FK metadata, `TypeValidator` already allows strings for INTEGER columns (with an explicit "FK label resolution" comment), and `Database::execute()` provides the parameterized SQL execution needed for the lookup query. The helper method lives on `Database::Impl` because it needs access to `execute()` (private) and must be shared across `database_create.cpp` and `database_update.cpp` (Phase 2/3).

The key risk is that the TypeValidator currently allows strings for ALL INTEGER columns, not just FK columns. After extracting the helper, a pre-resolve pass must detect strings on non-FK INTEGER columns and throw a clear Quiver error rather than letting a raw SQLite STRICT mode error surface later during SQL execution.

**Primary recommendation:** Extract `resolve_fk_label` on `Database::Impl` in `database_impl.h`, replace the inline set FK block in `create_element` with a call to the helper, add a pre-resolve error for strings on non-FK INTEGER columns, and write dedicated tests in `test_database_relations.cpp`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Error fires during a **pre-resolve pass**, before any SQL writes -- consistent with the pre-resolve approach that Phase 2 will use
- Only catch strings in **non-FK INTEGER columns** -- REAL columns are out of scope
- Error message: **just the error**, no hints about what to do instead -- matches existing terse error style
- ERR-01 format (`"Failed to resolve label 'X' to ID in table 'Y'"`) is locked from requirements
- **No defensive validation** of the target table's label column -- assume schema convention (TEXT UNIQUE label) holds
- Labels are UNIQUE by schema construction -- the helper does not need to handle ambiguity
- Helper should be **general from the start** -- designed to work for any FK column type (scalar, vector, set, time series), not just what sets need
- Phase 2 and 3 should be able to just call the existing helper without extending its interface
- **New dedicated tests** for the resolve_fk_label helper in addition to verifying existing tests pass unchanged
- Tests go in **existing `test_database_relations.cpp`** -- FK resolution is conceptually part of relations
- Test cases should cover: valid label resolution, missing label error, non-FK INTEGER column error

### Claude's Discretion
- Exact error message wording for non-FK INTEGER columns (within the 3 established patterns)
- Whether to include collection context or FK column path in error messages
- FK detection as a separate method vs built into the resolve helper
- Internal implementation details of schema introspection for FK detection

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FOUND-01 | Shared `resolve_fk_label` helper extracts the proven set FK pattern into a reusable method on `Database::Impl` | Existing inline code at `database_create.cpp:151-167` provides the exact algorithm. Helper goes on `Impl` because it needs `Database::execute()` (private) and must be shared across translation units. See Architecture Pattern 1 below. |
| CRE-03 | Existing set FK resolution in `create_element` uses the shared helper (refactor, no behavior change) | Direct replacement of the 17-line inline block with a call to the new helper. Existing relation tests in `test_database_relations.cpp` and `test_database_create.cpp` validate no behavioral change. See Integration Point 1 below. |
| ERR-01 | Passing a label that doesn't exist in the target table throws `"Failed to resolve label 'X' to ID in table 'Y'"` | Error message format already exists in the inline set FK code at `database_create.cpp:159`. The helper preserves this exact format. Tested via dedicated test case for missing label. |
</phase_requirements>

## Standard Stack

### Core

No new libraries or dependencies. Phase 1 uses only existing infrastructure:

| Component | Location | Purpose | Status |
|-----------|----------|---------|--------|
| `TableDefinition::foreign_keys` | `include/quiver/schema.h:43` | FK metadata (from_column, to_table, to_column) for every table | Exists, populated at schema load via `PRAGMA foreign_key_list` |
| `TypeValidator::validate_value()` | `src/type_validator.cpp:42-48` | Already allows `std::string` for `DataType::Integer` with comment "FK label resolution" | Exists, no changes needed |
| `Database::execute()` | `src/database.cpp:130-210` | Parameterized SQL execution for the lookup `SELECT id FROM {table} WHERE label = ?` | Exists, already used by the inline set FK code |
| `Database::Impl` | `src/database_impl.h:16-128` | Struct where the new helper method will live; already has helper methods like `require_collection`, `require_column` | Exists, will be extended |
| `Result` / `Row` | `include/quiver/result.h`, `include/quiver/row.h` | Return type of `execute()`, provides `empty()`, `get_integer(0)` | Exists, already used in inline set FK code |

### Alternatives Considered

| Instead of | Could Use | Why Rejected |
|------------|-----------|--------------|
| Method on `Impl` | Free function in `database_create.cpp` | `update_element` in `database_update.cpp` needs the same function -- must be shared across translation units, so it belongs in the shared header `database_impl.h` |
| Method on `Impl` | Method on `Schema` or `TypeValidator` | Resolution needs `Database::execute()` for SQL lookup -- neither `Schema` nor `TypeValidator` has access to database execution. Would create circular dependency. |
| Combined FK detect + resolve | Separate `find_fk_target` + `resolve_label` | Over-engineering for the current need. A single method that returns the value unchanged for non-FK columns is simpler and matches the existing inline pattern. Phase 2/3 callers benefit from the all-in-one interface. |

## Architecture Patterns

### Existing Code to Extract

The existing inline FK resolution in `database_create.cpp:151-167` within the set table insertion loop:

```cpp
// Current inline code (17 lines) -- lines 151-167
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

### Pattern 1: The `resolve_fk_label` Helper

**What:** A method on `Database::Impl` that takes a table definition, column name, and value. If the column is an FK and the value is a string, it resolves the label to an integer ID. Otherwise, it returns the value unchanged.

**Signature:**

```cpp
// In database_impl.h, inside struct Database::Impl:
Value resolve_fk_label(const TableDefinition& table_def,
                        const std::string& column,
                        const Value& value,
                        Database& db);
```

**Why this signature:**
- `table_def` -- caller already has this from `impl_->schema->get_table()`. Avoids re-fetching.
- `column` -- the column being resolved. Matched against `table_def.foreign_keys[].from_column`.
- `value` -- the input value (may be string label, int64_t ID, nullptr, or double).
- `db` -- needed to call `db.execute()` for the lookup query. `execute()` is private on `Database`, but `Impl` is a friend struct inside `Database`, so `db.execute()` is accessible.
- Returns `Value` -- resolved int64_t if FK+string, otherwise original value unchanged.

**Behavior matrix:**

| Value variant | Column is FK? | Action |
|--------------|---------------|--------|
| `std::string` | Yes | Resolve: `SELECT id FROM {to_table} WHERE label = ?`. Return `int64_t`. Throw ERR-01 on not found. |
| `std::string` | No, but column is INTEGER | Throw: string passed to non-FK INTEGER column (new guard, Success Criteria 4) |
| `std::string` | No, column is TEXT/DATETIME | Return unchanged (legitimate string value) |
| `int64_t` | Any | Return unchanged (raw ID or regular integer) |
| `double` | Any | Return unchanged (TypeValidator already validated type compatibility) |
| `nullptr_t` | Any | Return unchanged (NULL passthrough) |

**Source:** Extracted from the proven pattern at `src/database_create.cpp:151-167`.

### Pattern 2: Pre-Resolve Pass (for Phase 2/3 consumption, designed now)

**What:** The helper is designed so that Phase 2 and 3 can call it in a pre-resolve pass before any SQL writes:

```cpp
// Phase 2/3 will use this pattern:
const auto* table_def = impl_->schema->get_table(collection);
std::map<std::string, Value> resolved_scalars;
for (const auto& [name, value] : scalars) {
    resolved_scalars[name] = impl_->resolve_fk_label(*table_def, name, value, *this);
}
// Build SQL from resolved_scalars
```

**Why designed now:** The CONTEXT.md locks the decision that the helper should be "general from the start" and that "Phase 2 and 3 should be able to just call the existing helper without extending its interface." The signature above supports scalar resolution (collection table FK), vector resolution (group table FK), and set resolution (group table FK) with zero changes.

### Pattern 3: Refactored Set FK Path

**What:** The 17-line inline FK block in `create_element`'s set insertion loop collapses to a single call:

```cpp
// Before (17 lines):
for (const auto& fk : table_def->foreign_keys) {
    if (fk.from_column == col_name && std::holds_alternative<std::string>(val)) {
        // ... 15 more lines
    }
}

// After (1 line):
auto val = impl_->resolve_fk_label(*table_def, col_name, (*values_ptr)[row_idx], *this);
```

**Behavioral guarantee:** The refactored code must produce identical behavior to the inline code. The same tests that pass before refactoring must pass after. No new test cases are needed for the set path itself -- the existing tests already cover it.

### Integration Point 1: Set FK Refactor in `create_element`

**File:** `src/database_create.cpp`
**Location:** Lines 140-175 (set table insertion loop)
**Change:** Replace the inner `for (const auto& fk : ...)` block with a call to `impl_->resolve_fk_label()`.

The set table loop currently:
1. Gets `table_def` at line 122 (already done)
2. Validates array types (lines 129-137)
3. For each row, for each column, checks FK + resolves inline (lines 151-167)
4. Pushes resolved value to params (line 169)

After refactoring:
1. Gets `table_def` at line 122 (unchanged)
2. Validates array types (unchanged)
3. For each row, for each column, calls `resolve_fk_label()` (one line)
4. Pushes resolved value to params (unchanged)

### Anti-Patterns to Avoid

- **Duplicating the inline FK block:** The whole point of Phase 1 is extraction. Do not copy the 17-line block; extract it.
- **Making TypeValidator FK-aware:** TypeValidator's job is type checking, not resolution. Adding FK awareness would make it depend on `Database::execute()`, breaking its clean Schema-only dependency.
- **Putting resolution in the C API layer:** Resolution is business logic. The C API is a thin wrapper. All resolution stays in C++ per project philosophy.
- **Mutating the Element:** The helper returns a new Value, never modifies the input Element. The caller's Element data should remain unmodified.
- **Returning the resolved ID from `resolve_fk_label` as a separate type:** Return `Value` (the same variant type), not `int64_t`. This keeps the call site simple -- caller always gets a `Value` ready to push into params.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FK metadata lookup | Custom PRAGMA queries at resolve time | `table_def.foreign_keys` (already populated at schema load) | FK metadata is loaded once during `Schema::from_database()` and cached on `TableDefinition`. No need to re-query. |
| SQL parameterized execution | Raw `sqlite3_prepare_v2` calls in the helper | `Database::execute(sql, params)` | `execute()` handles binding, stepping, error reporting, and returns `Result`. The inline set FK code already uses it. |
| Label uniqueness checking | Multi-row handling in resolution | Schema UNIQUE constraint on label | `SchemaValidator` enforces `label TEXT UNIQUE NOT NULL` on all collections. Resolution is guaranteed 0 or 1 rows. |

**Key insight:** Every building block for the helper already exists. The task is extraction and composition, not creation.

## Common Pitfalls

### Pitfall 1: TypeValidator Allows Strings for ALL INTEGER Columns

**What goes wrong:** After Phase 1, the TypeValidator still accepts strings for any INTEGER column (not just FK columns). If the resolution helper does not guard non-FK INTEGER columns, a user passing `element.set("some_integer", "not_a_label")` will pass validation, skip resolution (no FK found), and fail at SQLite INSERT time with a confusing STRICT mode error.

**Why it happens:** TypeValidator has no FK awareness (by design). It permits string-for-INTEGER as a blanket rule to support FK label resolution.

**How to avoid:** The `resolve_fk_label` helper must explicitly check: if the value is a string AND the column is INTEGER (not TEXT/DATETIME) AND the column has no FK, throw a clear Quiver error. This is the "non-FK INTEGER guard" (Success Criteria 4).

**Warning signs:** Test `element.set("some_integer", std::string("hello"))` for a non-FK INTEGER column. If it produces a SQLite error rather than a Quiver error, the guard is missing.

### Pitfall 2: Resolution Helper Needs Access to `Database::execute()`

**What goes wrong:** If the helper is placed on a class that does not have access to `execute()`, it cannot run the lookup query.

**Why it happens:** `execute()` is a private method on `Database`. Only `Database` members and `Impl` (as a nested struct) can access it.

**How to avoid:** Place the helper on `Database::Impl` and pass a `Database&` reference. The existing inline set FK code already calls `execute()` from within `create_element` (a Database method), confirming the access pattern works.

**Warning signs:** Compilation errors about `execute()` being private.

### Pitfall 3: Refactored Set Path Must Be Behaviorally Identical

**What goes wrong:** Subtle behavioral differences between the inline code and the helper (e.g., different handling of the `break` after FK match, different empty-result check logic) cause existing tests to fail.

**Why it happens:** The inline code has a `break` after finding the first matching FK. The helper must also stop after the first match.

**How to avoid:** The helper iterates `table_def.foreign_keys`, checks `fk.from_column == column`, and returns immediately upon match. The `for` loop with `break` in the inline code maps directly to a `return` in the helper function.

**Warning signs:** Any existing test in `test_database_relations.cpp` or `test_database_create.cpp` failing after the refactor.

### Pitfall 4: Non-FK INTEGER Guard Must Not Fire for Regular Integer Values

**What goes wrong:** The guard rejects `int64_t` values for non-FK INTEGER columns, breaking normal integer writes.

**Why it happens:** Overly broad guard logic that checks column type without checking value type.

**How to avoid:** The guard only fires when ALL of these conditions are true: (1) value is `std::string`, (2) column type is `DataType::Integer`, (3) column has no FK in `table_def.foreign_keys`. If the value is `int64_t`, `double`, or `nullptr_t`, the guard does not fire regardless of column type.

**Warning signs:** Existing tests for `create_element` with integer scalars failing.

### Pitfall 5: The Helper Must Work for Both Collection Tables and Group Tables

**What goes wrong:** The helper assumes FK metadata is only on collection tables (like `Child`), but group tables (like `Child_set_parents`) also have FK definitions.

**Why it happens:** Thinking only about scalar FK columns and forgetting that the existing set FK code operates on group table FKs.

**How to avoid:** The helper takes a `const TableDefinition&` parameter, not a collection name. The caller provides the relevant table definition -- collection table for scalars, group table for vectors/sets. The helper does not care which type of table it is.

**Warning signs:** Set FK resolution breaking after refactor because the wrong table_def was passed.

## Code Examples

### Example 1: The `resolve_fk_label` Helper Implementation

```cpp
// In database_impl.h, inside struct Database::Impl:

Value resolve_fk_label(const TableDefinition& table_def,
                        const std::string& column,
                        const Value& value,
                        Database& db) {
    // Only strings need resolution
    if (!std::holds_alternative<std::string>(value)) {
        return value;
    }

    // Check if this column has a FK
    for (const auto& fk : table_def.foreign_keys) {
        if (fk.from_column == column) {
            // FK column with string value -- resolve label to ID
            const std::string& label = std::get<std::string>(value);
            auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
            auto lookup_result = db.execute(lookup_sql, {label});
            if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
                throw std::runtime_error(
                    "Failed to resolve label '" + label +
                    "' to ID in table '" + fk.to_table + "'");
            }
            return lookup_result[0].get_integer(0).value();
        }
    }

    // Not an FK column -- check if string was passed for a non-FK INTEGER column
    auto col_type = table_def.get_data_type(column);
    if (col_type && *col_type == DataType::Integer) {
        const std::string& str_val = std::get<std::string>(value);
        throw std::runtime_error(
            "Cannot create_element: attribute '" + column +
            "' is INTEGER but received string '" + str_val +
            "' and is not a foreign key");
    }

    // Not a FK, not an INTEGER column -- return string as-is (TEXT, DATETIME)
    return value;
}
```

**Note on error message for non-FK INTEGER:** The CONTEXT.md gives Claude's discretion on the exact wording. The example above uses Pattern 1 ("Cannot {operation}: {reason}") which fits naturally. However, the operation name "create_element" is hardcoded here. For generality (since Phase 2/3 call from both create and update), the error message could omit the operation name or accept it as a parameter. This is an implementation detail for Claude's discretion.

### Example 2: Refactored Set FK Path in `create_element`

```cpp
// Before refactoring (database_create.cpp, inside set table loop):
for (const auto& [col_name, values_ptr] : columns) {
    set_sql += ", " + col_name;
    set_placeholders += ", ?";

    auto val = (*values_ptr)[row_idx];

    // Check if this column is a FK and value is a string (label)
    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == col_name && std::holds_alternative<std::string>(val)) {
            // ... 12 lines of resolution logic
            break;
        }
    }

    set_params.push_back(val);
}

// After refactoring:
for (const auto& [col_name, values_ptr] : columns) {
    set_sql += ", " + col_name;
    set_placeholders += ", ?";

    auto val = impl_->resolve_fk_label(*table_def, col_name, (*values_ptr)[row_idx], *this);
    set_params.push_back(val);
}
```

### Example 3: Test for Valid Label Resolution

```cpp
// In test_database_relations.cpp:
TEST(Database, ResolveFkLabelInSetCreate) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element parent1;
    parent1.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent1);

    quiver::Element parent2;
    parent2.set("label", std::string("Parent 2"));
    db.create_element("Parent", parent2);

    // Create child with set FK using string labels
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_ref", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Verify: read back as integers, should be resolved IDs
    auto sets = db.read_set_integers("Child", "parent_ref");
    EXPECT_EQ(sets.size(), 1);
    EXPECT_EQ(sets[0].size(), 2);
    EXPECT_EQ(sets[0][0], 1);  // Parent 1's ID
    EXPECT_EQ(sets[0][1], 2);  // Parent 2's ID
}
```

### Example 4: Test for Missing Label Error (ERR-01)

```cpp
TEST(Database, ResolveFkLabelMissingTarget) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_ref", std::vector<std::string>{"Nonexistent Parent"});

    EXPECT_THROW({
        try {
            db.create_element("Child", child);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(),
                "Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'");
            throw;
        }
    }, std::runtime_error);
}
```

### Example 5: Test for Non-FK INTEGER Column Error

```cpp
TEST(Database, RejectStringForNonFkIntegerColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // basic.sql has integer_attribute (INTEGER, no FK)
    quiver::Element element;
    element.set("label", std::string("Config 1"));
    element.set("integer_attribute", std::string("not_a_label"));

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}
```

## State of the Art

No external technology changes apply. This is purely internal refactoring of existing patterns.

| Current State | After Phase 1 | Impact |
|--------------|---------------|--------|
| FK label resolution is inline in set path only (17 lines) | Shared helper method on `Database::Impl`, set path refactored to use it | Foundation for Phase 2/3 to add scalar/vector FK resolution |
| String for non-FK INTEGER produces raw SQLite STRICT error | Clear Quiver error before any SQL execution | Better developer experience, consistent error patterns |
| FK resolution logic exists in 2 places with different error messages (`create_element` set path, `update_scalar_relation`) | Shared helper with canonical error message format | Consistency; `update_scalar_relation` alignment can happen in Phase 4 |

## Open Questions

1. **Error message wording for non-FK INTEGER guard**
   - What we know: Must use one of the 3 established patterns (Cannot/Not found/Failed to). CONTEXT.md gives Claude's discretion.
   - What's unclear: Whether to include the string value in the error message (e.g., "received string 'hello'") or keep it terse.
   - Recommendation: Use Pattern 1 ("Cannot {operation}: {reason}") and include the string value for debuggability. The operation name could be passed as a parameter to `resolve_fk_label` for accuracy, or omitted for simplicity.

2. **Whether `resolve_fk_label` needs an operation name parameter**
   - What we know: The non-FK INTEGER error uses Pattern 1 which includes an operation name. The ERR-01 error uses Pattern 3 which does not include an operation name.
   - What's unclear: If the helper is called from both `create_element` and `update_element`, the operation name differs.
   - Recommendation: Two options: (a) pass operation name as parameter for the non-FK error only, or (b) use a generic wording that does not include an operation name (e.g., "Cannot resolve: attribute 'X' is INTEGER but received string and is not a foreign key"). Either way, the ERR-01 message is fixed and does not need an operation name. This is Claude's discretion per CONTEXT.md.

## Files That Will Be Modified

| File | What Changes |
|------|-------------|
| `src/database_impl.h` | Add `resolve_fk_label()` method to `Database::Impl` struct |
| `src/database_create.cpp` | Refactor set FK inline block (lines 151-167) to use the helper |
| `tests/test_database_relations.cpp` | Add dedicated tests for FK label resolution, missing label error, non-FK INTEGER error |

**No new files created.** No changes to `database.h`, `schema.h`, `type_validator.h`, `element.h`, C API files, or binding files.

## Sources

### Primary (HIGH confidence -- direct source code analysis)

- `src/database_create.cpp:151-167` -- existing inline set FK resolution, the exact code to extract
- `src/database_impl.h:16-128` -- `Database::Impl` struct where helper will live, existing helper patterns (`require_collection`, `require_column`)
- `src/database_relations.cpp:30-36` -- alternative FK resolution pattern in `update_scalar_relation` (different error message format)
- `src/type_validator.cpp:42-48` -- string-for-INTEGER allowance with "FK label resolution" comment
- `include/quiver/schema.h:24-30` -- `ForeignKey` struct providing `from_column`, `to_table`, `to_column`
- `include/quiver/schema.h:41-48` -- `TableDefinition` with `foreign_keys` vector and `get_data_type()` method
- `src/schema.cpp:366-396` -- `Schema::query_foreign_keys()` using `PRAGMA foreign_key_list`
- `src/schema_validator.cpp:66-101` -- `validate_collection()` enforcing `label TEXT UNIQUE NOT NULL` on all collections
- `tests/schemas/valid/relations.sql` -- test schema with scalar FK (`Child.parent_id`), self-referential FK (`Child.sibling_id`), vector FK (`Child_vector_refs.parent_ref`), set FK (`Child_set_parents.parent_ref`)
- `tests/schemas/valid/basic.sql` -- test schema with non-FK INTEGER column (`Configuration.integer_attribute`)
- `tests/test_database_relations.cpp` -- existing relation tests that must pass unchanged after refactor
- `tests/test_database_create.cpp` -- existing create tests that must pass unchanged after refactor
- `.planning/research/ARCHITECTURE.md` -- prior project research documenting the extraction pattern in detail
- `.planning/research/PITFALLS.md` -- prior project research documenting all known risks

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all infrastructure verified by direct source reading, no external dependencies
- Architecture: HIGH -- exact extraction pattern proven by existing inline code, line numbers confirmed
- Pitfalls: HIGH -- all pitfalls derived from direct code reading, TypeValidator bypass and transaction atomicity verified against actual source

**Research date:** 2026-02-23
**Valid until:** Indefinite (internal codebase refactoring, no external dependency drift)
