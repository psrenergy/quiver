# Coding Conventions

**Analysis Date:** 2026-02-26

## Naming Patterns

**Files:**
- C++ source: `snake_case.cpp` / `snake_case.h` (e.g., `database_csv_export.cpp`, `database_helpers.h`)
- C++ headers mirror directory: `include/quiver/database.h`, `include/quiver/c/database.h`
- Test files: `test_{layer}_{topic}.cpp` for C++ tests (e.g., `test_database_create.cpp`, `test_c_api_database_read.cpp`)
- Julia source: `snake_case.jl` mirroring C++ topic split (e.g., `database_create.jl`, `database_transaction.jl`)
- Dart test files: `{topic}_test.dart` (e.g., `database_create_test.dart`)
- Python test files: `test_{topic}.py` (e.g., `test_database_create.py`)

**C++ Classes and Structs:**
- `CamelCase` for all class, struct, and enum names: `Database`, `Element`, `ScalarMetadata`, `GroupMetadata`, `DatabaseOptions`
- `CamelCase` for enum constants in C++; `SCREAMING_SNAKE_CASE` for C macros: `QUIVER_LOG_DEBUG`, `QUIVER_OK`
- Nested private structs use `CamelCase`: `Database::Impl`, `Impl::TransactionGuard`

**C++ Functions and Methods:**
- `lower_case` (snake_case) for all functions, methods, variables, members, parameters
- Public `Database` methods follow `verb_[category_]type[_by_id]` pattern:
  - Verbs: `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`
  - `_by_id` suffix only when both "all elements" and "single element" variants exist
  - Plural name when return is a vector: `read_scalar_integers` returns `vector<int64_t>` (all elements)
  - Singular name when return is optional: `read_scalar_integer_by_id` returns `optional<int64_t>`
- Private members have `_` suffix: `impl_`
- Namespaces: `lower_case` — `quiver`, `quiver::test`

**C API:**
- All functions prefixed `quiver_` + entity + `_` + operation: `quiver_database_create_element`, `quiver_element_set_string`
- Types suffixed `_t`: `quiver_database_t`, `quiver_element_t`, `quiver_error_t`, `quiver_data_type_t`
- Constants: `SCREAMING_SNAKE_CASE`: `QUIVER_OK`, `QUIVER_ERROR`, `QUIVER_LOG_OFF`
- Free functions co-named with allocators: `quiver_database_free_integer_array`, `quiver_database_free_group_metadata`

**Cross-Language Naming:**
- Julia: same `snake_case` as C++; mutating operations get `!` suffix (`create_element!`, `commit!`, `rollback!`)
- Dart: `camelCase` methods (`createElement`, `readScalarIntegers`); named constructors `CamelCase.camelCase` (`Database.fromSchema`)
- Python: same `snake_case` as C++; factory methods as `@staticmethod` (`Database.from_schema`)

## Code Style

**C++ Formatting (enforced by `.clang-format`):**
- Style: LLVM-based, C++20 standard
- Indent: 4 spaces, no tabs
- Column limit: 120 characters
- Line endings: LF
- Pointer alignment: left (`int64_t* out_values`, not `int64_t *out_values`)
- Brace style: Attach (opening brace on same line as statement)
- Template declarations: always break before each `template<...>` (`AlwaysBreakTemplateDeclarations: Yes`)
- Includes: sorted and regrouped (`SortIncludes: true`, `IncludeBlocks: Regroup`)
- Trailing comments: 2 spaces before `//`
- No short if-statements, loops, or blocks on single line (except inline class functions)
- No bin-packed arguments or parameters

**Linting (`.clang-tidy`):**
- Checks enabled: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`
- Notable exceptions disabled: `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `modernize-use-ranges`, `modernize-use-nodiscard`
- Header filter: only `include/quiver/` and `src/` are linted

**Python (`bindings/python/ruff.toml`):**
- Line length: 120 characters, indent: 4 spaces
- Target: Python 3.13
- Quote style: double quotes
- Line endings: LF
- Imports: isort enforced (`ruff lint select = ["I"]`), `combine-as-imports = true`

## Import Organization

**C++ includes order (clang-format `IncludeBlocks: Regroup`, `SortIncludes: true`):**

1. Implementation's own paired header (first in `.cpp` files)
2. Internal project headers (quoted, e.g., `"internal.h"`, `"database_helpers.h"`)
3. Public library headers (angle brackets, e.g., `<quiver/database.h>`, `<quiver/c/database.h>`)
4. Standard library headers (angle brackets, e.g., `<string>`, `<vector>`)

Example from `src/c/database.cpp`:
```cpp
#include "quiver/c/database.h"

#include "internal.h"

#include <new>
#include <string>
```

Example from `src/c/database_read.cpp`:
```cpp
#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"
```

**Python: `from __future__ import annotations` at top of all files.**

## Error Handling

**C++ layer — exactly three message patterns:**

