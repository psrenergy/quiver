# Codebase Structure

**Analysis Date:** 2026-09-17

## Directory Layout

```
quiver3/
├── CMakeLists.txt                          # Project root; version source of truth
├── CMakePresets.json                       # Build presets (dev, release, windows-release, linux-release)
├── .clang-format                           # C++ formatting rules
├── .clang-tidy                             # C++ linting rules
├── CHANGELOG.md                            # User-visible changes (manual edit)
│
├── include/quiver/                         # C++ public headers
│   ├── database.h                          # Database class API
│   ├── element.h                           # Element builder
│   ├── lua_runner.h                        # LuaRunner class
│   ├── schema.h                            # Schema introspection types
│   ├── attribute_metadata.h                # ScalarMetadata, GroupMetadata
│   ├── options.h                           # DatabaseOptions, CSVOptions
│   ├── type_validator.h                    # TypeValidator
│   ├── value.h                             # Value variant (int64/double/string/nullptr)
│   ├── data_type.h                         # DataType enum
│   ├── row.h / result.h                    # Query result types
│   ├── migration.h / migrations.h          # Migration types
│   ├── binary/                             # Binary subsystem headers
│   │   ├── binary_file.h
│   │   ├── csv_converter.h
│   │   ├── binary_metadata.h
│   │   ├── dimension.h
│   │   └── iteration.h
│   └── expression/                         # Expression subsystem headers
│       ├── expression.h
│       └── expression_node.h
│
├── include/quiver/c/                       # C API public headers (FFI)
│   ├── common.h                            # Error handling, version
│   ├── database.h                          # quiver_database_* functions
│   ├── element.h                           # quiver_element_* functions
│   ├── options.h                           # Option types
│   ├── lua_runner.h                        # quiver_lua_runner_* functions
│   ├── binary/                             # Binary subsystem C API
│   └── expression/                         # Expression subsystem C API
│
├── src/                                    # C++ implementation
│   ├── CMakeLists.txt                      # Library target config
│   ├── database.cpp                        # Database lifecycle, factories
│   ├── database_create.cpp                 # Element create
│   ├── database_read.cpp                   # Read scalars/vectors/sets, element_ids, number_of_elements
│   ├── database_update.cpp                 # Update element, relation, vector/set/time-series groups
│   ├── database_delete.cpp                 # Delete element
│   ├── database_metadata.cpp               # Metadata getters/listers
│   ├── database_query.cpp                  # Parameterized queries
│   ├── database_time_series.cpp            # Time-series read/update/upsert
│   ├── database_describe.cpp               # Describe, describe_collection, summarize_collection
│   ├── database_csv_export.cpp             # CSV export with formatting
│   ├── database_csv_import.cpp             # CSV import with parsing
│   ├── database_impl.h                     # Database::Impl (Pimpl), TransactionGuard
│   ├── database_internal.h                 # Internal helpers: read templates, value_matches_type
│   ├── schema.cpp                          # Schema::from_database, table classification
│   ├── schema_validator.cpp                # Schema convention validation
│   ├── type_validator.cpp                  # Type checking (scalars, arrays, DATE_TIME)
│   ├── element.cpp                         # Element builder implementation
│   ├── row.cpp                             # Row query results
│   ├── result.cpp                          # Result template instantiations
│   ├── migration.cpp / migrations.cpp      # Migration discovery and validation
│   ├── lua_runner.cpp                      # LuaRunner (sol2 bindings, ~1300 lines)
│   ├── utils/
│   │   ├── string.h                        # new_c_str, trim helpers
│   │   └── datetime.h                      # ISO 8601 parsing/validation/formatting
│   ├── cli/
│   │   └── main.cpp                        # quiver_cli entry point
│   ├── binary/                             # Binary subsystem implementation
│   │   ├── binary_file.cpp
│   │   ├── csv_converter.cpp
│   │   ├── binary_metadata.cpp
│   │   ├── time_properties.cpp
│   │   └── iteration.cpp
│   └── expression/                         # Expression subsystem implementation
│       ├── expression.cpp
│       ├── expression_file.cpp
│       ├── expression_scalar.cpp
│       ├── expression_binary.cpp
│       ├── expression_unary.cpp
│       ├── expression_ternary.cpp
│       ├── expression_aggregate.cpp
│       ├── expression_aggregate_agents.cpp
│       ├── expression_select_agents.cpp
│       ├── expression_rename_agents.cpp
│       └── expression_helpers.h
│
├── src/c/                                  # C API implementation
│   ├── CMakeLists.txt                      # C API library target
│   ├── common.cpp                          # Error channel, version
│   ├── internal.h                          # Opaque handle structs, QUIVER_REQUIRE macro
│   ├── database_helpers.h                  # Marshaling templates, converters
│   ├── database_options.h                  # Option converters
│   ├── database.cpp                        # Lifecycle: open, close, describe, factories
│   ├── database_create.cpp                 # quiver_database_create_element
│   ├── database_read.cpp                   # Scalar/vector/set reads, free functions
│   ├── database_update.cpp                 # Element, group, relation updates
│   ├── database_delete.cpp                 # Element delete
│   ├── database_metadata.cpp               # Metadata getters/listers, free functions
│   ├── database_query.cpp                  # Parameterized queries
│   ├── database_time_series.cpp            # Time-series read/update/upsert
│   ├── database_transaction.cpp            # Begin/commit/rollback/dry_run
│   ├── database_csv_export.cpp / database_csv_import.cpp
│   ├── element.cpp                         # Element builder C API
│   ├── lua_runner.cpp                      # LuaRunner C API wrapper
│   ├── options.cpp                         # Option defaults
│   ├── binary/                             # Binary C API wrappers
│   └── expression/                         # Expression C API wrappers
│
├── tests/                                  # Test suites
│   ├── CMakeLists.txt                      # Test target config
│   ├── test_database_*.cpp                 # C++ core tests (one file per area)
│   ├── test_element.cpp / test_row_result.cpp / test_migrations.cpp
│   ├── test_lua_runner_*.cpp               # Lua bindings tests
│   ├── test_binary_*.cpp                   # Binary subsystem tests
│   ├── test_expression.cpp                 # Expression subsystem tests
│   ├── test_c_api_*.cpp                    # C API tests (mirror of C++ tests)
│   ├── test_issues.cpp                     # Regression tests (issue-numbered)
│   └── schemas/                            # Shared test schemas
│       ├── valid/                          # Schema files referenced by all test suites
│       │   ├── all_types.sql               # All column types, nullable, PK, FK
│       │   ├── basic.sql                   # Simple single-table schema
│       │   ├── collections.sql             # Multiple collections
│       │   ├── composite_helpers.sql       # For helper_maps tests (Julia only)
│       │   ├── csv_export.sql              # CSV export edge cases
│       │   ├── multi_column_groups.sql     # Multiple columns per vector/set
│       │   ├── multi_column_time_series.sql / multi_time_series.sql / multi_dim_time_series.sql
│       │   ├── mixed_time_series.sql
│       │   ├── nullable_time_series.sql
│       │   └── relations.sql               # Foreign key relations
│       ├── invalid/                        # Schemas that must be rejected
│       │   ├── duplicate_attribute_*.sql
│       │   ├── fk_*.sql
│       │   ├── label_*.sql
│       │   ├── no_configuration.sql
│       │   ├── set_no_unique.sql
│       │   └── vector_no_index.sql
│       └── migrations/                     # Versioned migrations for testing
│           ├── 1/ (up.sql, down.sql)
│           ├── 2/ (up.sql, down.sql)
│           ├── 3/ (up.sql, down.sql)
│           └── issues/ (issue-specific migrations)
│
├── bindings/
│   ├── julia/                              # Julia binding (Quiver.jl)
│   │   ├── Project.toml                    # Version (must match CMakeLists.txt)
│   │   ├── src/
│   │   │   ├── Quiver.jl                   # Module root
│   │   │   ├── database.jl                 # Main API (CRUD, metadata, etc.)
│   │   │   ├── element.jl                  # Element builder
│   │   │   ├── lua_runner.jl               # LuaRunner wrapper
│   │   │   ├── binary.jl                   # Binary subsystem (Julia only)
│   │   │   ├── expression.jl               # Expression subsystem (Julia only)
│   │   │   ├── composite_read.jl           # Convenience readers (scalars_by_id, etc.)
│   │   │   ├── helper_maps.jl              # Relation-map helpers (Julia only)
│   │   │   ├── c_api.jl                    # Generated FFI bindings (DO NOT HAND-EDIT)
│   │   │   └── binary/                     # Binary subsystem implementation
│   │   ├── generator/                      # FFI generator (Clang.jl)
│   │   └── test/                           # Test suite (mirrored by area)
│   │
│   ├── dart/                               # Dart binding (quiverdb on pub)
│   │   ├── pubspec.yaml                    # Version, ffigen config
│   │   ├── lib/
│   │   │   ├── quiverdb.dart               # Public API import
│   │   │   └── src/
│   │   │       ├── database.dart           # Main API (+ parts for areas)
│   │   │       ├── element.dart            # Element builder
│   │   │       ├── lua_runner.dart         # LuaRunner wrapper
│   │   │       ├── exceptions.dart         # Exception types
│   │   │       ├── date_time.dart          # DateTime parsing/formatting
│   │   │       ├── metadata.dart           # Metadata types
│   │   │       ├── ffi/
│   │   │       │   ├── bindings.dart       # Generated ffigen output (DO NOT HAND-EDIT)
│   │   │       │   └── library_loader.dart # Native library resolution (three tiers)
│   │   ├── hook/
│   │   │   └── build.dart                  # Native-assets build hook (CMake driver)
│   │   ├── generator/                      # FFI generator (ffigen)
│   │   └── test/                           # Test suite (*_test.dart per area)
│   │
│   ├── python/                             # Python binding (quiverdb on PyPI)
│   │   ├── pyproject.toml                  # Version, build config (scikit-build-core)
│   │   ├── quiverdb/
│   │   │   ├── __init__.py                 # Public API import
│   │   │   ├── database.py                 # Main API
│   │   │   ├── element.py                  # Element builder (kwargs→dict)
│   │   │   ├── lua_runner.py               # LuaRunner wrapper
│   │   │   ├── exceptions.py               # Exception types
│   │   │   ├── _c_api.py                   # Hand-written CFFI ABI-mode definitions
│   │   │   └── _loader.py                  # Native library loader
│   │   ├── tests/                          # Test suite (pytest, mirrors areas)
│   │   └── scripts/                        # Build helpers
│   │
│   └── js/                                 # JavaScript binding (quiverdb on npm)
│       ├── package.json                    # Version, build config
│       ├── src/
│       │   ├── index.ts                    # Public API entry point
│       │   ├── database.ts                 # Main API (database-*.ts per area)
│       │   ├── element.ts                  # Element builder
│       │   ├── lua-api.ts                  # Agent-facing Lua API reference
│       │   ├── loader.ts                   # Hand-written FFI symbol loader (Bun)
│       │   └── loader-*.ts                 # Platform-specific loaders (Darwin, Linux, Windows)
│       └── test/                           # Test suite (Bun, *.test.ts per area)
│
├── cmake/                                  # CMake infrastructure
│   ├── CMakeLists.txt                      # Helper macros and functions
│   ├── CompilerOptions.cmake               # Language-specific flags (C++20, warnings)
│   ├── Dependencies.cmake                  # FetchContent (sqlite3, lua, spdlog, sol2, etc.)
│   ├── Platform.cmake                      # OS-specific settings (macOS deployment target)
│   └── quiverConfig.cmake.in               # Config file for downstream cmake projects
│
├── scripts/                                # Build and utility scripts
│   ├── build-all.bat                       # Configure + build + test all (Debug)
│   ├── test-all.bat                        # Run all test suites
│   ├── clean-all.bat                       # Remove build artifacts
│   ├── format.bat                          # Format all code (clang-format, JuliaFormatter, ruff, biome)
│   ├── tidy.bat                            # Run clang-tidy
│   ├── generator.bat                       # Run all FFI generators
│   ├── assert_version.py                   # Single source of truth for version (bump all five)
│   ├── ci/
│   │   ├── native_s3.sh                    # Upload natives to S3 (release pipeline)
│   │   ├── dispatch_workflow.sh            # GitHub workflow dispatch with correlation
│   │   └── build_native_linux.sh           # Manylinux2014 Docker build (glibc 2.17 floor)
│   └── julia/
│       └── generate_artifacts.jl           # Generate Julia artifacts (release pipeline)
│
├── .github/                                # CI and release pipeline
│   ├── workflows/
│   │   ├── ci.yml                          # CI: build matrix, ctest, coverage
│   │   ├── bump-version.yml                # Manual: bump version in five manifests
│   │   ├── publish.yml                     # Manual: orchestrate full release
│   │   ├── publish-s3.yml                  # Build natives, upload to S3
│   │   ├── publish-julia.yml               # Mirror bindings/julia to psrenergy/Quiver.jl
│   │   ├── publish-python.yml              # cibuildwheel, publish to PyPI
│   │   └── publish-js.yml                  # Pack natives, publish to npm
│   └── actions/
│       └── build-cpp/                      # Composite action: configure + build C++
│
├── example/                                # Usage examples
│   ├── example1.lua                        # Lua script example
│   └── example1.bat                        # CLI invocation example
│
├── docs/                                   # User-facing documentation
│   ├── introduction.md
│   ├── rules.md
│   ├── attributes.md
│   ├── migrations.md
│   └── time_series.md
│
├── assets/                                 # Static assets
│   └── logo.svg
│
├── .pre-commit-config.yaml                 # Pre-commit hooks (clang-format, cppcheck, etc.)
├── .gitattributes                          # LF enforcement for source files
└── CLAUDE.md                               # Root project instructions
```

