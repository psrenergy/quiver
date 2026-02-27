# Codebase Structure

**Analysis Date:** 2026-02-26

## Directory Layout

```
quiver2/                          # Project root
├── include/
│   └── quiver/                   # C++ public headers (installed with library)
│       ├── database.h            # Database class — main API
│       ├── element.h             # Element builder for create/update
│       ├── attribute_metadata.h  # ScalarMetadata, GroupMetadata types
│       ├── options.h             # DatabaseOptions, CSVOptions types
│       ├── value.h               # Value variant type
│       ├── result.h / row.h      # Internal query result types
│       ├── schema.h              # Schema, TableDefinition, ColumnDefinition
│       ├── schema_validator.h    # Schema convention validator
│       ├── type_validator.h      # Runtime write type checker
│       ├── migration.h           # Single migration (version + SQL path)
│       ├── migrations.h          # Collection of migrations, pending query
│       ├── data_type.h           # DataType enum + helpers
│       ├── lua_runner.h          # LuaRunner scripting host
│       ├── export.h              # QUIVER_API dllexport/visibility macros
│       ├── quiver.h              # Umbrella header
│       ├── common.h              # (shared utilities)
│       └── blob/                 # Blob subsystem headers (experimental)
│           ├── blob.h
│           ├── blob_csv.h
│           ├── blob_metadata.h
│           ├── dimension.h
│           ├── time_constants.h
│           └── time_properties.h
│   └── quiver/c/                 # C API public headers (FFI surface)
│       ├── common.h              # Error codes, quiver_error_t, utility fns
│       ├── options.h             # quiver_database_options_t, quiver_csv_options_t
│       ├── database.h            # All database C API declarations
│       ├── element.h             # Element C API declarations
│       └── lua_runner.h          # LuaRunner C API declarations
├── src/                          # C++ implementation
│   ├── database.cpp              # Database constructor, logger, execute()
│   ├── database_impl.h           # Database::Impl struct + TransactionGuard
│   ├── database_internal.h       # Internal templates (read_grouped_values_*)
│   ├── database_create.cpp       # create_element, update_element
│   ├── database_read.cpp         # All read_scalar/vector/set operations
│   ├── database_update.cpp       # All update_scalar/vector/set operations
│   ├── database_delete.cpp       # delete_element
│   ├── database_metadata.cpp     # get/list scalar/vector/set/time_series metadata
│   ├── database_time_series.cpp  # read/update time series group + files
│   ├── database_query.cpp        # query_string/integer/float
│   ├── database_csv_export.cpp   # export_csv
│   ├── database_csv_import.cpp   # import_csv
│   ├── database_describe.cpp     # describe()
│   ├── element.cpp               # Element builder implementation
│   ├── lua_runner.cpp            # LuaRunner + sol2 binding (big obj)
│   ├── migration.cpp             # Migration versioning
│   ├── migrations.cpp            # Migrations collection
│   ├── result.cpp / row.cpp      # Result/Row implementations
│   ├── schema.cpp                # Schema::from_database, table classification
│   ├── schema_validator.cpp      # SchemaValidator implementation
│   ├── type_validator.cpp        # TypeValidator implementation
│   ├── c/                        # C API shim implementation
│   │   ├── internal.h            # quiver_database/quiver_element structs, QUIVER_REQUIRE
│   │   ├── database_helpers.h    # Marshaling templates, strdup_safe, metadata converters
│   │   ├── database_options.h    # CSV options conversion helpers
│   │   ├── common.cpp            # quiver_version, quiver_get_last_error, quiver_set_last_error
│   │   ├── database.cpp          # Lifecycle: open, close, factory methods
│   │   ├── database_create.cpp   # create_element, update_element C wrappers
│   │   ├── database_read.cpp     # Read ops + co-located free functions
│   │   ├── database_update.cpp   # Update ops C wrappers
│   │   ├── database_delete.cpp   # delete_element C wrapper
│   │   ├── database_metadata.cpp # Metadata ops + co-located free functions
│   │   ├── database_time_series.cpp # Time series ops + co-located free functions
│   │   ├── database_query.cpp    # Query ops + parameterized variants
│   │   ├── database_csv_export.cpp  # CSV export C wrapper
│   │   ├── database_csv_import.cpp  # CSV import C wrapper
│   │   ├── database_transaction.cpp # begin/commit/rollback/in_transaction
│   │   ├── element.cpp           # Element C API
│   │   └── lua_runner.cpp        # LuaRunner C API
│   ├── blob/                     # Blob subsystem implementation (experimental)
│   │   ├── blob.cpp
│   │   ├── blob_csv.cpp
│   │   ├── blob_metadata.cpp
│   │   ├── dimension.cpp
│   │   └── time_properties.cpp
│   └── utils/                    # Internal utility headers (not installed)
│       ├── string.h
│       └── datetime.h
├── bindings/
│   ├── julia/                    # Julia package (Quiver.jl)
│   │   ├── src/
│   │   │   ├── Quiver.jl         # Module root, exports
│   │   │   ├── c_api.jl          # Generated @ccall declarations (module C)
│   │   │   ├── database.jl       # Database struct + lifecycle
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
│   │   │   ├── lua_runner.jl
│   │   │   ├── date_time.jl      # DateTime convenience wrappers
│   │   │   ├── helper_maps.jl    # read_all_scalars_by_id etc.
│   │   │   └── exceptions.jl
│   │   ├── test/
│   │   └── generator/            # c_api.jl generator script
│   ├── dart/                     # Dart package (quiver)
│   │   ├── lib/src/
│   │   │   ├── database.dart     # Database class (uses part files)
│   │   │   ├── database_*.dart   # Partial classes (part of database.dart)
│   │   │   ├── element.dart
│   │   │   ├── lua_runner.dart
│   │   │   ├── date_time.dart
│   │   │   ├── exceptions.dart
│   │   │   └── ffi/
│   │   │       ├── bindings.dart       # Generated FFI bindings
│   │   │       └── library_loader.dart # DLL/so loading
│   │   ├── test/
│   │   ├── hook/                 # Native assets build hook
│   │   └── generator/            # bindings.dart generator script
│   └── python/                   # Python package (quiverdb)
│       ├── src/quiverdb/
│       │   ├── __init__.py
│       │   ├── _c_api.py         # Hand-written CFFI cdef declarations
│       │   ├── _declarations.py  # Generator output (reference only)
│       │   ├── _loader.py        # Library load strategy (bundled vs dev)
│       │   ├── _helpers.py       # check(), decode_string() etc.
│       │   ├── database.py       # Database class
│       │   ├── database_csv_export.py
│       │   ├── database_csv_import.py
│       │   ├── database_options.py
│       │   ├── element.py
│       │   ├── metadata.py
│       │   └── exceptions.py
│       ├── tests/
│       └── generator/            # _declarations.py generator script
├── tests/                        # C++ and C API tests (Catch2)
│   ├── test_database_*.cpp       # C++ core tests by feature
│   ├── test_c_api_database_*.cpp # C API tests by feature
│   ├── test_element.cpp
│   ├── test_lua_runner.cpp
│   ├── test_migrations.cpp
│   ├── test_row_result.cpp
│   ├── test_schema_validator.cpp
│   ├── test_issues.cpp           # Regression tests for specific issues
│   ├── test_utils.h              # Shared test helpers
│   ├── benchmark/                # Transaction benchmark (standalone exe)
│   ├── sandbox/                  # Ad-hoc exploration
│   └── schemas/
│       ├── valid/                # Shared SQL schemas used by tests
│       │   ├── basic.sql
│       │   ├── all_types.sql
│       │   ├── collections.sql
│       │   ├── relations.sql
│       │   ├── csv_export.sql
│       │   ├── mixed_time_series.sql
│       │   └── multi_time_series.sql
│       ├── invalid/              # Schemas that fail validation (error tests)
│       ├── migrations/           # Migration directories (1/, 2/, 3/)
│       └── issues/               # Schemas for specific bug reproductions
├── cmake/                        # CMake modules
│   ├── CompilerOptions.cmake
│   ├── Dependencies.cmake
│   ├── Platform.cmake
│   └── quiverConfig.cmake.in
├── scripts/                      # Build and test scripts
│   ├── build-all.bat
│   ├── test-all.bat
│   ├── tidy.bat
│   └── format.bat
├── docs/                         # Documentation
│   ├── introduction.md
│   ├── attributes.md
│   ├── migrations.md
│   ├── time_series.md
│   └── rules.md
├── CMakeLists.txt                # Root build file (v0.5.0, C++20)
├── CMakePresets.json
└── CLAUDE.md                     # Project instructions and architecture reference
```

