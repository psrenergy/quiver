# Technology Stack

**Project:** Quiver - FK Label Resolution in create_element / update_element
**Researched:** 2026-02-23
**Overall confidence:** HIGH

## Executive Summary

No new libraries or dependencies are needed. FK label resolution is a pure internal pattern extension of existing infrastructure. The codebase already has every building block required:

1. **Schema FK introspection** -- `TableDefinition::foreign_keys` (populated via `PRAGMA foreign_key_list`) already stores `ForeignKey{from_column, to_table, to_column}` for every table.
2. **Label-to-ID lookup SQL** -- The exact `SELECT id FROM {to_table} WHERE label = ?` pattern is already proven in `database_create.cpp:156` (set FK resolution) and `database_relations.cpp:30` (update_scalar_relation).
3. **TypeValidator FK bypass** -- Already implemented in `type_validator.cpp:44`: strings are accepted for INTEGER columns (`expected_type != DataType::Integer`), specifically commented as "FK label resolution".
4. **execute() for parameterized queries** -- The private `Database::execute()` method handles all SQL execution with bind parameters, already used by the FK resolution path.

The work is purely about applying the existing set FK pattern (lines 151-167 of `database_create.cpp`) to the scalar, vector, and set paths in both `create_element` and `update_element`.

## Recommended Stack

### No New Dependencies

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| SQLite3 | existing | FK metadata via PRAGMA foreign_key_list | Already loaded at schema init |
| C++20 | existing | std::variant, std::holds_alternative, std::get | Already used in FK resolution |
| spdlog | existing | Debug logging for FK resolution steps | Already integrated |

### Existing Infrastructure Used (No Changes Needed)

| Component | File | What It Provides |
|-----------|------|------------------|
| `Schema::query_foreign_keys()` | `src/schema.cpp:366` | Loads FK metadata from SQLite PRAGMA at database open |
| `TableDefinition::foreign_keys` | `include/quiver/schema.h:43` | Vector of `ForeignKey` structs per table |
| `ForeignKey` struct | `include/quiver/schema.h:24-29` | `from_column`, `to_table`, `to_column` fields |
| `TypeValidator::validate_value()` | `src/type_validator.cpp:23-51` | Already allows string->INTEGER for FK columns |
| `Database::execute()` | private method | Parameterized SQL execution for label lookups |
| `ScalarMetadata::is_foreign_key` | `include/quiver/attribute_metadata.h:19` | FK flag already exposed in metadata |

## Integration Points for FK Resolution

### 1. Schema FK Lookup (Already Complete)

The `ForeignKey` struct on `TableDefinition` provides everything needed to resolve a column to its target table:

```cpp
// schema.h - already exists
struct ForeignKey {
    std::string from_column;   // e.g., "parent_id"
    std::string to_table;      // e.g., "Parent"
    std::string to_column;     // e.g., "id" (always "id" per schema convention)
    std::string on_update;
    std::string on_delete;
};

// Usage: iterate table_def->foreign_keys to find FK for a column
for (const auto& fk : table_def->foreign_keys) {
    if (fk.from_column == col_name) {
        // fk.to_table is the target collection for label lookup
    }
}
```

**Confidence:** HIGH -- this is existing, working code used in 3 places today.

### 2. TypeValidator Bypass (Already Complete)

The `validate_value()` method already accepts `std::string` values for `DataType::Integer` columns:

```cpp
// type_validator.cpp:42-48 -- already exists
} else if constexpr (std::is_same_v<T, std::string>) {
    // String can go to TEXT, INTEGER (FK label resolution), or DATE_TIME
    if (expected_type != DataType::Text && expected_type != DataType::Integer &&
        expected_type != DataType::DateTime) {
        throw std::runtime_error(...);
    }
}
```

This means callers can pass `std::string` labels for INTEGER FK columns and validation passes. The actual label-to-ID resolution must happen *after* validation but *before* SQL execution.

**Confidence:** HIGH -- verified by reading `src/type_validator.cpp`.

### 3. Label-to-ID Resolution Pattern (Already Proven)

The exact SQL pattern exists in two places:

```cpp
// database_create.cpp:156-164 (set FK path - already working)
auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
auto lookup_result = execute(lookup_sql, {label});
if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
    throw std::runtime_error("Failed to resolve label '" + label +
                             "' to ID in table '" + fk.to_table + "'");
}
val = lookup_result[0].get_integer(0).value();
```

```cpp
// database_relations.cpp:30-36 (update_scalar_relation - same pattern)
auto lookup_sql = "SELECT id FROM " + to_table + " WHERE label = ?";
auto lookup_result = execute(lookup_sql, {to_label});
```

**Confidence:** HIGH -- this is existing, tested, working code.

### 4. Value Type System (No Changes Needed)

`Value = std::variant<std::nullptr_t, int64_t, double, std::string>` already supports passing string labels where integer IDs are expected. The `std::holds_alternative<std::string>(val)` check used in the set FK path is the detection mechanism.

