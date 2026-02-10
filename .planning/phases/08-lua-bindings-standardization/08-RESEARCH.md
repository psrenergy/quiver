# Phase 8: Lua Bindings Standardization - Research

**Researched:** 2026-02-10
**Domain:** sol2-based Lua bindings naming and error propagation in embedded C++/Lua system
**Confidence:** HIGH

## Summary

Phase 8 standardizes the Lua bindings in `src/lua_runner.cpp` to align with the C++ method names established in Phase 3, and ensures all errors propagate from C++ through sol2 without any custom Lua-side error messages. The Lua bindings are implemented as sol2 `new_usertype` registrations inside the `LuaRunner::Impl` struct, where each C++ `Database` method is exposed to Lua scripts via a string name.

The current state is very close to target. Of the ~55 Lua binding method names, exactly **1 has a name mismatch** with the corresponding C++ method (`delete_element_by_id` in Lua vs `delete_element` in C++). The remaining binding names already match their C++ counterparts. Additionally, there are **3 Lua-only composite methods** (`read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`) that have no C++ public API counterpart and **2 C++ methods** (`export_csv`, `import_csv`) that are not yet bound to Lua. Error handling is already correct: all errors originate from C++ `std::runtime_error` throws, flow through sol2's exception-to-Lua-error conversion, and surface via `LuaRunner::run()` which wraps them with a "Lua error: " prefix.

**Primary recommendation:** Rename the single mismatched binding (`delete_element_by_id` -> `delete_element`), update all Lua test scripts that reference it, decide whether to keep the 3 Lua-only composite methods (they are useful but have no C++ equivalent), and optionally bind `export_csv`/`import_csv`. Verify that the "Lua error: " prefix in `LuaRunner::run()` satisfies the error pattern requirement.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| sol2 | (fetched via CMake FetchContent) | C++/Lua binding library | Already used in project; provides type-safe Lua/C++ interop |
| Lua | 5.5 (fetched via CMake FetchContent) | Embedded scripting | Already used in project |
| GTest | (fetched via CMake FetchContent) | C++ test framework | Already used for all tests |

### Build Configuration
sol2 compile flags in `src/CMakeLists.txt`:
```
SOL_SAFE_NUMERICS=1
SOL_SAFE_FUNCTION=1
```
Note: `SOL_EXCEPTIONS_SAFE_PROPAGATION` is NOT set. This means sol2 uses its default exception handling where C++ exceptions thrown inside bound functions are caught by sol2, converted to Lua errors, then caught by `safe_script` and converted back to `sol::error`. The current `LuaRunner::run()` then wraps the error in a `std::runtime_error` with "Lua error: " prefix. This works correctly for the project's use case.

## Architecture Patterns

### Current Binding Structure

All Lua bindings live in a single file: `src/lua_runner.cpp`, inside `LuaRunner::Impl::bind_database()`. The bindings use sol2's `new_usertype<Database>` to register methods as string-name/lambda pairs:

```cpp
lua.new_usertype<Database>(
    "Database",
    "method_name_as_string",
    [](Database& self, args...) { /* delegate to self.method(...) */ },
    ...
);
```

### Complete Binding Name Audit

#### Methods with NAME MISMATCH (Lua name != C++ name)

| # | Lua Binding Name | C++ Method Name | Type |
|---|-----------------|-----------------|------|
| 1 | `delete_element_by_id` | `delete_element` | RENAME NEEDED |

Phase 3 renamed `delete_element_by_id` to `delete_element` in C++, but preserved the old Lua string name deliberately (for Phase 8 to handle).

#### Methods that MATCH (Lua name == C++ name) -- 49 methods

**Element CRUD:**
- `create_element` -> `Database::create_element`
- `update_element` -> `Database::update_element`

**Read scalars (all):**
- `read_scalar_strings` -> `Database::read_scalar_strings`
- `read_scalar_integers` -> `Database::read_scalar_integers`
- `read_scalar_floats` -> `Database::read_scalar_floats`

