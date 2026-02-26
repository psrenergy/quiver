# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Provides schema-driven database operations with typed scalars, vectors, sets, time series, parameterized queries, and CSV export/import. The Python binding (`quiverdb`) uses CFFI ABI-mode.

## Core Value

A single `pip install quiverdb` gives Python users a fully self-contained SQLite wrapper with no system dependencies — native libraries bundled inside the wheel.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- ✓ C++ core library with full CRUD, metadata, query, time series, CSV, transactions
- ✓ C API layer wrapping all public C++ methods (FFI-safe)
- ✓ Julia bindings (Quiver.jl)
- ✓ Dart bindings (quiver)
- ✓ Python bindings (quiverdb) — CFFI ABI-mode, all operations bound, tests passing
- ✓ Lua scripting support (LuaRunner)
- ✓ Schema validation and type checking

### Active

<!-- Current scope: PyPI distribution milestone. -->

- [ ] Native libraries (libquiver, libquiver_c) bundled inside platform-specific wheels
- [ ] Windows x64 wheel builds and works via `pip install`
- [ ] Linux x64 (manylinux) wheel builds and works via `pip install`
- [ ] GitHub Actions CI builds wheels on push/release
- [ ] GitHub Actions CI publishes to PyPI on tagged release
- [ ] `pip install quiverdb` is self-contained — no PATH setup or system deps

### Out of Scope

<!-- Explicit boundaries. -->

- macOS wheels — defer to future milestone, two platforms first
- Source distributions (sdist) — wheels-only for native-bundled package
- TestPyPI staging — publish directly to real PyPI
- Python < 3.13 support — current binding requires 3.13+

## Context

- Python binding already exists and tests pass locally (`bindings/python/`)
- CFFI ABI-mode means no compiler needed at install time — just load the shared library
- Current `_loader.py` loads DLLs from PATH at runtime; must change to find bundled libs
- `libquiver_c` depends on `libquiver` — both must be bundled and loaded in correct order
- Build uses CMake + hatchling; wheel building needs to compile C++ then package artifacts
- `cibuildwheel` is the standard tool for cross-platform wheel CI

## Constraints

- **Build system**: CMake for C++ compilation, hatchling for Python packaging
- **CFFI mode**: ABI-mode (runtime dlopen), not API-mode (compile-time)
- **Platforms**: Windows x64 (MSVC) and Linux x64 (GCC/Clang)
- **Python**: >=3.13 (current requirement)
- **CI**: GitHub Actions

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| CFFI ABI-mode | No compiler at install time, simpler user experience | ✓ Good |
| Bundle native libs in wheel | `pip install` must be self-contained | — Pending |
| cibuildwheel for CI | Industry standard for cross-platform wheel building | — Pending |
| Windows + Linux only | Ship what we can test, macOS later | — Pending |

---
*Last updated: 2026-02-25 after milestone v0.5 PyPI started*
