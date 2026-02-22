# Phase 6: C API - Research

**Researched:** 2026-02-22
**Domain:** C API FFI wrapper for CSV export with flat struct marshaling
**Confidence:** HIGH

## Summary

Phase 6 wraps the completed C++ CSV export functionality (`export_csv` + `CSVExportOptions`) through FFI-safe C API functions. The central challenge is flattening the nested C++ `unordered_map<string, unordered_map<int64_t, string>>` enum_labels into a flat struct that FFI generators (Julia Clang.jl and Dart ffigen) can parse into correct binding declarations.

The codebase has well-established patterns for every aspect of this phase: options structs (`quiver_database_options_t` in `options.h`), factory default functions (`quiver_database_options_default()`), header organization (`include/quiver/c/`), implementation file splitting (`src/c/database_*.cpp`), test organization (`tests/test_c_api_database_*.cpp`), and the CMakeLists.txt registration pattern. The implementation is mechanical: create a new flat struct, a conversion function, and update the existing `export_csv` stub.

**Primary recommendation:** Use a grouped-by-attribute parallel arrays layout for enum_labels (attribute_names[], entry counts[], flattened values[], flattened labels[]) -- this avoids nested pointer arrays beyond one level and maps cleanly to the FFI generators' capabilities.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Replace existing `quiver_database_export_csv(db, collection, group, path)` stub with new signature that includes `const quiver_csv_export_options_t* opts` parameter -- breaking change is acceptable (WIP project)
- Options passed by const pointer, consistent with `quiver_database_options_t` pattern
- Caller-owned memory: function borrows pointers during the call, does not copy
- `quiver_csv_export_options_default()` returns zero-initialized struct with `date_time_format = ""` (empty string = no formatting, matching C++ convention)
- Options struct is always required -- no NULL shortcut for defaults
- Group parameter: empty string `""` means scalar export, `NULL` is an error (consistent with C++ std::string semantics)
- New `include/quiver/c/csv.h` header for `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` factory
- `quiver_database_export_csv()` function declaration stays in `include/quiver/c/database.h` (database.h includes csv.h)
- Implementation in new `src/c/database_csv.cpp` file, mirroring C++ `src/database_csv.cpp` pattern
- Keep existing `quiver_database_import_csv()` stub as-is
- New `tests/test_c_api_database_csv.cpp` test file
- Mirror the full C++ CSV test suite through C API (same scenarios: scalar export, group export, enum labels, date formatting, empty collection, NULL values, RFC 4180 escaping)
- Reuse existing test schemas from `tests/schemas/valid/`
- Full const qualifiers on options struct: `const char* const*` for string arrays, `const int64_t*` for values
- Signals borrowing semantics at the type level

### Claude's Discretion
- Enum labels flat array layout approach (grouped vs triples vs other)
- Whether enum labels are a sub-struct or inlined in options
- Whether to provide a `quiver_csv_export_options_free()` function
- Header documentation style for ownership contract
- Options struct validation (trust caller vs validate consistency)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CAPI-01 | `quiver_csv_export_options_t` flat struct with parallel arrays for enum_labels, passable through FFI | Struct design with grouped-by-attribute parallel arrays pattern; verified FFI generator compatibility with `const char* const*` and `const int64_t*` types already used in codebase |
| CAPI-02 | `quiver_database_export_csv(db, collection, group, path, opts)` returning `quiver_error_t`; group="" for scalars | Direct wrapper following existing `export_csv` stub pattern in `src/c/database.cpp` lines 133-143; conversion from flat C struct to C++ `CSVExportOptions` |
</phase_requirements>

## Standard Stack

### Core
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| C API header pattern | `include/quiver/c/options.h` | Flat struct + factory default | Established project pattern for FFI-safe options |
| QUIVER_REQUIRE macro | `src/c/internal.h` | Null argument validation | Consistent error handling across all C API functions |
| database_helpers.h | `src/c/database_helpers.h` | strdup_safe, type conversion | Reusable marshaling utilities |
| GoogleTest | `tests/CMakeLists.txt` | C API test framework | Same test framework used by all existing C API tests |

