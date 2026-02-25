# Technology Stack

**Analysis Date:** 2026-02-25

## Languages

**Primary:**
- C++20 - Core library and database implementation (`src/`, `include/quiver/`)
- C - FFI API layer for language bindings (`include/quiver/c/`, `src/c/`)
- Julia - Language bindings (`bindings/julia/`)
- Dart - Language bindings (`bindings/dart/`)
- Python - Language bindings (`bindings/python/`)

**Secondary:**
- Lua - Scripting support via `sol2` (`include/quiver/lua_runner.h`, `src/lua_runner.cpp`)
- CMake - Build system (`CMakeLists.txt`, `cmake/`)
- Batch - Build and test scripts (`scripts/*.bat`)

## Runtime

**Environment:**
- C++20 compiler required (CMake enforces `CMAKE_CXX_STANDARD 20`)
- MSVC on Windows (Visual Studio 17 2022), GCC/Clang on Linux/macOS
- Julia 1.12+ for Julia bindings
- Dart SDK ^3.10.0 for Dart bindings
- Python 3.13+ for Python bindings

**Package Managers:**
- Julia: `Pkg` (built-in), lockfile: `Manifest.toml`
- Dart: `pub`, lockfile: `pubspec.lock`
- Python: `uv` (dev tooling), `hatchling` build backend
- CMake: `FetchContent` for C++ dependency management (no lockfile)

## Frameworks

**Core:**
- SQLite3 3.50.2 - Database engine (via `sjinks/sqlite3-cmake`)
- sol2 3.5.0 - Lua C++ binding library
- spdlog 1.17.0 - Structured logging framework

**Testing:**
- GoogleTest 1.17.0 - C++ unit test framework (with GMock)
- Dart `test` ^1.24.0 - Dart unit tests
- Julia `Test` (stdlib) + `Aqua` 0.8 - Julia tests
- Python `pytest` >=8.4.1 - Python tests

**Build/Dev:**
- CMake 3.21+ - C++ build system
- clang-format - C++ code formatting (`.clang-format` at root)
- clang-tidy - C++ static analysis (`.clang-tidy` at root)
- ffigen ^11.0.0 - FFI binding generator (Dart)
- `uv` - Python package and environment manager

## Key Dependencies

**Critical:**
- `SQLite::SQLite3` 3.50.2 - Primary data storage
  - Source: `https://github.com/sjinks/sqlite3-cmake.git` tag `v3.50.2`
  - Purpose: Core persistence engine for all read/write operations

**Infrastructure:**
- `tomlplusplus` 3.4.0 - TOML configuration file parsing
  - Source: `https://github.com/marzer/tomlplusplus.git` tag `v3.4.0`
- `spdlog` 1.17.0 - Logging at debug/info/warn/error/off levels
  - Source: `https://github.com/gabime/spdlog.git` tag `v1.17.0`
- `lua` (lua-cmake wrapper) 5.4.8 - Lua scripting engine (interpreter and compiler disabled, library only)
  - Source: `https://gitlab.com/codelibre/lua/lua-cmake.git` tag `lua-cmake/v5.4.8.0`
- `sol2` 3.5.0 - Lua usertype binding library (C++ header-only template library)
  - Source: `https://github.com/ThePhD/sol2.git` tag `v3.5.0`
  - Requires `/bigobj` (MSVC) or `-Wa,-mbig-obj` (MinGW) due to heavy template instantiation in `lua_runner.cpp`
- `rapidcsv` 8.92 - CSV reading/writing (header-only)
  - Source: `https://github.com/d99kris/rapidcsv.git` tag `v8.92`

**Julia binding dependencies** (`bindings/julia/Project.toml`):
- `CEnum` ^0.5 - C enum interop
- `DataFrames` ^1.0 - Tabular data return type
- `Dates` (stdlib) - DateTime parsing for convenience wrappers
- `Libdl` (stdlib) - Dynamic library loading

**Dart binding dependencies** (`bindings/dart/pubspec.yaml`):
- `ffi` ^2.1.0 - Dart FFI support
- `code_assets` ^1.0.0 - Native asset hooks
- `hooks` ^1.0.0 - Build hooks integration
- `native_toolchain_cmake` ^0.2.2 - CMake toolchain for Dart native builds
- `logging` ^1.3.0 - Dart logging
- `ffigen` ^11.0.0 (dev) - Auto-generates `bindings.dart` from C headers

**Python binding dependencies** (`bindings/python/pyproject.toml`):
- `cffi` >=2.0.0 - ABI-mode FFI (no compiler required at install time)
- `hatchling` (build) - Wheel build backend
- `pytest` >=8.4.1 (dev) - Test runner
- `ruff` >=0.12.2 (dev) - Linting and formatting
- `dotenv` >=0.9.9 (dev) - Environment variable loading in tests

