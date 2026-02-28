# Phase 2: Free Function Naming - Research

**Researched:** 2026-02-27
**Domain:** C API free function naming / cross-layer FFI rename
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Add `quiver_database_free_string()` in `include/quiver/c/database.h`
- Remove `quiver_element_free_string()` from header, implementation, bindings, and tests -- zero traces remain
- Breaking change is intentional (CLAUDE.md: "no backwards compatibility required")
- Julia and Dart: re-run generators (`generator.bat`) to pick up the renamed function
- Python: manually update hand-written CFFI cdef in `_c_api.py` (generator output is reference-only)
- All bindings must call `quiver_database_free_string` for database-returned strings
- Update all test suites (C++, C API, Julia, Dart, Python) that reference the old function name
- No new test files needed -- existing coverage validates the rename

### Claude's Discretion
- Implementation file placement (co-location with read allocations)
- Generator re-run sequencing
- Any intermediate refactoring needed for clean rename

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| NAME-01 | Add `quiver_database_free_string()` for database query results; update all bindings to use it | Implementation pattern documented below; co-location in `database_read.cpp`; exact list of 9 binding usage sites identified; generator mechanics verified |
| NAME-02 | Remove `quiver_element_free_string()` entirely (breaking change); update all bindings | Complete inventory of all 24 files referencing the old function; element.h/element.cpp removal plan; `quiver_element_to_string` comment update required; test_c_api_element.cpp test updates mapped |
</phase_requirements>

## Summary

This phase is a mechanical cross-layer rename. The function `quiver_element_free_string` (declared in `element.h`, implemented in `element.cpp`) is currently used by 9 binding call sites to free strings returned by **database** operations (`query_string`, `query_string_params`, `read_scalar_string_by_id`). The fix adds `quiver_database_free_string` to the database entity scope and removes `quiver_element_free_string` entirely from the codebase.

The rename touches 4 layers: C API header/implementation, Julia bindings (generated `c_api.jl` + hand-written `database_read.jl`, `database_query.jl`, `element.jl`), Dart bindings (generated `bindings.dart` + hand-written `database_read.dart`, `database_query.dart`), and Python bindings (hand-written `_c_api.py`, `database.py`, `element.py`, generator output `_declarations.py`). The critical constraint is atomicity: every reference to the old symbol must be updated before any test suite runs, or FFI symbol resolution will fail at runtime.

**Primary recommendation:** Add `quiver_database_free_string` to `database.h` and `database_read.cpp`, remove `quiver_element_free_string` from `element.h` and `element.cpp`, update the `quiver_element_to_string` comment to reference `quiver_database_free_string`, re-run all 3 generators, update all hand-written binding call sites, update C API element tests, then run `scripts/test-all.bat`.

## Standard Stack

Not applicable -- this phase uses no external libraries. It is a pure rename operation across existing codebase infrastructure.

### Generator Tools
| Tool | Input | Output | Invocation |
|------|-------|--------|------------|
| Julia Clang.Generators | All `include/quiver/c/*.h` (except `platform.h`) | `bindings/julia/src/c_api.jl` | `bindings/julia/generator/generator.bat` |
| Dart ffigen | `include/quiver/c/{common,database,element,lua_runner}.h` | `bindings/dart/lib/src/ffi/bindings.dart` | `bindings/dart/generator/generator.bat` |
| Python generator | `include/quiver/c/{common,options,database,element}.h` | `bindings/python/src/quiverdb/_declarations.py` | `bindings/python/generator/generator.bat` |

**Key insight:** Julia and Dart generators automatically produce FFI wrappers from headers. When `quiver_database_free_string` is added to `database.h` and `quiver_element_free_string` is removed from `element.h`, re-running generators will:
- Add `quiver_database_free_string` wrapper to `c_api.jl` and `bindings.dart`
- Remove `quiver_element_free_string` wrapper from `c_api.jl` and `bindings.dart`

Python's generator only updates `_declarations.py` (a reference copy). The hand-written `_c_api.py` must be manually edited.

## Architecture Patterns

### Pattern 1: Alloc/Free Co-location

**What:** Every allocation function and its corresponding free function live in the same translation unit.

**Applied here:** `quiver_database_free_string` belongs in `src/c/database_read.cpp` alongside `quiver_database_free_string_array`, `quiver_database_free_integer_array`, and `quiver_database_free_float_array`. This follows the existing convention exactly.