**Read scalars (by ID):**
- `read_scalar_string_by_id` -> `Database::read_scalar_string_by_id`
- `read_scalar_integer_by_id` -> `Database::read_scalar_integer_by_id`
- `read_scalar_float_by_id` -> `Database::read_scalar_float_by_id`

**Read vectors (all):**
- `read_vector_integers` -> `Database::read_vector_integers`
- `read_vector_floats` -> `Database::read_vector_floats`
- `read_vector_strings` -> `Database::read_vector_strings`

**Read vectors (by ID):**
- `read_vector_integers_by_id` -> `Database::read_vector_integers_by_id`
- `read_vector_floats_by_id` -> `Database::read_vector_floats_by_id`
- `read_vector_strings_by_id` -> `Database::read_vector_strings_by_id`

**Read sets (all):**
- `read_set_integers` -> `Database::read_set_integers`
- `read_set_floats` -> `Database::read_set_floats`
- `read_set_strings` -> `Database::read_set_strings`

**Read sets (by ID):**
- `read_set_integers_by_id` -> `Database::read_set_integers_by_id`
- `read_set_floats_by_id` -> `Database::read_set_floats_by_id`
- `read_set_strings_by_id` -> `Database::read_set_strings_by_id`

**Read other:**
- `read_element_ids` -> `Database::read_element_ids`

**Update scalars:**
- `update_scalar_integer` -> `Database::update_scalar_integer`
- `update_scalar_float` -> `Database::update_scalar_float`
- `update_scalar_string` -> `Database::update_scalar_string`

**Update vectors:**
- `update_vector_integers` -> `Database::update_vector_integers`
- `update_vector_floats` -> `Database::update_vector_floats`
- `update_vector_strings` -> `Database::update_vector_strings`

**Update sets:**
- `update_set_integers` -> `Database::update_set_integers`
- `update_set_floats` -> `Database::update_set_floats`
- `update_set_strings` -> `Database::update_set_strings`

**Metadata:**
- `get_scalar_metadata` -> `Database::get_scalar_metadata`
- `get_vector_metadata` -> `Database::get_vector_metadata`
- `get_set_metadata` -> `Database::get_set_metadata`
- `get_time_series_metadata` -> `Database::get_time_series_metadata`

**List:**
- `list_scalar_attributes` -> `Database::list_scalar_attributes`
- `list_vector_groups` -> `Database::list_vector_groups`
- `list_set_groups` -> `Database::list_set_groups`
- `list_time_series_groups` -> `Database::list_time_series_groups`

**Relations:**
- `update_scalar_relation` -> `Database::update_scalar_relation`
- `read_scalar_relation` -> `Database::read_scalar_relation`

**Time series data:**
- `read_time_series_group` -> `Database::read_time_series_group`
- `update_time_series_group` -> `Database::update_time_series_group`

**Time series files:**
- `has_time_series_files` -> `Database::has_time_series_files`
- `list_time_series_files_columns` -> `Database::list_time_series_files_columns`
- `read_time_series_files` -> `Database::read_time_series_files`
- `update_time_series_files` -> `Database::update_time_series_files`

**Query:**
- `query_string` -> `Database::query_string`
- `query_integer` -> `Database::query_integer`
- `query_float` -> `Database::query_float`

**Info/Inspection:**
- `describe` -> `Database::describe`
- `is_healthy` -> `Database::is_healthy`
- `current_version` -> `Database::current_version`
- `path` -> `Database::path`

#### Lua-Only Methods (no C++ public API counterpart) -- 3 methods

| Lua Method | What It Does | C++ Methods Called |
|------------|-------------|-------------------|
| `read_all_scalars_by_id` | Reads all scalar attributes for one element into a table | `list_scalar_attributes` + `read_scalar_{type}_by_id` for each |
| `read_all_vectors_by_id` | Reads all vector groups for one element into a table | `list_vector_groups` + `read_vector_{type}_by_id` for each |
| `read_all_sets_by_id` | Reads all set groups for one element into a table | `list_set_groups` + `read_set_{type}_by_id` for each |

