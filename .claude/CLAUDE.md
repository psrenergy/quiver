<!-- GSD:project-start source:PROJECT.md -->

## Project

**Quiver — UI Metadata Layer**

Quiver is a SQLite wrapper library with a C++ core, a C API for FFI, and five language
bindings (Julia, Dart, Python, JS/Bun, plus embedded Lua). This milestone teaches it to read
the PSR `database/ui/` TOML sidecar that every PSR Julia model already ships, and to surface
that metadata — enum vocabularies, labels, tooltips, units, formats, display order — through
its existing schema-description and metadata surfaces, in every layer.

It is for the LLM agents and applications that read a PSR study database through Quiver.
Today an agent calling `describe_data` sees an INTEGER column and cannot tell that it is an
enumeration, let alone what `0` means.

**Core Value:** An agent (or any consumer) calling Quiver's `describe` on a PSR model database sees what an
INTEGER column actually **means** — `values {0: 8 (Disabled), 1: 4 (Enabled)}`, not bare codes.

Everything else in this milestone is elaboration. If only that ships, the milestone succeeded.

### Constraints

- **Tech stack**: C++20 core, C API for FFI, five bindings. Logic lives in C++; bindings stay thin
  (root `CLAUDE.md` principle). A new metadata reader is a C++ feature bound outward, never five parsers.

- **Dependencies**: `tomlplusplus` v3.4.0 is already vendored and linked `PRIVATE` to the `quiver`
  target — **zero CMake change** required. Copy the `from_toml_content` / `from_toml_file` split
  from `src/binary/binary_metadata.cpp`.

- **Compatibility**: adding a field to `DatabaseOptions` is a **C ABI break** (the options struct
  grows 8 → 24 bytes). The highest-risk edit in the milestone is `bindings/js/src/ffi-helpers.ts`
  `makeDefaultOptions`, which hardcodes `new Uint8Array(8)` with `setInt32(0)` / `setInt32(4)`.
  Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139), so JS has no
  generated fallback — get this wrong and C writes 16 bytes past a `Uint8Array`. Needs its own
  plan and a runtime assert.

- **Compatibility**: do **not** add fields to `ScalarMetadata` / `GroupMetadata`.
  `bindings/js/src/metadata.ts` hardcodes `SCALAR_METADATA_SIZE = 56` and
  `GROUP_METADATA_SIZE = 32`, and those constants are the **out-buffer allocations** passed to C.
  Growing either struct turns a stale JS constant into a native out-of-bounds write. Use a new
  struct with its own size constant.

- **Compatibility**: Dart's `bindings.dart` must be **hand-edited**, not regenerated — a full
  ffigen regen flips enums and breaks Hub. Clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/`
  or tests silently run the old layout.

- **Compatibility**: Python's CFFI cdef is ABI-mode — a stale cdef corrupts silently with no
  compile error.

- **Behaviour**: with no UI config present, `describe` / `describe_collection` /
  `summarize_collection` output must be **byte-identical** to today. This keeps every existing
  assertion in `tests/test_database_describe.cpp` (including the brittle negative assertions) and
  `tests/test_lua_runner_describe.cpp` passing unchanged. New behaviour is proven by new fixtures.

- **Release**: any native change requires the full ritual — version bump across all five manifests,
  `publish-s3` → tag → Julia/Python/JS in parallel, Dart published by hand. **`CHANGELOG.md`
  currently heads `## [0.10.4] — unreleased` against `CMakeLists.txt` 0.10.6 — reconcile before
  the Bump Version workflow runs.**

- **Testing**: per `tests/CLAUDE.md`, a cross-layer feature adds cases to the C++ suite, the C API
  suite, and all five binding suites, with shared schemas under `tests/schemas/` (never copied into
  a binding). Note the four binding describe suites currently assert only "returns a String" — they
  prove nothing today and must be strengthened deliberately or left alone honestly.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->

## Technology Stack

## Languages

