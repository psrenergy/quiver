# Phase 3: C++ Naming and Error Standardization - Research

**Researched:** 2026-02-09
**Domain:** C++ API naming conventions and exception message patterns
**Confidence:** HIGH

## Summary

Phase 3 is a pure refactoring phase that standardizes two aspects of the C++ `Database` class: (1) method naming conventions and (2) exception message formats. The codebase currently has 60+ public methods with mostly consistent naming but several deviations that need correction. Error messages follow no formal pattern -- some include operation names, some include context, and many use ad-hoc formatting.

The good news is that the existing naming is already 90% consistent. The `verb_noun_type` pattern (e.g., `read_scalar_integers`, `update_vector_floats`) dominates the API. The primary issues are a small set of outliers and the need to formalize the existing pattern into a documented convention. Error messages are more varied -- roughly 55 unique `throw std::runtime_error(...)` sites across the C++ source files use at least 8 different message format styles.

**Primary recommendation:** Codify the existing `verb_noun_type` naming pattern as the standard, fix the ~8 method names that deviate, and adopt a structured error message format of `"operation_name: reason"` across all throw sites.

## Standard Stack

Not applicable -- this phase involves no new libraries, only refactoring existing C++ code. All tools are already in the project (GTest for testing, CMake for build).

## Architecture Patterns

### Current Public API Method Inventory

Complete catalog of all 60 public methods on `Database` class in `include/quiver/database.h`:

#### Lifecycle / Factory (6 methods) -- NO CHANGES NEEDED
| Method | Pattern | Notes |
|--------|---------|-------|
| `Database(path, options)` | constructor | Standard |
| `from_schema(db_path, schema_path, options)` | `from_noun` | Static factory |
| `from_migrations(db_path, migrations_path, options)` | `from_noun` | Static factory |
| `is_healthy()` | `is_adjective` | Accessor/predicate |
| `current_version()` | `noun` (accessor) | Returns state |
| `path()` | `noun` (accessor) | Returns state |

#### Element CRUD (3 methods)
| Method | Pattern | Issue |
|--------|---------|-------|
| `create_element(collection, element)` | `verb_noun` | OK |
| `update_element(collection, id, element)` | `verb_noun` | OK |
| `delete_element_by_id(collection, id)` | `verb_noun_by_id` | INCONSISTENT: only delete has `_by_id` suffix, while update_element also takes an id but lacks the suffix |

#### Read Scalar (all elements) (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `read_scalar_integers(collection, attribute)` | `read_noun_type` |
| `read_scalar_floats(collection, attribute)` | `read_noun_type` |
| `read_scalar_strings(collection, attribute)` | `read_noun_type` |

#### Read Scalar (by ID) (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `read_scalar_integer_by_id(collection, attribute, id)` | `read_noun_type_by_id` |
| `read_scalar_float_by_id(collection, attribute, id)` | `read_noun_type_by_id` |
| `read_scalar_string_by_id(collection, attribute, id)` | `read_noun_type_by_id` |

Note: singular `integer`/`float`/`string` for by_id (returns single), plural `integers`/`floats`/`strings` for all (returns vector). This is intentional and correct.

#### Read Vector (all elements) (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `read_vector_integers(collection, attribute)` | `read_noun_type` |
| `read_vector_floats(collection, attribute)` | `read_noun_type` |
| `read_vector_strings(collection, attribute)` | `read_noun_type` |

#### Read Vector (by ID) (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `read_vector_integers_by_id(collection, attribute, id)` | `read_noun_type_by_id` |
| `read_vector_floats_by_id(collection, attribute, id)` | `read_noun_type_by_id` |
| `read_vector_strings_by_id(collection, attribute, id)` | `read_noun_type_by_id` |

Note: vector by_id returns a single vector (plural `integers`), consistent with returning the vector contents for one element.

#### Read Set (all elements) (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `read_set_integers(collection, attribute)` | `read_noun_type` |
| `read_set_floats(collection, attribute)` | `read_noun_type` |
| `read_set_strings(collection, attribute)` | `read_noun_type` |

