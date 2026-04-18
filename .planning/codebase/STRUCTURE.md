# Codebase Structure

**Analysis Date:** 2026-04-18

## Directory Layout

```
quiver4/
├── CMakeLists.txt              # Top-level CMake (C++20, options, install rules)
├── CMakePresets.json           # Preset build configurations
├── CLAUDE.md                   # Project guidelines (authoritative)
├── README.md                   # Project readme
├── LICENSE
├── .clang-format               # Code formatting config
├── .clang-tidy                 # Lint config
├── .clangd
├── .pre-commit-config.yaml
├── codecov.yml
├── .gitignore                  # Excludes build/, .dart_tool/, .venv/, *.db, *.log
├── .github/                    # CI workflows (ci.yml, build-wheels.yml, publish.yml)
├── .planning/                  # Planning output (this directory; mostly git-ignored in practice)
├── cmake/                      # CompilerOptions.cmake, Dependencies.cmake, Platform.cmake
├── include/                    # Public C++ and C headers
│   └── quiver/
│       ├── *.h                 # Core C++ public headers (namespace quiver::)
│       ├── binary/             # Binary subsystem headers
│       └── c/                  # C API (extern "C", FFI)
│           └── binary/
├── src/                        # C++ implementation
│   ├── *.cpp, *.h              # Core C++ impl (database, schema, lua_runner, ...)
│   ├── binary/                 # Binary subsystem impl
│   ├── c/                      # C API impl
│   │   └── binary/
│   ├── cli/                    # quiver_cli entry point
│   └── utils/                  # Shared helpers (datetime.h, string.h)
├── tests/                      # C++ and C API tests (GoogleTest)
│   ├── test_*.cpp              # C++ core tests
│   ├── test_c_api_*.cpp        # C API tests
│   ├── schemas/                # Shared SQL fixtures (valid/, invalid/, migrations/, issues/)
│   ├── benchmark/              # quiver_benchmark executable source
│   ├── sandbox/                # quiver_sandbox scratchpad
│   ├── test_utils.h            # Shared GoogleTest helpers
│   └── CMakeLists.txt
├── bindings/                   # Language-specific wrappers over the C API
│   ├── julia/                  # Quiver.jl
│   ├── dart/                   # quiverdb (pub)
│   ├── python/                 # quiverdb (pip, CFFI ABI-mode)
│   ├── js/                     # @psrenergy/quiver (Deno)
│   └── js-wasm/                # (placeholder; empty)
├── scripts/                    # Cross-cutting batch scripts (build-all.bat, test-all.bat, ...)
├── example/                    # Standalone usage examples (example1.lua, example1.bat)
├── docs/                       # Hand-written reference docs (introduction.md, rules.md, ...)
└── build/                      # CMake output (gitignored)
    ├── bin/                    # Compiled executables + DLLs
    └── lib/                    # Compiled static libs
```

## Directory Purposes

**`include/quiver/`** (Public C++ headers, `namespace quiver`):
- `database.h` — main `Database` class (Pimpl).
- `element.h` — fluent `Element` builder (value type).
- `options.h` — `DatabaseOptions`, `CSVOptions`, `LogLevel`, `default_csv_options()`.
- `attribute_metadata.h` — `ScalarMetadata`, `GroupMetadata`.
- `data_type.h` — `DataType` enum (Integer, Real, Text, DateTime).
- `value.h` — `Value = std::variant<nullptr_t, int64_t, double, std::string>`.
- `row.h`, `result.h` — query row/result containers.
- `schema.h` — `Schema`, `TableDefinition`, `ColumnDefinition`, `ForeignKey`, `Index`, `GroupTableType`.
- `schema_validator.h`, `type_validator.h` — validators.
- `migration.h`, `migrations.h` — migration loader.
- `lua_runner.h` — `LuaRunner` (Pimpl).
- `export.h` — `QUIVER_API` macro for DLL export/import.
- `quiver.h` — aggregate include header.

