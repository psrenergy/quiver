# Phase 5: C API Naming and Error Standardization - Research

**Researched:** 2026-02-10
**Domain:** C API function naming conventions and error handling standardization
**Confidence:** HIGH

## Summary

Phase 5 standardizes the C API layer to match the conventions established in Phase 3 (C++ naming) and Phase 4 (C API decomposition). The work has two dimensions: (1) rename C API functions whose names deviate from the `quiver_{entity}_{operation}` convention, and (2) audit error handling to ensure every function that can fail uses `QUIVER_REQUIRE` for precondition checks and the try-catch-set_last_error pattern.

The good news is that the C API is already 95% consistent. Most functions follow `quiver_database_{operation}` naming. The primary naming issues stem from Phase 3 C++ renames that were deliberately NOT propagated to the C API (per the prior decision: "C API function signatures unchanged"). There are also free functions with inconsistent entity prefixes (`quiver_free_*` vs `quiver_database_free_*`) and a `quiver_string_free` in element.h that breaks the `quiver_{entity}_{operation}` pattern. Error handling is also mostly consistent -- the QUIVER_REQUIRE macro and try-catch pattern are used throughout, with only minor gaps in lua_runner.cpp (missing `QUIVER_C_API` declarations) and `quiver_database_close` (accepts null without QUIVER_REQUIRE, which is intentional and correct).

**Primary recommendation:** Apply the 5 C++ renames from Phase 3 to C API names, standardize free function naming under `quiver_database_free_*`, fix `quiver_string_free` to `quiver_element_free_string`, and audit error handling for consistency. Then re-run Julia/Dart generators.

## Standard Stack

Not applicable -- this phase involves no new libraries, only renaming existing C API functions and auditing error patterns. All tools are already in the project (GTest, CMake, Julia Clang.jl generator, Dart ffigen).

## Architecture Patterns

### Complete C API Function Inventory

#### common.h (3 functions) -- NO NAMING CHANGES NEEDED

| Function | Entity | Convention Check | Notes |
|----------|--------|-----------------|-------|
| `quiver_version()` | global | OK -- utility function | Direct return, no error code |
| `quiver_get_last_error()` | global | OK -- utility function | Direct return, no error code |
| `quiver_clear_last_error()` | global | OK -- utility function | void return |

These are global utility functions without an entity prefix. They don't take a database/element handle. This is correct for utility functions.

#### options.h (0 functions) -- types only, no changes needed

#### database.h - Lifecycle (7 functions)

| Current Name | Convention Check | Issue |
|-------------|-----------------|-------|
| `quiver_database_options_default()` | OK | Direct return (no error code needed) |
| `quiver_database_open(...)` | OK | |
| `quiver_database_from_migrations(...)` | OK | |
| `quiver_database_from_schema(...)` | OK | |
| `quiver_database_close(...)` | OK | Accepts null (delete nullptr is safe). No QUIVER_REQUIRE -- intentional. |
| `quiver_database_is_healthy(...)` | OK | |
| `quiver_database_path(...)` | OK | |

#### database.h - Version (1 function)

| Current Name | Convention Check | Issue |
|-------------|-----------------|-------|
| `quiver_database_current_version(...)` | OK | |

#### database.h - Element CRUD (3 functions)

| Current Name | Convention Check | Issue | Proposed |
|-------------|-----------------|-------|----------|
| `quiver_database_create_element(...)` | OK | | No change |
| `quiver_database_update_element(...)` | OK | | No change |
| `quiver_database_delete_element_by_id(...)` | **RENAME** | C++ was renamed to `delete_element` in Phase 3 | `quiver_database_delete_element(...)` |

#### database.h - Relations (2 functions)

| Current Name | Convention Check | Issue | Proposed |
|-------------|-----------------|-------|----------|
| `quiver_database_update_scalar_relation(...)` | OK | Already matches Phase 3 C++ name | No change |
| `quiver_database_read_scalar_relation(...)` | OK | | No change |

Note: The C API already uses `update_scalar_relation` (was renamed in C++ from `set_scalar_relation`). However, checking the actual header reveals the C API ALSO already uses `quiver_database_update_scalar_relation`. The Phase 3 decision was to update C API call sites but not C API signatures. Let me verify -- reading the header confirms the C API already has `quiver_database_update_scalar_relation`. This was updated alongside Phase 3. So no rename needed here.

