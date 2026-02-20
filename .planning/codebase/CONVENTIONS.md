# Coding Conventions

**Analysis Date:** 2026-02-20

## Naming Patterns

**Files:**
- C++ source: `snake_case.cpp` (e.g., `database_create.cpp`, `database_time_series.cpp`)
- C++ headers: `snake_case.h` (e.g., `database_impl.h`, `attribute_metadata.h`)
- Public headers: `include/quiver/*.h` and `include/quiver/c/*.h`
- Internal headers: co-located with source in `src/` or `src/c/`
- Test files: `test_{category}.cpp` for C++ tests, `test_c_api_{category}.cpp` for C API tests

**Classes:**
- `CamelCase` (e.g., `Database`, `Element`, `LuaRunner`, `SchemaValidator`, `TransactionGuard`)
- Enforced by clang-tidy `readability-identifier-naming.ClassCase: CamelCase`

**Structs:**
- `CamelCase` same as classes (e.g., `ScalarMetadata`, `GroupMetadata`, `DatabaseOptions`)
- Plain value types use Rule of Zero — no custom destructor/copy/move

**Functions and Methods:**
- `lower_case` (snake_case) (e.g., `create_element`, `read_scalar_integers`, `list_vector_groups`)
- Enforced by clang-tidy `readability-identifier-naming.FunctionCase: lower_case`

**Variables and Members:**
- `lower_case` (snake_case)
- Private member fields use `trailing_underscore_` suffix (e.g., `impl_`, `scalars_`, `arrays_`, `committed_`)
- Enforced by clang-tidy `readability-identifier-naming.PrivateMemberSuffix: '_'`

**Enums:**
- Enum types: `CamelCase` (e.g., `DataType`, `GroupTableType`)
- Enum constants: `CamelCase` (e.g., `DataType::Integer`, `DataType::Real`)
- C API enums: `ALL_CAPS` with prefix (e.g., `QUIVER_LOG_OFF`, `QUIVER_DATA_TYPE_INTEGER`)

**Namespaces:**
- `lower_case` (e.g., `quiver`, `quiver::internal`, `quiver::test`)

**C API Symbols:**
- Functions: `quiver_{entity}_{operation}` (e.g., `quiver_database_create_element`, `quiver_element_set_string`)
- Types: `quiver_{name}_t` (e.g., `quiver_database_t`, `quiver_element_t`, `quiver_error_t`)
- Constants: `QUIVER_{CATEGORY}_{NAME}` (e.g., `QUIVER_OK`, `QUIVER_ERROR`, `QUIVER_LOG_OFF`)
- Macros: `QUIVER_REQUIRE`, `QUIVER_C_API`

**Database Method Naming Pattern:**
`verb_[category_]type[_by_id]`
- Verbs: `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`
- `_by_id` suffix only when both "all elements" and "single element" variants exist
- Singular vs plural matches return cardinality (`read_scalar_integers` → vector, `read_scalar_integer_by_id` → optional)

## Code Style

**Formatting Tool:** clang-format (config: `/.clang-format`)

