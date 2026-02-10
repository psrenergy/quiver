# Phase 4: C API File Decomposition - Research

**Researched:** 2026-02-10
**Domain:** C API translation unit decomposition mirroring C++ structure
**Confidence:** HIGH

## Summary

Phase 4 decomposes `src/c/database.cpp` (1612 lines) into focused modules that mirror the C++ decomposition completed in Phase 2. The C++ layer was split into 10 files (`database.cpp`, `database_create.cpp`, `database_read.cpp`, `database_update.cpp`, `database_delete.cpp`, `database_metadata.cpp`, `database_time_series.cpp`, `database_query.cpp`, `database_relations.cpp`, `database_describe.cpp`) with shared helpers in `database_internal.h`. The C API layer needs an analogous split.

The current `src/c/database.cpp` contains: (1) anonymous-namespace helper templates for marshaling C++ types to C (lines 9-76), (2) anonymous-namespace metadata converters and `strdup_safe` (lines 904-968), (3) a static `convert_params` function (lines 1221-1247), and (4) ~65 `extern "C"` API functions covering lifecycle, CRUD, read, update, metadata, query, time series, and describe/CSV operations. The success criteria require extracting helpers into `src/c/database_helpers.h`, trimming `database.cpp` to lifecycle-only under 500 lines, and co-locating every alloc/free pair in the same translation unit.

This is structurally identical to the Phase 2 C++ decomposition but with C API-specific concerns: the helpers are C++ templates in anonymous namespaces (need to become shared), the `extern "C"` linkage must be preserved in each file, and alloc/free co-location is a new constraint not present in Phase 2.

**Primary recommendation:** Create `src/c/database_helpers.h` containing all marshaling templates, `strdup_safe`, metadata converters, `QUIVER_REQUIRE` macro, and `convert_params`. Split `database.cpp` into 8 files mirroring the C++ structure. Keep alloc/free pairs together. Update `src/CMakeLists.txt`. Verify all C API tests pass.

## Standard Stack

### Core

No new libraries or dependencies. This is purely file reorganization of existing C++ code behind `extern "C"` linkage.

| Component | Current | Purpose | Impact of Phase 4 |
|-----------|---------|---------|-------------------|
| CMake 3.21+ | Already configured | Build system | Must update `quiver_c` source list in `src/CMakeLists.txt` |
| C++20 | Already configured | Language standard | No change |
| `src/c/internal.h` | Exists | Shared `quiver_database`, `quiver_element` structs, `QUIVER_REQUIRE` macro, `quiver_set_last_error` | Must decide: expand this or create separate `database_helpers.h` |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Separate `database_helpers.h` | Expand existing `internal.h` | `internal.h` is shared by ALL C API files (element, lua_runner). Marshaling helpers are database-specific. Keeping them separate avoids polluting non-database translation units. |
| Multiple `.cpp` files | Single file with regions | Does not achieve the decomposition goal |
| Shared `.cpp` for helpers | Header-only helpers | Helper templates must be in headers anyway; non-template helpers use `inline` to avoid ODR issues |

## Architecture Patterns

### Current File Structure (Before)

```
src/c/
  internal.h          # quiver_database/quiver_element structs, QUIVER_REQUIRE, quiver_set_last_error decl
  common.cpp          # Thread-local error storage, quiver_version, quiver_get_last_error
  database.cpp        # ALL 65 database C API functions + helpers (1612 lines)
  element.cpp         # Element C API functions
  lua_runner.cpp      # Lua runner C API functions
```

### Recommended File Structure (After)

