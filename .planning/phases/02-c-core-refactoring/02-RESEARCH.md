# Phase 2: C++ Core Refactoring - Research

**Researched:** 2026-03-01
**Domain:** C++ RAII patterns, template-based code deduplication (SQLite statement management, typed row extraction)
**Confidence:** HIGH

## Summary

This phase is a pure internal refactoring of two specific areas in the C++ core: (1) replacing manual `sqlite3_finalize` calls in `current_version()` with a `unique_ptr`-based RAII wrapper, and (2) eliminating hand-written row-iteration loops in all 6 scalar read methods by delegating to existing and new templates in `database_internal.h`.

Both changes are well-understood mechanical transformations. The RAII pattern for SQLite statements already exists in `Database::execute()` (line 137 of `database.cpp`), so `current_version()` just needs to adopt the same approach. The template pattern for column extraction already exists as `read_grouped_values_by_id<T>` in `database_internal.h`, and the scalar read methods can call it directly after a rename for clarity. A new `read_single_value<T>` template handles the `_by_id` scalar variants.

**Primary recommendation:** Apply both changes as a single cohesive refactoring pass across 3 files (`database.cpp`, `database_read.cpp`, `database_internal.h`) plus the `StmtPtr` typedef in `database_impl.h`. Run the full test suite after each change to confirm zero behavioral differences.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Define reusable `StmtPtr` typedef (`unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>`) in `database_impl.h`
- Use `StmtPtr` directly in `current_version()` -- no helper method, no `Impl::prepare()`
- RAII handles cleanup on all paths: normal return and exception throw -- no manual `sqlite3_finalize` calls
- `StmtPtr` lives in `database_impl.h` alongside `Database::Impl` where `sqlite3` is already visible
- Scalar reads call the template directly -- no wrapper or alias
- Rename `read_grouped_values_by_id<T>` to `read_column_values<T>` for accuracy (it extracts column 0 from all rows, not "by id" specifically)
- Rename applies to all existing callers (vector/set by_id methods, `read_element_ids`)
- Validation (`require_collection`, `require_column`) stays in each method -- not moved into template
- Each scalar read method becomes 4 lines: 2 require calls + SQL string + `return read_column_values<T>(execute(sql))`
- All 6 scalar read methods refactored (not just the 3 vector-returning ones):
  - `read_scalar_integers`, `read_scalar_floats`, `read_scalar_strings` -> use `read_column_values<T>`
  - `read_scalar_integer_by_id`, `read_scalar_float_by_id`, `read_scalar_string_by_id` -> use new `read_single_value<T>`
- New template `read_single_value<T>` added to `database_internal.h`: checks `result.empty()`, returns `nullopt` or extracts column 0 from row 0
- `read_single_value<T>` used only by scalar by_id methods (vector/set by_id return `vector<T>`, different type)
- `read_element_ids` already uses the template -- gets the rename automatically, no other changes

### Claude's Discretion
- Exact `StmtPtr` typedef syntax and placement within `database_impl.h`
- Internal structure of `read_single_value<T>` template
- Whether `read_grouped_values_all<T>` should also be renamed for consistency (not required by scope)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| QUAL-01 | `current_version()` uses RAII `unique_ptr` with custom deleter instead of manual `sqlite3_finalize` | Existing `execute()` method (database.cpp:137) already uses identical `unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>` pattern. Direct copy of approach to `current_version()`. |
| QUAL-02 | Scalar read methods use `read_grouped_values_by_id` template instead of manual loops | Template exists in `database_internal.h:54-65`. Scalar reads currently have identical loop patterns (lines 6-55 of database_read.cpp). Direct delegation eliminates duplication. New `read_single_value<T>` template needed for `_by_id` variants. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SQLite3 | (bundled) | Database engine | Already in use; `sqlite3_finalize` is the standard SQLite statement cleanup function |
| C++20 `<memory>` | standard | `std::unique_ptr` with custom deleter for RAII | Language-standard approach to deterministic resource cleanup |

### Supporting
No new libraries required. All changes use existing C++ standard library features and existing project templates.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `unique_ptr` custom deleter | Wrapper class with destructor | Over-engineered for a typedef; `unique_ptr` is idiomatic C++ |
| Template delegation | Shared lambda/function object | Templates provide compile-time type safety with zero overhead |

## Architecture Patterns

### Pattern 1: StmtPtr RAII Typedef
**What:** A typedef alias for `unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>` that wraps raw SQLite statement pointers.
**When to use:** Any function that calls `sqlite3_prepare_v2` and needs automatic cleanup.
**Existing precedent:** `Database::execute()` at `database.cpp:137` already uses this exact pattern inline.

