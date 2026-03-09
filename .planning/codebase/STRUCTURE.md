# Codebase Structure

**Analysis Date:** 2026-03-08

## Directory Layout

```
quiver4/
├── include/quiver/          # C++ public headers (the library's API surface)
│   ├── database.h           # Database class - primary API
│   ├── element.h            # Element builder
│   ├── attribute_metadata.h # ScalarMetadata, GroupMetadata
│   ├── options.h            # DatabaseOptions, CSVOptions
│   ├── value.h              # Value variant type
│   ├── schema.h             # Schema, TableDefinition, ColumnDefinition
│   ├── schema_validator.h   # SchemaValidator
│   ├── type_validator.h     # TypeValidator
│   ├── migration.h          # Migration class
│   ├── migrations.h         # Migrations collection
│   ├── lua_runner.h         # LuaRunner (Pimpl, hides sol2)
│   ├── result.h             # Result (vector of Row)
│   ├── row.h                # Row (typed value extraction)
│   ├── data_type.h          # DataType enum
│   ├── export.h             # QUIVER_API DLL export macro
│   ├── quiver.h             # Umbrella include
│   ├── c/                   # C API public headers
│   │   ├── common.h         # quiver_error_t, QUIVER_OK/ERROR, QUIVER_C_API
│   │   ├── options.h        # quiver_database_options_t, quiver_csv_options_t
│   │   ├── database.h       # All quiver_database_* function declarations
│   │   ├── element.h        # quiver_element_* function declarations
│   │   └── lua_runner.h     # quiver_lua_runner_* function declarations
│   └── blob/                # Blob subsystem public headers
│       ├── blob.h           # Blob class (Pimpl)
│       ├── blob_csv.h       # BlobCSV class (Pimpl)
│       ├── blob_metadata.h  # BlobMetadata struct + factories
│       ├── dimension.h      # Dimension struct
│       ├── time_properties.h# TimeFrequency, TimeProperties
│       └── time_constants.h # Time dimension size constraints
│       └── c/blob/          # Blob C API headers
│           ├── blob.h
│           ├── blob_csv.h
│           └── blob_metadata.h
├── src/                     # C++ implementation
│   ├── database.cpp                 # Database open/close, factory methods, logging setup
│   ├── database_impl.h              # Database::Impl definition + TransactionGuard (private)
│   ├── database_internal.h          # Internal read templates (read_column_values, read_grouped_values_all)
│   ├── database_create.cpp          # create_element, update_element
│   ├── database_delete.cpp          # delete_element
│   ├── database_read.cpp            # read_scalar_*, read_vector_*, read_set_*, read_element_ids
│   ├── database_update.cpp          # update_scalar/vector/set group operations
│   ├── database_time_series.cpp     # read/update_time_series_group, time_series_files
│   ├── database_metadata.cpp        # get/list scalar/group metadata
│   ├── database_query.cpp           # query_string/integer/float
│   ├── database_csv_export.cpp      # export_csv
│   ├── database_csv_import.cpp      # import_csv
│   ├── database_describe.cpp        # describe()
│   ├── element.cpp                  # Element implementation
│   ├── lua_runner.cpp               # LuaRunner (Pimpl impl, embeds sol2/Lua)
│   ├── schema.cpp                   # Schema::from_database, table classification
│   ├── schema_validator.cpp         # SchemaValidator::validate()
│   ├── type_validator.cpp           # TypeValidator
│   ├── migration.cpp                # Migration
│   ├── migrations.cpp               # Migrations
│   ├── result.cpp                   # Result
│   ├── row.cpp                      # Row
│   ├── c/                           # C API implementation
│   │   ├── internal.h               # Shared opaque structs (quiver_database, quiver_element, etc.) + QUIVER_REQUIRE macro
│   │   ├── database_helpers.h       # Marshaling templates, string helpers, metadata converters
│   │   ├── database_options.h       # Option converters (C → C++)
│   │   ├── options.cpp              # quiver_database_options_default, quiver_csv_options_default
│   │   ├── database.cpp             # Lifecycle: open, close, from_schema, from_migrations, describe
│   │   ├── database_create.cpp      # create_element, update_element
│   │   ├── database_delete.cpp      # delete_element
│   │   ├── database_read.cpp        # read_scalar/vector/set + free functions
│   │   ├── database_update.cpp      # update scalar/vector/set
│   │   ├── database_metadata.cpp    # get/list metadata + free functions
│   │   ├── database_query.cpp       # query_string/integer/float + params variants
│   │   ├── database_time_series.cpp # time series read/update + free functions
│   │   ├── database_transaction.cpp # begin/commit/rollback/in_transaction
│   │   ├── database_csv_export.cpp  # export_csv
│   │   ├── database_csv_import.cpp  # import_csv
│   │   ├── element.cpp              # quiver_element_* lifecycle
│   │   ├── lua_runner.cpp           # quiver_lua_runner_* lifecycle
│   │   ├── common.cpp               # quiver_get/set/clear_last_error, quiver_version
│   │   └── blob/                    # Blob C API implementation
│   │       ├── blob.cpp
│   │       ├── blob_csv.cpp
│   │       └── blob_metadata.cpp
│   ├── blob/                        # Blob C++ implementation
│   │   ├── blob.cpp                 # Blob Pimpl impl
│   │   ├── blob_csv.cpp             # BlobCSV Pimpl impl
│   │   ├── blob_metadata.cpp        # BlobMetadata factories, serialization, validation
│   │   ├── blob_utils.h             # Internal blob utilities
│   │   ├── dimension.cpp            # Dimension helpers
│   │   └── time_properties.cpp      # TimeFrequency string conversion
│   ├── utils/
│   │   ├── string.h                 # quiver::string::new_c_str(), trim()
│   │   └── datetime.h               # DateTime parsing/formatting utilities
│   └── cli/
│       └── main.cpp                 # quiver_cli entry point
├── tests/                   # C++ tests (Catch2)
│   ├── test_database_*.cpp          # Core C++ API tests
│   ├── test_c_api_database_*.cpp    # C API tests
│   ├── test_blob*.cpp               # Blob C++ tests
│   ├── test_c_api_blob*.cpp         # Blob C API tests
│   ├── test_utils.h                 # Shared test helpers
│   ├── benchmark/
│   │   └── benchmark.cpp            # Transaction performance benchmark (manual run only)
│   ├── sandbox/                     # Scratch files, not part of test suite
│   └── schemas/
│       ├── valid/                   # SQL schemas used by tests (all languages reference these)
│       ├── invalid/                 # Schemas expected to fail validation
│       ├── migrations/              # Migration directories for migration tests
│       └── issues/                  # Schemas for specific regression tests
├── bindings/
│   ├── julia/
│   │   ├── src/
│   │   │   ├── Quiver.jl            # Module entry point; includes all sub-files
│   │   │   ├── c_api.jl             # Auto-generated C API declarations
│   │   │   ├── database.jl          # Database struct + lifecycle
│   │   │   ├── database_create.jl   # create_element!, update_element!
│   │   │   ├── database_read.jl     # read_scalar_*, read_vector_*, composites
│   │   │   ├── database_update.jl   # update operations
│   │   │   ├── database_delete.jl   # delete_element!
│   │   │   ├── database_metadata.jl # get/list metadata
│   │   │   ├── database_query.jl    # query_string/integer/float
│   │   │   ├── database_csv_*.jl    # CSV export/import
│   │   │   ├── database_transaction.jl # begin/commit/rollback + transaction() block
│   │   │   ├── database_options.jl  # DatabaseOptions, CSVOptions
│   │   │   ├── element.jl           # Element builder
│   │   │   ├── lua_runner.jl        # LuaRunner wrapper
│   │   │   ├── exceptions.jl        # DatabaseException
│   │   │   ├── date_time.jl         # DateTime convenience wrappers
│   │   │   ├── helper_maps.jl       # read_scalars_by_id, read_vectors_by_id, read_sets_by_id composites
│   │   │   └── blob/
│   │   │       ├── blob.jl          # Blob wrapper
│   │   │       ├── blob_csv.jl      # BlobCSV wrapper
│   │   │       └── blob_metadata.jl # BlobMetadata wrapper
│   │   ├── test/
│   │   │   ├── runtests.jl          # Test runner
│   │   │   ├── fixture.jl           # Shared test fixtures
│   │   │   ├── test_database_*.jl   # Per-feature test files
│   │   │   ├── test_blob*.jl        # Blob tests
│   │   │   └── test.bat / test.sh   # Test runner scripts
│   │   └── generator/
│   │       └── generator.bat        # Regenerate c_api.jl from C headers
│   ├── dart/
│   │   ├── lib/src/
│   │   │   ├── database.dart        # Database class (uses `part` files)
│   │   │   ├── database_create.dart # createElement, updateElement
│   │   │   ├── database_read.dart   # readScalar*, readVector*, composites
│   │   │   ├── database_update.dart # update operations
│   │   │   ├── database_delete.dart # deleteElement
│   │   │   ├── database_metadata.dart # getScalarMetadata, listVectorGroups, etc.
│   │   │   ├── database_query.dart  # queryString/Integer/Float
│   │   │   ├── database_csv_*.dart  # CSV export/import
│   │   │   ├── database_transaction.dart # transaction block + begin/commit/rollback
│   │   │   ├── database_options.dart # DatabaseOptions, CSVOptions
│   │   │   ├── element.dart         # Element builder
│   │   │   ├── lua_runner.dart      # LuaRunner wrapper
│   │   │   ├── metadata.dart        # ScalarMetadata, GroupMetadata
│   │   │   ├── exceptions.dart      # QuiverException
│   │   │   ├── date_time.dart       # DateTime convenience wrappers
│   │   │   └── ffi/
│   │   │       ├── bindings.dart    # Auto-generated FFI bindings
│   │   │       └── library_loader.dart # DLL loading logic
│   │   ├── test/                    # Dart tests
│   │   └── generator/
│   │       └── generator.bat        # Regenerate bindings.dart from C headers
│   └── python/
│       ├── src/quiverdb/
│       │   ├── __init__.py          # Package exports
│       │   ├── _c_api.py            # Hand-written CFFI cdef declarations
│       │   ├── _declarations.py     # Generator output (reference only)
│       │   ├── _loader.py           # DLL pre-loading (Windows dependency chain)
│       │   ├── _helpers.py          # check(), decode_string()
│       │   ├── database.py          # Database class
│       │   ├── database_csv_export.py # exportCSV
│       │   ├── database_csv_import.py # importCSV
│       │   ├── database_options.py  # DatabaseOptions, CSVOptions
│       │   ├── element.py           # Element (internal; Python users use **kwargs)
│       │   ├── lua_runner.py        # LuaRunner wrapper
│       │   ├── metadata.py          # ScalarMetadata, GroupMetadata, DataType
│       │   └── exceptions.py        # QuiverError
│       ├── tests/                   # Python tests (pytest)
│       └── generator/
│           └── generator.bat        # Regenerate _declarations.py from C headers
├── cmake/                   # CMake modules
│   ├── CompilerOptions.cmake
│   ├── Dependencies.cmake
│   └── Platform.cmake
├── scripts/                 # Build and test automation
│   ├── build-all.bat        # Build everything (Debug or Release)
│   └── test-all.bat         # Run all tests
├── docs/                    # Documentation
├── example/                 # Example code
├── CMakeLists.txt           # Root build configuration
├── CMakePresets.json        # CMake presets
└── CLAUDE.md                # Project instructions
```

