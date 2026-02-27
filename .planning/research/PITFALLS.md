# Domain Pitfalls

**Domain:** Code quality improvements for multi-language FFI library (C++/C/Julia/Dart/Python/Lua)
**Researched:** 2026-02-27
**Milestone:** v0.5 Code Quality

## Critical Pitfalls

Mistakes that cause crashes, silent corruption, or require cascading rewrites across layers.

### Pitfall 1: Partial Rename of `quiver_element_free_string` Leaves Dangling Symbol References

**What goes wrong:** Renaming or adding a free function alias in the C API header without updating every consumer creates link-time failures in some bindings and runtime crashes in others. The function `quiver_element_free_string` is currently used by Julia (`c_api.jl`, `database_query.jl`, `database_read.jl`), Dart (`bindings.dart`, `database_query.dart`, `database_read.dart`), and Python (`_c_api.py`, `database.py`) to free strings returned by both element operations AND query operations. That is the semantic confusion -- it lives in `element.cpp` but frees query result strings too.

**Why it happens:** The rename seems local ("just change one function name") but the symbol is resolved at runtime via FFI. Each binding has a separate copy of the symbol name:
- Julia: hand-written `@ccall` in `c_api.jl` (generated) and used directly in `database_query.jl` and `database_read.jl`
- Dart: auto-generated `bindings.dart` via ffigen, used in `database_query.dart` and `database_read.dart`
- Python: hand-written CFFI `cdef` in `_c_api.py`, used in `database.py` and `element.py`

**Consequences:** If the old symbol is removed without updating all bindings, Julia gets an `UnsatisfiedLinkError` at `@ccall` time, Dart crashes with an unresolved symbol at `_lookup`, and Python gets a CFFI `FFIError` on `dlopen` or a runtime `AttributeError`. Tests may pass in one language and crash in another.

**Prevention:**
1. Keep the old `quiver_element_free_string` as a thin inline wrapper calling the new function in the header for one release cycle (or just keep both since this is a WIP project with no backwards compat requirement -- delete the old one only after all bindings are updated).
2. Use a checklist: C header -> C implementation -> regenerate Julia `c_api.jl` -> regenerate Dart `bindings.dart` -> update Python `_c_api.py` cdef -> update each binding's usage sites -> run all test suites.
3. Run `scripts/test-all.bat` as the final gate. A rename is only complete when all five test suites pass.

**Detection:** Any single binding's test suite failing with "symbol not found" or "undefined function" after a C API header change.

### Pitfall 2: Changing `quiver_database_options_t` Struct Layout Breaks CFFI ABI-Mode Python Binding

**What goes wrong:** When fixing the layer inversion (moving `DatabaseOptions` ownership from C to C++), changing the struct definition in `include/quiver/c/options.h` changes the ABI layout. Python uses CFFI ABI-mode (`ffi.dlopen`) which performs no compile-time validation -- the struct layout in `_c_api.py` must exactly match the binary layout in the DLL. A mismatch causes silent memory corruption, not a clean error.

**Why it happens:** CFFI ABI-mode trusts the Python-side `cdef` struct declaration to match the C binary layout byte-for-byte. If a field is added, removed, reordered, or its type changes, CFFI reads/writes memory at wrong offsets. Unlike API-mode (which compiles against headers), ABI-mode has zero compile-time safety net. The Julia and Dart generators will catch layout changes during regeneration, but Python's hand-written `_c_api.py` will not.

**Consequences:** Fields read from the options struct contain garbage values. Writes corrupt adjacent memory. The database may open with wrong options (e.g., `read_only` reads the `console_level` bits, or vice versa). Symptoms are non-deterministic and may only surface under specific option combinations.

**Prevention:**
1. When modifying any struct in `include/quiver/c/options.h`, immediately update `bindings/python/src/quiverdb/_c_api.py` to match field-for-field.
2. Add a C API test that validates struct sizes match expectations: `static_assert(sizeof(quiver_database_options_t) == expected_size)`. This catches unintentional layout changes.
3. Run the Python generator (`bindings/python/generator/generator.bat`) and diff `_declarations.py` against `_c_api.py` to verify consistency.
4. If the layer inversion results in a new C++ `DatabaseOptions` type that the C struct merely mirrors, ensure the C-to-C++ conversion function (`convert_options` pattern in `database_options.h`) handles every field.