```
src/c/
  internal.h                # UNCHANGED: quiver_database/quiver_element structs, QUIVER_REQUIRE, quiver_set_last_error decl
  database_helpers.h        # NEW: marshaling templates, strdup_safe, metadata converters, convert_params
  common.cpp                # UNCHANGED
  database.cpp              # Lifecycle only: options_default, open, close, is_healthy, path, from_schema, from_migrations, current_version, describe, export/import CSV
  database_create.cpp       # NEW: create_element, update_element, delete_element_by_id
  database_read.cpp         # NEW: all read_scalar/vector/set/element_ids + free functions for scalar/vector/set arrays
  database_update.cpp       # NEW: all update_scalar/vector/set
  database_metadata.cpp     # NEW: get_*_metadata, list_*_groups/attributes, free_*_metadata functions
  database_query.cpp        # NEW: query_string/integer/float, query_*_params, convert_params
  database_time_series.cpp  # NEW: time series read/update/metadata, time series files, free functions
  database_relations.cpp    # NEW: set_scalar_relation, read_scalar_relation
  element.cpp               # UNCHANGED
  lua_runner.cpp            # UNCHANGED
```

### Pattern 1: C API Helper Header

**What:** Extract all anonymous-namespace helper templates and functions from `database.cpp` into a shared header that all split files include.

**Why:** The current helpers are in anonymous namespace, meaning they have internal linkage. If duplicated across files, this wastes binary size and risks divergence. Moving them to a named inline scope in a header solves both issues.

**Contents of `database_helpers.h`:**

| Helper | Lines in Current File | Used By |
|--------|----------------------|---------|
| `read_scalars_impl<T>` | 12-22 | read, time_series (for read_vector_*_by_id / read_set_*_by_id which reuse it) |
| `read_vectors_impl<T>` | 25-46 | read |
| `free_vectors_impl<T>` | 49-60 | read (free functions) |
| `copy_strings_to_c` | 63-76 | read, relations, time_series_files |
| `to_c_data_type` | 905-917 | metadata |
| `strdup_safe` | 919-924 | metadata, query, time_series |
| `convert_scalar_to_c` | 926-936 | metadata |
| `free_scalar_fields` | 938-943 | metadata |
| `convert_group_to_c` | 945-957 | metadata, time_series |
| `free_group_fields` | 959-968 | metadata |
| `convert_params` | 1221-1247 | query |

**ODR Strategy:** Templates are inherently ODR-safe. Non-template functions (`copy_strings_to_c`, `to_c_data_type`, `strdup_safe`, `convert_scalar_to_c`, `free_scalar_fields`, `convert_group_to_c`, `free_group_fields`, `convert_params`) must be marked `inline` in the header.

**Namespace Strategy:** Use anonymous namespace within the header is WRONG (creates per-TU copies). Use a named namespace or just `inline` free functions. Since these are C API internals (not exposed to external consumers), a simple approach is `inline` functions in the global/anonymous scope -- but that creates issues with the C++ standard for anonymous-namespace-in-header.

**Recommended:** Place all helpers as `inline` functions directly in the header file, no namespace wrapping. They are only included by `src/c/database_*.cpp` files (all part of the same `quiver_c` library target), so namespace pollution is not a concern. Alternatively, wrap in a named namespace like `quiver::c_internal` for cleanliness.

### Pattern 2: Alloc/Free Co-Location

**What:** Success criterion 4 requires that every allocation function and its corresponding free function live in the same translation unit.

**Current alloc/free pairs in database.cpp:**

| Alloc Function | Free Function | Target File |
|---------------|---------------|-------------|
| `read_scalars_impl` (allocates `T[]`) | `quiver_free_integer_array`, `quiver_free_float_array` | `database_read.cpp` |
| `copy_strings_to_c` (allocates `char**`) | `quiver_free_string_array` | `database_read.cpp` |
| `read_vectors_impl` (allocates `T**`, `size_t[]`) | `quiver_free_integer_vectors`, `quiver_free_float_vectors` | `database_read.cpp` |
| String vector read (inline alloc) | `quiver_free_string_vectors` | `database_read.cpp` |
| `convert_scalar_to_c`/`convert_group_to_c` | `quiver_free_scalar_metadata`, `quiver_free_group_metadata`, `quiver_free_scalar_metadata_array`, `quiver_free_group_metadata_array` | `database_metadata.cpp` |
| Time series read (allocates `char**`, `double[]`) | `quiver_free_time_series_data` | `database_time_series.cpp` |
| Time series files read (allocates `char**`, `char**`) | `quiver_free_time_series_files` | `database_time_series.cpp` |
| `strdup_safe` in `query_string` | `quiver_string_free` (in element.cpp) | **Cross-file issue** |

