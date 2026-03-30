# Structure

## Directory Layout

```
quiver4/
├── include/quiver/              # Public C++ headers
│   ├── database.h               # Database class (main API)
│   ├── element.h                # Element builder
│   ├── options.h                # DatabaseOptions, CSVOptions
│   ├── attribute_metadata.h     # ScalarMetadata, GroupMetadata
│   ├── value.h                  # Value variant type
│   ├── result.h                 # Result/Row query containers
│   ├── schema.h                 # Schema introspection
│   ├── schema_validator.h       # Schema convention validation
│   ├── type_validator.h         # Runtime type checking
│   ├── migration.h              # Migration definitions
│   ├── migrations.h             # Migration runner
│   ├── lua_runner.h             # Lua scripting
│   ├── export.h                 # DLL export macros
│   ├── quiver.h                 # Umbrella header
│   ├── binary/                  # Binary subsystem headers
│   │   ├── binary_file.h        # BinaryFile (Pimpl)
│   │   ├── binary_metadata.h    # BinaryMetadata (value type)
│   │   ├── csv_converter.h      # CSVConverter (Pimpl)
│   │   ├── dimension.h          # Dimension struct
│   │   ├── time_properties.h    # TimeFrequency, TimeProperties
│   │   └── time_constants.h     # Time dimension constraints
│   └── c/                       # C API headers
│       ├── common.h             # Error codes, version
│       ├── database.h           # Database C functions
│       ├── element.h            # Element C functions
│       ├── options.h            # Option types and defaults
│       ├── lua_runner.h         # Lua C functions
│       └── binary/              # Binary C API headers
│           ├── binary_file.h
│           ├── binary_metadata.h
│           └── csv_converter.h
├── src/                         # C++ implementation
│   ├── database.cpp             # Lifecycle, factories, path, describe
│   ├── database_impl.h         # Impl struct, TransactionGuard, helpers
│   ├── database_internal.h     # Internal declarations
│   ├── database_create.cpp      # create_element, update_element
│   ├── database_read.cpp        # All read operations
│   ├── database_update.cpp      # Update scalar/vector/set
│   ├── database_delete.cpp      # delete_element
│   ├── database_metadata.cpp    # Metadata get/list
│   ├── database_time_series.cpp # Time series read/update/files
│   ├── database_query.cpp       # Parameterized queries
│   ├── database_csv_export.cpp  # CSV export
│   ├── database_csv_import.cpp  # CSV import
│   ├── database_describe.cpp    # Schema inspection output
│   ├── element.cpp              # Element builder
│   ├── lua_runner.cpp           # LuaRunner implementation
│   ├── schema.cpp               # Schema::from_database
│   ├── schema_validator.cpp     # Validation rules
│   ├── type_validator.cpp       # Type checking
│   ├── migration.cpp            # Migration parsing
│   ├── migrations.cpp           # Migration execution
│   ├── result.cpp               # Result container
│   ├── row.cpp                  # Row container
│   ├── utils/                   # Internal utilities
│   │   ├── string.h             # new_c_str, trim
│   │   └── datetime.h           # Date/time utilities
│   ├── binary/                  # Binary implementation
│   │   ├── binary_file.cpp
│   │   ├── binary_metadata.cpp
│   │   ├── csv_converter.cpp
│   │   ├── dimension.cpp
│   │   ├── time_properties.cpp
│   │   └── binary_utils.h       # Internal binary utilities
│   ├── c/                       # C API implementation
│   │   ├── internal.h           # Opaque structs, QUIVER_REQUIRE macro
│   │   ├── database_helpers.h   # Marshaling, metadata converters
│   │   ├── database_options.h   # Option converters
│   │   ├── common.cpp           # Error storage, version
│   │   ├── options.cpp          # Default options
│   │   ├── database.cpp         # Lifecycle, factories
│   │   ├── database_create.cpp  # Element CRUD
│   │   ├── database_read.cpp    # Read + free functions
│   │   ├── database_update.cpp  # Update operations
│   │   ├── database_delete.cpp  # Delete operations
│   │   ├── database_metadata.cpp # Metadata + free functions
│   │   ├── database_query.cpp   # Query operations
│   │   ├── database_time_series.cpp # Time series + free functions
│   │   ├── database_transaction.cpp # Transaction control
│   │   ├── database_csv_export.cpp
│   │   ├── database_csv_import.cpp
│   │   ├── element.cpp          # Element C API
│   │   ├── lua_runner.cpp       # Lua C API
│   │   └── binary/              # Binary C API
│   │       ├── binary_file.cpp
│   │       ├── binary_metadata.cpp
│   │       └── csv_converter.cpp
│   └── cli/main.cpp             # CLI executable
├── tests/                       # C++ and C API tests
│   ├── CMakeLists.txt
│   ├── test_database_*.cpp      # C++ core tests (12 files)
│   ├── test_c_api_database_*.cpp # C API tests (13 files)
│   ├── test_c_api_binary_*.cpp  # Binary C API tests (3 files)
│   ├── test_binary_*.cpp        # Binary C++ tests (3 files)
│   ├── test_element.cpp         # Element builder tests
│   ├── test_issues.cpp          # Regression tests
│   ├── test_lua_runner.cpp      # Lua integration tests
│   ├── test_migrations.cpp      # Migration tests
│   ├── test_row_result.cpp      # Result container tests
│   ├── test_schema_validator.cpp # Schema validation tests
│   ├── test_csv_converter.cpp   # CSV converter tests
│   ├── schemas/                 # Test SQL schemas
│   │   ├── valid/               # 9 valid schemas
│   │   └── invalid/             # 10 invalid schemas
│   ├── benchmark/benchmark.cpp  # Transaction benchmark
│   └── sandbox/                 # Scratch/experiment area
├── bindings/
│   ├── julia/                   # Julia binding
│   │   ├── src/                 # Source (Quiver.jl + per-domain files)
│   │   │   └── binary/          # Binary Julia wrappers
│   │   ├── test/                # Tests (21 files)
│   │   └── generator/           # FFI generator
│   ├── dart/                    # Dart binding
│   │   ├── lib/src/             # Source (per-domain files)
│   │   │   └── ffi/             # FFI declarations
│   │   └── test/                # Tests (18 files)
│   └── python/                  # Python binding
│       ├── src/quiverdb/        # Source (CFFI ABI-mode)
│       ├── tests/               # Tests (15 files)
│       └── generator/           # Declaration generator
├── cmake/                       # CMake modules
│   ├── CompilerOptions.cmake    # Warning flags, platform settings
│   ├── Dependencies.cmake       # FetchContent declarations
│   └── Platform.cmake           # Platform detection
└── scripts/                     # Build/test automation
    ├── build-all.bat/.sh
    └── test-all.bat/.sh
```