## Directory Purposes

**`include/quiver/`:**
- Purpose: Public C++ API headers — the library's contract
- Contains: Class declarations, value types, enums; no implementation, no private dependencies
- Key files: `database.h`, `element.h`, `options.h`, `value.h`, `schema.h`

**`include/quiver/c/`:**
- Purpose: Public C API headers for FFI consumers
- Contains: Opaque handle typedefs, all `quiver_*` function declarations, C-compatible structs
- Key files: `database.h`, `options.h`, `common.h`

**`src/` (root level):**
- Purpose: C++ implementation files for the core library
- Contains: One `.cpp` per logical concern; private headers `database_impl.h` and `database_internal.h`
- Key files: `database.cpp`, `database_impl.h`, `database_internal.h`, `schema.cpp`

**`src/c/`:**
- Purpose: C API implementation — thin delegation from C functions to C++ methods
- Contains: `internal.h` (opaque struct definitions + `QUIVER_REQUIRE` macro), `database_helpers.h` (marshaling templates)
- Key files: `internal.h`, `database_helpers.h`, `database_options.h`, `options.cpp`

**`src/blob/` and `include/quiver/blob/`:**
- Purpose: Standalone blob binary file subsystem — independent of the database layer
- Contains: Pimpl implementations for `Blob` and `BlobCSV`; plain value types for metadata/dimensions

