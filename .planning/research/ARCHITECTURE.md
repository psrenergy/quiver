# Architecture Research: CSV Library Integration and Header Consolidation

**Domain:** CSV library integration into existing SQLite wrapper + C API header reorganization
**Researched:** 2026-02-22
**Confidence:** HIGH

## System Overview

The v0.5 milestone has two independent changes: replace the hand-rolled CSV writer in the C++ layer, and merge two C API headers. Each change has a distinct integration surface with different ripple effects.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Language Bindings                                   │
│  Julia (c_api.jl + database_csv.jl)                                         │
│  Dart  (bindings.dart + database_csv.dart)                                  │
│  Lua   (lua_runner.cpp via sol2)                                             │
└───────────────────────────────┬────────────────────────────────────────────┘
                                │ C API (FFI boundary)
┌───────────────────────────────▼────────────────────────────────────────────┐
│                          C API Layer                                         │
│  include/quiver/c/database.h         -- includes csv.h + options.h           │
│  include/quiver/c/options.h          -- DatabaseOptions + LogLevel           │
│  include/quiver/c/csv.h              -- CSVExportOptions (MERGE TARGET)      │
│  src/c/database_csv.cpp              -- convert_options(), C API wrappers    │
└───────────────────────────────┬────────────────────────────────────────────┘
                                │ C++ library boundary
┌───────────────────────────────▼────────────────────────────────────────────┐
│                          C++ Core                                            │
│  include/quiver/csv.h                -- C++ CSVExportOptions struct          │
│  include/quiver/database.h           -- Database class, export_csv()         │
│  src/database_csv.cpp                -- LIBRARY INTEGRATION POINT            │
│    csv_escape()    <-- REPLACE with csv-parser CSVWriter                    │
│    write_csv_row() <-- REPLACE with csv-parser CSVWriter                    │
│    parse_iso8601() <-- KEEP (date formatting, no library equivalent needed) │
│    format_datetime() <-- KEEP                                               │
│    value_to_csv_string() <-- KEEP                                           │
└───────────────────────────────┬────────────────────────────────────────────┘
                                │
┌───────────────────────────────▼────────────────────────────────────────────┐
│  External Dependencies                                                       │
│  SQLite3, spdlog, sol2, csv-parser (NEW via FetchContent)                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Change 1: CSV Library Integration

### Integration Point

The hand-rolled writer lives entirely in `src/database_csv.cpp`. Two static functions are the targets:

```cpp
// CURRENT -- remove both functions:
static std::string csv_escape(const std::string& field) { ... }  // ~20 lines
static void write_csv_row(std::ofstream& file, const std::vector<std::string>& fields) { ... }  // ~8 lines

// REPLACEMENT -- use csv-parser CSVWriter:
// csv::make_csv_writer(file) << fields_vector;
```

The library replaces these two helpers only. Everything else in `src/database_csv.cpp` stays: `parse_iso8601()`, `format_datetime()`, `value_to_csv_string()`, `Database::export_csv()` logic.

### Recommended Library: vincentlaucsb/csv-parser

**Rationale:** Read+write capable (enabling future import), RFC 4180 compliant writer via `csv::make_csv_writer()`, FetchContent compatible, C++11 minimum (project uses C++20), single-header variant available, active maintenance.

**Writing API (verified from test suite):**

```cpp
#include "csv.hpp"

// Create writer bound to an ofstream
auto writer = csv::make_csv_writer(file);

// Write a row -- operator<< accepts vector, array, or tuple of stringifiable values
writer << std::vector<std::string>{"label", "value", "date_created"};
writer << std::vector<std::string>{"Item1", "42", "2024-01-15"};
```

The library handles RFC 4180 quoting (comma, double-quote, newline in field) automatically. `csv_escape()` and `write_csv_row()` become one line each.

**FetchContent integration:**

```cmake
FetchContent_Declare(csv
    GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
    GIT_TAG 2.3.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(csv)

# Link against the core quiver library:
target_link_libraries(quiver PRIVATE csv)
```

### Files Changed by Library Integration

| File | Change |
|------|--------|
| `cmake/Dependencies.cmake` | Add FetchContent_Declare + FetchContent_MakeAvailable for csv-parser |
| `src/CMakeLists.txt` | Add `csv` to `target_link_libraries(quiver PRIVATE ...)` |
| `src/database_csv.cpp` | Remove `csv_escape()` + `write_csv_row()`; add `#include "csv.hpp"`; replace call sites with `make_csv_writer(file) << fields` |

No other files change. The C API, bindings, headers, and tests are entirely unaffected by the library swap.

