# Phase 1: Bug Fixes and Element Dedup - Research

**Researched:** 2026-02-28
**Domain:** C++ internal refactoring, bug fixes, cross-layer parameter rename
**Confidence:** HIGH

## Summary

Phase 1 addresses three bugs (BUG-01, BUG-02, BUG-03) and one code quality improvement (QUAL-03) in the Quiver C++ core. All changes are internal -- no new public API surface. The bugs are well-understood, the code is clearly structured, and the fixes are mechanically straightforward. The biggest piece of work is QUAL-03 (extracting the shared group insertion helper), which requires careful handling of behavioral differences between `create_element` and `update_element`.

The codebase is mature with clear patterns. All existing code follows strict conventions (3 error message patterns, consistent naming, established file organization). Test infrastructure is solid with GoogleTest/GoogleMock, in-memory databases, and shared SQL schemas.

**Primary recommendation:** Start with the trivial BUG-03 rename, then fix BUG-02 (describe), then tackle QUAL-03 (shared helper) which automatically resolves BUG-01 (type validation in update_element).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Both `create_element` and `update_element` accept empty arrays -- `create_element` skips them silently (element created with no group data), `update_element` clears existing rows for that group
- Error messages are unified across both methods -- the shared helper produces identical messages for the same failure type
- Error prefix retains the calling method name: `"Cannot create_element: ..."` or `"Cannot update_element: ..."` -- the helper receives the caller name as a parameter
- Add time series groups to `describe()` output alongside existing Vectors and Sets display
- Dimension column highlighted with brackets: `data: [date_time] value(Real), count(Integer)`
- Fix column ordering to match schema-definition order (not alphabetical from std::map iteration) -- use PRAGMA table_info or equivalent to get definition order
- Each category header (Scalars, Vectors, Sets, Time Series) prints exactly once per collection, not per matching table
- Full pipeline helper on `Database::Impl` covering routing (arrays to vector/set/time series tables) + type validation + row insertion
- Replace semantic for group updates: DELETE existing rows for the specific group being updated, then INSERT new rows. Groups not mentioned in the update call remain untouched
- Helper accepts caller name parameter for error message prefixes
- Dedup at C++ level only -- C API remains a thin wrapper calling C++ methods
- Rename `table` to `collection` in the C++ header declaration (`include/quiver/database.h:128`) -- implementation and all bindings already use `collection`

### Claude's Discretion
- Time series files display in describe() (whether to show them if present)
- Internal helper method signature and parameter design
- Test structure for regression tests

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BUG-01 | `update_element` validates array types for vector, set, and time series groups (matching `create_element` behavior) | Resolved automatically by QUAL-03: the shared helper incorporates `validate_array()` calls. Currently missing from `database_update.cpp` lines 80-166 (no `validate_array` calls anywhere in update path). |
| BUG-02 | `describe()` prints each category header (Vectors, Sets, Time Series) exactly once per collection | Bug located in `database_describe.cpp` lines 48, 75: `"  Vectors:\n"` and `"  Sets:\n"` are printed inside the per-table loop instead of before it. Time series not yet displayed at all. Fix: hoist headers above loops, add time series section. |
| BUG-03 | `import_csv` header parameter renamed from `table` to `collection` for consistency | Bug is at `include/quiver/database.h:128`: parameter named `table` in declaration. Implementation at `database_csv_import.cpp:220` already uses `collection`. All bindings (Julia, Dart, Python) and C API already use `collection`. One-line fix. |
| QUAL-03 | Group insertion logic extracted into shared helper used by both `create_element` and `update_element` | `database_create.cpp` lines 48-197 and `database_update.cpp` lines 49-166 contain near-identical routing + insertion logic. Extract to `Database::Impl` method. Key differences to reconcile: empty array handling, DELETE-before-INSERT for updates, error message prefix. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++20 | N/A | Language standard | Project target standard |
| SQLite3 | bundled | Database engine | Already integrated, PRAGMA table_info for column ordering |
| GoogleTest/GoogleMock | bundled | Test framework | Already used by all C++ tests |
| spdlog | bundled | Logging | Already used by Database::Impl |