**Testing:**
- `GoogleTest` 1.17.0 - C++ unit testing with GMock
  - Source: `https://github.com/google/googletest.git` tag `v1.17.0`

## Configuration

**Build Presets** (`CMakePresets.json`):
- `dev` - Debug build, tests ON, C API ON, output in `build/dev/`
- `release` - Release build, tests OFF, output in `build/release/`
- `windows-release` - VS 2022, Release, tests ON, C API ON
- `linux-release` - Unix Makefiles, Release, tests ON, C API ON
- `all-bindings` - Release, tests ON, C API ON, Python binding ON

**CMake Build Options:**
- `QUIVER_BUILD_SHARED` (default ON) - Build shared vs static library
- `QUIVER_BUILD_TESTS` (default ON) - Build GoogleTest suite
- `QUIVER_BUILD_C_API` (default OFF) - Build C FFI wrapper (`quiver_c`)

**Build Configuration Files:**
- `CMakeLists.txt` - Root configuration (project version 0.4.0)
- `src/CMakeLists.txt` - Core library and C API wrapper build rules
- `tests/CMakeLists.txt` - C++ and C API test suite configuration
- `cmake/Dependencies.cmake` - FetchContent declarations for all C++ dependencies
- `cmake/CompilerOptions.cmake` - Warning levels and compiler flags
- `cmake/Platform.cmake` - Platform-specific output paths and RPATH settings

**Compiler Flags:**
- MSVC: `/W4 /permissive- /Zc:__cplusplus /utf-8`, disable `/wd4251`
- GCC/Clang: `-Wall -Wextra -Wpedantic -Wno-unused-parameter`
- MinGW: Static linking of libgcc/libstdc++ and winpthread for portable binaries
- All platforms: Position-independent code ON, symbols hidden by default (`hidden` visibility preset)

## Platform Requirements

**Development:**
- Windows 10+: MSVC 2022 (Visual Studio 17) or MinGW-w64
- Linux: GCC 10+ or Clang 11+
- macOS: Clang 12+ (Xcode)
- CMake 3.21+ required
- Optional: LLVM (clang-format, clang-tidy) at `C:\Program Files\LLVM\bin\`
- Optional: Julia 1.12.3+ for Julia bindings
- Optional: Dart SDK 3.10.0+ for Dart bindings
- Optional: Python 3.13+ and `uv` for Python bindings

**Production Output:**
- Windows: `libquiver.dll` / `libquiver.lib` (core), `libquiver_c.dll` (C API)
- Linux: `libquiver.so` / `libquiver.a` (core), `libquiver_c.so` (C API)
- macOS: `libquiver.dylib` / `libquiver.a` (core), `libquiver_c.dylib` (C API)
- RPATH configured for executable-relative library loading on Linux (`$ORIGIN/../lib`) and macOS (`@executable_path/../lib`)

## Library Architecture

**Core Library (`quiver`):**
- Built as SHARED (default) or STATIC
- Exports C++ API via `QUIVER_API` macro (`include/quiver/export.h`)
- Compiled sources in `src/`:
  - `database*.cpp` - All database operations (create, read, update, delete, metadata, csv, query, time series, describe)
  - `element.cpp`, `row.cpp`, `result.cpp` - Data types
  - `lua_runner.cpp` - Lua scripting via sol2
  - `migration.cpp`, `migrations.cpp` - Schema migration support
  - `schema.cpp`, `schema_validator.cpp`, `type_validator.cpp` - Schema validation

**C API Wrapper (`quiver_c`) [Optional, `QUIVER_BUILD_C_API=ON`]:**
- Shared library only
- Provides stable C FFI interface consumed by all language bindings
- Sources in `src/c/`: `common.cpp`, `database*.cpp`, `element.cpp`, `lua_runner.cpp`

**Binding Layer:**
- Julia: CFFI via `Libdl` direct C API calls, auto-generated `c_api.jl` (`bindings/julia/`)
- Dart: Native FFI via `ffigen`-generated `bindings.dart`, hooks-based DLL build (`bindings/dart/`)
- Python: CFFI ABI-mode (no compiler at install), hand-written cdef in `_c_api.py`, `_loader.py` handles DLL pre-loading (`bindings/python/`)

## Version

**Current Release:** 0.4.0
- WIP status: breaking changes acceptable, no backward compatibility guarantees
- All bindings track the same version number

---

*Stack analysis: 2026-02-25*