## Key File Locations

**Entry Points:**
- C++ library initialization: `include/quiver/database.h` — `Database::Database(path, options)`
- C API entry: `include/quiver/c/database.h` — `quiver_database_open(path, options, out_db)`
- CLI: `src/cli/main.cpp` — reads `--schema`, `--migrations`, `--dry-run`, runs Lua script
- Julia: `bindings/julia/src/Quiver.jl` — module root, exports `Database`, etc.
- Dart: `bindings/dart/lib/quiverdb.dart` — public import of `Database`
- Python: `bindings/python/quiverdb/__init__.py` — public import of `Database`
- JS: `bindings/js/src/index.ts` — default export of `Database`

**Configuration:**
- `CMakeLists.txt`: Version, C++20 standard, build targets
- `.clang-format`: C++ style (2-space indent, LLVM style)
- `.clang-tidy`: Lint rules
- `pyproject.toml` (Python): Build backend (scikit-build-core), version
- `pubspec.yaml` (Dart): ffigen config (inline), version
- `package.json` (JS): version, build scripts
- `Project.toml` (Julia): version

**Core Logic:**
- Database CRUD: `src/database_create.cpp`, `database_read.cpp`, `database_update.cpp`, `database_delete.cpp`
- Metadata: `src/database_metadata.cpp`, `src/schema.cpp`
- Validation: `src/type_validator.cpp`, `src/schema_validator.cpp`
- Transactions: `src/database.cpp` (contains `begin_transaction`, `commit`, etc.)
- CSV: `src/database_csv_export.cpp`, `database_csv_import.cpp`
- Time-series: `src/database_time_series.cpp`

