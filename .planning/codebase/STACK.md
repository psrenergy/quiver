# Technology Stack

**Analysis Date:** 2026-02-09

## Languages

**Primary:**
- C++ 20 - Core SQLite wrapper library, main business logic
- C - C API wrapper layer for FFI bindings
- Julia 1.12+ - Julia language bindings

**Secondary:**
- Dart 3.10+ - Dart/Flutter bindings
- Lua 5.4.8 - Scripting runtime for database operations
- Batch (Windows) - Build and test scripts

## Runtime

**Environment:**
- CMake 3.21+ (required for build)
- Native compilation targets: Windows (MSVC/Clang), macOS (Xcode), Linux (GCC/Clang)

**Package Manager:**
- CMake FetchContent - Downloads and manages C++ dependencies
- Julia Package Manager (Pkg) - Manages Julia dependencies
- Dart pub - Manages Dart dependencies
- Cargo/pip - Not used

## Frameworks

**Core:**
- SQLite3 (v3.50.2) - Embedded SQL database engine via `sqlite3-cmake`
- spdlog (v1.17.0) - Structured logging library
- toml++ (v3.4.0) - TOML configuration parsing
- Lua 5.4.8 - Scripting engine via `lua-cmake`
- sol2 (v3.5.0) - C++ bindings for Lua

**Testing:**
- GoogleTest (v1.17.0) - C++ unit testing framework
- Aqua (0.8) - Julia code quality/style checking
- Dart test (1.24.0) - Dart testing framework
- Julia Test (1.0) - Julia built-in testing framework

**Build/Dev:**
- ffigen (11.0.0) - Dart FFI bindings generator from C headers
- code_assets (1.0.0) - Dart code asset management for native libraries
- hooks (1.0.0) - Dart native build hooks
- native_toolchain_cmake (0.2.2) - Dart CMake integration for native builds
- clang-format - Code formatting (searched at build time, optional)

## Key Dependencies

**Critical:**
- SQLite3 - All persistent data storage; no alternative database
- spdlog - Debug/info/warn/error logging throughout C++ codebase
- Lua - Script execution engine for LuaRunner class

**Infrastructure:**
- toml++ - Parses migration/schema configuration files
- sol2 - Bridges C++ and Lua; heavy template instantiation (requires MSVC `/bigobj` flag)
- CEnum (Julia) - C enum bindings for Julia FFI
- DataFrames (Julia) - Julia data structure compatibility
- logging (Dart) - Structured logging in Dart bindings

## Configuration

**Environment:**
- No environment variables required for build or runtime
- `.env` files: Not used
- Configuration passed via DatabaseOptions struct: `read_only` flag and `console_level` (log level enum)

**Build:**
- `CMakeLists.txt` (root) - Main build configuration
- `cmake/Dependencies.cmake` - FetchContent declarations for all C++ libraries
- `cmake/CompilerOptions.cmake` - Compiler flags and warnings (implicit)
- `cmake/Platform.cmake` - Platform-specific settings (Windows DLL naming, RPATH for Unix)
- `bindings/julia/Project.toml` - Julia project manifest
- `bindings/dart/pubspec.yaml` - Dart package manifest with ffigen config

## Build Options

**CMake Flags:**
- `QUIVER_BUILD_SHARED` (ON by default) - Build shared library (.dll/.so/.dylib)
- `QUIVER_BUILD_TESTS` (ON by default) - Build test executables
- `QUIVER_BUILD_C_API` (OFF by default) - Build C API wrapper (required for bindings)
- `CMAKE_BUILD_TYPE` - Debug (default) or Release

## Platform Requirements

**Development:**
- CMake 3.21+
- C++20 compiler: MSVC (Windows), Clang/GCC (Linux), Clang (macOS)
- Julia 1.12+ (for Julia bindings)
- Dart SDK 3.10+ (for Dart bindings)
- Windows: MSVC needs `/bigobj` compiler flag for sol2 template instantiation
- Windows: Both `libquiver.dll` and `libquiver_c.dll` must be in PATH for Dart tests

**Production:**
- No external runtime dependencies beyond platform libc
- SQLite embedded in library
- Lua embedded in library
- Deployment as shared library (.dll, .so, .dylib) or static library (.lib, .a)

---

*Stack analysis: 2026-02-09*