No new libraries needed. All changes use existing dependencies.

## Architecture Patterns

### Recommended File Structure (Changes Only)
```
include/quiver/database.h         # BUG-03: rename table->collection (line 128)
src/database_impl.h               # QUAL-03: add insert_group_data() helper to Impl
src/database_create.cpp           # QUAL-03: replace inline logic with helper call
src/database_update.cpp           # QUAL-03: replace inline logic with helper call (fixes BUG-01)
src/database_describe.cpp         # BUG-02: restructure output loops + add time series
tests/test_database_update.cpp    # BUG-01: regression test for type mismatch
tests/test_database_lifecycle.cpp # BUG-02: describe output verification tests
```

### Pattern 1: Shared Group Insertion Helper on Impl
**What:** A private method on `Database::Impl` that handles the full pipeline: routing arrays to group tables, type validation, optional DELETE, and row insertion.
**When to use:** Called by both `create_element` and `update_element` after scalar processing.

**Signature concept:**
```cpp
// In database_impl.h, inside struct Database::Impl
void insert_group_data(
    const char* caller,                    // "create_element" or "update_element"
    const std::string& collection,
    int64_t element_id,
    const std::map<std::string, std::vector<Value>>& arrays,
    bool delete_existing,                  // true for update, false for create
    Database& db                           // needed for execute()
);
```

**Key behaviors:**
- Routes each array to its vector/set/time series table via `schema->find_all_tables_for_column()`
- Groups columns by target table (same map-of-maps pattern used today)
- Calls `type_validator->validate_array()` for all columns (fixes BUG-01)
- If `delete_existing` is true, issues `DELETE FROM {table} WHERE id = ?` before inserting
- Empty arrays: when `delete_existing` is false (create), skip silently; when true (update), delete existing rows
- Error messages use `caller` parameter: `"Cannot " + caller + ": ..."`
- Inserts rows with correct structure per group type (vector_index for vectors, no vector_index for sets/time series)

**Why this pattern:** The existing code in `database_create.cpp` (lines 48-197) and `database_update.cpp` (lines 49-166) is structurally identical: build routing maps, validate types, insert rows. The only differences are (1) update does DELETE first, (2) create throws on empty arrays while update clears them, (3) error prefix differs. All three are parameterizable.

### Pattern 2: Describe Output with Collected Groups
**What:** Collect all matching tables per category before printing, so the header prints once.
**Current bug:** The header `"  Vectors:\n"` is inside the `for` loop over tables, causing it to repeat per vector table.

**Fix pattern:**
```cpp
// Collect all vector groups for this collection first
std::vector<std::pair<std::string, const TableDefinition*>> vector_groups;
for (const auto& table_name : impl_->schema->table_names()) {
    if (!impl_->schema->is_vector_table(table_name)) continue;
    if (impl_->schema->get_parent_collection(table_name) != collection) continue;
    auto group_name = table_name.substr(prefix_vec.size());
    const auto* vec_table = impl_->schema->get_table(table_name);
    if (vec_table) vector_groups.emplace_back(group_name, vec_table);
}
if (!vector_groups.empty()) {
    std::cout << "  Vectors:\n";
    for (const auto& [group_name, vec_table] : vector_groups) {
        // print columns in PRAGMA table_info order...
    }
}
```