**`include/quiver/binary/`** (Binary subsystem C++ headers):
- `binary_file.h` — `BinaryFile` (Pimpl).
- `binary_metadata.h` — `BinaryMetadata` (plain struct with TOML serialization).
- `csv_converter.h` — `CSVConverter` (Pimpl).
- `dimension.h` — `Dimension` struct with optional `TimeProperties`.
- `time_properties.h` — `TimeProperties`, `TimeFrequency` enum.
- `time_constants.h` — time-dimension size constraints.

**`include/quiver/c/`** (C API headers, `extern "C"`):
- `common.h` — `quiver_error_t`, `QUIVER_C_API` export macro, `quiver_version`, `quiver_get_last_error`, `quiver_clear_last_error`.
- `options.h` — `quiver_database_options_t`, `quiver_csv_options_t`, `quiver_log_level_t`, defaults.
- `database.h` — all `quiver_database_*` function declarations (lifecycle, CRUD, read/update, metadata, queries, transactions, time series).
- `element.h` — `quiver_element_t` opaque handle + builder functions.
- `lua_runner.h` — Lua script execution.

**`include/quiver/c/binary/`** (Binary C API headers):
- `binary_file.h` — `quiver_binary_file_t` handle, `open_read`, `open_write`, `close`, `read`, `write`, getters, free functions.
- `binary_metadata.h` — `quiver_binary_metadata_t` handle + flat `quiver_dimension_t`, `quiver_time_properties_t` structs, builders, getters, TOML serialization.
- `csv_converter.h` — `bin_to_csv` / `csv_to_bin`.

**`src/`** (C++ core implementation):
- Database partitioned by concern, one file per concern — mirrors the test suite:
  - `database.cpp` — lifecycle, `execute()`, logger setup, factory methods, transaction control.
  - `database_create.cpp` — `create_element`.
  - `database_read.cpp` — all read operations.
  - `database_update.cpp` — all update operations.
  - `database_delete.cpp` — `delete_element`.
  - `database_metadata.cpp` — scalar/vector/set metadata getters + list functions.
  - `database_time_series.cpp` — time series read/update, files table.
  - `database_query.cpp` — parameterized query methods.
  - `database_csv_export.cpp` / `database_csv_import.cpp` — CSV operations.
  - `database_describe.cpp` — schema pretty-printer.
- `database_impl.h` — `Database::Impl` struct (Pimpl private header, includes sqlite3). Contains `TransactionGuard`, `ResolvedElement`, shared helpers (`require_collection`, `resolve_element_fk_labels`, `insert_group_data`).
- `database_internal.h` — additional internal helpers.
- `element.cpp` — `Element` class.
- `lua_runner.cpp` — `LuaRunner::Impl` using sol2; binds all Database methods as Lua usertype members.
- `migration.cpp`, `migrations.cpp` — migration loader.
- `result.cpp`, `row.cpp` — query result containers.
- `schema.cpp`, `schema_validator.cpp`, `type_validator.cpp` — schema introspection and validation.

**`src/binary/`** (Binary subsystem implementation):
- `binary_file.cpp` — `BinaryFile::Impl` (Pimpl) with the module-local `write_registry` (`std::unordered_set<std::string>`).
- `binary_metadata.cpp` — TOML (de)serialization via `tomlplusplus`, `from_element`, validation.
- `csv_converter.cpp` — bidirectional conversion.
- `dimension.cpp`, `time_properties.cpp` — value-type impls.
- `binary_utils.h` — internal helpers (`QVR_EXTENSION`, `TOML_EXTENSION` constants).

