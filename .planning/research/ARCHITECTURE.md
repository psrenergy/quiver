# Architecture Patterns

**Domain:** PyPI distribution of Python package with bundled native C/C++ shared libraries (CFFI ABI-mode)
**Researched:** 2026-02-25

## Recommended Architecture

The architecture has three layers: **CMake build** produces native shared libraries, **scikit-build-core** packages them into the wheel alongside Python source, and **cibuildwheel** orchestrates cross-platform CI builds with wheel repair tools bundling transitive native dependencies.

```
Repository Root (CMakeLists.txt)
  |
  +-- src/                      # C++ core
  +-- include/                  # C/C++ headers
  +-- bindings/python/          # Python package source
  |     +-- pyproject.toml      # Build config (scikit-build-core backend)
  |     +-- CMakeLists.txt      # NEW: thin cmake that delegates to root + installs into wheel
  |     +-- src/quiverdb/
  |           +-- __init__.py
  |           +-- _loader.py    # MODIFIED: find bundled libs relative to __file__
  |           +-- _c_api.py
  |           +-- database.py
  |           +-- ...
  |           +-- _libs/        # WHERE native libs land in the installed wheel
  |                 +-- libquiver.{so,dll}
  |                 +-- libquiver_c.{so,dll}
  |
  +-- .github/workflows/
        +-- ci.yml              # Existing CI (unchanged)
        +-- wheels.yml          # NEW: cibuildwheel + publish workflow
```

### Decision: scikit-build-core as build backend (not hatchling + custom hook)

**Confidence: HIGH** (official docs, ecosystem standard)

Use scikit-build-core directly as the PEP 517 build backend. It invokes CMake during `pip install` or `python -m build`, runs the cmake install step, and packages results into the wheel.

**Why not hatchling + scikit-build-core plugin:** The hatchling plugin is marked `experimental = true` in scikit-build-core docs, lacks editable install support, and adds a layer of indirection. Going direct with scikit-build-core is simpler and better supported.

**Why not hatchling + custom build hook:** Writing a custom `hatch_build.py` to shell out to CMake is fragile. scikit-build-core already solves cmake-to-wheel integration with battle-tested code. Do not reinvent it.

**Why not keep hatchling and pre-build libs externally:** This fragments the build into two steps (cmake build, then hatch build) which cibuildwheel cannot orchestrate cleanly. scikit-build-core unifies them into one `pip wheel .` invocation.

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| Root `CMakeLists.txt` | Build `libquiver` + `libquiver_c` shared libs | scikit-build-core invokes this |
| `bindings/python/CMakeLists.txt` | NEW: Install targets into `${SKBUILD_PLATLIB_DIR}/quiverdb/_libs/` | Root CMake (via `add_subdirectory` or standalone build) |
| `bindings/python/pyproject.toml` | Declare scikit-build-core as build backend, configure cmake args | scikit-build-core reads this |
| `_loader.py` | Find and dlopen bundled libs from `_libs/` subdir, fallback to PATH | CFFI, `_c_api.py` |
| `cibuildwheel` (CI) | Build wheels per platform, run repair tools | GitHub Actions, scikit-build-core |
| `auditwheel` / `delvewheel` | Bundle transitive native deps (libc++, MSVC runtime) into wheel | cibuildwheel invokes automatically |
| GitHub Actions `wheels.yml` | Orchestrate build matrix, publish to PyPI | cibuildwheel, `gh-action-pypi-publish` |

### Data Flow

