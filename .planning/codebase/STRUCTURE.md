# Codebase Structure

**Analysis Date:** 2026-02-27

## Directory Layout

```
quiver4/
├── include/
│   └── quiver/               # C++ public headers (namespace quiver)
│       ├── database.h         # Database class -- primary public API
│       ├── element.h          # Element builder for create/update
│       ├── attribute_metadata.h # ScalarMetadata, GroupMetadata types
│       ├── options.h          # DatabaseOptions, CSVOptions
│       ├── value.h            # Value = variant<nullptr_t, int64_t, double, string>
│       ├── lua_runner.h       # LuaRunner class
│       ├── schema.h           # Schema, TableDefinition, ColumnDefinition
│       ├── result.h           # Result type (vector of Row)
│       ├── row.h              # Row type
│       ├── data_type.h        # DataType enum
│       ├── migration.h        # Migration type
│       ├── migrations.h       # Migrations collection
│       ├── schema_validator.h # SchemaValidator
│       ├── type_validator.h   # TypeValidator
│       ├── export.h           # QUIVER_API export macro
│       └── c/                 # C API public headers (for FFI)
│           ├── common.h       # QUIVER_C_API macro, quiver_error_t, version/error utils
│           ├── options.h      # quiver_database_options_t, quiver_csv_options_t, log levels
│           ├── database.h     # All database C API declarations
│           ├── element.h      # quiver_element_t C API
│           └── lua_runner.h   # LuaRunner C API
├── src/
│   ├── database.cpp           # Database lifecycle: open, close, factory, logging setup
│   ├── database_impl.h        # Database::Impl definition, TransactionGuard, ResolvedElement
│   ├── database_internal.h    # Internal helpers shared across database_*.cpp files
│   ├── database_create.cpp    # create_element implementation
│   ├── database_read.cpp      # read_scalar/vector/set/element_ids implementations
│   ├── database_update.cpp    # update_element, update_scalar/vector/set/time_series
│   ├── database_delete.cpp    # delete_element implementation
│   ├── database_query.cpp     # query_string/integer/float implementations
│   ├── database_metadata.cpp  # get/list scalar/group metadata implementations
│   ├── database_time_series.cpp # read/update time series + files implementations
│   ├── database_describe.cpp  # describe() schema inspection implementation
│   ├── database_csv_export.cpp # CSV export implementation
│   ├── database_csv_import.cpp # CSV import implementation
│   ├── element.cpp            # Element builder implementation
│   ├── lua_runner.cpp         # LuaRunner (Lua state, bindings to Database)
│   ├── schema.cpp             # Schema loading from SQLite, table classification
│   ├── schema_validator.cpp   # Structural schema validation rules
│   ├── type_validator.cpp     # Value type validation against schema
│   ├── migration.cpp          # Migration loading
│   ├── migrations.cpp         # Migrations collection
│   ├── result.cpp             # Result/Row implementation
│   ├── row.cpp                # Row implementation
│   ├── common.cpp             # Shared C API utility (version, error string)
│   ├── blob/                  # Blob/time series internal utilities
│   │   ├── blob.cpp
│   │   ├── blob_csv.cpp
│   │   ├── blob_metadata.cpp
│   │   ├── dimension.cpp
│   │   └── time_properties.cpp
│   ├── utils/
│   │   ├── datetime.h         # DateTime utilities
│   │   └── string.h           # String utilities
│   ├── cli/
│   │   └── main.cpp           # quiver_cli entry point (argparse + LuaRunner)
│   └── c/                     # C API implementation
│       ├── internal.h         # quiver_database struct, quiver_element struct, QUIVER_REQUIRE macro
│       ├── database_helpers.h # read_scalars_impl<T>, read_vectors_impl<T>, strdup_safe, convert_scalar_to_c, convert_group_to_c
│       ├── database_options.h # convert_csv_options helper, quiver_csv_options_default
│       ├── database.cpp       # Lifecycle: open, close, from_schema, from_migrations, describe
│       ├── database_create.cpp # create_element, update_element, delete_element
│       ├── database_read.cpp  # read_scalar/vector/set C API + free functions
│       ├── database_update.cpp # update_scalar/vector/set C API
│       ├── database_delete.cpp # delete_element C API
│       ├── database_metadata.cpp # get/list metadata C API + free functions
│       ├── database_query.cpp # query_string/integer/float + parameterized variants
│       ├── database_time_series.cpp # read/update time series group C API + free functions
│       ├── database_transaction.cpp # begin/commit/rollback/in_transaction C API
│       ├── database_csv_export.cpp # export_csv C API
│       ├── database_csv_import.cpp # import_csv C API
│       ├── element.cpp        # quiver_element_t lifecycle + set operations
│       └── lua_runner.cpp     # LuaRunner C API
├── bindings/
│   ├── julia/
│   │   ├── src/
│   │   │   ├── Quiver.jl      # Module entry point, exports
│   │   │   ├── c_api.jl       # FFI declarations (module C), ccall wrappers
│   │   │   ├── database.jl    # Database struct, from_schema, from_migrations, close!
│   │   │   ├── database_create.jl
│   │   │   ├── database_read.jl
│   │   │   ├── database_update.jl
│   │   │   ├── database_delete.jl
│   │   │   ├── database_metadata.jl
│   │   │   ├── database_query.jl
│   │   │   ├── database_transaction.jl
│   │   │   ├── database_csv_export.jl
│   │   │   ├── database_csv_import.jl
│   │   │   ├── database_options.jl
│   │   │   ├── element.jl
│   │   │   ├── exceptions.jl  # DatabaseException
│   │   │   ├── date_time.jl   # DateTime wrappers (read_scalar_date_time_by_id etc.)
│   │   │   ├── helper_maps.jl # read_scalars_by_id, read_vectors_by_id, read_sets_by_id
│   │   │   └── lua_runner.jl
│   │   ├── test/
│   │   │   └── test.bat       # Julia test runner
│   │   └── generator/
│   │       └── generator.bat  # Regenerates c_api.jl from C headers
│   ├── dart/
│   │   ├── lib/src/
│   │   │   ├── database.dart  # Database class (assembles parts)
│   │   │   ├── database_create.dart
│   │   │   ├── database_read.dart
│   │   │   ├── database_update.dart
│   │   │   ├── database_delete.dart
│   │   │   ├── database_metadata.dart
│   │   │   ├── database_query.dart
│   │   │   ├── database_transaction.dart
│   │   │   ├── database_csv_export.dart
│   │   │   ├── database_csv_import.dart
│   │   │   ├── database_options.dart
│   │   │   ├── element.dart
│   │   │   ├── exceptions.dart # QuiverException
│   │   │   ├── date_time.dart  # DateTime wrappers
│   │   │   ├── lua_runner.dart
│   │   │   └── ffi/
│   │   │       ├── bindings.dart       # Generated FFI bindings
│   │   │       └── library_loader.dart # DLL/SO loading
│   │   ├── test/
│   │   │   └── test.bat
│   │   └── generator/
│   │       └── generator.bat  # Regenerates ffi/bindings.dart from C headers
│   └── python/
│       ├── src/quiverdb/
│       │   ├── __init__.py    # Package entry point, public exports
│       │   ├── _c_api.py      # CFFI hand-written cdef declarations (authoritative)
│       │   ├── _declarations.py # Generator output (reference only)
│       │   ├── _loader.py     # DLL pre-loading on Windows
│       │   ├── _helpers.py    # check(), decode_string(), decode_string_or_none()
│       │   ├── database.py    # Database class (extends DatabaseCSVExport, DatabaseCSVImport)
│       │   ├── database_csv_export.py
│       │   ├── database_csv_import.py
│       │   ├── database_options.py
│       │   ├── element.py     # Internal Element builder (not public API)
│       │   ├── exceptions.py  # QuiverError
│       │   └── metadata.py    # ScalarMetadata, GroupMetadata
│       ├── tests/
│       └── generator/
│           └── generator.bat  # Regenerates _declarations.py from C headers
├── tests/
│   ├── schemas/
│   │   ├── valid/             # Shared SQL schemas for all tests
│   │   │   ├── basic.sql
│   │   │   ├── all_types.sql
│   │   │   ├── collections.sql
│   │   │   ├── csv_export.sql
│   │   │   ├── relations.sql
│   │   │   ├── composite_helpers.sql
│   │   │   ├── multi_time_series.sql
│   │   │   └── mixed_time_series.sql
│   │   ├── invalid/           # Schemas that should fail validation
│   │   ├── issues/            # Regression test schemas
│   │   └── migrations/        # Migration directory test fixtures
│   ├── benchmark/             # Transaction benchmark (manual run only)
│   ├── sandbox/               # Scratch test files
│   ├── test_database_lifecycle.cpp
│   ├── test_database_create.cpp
│   ├── test_database_read.cpp
│   ├── test_database_update.cpp
│   ├── test_database_delete.cpp
│   ├── test_database_query.cpp
│   ├── test_database_time_series.cpp
│   ├── test_database_transaction.cpp
│   ├── test_database_csv_export.cpp
│   ├── test_database_csv_import.cpp
│   ├── test_database_errors.cpp
│   ├── test_c_api_database_lifecycle.cpp
│   ├── test_c_api_database_create.cpp
│   ├── test_c_api_database_read.cpp
│   ├── test_c_api_database_update.cpp
│   ├── test_c_api_database_delete.cpp
│   ├── test_c_api_database_query.cpp
│   ├── test_c_api_database_time_series.cpp
│   ├── test_c_api_database_transaction.cpp
│   ├── test_c_api_database_csv_export.cpp
│   ├── test_c_api_database_csv_import.cpp
│   ├── test_c_api_database_metadata.cpp
│   ├── test_c_api_element.cpp
│   ├── test_c_api_lua_runner.cpp
│   ├── test_element.cpp
│   ├── test_lua_runner.cpp
│   ├── test_migrations.cpp
│   ├── test_schema_validator.cpp
│   ├── test_row_result.cpp
│   ├── test_issues.cpp
│   └── test_utils.h
├── cmake/
│   ├── CompilerOptions.cmake  # Warning flags, sanitizer options
│   ├── Dependencies.cmake     # SQLite3, spdlog, Lua, Catch2, argparse fetching
│   ├── Platform.cmake         # Windows/Linux/macOS platform settings
│   └── quiverConfig.cmake.in  # Package config template for find_package()
├── scripts/
│   ├── build-all.bat          # Build everything + run all tests
│   ├── test-all.bat           # Run all tests (assumes built)
│   ├── format.bat             # Run clang-format
│   ├── tidy.bat               # Run clang-tidy
│   ├── generator.bat          # Run all FFI generators
│   └── test-wheel*.bat        # Python wheel build/install tests
├── docs/                      # User-facing documentation (Markdown)
├── example/                   # Example Lua scripts and run scripts
├── CMakeLists.txt             # Root build configuration
├── CMakePresets.json          # CMake preset configurations
└── CLAUDE.md                  # Project instructions and architecture reference
```