**Detection:** Python tests that exercise non-default option values. A test that opens a database with `read_only=True` and verifies write operations fail will catch layout corruption that a default-options test would miss.

### Pitfall 3: Layer Inversion Fix Creates Circular Include Between C++ and C Headers

**What goes wrong:** The current layer inversion is: `include/quiver/options.h` (C++) includes `include/quiver/c/options.h` (C) and defines `using DatabaseOptions = quiver_database_options_t`. Fixing this means C++ should own the type and C should convert. But if the C++ header defines a new `DatabaseOptions` struct and the C API implementation needs both the C++ type and the C type, an `#include` cycle can form if not carefully managed.

**Why it happens:** The current codebase has a clean dependency: C++ -> C header. Reversing this to C -> C++ header requires the C implementation files (which already include the C++ `database.h` via `internal.h`) to also include the new C++ options header. The risk is that the C++ options header might depend on C++ standard library types (`std::string`, `std::unordered_map`) that cannot appear in a C header, creating a situation where the C API public header cannot directly include the C++ options header. The conversion must happen only in `.cpp` files, not headers.

**Consequences:** Build failure across all platforms if includes cycle. Or, if resolved incorrectly by duplicating type definitions, the types diverge silently when one is updated but not the other.

**Prevention:**
1. C++ options header (`include/quiver/options.h`) defines `DatabaseOptions` as a C++-native type. It does NOT include the C options header.
2. C options header (`include/quiver/c/options.h`) defines `quiver_database_options_t` as a POD struct. It does NOT include the C++ options header.
3. Conversion functions live in `src/c/database_options.h` (internal, not public), which includes both headers. This is where the existing `convert_options` pattern already lives.
4. The `using DatabaseOptions = quiver_database_options_t` alias in `include/quiver/options.h` is replaced with a proper C++ struct definition.

**Detection:** Build failure. This one is caught at compile time, but fixing it after the fact can cascade into significant restructuring if the wrong include pattern was chosen initially.

### Pitfall 4: Python LuaRunner Binding Exposes Lua State Lifetime Hazard

**What goes wrong:** Adding a Python `LuaRunner` wrapper creates a double-resource lifetime problem. The `LuaRunner` C API handle (`quiver_lua_runner_t`) borrows a reference to `quiver_database_t` -- it does not own it. If the Python `Database` object is garbage-collected or explicitly closed while a `LuaRunner` still holds a reference to its internal `quiver_database_t`, calling `quiver_lua_runner_run` triggers use-after-free.

**Why it happens:** Python's garbage collector has no concept of C-level ownership relationships. The `quiver_lua_runner_new` function takes a raw `quiver_database_t*` and stores it internally. The Python `LuaRunner` wrapper will hold a CFFI pointer to the runner, but Python does not know that the runner's validity depends on the database staying alive. Julia and Dart handle this by storing a reference to the Database object in the LuaRunner wrapper, preventing premature collection.

**Consequences:** Segfault or memory corruption when running Lua scripts after the database is closed. The crash may be delayed and non-deterministic depending on when Python's GC runs.

**Prevention:**
1. The Python `LuaRunner` class must store a reference to the `Database` instance (not just the `quiver_database_t*` pointer). This prevents GC from collecting the database while the runner exists.
2. The `LuaRunner.__init__` should accept a `Database` object, verify it is not closed, and store `self._db = db` to prevent collection.
3. Add a guard: if `self._db._closed`, raise `QuiverError` before any `quiver_lua_runner_run` call.
4. Implement `__del__` or a context manager to call `quiver_lua_runner_free` on cleanup.

**Detection:** A test that explicitly closes the database, then attempts to use the LuaRunner. Without the reference-holding pattern, this test will segfault. With the pattern, it should raise a clean Python exception.

## Moderate Pitfalls

### Pitfall 5: Python Magic Number Replacement Introduces Name Shadowing with CFFI Enum Constants