**`src/utils/`:**
- Purpose: Shared internal utilities (not part of public API)
- Contains: `string.h` (null-terminated C string allocation), `datetime.h` (date/time formatting)

**`tests/`:**
- Purpose: All C++ and C API tests; Catch2 framework
- Contains: One test file per feature area; shared test helpers in `test_utils.h`; all SQL schemas in `tests/schemas/`

**`tests/schemas/valid/`:**
- Purpose: SQL schemas referenced by tests across all languages (C++, C API, Julia, Dart, Python)
- Contains: `.sql` files for all test scenarios
- Key files: `basic.sql`, `all_types.sql`, `collections.sql`, `relations.sql`, `csv_export.sql`

**`bindings/{lang}/generator/`:**
- Purpose: Scripts to regenerate auto-generated FFI bindings from C headers
- Generated: `c_api.jl` (Julia), `bindings.dart` (Dart), `_declarations.py` (Python reference)
- Run when: C API headers change

## Key File Locations

**Entry Points:**
- `src/cli/main.cpp`: CLI executable entry point
- `include/quiver/quiver.h`: Umbrella C++ header
- `include/quiver/c/database.h`: Full C API surface

**Configuration:**
- `CMakeLists.txt`: Root build; controls `QUIVER_BUILD_SHARED`, `QUIVER_BUILD_TESTS`, `QUIVER_BUILD_C_API`
- `CMakePresets.json`: Build presets
- `cmake/Dependencies.cmake`: Dependency resolution