```cpp
// database_impl.h -- typedef placement (near top, after includes)
using StmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

// Usage in current_version():
sqlite3_stmt* raw_stmt = nullptr;
auto rc = sqlite3_prepare_v2(impl_->db, sql, -1, &raw_stmt, nullptr);
if (rc != SQLITE_OK) { throw ...; }
StmtPtr stmt(raw_stmt, sqlite3_finalize);
// stmt is automatically finalized on all exit paths
```

### Pattern 2: read_column_values<T> Template (Renamed)
**What:** Extracts column 0 from every row in a Result, collecting into `vector<T>`. Currently named `read_grouped_values_by_id<T>`.
**When to use:** Any read that returns a flat list of typed values from a single-column query.
**Current callers:** All vector/set `_by_id` methods (6 callers), `read_element_ids` (1 caller). After refactoring, also all 3 scalar bulk-read methods (3 new callers, total 10).

```cpp
// database_internal.h -- renamed template
template <typename T>
std::vector<T> read_column_values(const Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = get_row_value(result[i], 0, static_cast<T*>(nullptr));
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}
```

### Pattern 3: read_single_value<T> Template (New)
**What:** Extracts column 0 from row 0, returning `optional<T>`. Returns `nullopt` if result is empty.
**When to use:** Scalar `_by_id` reads that return a single optional value.
**Callers:** 3 scalar `_by_id` methods only.

```cpp
// database_internal.h -- new template
template <typename T>
std::optional<T> read_single_value(const Result& result) {
    if (result.empty()) {
        return std::nullopt;
    }
    return get_row_value(result[0], 0, static_cast<T*>(nullptr));
}
```

### Anti-Patterns to Avoid
- **Impl::prepare() helper:** Decision explicitly excludes this. `StmtPtr` is used directly at call sites, not wrapped in a factory method.
- **Moving validation into templates:** Decision explicitly keeps `require_collection` and `require_column` calls in each method, not inside the templates. Templates are pure data extraction.
- **Renaming `read_grouped_values_all`:** While allowed under Claude's discretion, the decision notes it's not required. Recommend leaving it as-is to keep the rename focused on the exact scope needed.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SQLite statement cleanup | Manual `sqlite3_finalize` at each return/throw | `unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>` | Exception-safe, handles early returns, already proven in `execute()` |
| Typed column extraction | Per-method iteration loops | `read_column_values<T>` / `read_single_value<T>` templates | Eliminates 6 duplicated loops, single point of maintenance |

**Key insight:** Both refactorings eliminate duplication that already has a proven solution in the codebase. This is not introducing new patterns -- it's applying existing patterns consistently.

## Common Pitfalls

### Pitfall 1: StmtPtr Initialization with nullptr
**What goes wrong:** Creating a `StmtPtr` with `nullptr` deleter argument or forgetting the deleter entirely.
**Why it happens:** `unique_ptr` with custom deleter requires the deleter at construction time.
**How to avoid:** Always construct as `StmtPtr stmt(raw_ptr, sqlite3_finalize)`. The typedef ensures the type is correct; the constructor call must pass the function pointer.
**Warning signs:** Compiler errors about missing deleter argument or type mismatch.

### Pitfall 2: Behavior Change in Null Handling
**What goes wrong:** The existing scalar read loops check `if (val)` and skip nulls. The template `read_column_values<T>` does the same. But if someone changes the template's null handling, it affects all callers.
**Why it happens:** Consolidation into a single template means one change has wide impact.
**How to avoid:** The template already has the correct null-skipping behavior (identical to current per-method loops). Run all existing tests to verify no behavioral change.
**Warning signs:** Tests failing with unexpected null values or different vector sizes.

### Pitfall 3: Rename Missed in a Caller
**What goes wrong:** Renaming `read_grouped_values_by_id` to `read_column_values` requires updating all 7 existing call sites in `database_read.cpp`. Missing one causes a compilation error.
**Why it happens:** Mechanical find-and-replace can miss edge cases.
**How to avoid:** The compiler will catch this -- `read_grouped_values_by_id` will no longer exist, so any missed call site fails to compile. This is a safe rename.
**Warning signs:** Compilation error mentioning `read_grouped_values_by_id` as undeclared.

### Pitfall 4: read_single_value returning wrong optional type
**What goes wrong:** `get_row_value` returns `optional<T>`, and `read_single_value<T>` returns `optional<T>`. If the row value itself is null (e.g., a NULL SQL column), `get_row_value` returns `nullopt`, which `read_single_value` propagates correctly.
**Why it happens:** The existing `_by_id` methods use `result[0].get_integer(0)` which already returns `optional<int64_t>`. The template must preserve this exact behavior.
**How to avoid:** Template returns `get_row_value(result[0], 0, ...)` directly, which is already `optional<T>`.
**Warning signs:** The `DateTimeNullable` test (line 799-809 of test_database_read.cpp) verifies null handling for `read_scalar_string_by_id`. If this test fails, the template has incorrect null semantics.

