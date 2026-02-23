# Technology Stack

**Project:** Quiver Python Bindings
**Researched:** 2026-02-23
**Scope:** NEW additions only — C++20 core, CMake, SQLite3, spdlog, sol2, rapidcsv already exist and are not re-researched.

## Confidence Note

Web search and fetch were unavailable during this session. All findings are based on:
- Direct codebase inspection (Julia/Dart binding patterns, existing Python stub, C headers)
- Knowledge through August 2025 cutoff (CFFI 1.17.x, pytest 8.x, uv stable)
- pyproject.toml already present in stub (confirms tooling choices are pre-decided)

Overall confidence: **MEDIUM** — core stack is well-established, specific current versions should be verified against PyPI before pinning.

---

## Recommended Stack

### Core FFI

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| cffi | >=1.17 | Load and call `libquiver_c.dll`/`.so`/`.dylib` | ABI-mode dlopen; no compiler needed at install time; pure-Python install; handles structs, enums, opaque pointers, and void* arrays required by the C API |

**Mode decision: ABI mode (dlopen/dlload), not API mode.**

The C API uses only C constructs that CFFI's ABI mode handles correctly: opaque struct pointers (`quiver_database_t*`, `quiver_element_t*`), fixed-layout value structs (`quiver_database_options_t`, `quiver_csv_export_options_t`), typed enums cast to int, `int64_t*`/`double*`/`char**` output arrays, and `void*` columns for time series. None of these require the compile-time type verification that API mode provides. API mode (which generates a C extension via a build step) would add compiler requirements and build complexity for no benefit given this project's "build from source" constraint (no PyPI distribution).

CFFI ABI mode workflow:
1. Define declarations in a Python string (hand-written or script-generated from headers)
2. `ffi = FFI(); ffi.cdef(declarations); lib = ffi.dlopen("path/to/libquiver_c.dll")`
3. Call `lib.quiver_database_open(...)` directly

### Package Manager / Build Backend

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| uv | latest | Dependency management, venv, test runner | Already in Makefile (`uv sync`, `uv run pytest`); fast, modern, replaces pip+virtualenv+pip-tools |
| hatchling | >=1.25 | Build backend for pyproject.toml | Already in stub's `[build-system]`; correct choice for a pure-Python package |

### Test Framework

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| pytest | >=8.4 | Test runner | Already pinned in stub's dev dependencies at `>=8.4.1`; standard for Python; `tmp_path` fixture handles temp DB files cleanly |

### Linting / Formatting

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| ruff | >=0.12 | Linting and formatting | Already in stub at `>=0.12.2`; replaces black + isort + flake8; configured in `ruff.toml` with `line-length=120`, `target-version="py313"` |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| dotenv (python-dotenv) | >=1.0 | Load env vars in tests | Already in stub dev deps as `dotenv>=0.9.9` — used to configure `QUIVER_LIB_PATH` or similar path env var pointing at build output |

No other Python dependencies are needed. CFFI ships its own `ffi` runtime and does not depend on ctypes, Cython, or cffi-build tools in ABI mode.

---

## Project Structure

The existing stub already defines a flat `src/` layout. The recommended structure:

```
bindings/python/
  pyproject.toml           # package config (rename "margaux" -> "quiver")
  ruff.toml                # already present, keep as-is
  Makefile                 # already present (lint/test targets)
  .python-version          # already present: "3.13"
  src/
    quiver/                # rename from bare src/ to named package
      __init__.py          # public API: Database, Element classes
      _lib.py              # CFFI loader: ffi + lib objects, DLL path resolution
      _declarations.py     # CFFI cdef string: all C API declarations
      database.py          # Database class wrapping C API
      element.py           # Element builder class
      exceptions.py        # QuiverError raised from quiver_get_last_error()
  tests/
    conftest.py            # fixtures: db factory, tmp paths, shared schema refs
    test_database_lifecycle.py
    test_database_create.py
    test_database_read.py
    test_database_update.py
    test_database_delete.py
    test_database_query.py
    test_database_relations.py
    test_database_time_series.py
    test_database_transaction.py
    test_database_csv.py
```

Key structural decisions:
- `_lib.py` centralizes `ffi.dlopen()` so the DLL is loaded once at import time, not per-call.
- `_declarations.py` holds the stripped C declarations (no `#include`, no `__declspec`, no `extern "C"`) as a raw string passed to `ffi.cdef()`.
- Test file names mirror the C++ test organization exactly for parity tracing.

---

## DLL Loading Pattern

CFFI ABI mode requires a path to the compiled `.dll`/`.so`. Three viable resolution strategies, in priority order:

1. **Environment variable** (`QUIVER_LIB_DIR`): Set to `build/bin` (Windows) or `build/lib` (Linux/macOS). Tests set this via `.env` loaded by python-dotenv. This is the correct approach for a "build from source, no PyPI" workflow.

