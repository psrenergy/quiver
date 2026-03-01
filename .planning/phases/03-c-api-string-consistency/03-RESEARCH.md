# Phase 3: C API String Consistency - Research

**Researched:** 2026-03-01
**Domain:** C API internal refactoring -- string allocation consistency
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Minimal swap only -- replace inline `new char[]` + copy patterns with `strdup_safe()` calls
- No 2D helper extraction -- the vector/set nested loops stay structurally the same, only the inner allocation changes
- Both `memcpy` and `std::copy` variants are in scope (same intent, different function call)
- `element.cpp` is included -- `element_to_string` (line 147) gets converted to `strdup_safe`
- Covers ALL inline patterns in `src/c/`, nothing left behind
- `strdup_safe` and `copy_strings_to_c` move from `database_helpers.h` to existing `src/utils/string.h`
- `database_helpers.h` gets `#include "utils/string.h"` -- no further cleanup of that file
- Functions added to `src/utils/string.h` alongside existing `quiver::string::trim()`
- CLAUDE.md Architecture tree updated to show `src/utils/`
- CLAUDE.md String Handling section updated to reference `src/utils/string.h` instead of `database_helpers.h`
- ROADMAP.md success criteria updated to cover both `memcpy` and `std::copy` patterns

### Claude's Discretion
- `element_to_string` bad_alloc catch: Claude decides whether to remove the dead `std::bad_alloc` catch or keep it
- Build system changes: Claude determines if CMakeLists.txt needs any updates for the utility header

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| QUAL-04 | C API string copies use `strdup_safe` instead of inline allocation | All 5 inline allocation sites identified, replacement pattern verified against existing `strdup_safe` usage, include path solution documented |
</phase_requirements>

## Summary

This phase is a pure internal refactoring of 5 inline string allocation sites across 3 files in `src/c/`. The target pattern (`strdup_safe`) already exists and is proven in production use by `database_query.cpp`, `database_time_series.cpp`, and the metadata converters in `database_helpers.h`. The refactoring also relocates `strdup_safe` and `copy_strings_to_c` from `database_helpers.h` to `src/utils/string.h` for broader reuse.

The key technical consideration is the include path: `quiver_c` does not have `src/` in its include directories, so `src/c/` files must use a relative path (`../utils/string.h`) or the CMakeLists.txt must be updated. The `element.cpp` file additionally needs to include the new location since it currently has no access to `strdup_safe`.

**Primary recommendation:** Add `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}` to `quiver_c`'s `target_include_directories` so `src/c/` files can use `#include "utils/string.h"` (matching the convention already used by the core `quiver` target), then perform the 5 mechanical replacements and utility extraction.

## Standard Stack

Not applicable -- this phase uses no external libraries. It is a pure internal refactoring of existing C++ code within the project's own utility infrastructure.

## Architecture Patterns

### Pattern 1: strdup_safe Single String Copy
**What:** Replace inline `new char[size + 1]` + `memcpy`/`std::copy` + null-terminator with a single `strdup_safe()` call.
**When to use:** Every site that copies a `std::string` to a `char*` for C API output.
**Existing (proven) usage:**
```cpp
// database_query.cpp:51 -- already uses strdup_safe
*out_value = strdup_safe(*result);

// database_helpers.h:103 -- metadata converters already use strdup_safe
dst.name = strdup_safe(src.name);
```

**Before (inline pattern):**
```cpp
// database_read.cpp:318-320
*out_value = new char[result->size() + 1];
std::copy(result->begin(), result->end(), *out_value);
(*out_value)[result->size()] = '\0';
```

**After (strdup_safe):**
```cpp
*out_value = strdup_safe(*result);
```

### Pattern 2: strdup_safe in Nested Loops (2D arrays)
**What:** Replace the inner string copy in vector/set 2D nested loops with `strdup_safe()`. The outer loop structure stays the same.
**When to use:** `read_vector_strings` and `read_set_strings` functions.

