# Coding Conventions

**Analysis Date:** 2026-02-25

## Naming Patterns

**Files:**
- C++ headers: `snake_case.h` (e.g., `database.h`, `attribute_metadata.h`)
- C++ source: `snake_case.cpp`, organized by category (e.g., `database_create.cpp`, `database_read.cpp`)
- C++ internal headers: `database_impl.h` (Pimpl struct), `database_internal.h` (internal templates)
- C API headers: `snake_case.h` under `include/quiver/c/` (e.g., `database.h`, `element.h`)
- C API implementation: `snake_case.cpp` under `src/c/` by category (e.g., `database_create.cpp`, `database_read.cpp`)
- Test files: `test_database_{category}.cpp` for C++ tests, `test_c_api_database_{category}.cpp` for C API tests

**Functions:**
- C++: `lower_case_with_underscores` for both free functions and methods (enforced by clang-tidy)
- C API: `quiver_{entity}_{verb}[_modifier]` pattern (e.g., `quiver_database_open`, `quiver_database_create_element`)
- Valid verbs: create, read, update, delete, get, list, has, query, describe, export, import, open, close, begin, commit, rollback
- Entity groupings: `quiver_database_*`, `quiver_element_*`, dealloc: `quiver_database_free_*`, `quiver_element_free_*`

**Variables:**
- Local variables: `lower_case_with_underscores` (enforced by clang-tidy)
- Private member variables: `lower_case_with_underscores_` trailing underscore suffix (e.g., `impl_`, `scalars_`, `rows_`)
- Output parameters in C API: `out_` prefix (e.g., `int64_t* out_version`, `quiver_database_t** out_db`)
- Meaningful names over abbreviations: `out_db` not `db_ptr`, `element_id` not `eid`

**Types:**
- Classes and structs: `CamelCase` (enforced by clang-tidy, e.g., `Database`, `Element`, `ScalarMetadata`, `GroupMetadata`)
- Enums: `CamelCase` for C++ enum class (e.g., `DataType`, with values `DataType::Integer`, `DataType::Real`)
- C API enums and constants: `UPPER_CASE` with `QUIVER_` prefix (e.g., `QUIVER_LOG_DEBUG`, `QUIVER_DATA_TYPE_INTEGER`, `QUIVER_OK`)
- C API typedef suffixes: `_t` (e.g., `quiver_database_t`, `quiver_element_t`, `quiver_error_t`)
- Namespaces: `lower_case` (e.g., `quiver`, `quiver::internal`, `quiver::string`)

**Database/Schema:**
- Collections (tables): `PascalCase` (e.g., `Configuration`, `Items`, `Test1`)
- Scalar attributes: `snake_case` (e.g., `label`, `integer_attribute`, `string_attribute`)
- Vector tables: `{Collection}_vector_{name}` (e.g., `Items_vector_values`)
- Set tables: `{Collection}_set_{name}` (e.g., `Items_set_tags`)
- Time series tables: `{Collection}_time_series_{name}` (e.g., `Items_time_series_data`)
- Time series files tables: `{Collection}_time_series_files`
- Dimension column (time series): starts with `date_` (e.g., `date_time`, `date_index`)

## Code Style

**Formatting:**
- Tool: clang-format (LLVM-based)
- Config: `.clang-format`
- Key settings:
  - Indent: 4 spaces (no tabs)
  - Column limit: 120 characters
  - Line ending: LF (Unix)
  - Namespace indentation: None (content not indented inside namespace)
  - Pointer alignment: Left (`int* ptr`, not `int *ptr`)
  - Brace style: Attach (opening brace same line)
  - `BinPackArguments: false` -- multi-param calls break one-per-line
  - `AllowShortFunctionsOnASingleLine: Inline` -- only inline functions can be single line
  - Includes sorted and regrouped by clang-format (`SortIncludes: true`, `IncludeBlocks: Regroup`)

```bash
cmake --build build --config Debug --target format
```

**Linting:**
- Tool: clang-tidy
- Config: `.clang-tidy`
- Checks: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`
- Notable disabled: `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `modernize-use-nodiscard`, `modernize-use-ranges`
- Applied to: `include/quiver/**` and `src/**` only