**Confidence:** HIGH -- verified by reading `include/quiver/value.h` and existing usage.

## What Changes Are Needed (Internal Patterns Only)

### Changes to `src/database_create.cpp`

| Location | Current Behavior | Needed Change |
|----------|-----------------|---------------|
| Scalar INSERT (lines 24-41) | Builds INSERT with raw values from `element.scalars()` | After type validation, resolve string values in FK columns to IDs before building INSERT params |
| Vector INSERT (lines 102-117) | Inserts raw values | Add FK resolution loop (same as set path on lines 151-167) |
| Set INSERT (lines 140-174) | **Already has FK resolution** | No change needed |

### Changes to `src/database_update.cpp`

| Location | Current Behavior | Needed Change |
|----------|-----------------|---------------|
| Scalar UPDATE (lines 21-43) | Builds UPDATE with raw values from `element.scalars()` | After type validation, resolve string values in FK columns to IDs |
| Vector UPDATE (lines 75-101) | Deletes + inserts raw values | Add FK resolution loop for FK columns in the insert path |
| Set UPDATE (lines 104-131) | Deletes + inserts raw values | Add FK resolution loop for FK columns in the insert path |

### Shared Resolution Helper (Recommended Extraction)

The FK resolution logic is identical across all paths. Extract to a private helper method on `Database::Impl` or as a free function in a shared location:

```cpp
// Suggested helper - resolve FK labels to IDs in a value vector
// Mutates values in-place, replacing string labels with int64_t IDs
void resolve_fk_labels(const TableDefinition& table_def,
                        const std::string& col_name,
                        Value& val,
                        /* execute function */);
```

### No Changes Needed

| Component | Why No Change |
|-----------|---------------|
| `Schema` / `schema.h` | FK metadata already loaded and accessible |
| `TypeValidator` | Already accepts strings for INTEGER columns |
| `Element` class | Already stores `Value` variant that can hold strings or ints |
| C API layer | `create_element` / `update_element` C API functions pass through to C++; no marshaling changes needed since callers already pass string values |
| Bindings (Julia/Dart/Lua) | Callers already pass string labels naturally; resolution happens in C++ layer |
| `ForeignKey` struct | Already has all needed fields |
| `ScalarMetadata` | Already exposes `is_foreign_key` flag |
| Error messages | Existing error pattern from set FK path applies directly |

## Alternatives Considered

| Question | Answer | Rationale |
|----------|--------|-----------|
| Add FK lookup cache? | No | Lookups are per-element within transactions; SQLite's page cache handles repeated queries efficiently. Premature optimization for a library that handles 10s-1000s of elements, not millions. |
| Add `Schema::is_fk_column()` helper? | No | The existing `table_def->foreign_keys` iteration is a 2-line inline pattern used in 3 places. A helper adds indirection without reducing complexity. |
| Modify TypeValidator to be FK-aware? | No | TypeValidator already allows the string-to-integer path. Making it FK-aware would couple validation to schema FK metadata unnecessarily. |
| Resolve in bindings instead of C++? | No | Violates "Intelligence resides in C++ layer. Bindings remain thin." principle from CLAUDE.md. |
| Add new `create_element_with_relations()` method? | No | The whole point is that `create_element` handles FK resolution transparently. No new API surface needed. |

## Installation

No new dependencies. No package changes.

```bash
# Existing build command - unchanged
cmake --build build --config Debug
```

## Test Schema

The existing `tests/schemas/valid/relations.sql` already defines the needed FK patterns for testing:

- **Scalar FK:** `Child.parent_id -> Parent(id)`, `Child.sibling_id -> Child(id)` (self-reference)
- **Vector FK:** `Child_vector_refs.parent_ref -> Parent(id)`
- **Set FK:** `Child_set_parents.parent_ref -> Parent(id)`

No new test schemas are needed for the core FK resolution. The existing schema covers all three FK contexts (scalar, vector, set) and both cross-collection and self-referential cases.

## Sources

All findings verified by direct source code reading:

- `src/database_create.cpp` -- existing set FK resolution pattern (lines 151-167) | HIGH confidence
- `src/database_update.cpp` -- current update_element lacking FK resolution | HIGH confidence
- `src/database_relations.cpp` -- label-to-ID lookup pattern (lines 30-36) | HIGH confidence
- `src/type_validator.cpp` -- string-to-INTEGER bypass (lines 42-48) | HIGH confidence
- `include/quiver/schema.h` -- ForeignKey struct, TableDefinition with FK vector | HIGH confidence
- `src/schema.cpp` -- PRAGMA foreign_key_list loading (lines 366-396) | HIGH confidence
- `include/quiver/attribute_metadata.h` -- is_foreign_key metadata flag | HIGH confidence
- `tests/schemas/valid/relations.sql` -- FK test schema covering scalar/vector/set | HIGH confidence