**What goes wrong:** Replacing magic numbers (`0`, `1`, `2`, `3`, `4`) with named constants like `DATA_TYPE_INTEGER = 0` in a Python module collides with the CFFI-declared `QUIVER_DATA_TYPE_INTEGER` enum values that are accessible on the `lib` object. Callers might use either form, leading to two parallel sets of constants with no enforced consistency.

**Why it happens:** CFFI ABI-mode exposes C enum values through the `lib` handle (e.g., `lib.QUIVER_DATA_TYPE_INTEGER`). If Python also defines `DataType.INTEGER = 0` as a Python-side constant, there are now two sources of truth. If the C enum values ever change (unlikely but possible), the Python constants silently diverge.

**Consequences:** Inconsistent API surface. Users may import `DataType.INTEGER` while internal code uses `lib.QUIVER_DATA_TYPE_INTEGER`. Code reviews become harder because the same concept has two names.

**Prevention:**
1. Define Python constants that derive their values FROM the CFFI lib object at module import time: `INTEGER = lib.QUIVER_DATA_TYPE_INTEGER`. This ensures they cannot diverge.
2. Alternatively, use a simple Python `IntEnum` class with hardcoded values matching the C enum, but document clearly that these must be updated when the C enum changes.
3. The recommended approach for this project (since CFFI ABI-mode is used): create a `DataType` IntEnum in a dedicated module (e.g., `constants.py`) and use it everywhere. The hardcoded values are acceptable because the C API explicitly documents `INTEGER=0, FLOAT=1, STRING=2, DATE_TIME=3, NULL=4` and these are stable.

**Detection:** A test that asserts `DataType.INTEGER == lib.QUIVER_DATA_TYPE_INTEGER` for all variants catches divergence.

### Pitfall 6: Adding `is_healthy()` / `path()` Tests Exposes the Dangling Pointer in `quiver_database_path`

**What goes wrong:** The CONCERNS.md already documents that `quiver_database_path` returns a `const char*` pointing into the internal C++ `std::string`. When adding cross-binding tests for `path()`, a naive test that reads the path after closing the database will trigger undefined behavior. The test itself becomes the reproducer for the existing bug.

**Why it happens:** Adding test coverage forces confrontation with an existing unsafe API. The path pointer is valid only while the database is open, but no binding currently documents or enforces this. The Python binding (`database.py`) already calls `path()` correctly (immediate decode before close), but a test that stores the pointer and reads it later will crash.

**Consequences:** If the test is written carelessly, it creates a flaky test that sometimes passes (if the memory has not been reused) and sometimes crashes. On Windows, the crash may manifest as an access violation in the test runner.

**Prevention:**
1. In all bindings' `path()` tests, always read the path while the database is open. Never store the raw C pointer across a close boundary.
2. Consider this the forcing function to fix the underlying issue: change `quiver_database_path` to use `strdup_safe` and add a corresponding `quiver_database_free_path` function. This makes the returned string independent of the database lifetime.
3. If the fix is deferred, document the test constraint clearly: "path() must be called before close()".

**Detection:** ASAN (AddressSanitizer) build catches use-after-free immediately. Without ASAN, the test may pass on some platforms and fail on others.

### Pitfall 7: Regenerating Bindings After C API Changes Overwrites Hand-Written Convenience Methods

**What goes wrong:** After renaming the free function, the Julia and Dart generators must be re-run. If the generator output file is the same file that contains hand-written wrapper code, regeneration overwrites the manual additions. In Julia, `c_api.jl` is fully generated and separate from the hand-written `database_*.jl` files. In Dart, `bindings.dart` is fully generated by ffigen and separate from `database_*.dart`. So the architecture already prevents this. But if someone mistakenly adds hand-written code to `c_api.jl` or `bindings.dart`, regeneration destroys it.

**Why it happens:** Under time pressure, a developer adds a quick fix directly to the generated file rather than the correct hand-written wrapper file. The next regeneration silently reverts the fix.

**Consequences:** A working fix disappears after the next C API change triggers regeneration. The regression is silent because the generated file compiles fine -- it just lacks the added code.