**Testing:**
- Test schemas: `tests/schemas/valid/*.sql`, `tests/schemas/invalid/*.sql`
- C++ tests: `tests/test_database_*.cpp` (one file per area)
- C API tests: `tests/test_c_api_*.cpp` (mirror C++ tests)
- Lua tests: `tests/test_lua_runner_*.cpp` (per-area split)

## Naming Conventions

**Files:**
- C++ headers: `snake_case.h` (e.g., `database.h`, `type_validator.h`)
- C++ impl: `snake_case.cpp` (e.g., `database.cpp`, `database_create.cpp`)
- Tests: `test_<area>.cpp` (C++ core), `test_c_api_<area>.cpp` (C API), `test_lua_runner_<area>.cpp` (Lua)
- Binding tests: Each binding uses its idiom (`*_test.dart`, `test_*.jl`, `test_*.py`, `*.test.ts`)
- Schemas: `snake_case.sql` (e.g., `multi_column_groups.sql`)

**Directories:**
- Core implementation: `src/` (C++) + `src/c/` (C API)
- Subsystems: `src/binary/`, `src/expression/`
- Bindings: `bindings/{julia,dart,python,js}/`
- Tests: `tests/` (C++/C), `<binding>/test/` (per-binding)

**C++ Identifiers:**
- Classes: `PascalCase` (e.g., `Database`, `Element`, `LuaRunner`)
- Methods: `snake_case` (e.g., `create_element`, `read_scalar_integers`)
- Enum values: `UPPER_SNAKE_CASE` (e.g., `QUIVER_DATA_TYPE_INTEGER`)
- Private members: `snake_case_` (trailing underscore, e.g., `impl_`)