#### database.h - Read Scalars (6 functions) -- OK, no changes

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_read_scalar_integers(...)` | OK |
| `quiver_database_read_scalar_floats(...)` | OK |
| `quiver_database_read_scalar_strings(...)` | OK |
| `quiver_database_read_scalar_integer_by_id(...)` | OK |
| `quiver_database_read_scalar_float_by_id(...)` | OK |
| `quiver_database_read_scalar_string_by_id(...)` | OK |

#### database.h - Read Vectors (6 functions) -- OK, no changes

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_read_vector_integers(...)` | OK |
| `quiver_database_read_vector_floats(...)` | OK |
| `quiver_database_read_vector_strings(...)` | OK |
| `quiver_database_read_vector_integers_by_id(...)` | OK |
| `quiver_database_read_vector_floats_by_id(...)` | OK |
| `quiver_database_read_vector_strings_by_id(...)` | OK |

#### database.h - Read Sets (6 functions) -- OK, no changes

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_read_set_integers(...)` | OK |
| `quiver_database_read_set_floats(...)` | OK |
| `quiver_database_read_set_strings(...)` | OK |
| `quiver_database_read_set_integers_by_id(...)` | OK |
| `quiver_database_read_set_floats_by_id(...)` | OK |
| `quiver_database_read_set_strings_by_id(...)` | OK |

#### database.h - Read Other (1 function) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_read_element_ids(...)` | OK |

#### database.h - Metadata Get (4 functions) -- OK, no changes

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_get_scalar_metadata(...)` | OK |
| `quiver_database_get_vector_metadata(...)` | OK |
| `quiver_database_get_set_metadata(...)` | OK |
| `quiver_database_get_time_series_metadata(...)` | OK |

#### database.h - Metadata List (4 functions) -- OK, no changes

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_list_scalar_attributes(...)` | OK |
| `quiver_database_list_vector_groups(...)` | OK |
| `quiver_database_list_set_groups(...)` | OK |
| `quiver_database_list_time_series_groups(...)` | OK |

#### database.h - Update Scalars (3 functions) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_update_scalar_integer(...)` | OK |
| `quiver_database_update_scalar_float(...)` | OK |
| `quiver_database_update_scalar_string(...)` | OK |

#### database.h - Update Vectors (3 functions) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_update_vector_integers(...)` | OK |
| `quiver_database_update_vector_floats(...)` | OK |
| `quiver_database_update_vector_strings(...)` | OK |

#### database.h - Update Sets (3 functions) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_update_set_integers(...)` | OK |
| `quiver_database_update_set_floats(...)` | OK |
| `quiver_database_update_set_strings(...)` | OK |

#### database.h - Time Series Data (2 functions)

| Current Name | Convention Check | Issue | Proposed |
|-------------|-----------------|-------|----------|
| `quiver_database_read_time_series_group(...)` | OK | Already matches Phase 3 name (was `read_time_series_group_by_id` in C++ but was renamed) | No change |
| `quiver_database_update_time_series_group(...)` | OK | | No change |

Wait -- checking the actual header again. The C API header still has `quiver_database_read_time_series_group` (no `_by_id`). Checking if this was already aligned... Yes, looking at `include/quiver/c/database.h` line 349, it is `quiver_database_read_time_series_group`. This is already correct -- it matches the Phase 3 C++ name `read_time_series_group`. No rename needed.

#### database.h - Time Series Files (4 functions) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_has_time_series_files(...)` | OK |
| `quiver_database_list_time_series_files_columns(...)` | OK |
| `quiver_database_read_time_series_files(...)` | OK |
| `quiver_database_update_time_series_files(...)` | OK |

#### database.h - CSV (2 functions)

| Current Name | Convention Check | Issue | Proposed |
|-------------|-----------------|-------|----------|
| `quiver_database_export_csv(...)` | OK | Already matches Phase 3 name | No change |
| `quiver_database_import_csv(...)` | OK | Already matches Phase 3 name | No change |