**`src/c/`** (C API implementation — partitioned to match `include/quiver/c/database.h` sections):
- `common.cpp` — `quiver_set_last_error` (thread-local), `quiver_get_last_error`, `quiver_clear_last_error`, `quiver_version`.
- `options.cpp` — `quiver_database_options_default`, `quiver_csv_options_default`.
- `database.cpp` — lifecycle: `open`, `from_schema`, `from_migrations`, `close`, `is_healthy`, `path`, `current_version`, `describe`.
- `database_transaction.cpp` — begin / commit / rollback / in_transaction.
- `database_create.cpp` — create_element.
- `database_read.cpp` — all read operations + co-located free functions (`free_integer_array`, `free_float_array`, `free_string_array`, `free_string`, `free_integer_vectors`, etc.).
- `database_update.cpp` — all update operations.
- `database_delete.cpp` — delete_element.
- `database_metadata.cpp` — metadata get + list + co-located free functions.
- `database_query.cpp` — parameterized queries with typed-array marshaling (`convert_params` helper).
- `database_time_series.cpp` — columnar time series read/update + free.
- `database_csv_export.cpp`, `database_csv_import.cpp` — CSV bridge.
- `database_options.h` — internal C-to-C++ option converters (`convert_database_options`, `convert_csv_options`).
- `database_helpers.h` — marshaling templates (`read_scalars_impl`, `read_vectors_impl`, `free_vectors_impl`, `copy_strings_to_c`, `convert_scalar_to_c`, `convert_group_to_c`, `free_scalar_fields`, `free_group_fields`, `to_c_data_type`).
- `internal.h` — opaque handle struct definitions (`quiver_database`, `quiver_element`, `quiver_binary_file`, `quiver_binary_metadata`) + `QUIVER_REQUIRE` variadic null-check macro (1..9 args).
- `element.cpp` — `quiver_element_*` builder functions.
- `lua_runner.cpp` — `quiver_lua_runner_*` wrappers.

**`src/c/binary/`** (Binary C API implementation):
- `binary_file.cpp` — lifecycle + read/write + getters + free.
- `csv_converter.cpp` — bin↔CSV wrappers.
- `binary_metadata.cpp` — lifecycle + builders + getters + TOML serialization.

**`src/cli/`**:
- `main.cpp` — `quiver_cli` executable entry point (`argparse`-based, runs Lua scripts against a database).

**`src/utils/`**:
- `string.h` — `quiver::string::new_c_str()` (allocates C-string from `std::string`), `trim`.
- `datetime.h` — datetime parsing/formatting helpers used by CSV and time-series code.

**`tests/`** (C++ core + C API tests):
- C++ core tests (one file per concern, mirrors `src/database_*.cpp`):
  - `test_database_lifecycle.cpp`, `test_database_create.cpp`, `test_database_read.cpp`, `test_database_update.cpp`, `test_database_delete.cpp`.
  - `test_database_query.cpp`, `test_database_time_series.cpp`, `test_database_transaction.cpp`.
  - `test_database_csv_export.cpp`, `test_database_csv_import.cpp`, `test_database_errors.cpp`.
  - `test_element.cpp`, `test_row_result.cpp`, `test_schema_validator.cpp`, `test_migrations.cpp`.
  - `test_lua_runner.cpp`, `test_issues.cpp` (regression tests per GitHub issue).
- Binary C++ tests: `test_binary_file.cpp`, `test_binary_metadata.cpp`, `test_binary_time_properties.cpp`, `test_csv_converter.cpp`.
- C API tests (same naming pattern with `_c_api_` prefix):
  - `test_c_api_database_lifecycle.cpp`, `test_c_api_database_create.cpp`, `test_c_api_database_read.cpp`, `test_c_api_database_update.cpp`, `test_c_api_database_delete.cpp`.
  - `test_c_api_database_query.cpp`, `test_c_api_database_time_series.cpp`, `test_c_api_database_transaction.cpp`.
  - `test_c_api_database_csv_export.cpp`, `test_c_api_database_csv_import.cpp`, `test_c_api_database_metadata.cpp`.
  - `test_c_api_element.cpp`, `test_c_api_lua_runner.cpp`.
- Binary C API tests: `test_c_api_binary_file.cpp`, `test_c_api_binary_metadata.cpp`, `test_c_api_csv_converter.cpp`.
- `test_utils.h` — shared GoogleTest helpers.
- `CMakeLists.txt` — builds `quiver_tests`, `quiver_c_tests` (conditional), `quiver_benchmark`, `quiver_sandbox`.