## Directory Purposes

**`include/quiver/`:**
- Purpose: Installed public C++ API headers; consumers use these
- Contains: Class declarations, value types, enums, inline helpers
- Key files: `database.h` (main API), `element.h` (write builder), `schema.h` (introspection types), `value.h` (Value variant)

**`include/quiver/c/`:**
- Purpose: C-linkage FFI headers; installed alongside C++ headers; consumed by binding generators
- Contains: All C API function declarations, opaque handle typedefs, C struct types
- Key files: `common.h` (error codes, utilities), `options.h` (option structs), `database.h` (all database operations)

**`src/`:**
- Purpose: C++ implementation; one `.cpp` per logical feature group for `Database`
- Contains: Implementation of all public C++ classes; internal headers (`database_impl.h`, `database_internal.h`)
- Key files: `database_impl.h` (Impl struct + TransactionGuard), `database_internal.h` (read templates)

**`src/c/`:**
- Purpose: C API shim; each file mirrors its C++ counterpart
- Contains: `extern "C"` functions wrapping C++ calls; alloc/free pairs co-located
- Key files: `internal.h` (handle structs, QUIVER_REQUIRE macro), `database_helpers.h` (marshaling templates)

**`src/blob/`:**
- Purpose: Experimental binary flat-file subsystem; independent of database layer
- Contains: Blob file I/O, TOML metadata serialization, dimension management