- **C++** 20 - Core library, binary subsystem, expression engine (`src/`, `include/quiver/`)
- **C** - FFI API layer for cross-language bindings (`src/c/`, `include/quiver/c/`)
- **Julia** 1.11+ - Native binding via Clang.jl FFI generator (`bindings/julia/`)
- **Dart** 3.10+ - Binding via ffigen code generator (`bindings/dart/`)
- **Python** 3.13+ - Binding via CFFI (ABI mode, no compile-time dependency) (`bindings/python/`)
- **JavaScript** (Bun) - Binding via Bun FFI (`bindings/js/`)
- **Lua** 5.4.8 - Embedded scripting engine (`src/lua_runner.cpp`, bound via sol2)

## Runtime

- **CMake** 3.26+ - Build system (`CMakeLists.txt`, `CMakePresets.json`)
- **C++ compiler** - MSVC, GCC, or Clang (minimum C++20 support)
- **Bun** - JavaScript runtime (test/build only; `bun:ffi` for FFI calls)
- **Python** - `pip`/`uv` (packages on PyPI as `quiverdb`)
- **JavaScript/npm** - npm 11.5.1+ (packages on npmjs.com as `quiverdb`)
- **Dart** - `pub` (packages on pub.dev as `quiverdb`)
- **Julia** - Pkg (canonical package in `bindings/julia/`, published mirror at psrenergy/Quiver.jl)
- **C/C++** - CMake/FetchContent (all dependencies vendored)
- `Manifest.toml` (Julia, generated by Pkg.instantiate)
- `package-lock.json` - None (Bun, JS has no lockfile; using lock flag)
- Python wheels - No lockfile (dependencies pinned via `pyproject.toml` + `cibuildwheel`)

## Frameworks

- **SQLite** 3.50.2 (via FetchContent/sjinks/sqlite3-cmake) - Embedded relational database
- **Lua** 5.4.8 (via FetchContent/lua-cmake) - Embedded scripting runtime (`LuaRunner`)
- **sol2** 3.5.0 (via FetchContent) - C++/Lua bindings for Lua integration
- **GoogleTest** 1.17.0 - C++ test framework (`tests/quiver_tests`, `tests/quiver_c_tests`)
- **bun:test** - JavaScript test framework (built-in to Bun)
- **pytest** 8.4.1+ - Python testing (`bindings/python/tests/`)
- **Test.jl** - Julia standard test framework (`bindings/julia/test/`)
- **test** 1.31.2+ - Dart testing framework (`bindings/dart/test/`)
- **clang-format** (v21) - C++ code formatting (`scripts/format.bat`)
- **cppcheck** - C++ static analysis (pre-commit hook)
- **cmake-format** - CMake code formatting (pre-commit hook)
- **clang-tidy** - C++ linting and refactoring (`scripts/tidy.bat`)
- **biome** 2.4.6+ - JavaScript linting and formatting (Dart/JS only)
- **ruff** 0.12.2+ - Python linting and formatting
- **JuliaFormatter** - Julia code formatting
- **ffigen** 20.1.1 - Dart FFI code generator (`bindings/dart/generator/`)
- **Clang.jl** - Julia FFI generator (`bindings/julia/generator/`)

## Key Dependencies

- **sqlite3** 3.50.2 - Relational database engine (vendored as submodule)
- **toml++** 3.4.0 - TOML file parsing (header-only, used for binary metadata)
- **spdlog** 1.17.0 - Logging library (`#include <spdlog/spdlog.h>`)
- **lua** 5.4.8 - Lua VM (embedded interpreter)
- **sol2** 3.5.0 - Lua/C++ bindings (header-only)
- **rapidcsv** 8.92 - CSV reading/writing (header-only, used in CSV export/import)
- **argparse** 3.2 - CLI argument parsing (header-only, used by quiver_cli)
- **GoogleTest** 1.17.0 - Testing framework (test-only dependency, development only)
- Python: **cffi** >=2.0.0 - FFI declarations in `src/quiverdb/_c_api.py`
- Dart: **ffigen** 20.1.1 - FFI code generation (`pubspec.yaml` devDependency)
- Julia: **CEnum** 0.5 - Enum marshaling for C API (`Project.toml`)
- JS/Bun: No external dependencies (native `Bun.FFI` used directly)

