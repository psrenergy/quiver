# Coding Conventions

**Analysis Date:** 2026-03-08

## Naming Patterns

### C++ Core

**Files:**
- `snake_case.cpp` / `snake_case.h` — all C++ source and header files
- Subsystem grouping: `database_csv_export.cpp`, `database_read.cpp`, `database_metadata.cpp`
- Internal headers (no install): `internal.h`, `database_helpers.h`, `database_options.h`

**Classes and Structs:**
- `CamelCase` — classes and structs: `Database`, `BlobMetadata`, `ScalarMetadata`, `GroupMetadata`
- `CamelCase` — enums: `LogLevel`, `TimeFrequency`
- `CamelCase` — enum constants: `TimeFrequency::Yearly`, `LogLevel::Off`

**Functions and Methods:**
- `lower_case` (snake_case) — all functions and methods: `read_scalar_integers`, `create_element`, `from_schema`
- Public `Database` methods follow `verb_[category_]type[_by_id]`:
  - Verbs: `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`
  - `_by_id` suffix only when both "all elements" and "single element" variants exist
  - Singular/plural matches cardinality: `read_scalar_integers` (vector), `read_scalar_integer_by_id` (optional)

**Variables and Members:**
- `lower_case` — variables, parameters, members
- Private members have `_` suffix: `impl_`, `ptr_`

**Namespaces:**
- `lower_case` — `quiver`, `quiver::string`, `quiver::test`

### C API

**Functions:**
- `quiver_{entity}_{method}` — e.g., `quiver_database_create_element`, `quiver_blob_open_read`
- Free functions co-located with allocation functions: `quiver_database_free_integer_array`, `quiver_blob_free_string`

**Types:**
- `quiver_{entity}_t` — opaque handle types: `quiver_database_t`, `quiver_blob_t`, `quiver_element_t`
- `quiver_{concept}_t` — value types: `quiver_database_options_t`, `quiver_blob_metadata_t`
- Error code: `quiver_error_t` with constants `QUIVER_OK = 0`, `QUIVER_ERROR = 1`

**Constants:**
- `QUIVER_LOG_OFF`, `QUIVER_DATA_TYPE_INTEGER` — all-caps with `QUIVER_` prefix

### Julia Bindings

**Functions:** Same `snake_case` as C++. Mutating operations get `!` suffix:
- `create_element!`, `update_element!`, `delete_element!`, `close!`, `commit!`, `rollback!`
- Non-mutating: `read_scalar_integers`, `get_scalar_metadata`, `query_string`

**Modules/Types:** `CamelCase` — `Database`, `BlobMetadata`, `DatabaseException`

### Dart Bindings

**Methods:** `camelCase` — `createElement`, `readScalarIntegers`, `fromSchema`
- Factory constructors: `Database.fromSchema()`, `Database.fromMigrations()`

**Classes:** `CamelCase` — `Database`, `DatabaseException`

### Python Bindings

**Methods/Functions:** Same `snake_case` as C++ — `create_element`, `read_scalar_integers`
- Static factories: `Database.from_schema()`, `Database.from_migrations()`
- `create_element` and `update_element` use `**kwargs`, not positional `Element` argument

**Classes:** `CamelCase` — `Database`, `QuiverError`

## Code Style

### C++ Formatting (`.clang-format`)
- Based on LLVM style
- `IndentWidth: 4`, `TabWidth: 4`, `UseTab: Never`
- `ColumnLimit: 120`
- `PointerAlignment: Left` — pointer on type side: `char* ptr`
- `BreakBeforeBraces: Attach` — braces on same line
- `SortIncludes: true`, `IncludeBlocks: Regroup`
- `AllowShortFunctionsOnASingleLine: Inline` — only inline class methods on one line

### C++ Linting (`.clang-tidy`)
- Enabled: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`
- Disabled: `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `modernize-use-ranges`, `performance-enum-size`
- Warnings are NOT treated as errors

### Julia Formatting (`.JuliaFormatter.toml`)
- `indent = 4`, `margin = 120`
- `always_use_return = true`, `trailing_comma = true`
- `format_docstrings = true`

### Python Formatting (`ruff.toml`)
- `line-length = 120`, `indent-width = 4`
- `quote-style = "double"`, `line-ending = "lf"`
- `isort` enabled; `from __future__ import annotations` at top of every file
- `target-version = "py313"`

### Dart Formatting (`analysis_options.yaml`)
- `page_width: 120`
- `trailing_commas: preserve`
- Based on `package:lints/recommended.yaml`

## Include Organization