**`src/utils/`:**
- Purpose: Internal-only utility headers; not installed
- Contains: `string.h` (string utilities), `datetime.h` (datetime parsing helpers)

**`bindings/julia/src/`:**
- Purpose: Julia package implementation; all files are included in `Quiver.jl` module
- Contains: `c_api.jl` (generated `@ccall` declarations in sub-module `C`); all other `.jl` files wrap C API calls

**`bindings/dart/lib/src/`:**
- Purpose: Dart package implementation using `part` directives
- Contains: `database.dart` is the main class; `database_*.dart` files are `part of` it; `ffi/` holds generated bindings and loader

**`bindings/python/src/quiverdb/`:**
- Purpose: Python package using CFFI ABI-mode
- Contains: `_c_api.py` (CFFI `ffi.cdef` declarations), `_loader.py` (bundled vs dev load strategy), `database.py` (main class, inherits from CSV mixins)

**`tests/schemas/valid/`:**
- Purpose: Shared SQL schema files referenced by both C++ and C API tests; single source of truth
- Contains: Schema files covering all attribute types, collections, relations, time series
- All bindings reference schemas from this directory via relative path

**`tests/schemas/migrations/`:**
- Purpose: Numbered migration subdirectories for migration tests
- Contains: `1/up.sql`, `2/up.sql`, `3/up.sql` etc.

## Key File Locations

**Entry Points:**
- `include/quiver/database.h`: C++ Database class public API
- `include/quiver/c/database.h`: Complete C API function declarations
- `src/database.cpp`: Database constructor, factory methods, internal `execute()`
- `src/c/database.cpp`: C API lifecycle functions (open, close, from_schema, from_migrations)

**Configuration:**
- `CMakeLists.txt`: Root build; options `QUIVER_BUILD_SHARED`, `QUIVER_BUILD_TESTS`, `QUIVER_BUILD_C_API`
- `src/CMakeLists.txt`: Source lists for both `libquiver` and `libquiver_c` targets
- `.clang-format`: Code formatting rules
- `.clang-tidy`: Static analysis rules

**Core Logic (C++ layer):**
- `src/database_impl.h`: `Database::Impl` struct, `TransactionGuard`, FK resolution
- `src/database_internal.h`: `read_grouped_values_all<T>`, `read_grouped_values_by_id<T>` templates
- `src/schema.cpp`: Schema introspection — table classification, table naming conventions
- `src/schema_validator.cpp`: Schema convention validation at open time

**Marshaling (C API layer):**
- `src/c/internal.h`: Handle structs (`quiver_database`, `quiver_element`), `QUIVER_REQUIRE` macro
- `src/c/database_helpers.h`: `read_scalars_impl<T>`, `read_vectors_impl<T>`, `strdup_safe`, metadata converters

