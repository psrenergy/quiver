# Codebase Structure

**Analysis Date:** 2026-02-23

## Directory Layout

```
quiver/
├── include/quiver/                 # C++ public headers
│   ├── c/                          # C API headers (FFI)
│   │   ├── common.h                # Error codes, logging levels
│   │   ├── database.h              # All database C API functions
│   │   ├── element.h               # Element builder C API
│   │   ├── options.h               # DatabaseOptions, CSVExportOptions C structs
│   │   └── lua_runner.h            # Lua runner C API
│   ├── blob/                       # Blob/time series related headers
│   │   ├── blob.h
│   │   ├── blob_csv.h
│   │   ├── blob_metadata.h
│   │   ├── dimension.h
│   │   ├── time_constants.h
│   │   └── time_properties.h
│   ├── attribute_metadata.h        # ScalarMetadata, GroupMetadata structs
│   ├── database.h                  # Database class (main C++ API)
│   ├── data_type.h                 # DataType enum
│   ├── element.h                   # Element builder class
│   ├── export.h                    # Symbol visibility macros
│   ├── lua_runner.h                # LuaRunner class
│   ├── migration.h                 # Migration schema builder
│   ├── migrations.h                # Migrations container
│   ├── options.h                   # DatabaseOptions, CSVExportOptions C++ types
│   ├── quiver.h                    # Master include (all public APIs)
│   ├── result.h                    # Query Result container
│   ├── row.h                       # Row accessor for Result
│   ├── schema.h                    # Schema parser and table definitions
│   ├── schema_validator.h          # Schema validation logic
│   ├── type_validator.h            # Type checking for values
│   └── value.h                     # Value variant type (nullptr, int64, double, string)
│
├── src/                            # C++ implementation
│   ├── c/                          # C API implementation (one file per category)
│   │   ├── internal.h              # Shared structs (quiver_database, quiver_element)
│   │   ├── database_helpers.h      # Marshaling templates, string handling
│   │   ├── common.cpp              # Error message thread-local storage
│   │   ├── database.cpp            # Lifecycle: open, close, from_schema, from_migrations
│   │   ├── database_create.cpp     # Element CRUD: create, update, delete
│   │   ├── database_read.cpp       # Scalar/vector/set reads + free functions
│   │   ├── database_update.cpp     # Scalar/vector/set updates
│   │   ├── database_metadata.cpp   # Metadata get/list + free functions
│   │   ├── database_query.cpp      # Parameterized and plain queries
│   │   ├── database_time_series.cpp # Time series read/update + free functions
│   │   ├── database_relations.cpp  # Foreign key relations
│   │   ├── database_transaction.cpp # begin/commit/rollback
│   │   ├── database_csv.cpp        # CSV export options conversion
│   │   ├── element.cpp             # Element builder implementation
│   │   └── lua_runner.cpp          # Lua runner FFI bindings
│   │
│   ├── blob/                       # Blob/time series implementation
│   │   └── (implementation details)
│   │
│   ├── database.cpp                # Database constructor, factories
│   ├── database_impl.h             # Pimpl: Impl struct, TransactionGuard, schema/logger
│   ├── database_internal.h         # (internal utilities)
│   ├── database_create.cpp         # create_element: scalars + arrays + transactions
│   ├── database_csv.cpp            # CSV export with enum/date formatting
│   ├── database_describe.cpp       # describe() schema printing
│   ├── database_delete.cpp         # delete_element
│   ├── database_metadata.cpp       # get/list scalar/vector/set/time-series metadata
│   ├── database_query.cpp          # query_string/integer/float with params
│   ├── database_read.cpp           # read_scalar/vector/set (all + by_id)
│   ├── database_relations.cpp      # update/read scalar foreign key relations
│   ├── database_time_series.cpp    # read/update time series groups
│   ├── database_update.cpp         # update_scalar/vector/set (by element ID)
│   ├── element.cpp                 # Element::set() methods, to_string()
│   ├── migration.cpp               # Migration builder
│   ├── migrations.cpp              # Migrations container
│   ├── result.cpp                  # Result container
│   ├── row.cpp                     # Row accessor
│   ├── schema.cpp                  # Schema::from_database() parser
│   ├── schema_validator.cpp        # SchemaValidator::validate()
│   ├── type_validator.cpp          # TypeValidator checks
│   ├── lua_runner.cpp              # LuaRunner implementation (51KB, largest file)
│   └── CMakeLists.txt              # C++ build configuration
│
├── bindings/                       # Language bindings
│   ├── julia/                      # Julia binding (Quiver.jl)
│   │   ├── src/                    # Julia source files
│   │   │   ├── Quiver.jl           # Module root, exports Database
│   │   │   ├── c_api.jl            # Generated FFI stubs (auto-regenerated)
│   │   │   ├── database.jl         # Factory methods, lifecycle
│   │   │   ├── database_create.jl  # create_element! convenience
│   │   │   ├── database_csv.jl     # export_csv, import_csv
│   │   │   ├── database_delete.jl  # delete_element!
│   │   │   ├── database_metadata.jl # Metadata queries + convenience readers
│   │   │   ├── database_query.jl   # query_string/integer/float, parameterized
│   │   │   ├── database_read.jl    # read_scalar/vector/set (all + by_id)
│   │   │   ├── database_update.jl  # update_scalar/vector/set/time_series!
│   │   │   ├── database_transaction.jl # transaction! block wrapper
│   │   │   ├── element.jl          # Element builder
│   │   │   ├── exceptions.jl       # QuiverException
│   │   │   ├── date_time.jl        # DateTime parsing helpers
│   │   │   ├── lua_runner.jl       # LuaRunner wrapper
│   │   │   └── (other files)
│   │   ├── test/                   # Julia tests
│   │   ├── generator/              # FFI stub generator (auto-regenerates c_api.jl)
│   │   └── Project.toml            # Julia package manifest
│   │
│   ├── dart/                       # Dart binding
│   │   ├── lib/src/                # Dart implementation
│   │   │   ├── database.dart       # Database class, factories
│   │   │   ├── database_create.dart # createElement convenience
│   │   │   ├── database_csv.dart   # exportCSV, importCSV
│   │   │   ├── database_delete.dart # deleteElement
│   │   │   ├── database_metadata.dart # Metadata queries + convenience readers
│   │   │   ├── database_query.dart # queryString/queryInteger/queryFloat
│   │   │   ├── database_read.dart  # readScalar/readVector/readSet (all + byId)
│   │   │   ├── database_update.dart # updateScalar/updateVector/updateSet
│   │   │   ├── database_transaction.dart # transaction() block wrapper
│   │   │   ├── element.dart        # Element builder
│   │   │   ├── exceptions.dart     # QuiverException
│   │   │   ├── date_time.dart      # DateTime parsing helpers
│   │   │   ├── ffi/                # Generated FFI bindings (auto-regenerated)
│   │   │   └── lua_runner.dart     # LuaRunner wrapper
│   │   ├── test/                   # Dart tests
│   │   ├── generator/              # FFI stub generator
│   │   ├── hook/                   # Build hook for native library compilation
│   │   └── pubspec.yaml            # Dart package manifest
│   │
│   └── python/                     # Python binding
│       ├── src/quiver/             # Python implementation
│       └── tests/unit/             # Python tests
│
├── tests/                          # C++ test suite
│   ├── test_database_lifecycle.cpp # Database open, close, move, options
│   ├── test_database_create.cpp    # create_element operations
│   ├── test_database_csv.cpp       # CSV export with formatting
│   ├── test_database_read.cpp      # Read scalar/vector/set operations
│   ├── test_database_update.cpp    # Update scalar/vector/set operations
│   ├── test_database_delete.cpp    # Delete operations
│   ├── test_database_query.cpp     # SQL query operations
│   ├── test_database_relations.cpp # Foreign key relations
│   ├── test_database_time_series.cpp # Time series operations
│   ├── test_database_transaction.cpp # Transaction control
│   ├── test_database_errors.cpp    # Error message validation
│   ├── test_c_api_*.cpp            # C API tests (mirror C++ tests)
│   ├── test_c_api_element.cpp      # Element builder C API
│   ├── test_c_api_lua_runner.cpp   # LuaRunner C API
│   ├── benchmark/                  # Performance benchmarks
│   │   └── benchmark.cpp           # Transaction performance (manual execution)
│   └── schemas/                    # Test database schemas
│       ├── valid/                  # Valid test schemas
│       │   ├── (various schema.sql files)
│       ├── invalid/                # Invalid test schemas (error cases)
│       ├── migrations/             # Migration test cases
│       │   ├── 1/
│       │   ├── 2/
│       │   └── 3/
│       └── issues/                 # Specific issue reproduction schemas
│           ├── issue52/
│           └── issue70/
│
├── cmake/                          # CMake build utilities
│   └── (build configuration)
│
├── scripts/                        # Build and test scripts
│   ├── build-all.bat               # Build all (C++, Julia, Dart, Python tests)
│   └── test-all.bat                # Run all tests
│
├── docs/                           # Documentation
│
├── CLAUDE.md                       # Project specification (binding rules, patterns, conventions)
├── CMakeLists.txt                  # Root CMake configuration
└── README.md                       # Project overview
```

