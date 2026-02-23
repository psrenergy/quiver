# External Integrations

**Analysis Date:** 2026-02-23

## APIs & External Services

**No external APIs configured.**
- Quiver is a self-contained database library with no outbound integrations
- All operations are local to the SQLite database
- No third-party service calls (payments, analytics, auth, etc.)

## Data Storage

**Databases:**
- SQLite3 (local file-based)
  - Connection: File path passed to `Database()` constructor
  - Client: `sqlite3` C API (included in dependencies)
  - Implementation: `include/quiver/database.h` / `src/database.cpp`
  - Schema validation: `src/schema_validator.cpp`

**File Storage:**
- Local filesystem only
- CSV export functionality via `database_csv.cpp`
  - Export target: Local file path parameter
  - Formatting: Optional enum mapping and date/time formatting via `CSVExportOptions`
  - Implementation: `src/database_csv.cpp` / `src/c/database_csv.cpp`

**Caching:**
- None - all data operations direct to SQLite

## Authentication & Identity

**Auth Provider:**
- None - library operates without authentication
- Database file access controlled by OS filesystem permissions
- Log levels configurable per database via `DatabaseOptions.console_level`

## Monitoring & Observability

**Error Tracking:**
- None - errors surface as exceptions (C++) or error codes (C API)

**Logs:**
- spdlog-based file and console logging
  - Location: Logs written to `<db_path>.log` in same directory as database file
  - Levels: DEBUG, INFO, WARN, ERROR, OFF (configurable via `quiver_log_level_t`)
  - Implementation: `src/database.cpp` (logger creation and setup)
  - Thread-safe with unique logger instance per database
  - Sinks: Console (colored) + file (basic file sink)

## CI/CD & Deployment

**Hosting:**
- Standalone C++ library - no hosted services
- Bindings provide wrappers for Julia, Dart, and Lua applications
- Distribution: Binary libraries (`.dll`/`.so`/`.dylib` + static `.lib`/`.a`)

**CI Pipeline:**
- None detected - build/test scripts are local (Windows `.bat` files)
  - `scripts/build-all.bat` - Build + test (Debug/Release)
  - `scripts/test-all.bat` - Run all tests
  - Individual test executables:
    - `./build/bin/quiver_tests.exe` - C++ core tests
    - `./build/bin/quiver_c_tests.exe` - C API tests
    - `bindings/julia/test/test.bat` - Julia binding tests
    - `bindings/dart/test/test.bat` - Dart binding tests
  - No continuous integration service configured

## Environment Configuration

**Required env vars:**
- None - all configuration via constructor parameters and options structs

**Configuration Inputs:**
- `DatabaseOptions` struct:
  - `read_only` (int) - Open database read-only
  - `console_level` (quiver_log_level_t) - Logging level
- `CSVExportOptions` struct:
  - `date_time_format` - strftime format string for date columns (empty = no formatting)
  - `enum_labels` - Map attribute names to (integer_value -> string_label) for CSV enum resolution

**Secrets location:**
- N/A - no secrets required

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Language-Specific Integrations

**Julia Bindings (`bindings/julia/`):**
- Direct FFI calls to C API via `libdl` dynamic loading
- Dependencies: CEnum, DataFrames, Dates, Libdl (standard library)
- Compatibility: Julia 1.12+
- Provides convenience wrappers:
  - DateTime parsing methods (Lua and Dart equivalents)
  - Composite read helpers (all scalars, vectors, sets by ID)
  - Transaction blocks with automatic commit/rollback

**Dart Bindings (`bindings/dart/`):**
- FFI via `dart:ffi` package
- Auto-generated C bindings via `ffigen` from C API headers
- Dependencies: `ffi`, `code_assets`, `hooks`, `logging`, `native_toolchain_cmake`
- Compatibility: Dart SDK 3.10.0+
- DLL Dependency: Both `libquiver.dll` and `libquiver_c.dll` must be in PATH
- Build: `native_toolchain_cmake` compiles C API on first build
- Code generation: `ffigen` in `pubspec.yaml` points to C API headers:
  - `include/quiver/c/common.h`
  - `include/quiver/c/database.h`
  - `include/quiver/c/element.h`
  - `include/quiver/c/lua_runner.h`

**Lua Integration (in-process):**
- sol2 C++ binding library embedded in core (`src/lua_runner.cpp`)
- No separate process or service
- Database reference injected as `db` userdata in Lua context
- C API: `quiver_lua_runner` functions for Lua script execution

## Network & Remote Access

- Not applicable - library is local-only
- No network ports opened
- No RPC/REST API exposed

---

*Integration audit: 2026-02-23*