**`tests/schemas/`** (SQL fixtures shared across C++ tests and all bindings):
- `valid/` — well-formed schemas: `basic.sql`, `all_types.sql`, `collections.sql`, `relations.sql`, `csv_export.sql`, `mixed_time_series.sql`, `multi_time_series.sql`, `composite_helpers.sql`, `describe_multi_group.sql`.
- `invalid/` — schemas that must fail validation: `duplicate_attribute_vector.sql`, `fk_actions.sql`, `label_not_unique.sql`, `label_not_null.sql`, `no_configuration.sql`, `set_no_unique.sql`, `vector_no_index.sql`, etc.
- `migrations/` — numbered migration directories (`1/up.sql + 1/down.sql`, `2/up.sql + 2/down.sql`, ...).
- `issues/` — issue-specific regression fixtures (`issue52/`, `issue70/`).

**`tests/benchmark/`** (`benchmark.cpp`) — compiled as `quiver_benchmark`; compares individual vs. batched transaction throughput; never run automatically.

**`tests/sandbox/`** (`sandbox.cpp`) — compiled as `quiver_sandbox`; scratchpad for quick manual testing. Links both `quiver` and `quiver_c`.

**`bindings/julia/`** (Quiver.jl — `@ccall` into `libquiver_c`):
- `Project.toml`, `Manifest.toml` — package manifest.
- `src/Quiver.jl` — module entry (`include`s per-concern files, re-exports public API).
- `src/c_api.jl` — generated `@ccall` bindings (via CEnum) + platform-specific library path resolution.
- `src/database_*.jl` — per-concern wrappers mirroring `src/c/database_*.cpp`:
  - `database.jl` (type + lifecycle), `database_create.jl`, `database_read.jl`, `database_update.jl`, `database_delete.jl`, `database_metadata.jl`, `database_query.jl`, `database_csv_export.jl`, `database_csv_import.jl`, `database_transaction.jl`, `database_options.jl`.
- `src/element.jl`, `src/date_time.jl`, `src/exceptions.jl`, `src/helper_maps.jl`, `src/lua_runner.jl`.
- `src/binary/` — binary subsystem wrappers: `Binary.jl` (submodule), `file.jl`, `metadata.jl`, `csv_converter.jl`.
- `test/` — per-concern Julia tests mirroring C++ tests: `test_database_*.jl`, `test_binary_*.jl`, `test_csv_converter.jl`, plus `fixture.jl` and `runtests.jl`. Shell/batch runners: `test.bat`, `test.sh`.
- `generator/` — ffi-header regeneration toolchain: `generator.jl`, `prologue.jl`, `epilogue.jl`, `generator.toml`, `generator.bat`, own `Project.toml`/`Manifest.toml`.
- `format/` — JuliaFormatter task (`format.jl`, `format.bat`).
- `revise/` — Revise-based interactive dev entry (`revise.jl`, `revise.bat`).
- `binary_builder/` — placeholder for BinaryBuilder integration.
- `.JuliaFormatter.toml`.

**`bindings/dart/`** (quiverdb — `dart:ffi`):
- `pubspec.yaml`, `pubspec.lock` — package manifest, ffigen config embedded.
- `analysis_options.yaml`, `ffigen.yaml`.
- `lib/quiverdb.dart` — public exports.
- `lib/src/database.dart` — `Database` class with `part`s for per-concern extensions (`database_create.dart`, `database_read.dart`, `database_update.dart`, `database_delete.dart`, `database_metadata.dart`, `database_query.dart`, `database_csv_export.dart`, `database_csv_import.dart`, `database_transaction.dart`, `database_options.dart`).
- `lib/src/element.dart`, `lib/src/metadata.dart`, `lib/src/date_time.dart`, `lib/src/exceptions.dart`, `lib/src/lua_runner.dart`.
- `lib/src/ffi/bindings.dart` — ffigen-generated raw FFI bindings.
- `lib/src/ffi/library_loader.dart` — platform-aware library loading (Native Assets → PATH fallback).
- `hook/build.dart` — Dart Native Assets hook; invokes CMake to build `quiver` + `quiver_c` targets.
- `generator/generator.bat` — regenerates `lib/src/ffi/bindings.dart` via ffigen.
- `test/*_test.dart` — per-concern tests mirroring C++ tests. Runners: `test.bat`, `test.sh`, `coverage.bat`, `coverage.sh`.
- `format.bat`.