**Database Schema Conventions:**
- Configuration table (required): `Configuration(id, label)`
- Main collections: `{Collection}(id PRIMARY KEY, label UNIQUE, ...)`
- Vector groups: `{Collection}_vector_{name}(id, vector_index, value, PRIMARY KEY(id, vector_index))`
- Set groups: `{Collection}_set_{name}(id, value, UNIQUE(id, value))`
- Time-series groups: `{Collection}_time_series_{name}(id, date_time, value, PRIMARY KEY(id, date_time))` with `date_*` dimension
- Time-series files (singleton): `{Collection}_time_series_files(data_file, metadata_file)`
- All tables: `STRICT` mode, foreign keys use `ON DELETE CASCADE ON UPDATE CASCADE`

## Where to Add New Code

**New Database Method (C++, C, all bindings):**
1. Add public method signature in `include/quiver/database.h`
2. Implement in `src/database_<area>.cpp` (use existing area or create new)
3. Export from C API in `include/quiver/c/database.h` as `quiver_database_<method_name>`
4. Implement C API wrapper in `src/c/database_<area>.cpp`
5. Bind in Julia: `src/c_api.jl` (regenerate via Clang.jl) + `src/database.jl` (add wrapper)
6. Bind in Dart: `lib/src/ffi/bindings.dart` (regenerate via ffigen) + `lib/src/database.dart` (add wrapper)
7. Bind in Python: Update `_c_api.py` + add method in `quiverdb/database.py`
8. Bind in JS: Update `src/loader.ts` + add method in `src/database.ts`
9. Test in each layer: `tests/test_*.cpp` (C++), `tests/test_c_api_*.cpp` (C), `bindings/*/test/*` (per binding)