**Before (database_read.cpp:143-146):**
```cpp
for (size_t j = 0; j < vectors[i].size(); ++j) {
    (*out_vectors)[i][j] = new char[vectors[i][j].size() + 1];
    std::copy(vectors[i][j].begin(), vectors[i][j].end(), (*out_vectors)[i][j]);
    (*out_vectors)[i][j][vectors[i][j].size()] = '\0';
}
```

**After:**
```cpp
for (size_t j = 0; j < vectors[i].size(); ++j) {
    (*out_vectors)[i][j] = strdup_safe(vectors[i][j]);
}
```

### Pattern 3: copy_strings_to_c Internal Conversion
**What:** The `copy_strings_to_c` helper itself contains an inline pattern. Its internal loop should use `strdup_safe` instead of manual allocation.
**When to use:** One site only -- `database_helpers.h` line 72-74.

**Before:**
```cpp
(*out_values)[i] = new char[values[i].size() + 1];
std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
(*out_values)[i][values[i].size()] = '\0';
```

**After:**
```cpp
(*out_values)[i] = strdup_safe(values[i]);
```

### Pattern 4: element_to_string memcpy Variant
**What:** `element.cpp` uses `memcpy` instead of `std::copy` but the intent is identical. Convert to `strdup_safe`.

**Before (element.cpp:147-148):**
```cpp
auto result = new char[str.size() + 1];
std::memcpy(result, str.c_str(), str.size() + 1);
*out_string = result;
```

**After:**
```cpp
*out_string = strdup_safe(str);
```

### Pattern 5: Utility Extraction to src/utils/string.h
**What:** Move `strdup_safe` and `copy_strings_to_c` from `database_helpers.h` to `src/utils/string.h`, placing them in `namespace quiver::string` alongside the existing `trim()`.
**Convention:** The project uses `namespace quiver::string` (see `trim()`), traditional `#ifndef` include guards, and `inline` functions in headers.

**Target structure of src/utils/string.h:**
```cpp
#ifndef QUIVER_STRING_H
#define QUIVER_STRING_H

#include <cstring>
#include <string>
#include <vector>

#include "quiver/c/database.h"  // for quiver_error_t, QUIVER_OK

namespace quiver::string {

inline std::string trim(const std::string& str) { /* existing */ }

inline char* strdup_safe(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}

inline quiver_error_t copy_strings_to_c(const std::vector<std::string>& values,
                                         char*** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new char*[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        (*out_values)[i] = strdup_safe(values[i]);
    }
    return QUIVER_OK;
}

}  // namespace quiver::string

#endif  // QUIVER_STRING_H
```

**CRITICAL CONCERN -- Namespace and C API dependency:**

Placing `copy_strings_to_c` in `src/utils/string.h` introduces a dependency on `quiver/c/database.h` (for `quiver_error_t` and `QUIVER_OK`) into a general utility header. This is architecturally questionable -- `src/utils/string.h` is included by the core `quiver` library (`database.cpp`, `database_csv_import.cpp`) which should NOT depend on C API headers.