**`bindings/python/`** (quiverdb — CFFI ABI-mode):
- `pyproject.toml` — scikit-build-core package, `dependencies = ["cffi>=2.0.0"]`, cibuildwheel config.
- `Makefile`, `ruff.toml`, `uv.lock`, `format.bat`.
- `src/quiverdb/__init__.py` — public exports.
- `src/quiverdb/database.py` — `Database` class; inherits `DatabaseCSVExport, DatabaseCSVImport` mixins for CSV.
- `src/quiverdb/database_csv_export.py`, `database_csv_import.py` — CSV mixins.
- `src/quiverdb/element.py`, `metadata.py`, `database_options.py`, `exceptions.py`, `lua_runner.py`.
- `src/quiverdb/_c_api.py` — hand-written CFFI `ffi.cdef(...)` block + `get_lib()`.
- `src/quiverdb/_declarations.py` — generator output (reference only).
- `src/quiverdb/_loader.py` — `load_library()` with bundled (`_libs/`) and development (system PATH) modes.
- `src/quiverdb/_helpers.py` — `check()`, `decode_string`, `decode_string_or_none`.
- `src/quiverdb/py.typed` — PEP 561 marker.
- `generator/generator.py`, `generator/generator.bat` — regenerates `_declarations.py`.
- `tests/test_database_*.py`, `tests/test_element.py`, `tests/test_lua_runner.py`, `tests/conftest.py`, `tests/test.bat`.

**`bindings/js/`** (@psrenergy/quiver — Deno FFI):
- `deno.json` — JSR publish config, `deno test` tasks.
- `biome.json` — formatter/lint config.
- `mod.ts` — public entry (re-exports from `src/index.ts`).
- `src/index.ts` — re-export hub.
- `src/database.ts` — `Database` class.
- `src/loader.ts` — Deno FFI symbol definitions grouped by concern (`lifecycleSymbols`, `elementSymbols`, `crudSymbols`, `readSymbols`, ...), platform-specific DLL resolution.
- `src/ffi-helpers.ts` — `toCString`, `allocPtrOut`, `readPtrOut`, `makeDefaultOptions`.
- `src/errors.ts` — `QuiverError`, `check()`.
- `src/create.ts`, `src/read.ts`, `src/query.ts`, `src/transaction.ts`, `src/metadata.ts`, `src/time-series.ts`, `src/csv.ts`, `src/introspection.ts`, `src/composites.ts`, `src/lua-runner.ts`.
- `src/types.ts` — TypeScript type aliases (`ScalarValue`, `ArrayValue`, etc.).
- `test/*.test.ts` — per-concern tests.
- `node_modules/` — included for `koffi` / `vitest` tooling (gitignored via npm conventions, but present in repo for some utilities).

**`bindings/js-wasm/`** — empty placeholder.

**`scripts/`** (Windows batch helpers — Unix counterparts live inside each binding):
- `build-all.bat` — CMake configure + build Debug/Release, then run C++/Julia/Dart/Python tests.
- `test-all.bat` — run all tests (assumes already built).
- `clean-all.bat`, `format.bat`, `generator.bat`, `tidy.bat`.
- `test-wheel.bat`, `test-wheel-install.bat`, `validate_wheel.py`, `validate_wheel_install.py` — Python wheel validation.

**`example/`**:
- `example1.lua`, `example1.bat` — Lua script example run through `quiver_cli`.

**`docs/`** — hand-written reference documentation:
- `introduction.md`, `rules.md`, `attributes.md`, `migrations.md`, `time_series.md`.

**`cmake/`**:
- `CompilerOptions.cmake` — compiler flags, warnings-as-errors, bigobj for sol2.
- `Dependencies.cmake` — FetchContent for sqlite3, spdlog, tomlplusplus, sol2, rapidcsv, argparse, GoogleTest.
- `Platform.cmake` — platform-specific settings.
- `quiverConfig.cmake.in` — template for `find_package(quiver)` support.