## Directory Purposes

**`include/quiver/`:**
- Purpose: Public C++ API headers, consumer-facing interfaces
- Contains: Database class, Element builder, Metadata types, Options, Schema types, Value variant
- Naming: All public types in `quiver::` namespace
- Exported: Symbol visibility controlled by `export.h` (QUIVER_API macro)

**`include/quiver/c/`:**
- Purpose: FFI-safe C API headers for language bindings
- Contains: Function declarations with C linkage, opaque handle types, error codes, enums for data types
- Naming: All functions prefixed `quiver_`, all handles suffixed `_t` (opaque typedef)
- Pattern: Binary return codes (QUIVER_OK=0, QUIVER_ERROR=1), values via output parameters

**`src/`:**
- Purpose: C++ implementation files organized by category
- Organization: One file per logical feature area (database_create.cpp, database_read.cpp, etc.)
- Internal headers: `database_impl.h` (Pimpl), `database_internal.h` (utilities)
- Dependencies: sqlite3, spdlog, RapidCSV, Lua

**`src/c/`:**
- Purpose: C API wrapper implementations
- Pattern: Thin wrappers around C++ methods with try-catch and error marshaling
- Co-location: Alloc/free pairs in same file (reads in database_read.cpp, metadata in database_metadata.cpp)
- Error handling: `QUIVER_REQUIRE` macro for null checks, `quiver_set_last_error()` for exception messages