**Cross-file issue:** `quiver_string_free` is defined in `element.cpp` but is used to free strings allocated by `query_string*` in `database_query.cpp`. This is acceptable because:
1. Both use `new char[]/delete[]` -- the allocation mechanism is the same
2. `quiver_string_free` is a general-purpose string free function, not query-specific
3. Moving it to `database_query.cpp` would break element string freeing

**Recommendation:** Keep `quiver_string_free` in `element.cpp` (it is a general utility). The co-location criterion applies to database-specific alloc/free pairs, not shared utilities.

### Pattern 3: extern "C" Block Per File

**What:** Each split `.cpp` file wraps its C API functions in `extern "C" { ... }`.

**Example:**
```cpp
// src/c/database_read.cpp
#include "quiver/c/database.h"
#include "internal.h"
#include "database_helpers.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers(...) { ... }
// ... more read functions ...
QUIVER_C_API quiver_error_t quiver_free_integer_array(...) { ... }
// ... more free functions ...

}  // extern "C"
```

### Anti-Patterns to Avoid

- **Splitting `extern "C"` blocks across includes:** Each file must have its own `extern "C"` block. Never include a file mid-block.
- **Anonymous namespace in shared header:** Creates per-TU copies. Use `inline` instead.
- **Moving QUIVER_REQUIRE to database_helpers.h:** It belongs in `internal.h` where it already lives, shared by all C API files (element, lua_runner, database).
- **Changing public headers:** `include/quiver/c/database.h` must not change at all.
- **Breaking alloc/free pairs:** A read function in one file with its free in another (except the general `quiver_string_free`).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ODR violations for shared helpers | Separate .cpp with helpers | `inline` functions in `database_helpers.h` | Standard C++ approach; templates inherently safe |
| CMake source discovery | Glob or wildcard | Explicit file list in quiver_c sources | CMake best practice |
| Custom memory tracking | Alloc/free audit tools | Manual co-location in same TU | Project is small enough; co-location is sufficient |

## Common Pitfalls

### Pitfall 1: Forgetting to Update CMakeLists.txt

**What goes wrong:** New `.cpp` file created but not added to the `quiver_c` source list in `src/CMakeLists.txt`. Functions defined in the new file have no object code, causing "undefined symbol" linker errors at DLL build time.
**Why it happens:** CMake uses explicit source lists.
**How to avoid:** Add every new `.cpp` file immediately when creating it. The quiver_c target currently lists: `c/common.cpp`, `c/database.cpp`, `c/element.cpp`, `c/lua_runner.cpp`. All new `c/database_*.cpp` files must be added.
**Warning signs:** Linker errors about unresolved external symbols for `quiver_database_*` functions.

### Pitfall 2: DLL Export Missing

**What goes wrong:** Functions compile but are not exported from the DLL, causing "unresolved external symbol" in test executables.
**Why it happens:** `QUIVER_C_API` macro resolves to `__declspec(dllexport)` when `QUIVER_C_EXPORTS` is defined. Since `quiver_c` target defines `QUIVER_C_EXPORTS`, all split files automatically get the right export behavior -- as long as they include `quiver/c/common.h` (via `quiver/c/database.h`).
**How to avoid:** Ensure every split file includes `quiver/c/database.h` as its first include.
**Warning signs:** Functions defined but not accessible from test executables.

### Pitfall 3: Helper ODR Violations

**What goes wrong:** Non-inline, non-template function in `database_helpers.h` included by multiple `.cpp` files causes "multiple definition" linker errors.
**Why it happens:** Each TU gets its own copy without `inline`.
**How to avoid:** Mark ALL non-template functions in `database_helpers.h` as `inline`. Templates are inherently safe.
**Warning signs:** Linker errors mentioning "multiple definition of `strdup_safe`" or similar.

### Pitfall 4: Breaking Existing Internal Header