**`build/`** (gitignored):
- `bin/` — compiled `.exe`/`.dll` output (CMake `RUNTIME_OUTPUT_DIRECTORY`).
- `lib/` — compiled `.lib`/`.a`/`.so` output.
- `_deps/` — FetchContent download cache.

## Key File Locations

**Entry Points:**
- `src/cli/main.cpp` — `quiver_cli` executable.
- `tests/CMakeLists.txt` — `quiver_tests`, `quiver_c_tests`, `quiver_benchmark`, `quiver_sandbox`.
- `bindings/julia/src/Quiver.jl` — Julia module root.
- `bindings/dart/lib/quiverdb.dart` — Dart library root.
- `bindings/python/src/quiverdb/__init__.py` — Python package root.
- `bindings/js/mod.ts` — JS/Deno module root.

**Configuration:**
- `CMakeLists.txt` — top-level CMake (C++20, `QUIVER_BUILD_SHARED`, `QUIVER_BUILD_TESTS`, `QUIVER_BUILD_C_API`, SKBUILD integration for Python wheels).
- `src/CMakeLists.txt` — defines `quiver` and `quiver_c` targets; `quiver_cli` executable.
- `bindings/dart/hook/build.dart` — Dart native-assets CMake invocation.
- `bindings/python/pyproject.toml` — scikit-build-core wheel configuration.
- `.github/workflows/` — CI (`ci.yml`), wheel publishing (`build-wheels.yml`, `publish.yml`).

**Core Logic:**
- `include/quiver/database.h` + `src/database.cpp` + `src/database_impl.h` — main Database class.
- `src/database_impl.h` — Pimpl struct, `TransactionGuard`, shared helpers.
- `src/database_create.cpp`, `database_read.cpp`, `database_update.cpp`, `database_delete.cpp`, `database_metadata.cpp`, `database_query.cpp`, `database_time_series.cpp`, `database_csv_export.cpp`, `database_csv_import.cpp`, `database_describe.cpp` — one concern per file.
- `src/binary/binary_file.cpp`, `binary_metadata.cpp`, `csv_converter.cpp` — binary subsystem.
- `src/c/*.cpp` — C API implementation partitioned by concern.
- `src/c/internal.h`, `src/c/database_helpers.h` — shared C API internals.

**Testing:**
- `tests/CMakeLists.txt` — test executable definitions.
- `tests/test_utils.h` — shared test helpers.
- `tests/schemas/` — SQL fixtures shared across all layers.

## Naming Conventions

**C++ public API methods** (see `CLAUDE.md`):
- Pattern: `verb_[category_]type[_by_id]`.
- Verbs: `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`.
- `_by_id` suffix only when both "all elements" and "single element" variants exist.
- Singular vs plural matches cardinality: `read_scalar_integers` (vector), `read_scalar_integer_by_id` (optional).

**C API functions:** prefix `quiver_database_`, `quiver_binary_file_`, `quiver_binary_metadata_`, `quiver_element_`, `quiver_lua_runner_`, `quiver_csv_converter_`. Free functions: `quiver_*_free_*`.

**C++ source files:**
- Organized by feature/verb: `database_create.cpp`, `database_read.cpp`, etc.
- Implementation details in companion `_impl.h` header (`database_impl.h`).
- Internal-only C API helpers: `src/c/internal.h`, `src/c/database_helpers.h`, `src/c/database_options.h`.

**Test files:**
- Mirror source file names with `test_` prefix: `test_database_create.cpp` ↔ `src/database_create.cpp`.
- C API tests: `test_c_api_database_*.cpp` ↔ `src/c/database_*.cpp`.
- Binary C API tests: `test_c_api_binary_*.cpp` ↔ `src/c/binary/*.cpp`.
- Binding tests mirror the same pattern in each language (`test_database_create.jl`, `database_create_test.dart`, `test_database_create.py`, `create.test.ts`).

**Directories:**
- Lowercase, single-word where possible.
- Binary subsystem is a `binary/` subdirectory under `include/quiver/`, `include/quiver/c/`, `src/`, `src/c/` — not a top-level folder.