### Binary Mode Constraint

Current code opens the file in binary mode to prevent Windows CRLF injection:

```cpp
std::ofstream file(path, std::ios::binary);
```

The csv-parser `make_csv_writer(file)` accepts the stream as-is; it does not reopen the file. Binary mode must be retained on the `std::ofstream` before passing it to `make_csv_writer`. This is not an issue -- the library wraps the stream, not the file path.

### What Does NOT Change

- `parse_iso8601()` and `format_datetime()` -- date formatting has no library equivalent in csv-parser; these stay unchanged.
- `value_to_csv_string()` -- converts `Value` variant to string; still needed to prepare the string values before passing to the writer.
- The `std::ofstream` lifecycle management, parent directory creation.
- `src/c/database_csv.cpp` -- C API wrapper is unaffected.
- All test files -- behavior is identical, output is identical (both produce RFC 4180).
- All bindings -- the library boundary is `Database::export_csv()` which is unchanged in signature.

---

## Change 2: Merge csv.h into options.h

### Current State

Two separate files in `include/quiver/c/`:

```
include/quiver/c/options.h
  - quiver_log_level_t enum
  - quiver_database_options_t struct
  - quiver_database_options_default() declaration (implemented in src/c/database.cpp)

include/quiver/c/csv.h
  - quiver_csv_export_options_t struct (includes "common.h")
  - quiver_csv_export_options_default() declaration
```

### Merge Target

`include/quiver/c/csv.h` is deleted. Its content moves into `include/quiver/c/options.h`.

**Resulting `options.h` structure:**

```c
#ifndef QUIVER_C_OPTIONS_H
#define QUIVER_C_OPTIONS_H

#include "common.h"   // ADD: needed for QUIVER_C_API, stddef.h, stdint.h

#ifdef __cplusplus
extern "C" {
#endif

// Log level enum (unchanged)
typedef enum {
    QUIVER_LOG_DEBUG = 0,
    ...
} quiver_log_level_t;

// Database options (unchanged)
typedef struct {
    int read_only;
    quiver_log_level_t console_level;
} quiver_database_options_t;

// CSV export options (MOVED from csv.h)
typedef struct {
    const char* date_time_format;
    const char* const* enum_attribute_names;
    const size_t* enum_entry_counts;
    const int64_t* enum_values;
    const char* const* enum_labels;
    size_t enum_attribute_count;
} quiver_csv_export_options_t;

// CSV factory (MOVED from csv.h)
QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_OPTIONS_H
```

Note: current `options.h` does not include `common.h`. The merged file needs it for `QUIVER_C_API`, `size_t`, and `int64_t`. Current `csv.h` already includes `common.h`. Adding `#include "common.h"` to `options.h` is the correct fix.

### Downstream Consumer Impact

Every file that currently includes `quiver/c/csv.h` must be updated.

**Direct includes of `quiver/c/csv.h` (verified by grep):**

| File | Required Change |
|------|-----------------|
| `src/c/database_csv.cpp` | Change `#include "quiver/c/csv.h"` to `#include "quiver/c/options.h"` |
| `tests/test_c_api_database_csv.cpp` | Change `#include <quiver/c/csv.h>` to `#include <quiver/c/options.h>` |

**`include/quiver/c/database.h` currently includes both:**

```c
#include "csv.h"       // line 5 -- DELETE this include
#include "options.h"   // line 6 -- KEEP (already there)
```

After the merge, `database.h` only needs `options.h`. The `csv.h` include line is removed.

**`include/quiver/c/csv.h`:** The file is deleted.

### FFI Generator Impact

#### Julia Generator

The Julia generator (`bindings/julia/generator/generator.jl`) reads ALL `.h` files in `include/quiver/c/` automatically:

```julia
headers = [
    joinpath(include_dir, header) for
    header in readdir(include_dir) if endswith(header, ".h") && header != "platform.h"
]
```

When `csv.h` is deleted, it disappears from the list automatically. When `options.h` gains the CSV struct and factory function, the generator picks them up from `options.h`. The generated `c_api.jl` content is identical -- `quiver_csv_export_options_t` and `quiver_csv_export_options_default` appear in the output regardless of which header defines them.

**Julia generator: no configuration change required.** The regenerated `c_api.jl` will be identical in content. Regeneration is still required to pick up the new header order, but no `.toml` changes are needed.

#### Dart ffigen

The Dart ffigen configuration in `pubspec.yaml` uses an explicit entry-points list:

