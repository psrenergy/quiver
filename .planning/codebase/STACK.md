# Stack

## Languages

| Language | Standard | Usage |
|----------|----------|-------|
| C++ | C++20 | Core library (`src/`), binary subsystem (`src/binary/`) |
| C | C11 | FFI layer (`src/c/`, `include/quiver/c/`) |
| Julia | 1.x | Binding (`bindings/julia/`) |
| Dart | 3.x | Binding (`bindings/dart/`) |
| Python | 3.x | Binding via CFFI ABI-mode (`bindings/python/`) |
| Lua | 5.4.8 | Embedded scripting via sol2 (`src/lua_runner.cpp`) |
| SQL | SQLite | Schema definitions (`tests/schemas/`), runtime queries |
| CMake | 3.26+ | Build system (`CMakeLists.txt`, `cmake/`) |

## Build System

- **CMake 3.26+** with FetchContent for all C/C++ dependencies
- **scikit-build-core** integration for Python wheel packaging (`SKBUILD` detection)
- Build options: `QUIVER_BUILD_SHARED`, `QUIVER_BUILD_TESTS`, `QUIVER_BUILD_C_API`
- Output: `build/bin/` (executables), `build/lib/` (libraries)
- Build scripts: `scripts/build-all.bat`, `scripts/test-all.bat`

### Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `quiver` | Shared/Static lib | Core C++ library |
| `quiver_c` | Shared lib | C API wrapper (optional) |
| `quiver_cli` | Executable | CLI entry point |
| `quiver_tests` | Executable | C++ test suite |
| `quiver_c_tests` | Executable | C API test suite |
| `quiver_benchmark` | Executable | Transaction performance benchmark |

## C++ Dependencies (FetchContent)

| Dependency | Version | Purpose |
|------------|---------|---------|
| SQLite | 3.50.2 | Embedded database engine (PUBLIC link) |
| toml++ | 3.4.0 | TOML parsing for binary metadata sidecars |
| spdlog | 1.17.0 | Structured logging |
| Lua | 5.4.8 | Embedded scripting runtime |
| sol2 | 3.5.0 | C++ Lua binding library |
| rapidcsv | 8.92 | CSV reading/writing (header-only) |
| argparse | 3.2 | CLI argument parsing (header-only) |
| GoogleTest | 1.17.0 | Testing framework (test builds only) |

## Binding Dependencies

### Julia
- Julia standard library, CFFI via `ccall`
- Tests via Julia `Test` module

### Dart
- `ffi` package for native interop
- `libquiver_c.dll` + `libquiver.dll` must be in PATH

### Python
- CFFI (ABI-mode, no compiler at install)
- `_loader.py` pre-loads `libquiver.dll` on Windows
- pytest for testing

## Compiler Configuration

- **MSVC**: `/W4 /permissive- /Zc:__cplusplus /utf-8`, `/bigobj` for sol2-heavy files
- **GCC/Clang**: `-Wall -Wextra -Wpedantic -Wno-unused-parameter`
- **MinGW**: Static linking of runtime libraries (`-static-libgcc -static-libstdc++`)
- Position-independent code enabled globally
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` for IDE support

## Platform

- Primary target: **Windows 11** (MSVC and MinGW)
- Cross-platform CMake structure supports Linux/macOS
- All scripts have `.bat` + `.sh` variants
