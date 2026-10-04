# Technology Stack

**Analysis Date:** 2026-10-02

## Languages

**Primary:**
- C++20 - core library (`src/`, `include/quiver/`), Lua runner (`src/lua_runner.cpp`), binary/expression subsystems (`src/binary/`)
- C (C API surface, compiled as C++) - FFI layer (`src/c/`, `include/quiver/c/`)

**Secondary:**
- Julia >= 1.11 - `bindings/julia/` (Quiver.jl)
- Dart SDK ^3.10.0 - `bindings/dart/` (quiverdb on pub)
- Python >= 3.13 - `bindings/python/` (quiverdb on PyPI)
- TypeScript on Bun - `bindings/js/` (quiverdb on npm)
- Lua 5.4.8 - embedded scripting via sol2 (`src/lua_runner.cpp`)
- SQL - test schemas in `tests/schemas/`
- Bash / Windows batch - `scripts/`, `scripts/ci/`

## Runtime

**Environment:**
- Native shared libraries: `libquiver` (core) + `libquiver_c` (C API), output to `build/bin/` and `build/lib/`
- Bindings load `libquiver_c` at runtime via each host's FFI (CFFI ABI-mode, Bun FFI, Julia `ccall`, `dart:ffi`)
- macOS deployment target floored at 13.3 (`cmake/Platform.cmake`) for floating-point `std::to_chars`
- Linux natives: glibc 2.17 floor (x86_64, manylinux2014 + GCC 11), glibc 2.28 floor (aarch64, manylinux_2_28) via `scripts/build_native_linux.sh`

**Package Manager:**
- CMake FetchContent for C++ deps (`cmake/Dependencies.cmake`) - no lockfile; tags pinned
- uv / pip for Python (`bindings/python/pyproject.toml`, dependency-groups `dev`)
- Bun for JS (`bindings/js/package.json`); no `package-lock.json`
- pub for Dart (`bindings/dart/pubspec.yaml`)
- Julia Pkg (`bindings/julia/Project.toml`); `Manifest.toml` gitignored

## Frameworks

**Core:**
- SQLite 3.53.4 - storage engine (psrenergy/sqlite3-cmake fork), thread-safe serialized
- sol2 3.5.0 - C++/Lua binding
- Lua 5.4.8 - via lua-cmake wrapper (gitlab codelibre)

**Testing:**
- GoogleTest 1.17.0 - `quiver_tests`, `quiver_c_tests` (`tests/`)
- Julia `Test` stdlib - `bindings/julia/test/`
- Dart `test` ^1.31.2 - `bindings/dart/test/`
- pytest >= 8.4.1 - `bindings/python/tests/`
- `bun test` - `bindings/js/test/`

**Build/Dev:**
- CMake >= 3.26 + Ninja; presets `dev`/`release` in `CMakePresets.json`
- scikit-build-core >= 0.10 - Python wheel build backend (drives root CMake, `SKBUILD` forces C API ON, tests OFF)
- cibuildwheel - wheels for `cp313-win_amd64`, `cp313-manylinux_x86_64`, `cp313-manylinux_aarch64`
- native_toolchain_cmake ^0.3.0 + hooks/code_assets - Dart native-assets build hook (`bindings/dart/hook/build.dart`, sets `QUIVER_UNVERSIONED_SHARED=ON`)
- ffigen ^20.1.1 - Dart bindings generator -> `bindings/dart/lib/src/ffi/bindings.dart`
- Clang.jl generator - `bindings/julia/generator/` -> `bindings/julia/src/c_api.jl`
- Python generator (cdecl diff aid) - `bindings/python/generator/`
- clang-format 22.1.8 (pinned), clang-tidy (`scripts/tidy.bat`), cppcheck (pre-commit)
- JuliaFormatter, `dart format`, ruff >= 0.12.2, biome ^2.4.6 - per-binding formatters (`scripts/format.bat`)

## Key Dependencies

**Critical:**
- sqlite3 v3.53.4 - the database; `sqlite3_ENABLE_THREADSAFE` FORCEd ON
- csv-parser 5.3.0 - sole CSV parser behind `csv_read::Reader` (`src/csv/csv_read.cpp`); `CSV_NO_SIMD` FORCEd (prevents AVX2 propagation / SIGILL)
- tomlplusplus v3.4.0 - binary `.toml` metadata sidecars and `ui/` sidecar parsing
- cffi >= 2.0.0 - only runtime dep of the Python binding
- ffi ^2.2.0, logging ^1.3.0 - Dart runtime deps
- CEnum 0.5, Artifacts, Libdl, Dates - Julia deps

**Infrastructure:**
- spdlog v1.17.0 - logging (`DatabaseOptions.console_level`)
- argparse v3.2 - `quiver_cli` argument parsing
- @types/bun - JS dev types

## Configuration

**Environment:**
- No runtime env vars required by the library; options pass via `DatabaseOptions` (`read_only`, `console_level`)
- CI-only: `QUIVER_S3_BUCKET` (default `julia-artifacts`), `QUIVER_S3_PREFIX` (default `quiver`) in `scripts/ci/native_s3.sh`
- No `.env` files present

**Build:**
- `CMakeLists.txt` (version 0.13.0, single source of truth), `CMakePresets.json`
- `cmake/CompilerOptions.cmake`, `cmake/Dependencies.cmake`, `cmake/Platform.cmake`, `cmake/quiverConfig.cmake.in`
- CMake options: `QUIVER_BUILD_SHARED` (ON), `QUIVER_BUILD_TESTS` (ON), `QUIVER_BUILD_C_API` (OFF), `QUIVER_UNVERSIONED_SHARED` (OFF)
- Lint/format: `.clang-format`, `.clang-tidy`, `.clangd`, `.pre-commit-config.yaml`, `.gitattributes` (LF everywhere)
- Version manifests kept in sync by `scripts/assert_version.py`: `bindings/python/pyproject.toml`, `bindings/js/package.json`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`

## Platform Requirements

**Development:**
- C++20 compiler (MSVC/MinGW on Windows, GCC >= 11, clang), CMake >= 3.26, Ninja
- Julia 1.11+, Dart 3.10+, Python 3.13 (run via `uv`), Bun
- Docker for reproducing Linux natives (`scripts/build_native_linux.sh`)

**Production:**
- Shipped native targets: `linux-x86_64`, `linux-aarch64`, `macos-aarch64`, `windows-x86_64`
- Distributed via PyPI wheels, npm (bundled `libs/`), Julia artifacts (S3), Dart native-assets (built from source on install)

---

*Stack analysis: 2026-10-02*
