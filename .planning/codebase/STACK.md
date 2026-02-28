# Technology Stack

**Analysis Date:** 2026-02-27

## Languages

**Primary:**
- C++20 - Core library and database implementation (`src/`, `include/quiver/`)
- C - C API layer for FFI interop (`src/c/`, `include/quiver/c/`)

**Secondary:**
- Julia 1.12+ - Language binding (`bindings/julia/`)
- Dart 3.10+ - Language binding (`bindings/dart/`)
- Python 3.13+ - Language binding (`bindings/python/`)
- Lua 5.4.8 - Embedded scripting engine, consumed inside C++ core via sol2

## Runtime

**Environment:**
- C++20 compiler required (CMake enforces `CMAKE_CXX_STANDARD 20`)
- MSVC 2022 on Windows, GCC 13+ on Linux/macOS
- Output artifacts: `libquiver.dll/.so/.dylib` (core), `libquiver_c.dll/.so/.dylib` (C API)
- Symbol visibility hidden by default; explicit exports required

**Package Manager:**
- CMake 3.21+ with FetchContent for C++ dependencies (`cmake/Dependencies.cmake`)
- Julia: built-in `Pkg` (lockfile `Manifest.toml` generated at runtime; delete if version conflicts)
- Dart: `pub` (`dart pub get`)
- Python: `uv` (`bindings/python/uv.lock` committed)

## Frameworks

**Core Build System:**
- CMake 3.21+ configured via `CMakeLists.txt`, `CMakePresets.json`, `cmake/`
- Build presets: `dev` (Debug + tests + C API), `release`, `windows-release`, `linux-release`, `all-bindings`

**Testing:**
- GoogleTest v1.17.0 + GMock - C++ and C API tests (`tests/`)
- Julia `Test` stdlib + `Aqua` 0.8 - Julia binding tests (`bindings/julia/test/`)
- Dart `test` ^1.24.0 - Dart binding tests (`bindings/dart/test/`)
- Python `pytest` 8.4.1+ - Python binding tests (`bindings/python/tests/`)

**Build/Dev:**
- scikit-build-core 0.10+ - Python wheel packaging (`bindings/python/pyproject.toml`)
- cibuildwheel 3.3.1 - Wheel matrix builds: `cp313-win_amd64`, `cp313-manylinux_x86_64`
- clang-format - C++ code formatting (format target in `CMakeLists.txt`)
- clang-tidy - C++ static analysis (run via `scripts/tidy.bat`, tidy target in `CMakeLists.txt`)
- Ruff 0.12.2+ - Python linting and formatting (`bindings/python/ruff.toml`)
- Dart `lints` ^3.0.0 + `ffigen` ^11.0.0 - Dart analysis and FFI generator (`bindings/dart/analysis_options.yaml`)
- Clang.jl (Julia) - Julia FFI binding generator (`bindings/julia/generator/`)

## Key Dependencies

**Critical (fetched via CMake FetchContent — `cmake/Dependencies.cmake`):**
- SQLite3 v3.50.2 - Core embedded relational database; fetched via `sjinks/sqlite3-cmake`; linked `PUBLIC` so consumers inherit the dependency
- sol2 v3.5.0 - C++ Lua binding (heavy templates; requires `/bigobj` on MSVC, `-Wa,-mbig-obj` on MinGW)
- Lua 5.4.8 - Scripting engine built from source via `lua-cmake` wrapper (`gitlab.com/codelibre/lua/lua-cmake`); interpreter and compiler disabled, library only
- CFFI 2.0.0+ - Python ABI-mode FFI; cdef declarations in `bindings/python/src/quiverdb/_c_api.py`