**Example (existing pattern in `database_read.cpp`):**
```cpp
// Allocating function (same file)
QUIVER_C_API quiver_error_t quiver_database_read_scalar_string_by_id(..., char** out_value, ...) {
    // allocates with new char[...]
}

// Free function (same file)
QUIVER_C_API quiver_error_t quiver_database_free_string_array(char** values, size_t count) {
    // deallocates with delete[]
}
```

### Pattern 2: Entity-Scoped Free Functions

**What:** Free functions are prefixed with the entity that allocated the memory.

**Current violation:** `quiver_element_free_string` frees strings allocated by `quiver_database_query_string` and `quiver_database_read_scalar_string_by_id`.

**After fix:** `quiver_database_free_string` frees all strings allocated by database operations. `quiver_element_free_string` is removed entirely (user decision: no element-scoped free function remains).

### Pattern 3: Generator Output vs Hand-Written Code

**What:** Generators produce FFI binding stubs. Hand-written wrapper code calls those stubs.

**Implication:** After re-running generators, the hand-written files must be updated to call the new function name. The generators handle `c_api.jl` and `bindings.dart` automatically, but `database_read.jl`, `database_query.jl`, `element.jl`, `database_read.dart`, `database_query.dart`, `database.py`, `element.py`, and `_c_api.py` require manual edits.

### Anti-Patterns to Avoid
- **Partial rename:** Updating the C API but forgetting a single binding file causes runtime FFI symbol-not-found errors. All files must be updated atomically (before any test run).
- **Generator overwrite:** Running generators BEFORE adding the new function to the C header means the generator output won't include `quiver_database_free_string`. Always: header change first, then generator run.

## Complete Change Inventory

### Files to MODIFY (source code changes)

| # | File | Change | Details |
|---|------|--------|---------|
| 1 | `include/quiver/c/database.h` | ADD declaration | `quiver_database_free_string(char* str)` near line 342 (alongside other free functions) |
| 2 | `src/c/database_read.cpp` | ADD implementation | `quiver_database_free_string` after `quiver_database_free_string_array` (line ~78) |
| 3 | `include/quiver/c/element.h` | REMOVE declaration + UPDATE comment | Remove `quiver_element_free_string` declaration (line 46); update `quiver_element_to_string` comment (line 44) to reference `quiver_database_free_string` |
| 4 | `src/c/element.cpp` | REMOVE implementation | Remove `quiver_element_free_string` function (lines 157-160) |
| 5 | `tests/test_c_api_element.cpp` | UPDATE 2 test calls | Line 244: `quiver_element_free_string(str)` -> `quiver_database_free_string(str)`; Line 254: `quiver_element_free_string(nullptr)` -> `quiver_database_free_string(nullptr)` |
| 6 | `bindings/julia/src/database_read.jl` | UPDATE 1 call | Line 242: `C.quiver_element_free_string` -> `C.quiver_database_free_string` |
| 7 | `bindings/julia/src/database_query.jl` | UPDATE 2 calls | Lines 62, 129: `C.quiver_element_free_string` -> `C.quiver_database_free_string` |
| 8 | `bindings/julia/src/element.jl` | UPDATE 1 call | Line 106: `C.quiver_element_free_string` -> `C.quiver_database_free_string` |
| 9 | `bindings/dart/lib/src/database_read.dart` | UPDATE 1 call | Line 485: `bindings.quiver_element_free_string` -> `bindings.quiver_database_free_string` |
| 10 | `bindings/dart/lib/src/database_query.dart` | UPDATE 2 calls | Lines 29, 131: `bindings.quiver_element_free_string` -> `bindings.quiver_database_free_string` |
| 11 | `bindings/python/src/quiverdb/_c_api.py` | ADD declaration + REMOVE old | Add `quiver_database_free_string(char* str)` to cdef; remove `quiver_element_free_string(char* str)` from cdef (line 90) |
| 12 | `bindings/python/src/quiverdb/database.py` | UPDATE 2 calls | Lines 243, 503: `lib.quiver_element_free_string` -> `lib.quiver_database_free_string` |
| 13 | `bindings/python/src/quiverdb/element.py` | UPDATE 1 call | Line 120: `lib.quiver_element_free_string` -> `lib.quiver_database_free_string` |

### Files to REGENERATE (generator output)