## Code Examples

### Before: current_version() with manual finalize (database.cpp:214-233)
```cpp
int64_t Database::current_version() const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "PRAGMA user_version;";
    auto rc = sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    rc = sqlite3_step(stmt);
    int64_t user_version = 0;
    if (rc == SQLITE_ROW) {
        user_version = sqlite3_column_int(stmt, 0);
    } else {
        sqlite3_finalize(stmt);  // manual cleanup on error path
        throw std::runtime_error("Failed to read user_version: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    sqlite3_finalize(stmt);  // manual cleanup on success path
    return user_version;
}
```

### After: current_version() with StmtPtr RAII
```cpp
int64_t Database::current_version() const {
    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql = "PRAGMA user_version;";
    auto rc = sqlite3_prepare_v2(impl_->db, sql, -1, &raw_stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }
    StmtPtr stmt(raw_stmt, sqlite3_finalize);

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_ROW) {
        throw std::runtime_error("Failed to read user_version: " + std::string(sqlite3_errmsg(impl_->db)));
    }
    return sqlite3_column_int(stmt.get(), 0);
}
```

### Before: read_scalar_integers with manual loop (database_read.cpp:6-21)
```cpp
std::vector<int64_t> Database::read_scalar_integers(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read_scalar_integers");
    impl_->require_column(collection, attribute, "read_scalar_integers");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<int64_t> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_integer(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}
```

### After: read_scalar_integers delegating to template
```cpp
std::vector<int64_t> Database::read_scalar_integers(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read_scalar_integers");
    impl_->require_column(collection, attribute, "read_scalar_integers");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    return internal::read_column_values<int64_t>(execute(sql));
}
```

### Before: read_scalar_integer_by_id with manual extraction (database_read.cpp:57-68)
```cpp
std::optional<int64_t>
Database::read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_scalar_integer_by_id");
    impl_->require_column(collection, attribute, "read_scalar_integer_by_id");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_integer(0);
}
```

### After: read_scalar_integer_by_id delegating to template
```cpp
std::optional<int64_t>
Database::read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_scalar_integer_by_id");
    impl_->require_column(collection, attribute, "read_scalar_integer_by_id");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    return internal::read_single_value<int64_t>(execute(sql, {id}));
}
```

## Detailed Change Inventory

### File: `src/database_impl.h`
- **Add:** `using StmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;` typedef in `quiver` namespace (before or after `ResolvedElement` struct, before `Database::Impl`)
- **No other changes.** `<memory>` and `<sqlite3.h>` are already included.

### File: `src/database_internal.h`
- **Rename:** `read_grouped_values_by_id<T>` to `read_column_values<T>` (line 54-65). Function body is identical.
- **Add:** `read_single_value<T>` template (new, after `read_column_values`). ~6 lines. Requires `<optional>` (already included).
- **No changes to:** `read_grouped_values_all<T>`, `get_row_value` overloads, `find_dimension_column`, `scalar_metadata_from_column`.

### File: `src/database.cpp`
- **Modify:** `current_version()` (lines 214-233) to use `StmtPtr` instead of raw pointer + manual finalize.
- **Optional:** Update `execute()` (line 137) to use `StmtPtr` typedef instead of inline `unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>`. This is a cosmetic improvement that follows naturally from adding the typedef.

### File: `src/database_read.cpp`
- **Modify:** `read_scalar_integers` (lines 6-21) -- replace loop with `return internal::read_column_values<int64_t>(execute(sql));`
- **Modify:** `read_scalar_floats` (lines 23-38) -- replace loop with `return internal::read_column_values<double>(execute(sql));`
- **Modify:** `read_scalar_strings` (lines 40-55) -- replace loop with `return internal::read_column_values<std::string>(execute(sql));`
- **Modify:** `read_scalar_integer_by_id` (lines 57-68) -- replace if/return with `return internal::read_single_value<int64_t>(execute(sql, {id}));`
- **Modify:** `read_scalar_float_by_id` (lines 70-81) -- replace if/return with `return internal::read_single_value<double>(execute(sql, {id}));`
- **Modify:** `read_scalar_string_by_id` (lines 83-94) -- replace if/return with `return internal::read_single_value<std::string>(execute(sql, {id}));`
- **Rename:** All 7 existing `read_grouped_values_by_id` calls to `read_column_values` (lines 129, 138, 147, 183, 192, 201, 207).

