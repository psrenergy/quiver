# Coding Conventions

**Analysis Date:** 2026-02-09

## Naming Patterns

**Files:**
- Header files: `snake_case.h` (e.g., `database.h`, `attribute_metadata.h`, `lua_runner.h`)
- Implementation files: `snake_case.cpp` (e.g., `database.cpp`, `element.cpp`)
- Test files: `test_<function_area>.cpp` or `test_<category>_<function>.cpp` (e.g., `test_database_lifecycle.cpp`, `test_c_api_database_read.cpp`)
- C API headers: `include/quiver/c/` namespace with `<name>.h` (e.g., `c/database.h`, `c/element.h`, `c/options.h`)

**Functions/Methods:**
- `snake_case` for all function and method names (e.g., `create_element()`, `read_scalar_integers()`, `set_scalar_relation()`)
- Static factory methods: `from_<source>` pattern (e.g., `from_schema()`, `from_migrations()`)
- Helper/utility methods: lowercase with underscores (e.g., `get_row_value()`, `read_grouped_values_all()`, `ensure_sqlite3_initialized()`)
- Private helper: `impl_` for Pimpl member variables

**Variables:**
- Member variables: `snake_case_` suffix with trailing underscore (e.g., `impl_`, `db_`, `scalars_`, `arrays_`, `committed_`, `logger_`)
- Local variables: `snake_case` (e.g., `current_id`, `vector_table`, `error_msg`)
- Constants: `CONSTANT_CASE` (e.g., `QUIVER_OK`, `QUIVER_ERROR`, `SQLITE_OK`)
- Macro constants: `CONSTANT_CASE` (e.g., `MAX_HOURS_IN_DAY`, `MIN_HOURS_IN_MONTH`)

**Types/Classes:**
- Classes: `PascalCase` (e.g., `Database`, `Element`, `Row`, `Result`, `Schema`, `TypeValidator`)
- Structs: `PascalCase` for public API (e.g., `ScalarMetadata`, `GroupMetadata`)
- Enums: `PascalCase` for enum names, `CONSTANT_CASE` for enum values (e.g., `quiver_log_level_t` with `QUIVER_LOG_DEBUG`, `QUIVER_LOG_INFO`)
- Type aliases: `PascalCase` or use `_t` suffix for C API (e.g., `using Value = std::variant<...>`, `quiver_database_t`)
- Using declarations: `PascalCase` (e.g., `DatabaseOptions = quiver_database_options_t`)
- Namespaces: `snake_case` (e.g., `namespace quiver`, `namespace fs = std::filesystem`)

**C API identifiers:**
- Functions: `quiver_<object>_<operation>` (e.g., `quiver_database_open()`, `quiver_read_scalar_integers()`)
- Opaque types: `quiver_<name>_t` (e.g., `quiver_database_t`, `quiver_element_t`)
- Enum values: `QUIVER_<CATEGORY>_<NAME>` (e.g., `QUIVER_LOG_DEBUG`, `QUIVER_DATA_TYPE_INTEGER`)
- Free functions: `quiver_free_<type>` (e.g., `quiver_free_strings()`, `quiver_free_scalar_metadata()`)
- Error codes: `QUIVER_OK` (0), `QUIVER_ERROR` (1)

## Code Style

**Formatting:**
- Tool: clang-format (config: `.clang-format`)
- Line length: 120 columns maximum
- Indentation: 4 spaces, no tabs
- Brace style: Attach (K&R variant) - opening brace on same line as declaration
- Pointer alignment: Left (e.g., `int* ptr` not `int *ptr`)

**Key clang-format settings:**
- `AccessModifierOffset: -4` - public/private not indented
- `AlwaysBreakTemplateDeclarations: Yes` - template keyword on new line
- `BreakConstructorInitializers: BeforeColon` - initializer list colon on new line
- `ColumnLimit: 120` - break lines exceeding 120 chars
- `AllowShortFunctionsOnASingleLine: Inline` - only inline functions fit on single line
- `AllowShortBlocksOnASingleLine: false` - blocks always multi-line
- `BinPackArguments: false`, `BinPackParameters: false` - one argument per line when wrapping
- `IncludeBlocks: Regroup` - group includes by type (system, then project)
- `SortIncludes: true` - alphabetically within groups
- `SortUsingDeclarations: true` - sort using declarations

## Import Organization

**Order (enforced by clang-format):**
1. System/standard library includes: `<iostream>`, `<vector>`, `<string>`, `<memory>`, `<optional>`, `<filesystem>`, `<fstream>`, `<sstream>`, `<map>`, `<atomic>`, `<mutex>`, `<new>`, `<cstdint>`, `<variant>`, `<algorithm>`
2. Third-party includes: `<spdlog/...>`, `<sqlite3.h>`, `<gtest/gtest.h>`, `<lua.hpp>`
3. Project includes: `"quiver/..."` (quoted, relative to project root)

