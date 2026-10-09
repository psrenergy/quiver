---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
# Technology Stack

**Analysis Date:** 2026-10-07

## Languages

**Primary:**

- C++20: database, schema validation, migrations, binary files, expression trees, embedded scripting, and CLI in `include/quiver/`, `src/`, and `src/cli/main.cpp`; standard enforced by `CMakeLists.txt`.
- C ABI: opaque-handle FFI surface in `include/quiver/c/` and C++ implementation in `src/c/`; C and C++ are enabled in `CMakeLists.txt`.

**Secondary:**

- Julia 1.11-compatible package in `bindings/julia/Project.toml` and `bindings/julia/src/`.
- Dart SDK `^3.10.0` in `bindings/dart/pubspec.yaml`; CI uses 3.11.0 in `.github/workflows/ci.yml`.
- Python >=3.13 in `bindings/python/pyproject.toml`; binding code in `bindings/python/src/quiverdb/`.
- TypeScript ESM executed by Bun in `bindings/js/mod.ts` and `bindings/js/src/`; package exports source `.ts` files directly in `bindings/js/package.json`.
- Lua 5.4.8 embedded through sol2 in `src/sandbox/`; SQL schemas/migrations in `tests/schemas/`; Bash, Windows batch, Python, and Julia automation in `scripts/`.

## Runtime

**Environment:**

- Native shared libraries `quiver` and optional `quiver_c`, plus `quiver_cli`, defined in `src/CMakeLists.txt`; static core supported by `QUIVER_BUILD_SHARED=OFF` in `CMakeLists.txt`.
- SQLite runs in-process, with serialized/thread-safe build forced in `cmake/Dependencies.cmake`; there is no database server requirement.
- Lua is linked into the core; `src/sandbox/sandbox.cpp` constructs restricted per-sandbox Lua state, rather than invoking a standalone interpreter.
- Julia >=1.11, Dart >=3.10 within the declared compatible SDK range, Python >=3.13, and Bun are the host runtimes recorded in binding manifests. Bun CI uses `latest`, with no pinned runtime version in `.github/workflows/ci.yml`.
- Node 24 and latest npm are publishing tools for npm OIDC, not the binding runtime (`.github/workflows/publish-js.yml`).

**Package Manager:**

- CMake FetchContent with exact Git tags for native dependencies (`cmake/Dependencies.cmake`); no native dependency lockfile.
- Julia Pkg with compatibility ranges in `bindings/julia/Project.toml`; publication excludes `Manifest.toml` in `.github/workflows/publish-julia.yml`.
- Dart pub with `bindings/dart/pubspec.lock` present; Python uv with `bindings/python/uv.lock` present.
- Bun package installation and npm publication in `bindings/js/package.json` and `.github/workflows/publish-js.yml`; no Bun or npm lockfile detected in `bindings/js/`.
- Package-manager binary versions are not pinned in manifests; CI provisions uv and Bun via setup actions (`.github/workflows/ci.yml`).

## Frameworks

**Core:**

- Native library, not a web framework: `Database` and `Sandbox` public APIs in `include/quiver/database.h` and `include/quiver/sandbox.h`.
- sol2 3.5.0 bridges Lua to C++ directly; other languages use the C ABI (`cmake/Dependencies.cmake`, `src/sandbox/internal.h`, `include/quiver/c/`).
- Python CFFI ABI mode >=2.0.0, locked 2.1.1: generated declarations and dynamic loading (`bindings/python/pyproject.toml`, `bindings/python/uv.lock`, `bindings/python/src/quiverdb/_loader.py`).
- Dart `dart:ffi`, generated bindings, and native-assets build hook (`bindings/dart/lib/src/ffi/bindings.dart`, `bindings/dart/hook/build.dart`).
- Julia `@ccall` wrappers generated with Clang.jl (`bindings/julia/src/c_api.jl`, `bindings/julia/generator/Project.toml`).
- Bun `bun:ffi` with hand-written symbol descriptions and lazy loading (`bindings/js/src/loader.ts`).

**Testing:**