### Supporting
| Component | Location | Purpose | When to Use |
|-----------|----------|---------|-------------|
| Clang.jl generator | `bindings/julia/generator/` | Julia FFI binding generation | After C API header is finalized |
| Dart ffigen | `bindings/dart/ffigen.yaml` | Dart FFI binding generation | After C API header is finalized |

### Alternatives Considered
None -- this phase uses only existing project infrastructure. No new libraries or tools needed.

## Architecture Patterns

### Pattern 1: Flat Options Struct with Parallel Arrays (Grouped-by-Attribute)

**What:** Flatten the C++ `unordered_map<string, unordered_map<int64_t, string>>` into four parallel arrays grouped by attribute name, plus a `date_time_format` string.

**When to use:** When representing nested maps through FFI where generators cannot handle C++ containers.

**Recommended struct layout:**
```c
typedef struct {
    // Date/time formatting (empty string = no formatting)
    const char* date_time_format;

    // Enum labels: maps attribute_name -> {integer_value -> string_label}
    // Flattened as grouped-by-attribute parallel arrays:
    //   enum_attribute_names[i]  = attribute name for group i
    //   enum_entry_counts[i]     = number of (value, label) pairs in group i
    //   enum_values[]            = flattened int64_t values (all groups concatenated)
    //   enum_labels[]            = flattened string labels (all groups concatenated)
    //   enum_attribute_count     = number of attribute groups
    //
    // Example: {"status": {1: "Active", 2: "Inactive"}, "priority": {0: "Low"}}
    //   enum_attribute_names = ["status", "priority"]
    //   enum_entry_counts    = [2, 1]
    //   enum_values          = [1, 2, 0]
    //   enum_labels          = ["Active", "Inactive", "Low"]
    //   enum_attribute_count = 2
    const char* const* enum_attribute_names;
    const size_t*      enum_entry_counts;
    const int64_t*     enum_values;
    const char* const* enum_labels;
    size_t             enum_attribute_count;
} quiver_csv_export_options_t;
```

**Why this layout:**
- No nested pointers beyond one level (arrays of pointers to strings are already used throughout the codebase: `const char* const*` in `quiver_database_update_vector_strings`, `quiver_database_update_time_series_group`, etc.)
- Both Julia Clang.jl and Dart ffigen successfully parse `const char* const*`, `const int64_t*`, and `const size_t*` -- these exact types already appear in the generated bindings
- The grouped-by-attribute layout keeps related data together and allows efficient reconstruction of the C++ `unordered_map` structure
- Total entry count is derivable from summing `enum_entry_counts[]`, so no redundant `total_entries` field needed
- All arrays are NULL when `enum_attribute_count == 0` (default case)

**Confidence:** HIGH -- this pattern uses only pointer types already proven to work with both FFI generators in this codebase.

### Pattern 2: Conversion Function (C flat struct to C++ CSVExportOptions)

**What:** A static helper in `src/c/database_csv.cpp` that reconstructs the C++ `CSVExportOptions` from the flat C struct.

**Example:**
```cpp
static quiver::CSVExportOptions convert_options(const quiver_csv_export_options_t* opts) {
    quiver::CSVExportOptions cpp_opts;
    cpp_opts.date_time_format = opts->date_time_format ? opts->date_time_format : "";

    size_t offset = 0;
    for (size_t i = 0; i < opts->enum_attribute_count; ++i) {
        std::string attr_name = opts->enum_attribute_names[i];
        auto& attr_map = cpp_opts.enum_labels[attr_name];
        for (size_t j = 0; j < opts->enum_entry_counts[i]; ++j) {
            attr_map[opts->enum_values[offset + j]] = opts->enum_labels[offset + j];
        }
        offset += opts->enum_entry_counts[i];
    }

    return cpp_opts;
}
```

**Why:** This follows the existing `convert_params()` pattern from `src/c/database_query.cpp` -- a static conversion function that transforms flat C arrays into C++ containers before calling the C++ API.

**Confidence:** HIGH -- identical pattern to existing codebase.

### Pattern 3: Default Factory Function

**What:** `quiver_csv_export_options_default()` returns a zero-initialized struct.