**Infrastructure:**
- spdlog v1.17.0 - Structured logging (debug/info/warn/error/off), console + file sinks per database instance
- toml++ v3.4.0 - TOML configuration file parsing (`marzer/tomlplusplus`)
- rapidcsv v8.92 - Header-only CSV read/write (`d99kris/rapidcsv`); used in `src/database_csv_export.cpp`, `src/database_csv_import.cpp`
- argparse v3.2 - Header-only CLI argument parsing (`p-ranav/argparse`); used only in `src/cli/main.cpp`
- native_toolchain_cmake ^0.2.2 - Dart native build hook invokes CMake to compile quiver (`bindings/dart/hook/build.dart`)
- ffigen ^11.0.0 - Dart FFI binding generator; output `bindings/dart/lib/src/ffi/bindings.dart` (excluded from Dart analysis)

**Julia Binding Dependencies:**
- `CEnum` 0.5 - C enum interop
- `DataFrames` 1.x - Multi-row query result type
- `Dates` stdlib - DateTime parsing
- `Libdl` stdlib - Native library loading

**Python Dev Dependencies:**
- `pytest` 8.4.1+ - Test runner
- `ruff` 0.12.2+ - Linter/formatter
- `dotenv` 0.9.9+ - Optional env overrides in tests

## Configuration

**C++ Build Flags:**
- MSVC: `/W4 /permissive- /Zc:__cplusplus /utf-8`; disables C4251 (DLL interface warning)
- GCC/Clang: `-Wall -Wextra -Wpedantic -Wno-unused-parameter`
- MinGW: Static link of `libgcc`, `libstdc++`, `winpthread` for portable binaries
- All platforms: Position-independent code (`CMAKE_POSITION_INDEPENDENT_CODE ON`)

**Python Formatting (`bindings/python/ruff.toml`):**
- line-length: 120; indent-width: 4; target-version: py313; quote-style: double; line-ending: LF
- Lint: isort checks only (`select = ["I"]`)

**Dart Formatting (`bindings/dart/analysis_options.yaml`):**
- page_width: 120; trailing_commas: preserve; based on `package:lints/recommended.yaml`

**sol2 Safety (`src/CMakeLists.txt`):**
- `SOL_SAFE_NUMERICS=1`, `SOL_SAFE_FUNCTION=1` compile definitions on `quiver` target

**Build Config Files:**
- `CMakeLists.txt` - Root build entry
- `CMakePresets.json` - Named configuration/build/test presets
- `cmake/Dependencies.cmake` - FetchContent dependency declarations with pinned Git tags
- `cmake/CompilerOptions.cmake` - Warning levels and linker flags
- `cmake/Platform.cmake` - RPATH, symbol visibility, lib prefix/suffix per OS
- `cmake/quiverConfig.cmake.in` - Package config for `find_package(quiver)` support
- `bindings/python/pyproject.toml` - Python package, build-system, cibuildwheel config
- `bindings/dart/pubspec.yaml` - Dart package manifest (includes ffigen config)
- `bindings/julia/Project.toml` - Julia package with compat constraints
- `codecov.yml` - Coverage threshold configuration (1% minimum, ignores `tests/` and `build/`)

## Platform Requirements

**Development:**
- CMake 3.21+ and C++20 compiler
- Julia 1.12+ for Julia binding work
- Dart SDK 3.10+ for Dart binding work
- Python 3.13+ with `uv` for Python binding work
- `clang-format` and `clang-tidy` optional (for format/tidy CMake targets)

**Production:**
- Shared libraries: `libquiver.dll/.so/.dylib` + `libquiver_c.dll/.so/.dylib`
- Windows: both DLLs must be in PATH; on Linux `$ORIGIN` RPATH handles co-location
- Python wheels target `cp313-win_amd64` and `cp313-manylinux_x86_64`; bundled `_libs/` directory for wheel installs
- PyPI publishing via OIDC trusted publisher on `v*.*.*` tags (`.github/workflows/publish.yml`)
- GitHub Releases automatically attach `.whl` files

## Version

**Current Release:** 0.5.0 (defined in `CMakeLists.txt`, mirrored in all binding package manifests)
- WIP status; breaking changes acceptable; no backwards compatibility guarantees

---

*Stack analysis: 2026-02-27*