## Configuration

- `CMakeLists.txt` - Top-level build configuration (version 0.10.6, C++20)
- `CMakePresets.json` - Build presets (dev, release, windows-release, linux-release)
- `cmake/CompilerOptions.cmake` - Compiler flags (MSVC: `/W4 /permissive- /Zc:__cplusplus`; others: `-Wall -Wextra -Wpedantic`)
- `cmake/Dependencies.cmake` - FetchContent declarations for all vendored dependencies
- `cmake/Platform.cmake` - Platform-specific flags (macOS deployment target 13.3, RPATH settings, visibility hidden)
- `.clang-format` - C++ formatting rules (LLVM-based, 120 col limit, spaces-4, C++20)
- `.clang-tidy` - C++ linting rules
- `.pre-commit-config.yaml` - Git hooks for trailing whitespace, YAML/JSON validation, clang-format, cppcheck, cmake-format
- `bindings/python/ruff.toml` - Python: isort checks only, 120 col, Python 3.13
- `bindings/js/biome.json` - JavaScript/TypeScript formatting and linting
- `bindings/dart/analysis_options.yaml` - Dart linting (generated via `dart pub get`)
- `bindings/julia/.JuliaFormatter.toml` - Julia formatting rules
- `QUIVER_LIB_DIR` - Julia runtime lib discovery (optional; falls back to DEPOT_PATH → artifact_path → in-tree `build/`)
- `QUIVER_DEBUG` - Optional debug logging (environment-dependent usage)
- `CODECOV_TOKEN` - CI coverage upload token (GitHub Actions secret)

## Platform Requirements

- **Windows:** MSVC 2022 or MinGW (static libgcc/libstdc++ for portability)
- **Linux:** GCC 11+ (CentOS 7 / glibc 2.17 for published natives via manylinux2014)
- **macOS:** Clang 13+ with libc++ (deployment target 13.3 for `std::to_chars` availability)
- **Python local runs:** Via `uv` only (no plain `python` on PATH; `uv run python -c "..."`)
- **Python wheels:** cp313 on manylinux_x86_64 and win_amd64 (via cibuildwheel)
- **npm package:** Bundled native libs for linux-x86_64, macos-aarch64, windows-x86_64
- **Julia package:** Plain S3 artifacts tarball (no _jll suffix; glibc 2.17 / GLIBCXX 3.4.29 floor)
- **Dart package:** Via native-assets hook (`hook/build.dart`), which invokes cmake with QUIVER_UNVERSIONED_SHARED=ON
- **JavaScript/Bun:** Native libs downloaded at install-time from S3 via Bun postinstall

## Build Outputs