### Pattern 3: Column Ordering via Schema Query Order
**What:** `TableDefinition.columns` is `std::map<std::string, ColumnDefinition>` which iterates alphabetically. For `describe()` output, use `PRAGMA table_info` order instead.
**Current issue:** `database_describe.cpp` iterates `table_def->columns` (a `std::map`), producing alphabetical order instead of schema-definition order.
**Fix:** `Schema::query_columns()` already returns columns in `PRAGMA table_info` order (confirmed in `schema.cpp:328-364`). Options:
  1. Store an ordered column list alongside the map (e.g., `std::vector<std::string> column_order` in `TableDefinition`)
  2. Query `PRAGMA table_info` directly in `describe()` (duplicates schema logic)
  3. Convert `columns` from `std::map` to an ordered container

**Recommendation:** Option 1 -- add a `std::vector<std::string> column_order` field to `TableDefinition`, populated in `Schema::load_from_database()` alongside the existing `columns` map. This preserves fast lookup by name (the map) while enabling ordered iteration for display. The `describe()` function iterates `column_order` and looks up each column in the map. This is the least invasive change -- no existing code that uses the map is affected.

### Anti-Patterns to Avoid
- **Duplicating SQL construction logic:** The insertion SQL building (column names, placeholders, params) is identical across vector/set/time series -- the helper should use a single code path parameterized by whether to include `vector_index`.
- **Branching on caller name for behavior:** Use the `delete_existing` boolean, not string comparison on `caller`. The caller name is only for error messages.
- **Changing `std::map<std::string, ColumnDefinition>` to `std::vector`:** This would break all existing code that does `columns.find(name)` or `columns[name]`. Add an ordered list alongside instead.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Column schema-definition order | Manual PRAGMA query in describe | `column_order` vector in `TableDefinition` | Already available from `query_columns()` return order; just persist it |
| Type validation | Custom checks in update path | `TypeValidator::validate_array()` | Already exists and tested in create path |
| Table routing | New lookup logic | `Schema::find_all_tables_for_column()` | Already used by both create and update |

**Key insight:** Every building block needed already exists in the codebase. The helper is purely a composition exercise -- taking routing, validation, and insertion code that exists in `create_element` and making it reusable.

## Common Pitfalls

### Pitfall 1: Empty Array Semantics Divergence
**What goes wrong:** The shared helper must handle empty arrays differently based on context. `create_element` should skip them silently (per CONTEXT decision -- changed from current behavior which throws). `update_element` should DELETE existing rows (clearing the group).
**Why it happens:** The current `create_element` throws on empty arrays (`line 60: "empty array not allowed"`). The CONTEXT decision changes this to "skip silently."
**How to avoid:** The `delete_existing` parameter naturally handles this: if `delete_existing` is false and array is empty, return early (skip). If `delete_existing` is true and array is empty, issue DELETE only. Verify with tests for both cases.
**Warning signs:** A test that creates an element with an empty array should succeed (no throw) but produce no group rows. A test that updates with an empty array should clear the group.

### Pitfall 2: std::map Column Iteration Order in Describe
**What goes wrong:** `TableDefinition.columns` is a `std::map<std::string, ColumnDefinition>` which orders alphabetically by key. The `describe()` output shows columns in wrong order.
**Why it happens:** `std::map` sorts by key, so `date_time` comes before `value` alphabetically -- even if the schema defines `value` first.
**How to avoid:** Add `column_order` vector to `TableDefinition`. Populate it from `query_columns()` return order in `load_from_database()`. Use it in `describe()`.
**Warning signs:** Describe output showing `date_time` before `value` when schema defines them in reverse order.

### Pitfall 3: Breaking the TransactionGuard Semantics
**What goes wrong:** The helper is called inside a `TransactionGuard` scope in both `create_element` and `update_element`. If the helper starts its own transaction, it could conflict.
**Why it happens:** `TransactionGuard` is nest-aware (checks `sqlite3_get_autocommit`), so a nested guard is a no-op. But if the helper calls `execute()` which uses `Database::execute()`, and the guard's scope is in the caller, the transactions should compose correctly.
**How to avoid:** The helper should NOT create its own `TransactionGuard`. It runs within the caller's existing guard. It calls `db.execute()` for SQL execution (same as current code).
**Warning signs:** Tests that create/update elements in an explicit transaction failing.

