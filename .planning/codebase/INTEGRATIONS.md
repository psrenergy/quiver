# External Integrations

**Analysis Date:** 2026-02-09

## APIs & External Services

**None Detected**

This codebase does not integrate with external REST APIs, cloud services, or third-party web services. It is a self-contained embedded database library.

## Data Storage

**Databases:**
- SQLite 3.50.2 (embedded)
  - Connection: File-based or in-memory (`:memory:`)
  - Client: Native sqlite3 C API via `SQLite::SQLite3` CMake target
  - Isolation: Each Database instance manages one sqlite3 connection
  - Schema: SQL files in `tests/schemas/` referenced via `Database::from_schema()` and `Database::from_migrations()`

**File Storage:**
- Local filesystem only
  - Database files: `.db` extension (configurable path)
  - Migration files: `.sql` extension in specified migrations directory
  - Time series blob files: Optional reference storage via `Items_time_series_files` table pattern
- No cloud storage integration (S3, Azure Blob, GCS, etc.)

**Caching:**
- None - SQLite manages its own page cache internally

## Authentication & Identity

**Auth Provider:**
- None - Not applicable for embedded library

**Read-Only Mode:**
- Supported via `DatabaseOptions.read_only` flag
- Set to 0 (writable) or 1 (read-only) at Database construction
- Enforced by SQLite connection pragma

## Monitoring & Observability

**Error Tracking:**
- None - Exceptions thrown locally with descriptive messages
- No external error reporting service

**Logs:**
- spdlog to console only (no files or remote services)
- Log level configurable via `DatabaseOptions.console_level` enum:
  - `QUIVER_LOG_DEBUG` (0)
  - `QUIVER_LOG_INFO` (1)
  - `QUIVER_LOG_WARN` (2)
  - `QUIVER_LOG_ERROR` (3)
  - `QUIVER_LOG_OFF` (4)
- All spdlog calls in `src/**/*.cpp` write to stdout/stderr only

## CI/CD & Deployment

**Hosting:**
- Not applicable - Library, not a service
- Distributed as compiled binaries (.dll, .so, .dylib) or source

**CI Pipeline:**
- GitHub Actions: `.github/workflows/ci.yml` (exists but not analyzed in detail)
- Pre-commit hooks: `.pre-commit-config.yaml` (exists, manages git hooks)
- Build script: `scripts/build-all.bat` orchestrates CMake + test execution

**Build Artifacts:**
- Shared library: `quiver.dll`/`libquiver.so`/`libquiver.dylib`
- C API library: `quiver_c.dll`/`libquiver_c.so`/`libquiver_c.dylib`
- Test executables: `quiver_tests.exe`, `quiver_c_tests.exe`

## Environment Configuration

**Required env vars:**
- None for runtime

**Build Secrets/Credentials:**
- None - Open-source project with no authentication

**Secrets location:**
- Not applicable

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Language Bindings Architecture

**Julia Bindings:**
- Location: `bindings/julia/`
- FFI approach: Direct ccall to C API functions in `libquiver_c.dll`
- Key files:
  - `bindings/julia/src/Quiver.jl` - Main module entry
  - `bindings/julia/generator/generator.bat` - Regenerates bindings from C headers
- Dependencies: CEnum (Julia enum bindings), Libdl (dynamic linking)

**Dart Bindings:**
- Location: `bindings/dart/`
- FFI approach: ffigen-generated bindings from C headers, native build hooks for CMake
- Key files:
  - `bindings/dart/pubspec.yaml` - ffigen config (entry points: `include/quiver/c/*.h`)
  - `bindings/dart/hook/build.dart` - CMake native build hook for compilation
  - `bindings/dart/lib/src/ffi/bindings.dart` - Generated FFI bindings (auto-generated)
  - `bindings/dart/lib/src/ffi/library_loader.dart` - Native library loading
- Build targets: Compiles `quiver` and `quiver_c` targets with CMake
- Generator: `ffigen` (v11.0.0) via `pubspec.yaml` ffigen config section
- Supported platforms: Windows (MSVC), macOS (Xcode), Linux (Ninja)

**Lua Integration:**
- Internal scripting engine - not a binding but embedded
- Location: `src/lua_runner.cpp`, `include/quiver/lua_runner.h`
- API: `LuaRunner` class takes Database reference, executes Lua scripts with `db` global object
- Lua can call C++ methods: `db:create_element()`, `db:read_scalar_integers()`, etc.
- Example:
  ```lua
  db:create_element("Collection", { label = "Item 1", value = 42 })
  local values = db:read_scalar_integers("Collection", "value")
  ```
- Implementation: sol2 (C++ Lua bindings) handles C++/Lua marshalling

## Database Schema Integration

**Schema Definition:**
- SQL files define table structure, constraints, foreign keys
- Canonical patterns in `tests/schemas/valid/`:
  - Configuration table (required, singleton)
  - Collections with scalar/vector/set attributes
  - Foreign key relationships with CASCADE rules
  - Time series tables with dimension (date_*) columns
  - Example files: `collections.sql`, `time_series.sql`

**Migration System:**
- `Database::from_migrations(db_path, migrations_path)` loads `.sql` files sequentially
- Tracks schema version via `current_version()` method
- Migration files numbered or ordered by filesystem

## Type System

**Data Types:**
- Scalar: INTEGER (int64_t), REAL (double), TEXT (string)
- Vector: Arrays of above types with index ordering
- Set: Unordered collections of above types with uniqueness constraint
- Time Series: Vectors with dimension column (e.g., `date_time`)
- NULL support via optional<T> in C++ API

**C API Type Enum:**
```c
QUIVER_DATA_TYPE_INTEGER = 0
QUIVER_DATA_TYPE_FLOAT = 1
QUIVER_DATA_TYPE_STRING = 2
QUIVER_DATA_TYPE_DATE_TIME = 3
QUIVER_DATA_TYPE_NULL = 4
```

---

*Integration audit: 2026-02-09*