```yaml
ffigen:
  headers:
    entry-points:
      - '../../include/quiver/c/common.h'
      - '../../include/quiver/c/database.h'
      - '../../include/quiver/c/element.h'
      - '../../include/quiver/c/lua_runner.h'
    include-directives:
      - '../../include/quiver/c/common.h'
      - '../../include/quiver/c/database.h'
      - '../../include/quiver/c/element.h'
      - '../../include/quiver/c/lua_runner.h'
```

`csv.h` is NOT listed as an entry-point or include-directive. It was picked up transitively through `database.h` which included `csv.h`. After the merge, `database.h` includes `options.h` instead, and `options.h` now contains the CSV types. The ffigen tool still sees `quiver_csv_export_options_t` transitively through `database.h -> options.h`.

**Dart ffigen: no pubspec.yaml changes required.** Regeneration required (same as Julia). Generated `bindings.dart` content will be functionally identical.

### Julia Binding Code

`bindings/julia/src/database_csv.jl` references `C.quiver_csv_export_options_t` and `C.quiver_csv_export_options_default()` -- both defined in the `C` module from `c_api.jl`. Since the generated `c_api.jl` content does not change (same type, same function, just sourced from `options.h` instead of `csv.h`), the Julia binding file does not change.

### Dart Binding Code

`bindings/dart/lib/src/database_csv.dart` uses `quiver_csv_export_options_t` from the generated `bindings.dart`. Same situation as Julia -- the generated type definition is identical regardless of which header it came from.

### C++ Side (include/quiver/csv.h)

The C++ public header `include/quiver/csv.h` (containing `CSVExportOptions` struct) is **not involved** in the consolidation. The merge is C API (`include/quiver/c/`) only. The C++ header is separate and stays as-is.

### Files Changed by Header Consolidation

| File | Change |
|------|--------|
| `include/quiver/c/options.h` | Add `#include "common.h"`; append `quiver_csv_export_options_t` struct + `quiver_csv_export_options_default()` declaration |
| `include/quiver/c/csv.h` | **DELETE** |
| `include/quiver/c/database.h` | Remove `#include "csv.h"` line (already includes `options.h`) |
| `src/c/database_csv.cpp` | `#include "quiver/c/csv.h"` -> `#include "quiver/c/options.h"` |
| `tests/test_c_api_database_csv.cpp` | `#include <quiver/c/csv.h>` -> `#include <quiver/c/options.h>` |
| `bindings/julia/src/c_api.jl` | Regenerate (content identical, source header changes) |
| `bindings/dart/lib/src/ffi/bindings.dart` | Regenerate (content identical, source header changes) |

---

## Component Boundaries: New vs Modified vs Deleted

### New Components

| Component | File | Purpose |
|-----------|------|---------|
| csv-parser dependency | `cmake/Dependencies.cmake` | FetchContent declaration for vincentlaucsb/csv-parser |

### Modified Components

| Component | File | Change Type |
|-----------|------|-------------|
| C++ CSV writer | `src/database_csv.cpp` | Replace 2 functions with library calls; add `#include "csv.hpp"` |
| C API options header | `include/quiver/c/options.h` | Absorb CSV struct + factory from csv.h; add common.h include |
| C API database header | `include/quiver/c/database.h` | Remove `#include "csv.h"` |
| C API CSV impl | `src/c/database_csv.cpp` | Update include path |
| C API CSV test | `tests/test_c_api_database_csv.cpp` | Update include path |
| Core library build | `src/CMakeLists.txt` | Add csv to PRIVATE link libraries |
| Julia generated bindings | `bindings/julia/src/c_api.jl` | Regenerate (no logic change) |
| Dart generated bindings | `bindings/dart/lib/src/ffi/bindings.dart` | Regenerate (no logic change) |

### Deleted Components

| Component | File | Reason |
|-----------|------|--------|
| C API CSV header | `include/quiver/c/csv.h` | Content merged into options.h |

### Unchanged Components

Everything else is unaffected:

- `include/quiver/csv.h` (C++ CSVExportOptions)
- `include/quiver/database.h` (Database class declaration)
- `include/quiver/c/common.h`, `element.h`, `lua_runner.h`
- `src/c/database_csv.cpp` logic (only include path changes)
- `src/database_csv.cpp` non-writer logic
- `src/lua_runner.cpp` (Lua binds to C++ directly, no C API headers)
- `bindings/julia/src/database_csv.jl` (no logic change)
- `bindings/dart/lib/src/database_csv.dart` (no logic change)
- All C++ core tests (`test_database_csv.cpp` and others)
- All Julia and Dart binding test files
- All Lua tests in `test_lua_runner.cpp`
- `tests/schemas/` -- no schema changes