### Files NOT changed
- `include/quiver/database.h` -- public API unchanged
- `tests/` -- all tests run unchanged (behavior-preserving refactoring)
- `src/c/` -- C API layer unchanged
- `bindings/` -- all bindings unchanged
- `src/schema.cpp` -- has manual `sqlite3_finalize` calls but these are OUT OF SCOPE (not in phase requirements)

## Exact Caller Count for Rename

`read_grouped_values_by_id` current call sites (all in `database_read.cpp`):
1. Line 129: `read_vector_integers_by_id`
2. Line 138: `read_vector_floats_by_id`
3. Line 147: `read_vector_strings_by_id`
4. Line 183: `read_set_integers_by_id`
5. Line 192: `read_set_floats_by_id`
6. Line 201: `read_set_strings_by_id`
7. Line 207: `read_element_ids`

All 7 become `read_column_values`. Plus 3 new callers from scalar bulk-read refactoring. Total: 10 callers of `read_column_values<T>`.

## Test Coverage

Existing tests that validate the refactored code (all must pass unchanged):

| Test | What it covers | File |
|------|---------------|------|
| `ReadScalarIntegers` | `read_scalar_integers` bulk read | `test_database_read.cpp` |
| `ReadScalarFloats` | `read_scalar_floats` bulk read | `test_database_read.cpp` |
| `ReadScalarStrings` | `read_scalar_strings` bulk read | `test_database_read.cpp` |
| `ReadScalarEmpty` | Empty collection returns empty vectors | `test_database_read.cpp` |
| `ReadScalarIntegerById` | `read_scalar_integer_by_id` | `test_database_read.cpp` |
| `ReadScalarFloatById` | `read_scalar_float_by_id` | `test_database_read.cpp` |
| `ReadScalarStringById` | `read_scalar_string_by_id` | `test_database_read.cpp` |
| `ReadScalarByIdNotFound` | Returns nullopt for nonexistent ID | `test_database_read.cpp` |
| `DateTimeNullable` | Null handling in string_by_id | `test_database_read.cpp` |
| `DateTimeReadScalarString` | DateTime as string scalar | `test_database_read.cpp` |
| `DateTimeReadScalarStringById` | DateTime as string scalar by id | `test_database_read.cpp` |
| `ReadVectorIntegerById` | vector_by_id (uses renamed template) | `test_database_read.cpp` |
| `ReadVectorFloatById` | vector_by_id (uses renamed template) | `test_database_read.cpp` |
| `ReadSetStringById` | set_by_id (uses renamed template) | `test_database_read.cpp` |
| `ReadElementIds` | read_element_ids (uses renamed template) | `test_database_read.cpp` |
| Various lifecycle tests | `current_version()` called during `from_migrations` | `test_database_lifecycle.cpp` |

Additionally: C API tests (`quiver_c_tests.exe`), Julia tests, Dart tests, and Python tests all exercise these methods through their bindings. The full test suite (`scripts/test-all.bat`) validates the complete stack.

## Open Questions

1. **Should `execute()` also use `StmtPtr` typedef?**
   - What we know: `execute()` at line 137 already uses `unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>` inline. Replacing with `StmtPtr` is a cosmetic improvement.
   - What's unclear: Whether this is considered "in scope" for QUAL-01 which only mentions `current_version()`.
   - Recommendation: Do it -- it's a 1-line change that improves consistency and is a natural consequence of adding the typedef. Falls under Claude's discretion for "exact StmtPtr typedef syntax and placement."

2. **Should `read_grouped_values_all<T>` be renamed?**
   - What we know: The decision notes this as Claude's discretion, not required.
   - Recommendation: Leave it as-is. It has a different semantic (groups rows by ID column into nested vectors) and the name is more accurate for its actual purpose. Renaming would add scope without adding value.

## Sources

### Primary (HIGH confidence)
- `src/database.cpp` -- existing RAII pattern in `execute()` (line 137), current `current_version()` implementation (lines 214-233)
- `src/database_read.cpp` -- all 6 scalar read method implementations (lines 6-94), all `read_grouped_values_by_id` call sites (lines 129-207)
- `src/database_internal.h` -- existing templates and `get_row_value` dispatchers (lines 16-65)
- `src/database_impl.h` -- `Database::Impl` struct, existing includes confirming `<sqlite3.h>` and `<memory>` availability
- `tests/test_database_read.cpp` -- 30+ tests covering all affected methods
- C++ standard: `std::unique_ptr` with custom deleter is well-documented standard behavior

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, all patterns already exist in the codebase
- Architecture: HIGH -- mechanical application of existing codebase patterns, all decisions locked by user
- Pitfalls: HIGH -- pitfalls are compiler-detectable (missed rename = compile error, wrong null handling = test failure)

**Research date:** 2026-03-01
**Valid until:** Indefinite (internal refactoring of stable C++ patterns, no external dependencies)