**Example:**
```c
QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void) {
    quiver_csv_export_options_t opts = {};
    opts.date_time_format = "";
    return opts;
}
```

**Why:** Mirrors `quiver_database_options_default()` in `src/c/database.cpp` line 10-12. Direct return (not out-parameter), consistent with CLAUDE.md's exception list for utility functions with direct return.

**Confidence:** HIGH -- direct replica of existing pattern.

### Pattern 4: Updated export_csv Function

**What:** Replace the current 4-parameter stub with a 5-parameter version including options.

**Current (to be replaced):**
```c
QUIVER_C_API quiver_error_t quiver_database_export_csv(
    quiver_database_t* db, const char* collection,
    const char* group, const char* path);
```

**New:**
```c
QUIVER_C_API quiver_error_t quiver_database_export_csv(
    quiver_database_t* db, const char* collection,
    const char* group, const char* path,
    const quiver_csv_export_options_t* opts);
```

**Implementation pattern:**
```cpp
QUIVER_C_API quiver_error_t quiver_database_export_csv(
    quiver_database_t* db, const char* collection,
    const char* group, const char* path,
    const quiver_csv_export_options_t* opts) {
    QUIVER_REQUIRE(db, collection, group, path, opts);

    try {
        auto cpp_opts = convert_options(opts);
        db->db.export_csv(collection, group, path, cpp_opts);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

**Confidence:** HIGH -- mechanical wrapper following exact codebase pattern.

### Anti-Patterns to Avoid
- **Nested struct pointers for enum_labels:** A `quiver_enum_entry_t** entries` approach creates two levels of indirection beyond strings. FFI generators handle it, but it complicates memory layout and binding usage compared to flat parallel arrays.
- **NULL opts for defaults:** CONTEXT.md explicitly states options struct is always required -- no NULL shortcut. The `QUIVER_REQUIRE` macro will enforce this.
- **Copying strings in the C wrapper:** CONTEXT.md specifies caller-owned memory with borrowing semantics. The conversion function reads from the caller's arrays during the call and constructs temporary C++ strings. No need to strdup.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Null argument validation | Manual if-checks | `QUIVER_REQUIRE` macro | Consistent error messages, already handles up to 9 args |
| String duplication | Manual new[]/memcpy | `strdup_safe()` from database_helpers.h | Already proven, handles null termination |
| C++ CSVExportOptions defaults | Manual field initialization | `quiver::default_csv_export_options()` | C++ factory already handles correct defaults |

**Key insight:** Every building block needed for this phase already exists in the codebase. The implementation is purely assembly of existing patterns.

## Common Pitfalls

### Pitfall 1: QUIVER_REQUIRE with Empty String vs NULL
**What goes wrong:** The `group` parameter uses `""` for scalar export. `QUIVER_REQUIRE` checks for null pointers, not empty strings. If someone passes `NULL` for group, `QUIVER_REQUIRE` correctly rejects it.
**Why it happens:** C has no distinction between "empty" and "null" for strings at the type level.
**How to avoid:** `QUIVER_REQUIRE(db, collection, group, path, opts)` handles the NULL case. The C++ layer handles the `""` vs non-empty routing. No additional validation needed.
**Warning signs:** Tests that pass `NULL` for group should return `QUIVER_ERROR`, not crash.

### Pitfall 2: Forgetting to Register Files in CMakeLists.txt
**What goes wrong:** New `src/c/database_csv.cpp` compiles but isn't linked into `quiver_c` target, or new test file isn't registered in `quiver_c_tests`.
**Why it happens:** The CMakeLists.txt uses explicit file lists, not GLOBs for source files.
**How to avoid:** Add `c/database_csv.cpp` to the `quiver_c` target in `src/CMakeLists.txt` (line 94-107) and `test_c_api_database_csv.cpp` to `quiver_c_tests` in `tests/CMakeLists.txt` (line 46-57).
**Warning signs:** Linker errors for unresolved `quiver_database_export_csv` symbol.

### Pitfall 3: FFI Generator Include Chain
**What goes wrong:** The new `include/quiver/c/csv.h` header is created but FFI generators don't pick it up.
**Why it happens:** Julia generator auto-discovers all `.h` files in `include/quiver/c/` via `readdir(include_dir)` (see `generator.jl` line 16-18). Dart ffigen uses an explicit entry-point list in `ffigen.yaml`.
**How to avoid:** Julia generator will auto-discover `csv.h` -- no changes needed. Dart ffigen requires adding `csv.h` to the `entry-points` and `include-directives` lists in `bindings/dart/ffigen.yaml`. However, since `database.h` will `#include "csv.h"`, Dart ffigen may transitively discover the types. Validation: run both generators after implementation and verify the CSV types appear in output.
**Warning signs:** Generated bindings missing `quiver_csv_export_options_t` struct.