#### Read Set (by ID) (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `read_set_integers_by_id(collection, attribute, id)` | `read_noun_type_by_id` |
| `read_set_floats_by_id(collection, attribute, id)` | `read_noun_type_by_id` |
| `read_set_strings_by_id(collection, attribute, id)` | `read_noun_type_by_id` |

#### Read Other (1 method) -- OK
| Method | Pattern |
|--------|---------|
| `read_element_ids(collection)` | `read_noun` |

#### Update Scalar (3 methods) -- MINOR ISSUE
| Method | Pattern | Issue |
|--------|---------|-------|
| `update_scalar_integer(collection, attribute, id, value)` | `update_noun_type` | Singular (correct: updates single value) |
| `update_scalar_float(collection, attribute, id, value)` | `update_noun_type` | Singular |
| `update_scalar_string(collection, attribute, id, value)` | `update_noun_type` | Singular |

These always take an `id` parameter but lack `_by_id` suffix. This is arguably fine since scalars are inherently per-element and there's no "update all scalars" variant. But it's inconsistent with `read_scalar_integer_by_id`.

#### Update Vector (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `update_vector_integers(collection, attribute, id, values)` | `update_noun_type` |
| `update_vector_floats(collection, attribute, id, values)` | `update_noun_type` |
| `update_vector_strings(collection, attribute, id, values)` | `update_noun_type` |

Same `_by_id` question applies here. All take `id`.

#### Update Set (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `update_set_integers(collection, attribute, id, values)` | `update_noun_type` |
| `update_set_floats(collection, attribute, id, values)` | `update_noun_type` |
| `update_set_strings(collection, attribute, id, values)` | `update_noun_type` |

#### Metadata (4 get methods) -- OK
| Method | Pattern |
|--------|---------|
| `get_scalar_metadata(collection, attribute)` | `get_noun_noun` |
| `get_vector_metadata(collection, group_name)` | `get_noun_noun` |
| `get_set_metadata(collection, group_name)` | `get_noun_noun` |
| `get_time_series_metadata(collection, group_name)` | `get_noun_noun` |

#### Metadata (4 list methods) -- INCONSISTENT
| Method | Pattern | Issue |
|--------|---------|-------|
| `list_scalar_attributes(collection)` | `list_noun_noun` | Uses `attributes` |
| `list_vector_groups(collection)` | `list_noun_noun` | Uses `groups` |
| `list_set_groups(collection)` | `list_noun_noun` | Uses `groups` |
| `list_time_series_groups(collection)` | `list_noun_noun` | Uses `groups` |