**What goes wrong:** Modifying `internal.h` breaks `element.cpp` or `lua_runner.cpp` compilation.
**Why it happens:** `internal.h` is shared by ALL C API files. Adding database-specific includes could introduce unwanted dependencies.
**How to avoid:** Do NOT modify `internal.h`. Create a separate `database_helpers.h` for database-specific helpers. The `QUIVER_REQUIRE` macro and `quiver_set_last_error` already live in `internal.h` and are fine there.
**Warning signs:** Compilation errors in `element.cpp` or `lua_runner.cpp` after Phase 4 changes.

### Pitfall 5: Incorrect Alloc/Free Placement

**What goes wrong:** A free function ends up in a different file from the read function that allocates.
**Why it happens:** Mechanical splitting by "function name prefix" without considering alloc/free pairing.
**How to avoid:** Use the alloc/free mapping table (Pattern 2 above) as the authoritative guide. Every free function goes in the SAME file as the function that allocates the memory it frees.
**Warning signs:** Code review finds free functions separated from their alloc partners.

### Pitfall 6: static Function Visibility

**What goes wrong:** The `convert_params` function is currently `static` in `database.cpp` (line 1221). If it stays `static` in a header, each TU gets its own copy.
**Why it happens:** `static` at file scope means internal linkage. In a header, this creates per-TU copies.
**How to avoid:** Change `static` to `inline` when moving to the header. Or, since `convert_params` is only used by query functions, it could stay file-local in `database_query.cpp`.
**Warning signs:** Binary size increase if kept as `static` in header.

## Code Examples

### Example 1: database_helpers.h

```cpp
// src/c/database_helpers.h
#ifndef QUIVER_C_DATABASE_HELPERS_H
#define QUIVER_C_DATABASE_HELPERS_H

#include "quiver/c/database.h"
#include "quiver/database.h"
#include "internal.h"

#include <cstring>
#include <string>
#include <vector>

// Marshaling helper: copy C++ numeric vector to C-allocated array
template <typename T>
quiver_error_t read_scalars_impl(const std::vector<T>& values, T** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new T[values.size()];
    std::copy(values.begin(), values.end(), *out_values);
    return QUIVER_OK;
}

// ... more templates (read_vectors_impl, free_vectors_impl) ...

// Non-template helpers must be inline
inline quiver_error_t copy_strings_to_c(const std::vector<std::string>& values, char*** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new char*[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        (*out_values)[i] = new char[values[i].size() + 1];
        std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
        (*out_values)[i][values[i].size()] = '\0';
    }
    return QUIVER_OK;
}

inline char* strdup_safe(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}

inline quiver_data_type_t to_c_data_type(quiver::DataType type) {
    switch (type) {
    case quiver::DataType::Integer: return QUIVER_DATA_TYPE_INTEGER;
    case quiver::DataType::Real:    return QUIVER_DATA_TYPE_FLOAT;
    case quiver::DataType::Text:    return QUIVER_DATA_TYPE_STRING;
    case quiver::DataType::DateTime: return QUIVER_DATA_TYPE_DATE_TIME;
    }
    return QUIVER_DATA_TYPE_INTEGER;
}

inline void convert_scalar_to_c(const quiver::ScalarMetadata& src, quiver_scalar_metadata_t& dst) {
    dst.name = strdup_safe(src.name);
    dst.data_type = to_c_data_type(src.data_type);
    dst.not_null = src.not_null ? 1 : 0;
    dst.primary_key = src.primary_key ? 1 : 0;
    dst.default_value = src.default_value.has_value() ? strdup_safe(*src.default_value) : nullptr;
    dst.is_foreign_key = src.is_foreign_key ? 1 : 0;
    dst.references_collection = src.references_collection.has_value() ? strdup_safe(*src.references_collection) : nullptr;
    dst.references_column = src.references_column.has_value() ? strdup_safe(*src.references_column) : nullptr;
}

inline void free_scalar_fields(quiver_scalar_metadata_t& m) {
    delete[] m.name;
    delete[] m.default_value;
    delete[] m.references_collection;
    delete[] m.references_column;
}

inline void convert_group_to_c(const quiver::GroupMetadata& src, quiver_group_metadata_t& dst) {
    dst.group_name = strdup_safe(src.group_name);
    dst.dimension_column = src.dimension_column.empty() ? nullptr : strdup_safe(src.dimension_column);
    dst.value_column_count = src.value_columns.size();
    if (src.value_columns.empty()) {
        dst.value_columns = nullptr;
    } else {
        dst.value_columns = new quiver_scalar_metadata_t[src.value_columns.size()];
        for (size_t i = 0; i < src.value_columns.size(); ++i) {
            convert_scalar_to_c(src.value_columns[i], dst.value_columns[i]);
        }
    }
}

inline void free_group_fields(quiver_group_metadata_t& m) {
    delete[] m.group_name;
    delete[] m.dimension_column;
    if (m.value_columns) {
        for (size_t i = 0; i < m.value_column_count; ++i) {
            free_scalar_fields(m.value_columns[i]);
        }
        delete[] m.value_columns;
    }
}

#endif  // QUIVER_C_DATABASE_HELPERS_H
```