- GoogleTest 1.17.0 plus CTest discovery in `cmake/Dependencies.cmake` and `tests/CMakeLists.txt`.
- Julia stdlib Test, compatibility 1, in `bindings/julia/Project.toml` and `bindings/julia/test/`.
- Dart test `^1.31.2`, locked 1.31.2, and coverage `^1.15.1` in `bindings/dart/pubspec.yaml` and `bindings/dart/pubspec.lock`.
- Python pytest >=8.4.1, locked 9.1.1, in `bindings/python/pyproject.toml` and `bindings/python/uv.lock`.
- Bun built-in runner via `bun test test` in `bindings/js/package.json`; CI supplies native libraries before testing (`.github/workflows/ci.yml`).

**Build/Dev:**

- CMake >=3.26.0, C++20-capable compiler, Git for FetchContent; Ninja Debug/Release presets in `CMakePresets.json`.
- MSVC and non-MSVC warning policies, MinGW runtime linkage, PIC, and compile-command generation in `cmake/CompilerOptions.cmake`.
- scikit-build-core >=0.10 builds Python wheels from root CMake; cibuildwheel 4.2.1 action builds platform wheels (`bindings/python/pyproject.toml`, `.github/workflows/publish-python.yml`).
- clang-format 22.1.8 pinned for pre-commit and CI; clang-tidy configuration in `.clang-tidy`, formatting in `.clang-format`, static analysis hooks in `.pre-commit-config.yaml`.
- Julia generator Clang.jl exactly 0.19.2; formatter Style.jl exactly 0.1.0 from PSR repository (`bindings/julia/generator/Project.toml`, `bindings/julia/format/Project.toml`).
- Dart ffigen `^20.1.1`, locked 20.1.1, and lints `^6.1.0` in `bindings/dart/pubspec.yaml`; header list and generator settings also in `bindings/dart/ffigen.yaml`.
- Python Ruff >=0.12.2, locked 0.16.3 (`bindings/python/pyproject.toml`, `bindings/python/uv.lock`).
- Biome `^2.4.6` and `@types/bun: latest` in `bindings/js/package.json`; configuration in `bindings/js/biome.json`.
- sccache and FetchContent caching via `.github/actions/build-cpp/action.yml`; actionlint 1.7.9 in `.github/workflows/ci.yml`.

## Key Dependencies

**Critical:**

- SQLite 3.53.4, PSR-maintained sqlite3-cmake fork: storage engine, linked as `SQLite::SQLite3` (`cmake/Dependencies.cmake`, `src/CMakeLists.txt`).
- toml++ 3.4.0: binary metadata and UI metadata parsing (`cmake/Dependencies.cmake`, `src/binary/binary_metadata.cpp`, `src/ui_metadata.cpp`).
- spdlog 1.17.0: per-database stderr and file sinks (`cmake/Dependencies.cmake`, `src/database.cpp`).
- Lua 5.4.8 via lua-cmake tag `lua-cmake/v5.4.8.0`, and sol2 3.5.0: embedded sandbox (`cmake/Dependencies.cmake`, `src/sandbox/sandbox.cpp`).
- csv-parser 5.3.0: shared streaming parser for Lua CSV reads and database import; threads and SIMD disabled (`cmake/Dependencies.cmake`, `src/csv/csv_read.cpp`, `src/database_csv_import.cpp`).
- OpenXLSX 0.5.1: internal Lua-only workbook reading; static dependency graph includes miniz, pugixml, and nowide, with upstream pins rather than independent root version declarations (`cmake/Dependencies.cmake`, `src/xlsx/xlsx_read.cpp`).
- argparse 3.2: native CLI argument parsing (`cmake/Dependencies.cmake`, `src/cli/main.cpp`).

**Infrastructure:**

- Julia CEnum compatible 0.5 and stdlib Artifacts/Libdl/Dates compatible 1 (`bindings/julia/Project.toml`).
- Dart ffi `^2.2.0`, code_assets `>=1.2.1 <3.0.0`, hooks `>=2.0.2 <3.0.0`, logging `^1.3.0`, native_toolchain_cmake `^0.3.0` (`bindings/dart/pubspec.yaml`). Locked versions are respectively 2.2.0, 1.2.1, 2.1.0, 1.3.0, and 0.3.1 (`bindings/dart/pubspec.lock`).
- CFFI is Python's sole declared runtime third-party dependency; JS declares no runtime npm dependencies (`bindings/python/pyproject.toml`, `bindings/js/package.json`).
- Docker manylinux images pinned by digest provide Linux release builds; AWS CLI, curl, patchelf, objdump, and gh support packaging/release automation (`scripts/build_native_linux.sh`, `scripts/ci/native_s3.sh`, `scripts/ci/dispatch_workflow.sh`).

