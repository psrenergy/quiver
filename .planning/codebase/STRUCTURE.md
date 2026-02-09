# Codebase Structure

**Analysis Date:** 2026-02-09

## Directory Layout

```
quiver/
├── include/quiver/              # C++ public headers
│   ├── c/                       # C API headers (for FFI)
│   ├── blob/                    # Blob/binary file support
│   ├── *.h                      # Core C++ API headers
│   └── export.h                 # Visibility macros
├── src/                         # C++ implementation
│   ├── c/                       # C API implementation
│   ├── blob/                    # Blob implementation
│   ├── *.cpp                    # Core implementation
│   └── CMakeLists.txt
├── tests/                       # Test suite
│   ├── schemas/                 # Shared SQL schemas for tests
│   │   ├── valid/               # Valid test schemas
│   │   └── invalid/             # Invalid schemas for validation testing
│   ├── test_*.cpp               # C++ tests (Google Test)
│   └── test_c_api_*.cpp         # C API tests
├── bindings/                    # Language bindings
│   ├── julia/                   # Julia binding (Quiver.jl)
│   ├── dart/                    # Dart binding
│   └── python/                  # Python binding
├── cmake/                       # CMake utilities
├── .clang-format                # Code formatting config
├── CMakeLists.txt               # Root build config
└── CLAUDE.md                    # Project instructions
```

## Directory Purposes

**`include/quiver/`:**
- Purpose: Public C++ API headers
- Contains: Class/struct definitions, type aliases, method signatures
- Key files:
  - `database.h`: Main Database class (lines 23-200)
  - `element.h`: Element builder with fluent API (lines 13-43)
  - `attribute_metadata.h`: ScalarMetadata, GroupMetadata structs
  - `schema.h`: Schema introspection API
  - `row.h`, `result.h`: Query result parsing
  - `value.h`: Variant-like value type
  - `export.h`: Visibility macros (QUIVER_API)

**`include/quiver/c/`:**
- Purpose: C API headers for FFI binding generators
- Contains: Opaque handle types, C function declarations, error codes
- Key files:
  - `database.h`: ~250+ C API functions (factory, CRUD, metadata, query)
  - `element.h`: C-style element construction
  - `options.h`: DatabaseOptions struct shared with C++
  - `common.h`: Error codes (QUIVER_OK=0, QUIVER_ERROR=1)
  - `lua_runner.h`: Lua script execution

**`include/quiver/blob/`:**
- Purpose: Binary file support for external time-series data
- Contains: Blob metadata, time constants, dimension handling
- Key files:
  - `blob.h`: File reading/writing with dimensional indexing
  - `blob_metadata.h`: Blob structure metadata
  - `dimension.h`, `time_properties.h`: Dimension/time utilities

**`src/`:**
- Purpose: C++ implementation
- Contains: Database lifecycle, CRUD implementation, query execution
- Key files:
  - `database.cpp`: Main implementation (2100+ lines)
    - Database::Impl struct with sqlite3 pointer, logger, schema cache
    - TransactionGuard RAII (lines 237-257)
    - Template helpers for reading grouped values (lines 40-77)
  - `schema.cpp`: Schema introspection from database metadata
  - `schema_validator.cpp`: Configuration table, FK constraint checks
  - `type_validator.cpp`: Runtime type validation
  - `migrations.cpp`: Migration tracking and application
  - `element.cpp`: Element builder implementation
  - `result.cpp`, `row.cpp`: Query result types
  - `lua_runner.cpp`: Lua script integration

**`src/c/`:**
- Purpose: C API implementation (FFI glue)
- Contains: Wrappers around C++ classes, error handling, memory management
- Key files:
  - `database.cpp`: All C API function implementations (1600+ lines)
    - Wraps Database class methods in try-catch blocks
    - Allocates/frees C arrays for output parameters
    - Helper templates for numeric/string vectors (lines 11-76)
  - `element.cpp`: C element construction
  - `internal.h`: Opaque struct definitions, QUIVER_REQUIRE macros (lines 28-80)
  - `common.cpp`: Error message storage and retrieval