---

## Data Flow

### CSV Export Path (After Both Changes)

```
caller -> Database::export_csv(collection, group, path, options)
    |
    v
src/database_csv.cpp:
    value_to_csv_string() -- unchanged, converts Value variant to string
    std::ofstream file(path, std::ios::binary)  -- unchanged
    auto writer = csv::make_csv_writer(file)    -- NEW (was: manual write_csv_row)
    writer << header_vector                      -- NEW (was: write_csv_row + csv_escape)
    writer << row_vector                         -- NEW (was: write_csv_row + csv_escape)
    |
    v
csv-parser library handles RFC 4180 escaping internally
```

### Header Include Chain (After Consolidation)

```
tests/test_c_api_database_csv.cpp
    #include <quiver/c/options.h>      (was: csv.h)
        #include "common.h"
    #include <quiver/c/database.h>
        #include "common.h"
        #include "options.h"           (csv.h include removed; options.h was already there)

src/c/database_csv.cpp
    #include "quiver/c/options.h"      (was: quiver/c/csv.h)
    -- quiver_csv_export_options_t found in options.h

Julia FFI generator:
    reads all *.h in include/quiver/c/ except platform.h
    csv.h gone -> not in list
    options.h now has csv struct -> picked up automatically

Dart ffigen:
    entry-points: database.h, element.h, lua_runner.h, common.h
    database.h includes options.h (not csv.h anymore)
    options.h has csv struct -> picked up transitively
```

---

## Build Order

The two changes are fully independent. Either order works. However, since library integration touches `src/CMakeLists.txt` and header consolidation does not, the cleaner sequence is:

**Step 1 -- Library integration (self-contained, no header changes):**
1. Add csv-parser to `cmake/Dependencies.cmake`
2. Link `csv` in `src/CMakeLists.txt`
3. Replace `csv_escape()` + `write_csv_row()` in `src/database_csv.cpp` with `make_csv_writer()`
4. Build and verify all tests pass (C++ core tests, especially `test_database_csv.cpp`)

**Step 2 -- Header consolidation (self-contained, no build system changes):**
1. Merge CSV content into `include/quiver/c/options.h` (add `#include "common.h"`, append struct + declaration)
2. Delete `include/quiver/c/csv.h`
3. Remove `#include "csv.h"` from `include/quiver/c/database.h`
4. Update `src/c/database_csv.cpp` include
5. Update `tests/test_c_api_database_csv.cpp` include
6. Build and verify all tests pass (especially C API tests)

**Step 3 -- FFI regeneration (required after header consolidation):**
1. Run `bindings/julia/generator/generator.bat` -> regenerates `c_api.jl`
2. Run `bindings/dart/generator/generator.bat` -> regenerates `bindings.dart`
3. Run Julia and Dart tests to confirm generated bindings are identical in behavior

**Dependency graph:**

```
Step 1 (library)  ---independent of--->  Step 2 (headers)
                                              |
                                              v
                                         Step 3 (FFI regen)
```

Steps 1 and 2 can be done in either order or in the same commit. Step 3 must come after Step 2.

---

## Architectural Patterns

### Pattern 1: Library Wraps the Stream, Not the File

**What:** Pass the already-opened `std::ofstream` to `csv::make_csv_writer()`, not a file path.
**When to use:** Always -- binary mode must be set on the stream before wrapping.
**Trade-off:** One extra line (open stream, then wrap). But binary mode is required for correct cross-platform LF output, and the library cannot be told to open in binary mode itself.

```cpp
std::ofstream file(path, std::ios::binary);  // open with binary mode
if (!file.is_open()) { throw ...; }
auto writer = csv::make_csv_writer(file);    // wrap the open stream
writer << header_fields;
for (const auto& row : data_result) {
    // ... build row fields ...
    writer << row_fields;
}
```

### Pattern 2: Single-Header Content Consolidation

**What:** When merging `csv.h` into `options.h`, keep the content in the same section order: enums, then structs, then function declarations.
**When to use:** Any time a small standalone header is merged into a companion header.
**Trade-off:** The merged header is longer but has fewer files to manage. The key invariant: `options.h` becomes the canonical location for all C API option types.

### Pattern 3: Include Guard Update on Merge

**What:** The merged `options.h` keeps its own `#ifndef QUIVER_C_OPTIONS_H` guard. No new guard needed.
**When to use:** When absorbing content from another file -- do not replicate the source file's guard.
**Consequence:** Anyone who had `#include <quiver/c/csv.h>` and `#include <quiver/c/options.h>` in the same TU now just needs `#include <quiver/c/options.h>`. No double-inclusion risk.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Creating a Compatibility Shim for csv.h