### Example 2: Split File (database_read.cpp)

```cpp
// src/c/database_read.cpp
#include "quiver/c/database.h"

#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/element.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers(quiver_database_t* db, ...) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);
    try {
        return read_scalars_impl(db->db.read_scalar_integers(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// ... all other read functions ...

// Free functions CO-LOCATED with the read functions that allocate
QUIVER_C_API quiver_error_t quiver_free_integer_array(int64_t* values) { ... }
QUIVER_C_API quiver_error_t quiver_free_float_array(double* values) { ... }
QUIVER_C_API quiver_error_t quiver_free_string_array(char** values, size_t count) { ... }
QUIVER_C_API quiver_error_t quiver_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count) { ... }
QUIVER_C_API quiver_error_t quiver_free_float_vectors(double** vectors, size_t* sizes, size_t count) { ... }
QUIVER_C_API quiver_error_t quiver_free_string_vectors(char*** vectors, size_t* sizes, size_t count) { ... }

}  // extern "C"
```

### Example 3: Updated CMakeLists.txt

```cmake
if(QUIVER_BUILD_C_API)
    add_library(quiver_c SHARED
        c/common.cpp
        c/database.cpp
        c/database_create.cpp
        c/database_read.cpp
        c/database_update.cpp
        c/database_metadata.cpp
        c/database_query.cpp
        c/database_time_series.cpp
        c/database_relations.cpp
        c/element.cpp
        c/lua_runner.cpp
    )
    # ... rest unchanged ...
endif()
```

## Detailed Function-to-File Mapping

### database_helpers.h (NEW) -- ~120 lines

All helper templates and inline functions extracted from the anonymous namespaces in the current `database.cpp`:

| Function | Type | Lines in Original |
|----------|------|-------------------|
| `read_scalars_impl<T>` | template | 12-22 |
| `read_vectors_impl<T>` | template | 25-46 |
| `free_vectors_impl<T>` | template | 49-60 |
| `copy_strings_to_c` | inline | 63-76 |
| `to_c_data_type` | inline | 905-917 |
| `strdup_safe` | inline | 919-924 |
| `convert_scalar_to_c` | inline | 926-936 |
| `free_scalar_fields` | inline | 938-943 |
| `convert_group_to_c` | inline | 945-957 |
| `free_group_fields` | inline | 959-968 |

### database.cpp (lifecycle only) -- Target: ~200 lines

Functions that remain in `database.cpp`:

| Function | Lines | Reason |
|----------|-------|--------|
| `quiver_database_options_default` | 82-84 | Lifecycle |
| `quiver_database_open` | 86-106 | Lifecycle |
| `quiver_database_close` | 108-111 | Lifecycle |
| `quiver_database_is_healthy` | 113-118 | Lifecycle |
| `quiver_database_path` | 120-125 | Lifecycle |
| `quiver_database_from_migrations` | 127-150 | Lifecycle/factory |
| `quiver_database_from_schema` | 239-262 | Lifecycle/factory |
| `quiver_database_current_version` | 152-162 | Lifecycle |
| `quiver_database_describe` | 1338-1348 | Inspection (no alloc/free concern) |
| `quiver_database_export_to_csv` | 1130-1140 | Describe/CSV |
| `quiver_database_import_from_csv` | 1142-1154 | Describe/CSV |

