# Codebase Structure

**Analysis Date:** 2026-09-14

## Directory Layout

```
quiver3/
├── CMakeLists.txt                 # Root CMake configuration (C++20, C API, tests, build targets)
├── CMakePresets.json              # Presets: dev (Debug), release, windows-release, linux-release
├── CLAUDE.md                       # Root project guide (versioning, design decisions, API tables)
├── CHANGELOG.md                    # User-facing changelog (versions, breaking changes)
├── README.md                       # Quick start
├── .clang-format                   # C++ formatting rules
├── .clang-tidy                     # Clang-tidy checks
├── .clangd                         # clangd LSP config
├── .gitattributes                  # Line ending enforcement (LF for .cpp/.h/.dart/.jl/.py)
├── .pre-commit-config.yaml         # Pre-commit hooks (clang-format, cppcheck, cmake-format)
├── codecov.yml                     # Coverage reporting config
│
├── cmake/                          # CMake modules
│   ├── CompilerOptions.cmake       # Compiler flags, C++20, warnings
│   ├── Dependencies.cmake          # FetchContent: sqlite3, tomlplusplus, spdlog, lua, sol2, rapidcsv, argparse, googletest
│   ├── Platform.cmake              # macOS deployment target 13.3 (for std::to_chars), iOS config
│   └── quiverConfig.cmake.in       # Package config for find_package()
│
├── include/quiver/                 # C++ public headers
│   ├── quiver.h                    # Umbrella header (includes database.h, element.h, etc.)
│   ├── export.h                    # QUIVER_API export macro
│   ├── database.h                  # Database class (main API: factories, CRUD, metadata, transactions)
│   ├── element.h                   # Element builder (fluent: set("label", "x"))
│   ├── value.h                     # Value variant (int64/double/string/nullptr)
│   ├── row.h / result.h            # Query result types (Row wraps sqlite3_stmt, Result is vector<Row>)
│   ├── attribute_metadata.h        # ScalarMetadata, GroupMetadata struct definitions
│   ├── options.h                   # DatabaseOptions, CSVOptions, LogLevel enum
│   ├── schema.h                    # Schema, TableDefinition, ColumnDefinition, ForeignKey (from_database, introspection)
│   ├── schema_validator.h          # SchemaValidator (checks naming, structure, group layout)
│   ├── type_validator.h            # TypeValidator (value-to-column type matching)
│   ├── data_type.h                 # DataType enum (Integer, Float, Text, DateTime), helpers
│   ├── migration.h / migrations.h  # Migration (version, up/down sql), Migrations (scan directory)
│   │
│   ├── lua_runner.h                # LuaRunner class (Lua scripting with db access)
│   │
│   ├── binary/                     # Binary subsystem (Julia + Lua only, not Dart/Python/JS)
│   │   ├── binary_file.h           # BinaryFile class (Pimpl, open/close/read/write)
│   │   ├── binary_metadata.h       # BinaryMetadata struct (dimensions, labels, unit, initial_datetime)
│   │   ├── csv_converter.h         # CSVConverter class (bin_to_csv, csv_to_bin)
│   │   ├── dimension.h             # Dimension struct (name, size, optional TimeProperties)
│   │   ├── time_properties.h       # TimeProperties struct (frequency, initial_value, parent_dimension_index)
│   │   ├── time_constants.h        # Time dimension size constants
│   │   └── iteration.h             # Free functions: first_dimensions, next_dimensions, dimension_sizes_at_values
│   │
│   ├── expression/                 # Expression subsystem (Julia + Lua only)
│   │   ├── expression.h            # Expression value type (wraps shared_ptr<ExpressionNode>)
│   │   └── expression_node.h       # ExpressionNode abstract base, concrete subclasses
│   │
│   └── c/                          # C API headers (for FFI)
│       ├── common.h                # quiver_error_t, quiver_get_last_error, quiver_version
│       ├── options.h               # C API option types (DatabaseOptions, CSVOptions, LogLevel)
│       ├── database.h              # C API Database functions (quiver_database_*)
│       ├── element.h               # C API Element (quiver_element_t, quiver_element_set_*)
│       ├── lua_runner.h            # C API LuaRunner (quiver_lua_runner_*)
│       ├── binary/                 # C API Binary wrappers
│       │   ├── binary_file.h
│       │   ├── binary_metadata.h
│       │   └── csv_converter.h
│       └── expression/             # C API Expression wrappers
│           └── expression.h
│
├── src/                            # C++ implementation (database core logic)
│   ├── CLAUDE.md                   # C++ internals guide (Pimpl, transactions, LuaRunner details, hot paths)
│   ├── database.cpp                # Lifecycle, factories, migrations, transactions, dry runs, logging
│   ├── database_impl.h             # Database::Impl struct (private: sqlite3*, schema, validators, helpers)
│   ├── database_internal.h         # Internal helpers: read templates, value_matches_type, metadata converters
│   ├── database_create.cpp         # create_element implementation
│   ├── database_read.cpp           # All read operations (scalar/vector/set/element_ids, metadata)
│   ├── database_update.cpp         # update_element, update_vector_group, update_set_group, update_relation
│   ├── database_delete.cpp         # delete_element implementation
│   ├── database_metadata.cpp       # get_*_metadata, list_* implementations
│   ├── database_query.cpp          # query_string, query_integer, query_float
│   ├── database_time_series.cpp    # Time series read/update/upsert operations
│   ├── database_describe.cpp       # describe, describe_collection, summarize_collection (text reports)
│   ├── database_csv_export.cpp     # export_csv implementation (CSV text generation)
│   ├── database_csv_import.cpp     # import_csv implementation (CSV parsing, INSERT)
│   ├── element.cpp                 # Element builder implementation
│   ├── row.cpp / result.cpp        # Row, Result query result types
│   ├── schema.cpp                  # Schema class implementation (from_database, group table classification)
│   ├── schema_validator.cpp        # Schema validation (conventions, group structure, migrations)
│   ├── type_validator.cpp          # Type validation (scalar/array type checking)
│   ├── migration.cpp / migrations.cpp  # Migration and Migrations classes
│   ├── lua_runner.cpp              # LuaRunner implementation (sol2 bindings, sandboxing)
│   │
│   ├── utils/                      # Shared utilities
│   │   ├── string.h                # new_c_str, trim helpers
│   │   └── datetime.h              # ISO 8601 parsing/formatting (parse_iso8601, format_datetime, is_valid_iso8601)
│   │
│   ├── binary/                     # Binary subsystem implementation
│   │   ├── binary_file.cpp         # BinaryFile class (Pimpl impl) + write registry
│   │   ├── binary_utils.h          # File extension constants
│   │   ├── binary_metadata.cpp     # BinaryMetadata factories, serialization
│   │   ├── csv_converter.cpp       # CSVConverter implementation
│   │   ├── iteration.cpp           # first_dimensions, next_dimensions implementations
│   │   ├── time_properties.cpp     # TimeFrequency string conversion
│   │   └── time_constants.cpp      # Time constants definitions
│   │
│   ├── expression/                 # Expression subsystem implementation
│   │   ├── expression.cpp          # Expression class implementation
│   │   ├── expression_helpers.h    # Shared inline helpers (validation, broadcast metadata, accumulators)
│   │   ├── expression_file.cpp     # ExpressionFile (lazy read from .qvr)
│   │   ├── expression_scalar.cpp   # ExpressionScalar (broadcast constant)
│   │   ├── expression_binary.cpp   # ExpressionBinary (arithmetic, comparison, logical ops)
│   │   ├── expression_unary.cpp    # ExpressionUnary (math, logical negation)
│   │   ├── expression_ternary.cpp  # ExpressionTernary (if-else)
│   │   ├── expression_aggregate.cpp        # ExpressionAggregate (collapse a dimension)
│   │   ├── expression_aggregate_agents.cpp # ExpressionAggregateAgents (collapse label axis)
│   │   ├── expression_select_agents.cpp    # ExpressionSelectAgents (label projection)
│   │   └── expression_rename_agents.cpp    # ExpressionRenameAgents (label rename)
│   │
│   ├── c/                          # C API implementation (FFI marshaling)
│   │   ├── CLAUDE.md               # C API internals (error handling, memory management, marshaling)
│   │   ├── common.cpp              # quiver_get_last_error, quiver_set_last_error, quiver_version
│   │   ├── internal.h              # Shared C API structs (quiver_database, quiver_element, opaque handles)
│   │   ├── database_helpers.h      # Marshaling templates (vector<T> → C arrays), metadata converters
│   │   ├── database_options.h      # Option converters (C ↔ C++)
│   │   ├── options.cpp             # quiver_database_options_default, quiver_csv_options_default
│   │   ├── database.cpp            # C API lifecycle (open, close, factories, validate_migrations)
│   │   ├── database_create.cpp     # C API quiver_database_create_element
│   │   ├── database_read.cpp       # C API all read operations (+ free functions)
│   │   ├── database_update.cpp     # C API update operations
│   │   ├── database_delete.cpp     # C API delete operations
│   │   ├── database_metadata.cpp   # C API metadata get/list (+ free functions)
│   │   ├── database_query.cpp      # C API query operations
│   │   ├── database_time_series.cpp # C API time series operations (+ free functions)
│   │   ├── database_transaction.cpp # C API transaction control
│   │   ├── database_csv_export.cpp / database_csv_import.cpp
│   │   ├── element.cpp             # C API Element builder (quiver_element_set_*)
│   │   ├── lua_runner.cpp          # C API LuaRunner (quiver_lua_runner_*; errors via quiver_get_last_error)
│   │   ├── binary/                 # C API Binary wrappers
│   │   │   ├── binary_file.cpp
│   │   │   ├── binary_metadata.cpp
│   │   │   └── csv_converter.cpp
│   │   └── expression/             # C API Expression wrappers
│   │       └── expression.cpp
│   │
│   └── cli/                        # CLI executable
│       └── main.cpp                # quiver_cli entry point (argparse, Database factory, LuaRunner)
│
├── bindings/                       # Language bindings (thin FFI wrappers)
│   ├── julia/                      # Julia binding (published repo is a mirror of this)
│   │   ├── CLAUDE.md               # Julia-specific patterns and design notes
│   │   ├── Project.toml            # Version, dependencies
│   │   ├── generator/              # FFI generator (Clang.jl → c_api.jl)
│   │   ├── src/                    # Julia implementation
│   │   │   ├── Quiver.jl           # Main module (exports public API)
│   │   │   ├── c_api.jl            # Generated FFI definitions (C API structs, functions)
│   │   │   ├── database.jl         # Database factory, lifecycle
│   │   │   ├── database_create.jl / database_read.jl / database_update.jl / database_delete.jl
│   │   │   ├── database_metadata.jl / database_query.jl / database_time_series.jl / database_transaction.jl
│   │   │   ├── database_csv_export.jl / database_csv_import.jl
│   │   │   ├── element.jl          # Element builder
│   │   │   ├── boolean.jl          # Boolean wrapper (strict 0/1 conversion on read)
│   │   │   ├── date_time.jl        # DateTime wrapper (ISO 8601 parsing on read)
│   │   │   ├── database_options.jl # Options marshaling
│   │   │   ├── lua_runner.jl       # LuaRunner integration
│   │   │   ├── binary/             # Binary subsystem (BinaryFile, BinaryMetadata, CSVConverter, Expression)
│   │   │   ├── helper_maps.jl      # Julia-only convenience (scalar_relation_map, set_relation_map)
│   │   │   └── metadata.jl         # Metadata result types
│   │   └── test/                   # Julia test suite
│   │
│   ├── dart/                       # Dart binding (published on pub.dev as quiverdb)
│   │   ├── CLAUDE.md               # Dart-specific patterns, hook system, Pimpl behavior
│   │   ├── pubspec.yaml            # Version, dependencies
│   │   ├── generator/              # FFI generator (ffigen → bindings.dart)
│   │   ├── hook/                   # Native assets build hook (compiles C++ on pub get)
│   │   ├── lib/src/                # Dart implementation
│   │   │   ├── database.dart       # Database factory, lifecycle
│   │   │   ├── database_create.dart / database_read.dart / database_update.dart / database_delete.dart
│   │   │   ├── database_metadata.dart / database_query.dart / database_time_series.dart / database_transaction.dart
│   │   │   ├── database_csv_export.dart / database_csv_import.dart
│   │   │   ├── element.dart        # Element builder
│   │   │   ├── boolean.dart        # Boolean wrapper
│   │   │   ├── date_time.dart      # DateTime wrapper
│   │   │   ├── database_options.dart
│   │   │   ├── lua_runner.dart     # LuaRunner integration
│   │   │   ├── ffi/                # Generated FFI bindings (bindings.dart, loader)
│   │   │   ├── exceptions.dart     # Exception definitions
│   │   │   └── metadata.dart       # Metadata result types
│   │   └── test/                   # Dart test suite
│   │
│   ├── python/                     # Python binding (published on PyPI as quiverdb)
│   │   ├── CLAUDE.md               # Python-specific patterns, CFFI ABI mode
│   │   ├── pyproject.toml          # Version, dependencies, scikit-build-core config
│   │   ├── generator/              # FFI generator (parses C API headers, prints cdecl diff)
│   │   ├── src/quiverdb/           # Python implementation
│   │   │   ├── __init__.py         # Package init, version export
│   │   │   ├── _c_api.py           # C API definitions (ctypes, via CFFI ABI mode)
│   │   │   ├── _loader.py          # Native library loading (platform detection)
│   │   │   ├── database.py         # Database factory, lifecycle, CRUD, metadata
│   │   │   ├── database_csv_export.py / database_csv_import.py
│   │   │   ├── element.py          # Element builder (accepts **kwargs)
│   │   │   ├── database_options.py # Options marshaling
│   │   │   ├── lua_runner.py       # LuaRunner integration
│   │   │   ├── exceptions.py       # Exception definitions
│   │   │   ├── metadata.py         # Metadata result types
│   │   │   └── py.typed            # PEP 561 marker (type stub support)
│   │   └── tests/                  # Python test suite
│   │
│   ├── js/                         # JavaScript/Bun binding (published on npm as quiverdb)
│   │   ├── CLAUDE.md               # JS-specific patterns, Bun FFI quirks, datetime surface
│   │   ├── package.json            # Version, dependencies, Bun native
│   │   ├── tsconfig.json           # TypeScript config
│   │   ├── src/                    # TypeScript implementation
│   │   │   ├── index.ts            # Package exports
│   │   │   ├── loader.ts           # Hand-written FFI loader + symbol table
│   │   │   ├── database.ts         # Database factory, lifecycle
│   │   │   ├── create.ts / read.ts / update.ts / time-series.ts / query.ts
│   │   │   ├── csv.ts              # CSV export/import
│   │   │   ├── group-columns.ts    # Group column helpers
│   │   │   ├── metadata.ts         # Metadata result types
│   │   │   ├── boolean.ts          # Boolean wrapper
│   │   │   ├── introspection.ts    # Metadata queries
│   │   │   ├── transaction.ts      # Transaction blocks
│   │   │   ├── composites.ts       # Composite helpers (readElementById, readVectorsById)
│   │   │   ├── ffi-helpers.ts      # FFI marshaling helpers (strings, arrays, masks)
│   │   │   ├── errors.ts           # Exception definitions
│   │   │   ├── types.ts            # Type definitions
│   │   │   ├── lua-runner.ts       # LuaRunner integration
│   │   │   └── lua-api.ts          # Agent-facing Lua reference (LUA_DB_API_REFERENCE)
│   │   └── test/                   # Bun test suite
│
├── tests/                          # Comprehensive test suites (all layers)
│   ├── CLAUDE.md                   # Test organization and patterns
│   ├── CMakeLists.txt              # Test targets and linking
│   ├── schemas/                    # SQL schema files (used by all test suites)
│   │   ├── valid/                  # Valid schemas
│   │   │   ├── simple.sql          # Minimal schema with one collection
│   │   │   ├── groups.sql          # Vector, set, time-series groups
│   │   │   ├── relations.sql       # Foreign key relationships
│   │   │   └── ... (more schemas)
│   │   └── invalid/                # Invalid schemas (for negative testing)
│   ├── sandbox/                    # Intentional scratch area (test output, temp files)
│   │
│   ├── test_database_*.cpp         # C++ core library tests
│   ├── test_c_api_*.cpp            # C API tests
│   ├── test_binary_*.cpp           # Binary subsystem tests
│   ├── test_expression_*.cpp       # Expression subsystem tests (if implemented in C++)
│   │
│   └── benchmark/                  # Performance benchmarks
│       └── transaction_benchmark.cpp   # Transaction throughput benchmark
│
├── docs/                           # User documentation
│   ├── introduction.md             # Overview and getting started
│   ├── rules.md                    # Schema rules and conventions
│   ├── attributes.md               # Scalar, vector, set, time-series attribute types
│   ├── migrations.md               # Migration system guide
│   ├── time_series.md              # Time-series operations
│   └── ... (more docs)
│
├── example/                        # Example usage
│   ├── example1.lua                # Lua CRUD example
│   └── example1.bat                # Run example script (Windows)
│
├── scripts/                        # Build and maintenance scripts
│   ├── build-all.bat               # Build everything + run all tests
│   ├── test-all.bat                # Run all test suites
│   ├── clean-all.bat               # Clean all build artifacts
│   ├── format.bat                  # Format all code (clang-format + per-binding formatters)
│   ├── tidy.bat                    # Run clang-tidy
│   ├── generator.bat               # Run all FFI generators
│   ├── assert_version.py           # Check/bump version across all manifests
│   ├── julia/                      # Julia scripts
│   │   └── generate_artifacts.jl   # Generate Julia artifacts (Jll packages)
│   └── ci/                         # CI/CD scripts
│       ├── dispatch_workflow.sh    # Trigger workflow runs
│       └── native_s3.sh            # Upload native binaries to S3
│
├── .github/                        # GitHub Actions
│   ├── workflows/                  # CI/CD pipelines
│   │   ├── test.yml                # Run all tests on PR
│   │   ├── publish-*.yml           # Publish to PyPI, npm, pub.dev, S3
│   │   └── ... (more workflows)
│   ├── actions/                    # Composite actions
│   │   ├── build-cpp/              # Build C++ core and C API
│   │   └── ... (more actions)
│   └── CLAUDE.md                   # Release and publish procedures
│
└── .planning/codebase/             # Codebase documentation (this directory)
    ├── ARCHITECTURE.md             # Architecture, layers, data flow, abstractions
    ├── STRUCTURE.md                # This file
    ├── CONVENTIONS.md              # Coding conventions (if quality focus)
    ├── TESTING.md                  # Testing patterns (if quality focus)
    ├── STACK.md                    # Technology stack (if tech focus)
    ├── INTEGRATIONS.md             # External integrations (if tech focus)
    └── CONCERNS.md                 # Technical debt and issues (if concerns focus)
```