## Naming Conventions

### Files
- C++ source: `snake_case.cpp` / `snake_case.h`
- C API source: `src/c/database_{domain}.cpp` mirrors `src/database_{domain}.cpp`
- Tests: `test_{domain}.cpp` (C++), `test_c_api_{domain}.cpp` (C API)
- Julia: `{domain}.jl`, tests as `test_{domain}.jl`
- Dart: `{domain}.dart`, tests as `{domain}_test.dart`
- Python: `{domain}.py`, tests as `test_{domain}.py`

### Code
- C++ namespaces: `quiver`, `quiver::string`
- C API functions: `quiver_database_{verb}_{type}()`, `quiver_binary_{entity}_{verb}()`
- Macros: `QUIVER_` prefix (e.g., `QUIVER_REQUIRE`, `QUIVER_API`)
- Types: PascalCase (`Database`, `Element`, `BinaryMetadata`)
- Methods: snake_case (`read_scalar_integers`, `create_element`)

## Key Locations

| What | Where |
|------|-------|
| Main API surface | `include/quiver/database.h` |
| Implementation details | `src/database_impl.h` |
| C API contracts | `include/quiver/c/` |
| C API internals | `src/c/internal.h`, `src/c/database_helpers.h` |
| Test schemas | `tests/schemas/valid/`, `tests/schemas/invalid/` |
| Build config | `CMakeLists.txt`, `cmake/` |
| FFI generators | `bindings/*/generator/` |