## Directory Purposes

**`include/quiver/`:**
- Purpose: Public C++ API headers -- everything a C++ consumer needs
- Contains: All public class declarations, type definitions, factory method signatures
- Key files: `database.h` (main API), `element.h` (builder), `attribute_metadata.h` (metadata types), `value.h` (Value variant), `options.h` (DatabaseOptions, CSVOptions)

**`include/quiver/c/`:**
- Purpose: C ABI headers for FFI consumers
- Contains: All C function declarations, C struct types, enums, error codes
- Key files: `common.h` (error codes, version), `options.h` (C options structs), `database.h` (all database operations)

**`src/`:**
- Purpose: C++ implementation files for the core library
- Contains: Split by functional domain, one `.cpp` per concern (`database_create.cpp`, `database_read.cpp`, etc.)
- Key files: `database_impl.h` (Impl struct, TransactionGuard), `database.cpp` (lifecycle/logging setup)

**`src/c/`:**
- Purpose: C API implementation -- thin pass-through converting C++ exceptions to error codes
- Contains: Mirrors `src/` functional split; also contains marshaling helpers
- Key files: `internal.h` (quiver_database/element structs, QUIVER_REQUIRE macro), `database_helpers.h` (marshal templates)

**`src/blob/`:**
- Purpose: Internal utilities for blob/time series data handling
- Contains: Dimension helpers, time property utilities, blob CSV helpers
- Generated: No