**`bindings/julia/src/`:**
- Purpose: Julia-specific wrappers around C API
- Generated: `c_api.jl` auto-regenerated by `generator/generator.bat`
- Manual: Database, Element, exceptions, and convenience methods (transaction blocks, composite readers)
- Naming: Snake_case for methods, `!` suffix for mutating operations

**`bindings/dart/lib/src/`:**
- Purpose: Dart-specific wrappers around C API
- Generated: FFI bindings in `ffi/` directory auto-regenerated by `generator/`
- Manual: Database, Element, exceptions, and convenience methods
- Naming: camelCase for methods, methods suffixed with () in Dart
- Build: `hook/` directory contains native library compilation hooks

**`bindings/python/src/`:**
- Purpose: Python-specific wrappers (if active)
- Currently minimal; Dart and Julia are primary bindings

**`tests/`:**
- Purpose: C++ test suite (not language binding tests)
- Pattern: One file per feature area (`test_database_*.cpp`), matching src/ organization
- Schemas: `tests/schemas/valid/` for working schemas, `invalid/` for error cases
- Frameworks: Catch2 or similar C++ test framework
- C API tests: Mirror C++ tests with `test_c_api_*.cpp` naming

**`scripts/`:**
- Purpose: Build and test automation
- `build-all.bat`: Builds Debug (or Release with --release), runs all test suites
- `test-all.bat`: Runs all compiled tests without rebuilding

