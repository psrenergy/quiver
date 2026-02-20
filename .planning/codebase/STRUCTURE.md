# Codebase Structure

**Analysis Date:** 2026-02-20

## Directory Layout

```
quiver/
├── include/
│   └── quiver/                    # C++ public headers
│       ├── database.h             # Database class — primary C++ API
│       ├── element.h              # Element builder
│       ├── attribute_metadata.h   # ScalarMetadata, GroupMetadata types
│       ├── schema.h               # Schema, TableDefinition, ColumnDefinition
│       ├── schema_validator.h     # SchemaValidator (validates on open)
│       ├── type_validator.h       # TypeValidator (validates on write)
│       ├── lua_runner.h           # LuaRunner (Lua scripting)
│       ├── migration.h            # Single migration file abstraction
│       ├── migrations.h           # Migration collection + pending logic
│       ├── result.h               # SQL result set
│       ├── row.h                  # Single result row
│       ├── value.h                # Value variant (nullptr/int64/double/string)
│       ├── data_type.h            # DataType enum + helpers
│       ├── export.h               # QUIVER_API DLL export macro
│       ├── quiver.h               # Convenience umbrella include
│       ├── c/                     # C API public headers (for FFI consumers)
│       │   ├── common.h           # quiver_error_t, quiver_get_last_error, quiver_version
│       │   ├── options.h          # quiver_database_options_t, quiver_log_level_t (shared C/C++)
│       │   ├── database.h         # All database C API declarations
│       │   ├── element.h          # quiver_element_t C API
│       │   └── lua_runner.h       # quiver_lua_runner C API
│       └── blob/                  # Blob/external binary file headers
│           ├── blob.h             # Blob class
│           ├── blob_csv.h         # CSV blob operations
│           ├── blob_metadata.h    # BlobMetadata type
│           ├── dimension.h        # Dimension type
│           ├── time_constants.h   # Time-related constants
│           └── time_properties.h  # Time property helpers
├── src/                           # C++ implementation
│   ├── database.cpp               # Lifecycle: open, close, factory methods, CSV
│   ├── database_create.cpp        # create_element, update_element (routing arrays)
│   ├── database_delete.cpp        # delete_element
│   ├── database_describe.cpp      # describe() — schema inspection to stdout
│   ├── database_impl.h            # Database::Impl definition + TransactionGuard
│   ├── database_internal.h        # Shared templates (read_grouped_values_all/by_id, etc.)
│   ├── database_metadata.cpp      # get/list scalar_metadata, vector/set/time series groups
│   ├── database_query.cpp         # query_string/integer/float with optional params
│   ├── database_read.cpp          # All scalar/vector/set/element ID read operations
│   ├── database_relations.cpp     # update/read scalar_relation
│   ├── database_time_series.cpp   # read/update_time_series_group + files operations
│   ├── database_update.cpp        # update_scalar/vector/set_*
│   ├── element.cpp                # Element builder implementation
│   ├── lua_runner.cpp             # LuaRunner Impl + all sol2 bindings (large: ~1700 lines)
│   ├── migration.cpp              # Migration implementation
│   ├── migrations.cpp             # Migrations directory discovery
│   ├── result.cpp                 # Result implementation
│   ├── row.cpp                    # Row implementation
│   ├── schema.cpp                 # Schema::from_database, table classification, naming
│   ├── schema_validator.cpp       # SchemaValidator validation rules
│   ├── type_validator.cpp         # TypeValidator scalar/array checks
│   ├── CMakeLists.txt             # Build targets for core + C API
│   ├── c/                         # C API implementation
│   │   ├── internal.h             # quiver_database/quiver_element structs + QUIVER_REQUIRE macro
│   │   ├── database_helpers.h     # Marshaling templates (read_scalars_impl, copy_strings_to_c, convert_*_to_c)
│   │   ├── common.cpp             # quiver_version, quiver_get_last_error (thread-local)
│   │   ├── database.cpp           # Lifecycle, describe, CSV C API functions
│   │   ├── database_create.cpp    # create/update/delete element C API
│   │   ├── database_metadata.cpp  # get/list metadata C API + free functions
│   │   ├── database_query.cpp     # query_string/integer/float C API (plain + _params)
│   │   ├── database_read.cpp      # All read C API + matching free functions
│   │   ├── database_relations.cpp # Relation C API
│   │   ├── database_time_series.cpp # Time series C API + free functions
│   │   ├── database_update.cpp    # All update C API
│   │   element.cpp                # Element C API
│   │   └── lua_runner.cpp         # LuaRunner C API
│   └── blob/                      # Blob implementation files
│       ├── blob.cpp
│       ├── blob_csv.cpp
│       ├── blob_metadata.cpp
│       ├── dimension.cpp
│       └── time_properties.cpp
├── bindings/
│   ├── julia/                     # Julia bindings (Quiver.jl package)
│   │   ├── src/
│   │   │   ├── Quiver.jl          # Module entry point — includes/exports all submodules
│   │   │   ├── c_api.jl           # Auto-generated C declarations (do not edit manually)
│   │   │   ├── database.jl        # Database struct, lifecycle (from_schema, close!, describe)
│   │   │   ├── database_create.jl # create_element!, update_element!
│   │   │   ├── database_csv.jl    # export_csv, import_csv
│   │   │   ├── database_delete.jl # delete_element!
│   │   │   ├── database_metadata.jl # get_*/list_* metadata
│   │   │   ├── database_query.jl  # query_string, query_integer, query_float, query_date_time
│   │   │   ├── database_read.jl   # All read operations + read_all_* helpers
│   │   │   ├── database_update.jl # All update operations
│   │   │   ├── date_time.jl       # DateTime parsing helpers
│   │   │   ├── element.jl         # Element struct + set methods
│   │   │   ├── exceptions.jl      # DatabaseException type + check() helper
│   │   │   └── lua_runner.jl      # LuaRunner struct + run()
│   │   ├── test/                  # Julia test files (mirror C++ test organization)
│   │   ├── generator/             # FFI binding generator scripts
│   │   └── binary_builder/        # BinaryBuilder.jl cross-compilation scripts
│   ├── dart/                      # Dart bindings (quiver package)
│   │   ├── lib/src/
│   │   │   ├── database.dart      # Database class — lifecycle + part includes
│   │   │   ├── database_create.dart
│   │   │   ├── database_csv.dart
│   │   │   ├── database_delete.dart
│   │   │   ├── database_metadata.dart
│   │   │   ├── database_query.dart
│   │   │   ├── database_read.dart # All read operations including read_all_* helpers
│   │   │   ├── database_relations.dart
│   │   │   ├── database_update.dart
│   │   │   ├── date_time.dart     # DateTime parsing helpers
│   │   │   ├── element.dart       # Element class + set methods
│   │   │   ├── exceptions.dart    # QuiverException + check() helper
│   │   │   ├── lua_runner.dart    # LuaRunner class
│   │   │   └── ffi/
│   │   │       ├── bindings.dart  # Auto-generated Dart FFI declarations (do not edit)
│   │   │       └── library_loader.dart # DLL/SO loading logic
│   │   ├── test/                  # Dart test files
│   │   ├── generator/             # FFI binding generator scripts
│   │   └── hook/                  # Dart native hook for DLL build
│   └── python/                    # Python bindings — stub only, not implemented
│       ├── src/__init__.py
│       └── tests/unit/test_database.py
├── tests/                         # C++ test suite (doctest)
│   ├── test_database_lifecycle.cpp
│   ├── test_database_create.cpp
│   ├── test_database_read.cpp
│   ├── test_database_update.cpp
│   ├── test_database_delete.cpp
│   ├── test_database_query.cpp
│   ├── test_database_relations.cpp
│   ├── test_database_time_series.cpp
│   ├── test_database_errors.cpp
│   ├── test_c_api_database_*.cpp  # C API tests (same categories)
│   ├── test_lua_runner.cpp
│   ├── test_c_api_lua_runner.cpp
│   ├── test_schema_validator.cpp
│   ├── test_migrations.cpp
│   ├── test_element.cpp
│   ├── test_row_result.cpp
│   ├── test_issues.cpp            # Regression tests for filed issues
│   ├── test_utils.h               # Shared test helpers
│   ├── schemas/
│   │   ├── valid/                 # Valid SQL schemas used across all tests
│   │   │   ├── basic.sql
│   │   │   ├── collections.sql
│   │   │   ├── relations.sql
│   │   │   ├── mixed_time_series.sql
│   │   │   └── multi_time_series.sql
│   │   ├── invalid/               # Schemas that must fail SchemaValidator
│   │   ├── migrations/            # Versioned migration directories (1/, 2/, 3/)
│   │   └── issues/                # Schemas reproducing specific issue regressions
│   └── CMakeLists.txt
├── cmake/                         # CMake modules
│   ├── CompilerOptions.cmake      # Compiler flags and warnings
│   ├── Dependencies.cmake         # Find/fetch SQLite3, spdlog, sol2, doctest
│   └── Platform.cmake             # Windows/Linux platform detection
├── scripts/                       # Build and utility scripts
│   ├── build-all.bat              # Build everything + run all tests
│   ├── test-all.bat               # Run all tests (assumes already built)
│   ├── format.bat                 # Run clang-format
│   ├── tidy.bat                   # Run clang-tidy
│   └── generator.bat              # Run both FFI generators
├── docs/                          # Human documentation
│   ├── introduction.md
│   ├── attributes.md
│   ├── migrations.md
│   ├── rules.md
│   └── time_series.md
├── .planning/codebase/            # GSD codebase analysis documents
├── CMakeLists.txt                 # Root CMake — sets C++20, configures subdirs
├── CMakePresets.json              # CMake preset configurations
├── .clang-format                  # Clang-format style rules
├── .clang-tidy                    # Clang-tidy check configuration
└── CLAUDE.md                      # Project instructions for AI agents
```