#### database.h - Query (6 functions) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_query_string(...)` | OK |
| `quiver_database_query_integer(...)` | OK |
| `quiver_database_query_float(...)` | OK |
| `quiver_database_query_string_params(...)` | OK |
| `quiver_database_query_integer_params(...)` | OK |
| `quiver_database_query_float_params(...)` | OK |

#### database.h - Describe (1 function) -- OK

| Current Name | Convention Check |
|-------------|-----------------|
| `quiver_database_describe(...)` | OK |

#### database.h - Free Functions (12 functions) -- NAMING ISSUES

| Current Name | Convention Check | Issue | Proposed |
|-------------|-----------------|-------|----------|
| `quiver_free_scalar_metadata(...)` | **RENAME** | Missing entity prefix; frees database-owned data | `quiver_database_free_scalar_metadata(...)` |
| `quiver_free_group_metadata(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_group_metadata(...)` |
| `quiver_free_scalar_metadata_array(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_scalar_metadata_array(...)` |
| `quiver_free_group_metadata_array(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_group_metadata_array(...)` |
| `quiver_free_time_series_data(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_time_series_data(...)` |
| `quiver_free_time_series_files(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_time_series_files(...)` |
| `quiver_free_integer_array(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_integer_array(...)` |
| `quiver_free_float_array(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_float_array(...)` |
| `quiver_free_string_array(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_string_array(...)` |
| `quiver_free_integer_vectors(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_integer_vectors(...)` |
| `quiver_free_float_vectors(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_float_vectors(...)` |
| `quiver_free_string_vectors(...)` | **RENAME** | Missing entity prefix | `quiver_database_free_string_vectors(...)` |

The convention `quiver_{entity}_{operation}` means free functions that release database-returned data should be `quiver_database_free_*`. This makes clear which entity "owns" the data being freed. All 12 free functions in database.h need the `database_` entity prefix added.

#### element.h (12 functions)

| Current Name | Convention Check | Issue | Proposed |
|-------------|-----------------|-------|----------|
| `quiver_element_create(...)` | OK | | |
| `quiver_element_destroy(...)` | OK | | |
| `quiver_element_clear(...)` | OK | | |
| `quiver_element_set_integer(...)` | OK | | |
| `quiver_element_set_float(...)` | OK | | |
| `quiver_element_set_string(...)` | OK | | |
| `quiver_element_set_null(...)` | OK | | |
| `quiver_element_set_array_integer(...)` | OK | | |
| `quiver_element_set_array_float(...)` | OK | | |
| `quiver_element_set_array_string(...)` | OK | | |
| `quiver_element_has_scalars(...)` | OK | | |
| `quiver_element_has_arrays(...)` | OK | | |
| `quiver_element_scalar_count(...)` | OK | | |
| `quiver_element_array_count(...)` | OK | | |
| `quiver_element_to_string(...)` | OK | | |
| `quiver_string_free(...)` | **RENAME** | Should be `quiver_element_free_string` to follow entity pattern | `quiver_element_free_string(...)` |

`quiver_string_free` is in element.h and frees a string returned by `quiver_element_to_string`. It should be `quiver_element_free_string` to follow the `quiver_{entity}_{operation}` convention.

#### lua_runner.h (4 functions)

| Current Name | Convention Check | Issue |
|-------------|-----------------|-------|
| `quiver_lua_runner_new(...)` | OK | Follows `quiver_lua_runner_{operation}` |
| `quiver_lua_runner_free(...)` | OK | |
| `quiver_lua_runner_run(...)` | OK | |
| `quiver_lua_runner_get_error(...)` | OK | |

