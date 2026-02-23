# Coding Conventions

**Analysis Date:** 2026-02-23

## Naming Patterns

**Files:**
- C++ headers: `snake_case.h` (e.g., `database.h`, `attribute_metadata.h`)
- C++ source: `snake_case.cpp` (e.g., `database.cpp`, `database_impl.h` for Pimpl private structs)
- C API headers: `snake_case.h` under `include/quiver/c/` (e.g., `database.h`, `element.h`)
- C API implementation: `snake_case.cpp` under `src/c/` with organization by category (e.g., `database_create.cpp`, `database_read.cpp`, `database_update.cpp`)
- Test files: `test_database_{category}.cpp` for C++ tests, `test_c_api_database_{category}.cpp` for C API tests (e.g., `test_database_lifecycle.cpp`, `test_c_api_database_read.cpp`)

**Functions:**
- C++: `lower_case_with_underscores` for both free functions and methods (enforced by clang-tidy)
- C API: `quiver_{entity}_{verb}[_modifier]` pattern (e.g., `quiver_database_open`, `quiver_database_create_element`, `quiver_element_free_string`)
- Verbs: create, read, update, delete, get, list, has, query, describe, export, import, open, close, begin, commit, rollback
- Entity groups use hierarchical names: `quiver_database_*`, `quiver_element_*`, `quiver_database_free_*` for deallocation

**Variables:**
- Local and member: `lower_case_with_underscores` (enforced by clang-tidy with suffix `_` for private members, e.g., `impl_`, `scalars_`)
- Parameter names: `lower_case_with_underscores`
- Meaningful names preferred over abbreviations (e.g., `out_db` not `db_ptr`, `element_id` not `eid`)

**Types:**
- Classes and structs: `CamelCase` (enforced by clang-tidy, e.g., `Database`, `Element`, `ScalarMetadata`, `GroupMetadata`)
- Enums: `CamelCase` (enforced by clang-tidy, e.g., `DataType`, `LogLevel`)
- Enum constants: `CamelCase` with QUIVER_ prefix for C API (e.g., `QUIVER_LOG_DEBUG`, `QUIVER_DATA_TYPE_INTEGER`)
- Type aliases: snake_case (e.g., `quiver_database_t`, `quiver_element_t`, `quiver_error_t`)
- Namespaces: `lower_case` (e.g., `quiver`, `quiver::test`)

**Database/Schema:**
- Collections (tables): `PascalCase` (e.g., `Configuration`, `Collection`, `Test1`)
- Scalar attributes: `snake_case` (e.g., `label`, `integer_attribute`, `string_attribute`)
- Vector tables: `{Collection}_vector_{name}` (e.g., `Items_vector_values`)
- Set tables: `{Collection}_set_{name}` (e.g., `Items_set_tags`)
- Time series tables: `{Collection}_time_series_{name}` (e.g., `Items_time_series_data`)
- Dimension column (time series): starts with `date_` (e.g., `date_time`, `date_index`)

## Code Style

**Formatting:**
- Tool: clang-format (LLVM-based)
- Config file: `.clang-format`
- Key settings:
  - Indent: 4 spaces (never tabs)
  - Column limit: 120 characters
  - Line ending: LF (Unix style)
  - Namespace indentation: None (namespace content unindented)
  - Pointer alignment: Left (e.g., `int* ptr` not `int *ptr`)
  - Brace style: Attach (opening braces on same line)

**Usage:**
```bash
cmake --build build --config Debug --target format
```

**Linting:**
- Tool: clang-tidy
- Config file: `.clang-tidy`
- Checks enabled: bugprone-*, modernize-*, performance-*, readability-identifier-naming
- Usage:
```bash
cmake --build build --config Debug --target tidy
```

**C++ Standard:**
- Target: C++20
- Set in CMakeLists.txt: `set(CMAKE_CXX_STANDARD 20)`
- Modern features used: concepts, ranges, structured bindings, std::optional, std::variant

## Import Organization

**C++ Header Order:**
1. Standard library headers (`<vector>`, `<string>`, `<memory>`, etc.)
2. Third-party library headers (`<gtest/gtest.h>`, `<spdlog/spdlog.h>`, `<sqlite3.h>`)
3. Project headers in quotes (`"quiver/database.h"`, `"internal.h"`)
4. Blank line separating groups

**Example from `src/database.cpp`:**
```cpp
#include "database_impl.h"
#include "quiver/migrations.h"
#include "quiver/result.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
```

**Path Aliases:**
- Not used; direct includes with full paths relative to `include/` or local headers with quotes

## Error Handling

**Strategy:**
- Exceptions: C++ layer (std::runtime_error)
- Binary error codes: C API (QUIVER_OK = 0, QUIVER_ERROR = 1)
- Pattern: Try-catch in C API wrappers, convert exceptions to error codes and last-error messages

**Error Message Patterns (3 required formats):**

1. **Precondition failure:** `"Cannot {operation}: {reason}"`
   ```cpp
   throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
   throw std::runtime_error("Cannot update_scalar_relation: attribute 'x' is not a foreign key in collection 'Y'");
   ```

2. **Not found:** `"{Entity} not found: {identifier}"`
   ```cpp
   throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");
   throw std::runtime_error("Schema file not found: path/to/schema.sql");
   ```

3. **Operation failure:** `"Failed to {operation}: {reason}"`
   ```cpp
   throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
   throw std::runtime_error("Failed to execute statement: " + sqlite3_errmsg(db));
   ```