**Key Settings:**
- Based on LLVM style
- Indent width: 4 spaces, no tabs
- Column limit: 120 characters
- Line endings: LF (Unix)
- Braces: Attach style (opening brace on same line)
- Pointer alignment: Left (`int64_t* ptr`)
- Short functions: allowed inline only (`AllowShortFunctionsOnASingleLine: Inline`)
- Includes: sorted and regrouped (`SortIncludes: true`, `IncludeBlocks: Regroup`)
- Template declarations: always broken (`AlwaysBreakTemplateDeclarations: Yes`)
- Parameters: never bin-packed (each on its own line if they don't fit)

**Static Analysis:** clang-tidy (config: `/.clang-tidy`)
- Active check families: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`
- Key disabled checks: `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `modernize-use-ranges`

## Import Organization

clang-format `IncludeBlocks: Regroup` with `SortIncludes: true` enforces automatic grouping.

**Observed order in source files:**
1. Own header (e.g., `"database_impl.h"`, `"internal.h"`)
2. Project headers using angle brackets (e.g., `<quiver/database.h>`, `<quiver/element.h>`)
3. Third-party headers (e.g., `<spdlog/spdlog.h>`, `<sqlite3.h>`)
4. Standard library headers (e.g., `<string>`, `<vector>`, `<memory>`, `<optional>`)

Example from `src/database.cpp`:
```cpp
#include "database_impl.h"
#include "quiver/migrations.h"
#include "quiver/result.h"

#include <atomic>
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <stdexcept>
```

## Error Handling

**C++ Layer — Three exact patterns:**

Pattern 1 — Precondition failure:
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error("Cannot update_element: attribute '" + attr_name + "' is not a vector...");
```

Pattern 2 — Not found:
```cpp
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");
throw std::runtime_error("Schema file not found: path/to/schema.sql");
```

Pattern 3 — Operation failure:
```cpp
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
throw std::runtime_error("Failed to begin transaction: " + error);
```

**Precondition helpers in `src/database_impl.h`:**
```cpp
impl_->require_schema("create_element");                       // throws if no schema loaded
impl_->require_collection(collection, "read_scalar_integers"); // throws if collection missing
impl_->require_column(table, column, "operation");             // throws if column missing
```

**C API Layer:**
- All functions return binary `quiver_error_t` (`QUIVER_OK = 0` or `QUIVER_ERROR = 1`)
- Try-catch wraps the C++ call; exception message stored thread-locally
- `QUIVER_REQUIRE(ptr1, ptr2, ...)` macro validates null args, sets error, returns `QUIVER_ERROR`
- Error details retrieved via `quiver_get_last_error()`

```cpp
quiver_error_t quiver_database_create_element(quiver_database_t* db, const char* collection,
                                               quiver_element_t* element, int64_t* out_id) {
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

No ad-hoc error message formats. Bindings surface errors from the C layer — never craft their own.

## Logging

**Framework:** spdlog (instance per `Database` object, stored in `impl_->logger`)

**Levels used:**
- `debug` — Operation entry points and state transitions (e.g., "Opening database: {}", "Inserting element with id: {}")
- `info` — Significant lifecycle events (e.g., "Database closed")
- `warn` — Non-fatal issues (e.g., "Failed to create file sink")
- `error` — Failures during rollback (non-throwing paths)

**Pattern:**
```cpp
impl_->logger->debug("Creating element in collection: {}", collection);
impl_->logger->debug("Inserted element with id: {}", element_id);
impl_->logger->error("Failed to rollback transaction: {}", error);
```

All tests suppress logging with `{.read_only = 0, .console_level = QUIVER_LOG_OFF}`.

## Comments

**Section dividers in test files:**
```cpp
// ============================================================================
// Read scalar tests
// ============================================================================
```

**Inline comments:** Used sparingly to clarify non-obvious behavior:
```cpp
// Don't throw - rollback is often called in error recovery
// In-memory database: no file logging
```

**No JSDoc/Doxygen.** Public API is documented in `CLAUDE.md`, not in-source.

## Function Design

**C++ methods:** Single-responsibility; each `database_*.cpp` file handles one concern
- `database_create.cpp` — element CRUD create/update
- `database_read.cpp` — all scalar/vector/set reads
- `database_update.cpp` — scalar/vector/set update-by-id
- `database_time_series.cpp` — time series read/update + free functions
- `database_metadata.cpp` — metadata get/list + free functions

**Parameters:** `const std::string&` for string inputs, value types for primitives, `int64_t` for integer IDs

**Return values:**
- Bulk reads: `std::vector<T>` (never throws on empty — returns empty vector)
- By-ID reads: `std::optional<T>` (returns `std::nullopt` if not found)
- Mutating operations: `void` or `int64_t` (for `create_element` returning new ID)

**Template helpers** extracted to `src/database_internal.h` to avoid duplication:
```cpp
template <typename T>
std::vector<std::vector<T>> read_grouped_values_all(const Result& result);
template <typename T>
std::vector<T> read_grouped_values_by_id(const Result& result);
```

## Module Design

**Exports:**
- C++ public API: `include/quiver/` headers with `QUIVER_API` macro
- C API: `include/quiver/c/` headers with `QUIVER_C_API` macro and `extern "C"` linkage
- Convenience umbrella header: `include/quiver/quiver.h`

**Pimpl Pattern:**
- Used only when hiding private dependencies (`Database` hides sqlite3, `LuaRunner` hides lua)
- Value types with no private deps use Rule of Zero (no Pimpl): `Element`, `Row`, `Migration`, `ScalarMetadata`, `GroupMetadata`

**Anonymous namespaces** used for TU-local helpers:
```cpp
namespace {
void ensure_sqlite3_initialized() { ... }
spdlog::level::level_enum to_spdlog_level(...) { ... }
}  // anonymous namespace
```

**RAII ownership is strict:**
- `Database` owns sqlite3 handle via Pimpl destructor
- `Impl::TransactionGuard` ensures rollback on exception
- C API: `new`/`delete` pairs with matching `quiver_{entity}_free_*` functions
- String copies via `strdup_safe()` in `src/c/database_helpers.h`

## Cross-Layer Naming Transformation Rules

| C++ method | C API | Julia | Dart | Lua |
|------------|-------|-------|------|-----|
| `create_element` | `quiver_database_create_element` | `create_element!` | `createElement` | `create_element` |
| `read_scalar_integers` | `quiver_database_read_scalar_integers` | `read_scalar_integers` | `readScalarIntegers` | `read_scalar_integers` |
| `update_vector_strings` | `quiver_database_update_vector_strings` | `update_vector_strings!` | `updateVectorStrings` | `update_vector_strings` |

Rules:
- **C API:** Prepend `quiver_database_` to C++ method name
- **Julia:** Same name; add `!` suffix for mutating operations
- **Dart:** Convert `snake_case` to `camelCase`; factory methods become named constructors (`fromSchema`)
- **Lua:** Identical to C++ name (1:1)

---

*Convention analysis: 2026-02-20*
