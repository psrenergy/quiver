# Technology Stack

**Analysis Date:** 2026-02-20

## Languages

**Primary:**
- C++20 - Core library (`src/`, `include/quiver/`)
- C - C API wrapper for FFI (`src/c/`, `include/quiver/c/`)

**Secondary:**
- Julia 1.12+ - Language bindings (`bindings/julia/`)
- Dart 3.10+ - Language bindings (`bindings/dart/`)
- Python 3.13+ - Language bindings (early-stage, `bindings/python/`)
- SQL (SQLite dialect) - Schema definitions (`tests/schemas/`)

## Runtime

**Environment:**
- Native compiled binary (shared library `.dll`/`.so`/`.dylib`)
- No runtime interpreter required for core library
- Julia runtime required for Julia bindings
- Dart SDK required for Dart bindings

**Package Manager:**
- CMake 3.21+ with `FetchContent` for C++ dependencies
- Julia `Pkg` for Julia dependencies (`bindings/julia/Project.toml`)
- Dart `pub` for Dart dependencies (`bindings/dart/pubspec.yaml`)
- Python `hatchling` for Python bindings (`bindings/python/pyproject.toml`)
- Lockfiles: `bindings/julia/Manifest.toml` (present), `bindings/dart/pubspec.lock` (present)

## Frameworks

**Core:**
- None (native C++ library with no framework dependency)

**Testing (C++ layer):**
- GoogleTest v1.17.0 - Unit and integration tests (`tests/`)
- GMock v1.17.0 - Mocking framework (bundled with GoogleTest)

**Testing (bindings):**
- Julia `Test` stdlib + `Aqua` 0.8 - Julia binding tests (`bindings/julia/test/`)
- Dart `test` 1.29.0 - Dart binding tests (`bindings/dart/test/`)
- Python `pytest` 8.4.1+ - Python binding tests (`bindings/python/tests/`)

**Build/Dev:**
- CMake 3.21+ with `CMakePresets.json` - Build system
- clang-format (LLVM style, 120 col limit) - C++ code formatting (`.clang-format`)
- clang-tidy - C++ static analysis (`.clang-tidy`)
- cmake-format - CMake file formatting
- cppcheck - Static analysis via pre-commit
- pre-commit v6.0.0 hooks (`.pre-commit-config.yaml`)
- Ruff (Python linting/formatting in `bindings/python/ruff.toml`)

## Key Dependencies

**Critical (C++ core - all fetched via FetchContent from `cmake/Dependencies.cmake`):**
- SQLite3 v3.50.2 (via `sjinks/sqlite3-cmake`) - Primary database engine. Linked PUBLIC, so consumers get SQLite headers.
- spdlog v1.17.0 (via `gabime/spdlog`) - Structured logging with debug/info/warn/error levels
- Lua 5.4.8 (via `codelibre/lua/lua-cmake`) - Embedded scripting engine
- sol2 v3.5.0 (via `ThePhD/sol2`) - C++ bindings for Lua; requires `/bigobj` MSVC flag due to heavy template use
- toml++ v3.4.0 (via `marzer/tomlplusplus`) - TOML config file parsing

**Infrastructure (Julia bindings - `bindings/julia/Project.toml`):**
- `CEnum` 0.5 - C enum interop for FFI
- `DataFrames` 1.x - Tabular data returned from queries
- `Dates` stdlib - DateTime handling for date_time scalar/vector/set reads
- `Libdl` stdlib - Dynamic library loading (`libquiver_c.dll`)
- `Clang` 0.19.0 (generator only) - Used by `bindings/julia/generator/generator.jl` to generate `c_api.jl` from C headers

**Infrastructure (Dart bindings - `bindings/dart/pubspec.yaml`):**
- `ffi` 2.1.5 - FFI interop with `libquiver_c.dll`
- `ffigen` 11.0.0 (dev) - Auto-generates `lib/src/ffi/bindings.dart` from C headers
- `code_assets` 1.0.0 - Native code asset hooks
- `hooks` 1.0.0 - Dart native build hooks
- `native_toolchain_cmake` 0.2.2 - CMake toolchain for Dart native hooks
- `logging` 1.3.0 - Dart logging

**Infrastructure (Python bindings - `bindings/python/pyproject.toml`):**
- `hatchling` - Build backend (early-stage binding, largely empty)

## Configuration

**Environment:**
- No environment variables required for core library
- Database path passed directly to constructor or factory methods
- `DatabaseOptions` struct controls log level (`QUIVER_LOG_DEBUG`, `QUIVER_LOG_INFO`, etc.) and other options

**Build:**
- `CMakeLists.txt` - Root build config (version 0.3.0)
- `CMakePresets.json` - Preset configs: `dev` (Debug + tests + C API), `release`, `windows-release` (VS 2022), `linux-release`
- `cmake/Dependencies.cmake` - All FetchContent dependency declarations
- `cmake/CompilerOptions.cmake` - MSVC (`/W4 /permissive-`) and GCC/Clang (`-Wall -Wextra -Wpedantic`) flags
- `cmake/Platform.cmake` - Platform detection, RPATH settings, symbol visibility (`hidden` by default)
- `.clang-format` - LLVM-based, 4-space indent, 120-column limit, LF line endings
- `.clang-tidy` - bugprone-*, modernize-*, performance-*, readability-identifier-naming checks
- `bindings/dart/analysis_options.yaml` - Dart linting (lints/recommended, 120-col page width)
- `bindings/python/ruff.toml` - Python linting (isort only), 120-col, LF endings, py313 target

**Build Outputs:**
- `build/bin/quiver.dll` (Windows) / `build/lib/libquiver.so` (Linux) - Core library
- `build/bin/quiver_c.dll` (Windows) - C API wrapper (depends on `quiver.dll`)
- `build/bin/quiver_tests.exe` - C++ test binary
- `build/bin/quiver_c_tests.exe` - C API test binary

## Platform Requirements

**Development:**
- CMake 3.21+
- C++20-capable compiler (MSVC 2022 on Windows, GCC/Clang on Linux/macOS)
- MinGW supported (static runtime link for portable binaries)
- Julia 1.12+ (for Julia binding development)
- Dart SDK 3.10+ (for Dart binding development)
- Python 3.13+ (for Python binding development)
- clang-format and clang-tidy (optional, for `format`/`tidy` CMake targets)

**Production:**
- Windows: `libquiver.dll` + `libquiver_c.dll` must be in PATH (for bindings)
- Linux/macOS: RPATH set to `$ORIGIN/../lib` so libraries resolve relative to executable
- Cross-platform shared library (Windows .dll, Linux .so, macOS .dylib)

---

*Stack analysis: 2026-02-20*