## Key File Locations

**Entry Points:**
- `src/cli/main.cpp`: CLI executable (quiver_cli)
- `src/database.cpp`: Database factory methods and lifecycle
- `src/lua_runner.cpp`: Lua script execution entry point
- `src/c/database.cpp`: C API lifecycle entry point

**Core Logic:**
- `include/quiver/database.h`: Public API interface
- `src/database_impl.h`: Private implementation (sqlite3 handle, schema, validators)
- `src/database_create.cpp`, `database_read.cpp`, `database_update.cpp`, `database_delete.cpp`: CRUD operations
- `src/type_validator.cpp`: Type checking logic (shared by all CRUD)
- `src/schema.cpp`: Schema introspection (from PRAGMA)

**Configuration & Build:**
- `CMakeLists.txt`: Top-level build configuration
- `cmake/Dependencies.cmake`: FetchContent dependency declarations
- `cmake/Platform.cmake`: macOS deployment target (13.3)
- `CMakePresets.json`: Build presets (dev, release, windows-release, linux-release)

**Database Schema & Migrations:**
- `tests/schemas/valid/`: Reference schemas for testing and examples
- Migration patterns documented in `include/quiver/migration.h`, `migrations.h`

**Binary Subsystem (Julia + Lua only):**
- `include/quiver/binary/binary_file.h`: Public interface
- `src/binary/binary_file.cpp`: Implementation with write registry
- `src/binary/iteration.cpp`: Dimension traversal helpers