**SQL fixtures:** `snake_case.sql` inside `tests/schemas/{valid,invalid,migrations,issues}/`.

**Binding-specific casing** (see "Cross-Layer Naming Conventions" in `CLAUDE.md`):
- Julia: `snake_case` with `!` suffix for mutating ops.
- Dart: `camelCase`, factory methods as named constructors (`Database.fromSchema`).
- Python: `snake_case`.
- JS: `camelCase`.
- Lua: `snake_case` (1:1 with C++).

## Where to Add New Code

**New public Database method:**
1. Declare in `include/quiver/database.h` (follow `verb_[category_]type[_by_id]` pattern).
2. Implement in the matching per-concern file (`src/database_<verb>.cpp`), using `impl_->require_collection(...)` and `Impl::TransactionGuard` where appropriate.
3. Add C API wrapper in the matching `src/c/database_<verb>.cpp` with `QUIVER_REQUIRE(...)` and try/catch boundary.
4. Declare in `include/quiver/c/database.h`.
5. Co-locate any `new[]`/`free` pairs in the same `src/c/database_<verb>.cpp`.
6. Add GoogleTest cases in `tests/test_database_<verb>.cpp` (C++) and `tests/test_c_api_database_<verb>.cpp` (C API).
7. Add binding wrappers:
   - Julia: `bindings/julia/src/database_<verb>.jl` + test in `bindings/julia/test/test_database_<verb>.jl`.
   - Dart: add to `bindings/dart/lib/src/database_<verb>.dart` (a `part` of `database.dart`) + test in `bindings/dart/test/database_<verb>_test.dart`.
   - Python: add to `bindings/python/src/quiverdb/database.py` (or a mixin) + test in `bindings/python/tests/test_database_<verb>.py`.
   - JS: add to `bindings/js/src/<verb>.ts` (symbol declarations in `bindings/js/src/loader.ts`) + test in `bindings/js/test/<verb>.test.ts`.
   - Lua: add binding inside `src/lua_runner.cpp` in `Impl::bind_database()`.
8. Regenerate FFI bindings if struct layouts changed:
   - `bindings/julia/generator/generator.bat`
   - `bindings/dart/generator/generator.bat`
   - `bindings/python/generator/generator.bat`

**New binary subsystem method:**
- Same pattern, but located under `include/quiver/binary/`, `src/binary/`, `include/quiver/c/binary/`, `src/c/binary/`, and bindings' `binary/` subfolders (where they exist — Julia only so far).

**New value type (no private deps):**
- Add header under `include/quiver/`, implementation under `src/`. Use Rule of Zero — no Pimpl.

**New external dependency:**
- Add to `cmake/Dependencies.cmake` (FetchContent_Declare + FetchContent_MakeAvailable).
- Link PRIVATE in `src/CMakeLists.txt` (so its headers don't leak into the public API).

**New SQL schema fixture for tests:**
- Add to `tests/schemas/valid/<name>.sql` (or `invalid/` if it should fail validation).
- Reference by path in C++ tests; bindings use the same path relative to project root.

**New script automation:**
- Windows: `scripts/<name>.bat`.
- Unix (binding-scoped): `bindings/<lang>/test/test.sh` or similar.

**New example:**
- `example/<example-name>.{lua,bat}`.

## Special Directories

**`build/`:** CMake output — generated, gitignored.
**`bindings/dart/.dart_tool/`:** Native Assets build artifacts — generated, gitignored.
**`bindings/python/.venv/`:** uv-managed virtual env — generated, gitignored.
**`bindings/js/node_modules/`:** Node packages used by tooling (biome, vitest) — generated.
**`.cache/`:** Assorted cache (CMake/IDE) — gitignored.
**`.planning/`:** Planning artifacts (including this file). Present in the working tree but not part of the published package.

**`tests/schemas/`:** Canonical SQL fixtures — committed, shared across all language bindings. Any new schema goes here, never inside a binding directory.

---

*Structure analysis: 2026-04-18*