### Pitfall 4: date_time_format Empty String vs NULL
**What goes wrong:** The default factory returns `date_time_format = ""` but the conversion function needs to handle the pointer being either `""` or `NULL`.
**Why it happens:** C callers might set `date_time_format = NULL` to mean "no formatting", which is semantically the same as `""`.
**How to avoid:** The conversion function should treat NULL as empty string: `opts->date_time_format ? opts->date_time_format : ""`. The default factory sets it to `""`.
**Warning signs:** Crash when `date_time_format` is NULL.

### Pitfall 5: Moving export_csv Implementation Out of database.cpp
**What goes wrong:** The current export_csv C API stub lives in `src/c/database.cpp` (lines 131-143). When creating the new `src/c/database_csv.cpp`, the old stub must be removed from `database.cpp` to avoid duplicate symbol errors.
**Why it happens:** The decision to create a new `database_csv.cpp` means the function definition moves.
**How to avoid:** Remove the `quiver_database_export_csv` function from `src/c/database.cpp` when adding it to `src/c/database_csv.cpp`. Keep `quiver_database_import_csv` in `database.cpp` as-is.
**Warning signs:** Linker "multiple definition" errors.

## Code Examples

### Complete Header: include/quiver/c/csv.h
```c
#ifndef QUIVER_C_CSV_H
#define QUIVER_C_CSV_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// CSV export options for controlling enum resolution and date formatting.
// All pointers are borrowed -- caller owns the memory, function reads during call only.
//
// Enum labels map attribute names to (integer_value -> string_label) pairs.
// Represented as grouped-by-attribute parallel arrays:
//   enum_attribute_names[i]  = attribute name for group i
//   enum_entry_counts[i]     = number of entries in group i
//   enum_values[]            = all integer values, concatenated across groups
//   enum_labels[]            = all string labels, concatenated across groups
//   enum_attribute_count     = number of attribute groups (0 = no enum mapping)
typedef struct {
    const char*        date_time_format;       // strftime format; "" = no formatting
    const char* const* enum_attribute_names;   // [enum_attribute_count]
    const size_t*      enum_entry_counts;      // [enum_attribute_count]
    const int64_t*     enum_values;            // [sum of enum_entry_counts]
    const char* const* enum_labels;            // [sum of enum_entry_counts]
    size_t             enum_attribute_count;    // number of attributes with enum mappings
} quiver_csv_export_options_t;

// Returns default options (no enum mapping, no date formatting).
QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_CSV_H
```

### Complete Implementation: src/c/database_csv.cpp
```cpp
#include "internal.h"
#include "quiver/c/csv.h"
#include "quiver/c/database.h"
#include "quiver/csv.h"

#include <string>

static quiver::CSVExportOptions convert_options(const quiver_csv_export_options_t* opts) {
    quiver::CSVExportOptions cpp_opts;
    cpp_opts.date_time_format = opts->date_time_format ? opts->date_time_format : "";

    size_t offset = 0;
    for (size_t i = 0; i < opts->enum_attribute_count; ++i) {
        std::string attr_name = opts->enum_attribute_names[i];
        auto& attr_map = cpp_opts.enum_labels[attr_name];
        for (size_t j = 0; j < opts->enum_entry_counts[i]; ++j) {
            attr_map[opts->enum_values[offset + j]] = opts->enum_labels[offset + j];
        }
        offset += opts->enum_entry_counts[i];
    }

    return cpp_opts;
}

extern "C" {

QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void) {
    quiver_csv_export_options_t opts = {};
    opts.date_time_format = "";
    return opts;
}

QUIVER_C_API quiver_error_t quiver_database_export_csv(quiver_database_t* db,
                                                       const char* collection,
                                                       const char* group,
                                                       const char* path,
                                                       const quiver_csv_export_options_t* opts) {
    QUIVER_REQUIRE(db, collection, group, path, opts);

    try {
        auto cpp_opts = convert_options(opts);
        db->db.export_csv(collection, group, path, cpp_opts);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
```