**Prevention:**
1. The architecture already handles this correctly. Reinforce: NEVER modify `c_api.jl` or `bindings.dart` by hand.
2. For Python, the split is `_c_api.py` (hand-written) vs `_declarations.py` (generated reference). Changes to CFFI declarations go in `_c_api.py` after verifying against `_declarations.py`. This dual-file pattern is a maintenance burden but is documented in CONCERNS.md.
3. After any C API rename: (1) change C headers, (2) regenerate all three binding FFI layers, (3) update hand-written wrapper code, (4) run all tests.

**Detection:** Diff the generated files before and after regeneration. Any unexpected removals indicate hand-written code was in the wrong file.

### Pitfall 8: `CSVOptions` Struct Change Breaks Enum Labels Marshaling Across All Bindings

**What goes wrong:** `CSVOptions` in C++ is a complex type with nested `unordered_map<string, unordered_map<string, unordered_map<string, int64_t>>>`. The C API flattens this into parallel arrays in `quiver_csv_options_t`. If the layer inversion fix changes the C++ `CSVOptions` type (e.g., different nesting, different key types), the C API's `convert_options` function in `database_options.h` must be updated, AND every binding that constructs `quiver_csv_options_t` structs must be updated too.

**Why it happens:** The enum labels parallel-array pattern is the most complex marshaling in the entire codebase. Julia (`database_csv_export.jl`), Dart (`database_csv_export.dart`), and Python (`database_csv_export.py`) each independently construct the parallel arrays (`enum_attribute_names`, `enum_locale_names`, `enum_entry_counts`, `enum_labels`, `enum_values`). A change to the struct layout or semantics requires synchronized updates in four places (C convert function + 3 bindings).

**Consequences:** Incorrectly marshaled enum labels cause CSV export to write wrong enum values, or CSV import to resolve labels to wrong integers. The corruption is silent -- no error is raised, but data is wrong.

**Prevention:**
1. If the layer inversion fix touches `CSVOptions`, keep the `quiver_csv_options_t` C struct layout identical to what it is today. The C++ type can change freely; only the C struct and `convert_options` need to match.
2. Add a round-trip CSV test that exercises enum labels: export with enum labels, import back, verify values match. This test exists in C++ and C API layers but should be verified in all binding layers.
3. If the C struct must change, update all four marshaling sites atomically (in a single phase/PR), not incrementally.

**Detection:** CSV round-trip tests with non-trivial enum labels (multiple attributes, multiple locales). The existing `test_c_api_database_csv_export.cpp` covers this at the C layer; ensure Python/Julia/Dart tests cover the same cases.

## Minor Pitfalls

### Pitfall 9: Python CFFI `_Bool` vs `bool` vs `int` Type Mismatch for `in_transaction`

**What goes wrong:** The C API declares `in_transaction` with `bool* out_active` (C99 `_Bool`). The Python `_c_api.py` currently declares this as `_Bool* out_active`. If during the v0.5 cleanup someone changes this to `int*` for consistency with other boolean-returning functions (which use `int*`), the size mismatch between `_Bool` (1 byte) and `int` (4 bytes) causes a buffer overrun on the CFFI side.

**Prevention:** Leave the `_Bool` declaration as-is in Python, matching the C header exactly. Or, change the C API to use `int*` (like `is_healthy` does) for consistency, then update the Python cdef.

### Pitfall 10: Test Schema Files Not Found When Running Python Tests from Wrong Directory

**What goes wrong:** Python tests use relative paths to `tests/schemas/valid/` and `tests/schemas/invalid/`. If the working directory changes (e.g., running tests from a different root), all schema-dependent tests fail with "Schema file not found" errors.

**Prevention:** The existing `conftest.py` likely handles path resolution. Verify that any new tests (for `is_healthy`, `path`, LuaRunner) use the same path resolution pattern as existing tests. Do not hardcode relative paths in new test files.

### Pitfall 11: Dart `.dart_tool` Cache Not Cleared After C API Struct Layout Changes