The mismatch here is that scalars use `attributes` while vectors/sets/time_series use `groups`. This is semantically accurate (scalars don't form groups), so the naming reflects the domain model. This is NOT a naming inconsistency -- it's domain-appropriate naming.

#### Relations (2 methods)
| Method | Pattern | Issue |
|--------|---------|-------|
| `set_scalar_relation(collection, attribute, from_label, to_label)` | `set_noun_noun` | Verb `set` is unusual; `update` would be more consistent |
| `read_scalar_relation(collection, attribute)` | `read_noun_noun` | OK |

#### Time Series Data (2 methods) -- MINOR ISSUE
| Method | Pattern | Issue |
|--------|---------|-------|
| `read_time_series_group_by_id(collection, group, id)` | `read_noun_noun_by_id` | OK but verbose |
| `update_time_series_group(collection, group, id, rows)` | `update_noun_noun` | Takes id but lacks `_by_id` |

#### Time Series Files (4 methods) -- OK
| Method | Pattern |
|--------|---------|
| `has_time_series_files(collection)` | `has_noun` |
| `list_time_series_files_columns(collection)` | `list_noun_noun` |
| `read_time_series_files(collection)` | `read_noun` |
| `update_time_series_files(collection, paths)` | `update_noun` |

#### Query (3 methods) -- OK
| Method | Pattern |
|--------|---------|
| `query_string(sql, params)` | `query_type` |
| `query_integer(sql, params)` | `query_type` |
| `query_float(sql, params)` | `query_type` |

#### Describe/CSV (3 methods) -- ISSUE
| Method | Pattern | Issue |
|--------|---------|-------|
| `describe()` | `verb` | OK (no noun needed) |
| `export_to_csv(table, path)` | `verb_to_noun` | DEVIATES: uses `_to_` preposition, unlike all other methods |
| `import_from_csv(table, path)` | `verb_from_noun` | DEVIATES: uses `_from_` preposition. Also: these are empty stubs |

### Naming Issues Summary

| # | Current Name | Issue | Proposed Fix | Impact |
|---|-------------|-------|-------------|--------|
| 1 | `delete_element_by_id` | Only delete adds `_by_id` while `update_element` takes id without suffix | Rename to `delete_element` (remove `_by_id`) | Tests, C API, Lua, Julia, Dart |
| 2 | `set_scalar_relation` | `set_` verb is used nowhere else; `update_` is the standard write verb | Rename to `update_scalar_relation` | Tests, C API, Lua, Julia, Dart |
| 3 | `export_to_csv` | `_to_` preposition pattern used nowhere else | Rename to `export_csv` | Tests, C API, Lua, Dart (but stubs) |
| 4 | `import_from_csv` | `_from_` preposition pattern used nowhere else | Rename to `import_csv` | Tests, C API, Lua, Dart (but stubs) |
| 5 | `update_time_series_group` | Takes id but lacks `_by_id` suffix while `read_time_series_group_by_id` has it | Decision needed (see Open Questions) |

Issues 3 and 4 are stubs (empty implementations). Renaming them is low risk.

Issue 5 is a design decision: should all methods that take an `id` parameter have `_by_id`? That would mean renaming ALL `update_scalar_*`, `update_vector_*`, `update_set_*`, etc. OR should `_by_id` be reserved only for reads where there are both "all" and "by id" variants? The latter is the current (mostly consistent) pattern and is the recommended approach.

### Recommended Naming Convention

**Formalized pattern (codifying existing practice):**

```
verb_[category]_[type][_by_id]
```

Where:
- **verb**: `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`
- **category**: `scalar`, `vector`, `set`, `time_series`, `element`
- **type** (for typed operations): `integer(s)`, `float(s)`, `string(s)`
- **_by_id**: ONLY when there exists both an "all elements" and "single element" variant of the same read operation

**Rules:**
1. Read operations returning all elements: plural type (`read_scalar_integers`)
2. Read operations for one element: singular type + `_by_id` (`read_scalar_integer_by_id`)
3. Update operations always operate on a single element (id in params): no `_by_id` suffix
4. `get_` prefix for metadata queries, `list_` prefix for enumerating groups/attributes
5. Factory methods use `from_` prefix (`from_schema`, `from_migrations`)
6. Predicates use `is_` or `has_` prefix

### Current Error Message Patterns

Cataloged all ~55 `throw std::runtime_error(...)` sites. Current patterns fall into these categories:

**Pattern 1: "Failed to verb noun: reason"** (most common in lifecycle)
```cpp
"Failed to open database: " + error_msg
"Failed to prepare statement: " + sqlite3_errmsg(...)
"Failed to execute statement: " + sqlite3_errmsg(...)
"Failed to set user_version: " + error
"Failed to execute SQL: " + error
"Failed to begin transaction: " + error
"Failed to commit transaction: " + error
"Failed to open schema file: " + schema_path
"Failed to resolve label '" + label + "' to ID in table '" + table + "'"
```

**Pattern 2: "Noun not found: identifier"** (schema/collection lookups)
```cpp
"Collection not found in schema: " + collection
"Schema file not found: " + schema_path
"Migrations path not found: " + migrations_path
"Vector table not found: " + vector_table
"Set table not found: " + set_table
"Time series table not found: " + ts_table
"Time series files table not found: " + tsf
"Time series files table not found for collection '" + collection + "'"
```

**Pattern 3: "Cannot verb noun: reason"** (require_schema/require_collection)
```cpp
"Cannot read scalar: no schema loaded"
"Cannot get scalar metadata: no schema loaded"
"Cannot set relation: no schema loaded"
"Cannot " + operation + ": no schema loaded"
```

**Pattern 4: "Noun 'name' not found in/for noun 'name'"** (attribute lookups)
```cpp
"Scalar attribute '" + attribute + "' not found in collection '" + collection + "'"
"Vector group '" + group_name + "' not found for collection '" + collection + "'"
"Set group '" + group_name + "' not found for collection '" + collection + "'"
"Time series group '" + group_name + "' not found for collection '" + collection + "'"
```

**Pattern 5: "Adjective noun: detail"** (validation)
```cpp
"Element must have at least one scalar attribute"
"Element must have at least one attribute to update"
"Empty array not allowed for '" + array_name + "'"
"Schema file is empty: " + schema_path
```

**Pattern 6: "Noun is/does description"** (constraint violations)
```cpp
"Migrations path is not a directory: " + migrations_path
"Attribute '" + attribute + "' is not a foreign key in collection '" + collection + "'"
```

**Pattern 7: "Verb noun detail"** (imperative, missing articles)
```cpp
"Migration " + version + " has no up.sql file"
"Migration " + version + " failed: " + reason
```

**Pattern 8: "Type not implemented" / "Blob not implemented"**
```cpp
"Blob not implemented"
"Type not implemented"
```

### Recommended Error Message Format

Adopt a single format: **`"operation: reason"` where operation is the method name or operation description, and reason explains why it failed.**

```
Format: "{operation}: {reason}"
```

Examples of transformation:
```cpp
// BEFORE:
"Failed to open database: " + error_msg
"Collection not found in schema: " + collection
"Cannot read scalar: no schema loaded"
"Scalar attribute '" + attribute + "' not found in collection '" + collection + "'"

// AFTER:
"open: " + error_msg
"create_element: collection not found: " + collection
"read_scalar_integers: no schema loaded"
"get_scalar_metadata: attribute '" + attribute + "' not found in collection '" + collection + "'"
```

However, this strict format has a tradeoff: current messages are more human-readable. A middle-ground approach preserves readability while standardizing structure:

**Recommended approach: Keep human-readable messages but standardize the patterns.**

Standardize to exactly THREE message patterns:
1. **Precondition failures**: `"Cannot {operation}: {reason}"` -- e.g., `"Cannot read_scalar_integers: no schema loaded"`
2. **Not found errors**: `"{Entity} not found: {identifier}"` -- e.g., `"Collection not found: Items"`
3. **Operation failures**: `"Failed to {operation}: {reason}"` -- e.g., `"Failed to open database: unable to open file"`

The key change: make sure `require_schema()` and `require_collection()` messages include the calling operation name (they already do via the `operation` parameter).

### Error Handling Observation: Missing require_schema Calls

Some methods use `require_schema` correctly but read_scalar_* methods use `require_collection` which calls `require_schema` internally. The vector/set read methods use `require_schema` directly but don't validate the collection exists -- they rely on the SQL failing. This is inconsistent but not broken. Phase 3 should standardize: all read/update operations should use `require_collection` for collection-level operations (which validates schema + collection existence).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String-based error codes | Error code enum system | `std::runtime_error` with consistent messages | Project principle: exceptions not error codes |
| Custom exception hierarchy | `QuiverException`, `SchemaException`, etc. | Single `std::runtime_error` | ERRH-01 specifies `std::runtime_error`; adding hierarchy is out of scope for Phase 3 |

**Key insight:** The success criteria explicitly state "All exceptions thrown use `std::runtime_error`". Do NOT introduce custom exception types -- that would be scope creep.

## Common Pitfalls

### Pitfall 1: Cascade Blindness
**What goes wrong:** Renaming a C++ method without tracking all downstream consumers
**Why it happens:** Method names appear in: database.h (declaration), database_*.cpp (implementation), C API (as `quiver_database_{name}`), Lua bindings, Julia bindings, Dart bindings, tests
**How to avoid:** Phase 3 scope is limited to C++ layer only. C API/bindings renames happen in later phases. But test files MUST be updated in Phase 3.
**Warning signs:** Compile errors in test files after renaming

### Pitfall 2: Over-Renaming
**What goes wrong:** Renaming methods that are already correctly named, causing unnecessary churn
**Why it happens:** Enthusiasm for consistency leads to renaming everything
**How to avoid:** The audit above identifies exactly 4-5 methods that need renaming. Everything else is already consistent.
**Warning signs:** More than ~5 methods being renamed

### Pitfall 3: Error Message Grep Breakage
**What goes wrong:** Tests that assert on specific error messages break when messages change
**Why it happens:** Tests like `EXPECT_THROW(..., std::runtime_error)` don't check message content, so this is low risk. But if any tests DO check message strings, they'll break.
**How to avoid:** Search for `EXPECT_THAT`, `HasSubstr`, or string comparisons on exception `.what()` before changing messages.
**Warning signs:** Test failures that aren't about wrong method names

### Pitfall 4: Lua Binding Name Mismatch
**What goes wrong:** Lua bindings in `lua_runner.cpp` reference C++ method names as string literals. If C++ methods are renamed but Lua binding strings are not updated, Lua scripts silently call wrong methods.
**Why it happens:** Lua binding names are string literals, not symbol references -- compiler can't catch mismatches.
**How to avoid:** Phase 3 should NOT rename Lua binding string names. Phase 8 (Lua Bindings Standardization) handles that. Phase 3 only renames C++ method declarations/definitions and updates their call sites in `lua_runner.cpp`.
**Warning signs:** Lua tests failing after Phase 3

### Pitfall 5: Inconsistent require_collection vs require_schema
**What goes wrong:** Some operations throw "no schema loaded" when the real issue is "collection not found" because they use `require_schema` without `require_collection`.
**Why it happens:** Read vector/set operations call `require_schema` directly, then rely on `find_vector_table`/`find_set_table` to throw if collection is wrong. This gives worse error messages.
**How to avoid:** Standardize all operations to use `require_collection` (which includes `require_schema`).
**Warning signs:** Error messages that say "no schema loaded" when the actual problem is a missing collection

## Code Examples

### Method Renaming Pattern

For each renamed method, changes touch exactly 3-4 locations in Phase 3:

```cpp
// 1. include/quiver/database.h (declaration)
void delete_element(const std::string& collection, int64_t id);  // was delete_element_by_id

// 2. src/database_delete.cpp (definition)
void Database::delete_element(const std::string& collection, int64_t id) { ... }

// 3. src/lua_runner.cpp (call site - update the C++ call, keep Lua name for Phase 8)
"delete_element_by_id",  // Lua-facing name stays until Phase 8
[](Database& self, const std::string& collection, int64_t id) {
    self.delete_element(collection, id);  // Updated C++ call
},

// 4. tests/test_database_delete.cpp (test call sites)
db.delete_element("Configuration", id);  // was delete_element_by_id

// 5. tests/test_database_errors.cpp (error test call sites)
EXPECT_THROW(db.delete_element("Configuration", 1), std::runtime_error);
```

### Error Message Standardization Pattern

Three canonical patterns:

```cpp
// Pattern 1: Precondition failure (from require_schema / require_collection)
void require_schema(const char* operation) const {
    if (!schema) {
        throw std::runtime_error(std::string("Cannot ") + operation + ": no schema loaded");
    }
}

void require_collection(const std::string& collection, const char* operation) const {
    require_schema(operation);
    if (!schema->has_table(collection)) {
        throw std::runtime_error(std::string("Cannot ") + operation + ": collection not found: " + collection);
    }
}

// Pattern 2: Not found (entity lookups)
throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" + collection + "'");
throw std::runtime_error("Vector group not found: '" + group_name + "' in collection '" + collection + "'");

// Pattern 3: Operation failure (I/O, SQL, external)
throw std::runtime_error("Failed to open database: " + error_msg);
throw std::runtime_error("Failed to execute statement: " + sqlite3_errmsg(db));
```

### Files Touched (Complete List)

**For naming changes:**
- `include/quiver/database.h` -- method declarations
- `src/database_delete.cpp` -- `delete_element_by_id` -> `delete_element`
- `src/database_relations.cpp` -- `set_scalar_relation` -> `update_scalar_relation`
- `src/database_describe.cpp` -- `export_to_csv` -> `export_csv`, `import_from_csv` -> `import_csv`
- `src/lua_runner.cpp` -- update C++ method calls (keep Lua string names)
- `tests/test_database_delete.cpp` -- update test calls
- `tests/test_database_errors.cpp` -- update test calls
- `tests/test_database_relations.cpp` -- update test calls

**For error message changes:**
- `src/database.cpp` -- lifecycle error messages
- `src/database_impl.h` -- `require_collection` message format
- `src/database_create.cpp` -- create operation error messages
- `src/database_read.cpp` -- add require_collection to vector/set reads
- `src/database_update.cpp` -- update operation error messages
- `src/database_delete.cpp` -- delete operation error messages
- `src/database_metadata.cpp` -- metadata error messages
- `src/database_time_series.cpp` -- time series error messages
- `src/database_relations.cpp` -- relation error messages
- `src/database_query.cpp` -- (minimal, queries mostly delegate to execute)
- `src/database_internal.h` -- `find_dimension_column` error message

**NOT touched in Phase 3:**
- `include/quiver/c/database.h` -- C API (Phase 5)
- `src/c/database.cpp` -- C API implementation (Phase 5)
- `bindings/julia/` -- Julia bindings (Phase 6)
- `bindings/dart/` -- Dart bindings (Phase 7)
- Lua binding STRING names in lua_runner.cpp (Phase 8)

## State of the Art

Not applicable -- this is internal refactoring, not adoption of external technology.

## Open Questions

### 1. Should `_by_id` be added to all update methods?

**What we know:** Read operations have both "all elements" and "by id" variants, distinguished by `_by_id` suffix. Update operations ONLY have "by id" variants (you always update one element), so there's no ambiguity.

**What's unclear:** Is the asymmetry confusing, or is it correct because there's no ambiguity?

**Recommendation:** Do NOT add `_by_id` to update methods. The suffix is only needed for disambiguation when both variants exist. This matches common C++ API design (e.g., `std::map::erase` vs `std::map::erase(key)` -- the overload handles disambiguation via parameter types). Confidence: HIGH.

### 2. Should `update_time_series_group` get `_by_id`?

**What we know:** `read_time_series_group_by_id` has the suffix but `update_time_series_group` does not.

**What's unclear:** The read variant has `_by_id` because time series data is always per-element. There's no "read all time series" variant, so the suffix is technically unnecessary even on the read.

**Recommendation:** REMOVE `_by_id` from `read_time_series_group_by_id` (rename to `read_time_series_group`), since there's no "read all" variant. This makes read and update consistent. Confidence: MEDIUM -- this is a judgment call.

### 3. Should `describe()` follow the naming pattern?

**What we know:** `describe()` has no noun because it operates on the database itself, not a collection.

**Recommendation:** Keep `describe()` as-is. It's a special introspection method. No change needed. Confidence: HIGH.

### 4. What about the 3 Lua-only methods not in C++ public API?

**What we know:** `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` exist only in Lua bindings (not in C++ public API). They're composite operations that call multiple C++ methods.

**Recommendation:** Out of scope for Phase 3 (C++ API only). Phase 8 handles Lua naming.

## Sources

### Primary (HIGH confidence)
- Direct source code analysis of `include/quiver/database.h` -- complete method inventory
- Direct source code analysis of all `src/database_*.cpp` files -- complete error message catalog
- Direct source code analysis of `src/lua_runner.cpp` -- Lua binding names
- Direct source code analysis of `include/quiver/c/database.h` -- C API names for cascade awareness
- Direct source code analysis of `tests/test_database_*.cpp` -- test call sites

### Secondary (MEDIUM confidence)
- `.planning/REQUIREMENTS.md` -- NAME-01, ERRH-01 requirements
- `.planning/ROADMAP.md` -- Phase 3 success criteria, Phase 4/5/8 dependencies

## Metadata

**Confidence breakdown:**
- Naming audit: HIGH -- complete inventory from source code, every method cataloged
- Error message audit: HIGH -- every throw site found via grep, patterns categorized
- Rename impact: HIGH -- all files that reference each method name identified
- Cascade risk: HIGH -- downstream phases clearly scoped in roadmap

**Research date:** 2026-02-09
**Valid until:** 2026-03-09 (stable -- internal refactoring, no external dependencies)