**`src/blob/`:**
- Purpose: Binary file I/O for time series
- Contains: File handle management, dimensional array indexing

**`tests/`:**
- Purpose: Comprehensive test coverage
- Contains: GTest-based unit tests for C++ and C APIs
- Organization by functionality:
  - `test_database_lifecycle.cpp`: Open, close, move semantics, options
  - `test_database_create.cpp`: Element creation with scalars/vectors/sets
  - `test_database_read.cpp`: Scalar/vector/set read operations
  - `test_database_update.cpp`: Scalar/vector/set updates
  - `test_database_delete.cpp`: Element deletion
  - `test_database_query.cpp`: Raw SQL queries with parameters
  - `test_database_relations.cpp`: Foreign key relations
  - `test_database_time_series.cpp`: Time series group operations
  - `test_lua_runner.cpp`: Lua script execution with database access
  - `test_element.cpp`: Element builder API
  - `test_migrations.cpp`: Migration application
  - `test_schema_validator.cpp`: Schema validation errors
  - C API tests mirror above with `test_c_api_database_*.cpp` prefix

**`tests/schemas/`:**
- Purpose: Shared SQL schema files for all tests
- Contains: Valid and invalid schemas
- Location: `tests/schemas/valid/`, `tests/schemas/invalid/`
- Examples: `basic.sql`, `collections.sql`, `time_series.sql`
- Macro usage: `VALID_SCHEMA("basic.sql")` expands to full schema file path

**`bindings/julia/`:**
- Purpose: Julia language binding
- Structure:
  - `src/*.jl`: Julia source files
  - `test/test.bat`: Julia test runner
  - `generator/generator.bat`: FFI binding code generator
- Key modules: `database.jl`, `element.jl`, `exceptions.jl`, split read/write modules

**`bindings/dart/`:**
- Purpose: Dart language binding
- Structure:
  - `lib/src/*.dart`: Dart implementation
  - `lib/src/ffi/`: Generated FFI bindings from C API
  - `test/test.bat`: Dart test runner
  - `generator/generator.bat`: FFI binding generator
- Pattern: Splits functionality into `database_create.dart`, `database_read.dart`, etc.

**`bindings/python/`:**
- Purpose: Python language binding
- Status: Minimal or placeholder

**`cmake/`:**
- Purpose: CMake utilities and helpers
- Contains: CompilerOptions.cmake, Dependencies.cmake, Platform.cmake

## Key File Locations

**Entry Points:**

- `include/quiver/quiver.h`: Convenience header including database.h and element.h (lines 1-8)
- `include/quiver/database.h`: Public API entry (Database class, all CRUD methods)
- `include/quiver/c/database.h`: C API entry (all ~250+ functions)
- `src/database.cpp`: Implementation of Database class and most operations

**Configuration:**

- `CMakeLists.txt`: Root build config (C++20, shared/static options, test targets)
- `CMakePresets.json`: Build presets for Debug/Release
- `.clang-format`: Code style rules
- `CLAUDE.md`: Project principles and patterns (required reading)

**Core Logic:**

- `src/database.cpp`: Database operations, transaction handling, query execution (lines 150-2100)
- `src/schema.cpp`: Schema introspection from SQLite metadata
- `src/c/database.cpp`: C API wrappers for all operations (lines 80-1600+)
- `src/lua_runner.cpp`: Lua integration with database binding

**Testing:**

- `tests/test_utils.h`: Utility macros like VALID_SCHEMA("name.sql")
- `tests/test_database_create.cpp`: Template for CRUD test patterns
- `tests/test_c_api_database_create.cpp`: Mirror C API test examples

## Naming Conventions

**Files:**

- C++ implementation: `snake_case.cpp` (e.g., `database.cpp`, `schema_validator.cpp`)
- C++ headers: `snake_case.h` (e.g., `database.h`, `attribute_metadata.h`)
- C API headers: `snake_case.h` (e.g., `database.h`, but under `include/quiver/c/`)
- Test files: `test_component.cpp` or `test_c_api_component.cpp`
- Test schemas: `lowercase.sql` (e.g., `basic.sql`, `collections.sql`)

**Classes:**