## Directory Purposes

**`include/quiver/`:**
- Purpose: Public C++ API headers. Everything a C++ consumer needs.
- Key files: `database.h` (full API surface), `element.h` (builder), `schema.h` (introspection), `value.h` (variant type).

**`include/quiver/c/`:**
- Purpose: C API headers for FFI consumption. `options.h` is shared between C++ and C layers.
- Key files: `common.h` (error codes, utility functions), `database.h` (all database C API), `element.h`, `lua_runner.h`.

**`src/`:**
- Purpose: C++ implementation. One `.cpp` per concern area; no monolithic file.
- Key files: `database_impl.h` (Impl struct, TransactionGuard), `database_internal.h` (shared templates for grouped reads), `lua_runner.cpp` (all sol2 Lua bindings).

**`src/c/`:**
- Purpose: C API implementation. Mirrors the C++ source split exactly.
- Key files: `internal.h` (opaque struct definitions + QUIVER_REQUIRE macro), `database_helpers.h` (marshaling templates, `strdup_safe`, metadata converters).

**`bindings/julia/src/`:**
- Purpose: Julia module. `c_api.jl` is auto-generated — do not edit. All other files are hand-written wrappers.
- Key files: `Quiver.jl` (module root), `exceptions.jl` (check() helper), `database.jl` (Database struct + lifecycle).

