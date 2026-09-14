# Technology Stack

**Analysis Date:** 2026-09-14

## Languages

**Primary:**
- C++ 20 - Core database library, Lua bindings, binary/expression subsystems (`include/quiver/`, `src/`)
- C - FFI API surface for bindings (`include/quiver/c/`, `src/c/`)

**Secondary:**
- Julia - Quiver.jl binding published to Julia registry (`bindings/julia/`)
- Dart - quiverdb on pub.dev, native-assets hook-driven build (`bindings/dart/`)
- Python - quiverdb on PyPI, scikit-build-core wheel build (`bindings/python/`)
- JavaScript/TypeScript - quiverdb on npm, Bun runtime only (`bindings/js/`)
- Lua 5.4 - Embedded scripting via sol2, sandboxed file operations (`src/lua_runner.cpp`)

## Runtime

**Environment:**
- C++20 standard library + platform-specific libc
- SQLite 3 (3.50.2) via `sqlite3-cmake` wrapper
- Lua 5.4.8 via `lua-cmake` wrapper

**Package Managers:**
- npm/bun for JS (`bindings/js/package.json`)
- pip/uv for Python (`bindings/python/pyproject.toml`, requires Python ≥3.13)
- Julia package manager (1.11+) for Julia (`bindings/julia/Project.toml`)
- pub.dev for Dart (SDK ^3.10.0) (`bindings/dart/pubspec.yaml`)

**Lockfiles:**
- `bindings/julia/Manifest.toml` - Julia manifest
- `package-lock.json` (npm) for JS builds
- Python: no lockfile, dependencies managed via `pyproject.toml` (cffi>=2.0.0)
- Dart: `pubspec.lock`

## Frameworks & Core Dependencies

**C++ Core:**
- sqlite3 3.50.2 (FetchContent) - SQLite database engine
- toml++ 3.4.0 (FetchContent) - TOML parsing for binary metadata
- spdlog 1.17.0 (FetchContent) - Structured logging
- lua 5.4.8 (FetchContent via lua-cmake) - Lua runtime
- sol2 3.5.0 (FetchContent) - C++/Lua bindings via sol2
- rapidcsv 8.92 (FetchContent, header-only) - CSV reading/writing
- argparse 3.2 (FetchContent, header-only) - CLI argument parsing

**Testing:**
- googletest 1.17.0 (FetchContent, C++ core and C API tests)
- Julia: Test stdlib
- Dart: test framework (^1.31.2)
- Python: pytest (>=8.4.1)
- JavaScript: bun:test (built into Bun runtime)

**Binding-Specific:**
- Dart FFI: ffi (^2.2.0), ffigen (dev, ^20.1.1), native_toolchain_cmake (^0.3.0), code_assets (>=1.2.1 <3.0.0), hooks (>=2.0.2 <3.0.0)
- Python CFFI: cffi (>=2.0.0 for ABI-mode, no compiler needed at runtime)
- Julia: CEnum (0.5), Artifacts (1), Libdl (1), Dates (1) - all stdlib or stdlib-like

## Build System

**CMake:**
- Version ≥3.26.0 (minimum required)
- Generator: Ninja (default), Visual Studio 17 2022 (Windows), Xcode (macOS)
- Presets: dev (Debug), release, windows-release, linux-release (in `CMakePresets.json`)
- FetchContent for all C++ dependencies (no pre-installed requirements)

**Platform-Specific:**
- macOS: deployment target floored at 13.3 (libc++ `std::to_chars` requirement in `cmake/Platform.cmake`)
- Linux: glibc 2.17 floor for published natives (built in manylinux2014 Docker image via `scripts/build_native_linux.sh`)
- Windows: Visual Studio 17 2022 or newer

**Build Outputs:**
- Static library: `build/lib/libquiver.a` (or `.lib` on Windows)
- Shared library: `build/lib/libquiver.so`/`.dylib`/`.dll` (version: 0.10.6)
- C API library: `build/lib/libquiver_c.so`/`.dylib`/`.dll` (when `QUIVER_BUILD_C_API=ON`)
- Executables: `build/bin/quiver_cli.exe`, `quiver_tests.exe`, `quiver_c_tests.exe`, `quiver_benchmark.exe`

## Configuration

**Environment:**
- CMake variables: `QUIVER_BUILD_SHARED` (ON), `QUIVER_BUILD_TESTS` (ON), `QUIVER_BUILD_C_API` (OFF by default, ON in wheel builds), `QUIVER_UNVERSIONED_SHARED` (OFF except Dart hook)
- scikit-build-core (Python wheels): detects `SKBUILD` CMake variable, forces C API ON, tests OFF
- Dart native-assets hook: `bindings/dart/hook/build.dart`, sets `QUIVER_UNVERSIONED_SHARED=ON`

**Build Configuration Files:**
- `CMakeLists.txt` - Root configuration, version source of truth (0.10.6)
- `cmake/Dependencies.cmake` - FetchContent declarations
- `cmake/CompilerOptions.cmake` - Compiler flags, visibility
- `cmake/Platform.cmake` - macOS/Linux platform tuning
- `.clang-format`, `.clang-tidy`, `.clangd` - C++ formatting/linting
- `CMakePresets.json` - Build presets for different configurations

## Code Quality Tooling

**Formatting:**
- clang-format (C++) configured in `.clang-format`
- JuliaFormatter (Julia) via `bindings/julia/format/`
- dart format (Dart)
- ruff (Python) via `bindings/python/ruff.toml`
- biome (JavaScript) via `bindings/js/biome.json` and `bindings/js/package.json`

**Linting:**
- clang-tidy (C++) configured in `.clang-tidy`, run via `scripts/tidy.bat`
- cppcheck (pre-commit)
- ruff (Python, import sorting only)
- biome (JavaScript)

**Pre-commit Hooks:**
- `.pre-commit-config.yaml`: trailing-whitespace, end-of-file-fixer, yaml/json checks, merge-conflict markers, clang-format, cppcheck, cmake-format
- `.gitattributes`: enforces LF for `.cpp`, `.h`, `.dart`, `.jl`, `.py`

## Platform Requirements

**Development:**
- C++ compiler: MSVC (Windows), Clang (Linux/macOS), GCC 11+ (for Linux native builds)
- CMake 3.26+
- Git
- For local Python: `uv` CLI (not plain Python) — see root CLAUDE.md "Running Python locally"

**CI/CD:**
- Ubuntu latest (ubuntu-latest), Windows latest (windows-latest), macOS latest (macos-latest)
- Docker (for manylinux Linux native build via `scripts/build_native_linux.sh`)
- GitHub Actions runners for CI/CD matrix builds

**Production:**
- macOS 13.3+ (deployment floor for shipped dylibs)
- Linux glibc 2.17+ (for published natives, built in manylinux2014)
- Windows 7+ (no floor; just "whatever Windows APIs SQLite uses")

## Versioning

**Source of Truth:** `CMakeLists.txt` `project(quiver VERSION x.y.z)`

**Synchronized manifests** (checked by `scripts/assert_version.py`):
- `bindings/python/pyproject.toml`
- `bindings/js/package.json`
- `bindings/dart/pubspec.yaml`
- `bindings/julia/Project.toml`

**Bump workflow:** `scripts/assert_version.py bump major|minor|patch` rewrites all five; GitHub Actions `bump-version.yml` workflow wraps it.

---

*Stack analysis: 2026-09-14*