| # | File | Generator | Automatic changes |
|---|------|-----------|-------------------|
| 14 | `bindings/julia/src/c_api.jl` | `bindings/julia/generator/generator.bat` | Adds `quiver_database_free_string` wrapper; removes `quiver_element_free_string` wrapper |
| 15 | `bindings/dart/lib/src/ffi/bindings.dart` | `bindings/dart/generator/generator.bat` | Adds `quiver_database_free_string` binding; removes `quiver_element_free_string` binding |
| 16 | `bindings/python/src/quiverdb/_declarations.py` | `bindings/python/generator/generator.bat` | Updates reference copy (not used at runtime) |

### Files to UPDATE (documentation)

| # | File | Change |
|---|------|--------|
| 17 | `CLAUDE.md` | Update Memory Management section: replace `quiver_element_free_string(char*)` with `quiver_database_free_string(char*)` |

**Total files modified:** 17 (13 manual edits + 3 generator outputs + 1 documentation)

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FFI binding generation | Manual Julia/Dart FFI wrappers | Run `generator.bat` for each binding | Generators read C headers directly; manual wrappers drift from the header and are error-prone |

**Key insight:** The generators are the authoritative source for `c_api.jl` and `bindings.dart`. Never hand-edit these files -- they will be overwritten on the next generator run.

## Common Pitfalls

### Pitfall 1: Running Generators Before Header Change
**What goes wrong:** Generator output won't include `quiver_database_free_string` if the header hasn't been updated yet.
**Why it happens:** Natural instinct to "regenerate first" before making changes.
**How to avoid:** Strict ordering: (1) modify C headers, (2) build C++ library, (3) run generators, (4) update hand-written code.
**Warning signs:** Generator output doesn't contain `quiver_database_free_string` -- check `c_api.jl` and `bindings.dart` after generation.

### Pitfall 2: Missing a Single Binding Call Site
**What goes wrong:** Runtime FFI error: "symbol not found: quiver_element_free_string" in whichever binding still references it.
**Why it happens:** 9 call sites across 7 hand-written files. Easy to miss one.
**How to avoid:** After all changes, grep entire codebase for `quiver_element_free_string` -- must return ZERO results from non-planning files.
**Warning signs:** Any test suite failing with symbol resolution errors.

### Pitfall 3: Forgetting element.py Uses quiver_element_free_string for Element.to_string
**What goes wrong:** Python `Element.__repr__()` calls `lib.quiver_element_free_string` (line 120 in `element.py`). If this isn't updated, Python element tests fail.
**Why it happens:** The initial architecture research recommended keeping `quiver_element_free_string` for element-scoped strings. The user decision overrides this: complete removal.
**How to avoid:** Update `element.py` to use `lib.quiver_database_free_string`. Same for `element.jl` line 106.
**Warning signs:** Python element repr test failures.

### Pitfall 4: element.h Comment Still References quiver_element_free_string
**What goes wrong:** After removing the function, the comment on `quiver_element_to_string` (line 44) still says "caller must free returned string with quiver_element_free_string". This is misleading documentation.
**Why it happens:** Comments are easy to overlook during mechanical renames.
**How to avoid:** Update the comment to reference `quiver_database_free_string`.

### Pitfall 5: Python _c_api.py Requires Both Add AND Remove
**What goes wrong:** If only `quiver_database_free_string` is added but `quiver_element_free_string` is not removed from the CFFI cdef, Python code that still references the old name won't fail at import time (CFFI ABI-mode resolves symbols lazily). But the DLL no longer exports the old symbol, so it fails at call time.
**Why it happens:** CFFI ABI-mode doesn't validate symbol existence at load time.
**How to avoid:** Remove `quiver_element_free_string` from `_c_api.py` cdef AND update all call sites in `database.py` and `element.py`.

### Pitfall 6: Julia Generator Requires Built DLL
**What goes wrong:** Julia generator (`Clang.Generators`) calls `Libdl.dlopen` on `libquiver_c.dll` during generation. If the DLL isn't built with the new function exported, the generator may fail or produce stale output.
**Why it happens:** The Julia generator parses headers but also loads the DLL.
**How to avoid:** Build the C++ library (`cmake --build build --config Debug`) BEFORE running the Julia generator.

## Code Examples

### New C API Declaration (`database.h`)
```c
// Memory cleanup for single string returned by query/read-by-id operations
QUIVER_C_API quiver_error_t quiver_database_free_string(char* str);
```

Place alongside existing free function declarations (after `quiver_database_free_string_array`).