## Key File Locations

**Entry Points:**

- `include/quiver/quiver.h`: Master header including all public APIs
- `include/quiver/database.h`: Database class (main entry point)
- `include/quiver/c/database.h`: C API entry point
- `include/quiver/lua_runner.h`: Lua integration entry point
- `bindings/julia/src/Quiver.jl`: Julia module root
- `bindings/dart/lib/src/database.dart`: Dart main class

**Configuration:**

- `CLAUDE.md`: Authoritative project specification (binding conventions, patterns, error messages)
- `CMakeLists.txt`: Root CMake build configuration
- `bindings/julia/Project.toml`: Julia package manifest
- `bindings/dart/pubspec.yaml`: Dart package manifest

**Core Logic:**

- `include/quiver/database.h`: Database method signatures (create, read, update, delete, query, transactions, CSV, Lua)
- `src/database_impl.h`: Pimpl struct, sqlite3* and logger storage, TransactionGuard
- `src/database.cpp`: Database constructor, factory methods, schema loading
- `src/database_create.cpp`: Element creation with scalar insert + array routing
- `src/database_read.cpp`: All scalar/vector/set reads (all elements + by_id)
- `src/database_update.cpp`: All scalar/vector/set updates by element ID
- `src/database_metadata.cpp`: Schema introspection (get/list metadata)
- `src/database_query.cpp`: SQL execution with parameterized and plain queries
- `src/database_time_series.cpp`: Time series multi-column read/update
- `src/schema.cpp`: Schema parser from sqlite3 database
- `src/schema_validator.cpp`: Validates Configuration table, naming conventions
- `src/type_validator.cpp`: Validates scalar/array value types before insert

**Testing:**

- `tests/test_database_*.cpp`: C++ unit tests (one per feature)
- `tests/test_c_api_*.cpp`: C API tests (same features, C API boundary)
- `tests/schemas/valid/*.sql`: Valid test database schemas
- `tests/schemas/invalid/*.sql`: Invalid schemas for error testing
- `tests/benchmark/benchmark.cpp`: Transaction performance comparison (manual execution)

**Language Bindings:**

- `bindings/julia/src/database.jl`: Julia Database class and factories
- `bindings/julia/src/c_api.jl`: Auto-generated FFI stubs (do not edit)
- `bindings/dart/lib/src/database.dart`: Dart Database class
- `bindings/dart/lib/src/ffi/`: Auto-generated FFI bindings (do not edit)

## Naming Conventions

**Files:**

- C++ headers: `lowercase_with_underscores.h`
- C++ implementation: `lowercase_with_underscores.cpp`
- Test files: `test_[area]_[feature].cpp` (e.g., `test_database_read.cpp`)
- C API tests: `test_c_api_[area]_[feature].cpp`
- Schemas: `schema.sql` in named directories (e.g., `tests/schemas/valid/`)

**Directories:**

- Feature areas: `src/[feature]/` (e.g., `src/c/`, `src/blob/`)
- Language bindings: `bindings/[language]/`
- Tests: `tests/`
- Headers: `include/quiver/` with optional category subdirs (e.g., `include/quiver/c/`)

**C++ Code:**

- Classes: `PascalCase` (Database, Element, Schema, LuaRunner)
- Namespaces: `lowercase` (quiver, quiver::c)
- Methods: `snake_case` (create_element, read_scalar_integers, begin_transaction)
- Suffixes: `_by_id` for single-element reads when "all" variant exists
- Prefixes on verbs: create, read, update, delete, get, list, has, query, describe, export, import