**New Lua Binding (sol2 method):**
1. Add method binding in `src/lua_runner.cpp` (sol2 syntax, e.g., `sol2_state["db"]["my_method"] = ...`)
2. Update `bindings/js/src/lua-api.ts` (`LUA_DB_API_REFERENCE` constant) with new method name and signature
3. Run `bindings/js/test/lua-api-sync.test.ts` to verify the reference is in sync
4. Add Lua test in `tests/test_lua_runner_<area>.cpp`

**New Test Schema:**
- Add `.sql` file to `tests/schemas/valid/` if it should be accepted
- Add `.sql` file to `tests/schemas/invalid/` if it should be rejected by `SchemaValidator`
- Reference from test files via `TEST_SCHEMA_DIR / "valid/my_schema.sql"` (C++) or relative path (bindings)

**New Binding Feature (not part of core API):**
- Implement in the binding's language (e.g., Julia helper maps in `bindings/julia/src/helper_maps.jl`)
- Document as binding-only in root `CLAUDE.md` (see "Binding-Only Convenience Methods")
- Add tests in that binding's test directory only (not cross-binding)

## Special Directories

**`tests/sandbox/`:**
- Purpose: Intentional scratch target for ad-hoc experiments
- Status: Not tracked by version control (in `.gitignore`), but do not delete the directory
- Usage: Developers add temporary test files here; the directory is never cleaned up as part of CI

**`build/`:**
- Purpose: CMake build output (executables, libraries, artifacts)
- Generated: `build/bin/` (executables), `build/lib/` (libraries)
- Committed: No (in `.gitignore`)

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents (ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, etc.)
- Generated: By `/gsd-map-codebase` command
- Committed: Yes (guidance for future implementations)

**`.gsd/`:**
- Purpose: GSD project state and history
- Committed: Yes (project metadata)

---

*Structure analysis: 2026-09-17*
