# Technology Stack

**Analysis Date:** 2026-02-23

## Languages

**Primary:**
- C++20 - Core library and database implementation
- C - FFI API layer for language bindings (`include/quiver/c/`)
- Julia - Language bindings (`bindings/julia/`)
- Dart - Language bindings (`bindings/dart/`)

**Secondary:**
- Lua - Scripting support via `sol2`
- CMake - Build system
- Bash/Batch - Build scripts (`scripts/`)

## Runtime

**Environment:**
- C++20 compiler required (CMake enforces `CMAKE_CXX_STANDARD 20`)
- MSVC on Windows, GCC/Clang on Linux/macOS
- Julia 1.12+ for Julia bindings
- Dart SDK 3.10.0+ for Dart bindings

**Package Manager:**
- Julia: `Pkg` (built-in)
- Dart: `pub`
- CMake: FetchContent for dependency management

## Frameworks

**Core:**
- SQLite3 3.50.2 - Database engine
- sol2 3.5.0 - Lua C++ bindings
- spdlog 1.17.0 - Logging framework

**Testing:**
- GoogleTest 1.17.0 - C++ test framework

**Build/Dev:**
- CMake 3.21+ - Build system
- clang-format - Code formatting
- clang-tidy - Static analysis
- ffigen - FFI binding generator (Dart)

## Key Dependencies

**Critical:**
- SQLite::SQLite3 3.50.2 - Primary data storage
  - Fetched from: https://github.com/sjinks/sqlite3-cmake.git
  - Why: Core persistence layer for all operations

**Infrastructure:**
- tomlplusplus 3.4.0 - Configuration file parsing
- spdlog 1.17.0 - Structured logging (debug/info/warn/error/off levels)
- lua (lua-cmake) 5.4.8 - Scripting engine
  - Fetched from: https://gitlab.com/codelibre/lua/lua-cmake.git
- sol2 3.5.0 - Lua usertype binding library
  - Fetched from: https://github.com/ThePhD/sol2.git
  - Requires `/bigobj` (MSVC) or `-Wa,-mbig-obj` (MinGW) compiler flags due to heavy template instantiation
- rapidcsv 8.92 - CSV reading/writing (header-only)
  - Fetched from: https://github.com/d99kris/rapidcsv.git

**Testing:**
- GoogleTest 1.17.0 - C++ unit testing

## Configuration

**Environment:**
- CMakePresets.json defines build configurations:
  - `dev` - Debug build with tests enabled
  - `release` - Release build without tests
  - `windows-release` - Windows-specific Release (MSVC)
  - `linux-release` - Linux-specific Release
  - `all-bindings` - Release with Python binding option (placeholder)

**Build:**
- CMakeLists.txt - Root configuration
- src/CMakeLists.txt - Core library configuration
- tests/CMakeLists.txt - Test suite configuration
- cmake/Dependencies.cmake - External dependency declarations
- cmake/CompilerOptions.cmake - Warning levels and compiler flags
- cmake/Platform.cmake - Platform-specific settings (Windows/Linux/macOS)

**Compiler Flags:**
- MSVC: `/W4 /permissive- /Zc:__cplusplus /utf-8`
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MinGW: Static linking of libgcc/libstdc++ for portability
- All platforms: Position-independent code enabled, symbols hidden by default

## Platform Requirements

**Development:**
- Windows 10+: MSVC 2022 or MinGW-w64
- Linux: GCC 10+ or Clang 11+
- macOS: Clang 12+ (Xcode)
- CMake 3.21+
- Optional: clang-format, clang-tidy for code quality

**Production:**
- Windows: `.dll` (shared library) or `.lib` (static)
- Linux: `.so` (shared library) or `.a` (static)
- macOS: `.dylib` (shared library) or `.a` (static)
- RPATH configured for executable-relative library loading (Linux/macOS)

## Library Architecture

**Core Library (quiver):**
- Built as SHARED (default) or STATIC library
- Exports C++ API and optionally C API (`QUIVER_BUILD_C_API`)
- Compiled sources:
  - `database_*.cpp` - Database operations (create, read, update, delete, metadata, csv, query, relations, time series, describe)
  - `element.cpp` - Element builder pattern
  - `lua_runner.cpp` - Lua integration via sol2
  - `migration.cpp`, `migrations.cpp` - Schema migration support
  - `result.cpp`, `row.cpp` - Query result handling
  - `schema.cpp`, `schema_validator.cpp`, `type_validator.cpp` - Schema validation

**C API Wrapper (quiver_c) [Optional]:**
- Shared library only (no static option)
- Provides C FFI interface for language bindings
- Includes:
  - `c/common.cpp` - Shared error handling and utilities
  - `c/database*.cpp` - C function wrappers matching C++ API
  - `c/element.cpp`, `c/lua_runner.cpp` - C binding support

**Bindings:**
- Julia: Uses FFI via direct C API calls (`bindings/julia/`)
- Dart: Uses native FFI via `ffigen` code generator (`bindings/dart/`)

## Version

**Current Release:** 0.3.0 (as of CMakeLists.txt)
- Supports breaking changes (WIP status)
- No backward compatibility guarantees

---

*Stack analysis: 2026-02-23*