**Core Logic:**
- `src/database_impl.h`: `Database::Impl` struct — owns sqlite3 handle, schema, logger, `TransactionGuard`
- `src/database_internal.h`: Internal read templates shared across database read/query implementations
- `src/schema.cpp`: `Schema::from_database()` — introspects SQLite schema at open time
- `src/c/internal.h`: Opaque struct definitions and `QUIVER_REQUIRE` null-check macro
- `src/c/database_helpers.h`: C↔C++ marshaling templates for all numeric and metadata types

**Testing:**
- `tests/test_utils.h`: Shared Catch2 test helpers and fixtures
- `tests/schemas/valid/`: SQL schemas used across all test suites
- `bindings/julia/test/fixture.jl`: Shared Julia test fixtures

## Naming Conventions

**Files:**
- C++ implementation: `database_{concern}.cpp` (e.g. `database_read.cpp`, `database_time_series.cpp`)
- C API implementation: same pattern under `src/c/` (e.g. `src/c/database_read.cpp`)
- Test files: `test_{layer}_{concern}.cpp` (e.g. `test_c_api_database_metadata.cpp`, `test_database_lifecycle.cpp`)
- Julia sources: `database_{concern}.jl`, `blob/{concern}.jl`
- Dart sources: `database{Concern}.dart` (camelCase file names)
- Python sources: `database_{concern}.py`

**Directories:**
- Feature subdirectories mirror the layer they implement: `src/c/blob/` mirrors `include/quiver/c/blob/`

**C++ symbols:** `snake_case` in `namespace quiver`
**C API symbols:** `quiver_{entity}_{verb}[_noun]` (e.g. `quiver_database_read_scalar_integers`)
**Julia symbols:** same as C++ with `!` suffix for mutating operations
**Dart symbols:** `camelCase` methods; `PascalCase` classes; factory constructors `ClassName.fromX()`
**Python symbols:** same `snake_case` as C++; factory methods are `@staticmethod`

## Where to Add New Code

**New C++ database operation:**
1. Add declaration to `include/quiver/database.h`
2. Implement in the appropriate `src/database_{concern}.cpp` (or create `src/database_{newconcern}.cpp`)
3. Add C API declaration to `include/quiver/c/database.h`
4. Implement in `src/c/database_{concern}.cpp` (co-locate alloc/free in same file)
5. Add Julia binding in `bindings/julia/src/database_{concern}.jl`
6. Add Dart binding in `bindings/dart/lib/src/database_{concern}.dart`
7. Add Python binding in `bindings/python/src/quiverdb/database_{concern}.py` or `database.py`
8. Add tests: `tests/test_database_{concern}.cpp` and `tests/test_c_api_database_{concern}.cpp`

**New blob operation:**
1. Add to `include/quiver/blob/blob.h` or `include/quiver/blob/blob_metadata.h`
2. Implement in `src/blob/blob.cpp` or `src/blob/blob_metadata.cpp`
3. Add C API to `include/quiver/c/blob/blob.h`
4. Implement in `src/c/blob/blob.cpp`
5. Add Julia wrapper in `bindings/julia/src/blob/blob.jl`
6. Add test in `tests/test_blob*.cpp` and `tests/test_c_api_blob*.cpp`

**New SQL test schema:**
- Add `.sql` file to `tests/schemas/valid/`
- Reference from test files using a relative path from the test binary working directory

**New utilities:**
- Shared C++ helpers used only internally: `src/utils/string.h` or `src/utils/datetime.h`
- C API marshaling helpers: `src/c/database_helpers.h`

## Special Directories

**`build/`:**
- Purpose: CMake build output — binaries, object files, generated files
- Generated: Yes
- Committed: No

**`bindings/python/.venv/`:**
- Purpose: Python virtual environment for test dependencies
- Generated: Yes
- Committed: No

**`bindings/julia/generator/`:**
- Purpose: Tool to regenerate `c_api.jl` from C headers; run when C API changes
- Generated: No (script itself is committed)
- Committed: Yes

**`tests/schemas/`:**
- Purpose: Shared SQL schema files; all language test suites reference these — do not split or relocate
- Generated: No
- Committed: Yes

**`.planning/codebase/`:**
- Purpose: GSD analysis documents consumed by plan and execute commands
- Generated: Yes (by codebase mapper)
- Committed: Depends on project policy

---

*Structure analysis: 2026-03-08*