### Pitfall 4: Parameter Rename Breaking Bindings
**What goes wrong:** Renaming `table` to `collection` in `database.h:128` could theoretically affect callers using named parameters.
**Why it happens:** C++ doesn't use named parameters, so the rename is purely cosmetic in the declaration. The implementation already uses `collection`.
**How to avoid:** Verify: the C++ implementation at `database_csv_import.cpp:220` already uses `collection` as the parameter name. The C API at `src/c/database_csv_import.cpp:12` already uses `collection`. All bindings already use `collection`. The only mismatch is the header declaration. This is truly a one-line cosmetic fix.
**Warning signs:** None expected. Purely a header declaration rename.

## Code Examples

### Current create_element Group Insertion (to be extracted)
```cpp
// database_create.cpp lines 48-86: Routing arrays to tables
// This exact pattern is duplicated in database_update.cpp lines 49-78
for (const auto& [array_name, values] : arrays) {
    if (values.empty()) {
        throw std::runtime_error("Cannot create_element: empty array not allowed for '" + array_name + "'");
    }
    auto matches = impl_->schema->find_all_tables_for_column(collection, array_name);
    if (matches.empty()) {
        throw std::runtime_error("Cannot create_element: array '" + array_name + "'...");
    }
    for (const auto& match : matches) {
        switch (match.type) {
        case GroupTableType::Vector: vector_table_columns[match.table_name][array_name] = &values; break;
        case GroupTableType::Set:    set_table_columns[match.table_name][array_name] = &values; break;
        case GroupTableType::TimeSeries: time_series_table_columns[match.table_name][array_name] = &values; break;
        }
    }
}
```

### Current update_element Missing validate_array (BUG-01)
```cpp
// database_update.cpp lines 80-108: Vector insertion WITHOUT type validation
for (const auto& [table, columns] : vector_table_columns) {
    execute("DELETE FROM " + table + " WHERE id = ?", {id});
    size_t num_rows = 0;
    for (const auto& [col_name, values_ptr] : columns) {
        // BUG: No validate_array() call here!
        // create_element has: impl_->type_validator->validate_array(table, col_name, *values_ptr);
        if (num_rows == 0) {
            num_rows = values_ptr->size();
        } else if (values_ptr->size() != num_rows) { /*...*/ }
    }
    // ... insertion ...
}
```

### Describe Bug: Header Inside Loop (BUG-02)
```cpp
// database_describe.cpp lines 36-59: "Vectors:" printed per table, not per collection
for (const auto& table_name : impl_->schema->table_names()) {
    if (!impl_->schema->is_vector_table(table_name)) continue;
    if (impl_->schema->get_parent_collection(table_name) != collection) continue;
    std::cout << "  Vectors:\n";  // BUG: This prints once per vector table!
    // ...
}
```

### BUG-03: Header vs Implementation Mismatch
```cpp
// include/quiver/database.h:128 (declaration - uses "table")
void import_csv(const std::string& table, ...);

// src/database_csv_import.cpp:220 (implementation - uses "collection")
void Database::import_csv(const std::string& collection, ...);
```