**Resolution options (Claude's discretion):**
1. **Only move `strdup_safe`** to `src/utils/string.h`. Keep `copy_strings_to_c` in `database_helpers.h` but update it to call `quiver::string::strdup_safe`. This avoids the C API dependency leak.
2. **Move both** but make `copy_strings_to_c` return `void` instead of `quiver_error_t`, removing the C API dependency. Callers already ignore the return value (it always returns `QUIVER_OK`).
3. **Move both** and add the C API include conditionally, but this is fragile.

**Recommendation:** Option 1 is cleanest. `strdup_safe` is a pure string utility with no C API dependency. `copy_strings_to_c` is a C API marshaling helper that belongs with the marshaling templates.

### Anti-Patterns to Avoid
- **Breaking free functions:** `strdup_safe` uses `new char[]`, and existing free functions use `delete[]`. This contract must be preserved exactly. Do NOT change to `malloc`/`free` or `std::unique_ptr`.
- **Changing function signatures:** `strdup_safe` returns `char*` allocated with `new[]`. All callers (`delete[]` in free functions, `delete[]` in metadata `free_scalar_fields`) depend on this. The replacement is purely internal.
- **Forgetting null-termination:** `strdup_safe` already handles null-termination. The replacement removes 3 lines (alloc + copy + null-terminate) and replaces with 1 call. Verify the existing `strdup_safe` implementation includes null-termination (it does -- line 98 of `database_helpers.h`).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String duplication to C | Inline `new char[] + memcpy/std::copy + '\0'` | `strdup_safe()` | Single point of truth, consistent null-termination, fewer lines to audit |

**Key insight:** The entire phase exists because the inline pattern was hand-rolled in 5 places instead of using the helper that already existed.

## Common Pitfalls

### Pitfall 1: Include Path for quiver_c Target
**What goes wrong:** `src/c/` files cannot `#include "utils/string.h"` because `quiver_c` does not have `src/` in its include directories.
**Why it happens:** Only the core `quiver` target has `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}` (which resolves to `src/`). The `quiver_c` target only has `${CMAKE_SOURCE_DIR}/include`.
**How to avoid:** Either add `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}` to `quiver_c`'s `target_include_directories`, or use relative includes like `#include "../utils/string.h"` from `src/c/` files.
**Warning signs:** Compile error: `fatal error: 'utils/string.h': No such file or directory`.

**Recommendation:** Add `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}` to `quiver_c`. This matches what `quiver` already does and allows `#include "utils/string.h"` consistently. The `database_helpers.h` include would be `#include "utils/string.h"` (since `database_helpers.h` is in `src/c/` and the include base would be `src/`).

Wait -- `database_helpers.h` is in `src/c/`. If `src/` is the include root, then from `database_helpers.h` you'd write `#include "utils/string.h"`. But the CONTEXT decision says the include is to `src/utils/string.h`. This is consistent: the include path would be `utils/string.h` relative to the `src/` include root.

However, `element.cpp` is also in `src/c/`. It currently includes `"quiver/c/element.h"` (from the public include root) and `"internal.h"` (from the current directory). With `src/` added to the include path, it would use `#include "utils/string.h"`.

### Pitfall 2: Namespace Qualification After Move
**What goes wrong:** After moving `strdup_safe` to `namespace quiver::string`, all existing callers in `database_helpers.h` (7 call sites in `convert_scalar_to_c` and `convert_group_to_c`) and in `database_query.cpp` (2 call sites) and `database_time_series.cpp` (4 call sites) break because they call bare `strdup_safe()`.
**Why it happens:** The function was previously at file scope (no namespace). Moving it into `namespace quiver::string` requires all callers to use `quiver::string::strdup_safe()` or add a `using` declaration.
**How to avoid:** Two options:
1. Add `using quiver::string::strdup_safe;` at the top of `database_helpers.h` (after the include), which propagates to all includers.
2. Use qualified calls `quiver::string::strdup_safe()` everywhere.
**Recommendation:** Option 1 (using-declaration in `database_helpers.h`) keeps the diff minimal since most callers already include `database_helpers.h`. For `element.cpp` (which does NOT include `database_helpers.h`), either add the using-declaration locally or use the qualified name.

### Pitfall 3: element.cpp bad_alloc Catch After Conversion
**What goes wrong:** After converting `element_to_string` to use `strdup_safe`, the `std::bad_alloc` catch block on line 151 is dead code (the `try` block only calls `element.to_string()` which returns `std::string`, and `strdup_safe` which allocates with `new`).
**Why it happens:** `strdup_safe` uses `new char[]` which throws `std::bad_alloc` on failure, but `std::exception` already catches that. The separate `bad_alloc` handler existed when `new` was inline and the author wanted specific messaging.
**How to avoid:** Claude's discretion per CONTEXT. The `bad_alloc` catch IS reachable (`new` in `strdup_safe` can throw), but it's already caught by the broader `std::exception` that other C API functions use. Removing it makes `element_to_string` consistent with all other C API string-returning functions (e.g., `quiver_database_query_string`, `read_scalar_string_by_id`). Keeping it is harmless but inconsistent.
**Recommendation:** Remove it for consistency. Every other C API function that calls `strdup_safe` has only a `std::exception` catch, not a separate `bad_alloc` handler.

### Pitfall 4: copy_strings_to_c Return Type in Utility Header
**What goes wrong:** Moving `copy_strings_to_c` to `src/utils/string.h` pulls `quiver/c/database.h` (for `quiver_error_t`) into the utility layer, creating a circular dependency concern: core library -> utils -> C API headers.
**Why it happens:** `copy_strings_to_c` returns `quiver_error_t` which is defined in the C API header.
**How to avoid:** Keep `copy_strings_to_c` in `database_helpers.h`. Only move `strdup_safe` (which has no C API dependencies) to `src/utils/string.h`.
**Warning signs:** Core library files that include `utils/string.h` would transitively include C API headers.

## Code Examples

### Complete strdup_safe in src/utils/string.h
```cpp
// Pure string utility -- no C API dependencies
inline char* strdup_safe(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}
```

### Updated copy_strings_to_c in database_helpers.h (using strdup_safe)
```cpp
inline quiver_error_t copy_strings_to_c(const std::vector<std::string>& values,
                                         char*** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new char*[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        (*out_values)[i] = strdup_safe(values[i]);
    }
    return QUIVER_OK;
}
```

### Updated read_scalar_string_by_id (database_read.cpp:317-320)
```cpp
if (result.has_value()) {
    *out_value = strdup_safe(*result);
    *out_has_value = 1;
}
```

### Updated read_vector_strings inner loop (database_read.cpp:143-146)
```cpp
for (size_t j = 0; j < vectors[i].size(); ++j) {
    (*out_vectors)[i][j] = strdup_safe(vectors[i][j]);
}
```

### Updated read_set_strings inner loop (database_read.cpp:245-248)
```cpp
for (size_t j = 0; j < sets[i].size(); ++j) {
    (*out_sets)[i][j] = strdup_safe(sets[i][j]);
}
```

### Updated element_to_string (element.cpp:146-149)
```cpp
try {
    auto str = element->element.to_string();
    *out_string = strdup_safe(str);
    return QUIVER_OK;
} catch (const std::exception& e) {
    quiver_set_last_error(e.what());
    return QUIVER_ERROR;
}
```

## Target Sites -- Complete Inventory

| # | File | Line(s) | Pattern | Replacement |
|---|------|---------|---------|-------------|
| 1 | `database_helpers.h` | 72-74 | `new char[] + std::copy + '\0'` in `copy_strings_to_c` | `strdup_safe(values[i])` |
| 2 | `database_read.cpp` | 144-146 | `new char[] + std::copy + '\0'` in string vector read | `strdup_safe(vectors[i][j])` |
| 3 | `database_read.cpp` | 246-248 | `new char[] + std::copy + '\0'` in string set read | `strdup_safe(sets[i][j])` |
| 4 | `database_read.cpp` | 318-320 | `new char[] + std::copy + '\0'` in `read_scalar_string_by_id` | `strdup_safe(*result)` |
| 5 | `element.cpp` | 147-148 | `new char[] + memcpy` in `element_to_string` | `strdup_safe(str)` |

Sites already using `strdup_safe` (no changes needed):
- `database_helpers.h:103,107,110,111` -- `convert_scalar_to_c`
- `database_helpers.h:122,123` -- `convert_group_to_c`
- `database_query.cpp:51,125` -- query string functions
- `database_time_series.cpp:120,156,410,412` -- time series operations

## Include Path Changes

| File | Current Includes | Add |
|------|-----------------|-----|
| `src/utils/string.h` | `<string>` | `<cstring>`, `<algorithm>` (for `std::copy`) |
| `src/c/database_helpers.h` | `"internal.h"`, `"quiver/c/database.h"`, `"quiver/database.h"` | `"utils/string.h"` |
| `src/c/element.cpp` | `"quiver/c/element.h"`, `"internal.h"` | `"utils/string.h"` |
| `src/CMakeLists.txt` | `quiver_c` has only `${CMAKE_SOURCE_DIR}/include` | `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}` |

Note: `<cstring>` may already be transitively included, but `src/utils/string.h` should be self-contained. Currently it only includes `<string>`. After adding `strdup_safe`, it needs `<cstring>` (not strictly required if only using `std::copy` from `<algorithm>`) and the new char[] allocation needs no additional includes.

Actually, `strdup_safe` currently uses `std::copy` (from `<algorithm>`) not `memcpy`. The header needs `<algorithm>` for `std::copy`. But `database_helpers.h` currently does NOT include `<algorithm>` either -- it works because `std::copy` is transitively available. For `src/utils/string.h` to be self-contained, it should include `<algorithm>`.

## CMakeLists.txt Change

The `quiver_c` target needs `src/` added to its private include path:

```cmake
target_include_directories(quiver_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
```

This matches what the core `quiver` target already does (line 35-41 of `src/CMakeLists.txt`).

## Verification Strategy

### Automated Tests
All existing C API tests validate behavior is preserved:

| Test File | Relevance |
|-----------|-----------|
| `test_c_api_database_read.cpp` | Covers string scalar reads, vector string reads, set string reads, string-by-id reads |
| `test_c_api_element.cpp` | Covers `element_to_string` |
| `test_c_api_database_query.cpp` | Already uses `strdup_safe` -- regression check |
| `test_c_api_database_metadata.cpp` | Already uses `strdup_safe` -- regression check |
| `test_c_api_database_time_series.cpp` | Already uses `strdup_safe` -- regression check |

**Commands:**
- Quick check: `./build/bin/quiver_c_tests.exe`
- Full suite: `scripts/test-all.bat`

### Manual Verification
After all changes, grep to confirm no inline patterns remain:
```bash
grep -n "new char\[" src/c/*.cpp src/c/*.h
```
Expected: only `strdup_safe` definition in `src/utils/string.h`, plus numeric array allocations in `database_helpers.h` (which are `new T[]` for `int64_t`/`double`, not string copies).

## Open Questions

1. **Namespace qualification strategy**
   - What we know: Moving `strdup_safe` into `namespace quiver::string` breaks 13+ existing bare `strdup_safe()` calls
   - What's unclear: Whether to use `using quiver::string::strdup_safe;` in `database_helpers.h` or qualify all call sites
   - Recommendation: Use `using quiver::string::strdup_safe;` in `database_helpers.h` for minimal diff. Add the same or use qualified name in `element.cpp`.

2. **copy_strings_to_c placement**
   - What we know: CONTEXT says both move to `src/utils/string.h`, but `copy_strings_to_c` has a `quiver_error_t` return type creating a C API dependency in the utility layer
   - What's unclear: Whether the user intended this dependency or overlooked it
   - Recommendation: Move only `strdup_safe` to `src/utils/string.h`. Keep `copy_strings_to_c` in `database_helpers.h` but update its body to use `strdup_safe`. If the planner considers this a deviation from CONTEXT, note it and let the user decide.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection of all 15 `.cpp` and `.h` files in `src/c/`
- `src/CMakeLists.txt` -- verified include paths for both `quiver` and `quiver_c` targets
- `src/utils/string.h` -- verified existing structure, namespace, include guard convention
- `src/c/database_helpers.h` -- verified current `strdup_safe` implementation and all callers
- `src/c/database_read.cpp` -- verified all 3 inline allocation sites
- `src/c/element.cpp` -- verified the `memcpy` variant

## Metadata

**Confidence breakdown:**
- Target sites: HIGH -- complete codebase grep confirms exactly 5 inline sites, all verified with line numbers
- Architecture (utility extraction): HIGH -- existing `src/utils/` convention is clear
- Include path concern: HIGH -- verified by reading CMakeLists.txt, known solution
- Namespace concern: HIGH -- mechanical consequence of the move, two clear resolution paths

**Research date:** 2026-03-01
**Valid until:** Indefinite (internal codebase refactoring, no external dependencies)