- **Libraries:** `build/lib/` → libquiver.so/dylib/dll (shared), libquiver_c.so/dylib/dll (C API)
- **Executables:** `build/bin/` → quiver_cli, quiver_tests, quiver_c_tests, quiver_benchmark
- **Wheels:** dist/*.whl (Python, via scikit-build-core)
- **npm package:** `package/libs/{linux-x86_64,macos-aarch64,windows-x86_64}/` bundled natives

## Test Infrastructure

- C++: CTest (via `cmake --build build --target test` or `ctest -C Debug`)
- C API: Same CTest (separate test executable)
- Python: pytest via `scripts/test-all.bat` or `bindings/python/tests/test.bat`
- Julia: via `bindings/julia/test/test.bat`
- Dart: via `bindings/dart/test/test.bat`
- JavaScript: via `bindings/js/test/test.bat` (runs `bun test test`)
- C++/C: lcov (captures via build with `-DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE=ON`)
- Python/Julia/Dart/JS: Native coverage tools (pytest-cov, Coverage.jl, coverage/lcov, c8)
- Upload: Codecov (GitHub Actions, flags: `cpp`, `julia`, `dart`, `python`)

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

## Error Handling

### C API Error Channel

## Cross-Layer Naming Transformations

| Layer | Rule | Example |
|-------|------|---------|
| C++ | snake_case | `create_element` |
| C API | Prefix `quiver_database_` | `quiver_database_create_element` |
| Julia | Same name, `!` suffix for mutators | `create_element!` |
| Dart | snake_case → camelCase, named constructors | `Database.fromSchema()`, `createElement()` |
| Python | Same snake_case, `@staticmethod` for factories, `**kwargs` for create/update | `Database.from_schema()`, `create_element()` |
| JS | snake_case → camelCase (includes `exportCsv` not `exportCsv`) | `Database.fromSchema()`, `createElement()` |
| Lua | Exactly 1:1 with C++ (colon method syntax) | `db:create_element()` |

## Method Naming Convention

- `read_scalar_integers` (returns vector of values)
- `read_scalar_integer_by_id` (returns optional single value)
- `create_element(collection, element)` → `create_element_by_label`
- `update_element(collection, id, element)` → `update_element_by_label(collection, label, element)`
- `read_scalar_integers(collection, attribute)` (all elements)
- `read_scalar_integer_by_id(collection, attribute, id)` (single element)
- `list_vector_groups(collection)` (plural — returns vector of group names)
- `get_scalar_metadata(collection, attribute)` (singular metadata type)

## C++ Code Style

### Pimpl vs Value Types

- `Database`, `LuaRunner`, `BinaryFile`
- `Element`, `Row`, `Result`, `Migration`, `Migrations`, `GroupMetadata`, `ScalarMetadata`, `CSVOptions`, `Dimension`, `TimeProperties`, `Expression`, `BinaryMetadata`

### RAII and Ownership

- Delete copy, default move for resource types:
- Ownership of pointers/resources is explicit and unambiguous.
- `TransactionGuard` is nest-aware RAII: a guard becomes a no-op if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`).

### Philosophy

- **Human-Centric:** Codebase optimized for readability, not machine parsing.
- **Clean code over defensive code:** Assume callers obey contracts; avoid excessive null checks.
- **Simple solutions over complex abstractions:** Delete unused code, do not deprecate.
- **Thin bindings:** Logic resides in C++ layer; bindings are thin marshaling layers.

### Logging

## Formatting and Linting

### Tools

- **C++:** `scripts/format.bat` runs clang-format, linting via `scripts/tidy.bat` (run-clang-tidy)
- **Dart:** `dart format` (invoked by scripts/format.bat)
- **Python:** `ruff` format and lint (invoked by scripts/format.bat)
- **JS/Bun:** `biome` format (invoked by scripts/format.bat)
- **Julia:** `JuliaFormatter` (invoked by scripts/format.bat)

### C++ Style Configuration

- Based on LLVM style, modified for project conventions
- Line ending: LF (UseCRLF=false)
- Indent: 4 spaces, no tabs
- Column limit: 120
- C++20 standard
- NamespaceIndentation: None (no indent in namespace blocks)
- BreakConstructorInitializers: BeforeColon
- AllowShortFunctionsOnASingleLine: Inline (inline functions only)
- Static analysis configuration; main build runs `run-clang-tidy` over `build/compile_commands.json`
- Skips `src/binary` (binary subsystem has load-bearing optimizations documented in `src/CLAUDE.md`)

### Pre-Commit Hooks

- Trailing whitespace removal
- End-of-file fixer
- YAML/JSON validation
- Mixed line ending fix (enforce LF)
- File size limit (>1 MB files flagged)
- clang-format (C++ files under `include/`, `src/`, `tests/`)
- cppcheck (C++ static analysis)
- cmake-format (CMake files)

### Line Endings

## Binding-Specific Conventions

### Dart (lib/src/database.dart)

- **Marshaling idiom:** Every method allocates through `package:ffi` `Arena` and releases in `finally`.
- **Typed column dispatch:** Dispatch on first non-null cell, then convert **every** cell individually. Never cast the rest as a type (defers the check to iteration).
- **Integer-for-REAL coercion:** An `int` (and a `bool`) is accepted into a REAL column.
- **Per-method boilerplate is the house style** — don't collapse into parameterized helpers.

### Python (src/quiverdb/)

- **CFFI ABI-mode:** No compiler required at install; `_c_api.py` declarations must match C headers exactly.
- **API shape:** `create_element`/`update_element` accept `**kwargs` (no `Element` class exposed). `LogLevel` is an `IntEnum`.
- **Positional-only parameters:** Methods like `update_element_by_label(collection, label, /, **kwargs)` use `/` to prevent a kwarg `label=` collision.
- **Per-method boilerplate is the house style** — don't collapse into parameterized helpers.
- **Null strings:** Pass `ffi.NULL` for a nullable string argument, never `b""` (clear vs lookup semantics).

### Julia (src/database_*.jl)

- **`GC.@preserve`:** All refs from `marshal_params` and any `Ref`s passed as pointers must stay inside a `GC.@preserve refs ...` block spanning the ccall.
- **Nullability-aware return types:** `read_scalar_{integers,floats,strings}` return concrete `Vector{T}` for NOT NULL columns and `Vector{Optional{T}}` for nullable ones (schema-driven, not data-driven).
- **`INTEGER PRIMARY KEY` (e.g., `id`):** Reported `not_null` by the C++ core, so `read_scalar_integers(db, c, "id")` returns a concrete `Vector{Int64}`.
- **One marshaller per group writer:** `_update_group_columns(db, update, ...)` shared by all four group writers and their `_by_label` forms.
- **Callback-first overloads for `do` syntax:** `from_schema(path) do db ... end` and similar — returns callback result, calls `close!` from `finally`.
- **Null strings:** Pass `Ptr{Cchar}(C_NULL)` for a nullable string argument, never `""` (clear vs lookup semantics).

### JS (bindings/js/src/index.ts)

- **String-based datetime surface:** No DateTime wrapper classes — all datetimes are ISO 8601 strings.
- **Query API shape:** `queryString`, `queryInteger`, `queryBoolean`, `queryFloat`, `queryDateTime` take an optional positional `List<Object?>? parameters` (no separate `*Params` methods).
- **Bun FFI loader:** Hand-written symbol table in `bindings/js/src/loader.ts` (no ffigen). Workarounds documented in `bindings/js/CLAUDE.md`.

### Lua (via src/lua_runner.cpp)

- **Filesystem sandbox:** File-touching operations (`db:open_file`, `db:export_csv`, etc.) are sandboxed to the database file's directory. Paths outside are rejected; `:memory:` databases reject all file operations.
- **Standard libraries enabled:** `base`, `string`, `table`, `math`, `coroutine`, `utf8` (pure computation only). `os`, `io`, `package`, `debug` are unloaded.
- **Disabled functions:** `dofile` and `loadfile` are nil'd (no loading Lua source from disk). String-form `load` stays available.
- **Lua boolean:** `true`/`false` are INTEGER 1/0 on every write path. No boolean **readers** (root design decision).
- **Return value encoding:** Script return is JSON-encoded via `to_lua_table` overloads (no binding gets a JSON dependency).
- **JSON null handling:** Non-finite numbers → `null`; table with `nil` holes → object (not array).

## Schema Conventions

### Configuration Table (Required)

### Collections

### Vector Tables

### Set Tables

### Time Series Tables

### Foreign Keys

## Scalar Typing Policy

- int64 accepted for INTEGER **and** REAL columns (int-for-REAL coercion)
- double accepted for REAL **only** (float into INTEGER rejected)
- string accepted for TEXT / INTEGER (FK label) / DATE_TIME columns
- **Keep these two implementations in sync** — they are the only two type gates.

## DATE_TIME Content Validation

- All fields fixed-width and zero-padded
- Year: 0001–9999
- Calendar day must exist (no Feb 31)
- No leap seconds

<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

## System Overview

```text

```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| **Database (C++)** | Main CRUD API, transactions, schema lazy-loading, validation | `include/quiver/database.h`, `src/database*.cpp` |
| **Database::Impl** | Private: pimpl holder, schema/type validators, label→id resolution | `src/database_impl.h` |
| **C API** | FFI marshaling, memory management, error propagation | `include/quiver/c/database.h`, `src/c/database*.cpp` |
| **LuaRunner** | Lua script execution with db access, sandboxed file I/O | `src/lua_runner.cpp` |
| **BinaryFile** | .qvr file I/O (read/write with metadata) | `include/quiver/binary/binary_file.h`, `src/binary/binary_file.cpp` |
| **Expression** | Lazy DAG expressions over .qvr files | `include/quiver/expression/expression.h`, `src/expression/*.cpp` |
| **Schema** | Introspection: tables, columns, groups, metadata | `src/schema.cpp` |
| **TypeValidator** | Value-vs-column type checking (scalars, arrays, DATE_TIME) | `src/type_validator.cpp` |
| **SchemaValidator** | Schema convention checks (required Configuration table, FK actions, etc.) | `src/schema_validator.cpp` |
| **FFI Bindings** | Language-specific wrappers (Julia, Dart, Python, JS) | `bindings/{julia,dart,python,js}/` |

## Pattern Overview

- **FFI-first design**: C API is the single public interface; all external code speaks C
- **Pimpl encapsulation**: Database, LuaRunner, BinaryFile hide private dependencies (sqlite3, lua headers)
- **Lazy schema loading**: Metadata not loaded at `Database(path)` but on first use (`require_schema`)
- **Nest-aware RAII**: TransactionGuard knows when a transaction is already active; dry runs reuse transactions
- **Single error channel**: `quiver_get_last_error()` is the only error reporting surface across all layers
- **Binding uniformity**: All bindings expose the same API surface (cross-layer naming conventions)

## Layers

- Purpose: All business logic, validation, SQL execution, file I/O
- Location: `include/quiver/database.h` (public API), `src/database*.cpp` (implementation)
- Contains: Database CRUD, metadata introspection, transactions, migration validation, CSV import/export
- Depends on: sqlite3, spdlog, lua (for LuaRunner), binary/expression libraries
- Used by: C API layer, which is used by all FFI bindings
- Purpose: FFI marshaling, memory management, error translation, opaque handles
- Location: `include/quiver/c/database.h` (public API), `src/c/database*.cpp` (implementation)
- Contains: `quiver_database_*` functions, element builder, metadata, time-series group readers/writers
- Depends on: C++ core (calls C++ methods, catches exceptions, marshals to C types)
- Used by: All FFI bindings (Julia Clang.jl, Dart ffigen, Python CFFI, JS Bun FFI)
- Purpose: Execute Lua scripts against a database (direct sol2 binding, NOT through C API)
- Location: `include/quiver/lua_runner.h`, `src/lua_runner.cpp`
- Contains: Lua→C++ converters, sandboxed file I/O, JSON encoder for script return values
- Depends on: C++ core directly, sol2 library
- Used by: C API (exposed as `quiver_lua_runner_*`), then by all FFI bindings
- Purpose: .qvr file I/O with metadata sidecars
- Location: `include/quiver/binary/binary_file.h`, `src/binary/binary_file.cpp`
- Contains: BinaryFile (Pimpl), CSVConverter, BinaryMetadata, dimension iteration
- Depends on: sqlite3 (for CSV parsing), no C API
- **Bound in: Julia and Lua only** (deliberate — see root design decisions)
- Purpose: Lazy arithmetic expressions over .qvr files
- Location: `include/quiver/expression/expression.h`, `src/expression/*.cpp`
- Contains: Expression DAG nodes (binary ops, unary math, aggregations, label projections), save/compute
- Depends on: BinaryFile, binary metadata, no C API
- **Bound in: Julia and Lua only** (deliberate — see root design decisions)

## Data Flow

### Primary Request Path: `create_element` (Dart example)

### Schema Metadata Lazy-Loading Path

### Transaction Nesting (TransactionGuard)

## Key Abstractions

- `Database::Impl` (contains `sqlite3*`, validators, logger) — hides sqlite3 headers from public API
- `BinaryFile::Impl` (contains file I/O state) — hides platform-specific file ops
- `LuaRunner` is NOT Pimpl (its `impl_` is a `lua_State*`, which is internal-use-only, but the pattern is inversion: C++ wrapper over C library)
- `Element` value type with fluent `.set(name, value)` API, converts to SQL INSERT/UPDATE
- Used in C++, wrapped in C API opaque `quiver_element_t`, language bindings provide their own builders (Dart `Element`, Python `**kwargs`, etc.)
- `Result<T>` (C++): Always has a value; exceptions surface errors (not used after C API)
- `Row` (C++): Query result row, typed accessors `get<T>(column)`
- Bindings: Language-native types (lists, dicts, objects)
- `ScalarMetadata`: column name, type, not_null flag, primary_key flag
- `GroupMetadata`: group name, dimension_column (NULL for vectors/sets, populated for time-series), column names + types
- C API: parallel `quiver_scalar_metadata_t`, `quiver_group_metadata_t` structs

## Entry Points

- `quiver/database.h` — `Database` class, factories, CRUD, transactions
- `quiver/lua_runner.h` — `LuaRunner`, script execution
- `quiver/binary/binary_file.h` — `BinaryFile` (Julia/Lua only)
- `quiver/expression/expression.h` — `Expression` (Julia/Lua only)
- `quiver/c/database.h` — All `quiver_database_*` and `quiver_element_*` functions
- `quiver/c/common.h` — `quiver_get_last_error`, version
- `quiver/c/lua_runner.h` — `quiver_lua_runner_*` functions
- `src/cli/main.cpp` — `quiver_cli` executable, reads `--schema`, `--migrations`, runs Lua script
- Julia: `bindings/julia/src/Quiver.jl` (module root), `src/database.jl` (main API)
- Dart: `bindings/dart/lib/quiverdb.dart` (public import), `lib/src/database.dart` (implementation)
- Python: `bindings/python/quiverdb/__init__.py`, `quiverdb/database.py`
- JS: `bindings/js/src/index.ts`, `bindings/js/src/database.ts`

## Architectural Constraints

- **Threading:** Single-threaded event loop within Lua; C++ core and C API are not thread-safe (sqlite3 built with `SQLITE_THREADSAFE=0`)
- **Global state:** 
- **Circular imports:** None by design (C API depends on C++, bindings depend on C API, binary/expression depend on core data types but NOT on C API)
- **Lazy loading:** Schema metadata loads on first use, must not be accessed during half-migrated DB state

## Anti-Patterns

### Accessing Struct Members Across Layers

### Replicating Type Validation in Bindings

### Reimplementing Group Inserts

## Error Handling

- **Pattern 1 (Precondition failure):** `"Cannot {operation}: {reason}"` — caller violated a contract
- **Pattern 2 (Not found):** `"{Entity} not found: {identifier}"` — data doesn't exist
- **Pattern 3 (Operation failure):** `"Failed to {operation}: {reason}"` — I/O or constraint error

## Cross-Cutting Concerns

- **Type validation** (scalars, arrays, DATE_TIME grammar): `TypeValidator` in C++, called on every write before group INSERT
- **Schema validation** (Configuration table exists, FKs use CASCADE, no duplicate attributes): `SchemaValidator` on schema load
- **Migration validation** (up then down round-trip, no tables left behind): `Database::validate_migrations()`

<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