These are convenience composites that iterate over metadata and read all data for a single element. They exist only in Lua because Lua scripts commonly need a complete element snapshot. Decision needed: keep them (they are useful), remove them (purity), or add C++ equivalents (feature expansion out of scope).

#### C++ Methods NOT Bound to Lua -- 2 methods

| C++ Method | Why Not Bound | Decision |
|------------|---------------|----------|
| `export_csv(table, path)` | Currently empty stubs | Optional: bind when implemented |
| `import_csv(table, path)` | Currently empty stubs | Optional: bind when implemented |

### Error Handling Architecture

The error flow through the Lua bindings is:

```
C++ Database method throws std::runtime_error("Cannot X: reason")
    -> sol2 catches C++ exception, converts to Lua error
    -> safe_script catches Lua error, returns sol::protected_function_result
    -> LuaRunner::run() checks result.valid()
    -> If invalid: throws std::runtime_error("Lua error: " + err.what())
    -> C API (if used) catches and stores in last_error
```

**Key observations:**
1. No custom error messages are crafted in the Lua binding code itself. The only throw is in `LuaRunner::run()` at line 1123.
2. The "Lua error: " prefix wraps ALL errors (both C++ exceptions surfaced through Lua and pure Lua errors like syntax errors or `assert(false)`).
3. The error message from the C++ layer is preserved intact inside the "Lua error: " wrapper.
4. Pure Lua errors (syntax errors, runtime errors from `error()`, assertion failures) also get the "Lua error: " prefix, which is appropriate since they originate from the Lua VM.

**The "Lua error: " prefix in `LuaRunner::run()` at line 1123:**
```cpp
throw std::runtime_error(std::string("Lua error: ") + err.what());
```

This follows the project's error message Pattern 3: "Failed to {operation}: {reason}". However, it uses "Lua error" instead of "Failed to run". The success criteria say "All Lua error handling uses pcall/error patterns that surface C++ exception messages" -- the current implementation already satisfies this because sol2 internally uses Lua's `pcall`-equivalent mechanism and the C++ exceptions are surfaced intact.

**Decision point for planner:** Should this remain as "Lua error: " or be changed to "Failed to run: " to match Pattern 3 strictly? Recommendation: keep "Lua error: " since it accurately describes the error source and is more useful for debugging than "Failed to run: ".

### Files Touched

**For naming fix (delete_element_by_id -> delete_element):**
- `src/lua_runner.cpp` line 112-113: Change Lua binding string from `"delete_element_by_id"` to `"delete_element"`
- `tests/test_lua_runner.cpp` lines 431, 456, 479, 497, 921: Change `delete_element_by_id` to `delete_element` in Lua script strings

**Total scope:** 2 files, ~6 lines changed.

### Anti-Patterns to Avoid

- **Over-engineering error handling:** The current error flow works correctly. Do not introduce pcall wrappers inside the binding lambdas -- sol2 already handles the C++ exception to Lua error conversion.
- **Adding Lua-side error crafting:** Never write `error("Custom message")` or `return nil, "error"` patterns in the binding functions. Let C++ exceptions propagate naturally through sol2.
- **Removing useful composite methods:** The 3 `read_all_*_by_id` methods are genuinely useful for Lua scripting even though they have no C++ counterpart. They compose existing C++ methods and don't introduce new logic.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Exception-to-Lua-error conversion | Manual pcall wrappers in Lua | sol2's built-in exception handling | sol2 already does this correctly with SOL_SAFE_FUNCTION=1 |
| Error message formatting in Lua | Lua-side string formatting of error messages | C++ std::runtime_error messages | Project principle: error messages defined in C++, bindings surface them |
| Type conversion helpers | Custom type marshaling | sol2's automatic type conversion + existing helper functions | The existing `lua_table_to_*` helpers are already correct |