- PascalCase: Database, Element, Result, Row, Schema, LuaRunner, TypeValidator
- Structs: PascalCase: ScalarMetadata, GroupMetadata, TableDefinition, ColumnDefinition

**Functions:**

- C++: snake_case: `create_element()`, `read_scalar_integers()`, `from_schema()`
- C: snake_case with `quiver_` prefix: `quiver_database_create_element()`, `quiver_database_close()`
- Helper functions: snake_case, often in anonymous namespaces

**Variables:**

- Local/member: snake_case: `impl_`, `logger`, `db_path`
- Constants: UPPER_CASE: `QUIVER_OK`, `QUIVER_LOG_INFO`
- Macros: UPPER_CASE: `QUIVER_REQUIRE`, `QUIVER_API`

**Types:**

- C++ types: PascalCase: `Database`, `DataType`, `DatabaseOptions`
- C opaque types: snake_case_t: `quiver_database_t`, `quiver_element_t`
- C enums: UPPER_CASE: `QUIVER_LOG_DEBUG`, `QUIVER_DATA_TYPE_INTEGER`

## Where to Add New Code

**New Feature (e.g., new CRUD operation):**
- Primary code: `include/quiver/database.h` (method declaration)
- Implementation: `src/database.cpp` (operation logic in Impl or public method)
- C API wrapper: `include/quiver/c/database.h` (function declaration) + `src/c/database.cpp` (implementation)
- Tests: `tests/test_database_*.cpp` for C++ + `tests/test_c_api_database_*.cpp` for C API
- Bindings: Auto-generated via `bindings/{julia,dart}/generator/generator.bat` from C API

**New Component/Module:**
- If hiding private dependencies (e.g., SQLite, sol2): Use Pimpl pattern
  - Public header: `include/quiver/component_name.h` (Pimpl class with move semantics)
  - Implementation: `src/component_name.cpp` (Impl struct with private members)
  - Example: `include/quiver/database.h` + `src/database.cpp`
- If no private dependencies: Use value type (Rule of Zero)
  - Public header: `include/quiver/component_name.h` (struct/class with default copy/move)
  - Implementation: Inline or `src/component_name.cpp` if complex
  - Example: `include/quiver/element.h`, `include/quiver/attribute_metadata.h`

**Utilities:**
- Shared helpers for C++ library: `src/*.cpp` (in anonymous namespace or static)
- Shared helpers for C API: `src/c/*.cpp` (in anonymous namespace)
- Example: Template helpers in `src/database.cpp` lines 40-77, `src/c/database.cpp` lines 12-76

**Schema/Migration:**
- SQL files: `tests/schemas/valid/` (for test usage)
- No production schema files (database created from provided SQL or migrations)
- Migration tracking: Configuration table with version field

**Tests:**
- C++ tests: `tests/test_[feature].cpp` using GTest framework
- C API mirror tests: `tests/test_c_api_[feature].cpp` with identical scenarios
- Test schemas: `tests/schemas/valid/[name].sql` referenced via VALID_SCHEMA macro
- Invalid schemas: `tests/schemas/invalid/[name].sql` for validator testing

## Special Directories

**`build/`:**
- Purpose: Build artifacts (generated during cmake build)
- Generated: Yes, created by CMake
- Committed: No (in .gitignore)
- Contains: bin/, lib/, CMakeFiles/

**`bindings/{language}/.dart_tool/`, `bindings/julia/.artifacts/`:**
- Purpose: Language-specific build caches
- Generated: Yes, by language build systems
- Committed: No
- Should be cleared if binding build fails

**`cmake/`:**
- Purpose: CMake helper modules
- Contains: CompilerOptions.cmake, Dependencies.cmake, Platform.cmake
- Examples: Sets C++ standard, adds spdlog/sqlite3 dependencies

**`.planning/codebase/`:**
- Purpose: Generated codebase analysis documents (this directory)
- Generated: Yes, by GSD orchestrator
- Committed: Yes, for reference by other GSD commands
- Contains: ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, CONCERNS.md

**`docs/`:**
- Purpose: Project documentation (if any)
- Status: Minimal, project-level instructions in CLAUDE.md