**C API Error Handling:**
```cpp
quiver_error_t quiver_database_open(const char* path, const quiver_database_options_t* options, quiver_database_t** out_db) {
    QUIVER_REQUIRE(path, out_db);  // Null check macro

    try {
        *out_db = new quiver_database(path, *options);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

**Precondition Checks:**
- Macro `QUIVER_REQUIRE(...)` in `src/c/internal.h` validates non-null pointers before operation
- Used at entry point of all C API functions
- Returns QUIVER_ERROR if preconditions fail

## Logging

**Framework:** spdlog
- Thread-safe logging with console and file sinks
- Initialized per Database instance with unique logger names

**Levels and Usage:**
- `debug`: Detailed operational information (e.g., "Opening database: {path}", "Executing query: {sql}")
- `info`: General informational messages
- `warn`: Unexpected but recoverable conditions (e.g., "Failed to create file sink: {}. Logging to console only.")
- `error`: Error conditions (e.g., "Failed to execute query: {}", sqlite3_errmsg(db))
- `off`: Disable all logging

**Configuration:**
- Console level (verbosity): set via `DatabaseOptions.console_level`
- File logging: Always at debug level if available; logs to `quiver_database.log` in database directory
- In-memory databases: Console-only logging

**Example:**
```cpp
impl_->logger->debug("Opening database: {}", path);
impl_->logger->warn("Database is in-memory only; no file logging will be performed.");
```

## Comments

**When to Comment:**
- Explain non-obvious intent, not what code does
- Document preconditions and postconditions for complex operations
- Clarify algorithm choices or workarounds
- Mark sections in large functions with comment dividers (e.g., `// ============================================================================`)
- Avoid redundant comments on straightforward code

**Example from `src/database.cpp`:**
```cpp
// Generate unique logger name for multiple Database instances
auto id = g_logger_counter.fetch_add(1);
auto logger_name = "quiver_database_" + std::to_string(id);

// Database file in current directory (no path separator)
db_dir = fs::current_path();
```

**JSDoc/TSDoc:**
- Not used in C++ headers (follow project style)
- Public class/function documentation via inline comments above declarations
- Focus on behavior contract, not implementation details

## Function Design

**Size:**
- Prefer small, focused functions (under 50 lines)
- Large functions (>100 lines) split into helper functions or separate files (e.g., `database_create.cpp`, `database_read.cpp`)

**Parameters:**
- Pass by const reference for large objects (`const std::string&`, `const Element&`)
- Pass by value for small types (`int64_t`, `double`)
- Output parameters in C API use pointers with `out_` prefix (e.g., `int64_t* out_version`)
- Builder pattern for complex initialization (e.g., `Element().set(...).set(...)`)

**Return Values:**
- Use `std::optional<T>` for single-element nullable reads (e.g., `read_scalar_integer_by_id`)
- Return vectors for multi-element results (e.g., `read_scalar_integers` returns `std::vector<int64_t>`)
- Use `std::vector<std::map<std::string, Value>>` for multi-column time series rows
- C API: Always return binary error code; values via output parameters

**Example from `include/quiver/database.h`:**
```cpp
// Single element with nullable result
std::optional<int64_t> read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id);

// All elements - vector return
std::vector<int64_t> read_scalar_integers(const std::string& collection, const std::string& attribute);

// Vectors of vectors for multi-element group reads
std::vector<std::vector<int64_t>> read_vector_integers(const std::string& collection, const std::string& attribute);

// Multi-column time series (map per row)
std::vector<std::map<std::string, Value>> read_time_series_group(const std::string& collection, const std::string& group, int64_t id);
```

## Module Design

**Exports:**
- C++ public API in `include/quiver/` (headers only)
- C API in `include/quiver/c/` (headers only)
- Implementation in `src/` and `src/c/` with internal headers as needed
- Pimpl pattern for classes with private implementation (e.g., `Database`, `LuaRunner`)

**Pimpl Usage:**
- Used only for classes hiding private dependencies (sqlite3, lua headers)
- Example in `include/quiver/database.h`:
```cpp
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```
- Actual Impl struct definition in `src/database_impl.h` or `.cpp`

**Value Types (Rule of Zero):**
- Classes with no private dependencies use direct members, no Pimpl, no custom copy/move/destructor
- Examples: `Element`, `Row`, `ScalarMetadata`, `GroupMetadata`, `CSVExportOptions`
- Compiler-generated special members are correct

**Barrel Files:**
Not used; each header is imported directly by name.

**RAII and Ownership:**
- Move semantics required for resource-owning types (e.g., `Database`)
- Delete copy constructors: `Database(const Database&) = delete`
- Default move: `Database(Database&&) = default`
- Unique pointers for heap allocations: `std::unique_ptr<Impl> impl_`
- String ownership via `std::string`, not raw `char*`
- C API allocation functions paired with free functions in same translation unit

## Cross-Layer Naming Transformations

**C++ to C API:**
- Add `quiver_database_` prefix to method name
- Example: `create_element` → `quiver_database_create_element`

**C++ to Julia (optional binding convention):**
- Same name as C++
- Add `!` suffix for mutating operations
- Example: `create_element` → `create_element!`, `read_scalar_integers` → `read_scalar_integers`

**C++ to Dart (optional binding convention):**
- Convert snake_case to camelCase
- Factory methods use Dart named constructors
- Example: `read_scalar_integers` → `readScalarIntegers`, `from_schema` → `Database.fromSchema()`

**C++ to Lua (optional binding convention):**
- Same name as C++ (1:1 match)
- Example: `read_scalar_integers` → `read_scalar_integers`, `begin_transaction` → `begin_transaction`

---

*Convention analysis: 2026-02-23*