**Key insight:** The Lua bindings are already thin wrappers. The standardization work is minimal (1 rename). The error handling architecture is already correct by design because sol2 does the heavy lifting.

## Common Pitfalls

### Pitfall 1: Test Script String Matching
**What goes wrong:** Changing the Lua binding name in `lua_runner.cpp` without updating the Lua script strings in test files causes tests to fail with "attempt to call nil" errors.
**Why it happens:** Lua binding names are string literals. The compiler can't detect mismatches between binding registrations and Lua script call sites.
**How to avoid:** Search for the old name (`delete_element_by_id`) in ALL test files, not just `test_lua_runner.cpp`. Also check if any other test files use LuaRunner.
**Warning signs:** Lua test failures mentioning "nil value" or "attempt to call"

### Pitfall 2: Confusing Lua Error Sources
**What goes wrong:** Treating the "Lua error: " prefix as a problem when it is actually correct behavior.
**Why it happens:** The error pattern requirement says "All Lua error handling uses pcall/error patterns." The "Lua error: " prefix is the result of sol2's pcall-based error handling working correctly.
**How to avoid:** Understand that sol2's `safe_script` IS the pcall/error pattern. The C++ exception -> Lua error -> C++ runtime_error pipeline IS the correct implementation of ERRH-05.
**Warning signs:** Attempting to add manual pcall/error wrappers when the existing mechanism already works.

### Pitfall 3: Breaking Lua-Only Composite Methods
**What goes wrong:** Removing `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` because they have no C++ counterpart.
**Why it happens:** Strict interpretation of "every Lua binding method name matches the corresponding C++ method name" implies these should not exist.
**How to avoid:** The success criteria say "Every Lua binding method name matches the corresponding C++ method name." These methods don't have a corresponding C++ method to match against -- they are Lua-specific composites. The requirement is about naming consistency, not API surface parity. Keep them.
**Warning signs:** Removing functionality that tests depend on without a clear requirement to do so.

### Pitfall 4: Scope Creep into Feature Work
**What goes wrong:** Adding `export_csv`/`import_csv` bindings, adding new composite methods, refactoring the binding structure.
**Why it happens:** Phase 8 is about standardization (names and errors), not feature completeness.
**How to avoid:** Stick to the 4 success criteria: (1) names match C++, (2) error handling uses pcall/error, (3) no custom Lua errors, (4) tests pass.
**Warning signs:** Changing more than ~6 lines of code.

## Code Examples

### The Single Required Rename

```cpp
// src/lua_runner.cpp, line 112-113
// BEFORE:
"delete_element_by_id",
[](Database& self, const std::string& collection, int64_t id) { self.delete_element(collection, id); },

// AFTER:
"delete_element",
[](Database& self, const std::string& collection, int64_t id) { self.delete_element(collection, id); },
```

Note: The C++ call `self.delete_element(collection, id)` is already correct (Phase 3 updated it). Only the Lua-facing string name needs to change.

### Test Script Update Pattern

```cpp
// tests/test_lua_runner.cpp
// BEFORE (multiple locations):
lua.run(R"(
    db:delete_element_by_id("Collection", 2)
)");

// AFTER:
lua.run(R"(
    db:delete_element("Collection", 2)
)");
```

### Error Propagation (Already Correct)

```cpp
// Current LuaRunner::run() -- this is already correct for ERRH-05
void LuaRunner::run(const std::string& script) {
    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Lua error: ") + err.what());
    }
}
```

This implements pcall/error pattern via sol2's `safe_script` (which uses `lua_pcall` internally). C++ exception messages are preserved in the error string.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `delete_element_by_id` in C++ | `delete_element` in C++ | Phase 3 (2026-02-10) | Lua binding string not yet updated |
| `set_scalar_relation` in C++ | `update_scalar_relation` in C++ | Phase 3 (2026-02-10) | Lua binding string already updated |
| `read_time_series_group_by_id` in C++ | `read_time_series_group` in C++ | Phase 3 (2026-02-10) | Lua binding string already updated |