No naming issues in lua_runner.h. However, the implementations in `src/c/lua_runner.cpp` are missing `QUIVER_C_API` declarations (the functions are declared in the header with `QUIVER_C_API` but the implementations don't have `extern "C"` wrapping or `QUIVER_C_API` annotations). This needs verification -- actually, looking more closely, `lua_runner.cpp` functions are not inside an `extern "C"` block. They ARE declared as `extern "C"` in the header, and since the .cpp includes the header, the linkage should be correct. But for consistency with other files (which use `extern "C" { ... }`), this should be normalized.

### Naming Issues Summary

| # | Current Name | Issue | Proposed Name | Files Affected |
|---|-------------|-------|--------------|----------------|
| 1 | `quiver_database_delete_element_by_id` | `_by_id` suffix removed in C++ Phase 3 | `quiver_database_delete_element` | header, src/c/database_create.cpp, tests, Julia/Dart bindings |
| 2 | `quiver_string_free` | Wrong entity prefix; belongs to element | `quiver_element_free_string` | element.h, element.cpp, tests, Julia/Dart bindings |
| 3-14 | `quiver_free_*` (12 functions) | Missing `database_` entity prefix | `quiver_database_free_*` | database.h, src/c/database_*.cpp, tests, Julia/Dart bindings |

**Total renames: 14 functions** (1 delete rename + 1 element free rename + 12 database free renames)

### Error Handling Audit

#### Pattern Compliance Check

Every C API function that can fail should:
1. Use `QUIVER_REQUIRE(...)` for null pointer precondition checks
2. Wrap C++ calls in `try { ... } catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`
3. Return `quiver_error_t` (`QUIVER_OK` or `QUIVER_ERROR`)

**Functions that correctly follow the pattern (ALL of database_*.cpp, element.cpp):**
- Every function in `database.cpp`, `database_create.cpp`, `database_read.cpp`, `database_update.cpp`, `database_relations.cpp`, `database_query.cpp`, `database_metadata.cpp`, `database_time_series.cpp` -- all use QUIVER_REQUIRE and try-catch.
- Every function in `element.cpp` uses QUIVER_REQUIRE and try-catch where applicable.

**Functions with minor error handling notes:**

1. **`quiver_database_close`**: Does NOT use QUIVER_REQUIRE for null check. Calls `delete db` directly. This is INTENTIONAL and CORRECT -- `delete nullptr` is safe in C++, and forcing callers to check for null before close would be a worse API. **No change needed.**

2. **`quiver_database_is_healthy`** and **`quiver_database_path`**: Use QUIVER_REQUIRE but no try-catch because `is_healthy()` and `path()` cannot throw (they're simple accessors). **No change needed** -- adding unnecessary try-catch would be defensive coding, which the project philosophy explicitly avoids.

3. **`quiver_lua_runner_free`**: Does NOT use QUIVER_REQUIRE. Calls `delete runner` directly. Same rationale as `quiver_database_close`. **No change needed.**

4. **`quiver_lua_runner_run` and `quiver_lua_runner_get_error`**: Use QUIVER_REQUIRE and try-catch. But `quiver_lua_runner_run` catches `...` (catch-all) in addition to `std::exception`. This is MORE defensive than other functions but appropriate for Lua (which can throw non-std exceptions). **No change needed.**

5. **`quiver_lua_runner_new`**: Uses both `std::exception` and `...` catches. Same rationale as above. **No change needed.**

6. **`src/c/lua_runner.cpp`**: Functions are NOT wrapped in `extern "C" { }`. They rely on the header's `extern "C"` linkage. While this works, it's inconsistent with all other .cpp files which explicitly wrap in `extern "C" { ... }`. **Should be normalized for consistency.**

7. **`quiver_lua_runner_*` functions**: Missing `QUIVER_C_API` in implementation. The header has `QUIVER_C_API` (for dllexport/dllimport), but the implementation file doesn't repeat it. This is actually fine -- the DLL export comes from the declaration, not the definition. However, all other implementation files DO include the `QUIVER_C_API` macro. **Should be normalized for consistency.**

#### Error Handling Summary

The error handling is already consistent across the codebase. The only changes needed are cosmetic normalizations in lua_runner.cpp:
- Wrap function bodies in `extern "C" { ... }` block
- Add `QUIVER_C_API` to function definitions for consistency

### Cascade Impact Analysis

Renaming C API functions affects:

1. **C API Headers** (`include/quiver/c/database.h`, `include/quiver/c/element.h`)
2. **C API Implementation** (`src/c/database_*.cpp`, `src/c/element.cpp`)
3. **C API Tests** (`tests/test_c_api_database_*.cpp`, `tests/test_c_api_element.cpp`)
4. **Julia Generated Bindings** (`bindings/julia/src/c_api.jl`) -- auto-generated, re-run generator
5. **Julia Wrapper Code** (`bindings/julia/src/database_*.jl`, `bindings/julia/src/element.jl`) -- manually references C API names via `C.quiver_*`
6. **Dart Generated Bindings** (`bindings/dart/lib/src/ffi/bindings.dart`) -- auto-generated, re-run generator
7. **Dart Wrapper Code** (`bindings/dart/lib/src/database_*.dart`, `bindings/dart/lib/src/element.dart`) -- manually references `bindings.quiver_*`

**Critical insight:** The Julia and Dart wrapper code manually references C API function names (e.g., `C.quiver_free_integer_array`, `bindings.quiver_database_delete_element_by_id`). Re-running generators updates the FFI layer (`c_api.jl`, `bindings.dart`), but the wrapper code that calls these generated functions must be manually updated too.

### Recommended Execution Strategy

**Plan 1: Rename C API functions and update all call sites**
- Rename 14 functions (1 delete, 1 string_free, 12 free_* functions)
- Update C API headers, implementations, tests
- Update Julia and Dart wrapper code (manual references to C API names)
- Re-run Julia/Dart generators

**Plan 2: Normalize error handling in lua_runner.cpp**
- Add `extern "C"` block wrapping
- Add `QUIVER_C_API` to function definitions
- Verify no behavioral changes

These two plans can be done in either order, but Plan 1 is the bulk of the work.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FFI binding updates | Manual edits to generated FFI code | Re-run generators (`bindings/julia/generator/generator.bat`, `bindings/dart/generator/generator.bat`) | Generators read C headers and auto-produce correct FFI bindings |
| Mass renames | Manual find-and-replace across files | Systematic rename per function with grep verification | Each rename touches header, impl, tests, Julia wrappers, Dart wrappers -- must be exhaustive |

**Key insight:** After renaming C API functions, the generated FFI files (`c_api.jl`, `bindings.dart`) will be regenerated automatically by generators. But the WRAPPER code that calls those generated bindings must be manually updated -- the generators don't touch wrapper files.

## Common Pitfalls

### Pitfall 1: Incomplete Rename Cascade
**What goes wrong:** Renaming a C API function in the header but missing one or more call sites in tests, Julia wrappers, or Dart wrappers.
**Why it happens:** A single C API function name appears in 5-7 locations (header, impl, C test, Julia c_api.jl, Julia wrapper, Dart bindings.dart, Dart wrapper).
**How to avoid:** After each rename, grep the ENTIRE repository for the old name. Only planning docs should contain old names.
**Warning signs:** Link errors (undefined symbol), runtime errors in Julia/Dart tests.

### Pitfall 2: Generator Produces Stale Output
**What goes wrong:** Running the Julia/Dart generators before the C library is rebuilt, so they parse old headers.
**Why it happens:** Generators read the C header files, not the compiled library. If the headers are updated but the build directory has stale artifacts, tools may get confused.
**How to avoid:** Update headers first, then re-run generators. The generators parse header files directly, so they don't need a fresh build. But verify the generated output matches expected renames.
**Warning signs:** Generated c_api.jl still has old function names.

### Pitfall 3: Julia/Dart Wrapper Code Not Updated
**What goes wrong:** Generators update the FFI layer but wrapper code still references old FFI function names.
**Why it happens:** The Julia generator produces `c_api.jl` with function wrappers like `quiver_free_integer_array(...)`. The Julia wrapper files (e.g., `database_read.jl`) call these as `C.quiver_free_integer_array(...)`. If the generator output changes the function name, the wrapper must change its call site too.
**How to avoid:** After re-running generators, grep all wrapper files for old function names. Systematically update each one.
**Warning signs:** Julia `UndefRefError` or Dart `NoSuchMethodError` at test time.

### Pitfall 4: Free Function Renames Break Test Cleanup
**What goes wrong:** Tests call `quiver_free_integer_array(values)` for cleanup. If renamed to `quiver_database_free_integer_array(values)` but a test file is missed, the test won't compile.
**Why it happens:** Free functions are called extensively in test cleanup code. Each test file has multiple calls.
**How to avoid:** This is actually self-detecting -- the compiler will catch it. Compile-then-fix cycle works. But doing a complete grep upfront saves time.
**Warning signs:** Compilation errors in test files (good -- caught at compile time).

### Pitfall 5: Lua Runner Implementation Missing extern "C"
**What goes wrong:** On some compilers/platforms, C++ name mangling could theoretically interfere with linking if the header declares `extern "C"` but the implementation doesn't.
**Why it happens:** The implementation relies on the header's `extern "C"` declaration to set linkage. This works in practice but is a landmine for future refactoring.
**How to avoid:** Add explicit `extern "C" { ... }` wrapping in lua_runner.cpp, matching all other C API implementation files.
**Warning signs:** Link errors on non-MSVC compilers (unlikely but possible).

## Code Examples

### C API Rename Pattern

For each renamed function, changes touch 5-7 locations:

```cpp
// 1. include/quiver/c/database.h (declaration)
QUIVER_C_API quiver_error_t quiver_database_delete_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id);
// Was: quiver_database_delete_element_by_id

// 2. src/c/database_create.cpp (implementation)
QUIVER_C_API quiver_error_t quiver_database_delete_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id) {
    QUIVER_REQUIRE(db, collection);
    try {
        db->db.delete_element(collection, id);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// 3. tests/test_c_api_database_delete.cpp
err = quiver_database_delete_element(db, "Configuration", id);
// Was: quiver_database_delete_element_by_id

// 4. bindings/julia/src/c_api.jl (auto-generated by generator)
function quiver_database_delete_element(db, collection, id)
    @ccall libquiver_c.quiver_database_delete_element(...)::quiver_error_t
end

// 5. bindings/julia/src/database_delete.jl (manual wrapper update)
check(C.quiver_database_delete_element(db.ptr, collection, id))
// Was: C.quiver_database_delete_element_by_id

// 6. bindings/dart/lib/src/ffi/bindings.dart (auto-generated by ffigen)
// 7. bindings/dart/lib/src/database_delete.dart (manual wrapper update)
```

### Free Function Rename Pattern

```cpp
// BEFORE (database.h):
QUIVER_C_API quiver_error_t quiver_free_integer_array(int64_t* values);

// AFTER (database.h):
QUIVER_C_API quiver_error_t quiver_database_free_integer_array(int64_t* values);

// Implementation in database_read.cpp -- rename function signature only
// Tests -- rename all call sites
// Julia c_api.jl -- auto-regenerated
// Julia wrappers -- update C.quiver_free_integer_array -> C.quiver_database_free_integer_array
// Dart bindings.dart -- auto-regenerated
// Dart wrappers -- update bindings.quiver_free_integer_array -> bindings.quiver_database_free_integer_array
```

### Error Handling Normalization Pattern

```cpp
// BEFORE (lua_runner.cpp):
quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner) {
    QUIVER_REQUIRE(db, out_runner);
    // ...
}

// AFTER (lua_runner.cpp):
extern "C" {

QUIVER_C_API quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner) {
    QUIVER_REQUIRE(db, out_runner);
    // ...
}

// ... all other functions ...

}  // extern "C"
```

### Files Touched (Complete List)

**For naming changes (Plan 1):**

Header files:
- `include/quiver/c/database.h` -- rename 13 functions (1 delete + 12 free)
- `include/quiver/c/element.h` -- rename 1 function (quiver_string_free)

Implementation files:
- `src/c/database_create.cpp` -- rename quiver_database_delete_element_by_id
- `src/c/database_read.cpp` -- rename 9 free functions (scalar + vector arrays)
- `src/c/database_metadata.cpp` -- rename 4 free functions (metadata)
- `src/c/database_time_series.cpp` -- rename 2 free functions (time series data + files)
- `src/c/element.cpp` -- rename quiver_string_free

C API test files:
- `tests/test_c_api_database_lifecycle.cpp` -- references delete_element_by_id, update_scalar_relation, free functions
- `tests/test_c_api_database_delete.cpp` -- references delete_element_by_id
- `tests/test_c_api_database_create.cpp` -- references free functions
- `tests/test_c_api_database_read.cpp` -- references free functions extensively
- `tests/test_c_api_database_update.cpp` -- may reference free functions
- `tests/test_c_api_database_query.cpp` -- may reference free functions
- `tests/test_c_api_database_time_series.cpp` -- references time series free functions
- `tests/test_c_api_element.cpp` -- references quiver_string_free

Julia bindings (re-run generator + manual wrapper updates):
- `bindings/julia/src/c_api.jl` -- auto-regenerated
- `bindings/julia/src/database_read.jl` -- manual: update free function calls
- `bindings/julia/src/database_delete.jl` -- manual: update delete call
- `bindings/julia/src/database_create.jl` -- manual: check for free function calls
- `bindings/julia/src/database_metadata.jl` -- manual: update metadata free calls
- `bindings/julia/src/element.jl` -- manual: update quiver_string_free
- `bindings/julia/src/database_query.jl` -- manual: update quiver_string_free calls

Dart bindings (re-run generator + manual wrapper updates):
- `bindings/dart/lib/src/ffi/bindings.dart` -- auto-regenerated
- `bindings/dart/lib/src/database_read.dart` -- manual: update free function calls
- `bindings/dart/lib/src/database_delete.dart` -- manual: update delete call
- `bindings/dart/lib/src/database_create.dart` -- manual: check for free function calls
- `bindings/dart/lib/src/database_metadata.dart` -- manual: update metadata free calls
- `bindings/dart/lib/src/element.dart` -- manual: update quiver_string_free
- `bindings/dart/lib/src/database_query.dart` -- manual: update quiver_string_free calls
- `bindings/dart/lib/src/database_relations.dart` -- manual: check for free calls
- `bindings/dart/lib/src/database_csv.dart` -- manual: check for export/import references

**For error handling normalization (Plan 2):**
- `src/c/lua_runner.cpp` -- add extern "C" block, add QUIVER_C_API to definitions

### Rename Mapping Table

Complete old-to-new name mapping for mechanical rename:

| Old Name | New Name | Header | Impl File |
|----------|----------|--------|-----------|
| `quiver_database_delete_element_by_id` | `quiver_database_delete_element` | database.h | database_create.cpp |
| `quiver_string_free` | `quiver_element_free_string` | element.h | element.cpp |
| `quiver_free_scalar_metadata` | `quiver_database_free_scalar_metadata` | database.h | database_metadata.cpp |
| `quiver_free_group_metadata` | `quiver_database_free_group_metadata` | database.h | database_metadata.cpp |
| `quiver_free_scalar_metadata_array` | `quiver_database_free_scalar_metadata_array` | database.h | database_metadata.cpp |
| `quiver_free_group_metadata_array` | `quiver_database_free_group_metadata_array` | database.h | database_metadata.cpp |
| `quiver_free_time_series_data` | `quiver_database_free_time_series_data` | database.h | database_time_series.cpp |
| `quiver_free_time_series_files` | `quiver_database_free_time_series_files` | database.h | database_time_series.cpp |
| `quiver_free_integer_array` | `quiver_database_free_integer_array` | database.h | database_read.cpp |
| `quiver_free_float_array` | `quiver_database_free_float_array` | database.h | database_read.cpp |
| `quiver_free_string_array` | `quiver_database_free_string_array` | database.h | database_read.cpp |
| `quiver_free_integer_vectors` | `quiver_database_free_integer_vectors` | database.h | database_read.cpp |
| `quiver_free_float_vectors` | `quiver_database_free_float_vectors` | database.h | database_read.cpp |
| `quiver_free_string_vectors` | `quiver_database_free_string_vectors` | database.h | database_read.cpp |

## State of the Art

Not applicable -- this is internal refactoring, not adoption of external technology.

## Open Questions

### 1. Should free functions stay in database.h or move to a separate header?

**What we know:** All 12 `quiver_free_*` functions are currently declared in `database.h`. After renaming to `quiver_database_free_*`, they fit naturally with the database entity.

**What's unclear:** Whether splitting free functions into a separate header would improve organization.

**Recommendation:** Keep them in `database.h`. They free data returned by database operations. Splitting would create a header with no clear entity ownership and force callers to include an extra header. Confidence: HIGH.

### 2. Should `quiver_element_free_string` move to its own header or stay in element.h?

**What we know:** `quiver_string_free` is declared in element.h. After renaming to `quiver_element_free_string`, it naturally belongs there (it frees data returned by `quiver_element_to_string`).

**Recommendation:** Keep in element.h. Confidence: HIGH.

### 3. Should the Julia/Dart generators be run BEFORE or AFTER manual wrapper updates?

**What we know:** Generators read C headers and produce FFI binding files. Wrapper code references functions from the generated FFI layer.

**Recommendation:** Run generators FIRST. This produces the new function names in the FFI layer. Then update wrapper code to reference the new names. If you update wrappers first, they'll reference functions that don't exist in the FFI layer yet. Confidence: HIGH.

### 4. Are there any C API functions that Phase 3 renamed in C++ but where the C API was NOT updated?

**What we know:** Phase 3 renamed 5 C++ methods. The prior decision said "C API function signatures unchanged." Let me verify each:

| C++ Rename | C API Current | Needs Rename? |
|-----------|--------------|---------------|
| `delete_element_by_id` -> `delete_element` | `quiver_database_delete_element_by_id` | **YES** |
| `set_scalar_relation` -> `update_scalar_relation` | `quiver_database_update_scalar_relation` | NO (already correct) |
| `export_to_csv` -> `export_csv` | `quiver_database_export_csv` | NO (already correct) |
| `import_from_csv` -> `import_csv` | `quiver_database_import_csv` | NO (already correct) |
| `read_time_series_group_by_id` -> `read_time_series_group` | `quiver_database_read_time_series_group` | NO (already correct) |

So only `delete_element_by_id` needs the Phase 3 rename propagated. The other 4 were apparently already updated in the C API during Phase 3 or Phase 4 (the internal C++ call sites were updated alongside).

**Recommendation:** Rename only `quiver_database_delete_element_by_id` -> `quiver_database_delete_element`. Confidence: HIGH.

## Sources

### Primary (HIGH confidence)
- Direct source code analysis of `include/quiver/c/database.h` -- complete function inventory (all 70+ functions)
- Direct source code analysis of `include/quiver/c/element.h` -- complete element API inventory (16 functions)
- Direct source code analysis of `include/quiver/c/lua_runner.h` -- complete Lua runner API (4 functions)
- Direct source code analysis of `include/quiver/c/common.h` -- global utility functions (3 functions)
- Direct source code analysis of all `src/c/*.cpp` files -- error handling pattern verification
- Direct source code analysis of `src/c/internal.h` -- QUIVER_REQUIRE macro definition
- Direct source code analysis of `src/c/database_helpers.h` -- helper templates
- Direct source code analysis of all `tests/test_c_api_*.cpp` files -- test call sites
- Direct source code analysis of `bindings/julia/src/c_api.jl` -- generated Julia FFI bindings
- Direct source code analysis of `bindings/julia/src/database_read.jl` -- Julia wrapper code
- Direct source code analysis of `bindings/dart/lib/src/ffi/bindings.dart` config (`ffigen.yaml`)
- Direct source code analysis of `bindings/dart/lib/src/database_read.dart` -- Dart wrapper code
- Phase 3 Research (`03-RESEARCH.md`) -- C++ naming convention decisions
- Phase 3 Plan (`03-01-PLAN.md`) -- specific renames applied, cascade decisions

### Secondary (MEDIUM confidence)
- Roadmap (`roadmap.md`) -- Phase 5 success criteria, dependency chain
- CLAUDE.md -- project principles and C API patterns

## Metadata

**Confidence breakdown:**
- Naming audit: HIGH -- every C API function cataloged from headers, issues identified by comparing against `quiver_{entity}_{operation}` convention
- Error handling audit: HIGH -- every implementation file read, QUIVER_REQUIRE and try-catch usage verified line-by-line
- Cascade impact: HIGH -- all downstream consumers (tests, Julia, Dart) identified with specific file lists
- Rename count: HIGH -- exactly 14 functions need renaming, complete mapping table provided

**Research date:** 2026-02-10
**Valid until:** 2026-03-10 (stable -- internal refactoring, no external dependencies)