### C++ Include Order (managed by clang-format `IncludeBlocks: Regroup`):
1. Module's own header (first, for `.cpp` files): `#include "quiver/blob/blob.h"`
2. Internal project headers: `#include "blob_utils.h"` / `#include "internal.h"`
3. Standard library headers: `#include <filesystem>`, `#include <string>`

### Example pattern from `src/blob/blob.cpp`:
```cpp
#include "quiver/blob/blob.h"

#include "blob_utils.h"
#include "quiver/blob/dimension.h"
#include "quiver/blob/time_constants.h"

#include <chrono>
#include <cmath>
#include <filesystem>
```

### Example pattern from `src/c/database_read.cpp`:
```cpp
#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

extern "C" {
```

## Error Handling

### C++ Layer
Three exact error message patterns — no ad-hoc formats:

```cpp
// Pattern 1: Precondition failure
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");

// Pattern 2: Not found
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");

// Pattern 3: Operation failure
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
```

### C API Layer
`QUIVER_REQUIRE` macro validates non-null pointers at entry; try/catch wraps all operations:

```cpp
quiver_error_t quiver_database_create_element(quiver_database_t* db,
                                              const char* collection,
                                              quiver_element_t* element,
                                              int64_t* out_id) {
    QUIVER_REQUIRE(db, collection, element, out_id);

    try {
        *out_id = db->db.create_element(collection, element->element);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

### Julia Layer
`check()` function in `bindings/julia/src/exceptions.jl`:
```julia
function check(err)
    if err != C.QUIVER_OK
        detail = unsafe_string(C.quiver_get_last_error())
        throw(DatabaseException(detail))
    end
end
```
Throws `DatabaseException <: Exception`.

### Dart Layer
`check()` function in `bindings/dart/lib/src/exceptions.dart`:
```dart
void check(int err) {
  if (err != 0) {
    final detail = bindings.quiver_get_last_error().cast<Utf8>().toDartString();
    throw DatabaseException(detail);
  }
}
```
Throws `DatabaseException implements Exception`.

### Python Layer
`check()` function in `bindings/python/src/quiverdb/_helpers.py`:
```python
def check(err: int) -> None:
    if err != 0:
        lib = get_lib()
        ptr = lib.quiver_get_last_error()
        detail = ffi.string(ptr).decode("utf-8") if ptr != ffi.NULL else ""
        raise QuiverError(detail or "Unknown error")
```
Raises `QuiverError(Exception)`.

## Logging

**Framework:** spdlog (`bindings/julia`: no logging; C API uses C++ layer logging)

**Usage in C++ (`src/`):**
```cpp
spdlog::debug("Opening database: {}", path);
spdlog::info("Applied migration {}", version);
spdlog::warn("Something unexpected: {}", detail);
spdlog::error("Failed to execute query: {}", sqlite3_errmsg(db));
```

**Level control:** `quiver::LogLevel` enum passed via `DatabaseOptions::console_level`. Tests always use `LogLevel::Off`.

## Comments

**When to Comment:**
- Describe non-obvious behavior, not what the code says
- Section dividers in long test files use `// =====...=====` banners
- Comments explain "why" for edge cases: `// Configuration required first`

**No JSDoc/Doxygen:** C++ headers have no doc comments. Python uses short one-line docstrings on public methods only.

## Struct/Class Design

### Pimpl Pattern
Used only for classes hiding external dependencies:
```cpp
// database.h (public header - no SQLite exposure)
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```
`Database`, `Blob`, `BlobCSV`, `LuaRunner` use Pimpl.

### Rule of Zero (Plain Value Types)
Classes with no private dependencies are plain value types — no Pimpl, no custom destructor:
- `Element`, `BlobMetadata`, `Dimension`, `TimeProperties`, `ScalarMetadata`, `GroupMetadata`

### Move-only Resource Types
```cpp
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&&) noexcept = default;
Database& operator=(Database&&) noexcept = default;
```

## Module Design

### C++ Exports
- `QUIVER_API` macro on public-facing class definitions (e.g., `class QUIVER_API Database`)
- `QUIVER_C_API` on all C API functions
- Internal implementation in `src/` is never exported

### Translation Unit Organization
- Alloc and free functions co-located in the same `.cpp` file
- Read alloc/free: `src/c/database_read.cpp`
- Metadata alloc/free: `src/c/database_metadata.cpp`
- Time series alloc/free: `src/c/database_time_series.cpp`

### Binding File Structure
Julia: one `.jl` file per topic (`database_read.jl`, `database_create.jl`)
Dart: one `.dart` file per topic, composed via `part` directives from `database.dart`
Python: one `.py` file per topic, composed via multiple inheritance from `Database`

---

*Convention analysis: 2026-03-08*