**Expression Subsystem (Julia + Lua only):**
- `include/quiver/expression/expression.h`: Public Expression type
- `include/quiver/expression/expression_node.h`: Node polymorphism
- `src/expression/expression*.cpp`: 10+ node subclasses

## Naming Conventions

**Files:**
- Database operations split by feature: `database_create.cpp`, `database_read.cpp`, `database_update.cpp`, `database_delete.cpp`, `database_metadata.cpp`, `database_query.cpp`, `database_time_series.cpp`, `database_describe.cpp`, `database_csv_*.cpp`
- C API mirrors: `src/c/database_create.cpp`, etc.
- Binary/expression subsystems: `src/binary/binary_file.cpp`, `src/expression/expression_binary.cpp` (operation type determines filename)
- Tests: `test_<feature>.cpp` (e.g., `test_database_create.cpp`, `test_c_api_database_metadata.cpp`)

**Directories:**
- `include/quiver/`: Public headers (no impl)
- `src/`: C++ implementation (private headers + .cpp)
- `src/c/`: C API (thin wrappers + helpers)
- `src/binary/`, `src/expression/`, `src/cli/`, `src/utils/`: Subsystems
- `bindings/<lang>/src/`: Language-specific implementation
- `tests/schemas/`: Shared test data

**Classes & Types:**
- Value types (no Pimpl): `Element`, `Value`, `Row`, `Result`, `BinaryMetadata`, `Dimension`, `TimeProperties`, `Expression`
- Pimpl classes (hiding dependencies): `Database`, `LuaRunner`, `BinaryFile`, `Schema`
- Validators: `TypeValidator`, `SchemaValidator`