2. **Relative path from package root**: Walk up from `_lib.py` to find `build/bin/libquiver_c.dll`. Works when running tests from the repo root. Fragile if installed outside repo.

3. **`ctypes.util.find_library`**: Searches PATH. Requires the build output directory to be on PATH. The Dart test.bat already does PATH manipulation for the same reason.

Recommended: env var primary, relative path fallback. The `conftest.py` loads `.env` so tests set the variable without polluting the shell.

Windows requires both `libquiver.dll` and `libquiver_c.dll` on PATH or in the same directory (as `libquiver_c.dll` depends on `libquiver.dll`). The same constraint exists for Dart (noted in CLAUDE.md). Handle by loading `libquiver.dll` first via a second `ffi.dlopen()` call, or by ensuring the build dir is on PATH before loading.

---

## CFFI Declarations Generation

CFFI's `cdef()` requires stripped declarations: no preprocessor directives, no `__declspec`, no `extern "C"`, no `#ifdef`. Two approaches:

**Option A (recommended): Hand-write `_declarations.py`**

The C API is fully visible in the Julia `c_api.jl` (generated from the same headers). The declarations are stable and not regenerated often. Hand-writing takes ~30 minutes, produces readable code, and avoids a build dependency on a C preprocessor. The Julia file is a 1:1 reference — every function listed there maps directly to a CFFI declaration. This is the simplest approach and matches the project's "simple solutions over complex abstractions" principle.

**Option B: Script-generate from headers using pcpp or pycparser**

`pcpp` (a pure-Python C preprocessor) can strip `#include`, expand `#ifdef`, and output a clean string for `ffi.cdef()`. Not needed given Option A, but viable if the API grows significantly.

Do NOT use `cffi`'s API mode `ffibuilder.set_source()` — it compiles a C extension at install time, requiring MSVC/GCC at the user's machine. This contradicts the project's build-from-source-binary approach.

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| FFI library | cffi (ABI mode) | ctypes | CFFI has cleaner syntax for structs, better pointer arithmetic, and is what was pre-decided in PROJECT.md |
| FFI library | cffi (ABI mode) | cffi API mode | API mode needs a C compiler at install time; no distribution benefit when packaging is out of scope |
| FFI library | cffi (ABI mode) | pybind11 | pybind11 requires writing C++ wrapper code; we have the C API specifically to avoid that |
| Declarations | Hand-written `_declarations.py` | pcpp/pycparser | Extra dependency, more complexity; Julia c_api.jl is a complete, up-to-date reference |
| Build backend | hatchling | setuptools | hatchling already in stub; setuptools only needed for C extensions (not applicable in ABI mode) |
| Test framework | pytest | unittest | pytest `tmp_path` fixture is cleaner for temp DB files; parametrize works well for typed-variant tests |

---

## Installation

```bash
# Core runtime dependency
uv add cffi

# Dev dependencies (already in stub, verify versions)
uv add --dev pytest>=8.4 ruff>=0.12 python-dotenv>=1.0
```

pyproject.toml additions needed:
```toml
[project]
name = "quiver"           # was "margaux"
dependencies = [
    "cffi>=1.17",
]

[dependency-groups]
dev = [
    "pytest>=8.4",
    "ruff>=0.12",
    "python-dotenv>=1.0",
]
```

---

## What NOT to Add

- **No pybind11, Cython, or SWIG** — the C API exists specifically to avoid C++ binding tools.
- **No cffi API mode** (`ffibuilder.set_source()`) — requires C compiler at install time, no PyPI distribution to justify it.
- **No mypy or pyright** — the binding is thin; type annotations on thin wrappers add maintenance cost without proportional value.
- **No hypothesis or property-based testing** — the C++ layer has its own correctness tests; Python tests verify binding correctness, not C++ logic.
- **No LuaRunner binding** — explicitly out of scope per PROJECT.md.
- **No async wrappers** — SQLite is synchronous; async would be misleading.
- **No packaging (PyPI wheel)** — explicitly out of scope per PROJECT.md.

---

## Sources

- Direct inspection: `bindings/python/pyproject.toml`, `bindings/python/Makefile`, `bindings/python/ruff.toml`, `bindings/python/.python-version`
- Direct inspection: `bindings/julia/src/c_api.jl` (complete C API declaration reference)
- Direct inspection: `include/quiver/c/*.h` (common.h, database.h, element.h, options.h)
- Direct inspection: `.planning/PROJECT.md` (constraints, decisions, out-of-scope list)
- CLAUDE.md: DLL dependency note (libquiver_c.dll depends on libquiver.dll)
- Knowledge cutoff August 2025: CFFI stable series 1.17.x, pytest 8.x, ruff 0.x
- **Verify before pinning**: cffi, pytest, ruff — check PyPI for current patch versions
