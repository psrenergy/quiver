# External Integrations

**Analysis Date:** 2026-02-20

## APIs & External Services

**None.**

Quiver is a self-contained embedded database library. It does not call any external HTTP APIs, cloud services, or third-party web services. All functionality is local.

## Data Storage

**Databases:**
- SQLite 3.50.2 (embedded, no server required)
  - Connection: File path passed to `Database` constructor or factory methods (`from_schema()`, `from_migrations()`)
  - Client: Direct `sqlite3*` C API, accessed via `Database::Impl` Pimpl struct in `src/database.cpp`
  - Mode: Single-file embedded database (`.db` file)
  - Features used: STRICT tables, foreign keys with CASCADE, WAL mode (implicit), parameterized queries, transactions
  - Schema: Defined in `.sql` files under `tests/schemas/valid/` and applied via `from_schema()` or `from_migrations()`

**File Storage:**
- Local filesystem only
  - CSV import/export via `Database::export_csv()` and `Database::import_csv()` (arbitrary file paths)
  - Time series file references stored as string paths in `{Collection}_time_series_files` singleton tables (paths stored, not file content)
  - Schema files read from disk at database creation time

**Caching:**
- None (SQLite's page cache is the only caching layer, managed by SQLite internally)

## Authentication & Identity

**Auth Provider:**
- None. No authentication layer. Database access is file-system-level (whoever has filesystem access to the `.db` file can open it).

## Monitoring & Observability

**Error Tracking:**
- None (no external error tracking service)

**Logs:**
- spdlog v1.17.0 (`gabime/spdlog`)
  - Default log level: `QUIVER_LOG_INFO` (configurable via `DatabaseOptions` at construction time)
  - Log levels: `QUIVER_LOG_DEBUG`, `QUIVER_LOG_INFO`, `QUIVER_LOG_WARN`, `QUIVER_LOG_ERROR`
  - Output: stdout/stderr via spdlog default sink
  - Log file: `quiver_database.log` observed at project root (spdlog file sink)
  - C API log level set via `quiver_database_options_t.log_level` field in `include/quiver/c/options.h`

**Coverage:**
- Codecov integration configured via `codecov.yml`
  - Precision: 2 decimal places
  - Target: auto (relative to base)
  - Ignores: `tests/**/*`, `build/**/*`
  - Comment trigger: `require_changes: true`

## CI/CD & Deployment

**Hosting:**
- Library distributed as compiled shared library (`.dll`/`.so`/`.dylib`)
- No cloud deployment target; consumers embed the library

**CI Pipeline:**
- Codecov for coverage reporting (`codecov.yml`)
- pre-commit hooks for code quality (`.pre-commit-config.yaml`):
  - `pre-commit/pre-commit-hooks` v6.0.0: trailing whitespace, EOF fixer, YAML/JSON checks, merge conflict detection, LF enforcement, large file guard (1024KB limit)
  - `pocc/pre-commit-hooks` v1.3.5: clang-format and cppcheck on `include/`, `src/`, `tests/`
  - `cheshirekow/cmake-format-precommit` v0.6.13: cmake-format on CMakeLists files
- No GitHub Actions or other CI pipeline configuration files detected

**Build Scripts (Windows batch):**
- `scripts/build-all.bat` - Full build + all tests (C++, C API, Julia, Dart)
- `scripts/test-all.bat` - Run all tests assuming already built
- `scripts/format.bat` - Run clang-format
- `scripts/tidy.bat` - Run clang-tidy
- `bindings/julia/generator/generator.bat` - Regenerate Julia FFI bindings from C headers
- `bindings/dart/generator/generator.bat` - Regenerate Dart FFI bindings from C headers

## FFI / Language Binding Integrations

**Julia FFI (Clang.jl code generation):**
- Tool: `Clang.jl` v0.19.0 (generator only, `bindings/julia/generator/Project.toml`)
- Generator script: `bindings/julia/generator/generator.jl`
- Output: `bindings/julia/src/c_api.jl` (auto-generated, do not edit manually)
- Loads: `libquiver_c.dll` via `Libdl.dlopen`
- Config: `bindings/julia/generator/generator.toml` (library name `libquiver_c`, module `C`)

**Dart FFI (ffigen code generation):**
- Tool: `ffigen` v11.0.0 (`bindings/dart/pubspec.yaml` dev dependency)
- Config: `bindings/dart/pubspec.yaml` (`ffigen:` section), `bindings/dart/ffigen.yaml`
- Headers parsed: `include/quiver/c/common.h`, `include/quiver/c/database.h`, `include/quiver/c/element.h`, `include/quiver/c/lua_runner.h`
- Output: `bindings/dart/lib/src/ffi/bindings.dart` (auto-generated, excluded from linter)
- Native build hook: `bindings/dart/hook/` uses `native_toolchain_cmake` to build `libquiver_c.dll` via CMake during `dart pub get`

## Scripting Integration

**Lua 5.4.8 (embedded scripting engine):**
- Embedded directly into the core library via FetchContent (`cmake/Dependencies.cmake`)
- C++ bindings via sol2 v3.5.0
- Exposed through `LuaRunner` class (`include/quiver/lua_runner.h`, `src/lua_runner.cpp`)
- C API: `quiver_lua_runner_*` functions in `include/quiver/c/lua_runner.h`
- Julia binding: `bindings/julia/src/lua_runner.jl`
- Dart binding: `bindings/dart/lib/src/lua_runner.dart`
- Lua scripts receive a `db` userdata object with full `Database` method access (same naming as C++)

## Webhooks & Callbacks

**Incoming:** None

**Outgoing:** None

## Environment Configuration

**Required env vars:** None. The library operates entirely without environment variables.

**Optional configuration:**
- All options passed via `DatabaseOptions` struct at construction time
- Log level controlled via `quiver_database_options_t.log_level` (C API) or `DatabaseOptions.log_level` (C++)

**Secrets location:** Not applicable (no secrets or credentials used)

---

*Integration audit: 2026-02-20*