## Where to Add New Code

**New Database Feature (e.g., new CRUD operation):**
1. Add method to `include/quiver/database.h` (public header)
2. Implement in `src/database_<feature>.cpp` (e.g., `database_frobnicate.cpp`)
3. Add corresponding C API function to `include/quiver/c/database.h`
4. Implement C API wrapper in `src/c/database_<feature>.cpp`
5. Add binding methods to each language:
   - Julia: `bindings/julia/src/database_frobnicate.jl`
   - Dart: `bindings/dart/lib/src/database_frobnicate.dart`
   - Python: `bindings/python/src/quiverdb/database.py` (add method)
   - JS: `bindings/js/src/database.ts` (add method)
   - Lua: `src/lua_runner.cpp` (sol2 binding)
6. Add tests:
   - C++ core: `tests/test_database_frobnicate.cpp`
   - C API: `tests/test_c_api_database_frobnicate.cpp`
   - Bindings: per-binding test files

**New Validation Rule:**
1. Add check to `include/quiver/schema_validator.h` or `type_validator.h`
2. Implement validation logic
3. Thread the operation name through for Pattern 1 error messages
4. Test in `tests/test_schema_validator.cpp` or `tests/test_type_validator.cpp`

**New Binding Convenience Method (e.g., composite reader):**
1. Implement in binding-specific files (e.g., `bindings/dart/lib/src/composites.dart`)
2. Document the wrapping (which core methods it uses)
3. Add binding-specific tests
4. No C API or C++ core changes needed

