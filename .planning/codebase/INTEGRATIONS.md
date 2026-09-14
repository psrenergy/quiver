# External Integrations

**Analysis Date:** 2026-09-14

## Overview

Quiver is a library with no external APIs, webhooks, or cloud services. Integration surface consists of:
1. **FFI boundary** — Five language bindings consume the C API via foreign-function interfaces
2. **Package registries** — Bindings are published to language-specific repositories
3. **Vendored dependencies** — All C++ dependencies fetched and built with the project
4. **CI/CD infrastructure** — GitHub Actions for build, test, and release automation

## FFI Boundary & Language Bindings

The core C API (`include/quiver/c/`) is the central integration point. All bindings call the C library (`libquiver_c.so`/`.dylib`/`.dll`) via FFI; the shared C++ library (`libquiver`) is a private implementation detail.

**Binding Integration Points:**
- **Julia** (`bindings/julia/`): Clang.jl-based generator → `src/c_api.jl` (GENERATED from C headers) → ccall via FFI
- **Dart** (`bindings/dart/`): ffigen (code_assets) → `lib/src/ffi/bindings.dart` (GENERATED) → dynamic library loading via ffi package
- **Python** (`bindings/python/`): Hand-written CFFI declarations in `src/quiverdb/_c_api.py` → ABI-mode FFI (no compilation at install)
- **JavaScript/Bun** (`bindings/js/`): Hand-written symbol table in `src/loader.ts` → `bun:ffi` dynamic loading
- **Lua** (`src/lua_runner.cpp`): sol2 direct C++ class bindings (not FFI) → embedded scripting sandboxed to database directory

**FFI Libraries Location:**
- Development: `build/bin/libquiver.so`/`.dylib`/`.dll` and `build/bin/libquiver_c.so`/`.dylib`/`.dll`
- Julia artifacts: Artifacts.jl resolution → platform-specific published natives on S3
- Dart native-assets: `.dart_tool/hooks_runner/shared/quiverdb/build/` from hook output
- Python wheel: `quiverdb/_libs/libquiver*.so`/`.dll` installed alongside the package
- JS npm: `libs/{os}-{arch}/libquiver*` bundled in the package
- Lua: in-process (no separate library; sol2 links directly to libquiver)

## Package Registries & Publishing

**npm (JavaScript/TypeScript):**
- Package: `quiverdb` on npmjs.com
- Publishing: GitHub Actions `publish-js.yml` (workflow_dispatch)
- Native assets: `libs/linux-x86_64/`, `libs/macos-aarch64/`, `libs/windows-x86_64/` bundled in tarball
- OIDC trusted publishing: `publishConfig.provenance: true` in `package.json`
- Hosting: github.com/psrenergy/quiver (monorepo mirror, bindings/js published directly)

**PyPI (Python):**
- Package: `quiverdb` on pypi.org
- Publishing: GitHub Actions `publish-python.yml` (cibuildwheel matrix: cp313-win_amd64, cp313-manylinux_x86_64)
- Wheel building: scikit-build-core + CMake, C API compiled at wheel-build time
- OIDC trusted publishing: configured in PyPI project settings
- Local testing: `scripts/validate_wheel.py`, `scripts/test-wheel.bat`

**pub.dev (Dart):**
- Package: `quiverdb` on pub.dev
- Publishing: GitHub Actions `publish-js.yml` after version tag
- Native assets: Hook-driven native build via native_toolchain_cmake (not pre-built binaries)
- Hook: `bindings/dart/hook/build.dart` compiles C++ at package install time
- Version check: Dart hook sets `QUIVER_UNVERSIONED_SHARED=ON` for asset scanning compatibility

**Julia Registry:**
- Package: `Quiver.jl` published to Julia registry
- Publishing: GitHub Actions `publish-julia.yml` creates/updates mirror PR in psrenergy/Quiver.jl
- Distribution: Generated mirror (this repo is canonical, mirror auto-syncs before every release)
- Artifacts: Published natives on S3 (resolved via Artifacts.jl at runtime)
- UUID: `cdbb3f72-2527-4dbd-9d0e-93533a5519ac` in `bindings/julia/Project.toml`

## Vendored Dependencies & Build-Time Integration

**All C++ dependencies are vendored via CMake FetchContent** — no system library requirements:

| Dependency | Version | Source | Used By |
|------------|---------|--------|---------|
| sqlite3 | 3.50.2 | github.com/sjinks/sqlite3-cmake | C++ core |
| toml++ | 3.4.0 | github.com/marzer/tomlplusplus | Binary metadata serialization |
| spdlog | 1.17.0 | github.com/gabime/spdlog | Logging infrastructure |
| lua | 5.4.8 | gitlab.com/codelibre/lua/lua-cmake | Lua runtime (embedded scripting) |
| sol2 | 3.5.0 | github.com/ThePhD/sol2 | C++/Lua bindings |
| rapidcsv | 8.92 | github.com/d99kris/rapidcsv | CSV export/import |
| argparse | 3.2 | github.com/p-ranav/argparse | CLI argument parsing |
| googletest | 1.17.0 | github.com/google/googletest | C++ unit tests (dev only) |

**No external API calls during build.** All dependencies resolved offline after initial clone.

## CI/CD Infrastructure

**GitHub Actions Workflows** (`.github/workflows/`):

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci.yml` | push/PR to master | Matrix build (ubuntu/windows/macos × Release/Debug) + ctest; coverage upload to Codecov (flags: cpp, julia, dart, python); clang-format check, actionlint, bun-test matrix |
| `bump-version.yml` | workflow_dispatch | Version bump (major/minor/patch) via `scripts/assert_version.py` → PR with five manifests rewritten |
| `publish.yml` | workflow_dispatch | Release orchestrator: version assert → dispatch S3/Julia/Python/JS publishes → create tag + GitHub release |
| `publish-s3.yml` | workflow_dispatch | Native library build (linux-x86_64, macos-aarch64, windows-x86_64) → S3 staging |
| `publish-julia.yml` | workflow_dispatch | Mirror `bindings/julia` into psrenergy/Quiver.jl repository |
| `publish-python.yml` | push/PR/workflow_dispatch | cibuildwheel Python package build → PyPI publish (workflow_dispatch only) |
| `publish-js.yml` | workflow_dispatch | npm publish with bundled natives (workflow_dispatch only) |

**Composite Actions** (`.github/actions/`):
- `build-cpp` — Configure/build C++/C API with FetchContent source cache (toolchain-fingerprint-aware)
- Used in ci.yml for all OSes; publish-s3.yml uses only for macOS/Windows (Linux builds in manylinux Docker)

**Linux Native Builds:**
- Container: `manylinux2014` image (CentOS 7 → glibc 2.17, GCC 11, GLIBCXX 3.4.29)
- Script: `scripts/build_native_linux.sh` (runs via `docker run`, not job-level `container:`)
- Rationale: glibc 2.17 floor matches Julia's floor; GCC 11 minimum for C++20 features

**Release Flow:**
1. `bump-version.yml` (manual dispatch) → version PR
2. Merge bump PR to master
3. `publish.yml` (manual dispatch) → assert version tag absent → `publish-s3.yml` + S3 staging → create tag → `publish-julia.yml`/`publish-python.yml`/`publish-js.yml` in parallel on tag
4. Each binding-publish job validates OIDC trusted-publishing credentials

## Development Integrations

**Pre-commit Hook Configuration:**
- `.pre-commit-config.yaml`: clang-format, cppcheck, cmake-format, trailing-whitespace, line-ending checks
- Run: `pre-commit run --all-files` (local or CI gate)

**Codecov Integration:**
- Coverage upload on CI via GitHub Actions
- Flags: `cpp`, `julia`, `dart`, `python` (separate coverage per language)
- Config: `codecov.yml` in repo root

**Version Management:**
- Single source: `CMakeLists.txt` project() VERSION
- Sync check: `scripts/assert_version.py` (no-arg reads version, `bump` writes five manifests)
- Consumed by: GitHub Actions publish workflows via `$(python3 scripts/assert_version.py)`

## Data Storage & File I/O

**Persistent Storage:**
- SQLite databases (`.db` files) — opened via C API, read/written by core
- Optional binary files (`.qvr`) for time-series data — I/O via C++ BinaryFile class
- Optional CSV export/import — I/O via C++ CSVConverter class

**Lua Sandboxing:**
- File operations scoped to database directory only (`src/lua_runner.cpp`, `resolve_sandboxed_path`)
- In-memory databases (`:memory:`) reject all file operations
- Relative paths resolved against database file directory
- Escapes checked via `weakly_canonical` with strict containment

## No External Service Integrations

This codebase has **no**:
- Remote API calls at runtime
- Cloud service dependencies (AWS, Azure, GCP)
- Third-party authentication (OAuth, API keys in secrets)
- Webhooks or callbacks to external systems
- Database backends other than SQLite
- Network I/O (except CI/CD artifact staging to S3)

---

*Integration audit: 2026-09-14*