**`bindings/dart/lib/src/`:**
- Purpose: Dart package. `ffi/bindings.dart` is auto-generated — do not edit. `database.dart` uses `part` directives to include the other database files.
- Key files: `ffi/bindings.dart` (auto-generated), `ffi/library_loader.dart` (DLL loading), `database.dart` (Database class + part includes), `exceptions.dart` (check() helper).

**`tests/schemas/`:**
- Purpose: Shared SQL schema files used by C++, C API, Julia, and Dart tests. All bindings reference schemas from this directory.
- Key files: `valid/basic.sql` (most common test schema), `valid/collections.sql`, `valid/relations.sql`, `valid/mixed_time_series.sql`.

## Key File Locations

**Entry Points:**
- `include/quiver/database.h`: C++ API — all method signatures.
- `include/quiver/c/database.h`: C API — all FFI function declarations.
- `bindings/julia/src/Quiver.jl`: Julia module root.
- `bindings/dart/lib/src/database.dart`: Dart Database class.

**Configuration:**
- `CMakeLists.txt`: Root build configuration (C++20, output dirs, subdirs).
- `CMakePresets.json`: Debug/Release CMake presets.
- `.clang-format`: Code formatting rules.
- `.clang-tidy`: Static analysis configuration.
- `include/quiver/c/options.h`: `quiver_database_options_t` — shared by C++ and C API.

**Core Logic (C++):**
- `src/database_impl.h`: `Database::Impl` definition — sqlite3 handle, logger, Schema, TypeValidator, TransactionGuard.
- `src/database_internal.h`: Shared templates for grouped value reads.
- `src/schema.cpp`: Table classification, naming conventions, column introspection.
- `src/schema_validator.cpp`: Quiver schema convention enforcement.
- `src/lua_runner.cpp`: All sol2 Lua bindings (~1700 lines).

**C API Internal Utilities:**
- `src/c/internal.h`: Opaque struct definitions and QUIVER_REQUIRE macro.
- `src/c/database_helpers.h`: `strdup_safe()`, marshaling templates, `convert_scalar_to_c()`, `convert_group_to_c()`.