### New C API Implementation (`database_read.cpp`)
```cpp
QUIVER_C_API quiver_error_t quiver_database_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}
```

Note: No `QUIVER_REQUIRE(str)` -- matches the existing `quiver_element_free_string` which accepts nullptr safely (`delete[] nullptr` is a no-op in C++). This is intentional: callers may have null strings from optional read results.

### Updated element.h Comment
```c
// Pretty print (caller must free returned string with quiver_database_free_string)
QUIVER_C_API quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
```

### Updated C API Test (`test_c_api_element.cpp`)
```cpp
// Was: quiver_element_free_string(str);
quiver_database_free_string(str);

// Was: EXPECT_EQ(quiver_element_free_string(nullptr), QUIVER_OK);
EXPECT_EQ(quiver_database_free_string(nullptr), QUIVER_OK);
```

Note: `test_c_api_element.cpp` must `#include "quiver/c/database.h"` in addition to `"quiver/c/element.h"` to access the new free function. Verify this include exists.

### Updated Python `_c_api.py` (cdef section)
```python
# Remove this line:
#   quiver_error_t quiver_element_free_string(char* str);
# Add (in database free functions section):
    quiver_error_t quiver_database_free_string(char* str);
```

### Binding Call Site Pattern (all bindings follow same transformation)
```
# Julia:  C.quiver_element_free_string(...)  ->  C.quiver_database_free_string(...)
# Dart:   bindings.quiver_element_free_string(...)  ->  bindings.quiver_database_free_string(...)
# Python: lib.quiver_element_free_string(...)  ->  lib.quiver_database_free_string(...)
```

## Execution Order

The correct build/update sequence:

1. **C API header** -- Add `quiver_database_free_string` to `database.h`; remove from `element.h` (keep `quiver_element_to_string`, update its comment)
2. **C API implementation** -- Add `quiver_database_free_string` to `database_read.cpp`; remove from `element.cpp`
3. **Build C++ library** -- `cmake --build build --config Debug` (exports new symbol, stops exporting old)
4. **Update C API tests** -- `test_c_api_element.cpp`: change 2 call sites + add include
5. **Run C++ and C API tests** -- Validates C-layer rename is correct before touching bindings
6. **Run generators** -- Julia, Dart, Python (in any order). Julia requires the built DLL.
7. **Update hand-written binding files** -- 7 files, 9 call sites total
8. **Run all test suites** -- `scripts/test-all.bat`
9. **Update CLAUDE.md** -- Replace `quiver_element_free_string` reference in Memory Management section

## Verification

### Completeness Check
After all changes, run:
```bash
grep -r "quiver_element_free_string" --include="*.h" --include="*.cpp" --include="*.jl" --include="*.dart" --include="*.py" .
```
Expected result: **zero matches** outside `.planning/` directory.

### Test Gate
```bash
scripts/test-all.bat
```
All five test suites (C++, C API, Julia, Dart, Python) must pass.

## Open Questions

1. **Include in test_c_api_element.cpp**
   - What we know: The file currently includes `quiver/c/element.h`. After removing `quiver_element_free_string` from `element.h`, the test needs `quiver_database_free_string` from `database.h`.
   - What's unclear: Whether `test_c_api_element.cpp` already includes `database.h` (it may, for database operations in other tests in the same file).
   - Recommendation: Check at implementation time; add `#include "quiver/c/database.h"` if missing.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection of all 24 files containing `quiver_element_free_string`
- Direct codebase inspection of `include/quiver/c/database.h` confirming `quiver_database_free_string` does not yet exist
- Direct inspection of all 3 generator scripts (`generator.bat`/`.jl`/`.py`) confirming header-driven generation
- `CLAUDE.md` project conventions: alloc/free co-location, entity-scoped naming, breaking changes acceptable

### Secondary (MEDIUM confidence)
- `.planning/research/ARCHITECTURE.md` -- prior research on the same rename (note: recommended keeping `quiver_element_free_string` for element scope, but user decision overrides to full removal)

## Metadata

**Confidence breakdown:**
- Architecture: HIGH -- Direct codebase inspection; all 17 files enumerated with exact line numbers
- Pitfalls: HIGH -- Every pitfall is derived from codebase structure, not speculation
- Execution order: HIGH -- Generator dependencies verified by reading generator source code

**Research date:** 2026-02-27
**Valid until:** No expiration -- this is a pure codebase-internal rename with no external dependencies