**C API:**

- Functions: `quiver_[entity]_[method]()` (quiver_database_create_element)
- Types: `quiver_[entity]_t` opaque (quiver_database_t) or concrete (quiver_scalar_metadata_t)
- Enums: `QUIVER_[CONSTANT]` (QUIVER_OK, QUIVER_DATA_TYPE_INTEGER)
- Return codes: QUIVER_OK = 0, QUIVER_ERROR = 1

**Language Bindings:**

- Julia: `snake_case` methods, `!` suffix for mutating, `from_schema()` factory
- Dart: `camelCase` methods, `Database.fromSchema()` named constructor, `ExportCSV` class
- Python: Follows Python conventions (snake_case methods, PascalCase classes)

## Where to Add New Code

**New Feature (Database Operation):**

1. Add C++ method to `include/quiver/database.h`
2. Implement in `src/database_[category].cpp` (create, read, update, delete, query, etc.)
3. Add C API wrapper to `src/c/database_[category].cpp`
4. Add C API declaration to `include/quiver/c/database.h`
5. Regenerate Julia FFI: `bindings/julia/generator/generator.bat`
6. Regenerate Dart FFI: `bindings/dart/generator/generator.bat`
7. Add manual wrappers in `bindings/julia/src/database_[category].jl` and `bindings/dart/lib/src/database_[category].dart` as needed
8. Add C++ tests to `tests/test_database_[feature].cpp`
9. Add C API tests to `tests/test_c_api_database_[feature].cpp`

**New Data Type (Scalar/Vector/Set/TimeSeries):**

1. Update schema validation in `src/schema_validator.cpp` if needed
2. Update type validation in `src/type_validator.cpp`
3. Implement read operations in `src/c/database_read.cpp` and `src/database_read.cpp`
4. Implement update operations in `src/c/database_update.cpp` and `src/database_update.cpp`
5. Add C declarations to `include/quiver/c/database.h`
6. Update metadata introspection in `src/c/database_metadata.cpp` if needed
7. Add tests to `tests/test_database_read.cpp` and `tests/test_database_update.cpp`

**New Validation Rule:**

- Schema validation: Add to `src/schema_validator.cpp` (checks Configuration table, naming conventions)
- Type validation: Add to `src/type_validator.cpp` (checks scalar and array values)
- Add corresponding tests to `tests/test_database_errors.cpp`

**New Test Schema:**

- Valid schema: `tests/schemas/valid/[feature_name]/schema.sql`
- Invalid schema: `tests/schemas/invalid/[error_case]/schema.sql`
- Reference in test via path relative to build output

**Utilities:**

- Shared C API helpers: `src/c/database_helpers.h` (marshaling templates, string handling)
- C++ helpers: `src/database_internal.h` (shared utilities)
- Type definitions: `include/quiver/value.h` (Value variant), `include/quiver/data_type.h` (DataType enum)

## Special Directories

**`.planning/codebase/`:**
- Purpose: Generated documentation (ARCHITECTURE.md, STRUCTURE.md, etc.)
- Generated: By `/gsd:map-codebase` command
- Consumed: By `/gsd:plan-phase` and `/gsd:execute-phase` commands

**`build/`:**
- Purpose: CMake build output (generated, not committed)
- Contains: bin/ (executables), lib/ (compiled libraries)
- Generated: `cmake --build build --config Debug`

**`tests/schemas/`:**
- Purpose: Database schemas for testing
- Valid: `valid/` contains working schemas (one per directory)
- Invalid: `invalid/` contains error case schemas
- Migrations: `migrations/1/`, `migrations/2/`, `migrations/3/` for migration tests
- Issues: `issues/issue52/`, `issues/issue70/` for specific bug reproduction

---

*Structure analysis: 2026-02-23*