```
1. cibuildwheel starts a build environment (manylinux container or Windows runner)
2. cibuildwheel calls: pip wheel bindings/python/ (or python -m build)
3. scikit-build-core reads bindings/python/pyproject.toml
4. scikit-build-core invokes cmake -B build -S . (from bindings/python/)
   with args: -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_C_API=ON
5. cmake builds libquiver.so + libquiver_c.so
6. cmake install() copies them into ${SKBUILD_PLATLIB_DIR}/quiverdb/_libs/
7. scikit-build-core also copies wheel.packages (src/quiverdb/) into wheel
8. Result: unrepaired wheel with Python code + native libs in quiverdb/_libs/
9. cibuildwheel runs repair tool:
   - Linux: auditwheel repair (bundles glibc deps, sets RPATH, tags manylinux)
   - Windows: delvewheel repair --add-path ... (bundles MSVC runtime if needed)
10. Repaired wheel is output
11. GitHub Actions uploads wheel artifact
12. On tagged release: gh-action-pypi-publish uploads to PyPI
```

## Detailed Component Designs

### 1. `bindings/python/pyproject.toml` (MODIFIED)

Replace hatchling with scikit-build-core as the build backend.

```toml
[project]
name = "quiverdb"
version = "0.5.0"
requires-python = ">=3.13"
dependencies = ["cffi>=2.0.0"]

[build-system]
requires = ["scikit-build-core>=0.11"]
build-backend = "scikit_build_core.build"

[tool.scikit-build]
cmake.build-type = "Release"
cmake.args = ["-DQUIVER_BUILD_C_API=ON", "-DQUIVER_BUILD_TESTS=OFF"]
cmake.source-dir = "../.."
wheel.packages = ["src/quiverdb"]
install.strip = true

[dependency-groups]
dev = [
    "dotenv>=0.9.9",
    "pytest>=8.4.1",
    "ruff>=0.12.2",
]
```

**Key settings:**
- `cmake.source-dir = "../.."` -- points scikit-build-core at the repo root where the main `CMakeLists.txt` lives
- `cmake.args` -- enables C API, disables tests (not needed in wheel builds)
- `wheel.packages = ["src/quiverdb"]` -- tells scikit-build-core where the pure Python package lives
- `install.strip = true` -- strips debug symbols from release binaries (smaller wheels)

**Confidence: HIGH** for scikit-build-core config. MEDIUM for `cmake.source-dir = "../.."` -- verified in scikit-build-core docs that this is supported, but the binding subdir layout is unusual. May need testing.

### 2. `bindings/python/CMakeLists.txt` (NEW -- alternative approach)

**Option A (Preferred): No separate CMakeLists.txt** -- modify the root `CMakeLists.txt` to add an install component for Python:

```cmake
# Add to root CMakeLists.txt or src/CMakeLists.txt, guarded by SKBUILD:
if(DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs  # Windows DLLs go to RUNTIME
    )
endif()
```

When scikit-build-core invokes CMake, it sets `SKBUILD=2`. The install destination is relative to `${CMAKE_INSTALL_PREFIX}`, which scikit-build-core sets to a staging directory that becomes the wheel contents. `quiverdb/_libs` places the libraries inside the package directory in the wheel.