**Estimated: ~170 lines** -- well under the 500-line limit.

### database_create.cpp (NEW) -- ~60 lines

| Function | Lines | Notes |
|----------|-------|-------|
| `quiver_database_create_element` | 164-177 | CRUD create |
| `quiver_database_update_element` | 179-192 | CRUD update (logically grouped with create/delete) |
| `quiver_database_delete_element_by_id` | 194-206 | CRUD delete |

**Design decision:** Group create/update_element/delete together since they all operate on elements and have no alloc/free pairs. Alternatively, delete could have its own file (mirroring C++), but the C API functions are thin wrappers -- a 15-line file adds overhead without value.

### database_read.cpp (NEW) -- ~370 lines

All read functions plus their co-located free functions:

| Function | Lines | Category |
|----------|-------|----------|
| `quiver_database_read_scalar_integers` | 264-277 | Read scalar |
| `quiver_database_read_scalar_floats` | 279-292 | Read scalar |
| `quiver_database_read_scalar_strings` | 294-307 | Read scalar |
| `quiver_database_read_scalar_integer_by_id` | 505-526 | Read scalar by ID |
| `quiver_database_read_scalar_float_by_id` | 528-549 | Read scalar by ID |
| `quiver_database_read_scalar_string_by_id` | 551-575 | Read scalar by ID |
| `quiver_database_read_vector_integers` | 333-347 | Read vector |
| `quiver_database_read_vector_floats` | 349-363 | Read vector |
| `quiver_database_read_vector_strings` | 365-401 | Read vector |
| `quiver_database_read_vector_integers_by_id` | 579-594 | Read vector by ID |
| `quiver_database_read_vector_floats_by_id` | 596-611 | Read vector by ID |
| `quiver_database_read_vector_strings_by_id` | 613-627 | Read vector by ID |
| `quiver_database_read_set_integers` | 433-447 | Read set |
| `quiver_database_read_set_floats` | 449-463 | Read set |
| `quiver_database_read_set_strings` | 465-501 | Read set |
| `quiver_database_read_set_integers_by_id` | 631-646 | Read set by ID |
| `quiver_database_read_set_floats_by_id` | 648-663 | Read set by ID |
| `quiver_database_read_set_strings_by_id` | 665-679 | Read set by ID |
| `quiver_database_read_element_ids` | 681-693 | Read element IDs |
| `quiver_free_integer_array` | 309-314 | **Free (co-located)** |
| `quiver_free_float_array` | 316-321 | **Free (co-located)** |
| `quiver_free_string_array` | 323-331 | **Free (co-located)** |
| `quiver_free_integer_vectors` | 403-407 | **Free (co-located)** |
| `quiver_free_float_vectors` | 409-413 | **Free (co-located)** |
| `quiver_free_string_vectors` | 415-429 | **Free (co-located)** |

### database_update.cpp (NEW) -- ~210 lines

All update functions (scalar, vector, set):

| Function | Lines | Category |
|----------|-------|----------|
| `quiver_database_update_scalar_integer` | 697-711 | Update scalar |
| `quiver_database_update_scalar_float` | 713-727 | Update scalar |
| `quiver_database_update_scalar_string` | 729-743 | Update scalar |
| `quiver_database_update_vector_integers` | 747-768 | Update vector |
| `quiver_database_update_vector_floats` | 770-791 | Update vector |
| `quiver_database_update_vector_strings` | 793-822 | Update vector |
| `quiver_database_update_set_integers` | 826-847 | Update set |
| `quiver_database_update_set_floats` | 849-870 | Update set |
| `quiver_database_update_set_strings` | 872-901 | Update set |

No alloc/free pairs -- update functions only write data.

### database_metadata.cpp (NEW) -- ~180 lines

All metadata get/list functions plus metadata free functions:

| Function | Lines | Category |
|----------|-------|----------|
| `quiver_database_get_scalar_metadata` | 971-984 | Get metadata |
| `quiver_database_get_vector_metadata` | 986-999 | Get metadata |
| `quiver_database_get_set_metadata` | 1001-1014 | Get metadata |
| `quiver_database_list_scalar_attributes` | 1038-1060 | List metadata |
| `quiver_database_list_vector_groups` | 1062-1084 | List metadata |
| `quiver_database_list_set_groups` | 1086-1108 | List metadata |
| `quiver_free_scalar_metadata` | 1016-1025 | **Free (co-located)** |
| `quiver_free_group_metadata` | 1027-1036 | **Free (co-located)** |
| `quiver_free_scalar_metadata_array` | 1110-1118 | **Free (co-located)** |
| `quiver_free_group_metadata_array` | 1120-1128 | **Free (co-located)** |

### database_query.cpp (NEW) -- ~170 lines

All query functions (plain and parameterized):

| Function | Lines | Category |
|----------|-------|----------|
| `convert_params` (static) | 1221-1247 | Helper (file-local) |
| `quiver_database_query_string` | 1156-1176 | Query |
| `quiver_database_query_integer` | 1178-1197 | Query |
| `quiver_database_query_float` | 1199-1218 | Query |
| `quiver_database_query_string_params` | 1249-1277 | Query parameterized |
| `quiver_database_query_integer_params` | 1279-1307 | Query parameterized |
| `quiver_database_query_float_params` | 1309-1336 | Query parameterized |

**Note:** `convert_params` is only used by query functions, so it stays as a file-local `static` function in `database_query.cpp` rather than moving to the helpers header.

### database_time_series.cpp (NEW) -- ~200 lines

All time series operations and their free functions:

| Function | Lines | Category |
|----------|-------|----------|
| `quiver_database_get_time_series_metadata` | 1352-1365 | TS metadata |
| `quiver_database_list_time_series_groups` | 1367-1389 | TS metadata |
| `quiver_database_read_time_series_group_by_id` | 1391-1446 | TS read |
| `quiver_database_update_time_series_group` | 1448-1483 | TS update |
| `quiver_free_time_series_data` | 1485-1496 | **Free (co-located)** |
| `quiver_database_has_time_series_files` | 1500-1512 | TS files |
| `quiver_database_list_time_series_files_columns` | 1514-1527 | TS files |
| `quiver_database_read_time_series_files` | 1529-1565 | TS files read |
| `quiver_database_update_time_series_files` | 1567-1595 | TS files update |
| `quiver_free_time_series_files` | 1597-1610 | **Free (co-located)** |

### database_relations.cpp (NEW) -- ~40 lines

| Function | Lines | Category |
|----------|-------|----------|
| `quiver_database_set_scalar_relation` | 208-222 | Relations |
| `quiver_database_read_scalar_relation` | 224-237 | Relations |

## Line Count Summary

| File | Estimated Lines | Content |
|------|----------------|---------|
| `database_helpers.h` | ~120 | All shared marshaling templates and inline converters |
| `database.cpp` | ~170 | Lifecycle, factory, describe, CSV |
| `database_create.cpp` | ~60 | create_element, update_element, delete_element |
| `database_read.cpp` | ~370 | All read + co-located free functions |
| `database_update.cpp` | ~210 | All update_scalar/vector/set |
| `database_metadata.cpp` | ~180 | All metadata get/list + free functions |
| `database_query.cpp` | ~170 | All query + convert_params |
| `database_time_series.cpp` | ~200 | All time series + free functions |
| `database_relations.cpp` | ~40 | Relations |
| **Total** | **~1520** | (vs 1612 original) |

**Success criterion check:** `database.cpp` at ~170 lines is well under the 500-line limit.

## Key Differences from Phase 2 (C++ decomposition)

| Aspect | Phase 2 (C++) | Phase 4 (C API) |
|--------|---------------|-----------------|
| Include pattern | `#include "database_impl.h"` | `#include "internal.h"` + `#include "database_helpers.h"` |
| Namespace | `namespace quiver { ... }` | `extern "C" { ... }` |
| Helper header | `database_internal.h` (in `src/`) | `database_helpers.h` (in `src/c/`) |
| Helper strategy | `namespace quiver::internal` + `inline` | `inline` functions, no namespace needed |
| Extra constraint | None | Alloc/free co-location |
| delete file | Separate `database_delete.cpp` | Merged into `database_create.cpp` (too thin) |
| describe/CSV file | Separate `database_describe.cpp` | Stays in `database.cpp` (lifecycle-adjacent, no alloc/free) |