**What people do:** Keep `csv.h` as a thin header that includes `options.h` with a deprecation comment.
**Why it's wrong:** The project philosophy is "Delete unused code, do not deprecate." There are exactly two direct includes to update (`src/c/database_csv.cpp` and `tests/test_c_api_database_csv.cpp`). A shim adds permanent technical debt for two trivial find-and-replace edits.
**Do this instead:** Delete `csv.h`, update the two files, regenerate FFI bindings.

### Anti-Pattern 2: Replacing the ofstream with a Library-Managed File

**What people do:** Use the library's file-path-based API (if it exists) to let the library open the file.
**Why it's wrong:** The existing code opens in binary mode (`std::ios::binary`) to prevent CRLF conversion on Windows. If the library opens the file internally, binary mode cannot be guaranteed. The current approach (open stream, pass to writer) is correct and must be preserved.
**Do this instead:** Keep `std::ofstream file(path, std::ios::binary)`, then `auto writer = csv::make_csv_writer(file)`.

### Anti-Pattern 3: Merging csv.h into database.h Instead of options.h

**What people do:** Put CSV options in the main database header since that's where the function is declared.
**Why it's wrong:** `database.h` is already large (~490 lines). More importantly, `quiver_database_options_t` lives in `options.h` as a structural peer. CSV export options are per-call options of the same conceptual kind. `options.h` is the correct home.
**Do this instead:** Merge into `options.h`, keep `database.h` focused on function signatures.

### Anti-Pattern 4: Linking csv-parser as PUBLIC

**What people do:** Add `csv` to `target_link_libraries(quiver PUBLIC ...)`.
**Why it's wrong:** csv-parser is an implementation detail of `src/database_csv.cpp`. No public header includes `csv.hpp`. Marking it PUBLIC would propagate the dependency to all consumers of `libquiver`.
**Do this instead:** `target_link_libraries(quiver PRIVATE ... csv)`.

---

## Integration Points Summary

| Integration Point | Change 1 (Library) | Change 2 (Header Merge) |
|------------------|-------------------|------------------------|
| `src/database_csv.cpp` | Replace 2 functions, add #include | No change |
| `cmake/Dependencies.cmake` | Add FetchContent | No change |
| `src/CMakeLists.txt` | Add csv to PRIVATE links | No change |
| `include/quiver/c/options.h` | No change | Add common.h include + CSV content |
| `include/quiver/c/csv.h` | No change | Delete file |
| `include/quiver/c/database.h` | No change | Remove csv.h include |
| `src/c/database_csv.cpp` | No change | Update include path |
| `tests/test_c_api_database_csv.cpp` | No change | Update include path |
| `bindings/julia/src/c_api.jl` | No change | Regenerate (same content) |
| `bindings/dart/lib/src/ffi/bindings.dart` | No change | Regenerate (same content) |
| All other files | No change | No change |

**Total files touched:** 10 (across both changes combined)

---

## Scalability Considerations

Not applicable to this milestone -- v0.5 is a refactor with no behavior change. Library selection (csv-parser) is appropriate for any scale; writing is stream-based and does not buffer entire file in memory.

---

## Sources

- Codebase analysis: `src/database_csv.cpp`, `src/c/database_csv.cpp`, `include/quiver/c/csv.h`, `include/quiver/c/options.h`, `include/quiver/c/database.h`, `src/CMakeLists.txt`, `cmake/Dependencies.cmake`
- FFI generator configs: `bindings/julia/generator/generator.jl`, `bindings/julia/generator/generator.toml`, `bindings/dart/pubspec.yaml`
- Binding implementations: `bindings/julia/src/database_csv.jl`, `bindings/dart/lib/src/database_csv.dart`
- Test files: `tests/test_c_api_database_csv.cpp`
- csv-parser library: [vincentlaucsb/csv-parser](https://github.com/vincentlaucsb/csv-parser) -- write API verified from [test_write_csv.cpp](https://github.com/vincentlaucsb/csv-parser/blob/master/tests/test_write_csv.cpp) and [CMake FetchContent wiki](https://github.com/vincentlaucsb/csv-parser/wiki/Example:-Using-csv%E2%80%90parser-with-CMake-and-FetchContent)

---

*Architecture research for: v0.5 CSV Refactor (library integration + header consolidation)*
*Researched: 2026-02-22*