**Key observation:** Phase 3's verification report at line 100 explicitly states: "Old names preserved in Lua strings (src/lua_runner.cpp) for Phase 8." The `delete_element_by_id` rename was deliberately deferred.

However, checking the actual code reveals that `update_scalar_relation` and `read_time_series_group` Lua binding strings were ALREADY updated during Phase 3 (they match the new C++ names). Only `delete_element_by_id` was left with the old name.

## Open Questions

### 1. Should the 3 Lua-only composite methods be kept?

**What we know:** `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` have no C++ counterpart. They compose multiple C++ method calls into a single Lua-friendly result.

**What's unclear:** Does success criterion #1 ("Every Lua binding method name matches the corresponding C++ method name") require removing these? Or does it only apply to methods that DO have a C++ counterpart?

**Recommendation:** Keep them. The criterion is about naming consistency for methods that map to C++, not about prohibiting Lua-specific convenience methods. These methods don't violate any naming convention -- their names follow the same `verb_noun_by_id` pattern. No tests would break. Removing them would be destructive with no clear benefit. Confidence: HIGH.

### 2. Should `export_csv` and `import_csv` be bound to Lua?

**What we know:** These are empty stubs in C++. They exist in the C++ public API but are not bound to Lua.

**What's unclear:** Does "every public C++ method should be bound to Lua" (from CLAUDE.md) apply here? These are no-ops.

**Recommendation:** Out of scope for Phase 8. Phase 8 is about naming and error standardization, not feature completeness. Binding empty stubs adds no value. If/when they get implemented, they can be bound at that time. Confidence: HIGH.

### 3. Should the "Lua error: " prefix format change?

**What we know:** `LuaRunner::run()` wraps all errors with `"Lua error: " + err.what()`. The project's error patterns from Phase 3 are: "Cannot {op}: {reason}", "{Entity} not found: {id}", "Failed to {op}: {reason}".

**What's unclear:** Should this be changed to "Failed to run: {message}" to match Pattern 3?

**Recommendation:** Keep "Lua error: " as-is. It is more descriptive than "Failed to run" because it identifies the error source (Lua VM). The LuaRunner is not a Database method -- it is a separate class with its own error semantics. The 3 error patterns from Phase 3 apply to the Database class specifically. Confidence: MEDIUM.

## Sources

### Primary (HIGH confidence)
- Direct source code analysis of `src/lua_runner.cpp` -- complete binding inventory (all 55 method registrations cataloged)
- Direct source code analysis of `include/quiver/database.h` -- all 60 public C++ methods
- Direct source code analysis of `tests/test_lua_runner.cpp` -- all 5 test locations using `delete_element_by_id`
- Direct source code analysis of `src/CMakeLists.txt` -- sol2 compile flags
- Phase 3 verification report (`.planning/phases/03-cpp-naming-error-standardization/03-VERIFICATION.md`) -- confirms old Lua names preserved for Phase 8
- Phase 3 research (`.planning/phases/03-cpp-naming-error-standardization/03-RESEARCH.md`) -- naming convention definition

### Secondary (MEDIUM confidence)
- `.planning/ROADMAP.md` -- Phase 8 success criteria and dependencies
- `.planning/REQUIREMENTS.md` -- NAME-05, ERRH-05 requirement definitions

## Metadata

**Confidence breakdown:**
- Naming audit: HIGH -- complete inventory of all 55 Lua binding names compared against all 60 C++ methods
- Error handling: HIGH -- single error path verified in source code, sol2 mechanism understood
- Scope estimate: HIGH -- exactly 1 rename needed, affecting 2 files and ~6 lines
- Composite methods decision: HIGH -- keeping them is safe, removing them would break tests

**Research date:** 2026-02-10
**Valid until:** 2026-03-10 (stable -- internal refactoring, no external dependency changes)
