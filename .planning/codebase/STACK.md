# Technology Stack

**Analysis Date:** 2026-03-08

## Languages

**Primary:**
- C++20 - Core library implementation (`src/`, `include/quiver/`)
- C - C API wrapper layer (`include/quiver/c/`, `src/c/`)

**Secondary:**
- Julia 1.12+ - Language bindings (`bindings/julia/`)
- Dart 3.10+ - Language bindings (`bindings/dart/`)
- Python 3.13+ - Language bindings (`bindings/python/`)
- Lua 5.4 - Embedded scripting engine for database operations (`src/lua_runner.cpp`)

## Runtime

**Environment:**
- Native compiled library (shared/static); no managed runtime for C++ core
- Julia runtime 1.12+ for Julia bindings
- Dart VM / native compilation for Dart bindings
- CPython 3.13+ for Python bindings

**Package Manager:**
- C++: CMake FetchContent (no lockfile — versions pinned by `GIT_TAG` in `cmake/Dependencies.cmake`)
- Julia: Pkg (lockfile: `bindings/julia/Manifest.toml`)
- Dart: pub (lockfile: `bindings/dart/pubspec.lock`)
- Python: uv (lockfile: `bindings/python/uv.lock`)

## Frameworks

**Core:**
- CMake 3.26+ - Build system (`CMakeLists.txt`, `cmake/`)
- CTest - C++ test runner (integrated with CMake)
- Ninja - Preferred build generator (used in `scripts/build-all.bat`)

**Testing:**
- GoogleTest v1.17.0 - C++ unit test framework (fetched via FetchContent)
- GMock - Mocking (bundled with GoogleTest)
- Julia `Test` stdlib + `Aqua` 0.8 - Julia binding tests
- Dart `test` 1.24+ - Dart binding tests
- pytest 8.4+ - Python binding tests

**Build/Dev:**
- scikit-build-core 0.10+ - Python wheel packaging backend (`bindings/python/pyproject.toml`)
- cibuildwheel 3.3.1 - Cross-platform wheel builder (GitHub Actions CI)
- clang-format 21 - C++ code formatter (config: `.clang-format`)
- clang-tidy - C++ static analysis (config: `.clang-tidy`)
- ruff 0.12+ - Python linter/formatter (`bindings/python/ruff.toml`)
- JuliaFormatter 2+ - Julia formatter (`bindings/julia/.JuliaFormatter.toml`)
- Dart analyzer with `lints` 3.0+ (`bindings/dart/analysis_options.yaml`)

## Key Dependencies

**Critical:**
- SQLite 3.50.2 - Primary database engine; fetched via `sjinks/sqlite3-cmake` wrapper (`cmake/Dependencies.cmake`)
- Lua 5.4.8 - Embedded scripting; fetched via `lua-cmake` GitLab wrapper (`cmake/Dependencies.cmake`)
- sol2 v3.5.0 - C++ Lua binding library; enables fluent Lua/C++ interop (`cmake/Dependencies.cmake`, used in `src/lua_runner.cpp`)
- toml++ v3.4.0 - TOML parsing for blob metadata sidecar files (`cmake/Dependencies.cmake`, used in `src/blob/blob_metadata.cpp`)
- spdlog v1.17.0 - Structured logging (`cmake/Dependencies.cmake`)
- rapidcsv v8.92 - Header-only CSV reading/writing (`cmake/Dependencies.cmake`)
- argparse v3.2 - CLI argument parsing for `quiver_cli` (`cmake/Dependencies.cmake`)

**Julia Binding Dependencies:**
- `CEnum` 0.5 - Julia enum type wrappers
- `DataFrames` 1 - DataFrame support for query results
- `Dates` stdlib - DateTime parsing/formatting
- `Libdl` stdlib - Dynamic library loading
- `Clang` 0.19.2 (generator only) - FFI binding generator (`bindings/julia/generator/Project.toml`)

**Dart Binding Dependencies:**
- `ffi` 2.1+ - Dart FFI support
- `code_assets` 1.0+ - Native asset support
- `hooks` 1.0+ - Build hooks
- `native_toolchain_cmake` 0.2.2+ - CMake integration for Dart native builds
- `logging` 1.3+ - Logging support
- `ffigen` 11.0+ (dev) - Dart FFI binding generator from C headers

**Python Binding Dependencies:**
- `cffi` 2.0.0 - C Foreign Function Interface (ABI-mode, no compiler at install time)
- `pytest` 8.4+ (dev) - Test runner
- `ruff` 0.12+ (dev) - Linter/formatter
- `dotenv` 0.9.9+ (dev) - Environment loading

## Configuration

**Environment:**
- No required environment variables for the core library
- Python dev mode requires build DLLs on PATH (handled by `bindings/python/tests/test.bat`)
- Julia tests require `LD_LIBRARY_PATH` pointing to `build/lib/` on Linux
- Dart tests require `libquiver.dll` and `libquiver_c.dll` in PATH (handled by `bindings/dart/test/test.bat`)

**Build:**
- `CMakePresets.json` - Defines `dev`, `release`, `windows-release`, `linux-release`, `all-bindings` presets
- `cmake/Dependencies.cmake` - All external dependency version pins
- `cmake/CompilerOptions.cmake` - MSVC/GCC/Clang warning flags and MinGW static link options
- `cmake/Platform.cmake` - Platform detection, RPATH settings, symbol visibility
- `.clang-format` - LLVM-based style, 4-space indent, 120-column limit, C++20 standard
- `.clang-tidy` - `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming` checks

**Key Build Options:**
- `QUIVER_BUILD_SHARED` (ON) - Build shared library (default ON)
- `QUIVER_BUILD_TESTS` (ON) - Build test suite
- `QUIVER_BUILD_C_API` (OFF default, ON in dev/CI) - Build C API wrapper (`libquiver_c`)
- `SKBUILD` - Defined by scikit-build-core for Python wheel builds; auto-enables C API, disables tests

## Platform Requirements

**Development:**
- CMake 3.26+ required
- C++20-capable compiler: MSVC (Visual Studio 2022 recommended), GCC 13+, Clang
- Ninja generator recommended for local builds
- Windows: MSVC `/W4 /permissive-` flags; MinGW supported with static runtime link
- Linux/macOS: `-Wall -Wextra -Wpedantic` flags; GCC 13+ required on macOS CI

**Production (Library Distribution):**
- Windows: `libquiver.dll` + `libquiver_c.dll` (both needed in PATH for bindings)
- Linux: `libquiver.so` + `libquiver_c.so` (RPATH `$ORIGIN/../lib` set for installed binaries)
- macOS: `libquiver.dylib` + `libquiver_c.dylib` (RPATH `@executable_path/../lib`)
- Python: Distributed as platform wheels via PyPI; wheels bundle DLLs in `quiverdb/_libs/`

**Outputs:**
- `build/bin/quiver_tests.exe` - C++ test executable
- `build/bin/quiver_c_tests.exe` - C API test executable
- `build/bin/quiver_cli.exe` - CLI tool
- `build/bin/quiver_benchmark.exe` - Transaction performance benchmark (not in test suite)
- `build/lib/libquiver.{dll,so,dylib}` - Core shared library
- `build/lib/libquiver_c.{dll,so,dylib}` - C API shared library

---

*Stack analysis: 2026-03-08*