**Example (from `src/database.cpp`):**
```cpp
#include "quiver/database.h"

#include "quiver/migrations.h"
#include "quiver/result.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
```

**Path aliases:**
- None currently defined; full relative paths used in includes

## Error Handling

**Exceptions:**
- Use `std::runtime_error` for all user-facing errors with descriptive messages
- Error messages include context: collection name, attribute name, operation type (e.g., `"Collection not found: " + name`)
- Never use error codes or null returns for errors; exceptions are the standard

**C API Error Handling:**
- Return `quiver_error_t` (binary: `QUIVER_OK = 0`, `QUIVER_ERROR = 1`)
- Capture exceptions with try-catch block; set error message via `quiver_set_last_error()`
- Error details retrieved via `quiver_get_last_error()` - no exceptions cross C boundary
- Example pattern in `src/c/database.cpp`:
```cpp
quiver_error_t quiver_database_open(const char* path,
                                    const quiver_database_options_t* options,
                                    quiver_database_t** out_db) {
    QUIVER_REQUIRE(path, out_db);

    try {
        *out_db = new quiver_database(path, *options);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

**Validation:**
- Input validation via `QUIVER_REQUIRE` macro in C API (checks non-null pointers)
- Schema validation via `SchemaValidator` class (`src/schema_validator.cpp`)
- Type validation via `TypeValidator` class with `validate_value()` method
- Empty element validation: `"Element must have at least one scalar attribute"`
- Empty array validation: `"Empty array not allowed for '...'"`

## Logging

**Framework:** spdlog v1.x with multi-sink support

**Levels:** debug, info, warn, error, off (configurable per `DatabaseOptions`)

**Patterns:**
- Initialization: one logger per database instance with unique name (counter-based: `quiver_logger_0`, `quiver_logger_1`)
- Console sink: always present, color output on Windows/Unix
- File sink: optional, writes to `<db_path>.log` if logging enabled
- Log all operations: database open/close, transactions, schema load, queries
- Example from `src/database.cpp`:
```cpp
logger->debug("Opening database: {}", path);
logger->info("Database closed");
logger->debug("Transaction started");
logger->debug("Transaction committed");
logger->debug("Transaction rolled back");
```

**Access:** via `spdlog::debug()`, `spdlog::info()`, `spdlog::warn()`, `spdlog::error()` for direct calls; `logger->debug()` for instance methods

## Comments

**When to Comment:**
- Explain non-obvious algorithms or business logic
- Document private helper functions in anonymous namespace
- Clarify intent of complex conditionals or template metaprogramming
- Mark intentional skipping of cleanup (e.g., `// Don't throw - rollback is often called in error recovery`)

**JSDoc/TSDoc:** Not used; C++ lacks standard documentation format. Comments are inline only.

**Example (from `src/database.cpp`):**
```cpp
namespace {
    // Type-specific Row value extractors for template implementations
    inline std::optional<int64_t> get_row_value(const quiver::Row& row, size_t index, int64_t*) {
        return row.get_integer(index);
    }

    // Template for reading grouped values (vectors or sets) for all elements
    template <typename T>
    std::vector<std::vector<T>> read_grouped_values_all(const quiver::Result& result) {
        // ...
    }
}
```

## Function Design

**Size:** Typically 15-100 lines; decompose longer functions into helpers (e.g., `read_grouped_values_all()`, `read_grouped_values_by_id()`)

**Parameters:**
- By reference for input: `const std::string&`, `const Element&`, `const DatabaseOptions&`
- By pointer for outputs (C API only): `T**` for out-parameters (e.g., `quiver_database_t** out_db`)
- Const correctness: methods that don't modify state marked `const`
- Optional parameters in C++ API via default arguments (e.g., `const DatabaseOptions& options = default_database_options()`)

**Return Values:**
- Value semantics for built-in types: `int64_t`, `double`
- STL containers by value: `std::vector<int64_t>`, `std::vector<std::string>` (move semantics optimized)
- Optional results: `std::optional<T>` for nullable single values (e.g., `read_scalar_integer_by_id()`)
- Always return meaningful data; no void returns with side effects via output parameters in C++ API

## Module Design

**Exports:**
- All public C++ symbols in `quiver` namespace
- Marked with `QUIVER_API` macro for visibility control (Windows DLL symbol export)
- C API symbols in `extern "C"` block with `QUIVER_C_API` macro

**Barrel Files:**
- `include/quiver/quiver.h` - main public header, includes core classes
- No barrel pattern within subdirectories; each header includes only necessary dependencies