## Configuration

**Environment:**

- Configure build options with `QUIVER_BUILD_SHARED`, `QUIVER_BUILD_TESTS`, `QUIVER_BUILD_C_API`, and `QUIVER_UNVERSIONED_SHARED` in `CMakeLists.txt`; C API defaults OFF, tests and shared core default ON.
- Python scikit-build invocation forces C API ON and tests OFF, and installs libraries into `quiverdb/_libs` (`CMakeLists.txt`, `bindings/python/pyproject.toml`).
- Dart hook enables shared C API builds, disables tests, requests only native library targets, and uses unversioned real library files (`bindings/dart/hook/build.dart`).
- Julia `QUIVER_LIB_DIR` overrides artifact/development loading (`bindings/julia/src/c_api.jl`); release S3 location overrides are `QUIVER_S3_BUCKET` and `QUIVER_S3_PREFIX` (`scripts/ci/native_s3.sh`).
- Database runtime options are explicit `DatabaseOptions` (`read_only`, `console_level`) rather than environment-driven configuration (`include/quiver/options.h`).
- Keep domain rules in C++, expose through C ABI, and keep host bindings thin as required by `AGENTS.md`; binary/expression APIs intentionally target Julia/Lua only.

**Build:**

- Root orchestration: `CMakeLists.txt`; target/source lists: `src/CMakeLists.txt`; tests: `tests/CMakeLists.txt`.
- Dependency pins: `cmake/Dependencies.cmake`; compiler options: `cmake/CompilerOptions.cmake`; platform/RPATH policies: `cmake/Platform.cmake`; downstream package config: `cmake/quiverConfig.cmake.in`.
- Development presets: `CMakePresets.json`; batch entry points in `scripts/build-all.bat`, `scripts/test-all.bat`, and `scripts/generator.bat`.
- Core, Julia, Dart, and Python manifests declare 0.13.1; JS manifest declares 0.13.2 (`CMakeLists.txt`, `bindings/julia/Project.toml`, `bindings/dart/pubspec.yaml`, `bindings/python/pyproject.toml`, `bindings/js/package.json`). Python lockfile's package entry declares 0.13.0 (`bindings/python/uv.lock`). Use `scripts/assert_version.py` as the repository's consistency gate rather than assuming all manifests agree.

## Platform Requirements

**Development:**

- CMake >=3.26, Git, compiler with C++20 calendar support; Ninja for supplied presets (`CMakeLists.txt`, `CMakePresets.json`, `scripts/build_native_linux.sh`).
- Windows MSVC or supported alternate toolchain, Linux compiler, or macOS Xcode toolchain (`cmake/CompilerOptions.cmake`, `bindings/dart/hook/build.dart`).
- macOS deployment target >=13.3 for floating-point `std::to_chars` (`cmake/Platform.cmake`); Dart hook mirrors that floor (`bindings/dart/hook/build.dart`).
- Language-specific runtime and package manager for the binding under development, plus native libraries available through its loader (`bindings/julia/src/c_api.jl`, `bindings/python/src/quiverdb/_loader.py`, `bindings/js/src/loader.ts`, `bindings/dart/lib/src/ffi/library_loader.dart`).

**Production:**

- Published S3/Julia/npm natives: Linux x86_64, Linux aarch64, macOS aarch64, Windows x86_64 (`scripts/ci/native_s3.sh`). Linux floors are glibc 2.17 x86_64 and 2.28 aarch64, with dynamic libstdc++ and GLIBCXX <=3.4.30 (`scripts/build_native_linux.sh`).
- Python wheels target CPython 3.13 Windows amd64 and manylinux x86_64/aarch64; no macOS wheel target is declared (`bindings/python/pyproject.toml`).
- Dart builds from the root source tree using its relative source path and packages native assets into consuming apps (`bindings/dart/hook/build.dart`, `bindings/dart/lib/src/ffi/library_loader.dart`). No Dart publish workflow is detected in `.github/workflows/`.
- C++ installation exports CMake package targets; host applications own execution and database files (`CMakeLists.txt`, `src/CMakeLists.txt`).

---

*Stack analysis: 2026-10-07*