**New Binary Operation (Expression subsystem):**
1. Add node type to `include/quiver/expression/expression_node.h`
2. Implement `<operation>.cpp` in `src/expression/`
3. Add factory/operator to `include/quiver/expression/expression.h`
4. Add C API wrapper to `include/quiver/c/expression/expression.h`
5. Bind in Julia (`bindings/julia/src/binary/expression.jl`) and Lua (`src/lua_runner.cpp`)

**Utilities & Helpers:**
- String utilities: `src/utils/string.h`
- DateTime utilities: `src/utils/datetime.h`
- Internal read templates: `src/database_internal.h`
- C API marshaling: `src/c/database_helpers.h`

## Special Directories

**`tests/sandbox/`:**
- Purpose: Intentional scratch area for temporary test outputs
- Generated: Yes (by tests at runtime)
- Committed: No (.gitignored)
- Use for: Writing test databases, temporary fixtures, debugging
- **Never** delete or "clean up" — intentional scratch target

**`tests/schemas/valid/` and `tests/schemas/invalid/`:**
- Purpose: Reference SQL schemas used by all test suites (C++, C API, all bindings)
- Generated: No (hand-written)
- Committed: Yes
- Use for: Schema validation tests, CRUD tests, integration tests

**`.planning/codebase/`:**
- Purpose: Codebase mapping documents (ARCHITECTURE.md, STRUCTURE.md, etc.)
- Generated: Yes (by `/gsd-map-codebase` command)
- Committed: Yes
- Use for: Navigation and reference by other Claude commands

**`build/`:**
- Purpose: CMake build output directory
- Generated: Yes (by cmake/build commands)
- Committed: No (.gitignored)
- Subdirs: `build/bin/` (executables), `build/lib/` (libraries), `build/compile_commands.json` (clang-tidy)

## Build System Notes

- **Presets-based:** Use `cmake --build build --preset dev` or `cmake --build build --preset release`
- **First-time configure:** `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`
- **Test runners:** `./build/bin/quiver_tests.exe` (C++), `./build/bin/quiver_c_tests.exe` (C API), `bindings/julia/test/test.bat` (Julia), etc.
- **Benchmark:** `./build/bin/quiver_benchmark.exe` (manual run, never in CI)

## Cross-Binding Consistency

All five bindings must have:
1. Same Database class interface (with language-idiomatic names)
2. Same CRUD operation signatures
3. Same metadata structures
4. Same error handling behavior (operations throw on missing id, not silent no-op)
5. Same nullable handling (nulls preserved in scalar bulk reads, dropped in vector/set)

Schema compliance documented in root `CLAUDE.md` cross-layer naming tables.

---

*Structure analysis: 2026-09-14*