**Option B: Separate CMakeLists.txt in bindings/python/** -- use `cmake.source-dir = "../.."` and still modify root CMakeLists.txt. Same outcome but keeps the binding directory self-contained for pyproject.toml purposes.

**Recommendation: Option A.** Minimal change, no new CMakeLists.txt file needed. The `SKBUILD` guard ensures the install targets only activate during wheel builds, not during normal CMake development builds.

### 3. `_loader.py` (MODIFIED)

The current `_loader.py` walks up directories looking for a `build/` folder. For installed wheels, the libraries live in `quiverdb/_libs/` relative to the package. The new `_loader.py` must:

1. First check for bundled libs in `_libs/` subdirectory (installed wheel case)
2. Fall back to walking up for `build/` directory (development case)
3. Final fallback: system PATH (legacy behavior)

```python
from __future__ import annotations

import sys
from pathlib import Path

from cffi import FFI


def _lib_names() -> tuple[str, str, list[str]]:
    """Return (core_lib_name, c_api_lib_name, subdirs_to_search)."""
    if sys.platform == "win32":
        return "libquiver.dll", "libquiver_c.dll", ["bin", "lib"]
    elif sys.platform == "darwin":
        return "libquiver.dylib", "libquiver_c.dylib", ["lib", "bin"]
    else:
        return "libquiver.so", "libquiver_c.so", ["lib", "bin"]


def _find_bundled_dir() -> Path | None:
    """Check for libraries bundled inside the installed package."""
    libs_dir = Path(__file__).resolve().parent / "_libs"
    _, c_lib_name, _ = _lib_names()
    if libs_dir.is_dir() and (libs_dir / c_lib_name).exists():
        return libs_dir
    return None


def _find_dev_dir() -> Path | None:
    """Walk up from this file to find a build directory (development mode)."""
    _, c_lib_name, subdirs = _lib_names()
    current = Path(__file__).resolve().parent
    while current != current.parent:
        build_dir = current / "build"
        if build_dir.is_dir():
            for subdir in subdirs:
                candidate = build_dir / subdir
                if candidate.is_dir() and (candidate / c_lib_name).exists():
                    return candidate
        current = current.parent
    return None


def load_library(ffi: FFI):
    """Load the Quiver C API shared library.

    Search order:
    1. Bundled libs in _libs/ subdirectory (pip-installed wheel)
    2. Build directory walk-up (development mode)
    3. System PATH fallback
    """
    core_name, c_api_name, _ = _lib_names()

    # 1. Bundled libs (installed wheel)
    lib_dir = _find_bundled_dir()

    # 2. Development build directory
    if lib_dir is None:
        lib_dir = _find_dev_dir()

    if lib_dir is not None:
        if sys.platform == "win32":
            import os
            os.add_dll_directory(str(lib_dir))
        ffi.dlopen(str(lib_dir / core_name))
        return ffi.dlopen(str(lib_dir / c_api_name))

    # 3. System PATH fallback
    try:
        ffi.dlopen(core_name.replace(".dll", "") if sys.platform == "win32" else core_name)
    except Exception:
        pass
    fallback_name = c_api_name.replace(".dll", "") if sys.platform == "win32" else c_api_name
    return ffi.dlopen(fallback_name)
```

**Key change: `os.add_dll_directory()`** on Windows. Starting with Python 3.8, Windows DLL search paths changed. `os.add_dll_directory()` is required for DLLs to find their own dependencies (e.g., `libquiver_c.dll` finding `libquiver.dll` in the same directory). On Linux, `auditwheel` patches RPATH so this is handled automatically.

**Confidence: HIGH** for the search order pattern. MEDIUM for `os.add_dll_directory` -- this is the documented Python approach but needs testing to confirm it works within the CFFI dlopen flow.

### 4. Root `CMakeLists.txt` (MODIFIED -- minimal addition)

Add install targets for the Python wheel, guarded by `SKBUILD`:

```cmake
# At the bottom of src/CMakeLists.txt, after the quiver_c target definition:
if(QUIVER_BUILD_C_API AND DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
endif()
```

`LIBRARY DESTINATION` handles `.so` on Linux and `.dylib` on macOS. `RUNTIME DESTINATION` handles `.dll` on Windows (Windows puts DLLs in the RUNTIME category, not LIBRARY). Both point to the same `quiverdb/_libs` directory inside the wheel.

**Confidence: HIGH** -- standard CMake install semantics, verified in CMake docs.

### 5. GitHub Actions `wheels.yml` (NEW)

```yaml
name: Build Wheels

on:
  push:
    tags: ["v*"]
  workflow_dispatch:  # Manual trigger for testing

jobs:
  build-wheels:
    name: Build wheels (${{ matrix.os }})
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest]

    steps:
      - uses: actions/checkout@v6

      - uses: actions/setup-python@v5
        with:
          python-version: "3.13"

      - name: Install cibuildwheel
        run: pip install cibuildwheel

      - name: Build wheels
        run: cibuildwheel --output-dir wheelhouse bindings/python
        env:
          CIBW_BUILD: "cp313-*"
          CIBW_ARCHS_LINUX: "x86_64"
          CIBW_ARCHS_WINDOWS: "AMD64"
          CIBW_MANYLINUX_X86_64_IMAGE: "manylinux_2_28"
          # Linux: auditwheel runs by default
          # Windows: install + configure delvewheel
          CIBW_BEFORE_BUILD_WINDOWS: "pip install delvewheel"
          CIBW_REPAIR_WHEEL_COMMAND_WINDOWS: >-
            delvewheel repair
            --add-path {project}/build/Release/bin;{project}/build/Release/lib;{project}/build/bin;{project}/build/lib
            -w {dest_dir} {wheel}
          CIBW_TEST_REQUIRES: "pytest"
          CIBW_TEST_COMMAND: "pytest {project}/bindings/python/tests -x"

      - uses: actions/upload-artifact@v6
        with:
          name: wheels-${{ matrix.os }}
          path: wheelhouse/*.whl

  publish:
    name: Publish to PyPI
    needs: build-wheels
    runs-on: ubuntu-latest
    if: startsWith(github.ref, 'refs/tags/v')
    permissions:
      id-token: write  # Required for trusted publishing
    steps:
      - uses: actions/download-artifact@v6
        with:
          pattern: wheels-*
          merge-multiple: true
          path: dist/

      - uses: pypa/gh-action-pypi-publish@release/v1
        with:
          packages-dir: dist/
```

**Key design decisions:**

- **`CIBW_BUILD: "cp313-*"`** -- only build for Python 3.13 (project requires >=3.13)
- **`CIBW_MANYLINUX_X86_64_IMAGE: manylinux_2_28`** -- uses the current default, based on glibc 2.28 (RHEL 8+, Ubuntu 20.04+, Debian 11+)
- **`CIBW_REPAIR_WHEEL_COMMAND_WINDOWS`** with `--add-path` -- tells delvewheel where to find the built DLLs since CFFI dlopen is invisible to static analysis
- **Trusted publishing** -- PyPI supports OIDC-based publishing from GitHub Actions without API tokens. Configure once on PyPI, then `gh-action-pypi-publish` works with zero secrets.
- **`cibuildwheel bindings/python`** -- pass the path to the Python package directory, where pyproject.toml lives

**Confidence: HIGH** for cibuildwheel + GitHub Actions pattern. MEDIUM for the exact `--add-path` paths in delvewheel -- these depend on where scikit-build-core places the CMake build directory, which may need adjustment.

### 6. Linux-specific: auditwheel and RPATH

On Linux, auditwheel automatically:
1. Detects shared library dependencies of extension modules
2. Copies them into `quiverdb.libs/` inside the wheel
3. Patches RPATH so libraries find each other

**Critical caveat for CFFI ABI-mode:** auditwheel detects dependencies via `DT_NEEDED` in ELF headers. CFFI's `ffi.dlopen()` loads libraries at runtime, which auditwheel **cannot detect**. This means auditwheel will NOT automatically bundle `libquiver.so` or `libquiver_c.so`.

**Solution:** The libraries are already placed inside the wheel by the CMake install step (into `quiverdb/_libs/`). auditwheel will then:
- Detect that `libquiver_c.so` has `DT_NEEDED` for `libquiver.so` (static linkage dependency)
- See that `libquiver.so` is already in the wheel and patch RPATH accordingly
- Bundle any transitive system deps (e.g., `libstdc++.so`) that aren't in the manylinux policy
- Relabel the wheel with the appropriate manylinux tag

**Confidence: MEDIUM** -- this flow is correct in theory but the interaction between pre-bundled libs in `_libs/` and auditwheel's repair step needs testing. auditwheel may move them to `quiverdb.libs/` at the wheel root level instead. If so, `_loader.py` needs to also check that path.

### 7. Windows-specific: delvewheel and DLL loading

On Windows, delvewheel:
1. Analyzes PE headers of extension modules for DLL dependencies
2. Copies dependent DLLs into a `.libs` directory in the wheel
3. Patches the wheel's `__init__.py` to call `os.add_dll_directory()`

**Same CFFI caveat:** delvewheel cannot detect `ffi.dlopen()` calls. Must use `--add-path` to tell it where the DLLs are, plus potentially `--include libquiver.dll --include libquiver_c.dll` to force-include them.

**Alternative (simpler):** Skip delvewheel entirely on Windows. The CMake install step already bundles the DLLs into `quiverdb/_libs/`. The updated `_loader.py` uses `os.add_dll_directory()` to make them discoverable. Set `CIBW_REPAIR_WHEEL_COMMAND_WINDOWS = ""` to disable repair.

**Recommendation:** Start with the simpler approach (no delvewheel, rely on CMake install + `_loader.py`). Add delvewheel later only if users report missing MSVC runtime DLLs. The MSVC runtime (`vcruntime140.dll`) is usually already present on Windows systems, and Python itself ships with it.

**Confidence: MEDIUM** -- the no-delvewheel approach is simpler but may miss edge cases where the MSVC redistributable is not installed.

## Patterns to Follow

### Pattern 1: Bundled Library Discovery (relative to `__file__`)

**What:** Locate native libraries relative to the Python package's own directory, not via PATH or environment variables.

**When:** Always in installed packages. This is the only reliable way to find bundled libraries after `pip install`.

**Example:**
```python
libs_dir = Path(__file__).resolve().parent / "_libs"
ffi.dlopen(str(libs_dir / "libquiver_c.so"))
```

**Why:** `__file__` always resolves to the installed location in site-packages. PATH-based discovery breaks in virtual environments, containers, and multi-version installs. `importlib.resources` is an alternative but overly complex for this use case since we know the exact subdirectory structure.

### Pattern 2: SKBUILD guard in CMakeLists.txt

**What:** Conditionally add install targets only when building via scikit-build-core.

**When:** The project has both a standalone CMake build (for development/testing) and a Python wheel build.

**Example:**
```cmake
if(DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
endif()
```

**Why:** Without the guard, `cmake --install` during development would try to install into `quiverdb/_libs/` which makes no sense outside wheel building. The guard keeps the two build modes cleanly separated.

### Pattern 3: Dependency chain loading order

**What:** Load `libquiver` before `libquiver_c` because `libquiver_c` depends on `libquiver`.

**When:** Always. The C API library dynamically links against the core library.

**Example:**
```python
ffi.dlopen(str(lib_dir / "libquiver.so"))      # core first
return ffi.dlopen(str(lib_dir / "libquiver_c.so"))  # then C API
```

**Why:** On Windows especially, if `libquiver.dll` is not already loaded when `libquiver_c.dll` tries to resolve its imports, the load fails. Pre-loading with `ffi.dlopen()` keeps the handle alive in-process. On Linux, RPATH handles this automatically in repaired wheels, but explicit ordering is still good practice.

### Pattern 4: Platform wheel tags (not pure-python)

**What:** The wheel must be tagged as platform-specific (e.g., `cp313-cp313-manylinux_2_28_x86_64` or `cp313-cp313-win_amd64`), not as `py3-none-any`.

**When:** Always -- the package contains native `.so`/`.dll` files.

**Why:** scikit-build-core handles this automatically. A pure-Python wheel (`py3-none-any`) would be installed on any platform, but the bundled native libraries only work on the platform they were compiled for. scikit-build-core detects the compiled artifacts and tags the wheel correctly.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Loading libraries from PATH in installed packages

**What:** Relying on `LD_LIBRARY_PATH`, `PATH`, or system library directories to find `libquiver_c.so`.

**Why bad:** Breaks `pip install` self-containment. Users would need to manually set environment variables or install system packages. Defeats the purpose of bundling.

**Instead:** Always look in `_libs/` first. PATH fallback only for development convenience.

### Anti-Pattern 2: Using hatchling + scikit-build-core plugin

**What:** Keeping hatchling as build backend and using scikit-build-core as a hatchling build hook plugin.

**Why bad:** The hatchling plugin is marked `experimental = true`. It lacks editable install support. It adds indirection without benefit. If scikit-build-core is already doing the heavy lifting, just use it as the backend directly.

**Instead:** Use `build-backend = "scikit_build_core.build"` directly.

### Anti-Pattern 3: Pre-building native libs in CI before cibuildwheel

**What:** Building `libquiver.so` in a separate CI step, then passing it to cibuildwheel.

**Why bad:** cibuildwheel runs builds inside isolated environments (manylinux Docker containers on Linux). Libraries built outside the container use the host's glibc/libstdc++, making them incompatible with the manylinux container. The whole point of manylinux is to build inside a controlled environment with old-enough glibc.

**Instead:** Let scikit-build-core invoke CMake inside the cibuildwheel build environment. The CMake build happens inside the manylinux container, ensuring binary compatibility.

### Anti-Pattern 4: Shipping sdist (source distribution)

**What:** Publishing a source distribution that requires users to have CMake, a C++ compiler, and all dependencies to build from source.

**Why bad:** The project uses C++20 with multiple FetchContent dependencies (SQLite, spdlog, lua, sol2, etc.). Building from source is complex and fragile. CFFI ABI-mode specifically avoids needing a compiler at install time.

**Instead:** Publish only platform-specific wheels. Users on unsupported platforms can build from source manually, but `pip install` should always resolve to a pre-built wheel.

### Anti-Pattern 5: Static linking everything into one library

**What:** Linking `libquiver` statically into `libquiver_c` to produce a single shared library.

**Why bad:** Changes the CMake build architecture. Other bindings (Julia, Dart, Lua) depend on the current two-library structure. Static linking may cause symbol conflicts with SQLite (which could be loaded by other Python packages).

**Instead:** Keep two shared libraries. The `_loader.py` dependency-chain pattern handles loading order correctly.

## Wheel Structure (installed layout)

```
site-packages/
  quiverdb/
    __init__.py
    _c_api.py
    _loader.py
    _declarations.py
    _helpers.py
    database.py
    element.py
    exceptions.py
    metadata.py
    py.typed
    _libs/
      libquiver.so          (Linux)
      libquiver_c.so        (Linux)
      libquiver.dll         (Windows)
      libquiver_c.dll       (Windows)
  quiverdb-0.5.0.dist-info/
    METADATA
    WHEEL
    RECORD
    top_level.txt
```

After `auditwheel repair` on Linux, transitive deps may also appear:
```
  quiverdb.libs/            (auditwheel places transitive deps here)
    libstdc++-abcdef12.so.6
    libgcc_s-12345678.so.1
```

auditwheel patches the RPATH of `libquiver.so` and `libquiver_c.so` to find libraries in `../quiverdb.libs/`.

## Scalability Considerations

| Concern | Current (2 platforms) | Future (3+ platforms) | Notes |
|---------|----------------------|----------------------|-------|
| CI build time | ~10 min (2 platform x 1 Python) | ~15 min (add macOS) | Each platform builds independently in parallel |
| Wheel size | ~5-10 MB per platform | Same | Native libs + SQLite are small |
| Python versions | cp313 only | Add cp314 when released | Just add to CIBW_BUILD list |
| Architecture | x86_64 only | Add aarch64/arm64 | cibuildwheel supports cross-compilation for Linux aarch64 via QEMU |
| Dependency management | FetchContent downloads at build time | Cache with GitHub Actions cache | CMake FetchContent can be slow; consider caching `_deps/` |

## New vs Modified Components Summary

| Component | Status | What Changes |
|-----------|--------|-------------|
| `bindings/python/pyproject.toml` | **MODIFIED** | Replace hatchling with scikit-build-core backend |
| `bindings/python/src/quiverdb/_loader.py` | **MODIFIED** | Add `_libs/` discovery, keep dev fallback |
| `src/CMakeLists.txt` | **MODIFIED** | Add SKBUILD-guarded install targets |
| `.github/workflows/wheels.yml` | **NEW** | cibuildwheel + PyPI publish workflow |
| `bindings/python/src/quiverdb/_libs/` | **NEW** (build artifact) | Directory where native libs are installed in wheel |

## Build Order and Dependencies

```
1. scikit-build-core reads pyproject.toml
2. scikit-build-core invokes cmake configure (downloads FetchContent deps)
3. cmake builds libquiver (core C++ library)
4. cmake builds libquiver_c (C API, links against libquiver)
5. cmake install copies libquiver + libquiver_c into staging/quiverdb/_libs/
6. scikit-build-core copies src/quiverdb/*.py into staging/quiverdb/
7. scikit-build-core creates wheel from staging directory
8. cibuildwheel invokes repair tool on the wheel
9. Repaired wheel is the final artifact
```

Steps 2-4 happen inside the cibuildwheel build environment (manylinux container on Linux, native runner on Windows). This ensures binary compatibility.

## Sources

- [scikit-build-core documentation](https://scikit-build-core.readthedocs.io/en/stable/) -- build backend configuration, SKBUILD variables, wheel packaging
- [scikit-build-core CMakeLists authoring](https://scikit-build-core.readthedocs.io/en/latest/guide/cmakelists.html) -- SKBUILD_PLATLIB_DIR, install destinations
- [scikit-build-core dynamic linking guide](https://scikit-build-core.readthedocs.io/en/stable/guide/dynamic_link.html) -- auditwheel/delocate/delvewheel integration
- [scikit-build-core hatchling plugin](https://scikit-build-core.readthedocs.io/en/stable/plugins/hatchling.html) -- experimental status, why not to use it
- [scikit-build-core issue #966](https://github.com/scikit-build/scikit-build-core/issues/966) -- multi-component project structure
- [scikit-build-core issue #374](https://github.com/scikit-build/scikit-build-core/issues/374) -- shared library install destinations
- [cibuildwheel documentation](https://cibuildwheel.pypa.io/en/stable/) -- CI wheel building
- [cibuildwheel options](https://cibuildwheel.pypa.io/en/stable/options/) -- before-all, repair-wheel-command, environment
- [cibuildwheel tips and tricks](https://cibuildwheel.pypa.io/en/stable/faq/) -- CMake integration, native deps
- [delvewheel](https://github.com/adang1345/delvewheel) -- Windows wheel repair, DLL bundling, CFFI limitations
- [auditwheel](https://github.com/pypa/auditwheel) -- Linux wheel repair, manylinux compliance, CFFI limitations
- [CFFI documentation](https://cffi.readthedocs.io/en/stable/overview.html) -- ABI mode, dlopen behavior
- [gh-action-pypi-publish](https://github.com/pypa/gh-action-pypi-publish) -- trusted publishing from GitHub Actions

### Confidence Levels

| Finding | Confidence | Basis |
|---------|------------|-------|
| scikit-build-core as build backend | HIGH | Official docs, established pattern for CMake+Python |
| SKBUILD guard for install targets | HIGH | Official docs, SKBUILD variable documented |
| `_libs/` subdirectory for bundled libs | HIGH | Standard pattern, CMake install semantics |
| `_loader.py` relative-path discovery | HIGH | Python `__file__` semantics are well-defined |
| cibuildwheel workflow structure | HIGH | Official docs, widely used pattern |
| auditwheel interaction with pre-bundled CFFI libs | MEDIUM | Correct in theory, needs testing |
| delvewheel `--add-path` for CFFI dlopen | MEDIUM | Documented flag, but CFFI interaction untested |
| `cmake.source-dir = "../.."` from binding subdir | MEDIUM | Documented feature, unusual layout needs testing |
| Trusted publishing (OIDC) on PyPI | HIGH | PyPI official feature, documented workflow |
| No sdist needed | HIGH | CFFI ABI-mode + pre-built wheels = no compiler needed |