**What goes wrong:** Dart's ffigen caches DLL bindings in `.dart_tool/hooks_runner/` and `.dart_tool/lib/`. After changing struct layouts in the C API, Dart tests pass locally because the old cached DLL is still used. CI or a fresh checkout fails because the cache does not exist.

**Prevention:** After any C API struct change, clear Dart caches: delete `.dart_tool/hooks_runner/` and `.dart_tool/lib/` before running Dart tests. The existing `CLAUDE.md` documents this but it is easy to forget.

## Phase-Specific Warnings

These warnings map to the six specific improvements in the v0.5 milestone.

| Improvement | Likely Pitfall | Severity | Mitigation |
|-------------|---------------|----------|------------|
| Fix layer inversion (DatabaseOptions/CSVOptions) | Circular include between C++ and C headers (Pitfall 3) | Critical | Keep C and C++ option headers independent; conversion only in .cpp |
| Fix layer inversion (DatabaseOptions/CSVOptions) | Python CFFI struct layout mismatch (Pitfall 2) | Critical | Update `_c_api.py` immediately; add size assertions |
| Fix layer inversion (CSVOptions) | Enum labels marshaling breaks silently (Pitfall 8) | Moderate | Keep `quiver_csv_options_t` layout unchanged if possible |
| Rename `quiver_element_free_string` | Partial rename across bindings (Pitfall 1) | Critical | Checklist: C header -> C impl -> 3 generators -> Python cdef -> usage sites -> test-all |
| Add Python LuaRunner binding | Database lifetime hazard (Pitfall 4) | Critical | Store Database reference in LuaRunner; guard against use-after-close |
| Replace Python magic numbers | Dual constant sets (Pitfall 5) | Moderate | Use IntEnum; assert equality with CFFI lib values in tests |
| Add Python convenience helper tests | Exposing existing bugs (Pitfall 6 analog) | Low | Write tests carefully; document known limitations |
| Add `is_healthy()`/`path()` tests | Dangling pointer exposure (Pitfall 6) | Moderate | Always test path() before close(); consider fixing the API |

## Recommended Ordering to Minimize Risk

Based on pitfall analysis, the safest order for the six improvements:

1. **Fix layer inversion first** -- This is the most architecturally impactful change. Every other change must be compatible with the new type ownership. Doing it first means all subsequent changes build on the correct foundation.

2. **Rename free function second** -- This is a cross-cutting rename that touches all bindings. Do it immediately after the layer inversion while all generators need to be re-run anyway. Combine the regeneration step.

3. **Python magic number constants third** -- Self-contained change with no cross-layer impact. Can be done independently.

4. **Python LuaRunner binding fourth** -- New feature addition. Depends on the C API being stable (no more renames or struct changes pending).

5. **Test coverage additions last** -- Adding tests for `is_healthy()`/`path()` and convenience helpers should come after all API changes are complete. Tests validate the final state.

**Rationale:** Changes that modify the C API surface (items 1-2) must happen before changes that add bindings or tests on top of that surface (items 3-5). Running generators twice (once for struct changes, once for renames) is wasteful; combining them into one phase saves time and reduces the window where bindings are out of sync.

## Sources

- Direct code analysis of `include/quiver/c/options.h`, `include/quiver/options.h`, `src/c/database_options.h` (layer inversion)
- Direct code analysis of `src/c/element.cpp`, `bindings/*/database_query.*`, `bindings/*/database_read.*` (free function usage)
- Direct code analysis of `bindings/python/src/quiverdb/_c_api.py`, `_loader.py`, `database.py` (CFFI ABI-mode patterns)
- Direct code analysis of `include/quiver/c/lua_runner.h` (LuaRunner C API surface)
- `.planning/codebase/CONCERNS.md` (existing known issues including `quiver_database_path` lifetime)
- `.planning/PROJECT.md` (active requirements for v0.5)
- CFFI documentation: ABI-mode struct layout must match binary exactly (HIGH confidence -- well-documented behavior)
- Python GC behavior with C pointer lifetimes (HIGH confidence -- fundamental Python/FFI interaction)

---

*Pitfalls audit: 2026-02-27*