**Header Layout (C++ public):**
1. Include guard
2. System/third-party includes
3. Project includes (relative)
4. `namespace quiver {`
5. Class/struct declarations
6. Inline implementations (factory helpers, metadata getters)
7. `}  // namespace quiver`
8. Closing include guard

**Source File Layout (C++):**
1. Public header include
2. Related internal headers
3. Standard library includes (sorted)
4. Third-party includes
5. Anonymous namespace with private helpers
6. Named namespace (`namespace quiver {`)
7. Implementation
8. Closing namespace comment

**Example structure from `src/element.cpp`:**
```cpp
#include "quiver/element.h"

#include <sstream>

namespace quiver {

namespace {
    // Private helper functions
    std::string value_to_string(const Value& value) { /* ... */ }
}  // namespace

// Public method implementations
Element& Element::set(const std::string& name, int64_t value) { /* ... */ }

}  // namespace quiver
```

**Private Implementation Details:**
- Use anonymous namespace (`namespace { }`) for file-local helpers
- Use Pimpl (`struct Impl`) only when hiding private dependencies (sqlite3, lua headers)
- Prefer plain value types (direct member variables, compiler-generated copy/move) for simple data holders

## Move Semantics & RAII

**Resource Types (those with pointers to external resources):**
- Deleted copy constructor and copy assignment: `= delete`
- Defaulted move constructor and move assignment: `= default`
- Default destructor or explicit RAII cleanup
- Example from `include/quiver/database.h`:
```cpp
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;

Database(Database&& other) noexcept;
Database& operator=(Database&&) noexcept;

~Database();
```

**Value Types (Element, Row, Result, Metadata structs):**
- Rule of Zero: no explicit destructor, copy, or move
- Compiler-generated defaults used
- Safe to copy and move
- Example from `include/quiver/element.h`:
```cpp
class Element {
public:
    Element() = default;
    // No destructor, no copy/move operators - compiler-generated
private:
    std::map<std::string, Value> scalars_;
    std::map<std::string, std::vector<Value>> arrays_;
};
```

**RAII Pattern:**
- `TransactionGuard` in `Database::Impl` ensures rollback on exception
- Constructor calls `begin_transaction()`
- Destructor calls `rollback()` if not `commit()` explicitly called
- Located in anonymous namespace within `src/database.cpp`

## Inline vs Definition

**Inline Functions:**
- Short factory/helper methods in headers: `inline DatabaseOptions default_database_options() { return {0, QUIVER_LOG_INFO}; }`
- Conversion functions: `inline DataType data_type_from_string(const std::string& type_str) { /* ... */ }`
- Condition checks: `inline bool is_date_time_column(const std::string& name) { /* ... */ }`
- Accessor methods: `inline size_t Row::column_count() const { return size(); }`

**Out-of-line Definitions:**
- All class method implementations in `.cpp` files
- Complex logic kept out of headers to reduce compilation dependencies

## Template Usage

**Patterns:**
- Type-specific readers use template functions with tag dispatch (e.g., `get_row_value()` with `int64_t*`, `double*`, `std::string*` tags)
- Container readers use template for value type: `template <typename T> std::vector<T> read_grouped_values_by_id()`
- C API uses templates internally for shared logic, then explicit instantiation for each type

**Example from `src/database.cpp`:**
```cpp
// Tag dispatch overloads
template <typename T>
std::vector<T> read_grouped_values_by_id(const quiver::Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = get_row_value(result[i], 0, static_cast<T*>(nullptr));
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}
```

## C API Specific Conventions

**Opaque Pointers:**
- `quiver_database_t` is opaque (implementation hidden)
- `quiver_element_t` wraps C++ `Element` class
- Factory functions return via out-parameter: `quiver_error_t quiver_database_open(..., quiver_database_t** out_db)`

**String Handling:**
- Null-terminated C strings via `strdup_safe()` helper
- Caller responsible for freeing returned strings with `quiver_free_strings()`
- No string length parameters; length inferred from array size parameter

**Parameterized Queries:**
- Parallel arrays for types and values: `param_types[]`, `param_values[]`, `param_count`
- Type codes: `QUIVER_DATA_TYPE_INTEGER`, `QUIVER_DATA_TYPE_FLOAT`, `QUIVER_DATA_TYPE_STRING`, `QUIVER_DATA_TYPE_NULL`
- Values are void pointers to typed data: `const int64_t*`, `const double*`, `const char*`, `nullptr`

**Metadata Conversion:**
- Internal helpers `convert_scalar_to_c()`, `convert_group_to_c()` in `src/c/database.cpp`
- Unified free functions: `quiver_free_scalar_metadata()`, `quiver_free_group_metadata()`, and array variants
- No duplication between metadata types

---

*Convention analysis: 2026-02-09*