**Testing:**
- `tests/test_utils.h`: Shared test helpers (temp file paths, etc.)
- `tests/schemas/valid/`: SQL schemas used across all test files

## Naming Conventions

**C++ source files:**
- Pattern: `database_{feature}.cpp` — one file per Database method group
- Examples: `database_create.cpp`, `database_read.cpp`, `database_time_series.cpp`

**C API source files:**
- Pattern: Same as C++ counterpart, located under `src/c/`
- Examples: `src/c/database_create.cpp` mirrors `src/database_create.cpp`

**C++ headers (public):**
- Pattern: `snake_case.h` under `include/quiver/`
- Namespace: `quiver::`

**C API headers:**
- Pattern: Same base name as C++ header; under `include/quiver/c/`
- All symbols prefixed: `quiver_` (functions), `quiver_*_t` (types), `QUIVER_*` (macros/enums)

**Binding source files:**
- Pattern: Mirror C++ decomposition — `database_read.jl`, `database_read.dart`, etc.
- Julia: `snake_case.jl`; Dart: `snake_case.dart`; Python: `snake_case.py`

**Test files:**
- C++ core tests: `test_database_{feature}.cpp`
- C API tests: `test_c_api_database_{feature}.cpp`
- Both in `tests/` flat (no subdirectory)

**Schema files:**
- Valid schemas: `tests/schemas/valid/{name}.sql`
- Invalid schemas: `tests/schemas/invalid/{name}.sql`
- Migration directories: `tests/schemas/migrations/{N}/up.sql`

## Where to Add New Code

**New Database method (C++ core):**
- Declare in `include/quiver/database.h`
- Implement in the appropriate `src/database_{feature}.cpp` (or create a new one matching the pattern)
- Add `Database::Impl` helpers in `src/database_impl.h` if needed

**New C API function:**
- Declare in `include/quiver/c/database.h`
- Implement in `src/c/database_{feature}.cpp` (mirror the C++ file naming)
- Follow pattern: `QUIVER_REQUIRE(...)` → `try { db->db.method(); return QUIVER_OK; } catch (...) { quiver_set_last_error(...); return QUIVER_ERROR; }`
- Co-locate any free functions in the same `.cpp` as the allocating function

**New Julia binding method:**
- Add to `bindings/julia/src/database_{feature}.jl`
- Regenerate `c_api.jl` if C API changed: `bindings/julia/generator/generator.bat`

**New Dart binding method:**
- Add to `bindings/dart/lib/src/database_{feature}.dart` as a `part of` file
- Regenerate `ffi/bindings.dart` if C API changed: `bindings/dart/generator/generator.bat`

**New Python binding method:**
- Add to `bindings/python/src/quiverdb/database.py` or the appropriate module
- Update `_c_api.py` with new CFFI cdef if C API changed
- Regenerate `_declarations.py` reference: `bindings/python/generator/generator.bat`

**New test schema:**
- Add `.sql` to `tests/schemas/valid/`
- Reference from tests using relative path from test binary location

**New test file:**
- C++ core: `tests/test_database_{feature}.cpp`
- C API: `tests/test_c_api_database_{feature}.cpp`

## Special Directories

**`src/c/` (C API shim):**
- Purpose: All C-linkage implementation; compiled into `libquiver_c` separately from `libquiver`
- Generated: No (hand-written, though generators create the declarations referenced)
- Committed: Yes

**`bindings/*/generator/`:**
- Purpose: Scripts that regenerate FFI binding declarations from C headers
- Generated: No (the generators themselves are committed; their output is also committed)
- Committed: Yes (both generator scripts and their output)

**`bindings/python/.venv/`:**
- Purpose: Python virtual environment for development and testing
- Generated: Yes (via `pip install -e .` in test.bat)
- Committed: No (in `.gitignore`)

**`build/`:**
- Purpose: CMake build output — compiled binaries, DLLs
- Generated: Yes
- Committed: No

**`tests/schemas/issues/`:**
- Purpose: Schemas that reproduce specific filed issues (named `issue{N}/`)
- Committed: Yes; used by `test_issues.cpp`

---

*Structure analysis: 2026-02-26*