**`src/cli/`:**
- Purpose: CLI executable entry point
- Key files: `main.cpp` (argparse setup, Database + LuaRunner invocation)

**`bindings/julia/src/`:**
- Purpose: Julia FFI wrapper using `ccall`
- Key files: `Quiver.jl` (module root), `c_api.jl` (all ccall declarations in module `C`)

**`bindings/dart/lib/src/`:**
- Purpose: Dart FFI wrapper using `dart:ffi`
- Key files: `database.dart` (main class assembled with `part` directives), `ffi/bindings.dart` (generated), `ffi/library_loader.dart` (DLL loader)

**`bindings/python/src/quiverdb/`:**
- Purpose: Python CFFI ABI-mode wrapper
- Key files: `_c_api.py` (authoritative hand-written cdef declarations), `_loader.py` (DLL pre-loading), `database.py` (Database class)

**`tests/`:**
- Purpose: All C++ and C API tests
- Key files: `test_database_*.cpp` (C++ API), `test_c_api_*.cpp` (C API), `test_utils.h` (shared helpers)

**`tests/schemas/valid/`:**
- Purpose: Shared SQL schemas referenced by all test suites (C++, C API, Julia, Dart, Python)
- Generated: No
- Committed: Yes

## Key File Locations

**Entry Points:**
- `src/cli/main.cpp`: CLI executable
- `bindings/julia/src/Quiver.jl`: Julia module root
- `bindings/dart/lib/src/database.dart`: Dart Database class
- `bindings/python/src/quiverdb/__init__.py`: Python package root

**Public C++ API:**
- `include/quiver/database.h`: Database class declaration
- `include/quiver/element.h`: Element builder
- `include/quiver/attribute_metadata.h`: ScalarMetadata, GroupMetadata
- `include/quiver/options.h`: DatabaseOptions, CSVOptions
- `include/quiver/value.h`: Value variant type