Pattern 1 — Precondition failure:
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error("Cannot update_element: element must have at least one scalar or array attribute");
```

Pattern 2 — Not found:
```cpp
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");
throw std::runtime_error("Schema file not found: path/to/schema.sql");
throw std::runtime_error("Time series table not found: Items_time_series_data");
```

Pattern 3 — Operation failure:
```cpp
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
throw std::runtime_error("Failed to execute statement: " + sqlite3_errmsg(db));
```

No ad-hoc formats. Binding layers never craft their own error messages — they only retrieve and surface C++ exceptions.

**C API layer — `QUIVER_REQUIRE` + try/catch:**
```cpp
quiver_error_t quiver_some_function(quiver_database_t* db, const char* collection) {
    QUIVER_REQUIRE(db, collection);   // macro in src/c/internal.h

    try {
        db->db.some_operation(collection);
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

`QUIVER_REQUIRE` (defined in `src/c/internal.h`): validates 1–9 pointer arguments, auto-generates "Null argument: {arg_name}" using macro stringification.

**Julia binding — `check()` helper in `bindings/julia/src/exceptions.jl`:**
```julia
function check(err)
    if err != C.QUIVER_OK
        detail = unsafe_string(C.quiver_get_last_error())
        throw(DatabaseException(detail))
    end
end
```
Every C API call is wrapped: `check(C.quiver_database_create_element(...))`.

**Python binding:**
- Raises `QuiverError` wrapping the C API error string
- Operations after `close()` raise `QuiverError("closed")` — checked before every delegation
- Context manager protocol: `__enter__` returns self, `__exit__` calls `close()`

**Dart binding:**
- Throws `DatabaseException` (wraps C API error string)
- Uses manual try/finally for resource cleanup (Dart has no RAII)

## Logging

**Framework:** spdlog with structured format-string logging.

**Levels used:**
```cpp
spdlog::debug("Opening database: {}", path);
spdlog::info("Applied migration: {}", version);
spdlog::warn("...");
spdlog::error("Failed to execute query: {}", sqlite3_errmsg(db));
```

Tests suppress all output by passing `QUIVER_LOG_OFF`:
```cpp
// C++ test pattern
auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"),
    {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

The `quiver::test::quiet_options()` helper in `tests/test_utils.h` returns `QUIVER_LOG_OFF` options for C API tests.

## Comments

**Section separators in C++ test and source files:**
```cpp
// ============================================================================
// Section name
// ============================================================================
```
Used to group related tests or logical sections within a single `.cpp` file. Not used in headers.

**Inline comments:** Only for non-obvious logic. Obvious code is not commented.

**No JSDoc/Doxygen** on C++ public headers. The `check()` helper in Julia uses a Julia docstring. Python conftest fixtures use Google-style one-line docstrings.

## Function Design

**C++ public API:**
- Write operations return `void` (update, delete) or `int64_t` (create, returning new ID); they throw on failure
- Read operations return value types: `std::vector<T>`, `std::optional<T>`, `std::map<...>`
- No output parameters — only the C API uses output parameters

**C API:**
- Every function returns `quiver_error_t` (binary: `QUIVER_OK` or `QUIVER_ERROR`)
- Results via output parameters: `T* out_value` for scalars, `T** out_array` + `size_t* out_count` for arrays
- Exceptions: utility functions (`quiver_get_last_error`, `quiver_version`, `quiver_database_options_default`) return directly

**Pimpl pattern (only where justified):**
```cpp
// database.h — Pimpl hides sqlite3 header dependency
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```
Plain value types (`Element`, `ScalarMetadata`, `GroupMetadata`, `CSVOptions`) use Rule of Zero — direct members, no Pimpl, compiler-generated copy/move/destructor.

**Move semantics for resource types:**
```cpp
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&&) noexcept;
Database& operator=(Database&&) noexcept;
```

**Factory methods:**
```cpp
static Database from_schema(const std::string& db_path, const std::string& schema_path,
                             const DatabaseOptions& options = default_database_options());
static Database from_migrations(const std::string& db_path, const std::string& migrations_path,
                                const DatabaseOptions& options = default_database_options());
```

## Module Design

**C++ — translation unit co-location rule:**
Each C API source file owns its alloc/free pairs for the data it produces:
- `src/c/database_read.cpp` owns `quiver_database_free_integer_array`, `quiver_database_free_string_array`
- `src/c/database_metadata.cpp` owns `quiver_database_free_scalar_metadata`, `quiver_database_free_group_metadata`
- `src/c/database_time_series.cpp` owns `quiver_database_free_time_series_data`

Shared helpers are extracted to headers:
- `src/c/database_helpers.h`: marshaling templates (`read_scalars_impl`, `read_vectors_impl`, `free_vectors_impl`), `strdup_safe`, metadata converters (`convert_scalar_to_c`, `convert_group_to_c`)
- `src/c/internal.h`: shared opaque structs (`quiver_database`, `quiver_element`), `QUIVER_REQUIRE` macro

**Julia — one file per functional area:**
`bindings/julia/src/` contains `database_create.jl`, `database_read.jl`, `database_transaction.jl`, etc., all included by `bindings/julia/src/Quiver.jl`.

**String handling in C API (always `new`/`delete[]`, never `malloc`/`free`):**
```cpp
// strdup_safe in src/c/database_helpers.h
inline char* strdup_safe(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}
```
Callers free with `delete[]`. Matching `quiver_*_free_*` functions always provided for every C API allocation.

---

*Convention analysis: 2026-02-26*