### Test Pattern: tests/test_c_api_database_csv.cpp (representative excerpt)
```cpp
#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/c/csv.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

namespace fs = std::filesystem;

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static fs::path temp_csv(const std::string& name) {
    return fs::temp_directory_path() / ("quiver_c_test_" + name + ".csv");
}

TEST(DatabaseCApiCSV, ExportCSV_ScalarExport_HeaderAndData) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(
        ":memory:", VALID_SCHEMA("csv_export.sql").c_str(), &options, &db), QUIVER_OK);

    // Create element via C API...
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item1");
    quiver_element_set_string(e1, "name", "Alpha");
    quiver_element_set_integer(e1, "status", 1);
    quiver_element_set_float(e1, "price", 9.99);
    quiver_element_set_string(e1, "date_created", "2024-01-15T10:30:00");
    quiver_element_set_string(e1, "notes", "first");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    auto csv_path = temp_csv("ScalarExport");
    auto csv_opts = quiver_csv_export_options_default();
    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", csv_path.string().c_str(), &csv_opts), QUIVER_OK);

    auto content = read_file(csv_path.string());
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);

    fs::remove(csv_path);
    quiver_database_close(db);
}

TEST(DatabaseCApiCSV, ExportCSV_EnumLabels_ReplacesIntegers) {
    // ... setup db and create elements ...

    // Set up enum labels for "status" attribute
    const char* attr_names[] = {"status"};
    size_t entry_counts[] = {2};
    int64_t values[] = {1, 2};
    const char* labels[] = {"Active", "Inactive"};

    quiver_csv_export_options_t csv_opts = {};
    csv_opts.date_time_format = "";
    csv_opts.enum_attribute_names = attr_names;
    csv_opts.enum_entry_counts = entry_counts;
    csv_opts.enum_values = values;
    csv_opts.enum_labels = labels;
    csv_opts.enum_attribute_count = 1;

    ASSERT_EQ(quiver_database_export_csv(db, "Items", "", path, &csv_opts), QUIVER_OK);

    auto content = read_file(path);
    EXPECT_NE(content.find("Active"), std::string::npos);
    EXPECT_NE(content.find("Inactive"), std::string::npos);
    // ...
}
```

## Discretion Recommendations

### Enum Labels Layout: Grouped-by-Attribute (Recommended)

**Decision:** Use the grouped-by-attribute parallel arrays layout as shown above.

**Rationale:**
- **Triples layout** (attribute_name[], value[], label[], count) would repeat attribute names for every entry, wasting space and making reconstruction harder.
- **Sub-struct layout** (`quiver_enum_attribute_t` with per-attribute arrays) adds a nested struct pointer -- technically works, but adds complexity for both the header and FFI generator output. Julia Clang.jl and Dart ffigen handle nested structs, but the binding code becomes more verbose.
- **Grouped-by-attribute** (recommended) keeps all arrays flat at one level, uses `enum_entry_counts[]` to partition the concatenated value/label arrays. Clean reconstruction in conversion function via running offset. Caller setup is straightforward.

### No quiver_csv_export_options_free() (Recommended)

**Decision:** Do not provide a free function.

**Rationale:** The struct is caller-owned and stack-allocated. The caller manages their own arrays. The C API borrows pointers during the call only. There is nothing to free. This matches `quiver_database_options_t` which also has no free function.

### Trust Caller for Consistency (Recommended)

**Decision:** Do not validate that `enum_values` and `enum_labels` array lengths match `sum(enum_entry_counts)`.