**Public C API:**
- `include/quiver/c/common.h`: Error codes, quiver_error_t, version/error utilities
- `include/quiver/c/options.h`: quiver_database_options_t, quiver_csv_options_t
- `include/quiver/c/database.h`: All database operation declarations

**Core Implementation:**
- `src/database_impl.h`: Database::Impl, TransactionGuard (private, not installed)
- `src/schema.cpp`: Schema introspection from SQLite
- `src/c/internal.h`: QUIVER_REQUIRE macro, quiver_database/element structs
- `src/c/database_helpers.h`: Marshaling templates (read_scalars_impl, read_vectors_impl, strdup_safe)

**Test Schemas:**
- `tests/schemas/valid/`: SQL schemas shared across all test suites

**Configuration:**
- `CMakeLists.txt`: Build targets, options (QUIVER_BUILD_SHARED, QUIVER_BUILD_TESTS, QUIVER_BUILD_C_API)
- `cmake/Dependencies.cmake`: External dependency acquisition
- `CMakePresets.json`: Preset build configurations

## Naming Conventions

**C++ Source Files:**
- `database_{domain}.cpp` -- one file per operational domain (e.g., `database_create.cpp`, `database_read.cpp`)
- `database_impl.h` -- private Impl struct (not installed as public header)
- Plain noun names for types: `element.cpp`, `schema.cpp`, `migration.cpp`

**C API Source Files:**
- Same `database_{domain}.cpp` split as C++ layer, under `src/c/`
- `database_helpers.h` -- shared marshaling templates (not a public header)
- `internal.h` -- shared internal structs (not a public header)

**Test Files:**
- `test_database_{domain}.cpp` -- C++ API tests
- `test_c_api_database_{domain}.cpp` -- C API tests
- `test_{other}.cpp` -- standalone tests for Element, LuaRunner, Schema, etc.

**Schema Tables:**
- Collection: `{CollectionName}` (e.g., `Items`)
- Vector group table: `{Collection}_vector_{name}` (e.g., `Items_vector_values`)
- Set group table: `{Collection}_set_{name}` (e.g., `Items_set_tags`)
- Time series table: `{Collection}_time_series_{name}` (e.g., `Items_time_series_data`)
- Time series files table: `{Collection}_time_series_files`

**Binding Files:**
- Each binding mirrors the C++ functional split: `database_create.{ext}`, `database_read.{ext}`, etc.
- Julia: snake_case files, `!` suffix on mutating functions
- Dart: camelCase methods, `database.dart` assembles all via `part` directives
- Python: snake_case files and methods, inheritance for CSV mixin classes

## Where to Add New Code

**New C++ Database Operation:**
- Add declaration to `include/quiver/database.h`
- Add implementation to the appropriate `src/database_{domain}.cpp` (or create a new one if no domain matches)
- Add C API declaration to `include/quiver/c/database.h`
- Add C API implementation to the matching `src/c/database_{domain}.cpp` (alloc/free pairs stay co-located)
- Add Julia binding to `bindings/julia/src/database_{domain}.jl` (include in `Quiver.jl`)
- Add Dart binding to `bindings/dart/lib/src/database_{domain}.dart` (add `part` in `database.dart`)
- Add Python binding to `bindings/python/src/quiverdb/database.py` or a new mixin file
- Add C++ tests to `tests/test_database_{domain}.cpp`
- Add C API tests to `tests/test_c_api_database_{domain}.cpp`

**New Test Schema:**
- Add `.sql` file to `tests/schemas/valid/`
- Reference from test files using relative path from test executable location

**New Binding Language:**
- Create `bindings/{language}/` directory
- Implement against `include/quiver/c/` headers only
- Add `generator/` subdirectory with FFI binding generator script
- Add `test/test.{bat|sh}` entry point
- Reference test schemas from `tests/schemas/valid/`

**Utilities:**
- Shared C++ helpers: `src/utils/string.h` or `src/utils/datetime.h`
- C API marshaling helpers: `src/c/database_helpers.h`

## Special Directories

**`build/`:**
- Purpose: CMake build output (binaries, libs)
- Generated: Yes
- Committed: No
- Key outputs: `build/bin/quiver_tests.exe`, `build/bin/quiver_c_tests.exe`, `build/bin/quiver_cli.exe`, `build/bin/libquiver_c.dll`, `build/lib/libquiver.dll`

**`tests/schemas/`:**
- Purpose: SQL schema fixtures shared across all test suites in all languages
- Generated: No
- Committed: Yes

**`bindings/{language}/generator/`:**
- Purpose: FFI binding code generators (run when C API changes)
- Generated: No (the generators themselves are committed; their output is also committed)
- Committed: Yes

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents consumed by plan/execute commands
- Generated: Yes (by gsd:map-codebase)
- Committed: Yes

---

*Structure analysis: 2026-02-27*
