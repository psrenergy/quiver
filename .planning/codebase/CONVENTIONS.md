# Coding Conventions

**Analysis Date:** 2026-02-27

## Naming Patterns

**Files (C++ source):**
- `snake_case` for all `.cpp` and `.h` files: `database_create.cpp`, `database_helpers.h`
- Test files: `test_<subject>.cpp` (C++ layer), `test_c_api_<subject>.cpp` (C API layer)
- Group related operations into one file per domain: `database_read.cpp`, `database_update.cpp`

**Files (bindings):**
- Julia: `test_database_create.jl` — same snake_case convention
- Dart: `database_create_test.dart` — suffix `_test` per Dart convention
- Python: `test_database_create.py` — prefixed `test_` per pytest convention

**Classes and Structs:**
- `CamelCase` for all class and struct names: `Database`, `Element`, `ScalarMetadata`, `GroupMetadata`
- Public C++ classes use `QUIVER_API` macro: `class QUIVER_API Database`
- Internal C API structs prefixed with `quiver_`: `quiver_database`, `quiver_element`

**Functions and Methods (C++):**
- `lower_case` (snake_case) for all function and method names
- Public `Database` methods follow `verb_[category_]type[_by_id]` pattern:
  - Verbs: `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`
  - `_by_id` suffix only when both "all elements" and "single element" variants exist
  - Singular vs plural matches return cardinality: `read_scalar_integers` returns `vector`, `read_scalar_integer_by_id` returns `optional`
- Examples: `create_element`, `read_vector_floats_by_id`, `get_scalar_metadata`, `list_time_series_groups`

**C API Functions:**
- Prefix `quiver_database_` for database operations: `quiver_database_create_element`
- Prefix `quiver_element_` for element operations: `quiver_element_set_string`
- Free functions follow: `quiver_database_free_integer_array`, `quiver_element_free_string`
- Utility functions (no entity prefix): `quiver_get_last_error`, `quiver_version`

**Variables and Parameters:**
- `lower_case` (snake_case): `out_db`, `collection`, `attribute`, `out_count`
- Output parameters prefixed `out_`: `out_db`, `out_values`, `out_count`, `out_id`
- Private class members suffixed `_`: `impl_`, `scalars_`, `arrays_`

**Macros:**
- `UPPER_CASE` with `QUIVER_` prefix: `QUIVER_REQUIRE`, `QUIVER_OK`, `QUIVER_ERROR`, `QUIVER_C_API`, `QUIVER_API`

**Enums:**
- `CamelCase` for C++ enum type and constant names: `DataType::Integer`, `DataType::Real`
- C API enums use `UPPER_CASE` with prefix: `QUIVER_DATA_TYPE_INTEGER`, `QUIVER_LOG_OFF`

## Code Style

**Formatter:** `clang-format` with LLVM base style (see `.clang-format`)

**Key settings:**
- Indent: 4 spaces, no tabs
- Line limit: 120 characters
- LF line endings
- `BinPackArguments: false` — all arguments on separate lines when they don't fit
- `BreakBeforeBraces: Attach` — opening brace on same line
- `PointerAlignment: Left` — `int64_t* out_values` not `int64_t *out_values`
- `SortIncludes: true` — includes auto-sorted by clang-format
- `AllowShortFunctionsOnASingleLine: Inline` — inline short functions permitted

**Static analyzer:** `clang-tidy` (see `.clang-tidy`):
- Enabled: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`
- Notable exclusions: `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `modernize-use-ranges`
- Identifier naming enforced: classes/structs = `CamelCase`, functions/methods/variables = `lower_case`, private members = `lower_case_` with suffix

## Import Organization

**C++ includes order** (enforced by clang-format):

1. Own header (for `.cpp` files matching the translation unit)
2. Internal headers (quoted)
3. Third-party and standard library headers (angle brackets)

Example from `src/c/database.cpp`:
```cpp
#include "quiver/c/database.h"

#include "internal.h"

#include <new>
#include <string>
```

Example from `tests/test_database_create.cpp`:
```cpp
#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
```

## Error Handling

### C++ Layer
All `throw std::runtime_error(...)` use exactly three message patterns — no ad-hoc formats:

**Pattern 1 — Precondition failure:** `"Cannot {operation}: {reason}"`
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error("Cannot begin_transaction: transaction already active");
throw std::runtime_error("Cannot commit: no active transaction");
throw std::runtime_error("Cannot rollback: no active transaction");
```

**Pattern 2 — Not found:** `"{Entity} not found: {identifier}"`
```cpp
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");
throw std::runtime_error("Schema file not found: path/to/schema.sql");
throw std::runtime_error("Time series table not found: Items_time_series_data");
```

**Pattern 3 — Operation failure:** `"Failed to {operation}: {reason}"`
```cpp
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
throw std::runtime_error("Failed to execute statement: " + sqlite3_errmsg(db));
```

### C API Layer
All C API functions use try-catch with `quiver_set_last_error()` and binary return codes:

```cpp
quiver_error_t quiver_some_function(quiver_database_t* db) {
    QUIVER_REQUIRE(db);  // null-check; sets error and returns QUIVER_ERROR if null

    try {
        // operation...
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

`bad_alloc` is caught separately only in factory functions (`quiver_database_open`, `quiver_database_from_schema`, `quiver_database_from_migrations`). Other functions catch only `std::exception`.

### `QUIVER_REQUIRE` macro
Validates pointer arguments at function entry, supports 1–9 arguments. Auto-generates error messages from argument names via stringification:
```cpp
QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);
// On null db: sets "Null argument: db" and returns QUIVER_ERROR
```

Defined in `src/c/internal.h`.

## Logging

**Framework:** `spdlog`

**Levels:** `debug`, `info`, `warn`, `error`

**Format:** `{}` placeholder style (fmt/spdlog):
```cpp
spdlog::debug("Opening database: {}", path);
spdlog::error("Failed to execute query: {}", sqlite3_errmsg(db));
```

**In tests:** Always suppress logging via `{.read_only = 0, .console_level = QUIVER_LOG_OFF}`

## Comments

**Section headers** in test files and long implementation files use visual separators:
```cpp
// ============================================================================
// Vector/Set edge case tests
// ============================================================================
```

**Inline comments** appear two spaces before `//`:
```cpp
(void)sizes;  // unused for numeric types
```

**No JSDoc/Doxygen.** Public API documented in `CLAUDE.md`, not inline.

**Comments in tests:** Brief comment above test body explaining what is being set up or why:
```cpp
// Configuration required first
quiver::Element config;
config.set("label", std::string("Test Config"));
```

## Pimpl vs Value Types

**Pimpl** used only when hiding private dependencies (sqlite3, lua headers):
```cpp
// include/quiver/database.h — Pimpl because it hides sqlite3*
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

**Plain value types** (Rule of Zero) for classes with no private dependencies:
```cpp
// include/quiver/element.h — plain members, compiler-generated copy/move/dtor
class Element {
    std::map<std::string, Value> scalars_;
    std::map<std::string, std::vector<Value>> arrays_;
};
```

Types using Rule of Zero: `Element`, `Row`, `Migration`, `Migrations`, `GroupMetadata`, `ScalarMetadata`, `CSVOptions`

## Move Semantics

Resource types delete copy, explicitly define move:
```cpp
// include/quiver/database.h
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&&) noexcept;
Database& operator=(Database&&) noexcept;
```

## Module Design

**C++ library namespace:** `quiver::` for all public types
**C API:** All symbols prefixed `quiver_` with no namespace (C-compatible `extern "C"`)
**No barrel/umbrella includes** — each header is self-contained
**Exports:** `QUIVER_API` macro on all public C++ class/struct declarations; `QUIVER_C_API` on all C API function declarations

## Memory Management (C API)

**Pattern:** `new`/`delete[]` with matching `quiver_{entity}_free_*` functions.

**Alloc/free co-location:** Every allocation function and its corresponding free function live in the same translation unit:
- Read alloc/free pairs: `src/c/database_read.cpp`
- Metadata alloc/free pairs: `src/c/database_metadata.cpp`
- Time series alloc/free pairs: `src/c/database_time_series.cpp`

**String helper:** `strdup_safe()` in `src/c/database_helpers.h` — always null-terminates:
```cpp
static char* strdup_safe(const std::string& str) {
    char* result = new char[str.size() + 1];
    std::memcpy(result, str.c_str(), str.size() + 1);
    return result;
}
```

## Cross-Layer Naming Rules

| Layer | Convention | Example |
|-------|-----------|---------|
| C++ | `snake_case` | `read_scalar_integers` |
| C API | `quiver_database_` prefix | `quiver_database_read_scalar_integers` |
| Julia | Same as C++, `!` suffix if mutating | `create_element!`, `read_scalar_integers` |
| Dart | `camelCase`, named constructors for factories | `readScalarIntegers`, `Database.fromSchema()` |
| Python | Same as C++, `**kwargs` for create/update | `create_element("Coll", label="x")` |
| Lua | Same as C++ exactly | `read_scalar_integers` |

The rules are mechanical — given any C++ method name, derive the equivalent in any layer without a lookup table.

---

*Convention analysis: 2026-02-27*