**Rationale:** The codebase philosophy is "clean code over defensive code (assume callers obey contracts)." The `convert_options` function reads exactly `sum(enum_entry_counts[i])` entries from the value/label arrays. If the caller provides inconsistent counts, the behavior is undefined (likely out-of-bounds read), which is the standard C API contract. This matches how `quiver_database_update_time_series_group` trusts `column_count` and `row_count`.

## File Changes Summary

### New Files
| File | Purpose |
|------|---------|
| `include/quiver/c/csv.h` | `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` factory |
| `src/c/database_csv.cpp` | `convert_options()` helper, factory implementation, `quiver_database_export_csv()` wrapper |
| `tests/test_c_api_database_csv.cpp` | Full C API CSV test suite mirroring C++ tests |

### Modified Files
| File | Change |
|------|--------|
| `include/quiver/c/database.h` | Add `#include "csv.h"`, update `quiver_database_export_csv` signature to 5 params |
| `src/c/database.cpp` | Remove `quiver_database_export_csv` stub (moves to database_csv.cpp) |
| `src/CMakeLists.txt` | Add `c/database_csv.cpp` to `quiver_c` target |
| `tests/CMakeLists.txt` | Add `test_c_api_database_csv.cpp` to `quiver_c_tests` target |
| `bindings/dart/ffigen.yaml` | Add `csv.h` to entry-points and include-directives (if not transitively discovered) |

### Unchanged Files
| File | Why |
|------|-----|
| `bindings/julia/generator/generator.jl` | Auto-discovers all `.h` files via `readdir` -- no change needed |
| `include/quiver/csv.h` | C++ struct unchanged |
| `src/database_csv.cpp` | C++ implementation unchanged |
| `tests/test_database_csv.cpp` | C++ tests unchanged |

## Open Questions

1. **Dart ffigen transitive include discovery**
   - What we know: `database.h` will `#include "csv.h"`, so ffigen may discover `quiver_csv_export_options_t` transitively
   - What's unclear: Whether ffigen requires explicit entry-point listing for types in transitively included headers
   - Recommendation: Add `csv.h` to `ffigen.yaml` entry-points defensively. If ffigen discovers it transitively, having it explicitly listed is harmless. Verify by running the generator after implementation.

## Sources

### Primary (HIGH confidence)
- Codebase inspection: `include/quiver/c/options.h` -- existing options struct pattern
- Codebase inspection: `include/quiver/c/database.h` -- current export_csv stub signature, all pointer types used in API
- Codebase inspection: `src/c/database.cpp` lines 10-12, 131-143 -- factory default pattern, current export_csv implementation
- Codebase inspection: `src/c/database_query.cpp` lines 10-36 -- `convert_params()` flat-array conversion pattern
- Codebase inspection: `src/c/database_helpers.h` -- strdup_safe, conversion utilities
- Codebase inspection: `src/c/internal.h` -- QUIVER_REQUIRE macro
- Codebase inspection: `include/quiver/csv.h` -- C++ CSVExportOptions struct definition
- Codebase inspection: `src/database_csv.cpp` -- C++ export_csv implementation
- Codebase inspection: `tests/test_database_csv.cpp` -- C++ test suite to mirror
- Codebase inspection: `tests/test_c_api_database_transaction.cpp` -- C API test patterns
- Codebase inspection: `src/CMakeLists.txt` lines 93-107 -- quiver_c target file list
- Codebase inspection: `tests/CMakeLists.txt` lines 45-57 -- quiver_c_tests target file list
- Codebase inspection: `bindings/julia/generator/generator.jl` -- Julia FFI auto-discovery
- Codebase inspection: `bindings/dart/ffigen.yaml` -- Dart FFI explicit header list

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components are existing codebase patterns, no new libraries
- Architecture: HIGH -- flat parallel arrays use only pointer types already proven with both FFI generators
- Pitfalls: HIGH -- identified from direct codebase inspection (CMakeLists registration, include chains, NULL vs empty string)

**Research date:** 2026-02-22
**Valid until:** Indefinite -- this is codebase-internal research with no external dependency version concerns