### Proposed Helper Skeleton
```cpp
// In database_impl.h, struct Database::Impl
void insert_group_data(
    const char* caller,
    const std::string& collection,
    int64_t element_id,
    const std::map<std::string, std::vector<Value>>& arrays,
    bool delete_existing,
    Database& db
) {
    // 1. Route arrays to tables (existing routing logic)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> vector_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> set_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> time_series_table_columns;

    for (const auto& [array_name, values] : arrays) {
        if (values.empty()) {
            if (!delete_existing) continue;  // create: skip silently
            // update: will DELETE below even if no new rows to insert
        }
        auto matches = schema->find_all_tables_for_column(collection, array_name);
        if (matches.empty()) {
            throw std::runtime_error(std::string("Cannot ") + caller + ": array '" + array_name + "'...");
        }
        // ... route to maps ...
    }

    // 2. For each group type: validate types, optionally DELETE, INSERT rows
    for (const auto& [table, columns] : vector_table_columns) {
        for (const auto& [col_name, values_ptr] : columns) {
            type_validator->validate_array(table, col_name, *values_ptr);  // Fixes BUG-01
        }
        if (delete_existing) {
            db.execute("DELETE FROM " + table + " WHERE id = ?", {element_id});
        }
        // ... insert with vector_index ...
    }
    // Same pattern for sets (no vector_index) and time series (no vector_index)
}
```

## Open Questions

1. **Helper placement: database_impl.h inline vs separate file?**
   - What we know: `database_impl.h` currently contains `Impl` struct definition with inline methods (TransactionGuard, resolve_fk_label, etc.). The helper is ~80 lines.
   - What's unclear: Whether to keep it inline in the header or move to a new `database_group_insert.cpp` and declare in `database_impl.h`.
   - Recommendation: Keep it in `database_impl.h` as an inline method if it stays under ~100 lines. The existing pattern is inline Impl methods. If it grows larger, split into a separate .cpp file with just the declaration in the header.

2. **Column ordering: Should `column_order` be added to `TableDefinition` or handled locally in `describe()`?**
   - What we know: `TableDefinition` lives in `include/quiver/schema.h` (public header). Adding a field there changes the public type.
   - What's unclear: Whether other code (bindings, tests) might benefit from ordered column access.
   - Recommendation: Add `column_order` to `TableDefinition`. It's a WIP project with no ABI stability requirements (per CLAUDE.md). The ordered list is useful beyond just `describe()` -- any future display or serialization code would benefit.

3. **Empty array handling in create_element: behavior change**
   - What we know: Current `create_element` throws on empty arrays. CONTEXT decision says "skip silently."
   - What's unclear: Whether any existing tests rely on the throw behavior.
   - Recommendation: Check existing tests for empty array behavior. If any test asserts the throw, update it. The CONTEXT decision is authoritative.

## Sources

### Primary (HIGH confidence)
- `src/database_create.cpp` - Full create_element implementation with group insertion logic
- `src/database_update.cpp` - Full update_element implementation, confirmed missing `validate_array` calls
- `src/database_describe.cpp` - Confirmed header-inside-loop bug on lines 48 and 75
- `include/quiver/database.h:128` - Confirmed `table` parameter name mismatch
- `src/database_csv_import.cpp:220` - Confirmed implementation uses `collection`
- `include/quiver/schema.h` - `TableDefinition` with `std::map<std::string, ColumnDefinition> columns`
- `src/schema.cpp:328-364` - `query_columns()` returns PRAGMA table_info order
- `src/schema.cpp:293-326` - `load_from_database()` stores columns into map (loses order)
- `include/quiver/type_validator.h` - `validate_array()` method signature
- `src/type_validator.cpp` - `validate_array()` implementation
- `src/database_impl.h` - `Impl` struct with `TransactionGuard`, `resolve_element_fk_labels`

### Binding verification (HIGH confidence)
- `bindings/julia/src/database_csv_import.jl:1` - Uses `collection` parameter
- `bindings/dart/lib/src/database_csv_import.dart:16` - Uses `collection` parameter
- `bindings/python/src/quiverdb/database_csv_import.py:12` - Uses `collection` parameter
- `src/c/database_csv_import.cpp:12` - C API uses `collection` parameter

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - No new dependencies, all existing tools
- Architecture: HIGH - Direct codebase inspection confirms all bug locations and duplication patterns
- Pitfalls: HIGH - All identified from actual code analysis, not speculation

**Research date:** 2026-02-28
**Valid until:** 2026-03-30 (stable codebase, internal refactoring only)