**Testing:**
- `tests/test_utils.h`: Shared C++ test helpers.
- `tests/schemas/valid/`: SQL schemas used by all test suites.
- `bindings/julia/test/fixture.jl`: Julia test fixture helper.

## Naming Conventions

**Files (C++):**
- Headers: `lowercase_underscore.h`
- Implementations: `lowercase_underscore.cpp`
- Per-concern split: `database_{concern}.cpp` (e.g., `database_read.cpp`, `database_time_series.cpp`)
- Private implementation headers: suffix `_impl.h` (e.g., `database_impl.h`) or `_internal.h` (e.g., `database_internal.h`)

**Files (C API):**
- Same pattern as C++: `database_{concern}.cpp` under `src/c/`
- Shared helpers: `database_helpers.h`, `internal.h` (no `database_` prefix as they span concerns)

**Files (Julia):**
- Module: `Quiver.jl`
- Feature files: `database_{concern}.jl`, matching the C++ split
- Auto-generated: `c_api.jl` (do not edit)

**Files (Dart):**
- Feature files: `database_{concern}.dart`
- Auto-generated: `ffi/bindings.dart` (do not edit)
- Test files: `database_{concern}_test.dart`

**Test files (C++):**
- C++ core tests: `test_{subject}.cpp` (e.g., `test_database_read.cpp`)
- C API tests: `test_c_api_{subject}.cpp` (e.g., `test_c_api_database_read.cpp`)

**Directories:**
- Collection tables by convention: `{Collection}` (PascalCase, matches SQL table name)
- Vector tables: `{Collection}_vector_{name}`
- Set tables: `{Collection}_set_{name}`
- Time series tables: `{Collection}_time_series_{name}`

## Where to Add New Code

**New C++ Database Method:**
1. Add declaration to `include/quiver/database.h`.
2. Implement in the appropriate `src/database_{concern}.cpp` (or create a new file if the concern is genuinely new).
3. Add C API declaration to `include/quiver/c/database.h`.
4. Implement in `src/c/database_{concern}.cpp`.
5. Regenerate FFI bindings: `scripts/generator.bat` (runs both Julia and Dart generators).
6. Add Julia wrapper in `bindings/julia/src/database_{concern}.jl`.
7. Add Dart wrapper in `bindings/dart/lib/src/database_{concern}.dart`.
8. Add Lua binding in `src/lua_runner.cpp` (inside `bind_database()`).
9. Add C++ tests in `tests/test_database_{concern}.cpp`.
10. Add C API tests in `tests/test_c_api_database_{concern}.cpp`.

**New Collection Schema:**
- Add to an existing `.sql` file in `tests/schemas/valid/`, or create a new file there if the test domain warrants it.
- All test suites (C++, C API, Julia, Dart) reference schemas from `tests/schemas/`.

**New Binding-Only Convenience Method:**
- Julia: Add to the appropriate `bindings/julia/src/database_{concern}.jl`.
- Dart: Add as a method in the matching `part` file (`bindings/dart/lib/src/database_{concern}.dart`).
- These do NOT require C API changes or regeneration.

**New Error Type:**
- Do NOT add new error message formats. Use one of the three existing patterns defined in `CLAUDE.md`. Error text is defined in `src/*.cpp` (C++ layer) only; bindings never craft messages.

**New Utility/Helper (C++ layer):**
- Shared read utilities: `src/database_internal.h`
- C API marshaling helpers: `src/c/database_helpers.h`
- Do not introduce new headers unless the scope is genuinely cross-cutting.

## Special Directories

**`build/`:**
- Purpose: CMake build output. Contains compiled binaries in `build/bin/` and libraries in `build/lib/`.
- Generated: Yes.
- Committed: No.

**`bindings/julia/generator/` and `bindings/dart/generator/`:**
- Purpose: FFI binding generators — run after C API changes to regenerate `c_api.jl` and `ffi/bindings.dart`.
- Generated output: `bindings/julia/src/c_api.jl`, `bindings/dart/lib/src/ffi/bindings.dart`.
- Committed: Yes (both the generator and the generated output).

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents consumed by plan/execute commands.
- Generated: Yes (by GSD map-codebase).
- Committed: Yes.

**`tests/schemas/`:**
- Purpose: Canonical SQL schema fixtures shared across all language test suites.
- Generated: No — hand-authored.
- Committed: Yes.

---

*Structure analysis: 2026-02-20*