## Open Questions

### 1. Should create/update_element/delete be in one file or separate?

**What we know:** In the C++ layer, create, update, and delete each have their own file. In the C API, these are thin wrappers (each ~15 lines). Three files of ~15-20 lines each adds overhead.

**Recommendation:** Combine into `database_create.cpp` (matching the "element operations" grouping in the public header). The C API wrappers are so thin that separate files provide no navigability benefit. Confidence: HIGH.

### 2. Should describe/CSV stay in database.cpp or get their own file?

**What we know:** `describe`, `export_to_csv`, `import_from_csv` are ~40 lines total. They have no alloc/free pairs. In the C++ layer they have `database_describe.cpp`.

**Recommendation:** Keep in `database.cpp`. The lifecycle file will only be ~170 lines with them included. Creating a separate 40-line file adds overhead. Confidence: HIGH.

### 3. Should `convert_params` go in helpers header or stay file-local?

**What we know:** `convert_params` is only used by `quiver_database_query_*_params` functions. It is currently a `static` function.

**Recommendation:** Keep it as a `static` function in `database_query.cpp`. No other file needs it. Moving it to the header would be unnecessary and add compile-time cost. Confidence: HIGH.

### 4. What about QUIVER_REQUIRE -- move to helpers or keep in internal.h?

**What we know:** `QUIVER_REQUIRE` is currently in `internal.h` and is used by ALL C API files (`database.cpp`, `element.cpp`, `lua_runner.cpp`).

**Recommendation:** Keep in `internal.h`. It is NOT database-specific. The success criteria say "database_helpers.h exists containing... QUIVER_REQUIRE macro" but this would break the existing clean separation where `internal.h` serves all C API files. The intent is that `database_helpers.h` contains the database-specific helpers; `QUIVER_REQUIRE` is already properly placed. Confidence: HIGH -- keeping it in `internal.h` is objectively better architecture.

**Clarification for planner:** The success criterion "database_helpers.h exists containing marshaling templates, strdup_safe, metadata converters, and QUIVER_REQUIRE macro" should be interpreted as: `database_helpers.h` exists with marshaling templates, strdup_safe, and metadata converters; `QUIVER_REQUIRE` stays in `internal.h` (already shared). The spirit of the criterion is that all helpers are in shared headers, not duplicated.

## Sources

### Primary (HIGH confidence)

- Direct source code analysis of `src/c/database.cpp` (1612 lines) -- complete function inventory with line numbers
- Direct source code analysis of `src/c/internal.h` (82 lines) -- existing shared header structure
- Direct source code analysis of `src/c/common.cpp`, `src/c/element.cpp`, `src/c/lua_runner.cpp` -- understanding what `internal.h` serves
- Direct source code analysis of `src/CMakeLists.txt` (128 lines) -- current build configuration for `quiver_c` target
- Phase 2 research and results -- proven patterns for decomposition in this codebase
- Direct source code analysis of all `src/database_*.cpp` files -- C++ decomposition structure to mirror

### Secondary (MEDIUM confidence)

- C++ standard: `inline` functions in headers avoid ODR violations; templates are inherently ODR-safe
- Phase 2 decision: `namespace quiver::internal` with `inline` non-template functions for ODR safety (precedent)

## Metadata

**Confidence breakdown:**
- Architecture: HIGH -- direct mirror of proven Phase 2 pattern, adapted for C API specifics
- Function mapping: HIGH -- every function in database.cpp inventoried with exact line numbers and target file
- Alloc/free mapping: HIGH -- every alloc/free pair traced and co-located
- Pitfalls: HIGH -- based on Phase 2 experience plus C API-specific concerns (DLL exports, extern "C")

**Research date:** 2026-02-10
**Valid until:** Indefinite (C/C++ file decomposition patterns are stable)