```bash
cmake --build build --config Debug --target tidy
```

**C++ Standard:**
- Target: C++20
- Features used: structured bindings (`const auto& [name, value]`), `std::optional`, `std::variant`, `starts_with()`, `if constexpr`, `std::visit`

## Import Organization

**C++ Include Order (clang-format regrouped):**

Project headers (quote-style) appear first, then system/third-party headers (angle-bracket):

```cpp
// src/database.cpp - project headers first
#include "database_impl.h"
#include "quiver/migrations.h"
#include "quiver/result.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <stdexcept>
```

```cpp
// src/c/database_read.cpp - C API files
#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"
```

**Path Aliases:**
- Not used. All includes are full paths relative to `include/` or local headers by filename.
- Public headers: `"quiver/database.h"`, `"quiver/c/database.h"`
- Internal headers: `"database_impl.h"`, `"database_internal.h"`, `"internal.h"`, `"database_helpers.h"`

## Error Handling

**Strategy:**
- C++ layer: throw `std::runtime_error` with structured messages
- C API layer: try-catch wrappers convert exceptions to binary codes + thread-local last-error string
- Bindings: retrieve and surface C API error message, never craft their own messages

**Error Message Patterns (exactly 3 formats, no deviations):**

1. **Precondition failure:** `"Cannot {operation}: {reason}"`
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
throw std::runtime_error("Cannot begin_transaction: transaction already active");
throw std::runtime_error("Cannot apply_schema: schema file is empty: " + schema_path);
```

2. **Not found:** `"{Entity} not found: {identifier}"`
```cpp
throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" + collection + "'");
throw std::runtime_error("Schema file not found: " + schema_path);
throw std::runtime_error("Time series table not found: " + ts_table);
throw std::runtime_error("Vector group not found: '" + group_name + "' in collection '" + collection + "'");
```

3. **Operation failure:** `"Failed to {operation}: {reason}"`
```cpp
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
throw std::runtime_error("Failed to begin transaction: " + error);
```

**C API Error Handling Pattern:**
```cpp
quiver_error_t quiver_database_open(const char* path,
                                    const quiver_database_options_t* options,
                                    quiver_database_t** out_db) {
    QUIVER_REQUIRE(path, out_db);  // Null guard macro - returns QUIVER_ERROR if any arg is null

    try {
        *out_db = new quiver_database(path, *options);
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

**QUIVER_REQUIRE Macro:**
- Defined in `src/c/internal.h`
- Variadic: supports 1-9 arguments
- Each null argument generates: `"Null argument: {param_name}"` via stringification
- Always placed at the top of every C API function before any logic

**Utility functions that bypass binary return (direct return):**
- `quiver_get_last_error()` → `const char*`
- `quiver_version()` → `const char*`
- `quiver_clear_last_error()` → `void`
- `quiver_database_options_default()` → `quiver_database_options_t`

## Logging

**Framework:** spdlog (per-instance named logger with unique IDs)

**Levels and Usage:**
- `debug`: Detailed operational trace (e.g., `"Opening database: {}"`, `"Inserted element with id: {}"`)
- `info`: General lifecycle events (e.g., `"Created element {} in {}"`, `"Schema applied successfully"`)
- `warn`: Recoverable non-fatal conditions (e.g., `"Failed to create file sink: {}. Logging to console only."`)
- `error`: Error conditions (e.g., `"Failed to execute query: {}"`, `"Migration {} failed: {}"`)

**Configuration:**
- Console level controlled by `DatabaseOptions.console_level` (`QUIVER_LOG_DEBUG` through `QUIVER_LOG_OFF`)
- File sink: always debug-level, written to `quiver_database.log` in database directory
- In-memory databases (`:memory:`): console-only, no file logging

**Pattern:**
```cpp
impl_->logger->debug("Updating {}.{} for id {} to {}", collection, attribute, id, value);
impl_->logger->info("Created element {} in {}", element_id, collection);
impl_->logger->error("Migration {} failed: {}", migration.version(), e.what());
```

## Comments

**When to Comment:**
- Explain non-obvious intent or algorithm choices, not what code does
- Precondition notes on complex parameters (e.g., `// column_count == 0 clears all rows`)
- Section dividers using `//` comments on separate lines for long functions
- Clarify workarounds: `// NOLINT(performance-no-int-to-ptr) SQLite macro`

**Pattern from actual code:**
```cpp
// Generate unique logger name for multiple Database instances
auto id = g_logger_counter.fetch_add(1);

// Pre-resolve pass: resolve all FK labels before any writes
auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

// Don't throw - rollback is often called in error recovery
```

**JSDoc/TSDoc/Docstrings:**
- C++ headers: minimal inline comments above declarations, not docblocks
- Dart: `///` triple-slash docstrings on all public methods
- Python: `"""double-quoted docstrings"""` on all public methods
- Julia/Lua: comment style only where needed; no formal docblock pattern

## Function Design

**Size:**
- Prefer focused functions under ~50 lines
- C++ Database methods are split across multiple `.cpp` files by category (create, read, update, delete, etc.)
- Internal helpers extracted to templates in `src/database_internal.h`

**Parameters:**
- Pass by `const std::string&` for string parameters (not by value)
- Pass by `const Element&`, `const std::vector<T>&` for composite types
- Pass by value for small types: `int64_t`, `double`, `bool`
- C API output params use `out_` prefix and pointer types (e.g., `int64_t* out_id`, `char** out_value`)

**Return Values:**
- `std::optional<T>` for single-element nullable reads (e.g., `read_scalar_integer_by_id`)
- `std::vector<T>` for all-element reads
- `std::vector<std::vector<T>>` for group reads across all elements
- `std::vector<std::map<std::string, Value>>` for multi-column time series rows
- C API: always returns `quiver_error_t`; values via output pointers

```cpp
// Nullable single-element read
std::optional<int64_t> read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id);

// All elements
std::vector<int64_t> read_scalar_integers(const std::string& collection, const std::string& attribute);

// Per-element group data across all elements
std::vector<std::vector<int64_t>> read_vector_integers(const std::string& collection, const std::string& attribute);

// Multi-column time series (column name → typed value per row)
std::vector<std::map<std::string, Value>> read_time_series_group(const std::string& collection, const std::string& group, int64_t id);
```

## Module Design

**Exports:**
- C++ public API: `include/quiver/` headers (never implementation details)
- C API: `include/quiver/c/` headers
- Implementation: `src/` and `src/c/` with internal headers (not exported)
- DLL export macros: `QUIVER_API` for C++, `QUIVER_C_API` for C API (defined in `include/quiver/export.h` and `include/quiver/c/common.h`)

**Pimpl Usage (only for classes hiding private dependencies):**
```cpp
// include/quiver/database.h
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// src/database_impl.h (private, not exported)
struct Database::Impl {
    sqlite3* db = nullptr;
    std::shared_ptr<spdlog::logger> logger;
    // ...
};
```
- Used for: `Database`, `LuaRunner` (hide sqlite3/lua headers)
- NOT used for: `Element`, `Row`, `Result`, `ScalarMetadata`, `GroupMetadata` (plain value types, Rule of Zero)

**Value Types (Rule of Zero):**
- Plain public members, no Pimpl, compiler-generated copy/move/destructor
- Examples: `Element` (`include/quiver/element.h`), `Row` (`include/quiver/row.h`), `ScalarMetadata`, `GroupMetadata` (`include/quiver/attribute_metadata.h`)

**Move Semantics for Resource Types:**
```cpp
// Delete copy, default move
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&&) noexcept = default;
Database& operator=(Database&&) noexcept = default;
```

**RAII:**
- `std::unique_ptr<Impl>` for Pimpl pattern
- `std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>` for SQLite statement handles
- `Impl::TransactionGuard` (nested class in `src/database_impl.h`): RAII guard that is nest-aware (no-op if transaction already active)
- C API heap memory: `new`/`delete[]` with matched free functions co-located in same translation unit

**Barrel Files:** Not used. Each header imported directly by name.

## C API Memory Management

**Allocation/Free Co-location Rule:**
Every `new` allocation and its matching `delete` must live in the same `.cpp` file:
- Read alloc/free → `src/c/database_read.cpp`
- Metadata alloc/free → `src/c/database_metadata.cpp`
- Time series alloc/free → `src/c/database_time_series.cpp`

**String Allocation:**
Always use `strdup_safe()` from `src/c/database_helpers.h`:
```cpp
inline char* strdup_safe(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}
```
Never use `strdup()` (platform-specific) or `malloc`/`free` (mixed with `new`/`delete`).

**Null Handling for Empty Collections:**
All read functions return `nullptr` with `out_count = 0` for empty results (never allocate empty arrays).

## Cross-Layer Naming Transformations

Given any C++ method name, derive the equivalent mechanically:

| From C++ | To C API | To Julia | To Dart | To Python | To Lua |
|----------|----------|----------|---------|-----------|--------|
| `method_name` | `quiver_database_method_name` | `method_name` (+ `!` if mutating) | `methodName` | `method_name` | `method_name` |

**C++ → C API:** Prefix `quiver_database_` to the C++ method name.
```
create_element  →  quiver_database_create_element
read_scalar_integers  →  quiver_database_read_scalar_integers
begin_transaction  →  quiver_database_begin_transaction
```

**C++ → Julia:** Same name. Add `!` suffix for mutating operations (create, update, delete).
```
create_element  →  create_element!
read_scalar_integers  →  read_scalar_integers
in_transaction  →  in_transaction
```

**C++ → Dart:** `snake_case` → `camelCase`. Factory methods use Dart named constructors.
```
read_scalar_integers  →  readScalarIntegers
from_schema  →  Database.fromSchema()
read_scalar_integer_by_id  →  readScalarIntegerById
```

**C++ → Python:** Same `snake_case`. Factories are `@staticmethod`. Properties are regular methods (not `@property`).
```
create_element  →  create_element
from_schema  →  Database.from_schema()
in_transaction  →  in_transaction()   # called as method, not property
```

**C++ → Lua:** Same name exactly (1:1 match). No lifecycle methods.
```
read_scalar_integers  →  read_scalar_integers
begin_transaction  →  begin_transaction
```

## Binding-Only Convenience Methods

These exist in bindings but have no C++ or C API counterpart. They compose core operations.

**DateTime wrappers (Julia and Dart only):**
- `read_scalar_date_time_by_id` / `readScalarDateTimeById` — wraps string read + date parsing
- `read_vector_date_time_by_id` / `readVectorDateTimesById`
- `read_set_date_time_by_id` / `readSetDateTimesById`
- `query_date_time` / `queryDateTime`

**Composite read helpers (Julia, Dart, Lua):**
- `read_all_scalars_by_id` / `readAllScalarsById` — dispatches over `list_scalar_attributes` + typed reads
- `read_all_vectors_by_id` / `readAllVectorsById`
- `read_all_sets_by_id` / `readAllSetsById`

**Multi-column group readers (Julia and Dart only):**
- `read_vector_group_by_id` / `readVectorGroupById` — metadata + per-column vector reads, transposes to rows
- `read_set_group_by_id` / `readSetGroupById`

**Transaction block wrappers (Julia, Dart, Lua):**
- Julia: `transaction(db) do db...end`
- Dart: `db.transaction((db) {...})`
- Lua: `db:transaction(function(db)...end)`
- All wrap begin + fn + commit/rollback on exception

## Database Method Naming Convention

Public `Database` methods follow `verb_[category_]type[_by_id]`:

- **Verbs:** create, read, update, delete, get, list, has, query, describe, export, import
- **`_by_id` suffix:** Only when both "all elements" and "single element" variants exist
- **Singular vs plural:** Matches return cardinality (`read_scalar_integers` → `vector`, `read_scalar_integer_by_id` → `optional`)

```
create_element                     # create + entity
read_scalar_integers               # read + category + type (all elements)
read_scalar_integer_by_id          # read + category + type (single element, _by_id suffix)
read_vector_floats_by_id           # read + category + type + _by_id
get_scalar_metadata                # get + category + type
list_vector_groups                 # list + category + entity (plural)
update_time_series_group           # update + category + entity
query_string                       # query + return type
export_csv                         # export + format
```

---

*Convention analysis: 2026-02-25*
