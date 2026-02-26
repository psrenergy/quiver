# Phase 2: Loader Rewrite - Research

**Researched:** 2026-02-25
**Domain:** Python native library loading (CFFI ABI-mode, DLL/SO discovery, wheel packaging)
**Confidence:** HIGH

## Summary

Phase 2 rewrites `bindings/python/src/quiverdb/_loader.py` to discover and load bundled native libraries (`libquiver` + `libquiver_c`) from the `_libs/` subdirectory inside an installed wheel, with fallback to development-mode PATH discovery. The scope is one file rewrite plus a small CMake RPATH addition for Linux.

The key insight simplifying this phase: **CFFI >= 1.17 on Windows** already handles transitive DLL resolution when `ffi.dlopen()` receives a path containing slashes. It passes `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` to `LoadLibraryEx()`, so `libquiver_c.dll` automatically finds `libquiver.dll` in the same `_libs/` directory. This means `os.add_dll_directory()` is needed only as a belt-and-suspenders safety measure (and to satisfy LOAD-03 explicitly), not as the primary resolution mechanism. On Linux, the same-directory resolution requires either pre-loading `libquiver.so` first (current approach, works with absolute paths) or setting `RPATH=$ORIGIN` on `libquiver_c.so` during wheel builds.

**Primary recommendation:** Rewrite `_loader.py` with a two-step fallback chain (bundled `_libs/` first, then PATH-based dev mode), using absolute paths for `ffi.dlopen()`. Add `INSTALL_RPATH $ORIGIN` to `libquiver_c` for SKBUILD builds in CMake.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Claude has full discretion on the dev-mode discovery strategy (walk-up, PATH-only, or hybrid)
- No backwards compatibility required -- WIP project, breaking changes acceptable
- User always runs Python tests via `test.bat` (which sets up PATH), so dev workflow convenience through the loader is not critical
- Silent on success -- no logging when libraries load correctly
- Use plain `RuntimeError` (no custom exception class)
- Single error message covering both wheel and dev scenarios (no context-aware branching)
- Error must name the specific library that failed to load (e.g., "Found libquiver but libquiver_c is missing")
- Error must include the actual paths that were searched (e.g., "Searched: /path/to/_libs/, PATH")
- Expose internal packaging details (mention `_libs/` directory by name) -- transparency over abstraction
- Bundled `_libs/` discovery first -- this is the primary path for installed wheels
- If bundled libs are found but fail to load (corrupt, wrong arch), fail immediately with error -- do NOT fall through to dev mode
- If `_libs/` does not exist or is empty, fall through to dev-mode discovery
- Expose a module-level `_load_source` attribute (e.g., `'bundled'` or `'development'`) indicating which strategy succeeded
- Claude has discretion on standalone diagnostic mode (`python -m quiverdb._loader`) -- use best practice
- Strip all macOS/darwin code paths -- only Windows and Linux supported
- Assume Python 3.13+ -- no version guards for `os.add_dll_directory()`
- Windows: use `os.add_dll_directory()` for the bundled lib directory so `libquiver_c.dll` resolves its `libquiver.dll` dependency
- Linux: Claude has discretion on rpath-only vs explicit `LD_LIBRARY_PATH` -- use standard practice for Python native wheels
- Derive library file extensions at runtime (from `sys.platform` or similar), not hardcoded per-platform

### Claude's Discretion
- Dev-mode fallback strategy (walk-up vs PATH-only vs hybrid)
- Fallback chain depth (two-step vs three-step)
- Linux dependency resolution approach (rpath vs explicit)
- Whether to include standalone diagnostic mode
- Internal code structure and helper functions

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LOAD-01 | `_loader.py` discovers bundled native libs from `_libs/` subdirectory relative to package | Bundled discovery via `Path(__file__).resolve().parent / "_libs/"` with absolute-path `ffi.dlopen()`. CFFI docs confirm absolute paths work on all platforms. |
| LOAD-02 | `_loader.py` falls back to development PATH discovery when bundled libs not found | When `_libs/` does not exist or is empty, fall through to `ffi.dlopen("libquiver_c")` (bare name triggers PATH/LD_LIBRARY_PATH search). Pre-load `libquiver` first with exception swallowed. |
| LOAD-03 | Windows `os.add_dll_directory()` registers bundled lib directory for transitive DLL resolution | Call `os.add_dll_directory(str(libs_dir))` before `ffi.dlopen()` on Windows. CFFI >= 1.17 also handles this automatically via `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` when path contains slashes, but explicit registration satisfies the requirement unambiguously. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | >= 2.0.0 | ABI-mode foreign function interface | Already a project dependency. `ffi.dlopen()` with absolute paths is the standard CFFI pattern for loading pre-built shared libraries. |
| pathlib | stdlib | File path manipulation | `Path(__file__).resolve().parent` is the standard Python pattern for locating files relative to a module. |
| os | stdlib | `os.add_dll_directory()` (Windows) | Python 3.8+ API for registering DLL search directories. Required by LOAD-03. |
| sys | stdlib | `sys.platform` for platform detection | Standard approach to derive platform-specific library extensions at runtime. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| CMake INSTALL_RPATH | N/A | Set `$ORIGIN` RPATH on `libquiver_c.so` for Linux wheel builds | During SKBUILD builds only -- ensures `libquiver.so` is found in same directory as `libquiver_c.so` by the Linux dynamic linker. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `os.add_dll_directory()` | Rely solely on CFFI 1.17+ `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` | CFFI handles it automatically when path has slashes, but explicit `os.add_dll_directory()` is more transparent and satisfies LOAD-03 unambiguously |
| RPATH `$ORIGIN` on Linux | Pre-load `libquiver.so` before `libquiver_c.so` in Python | Both work. RPATH is cleaner (OS-level resolution), pre-load is more explicit (Python-level). Recommend both: RPATH as primary, pre-load as belt-and-suspenders. |
| `ctypes.util.find_library()` for dev mode | Direct `ffi.dlopen("libquiver_c")` bare name | `find_library()` adds complexity with no benefit -- `ffi.dlopen()` with bare names already searches PATH/LD_LIBRARY_PATH via the system `dlopen()` / `LoadLibrary()` |

## Architecture Patterns

### Recommended File Structure (after rewrite)
```
bindings/python/src/quiverdb/
  _loader.py          # REWRITTEN - bundled-first discovery + dev fallback
  _c_api.py           # UNCHANGED - calls _loader.load_library(ffi)
  __init__.py          # UNCHANGED
  ...
```

The wheel layout (after install):
```
site-packages/quiverdb/
  _libs/
    libquiver.dll / libquiver.so
    libquiver_c.dll / libquiver_c.so
  _loader.py
  _c_api.py
  ...
```

### Pattern 1: Bundled Library Discovery
**What:** Locate `_libs/` relative to `_loader.py` using `Path(__file__)`, check for existence of expected library files, load with absolute paths.
**When to use:** Primary path -- when package is installed from a wheel.
**Example:**
```python
# Source: CFFI docs (https://cffi.readthedocs.io/en/latest/cdef.html)
# + scikit-build-core dynamic linking guide
_PACKAGE_DIR = Path(__file__).resolve().parent
_LIBS_DIR = _PACKAGE_DIR / "_libs"

# Platform-specific library names
if sys.platform == "win32":
    _EXT = ".dll"
else:
    _EXT = ".so"

_LIB_CORE = f"libquiver{_EXT}"
_LIB_C_API = f"libquiver_c{_EXT}"
```

### Pattern 2: Fail-Fast on Corrupt Bundled Libs
**What:** If `_libs/` exists and contains the expected files but `ffi.dlopen()` raises, fail immediately with a diagnostic error. Do NOT fall through to dev mode.
**When to use:** Prevents silent use of a different (possibly incompatible) library version from PATH.
**Example:**
```python
libs_dir = _LIBS_DIR
core_path = libs_dir / _LIB_CORE
c_api_path = libs_dir / _LIB_C_API

if libs_dir.is_dir() and c_api_path.exists():
    # Bundled libs found -- attempt load, fail-fast on error
    try:
        if sys.platform == "win32":
            os.add_dll_directory(str(libs_dir))
        ffi.dlopen(str(core_path))       # pre-load dependency
        lib = ffi.dlopen(str(c_api_path))
        _load_source = "bundled"
        return lib
    except OSError as e:
        raise RuntimeError(
            f"Cannot load bundled native libraries: {e}. "
            f"Searched: {libs_dir}"
        ) from None
```

### Pattern 3: Dev-Mode Fallback (Bare Name)
**What:** When `_libs/` does not exist or is empty, load by bare library name, which triggers system PATH / LD_LIBRARY_PATH search.
**When to use:** Development mode. `test.bat` prepends `build/bin/` to PATH.
**Example:**
```python
# _libs/ not found -- try system PATH (dev mode)
try:
    ffi.dlopen("libquiver" if sys.platform == "win32" else "libquiver.so")
except Exception:
    pass  # dependency may be statically linked or already loaded

try:
    lib = ffi.dlopen(
        "libquiver_c" if sys.platform == "win32" else "libquiver_c.so"
    )
    _load_source = "development"
    return lib
except OSError as e:
    raise RuntimeError(
        f"Cannot load native libraries. "
        f"Searched: {_LIBS_DIR} (not found), system PATH. "
        f"Missing: libquiver_c{_EXT}"
    ) from None
```

### Pattern 4: Linux RPATH for Wheel Builds
**What:** Set `INSTALL_RPATH` to `$ORIGIN` on `libquiver_c` when building under SKBUILD, so the Linux dynamic linker finds `libquiver.so` in the same directory.
**When to use:** CMake configuration for wheel builds only.
**Example:**
```cmake
# In root CMakeLists.txt, inside the SKBUILD install block
if(DEFINED SKBUILD AND NOT WIN32)
    set_target_properties(quiver_c PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()
```

### Anti-Patterns to Avoid
- **Walking up directories to find `build/`:** The current `_loader.py` walks up from `__file__` to find a `build/` directory. This is fragile (breaks if package is installed in an unexpected location) and unnecessary for wheel installs. The rewrite should NOT walk up for bundled mode -- only use `_libs/` relative to `__file__`.
- **Swallowing OSError on bundled path:** If bundled libs exist but fail to load, the error indicates a real problem (wrong arch, corrupt file, missing system dependency). Falling through to dev mode would mask this.
- **Hardcoded platform extensions:** Use `sys.platform` to derive `.dll` vs `.so` at runtime, not separate per-platform blocks.
- **Using `os.environ['PATH']` manipulation:** Modifying PATH is a global side effect. Use `os.add_dll_directory()` on Windows (process-scoped, removable) and RPATH on Linux (binary-level, zero side effects).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Windows DLL search path registration | Manual `ctypes.windll.kernel32.SetDllDirectoryW()` or PATH manipulation | `os.add_dll_directory()` | Standard Python API since 3.8, returns a handle that can be closed, no global side effects |
| Linux same-directory shared lib resolution | `LD_LIBRARY_PATH` manipulation or `ctypes.cdll.LoadLibrary()` pre-loads | CMake `INSTALL_RPATH "$ORIGIN"` | OS-level, zero Python-side code needed, standard practice per scikit-build-core docs |
| Library file extension mapping | Dict/map of platform to extension | `".dll" if sys.platform == "win32" else ".so"` | Two platforms only (macOS stripped), no mapping needed |
| Custom exception classes for load errors | `class LoaderError(Exception)` | `RuntimeError` with diagnostic message | User decision: plain RuntimeError, single error message |

**Key insight:** The loader is small enough that the entire rewrite is ~60-80 lines of straightforward Python. There is nothing here complex enough to warrant external libraries or abstractions beyond what the stdlib provides.

## Common Pitfalls

### Pitfall 1: CFFI dlopen Without Path Separator on Windows
**What goes wrong:** `ffi.dlopen("libquiver_c")` on Windows works because it triggers `LoadLibrary()` with system search. But `ffi.dlopen("libquiver_c.dll")` also works. The inconsistency is that bare names without slashes use a different code path than paths with slashes.
**Why it happens:** CFFI >= 1.17 detects whether the filename contains `/` or `\`. If it does, it uses `LoadLibraryEx()` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`. If it doesn't, it uses plain `LoadLibrary()`.
**How to avoid:** Always use absolute paths (from `Path().resolve()`) for bundled mode. Use bare names (no slashes) for dev-mode fallback.
**Warning signs:** Works in dev mode but fails from installed wheel, or vice versa.

### Pitfall 2: Linux Missing RPATH on libquiver_c.so
**What goes wrong:** `ffi.dlopen("/path/to/_libs/libquiver_c.so")` loads the file, but `libquiver_c.so` has a `DT_NEEDED` entry for `libquiver.so`. Without RPATH, the dynamic linker searches only default paths (`/lib`, `/usr/lib`, `ld.so.cache`), not `_libs/`.
**Why it happens:** CMake's default `INSTALL_RPATH` is `$ORIGIN/../lib` (from `cmake/Platform.cmake`), which doesn't match the wheel layout where both libs are in the same directory.
**How to avoid:** Set `INSTALL_RPATH "$ORIGIN"` on `libquiver_c` for SKBUILD builds. Also pre-load `libquiver.so` with `ffi.dlopen()` as a second safety net.
**Warning signs:** Works on Windows but fails on Linux with "cannot open shared object file: No such file or directory".

### Pitfall 3: _libs/ Directory Exists but Is Empty
**What goes wrong:** Checking `_libs_dir.is_dir()` alone is not sufficient to determine bundled mode. An empty `_libs/` directory could exist from a failed build or manual interference.
**Why it happens:** Directory existence does not imply library presence.
**How to avoid:** Check for the specific C API library file (`libquiver_c.dll` or `libquiver_c.so`) inside `_libs/`, not just directory existence.
**Warning signs:** "Cannot load bundled native libraries" error when the user expects dev-mode fallback.

### Pitfall 4: Forgetting to Pre-load libquiver Before libquiver_c
**What goes wrong:** `libquiver_c` depends on `libquiver`. On Linux without RPATH, or in dev mode on any platform, loading `libquiver_c` first fails because its dependency isn't resolved.
**Why it happens:** CFFI's `ffi.dlopen()` is equivalent to the system `dlopen()` -- it doesn't automatically search for transitive dependencies in the same directory (on Linux without RPATH).
**How to avoid:** Always load `libquiver` first, then `libquiver_c`. For bundled mode, RPATH handles this at the OS level, but the pre-load is harmless and provides a safety net.
**Warning signs:** OSError mentioning `libquiver.so` when trying to load `libquiver_c.so`.

### Pitfall 5: os.add_dll_directory Handle Lifetime
**What goes wrong:** `os.add_dll_directory()` returns a handle. If that handle is garbage collected or explicitly closed, the directory is removed from the search path.
**Why it happens:** The API is designed as a context manager for temporary additions.
**How to avoid:** Store the returned handle at module level so it persists for the process lifetime. Do NOT use it as a context manager (with-statement).
**Warning signs:** Libraries load initially but fail on subsequent `ffi.dlopen()` calls or when CFFI resolves symbols lazily.

## Code Examples

### Complete Loader Skeleton
```python
# Source: Synthesized from CFFI docs, scikit-build-core dynamic linking guide,
# and project CONTEXT.md decisions

from __future__ import annotations

import sys
from pathlib import Path
from cffi import FFI

_PACKAGE_DIR = Path(__file__).resolve().parent
_LIBS_DIR = _PACKAGE_DIR / "_libs"
_EXT = ".dll" if sys.platform == "win32" else ".so"
_LIB_CORE = f"libquiver{_EXT}"
_LIB_C_API = f"libquiver_c{_EXT}"

# Set after successful load
_load_source: str = ""

# Windows DLL directory handle (kept alive for process lifetime)
_dll_dir_handle = None


def load_library(ffi: FFI):
    """Load the Quiver C API shared library.

    Strategy:
    1. Bundled: Look for _libs/ subdirectory (wheel install)
    2. Development: Fall back to system PATH (dev mode)
    """
    global _load_source, _dll_dir_handle

    # --- Bundled mode ---
    c_api_path = _LIBS_DIR / _LIB_C_API
    if c_api_path.exists():
        core_path = _LIBS_DIR / _LIB_CORE
        try:
            if sys.platform == "win32":
                import os
                _dll_dir_handle = os.add_dll_directory(str(_LIBS_DIR))
            ffi.dlopen(str(core_path))
            lib = ffi.dlopen(str(c_api_path))
            _load_source = "bundled"
            return lib
        except OSError as e:
            raise RuntimeError(
                f"Cannot load bundled native libraries: {e}. "
                f"Searched: {_LIBS_DIR}"
            ) from None

    # --- Dev mode ---
    dev_core = "libquiver" if sys.platform == "win32" else f"libquiver{_EXT}"
    dev_c_api = "libquiver_c" if sys.platform == "win32" else f"libquiver_c{_EXT}"
    try:
        ffi.dlopen(dev_core)
    except Exception:
        pass

    try:
        lib = ffi.dlopen(dev_c_api)
        _load_source = "development"
        return lib
    except OSError:
        raise RuntimeError(
            f"Cannot load native libraries. "
            f"Searched: {_LIBS_DIR} (not found), system PATH. "
            f"Missing: {_LIB_C_API}"
        ) from None
```

### CMake RPATH for SKBUILD (Linux)
```cmake
# In root CMakeLists.txt, inside or near the existing SKBUILD install block
if(DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
    # On Linux, set RPATH so libquiver_c.so finds libquiver.so in same dir
    if(NOT WIN32)
        set_target_properties(quiver_c PROPERTIES
            INSTALL_RPATH "$ORIGIN"
        )
    endif()
endif()
```

### Diagnostic Mode (__main__)
```python
# Optional: python -m quiverdb._loader
if __name__ == "__main__":
    from quiverdb._c_api import ffi
    try:
        lib = load_library(ffi)
        print(f"OK: loaded via '{_load_source}'")
        print(f"  _libs dir: {_LIBS_DIR}")
        print(f"  _libs exists: {_LIBS_DIR.is_dir()}")
    except RuntimeError as e:
        print(f"FAIL: {e}")
        raise SystemExit(1)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `ctypes.windll.kernel32.SetDllDirectoryW()` | `os.add_dll_directory()` | Python 3.8 (2019) | Safer, scoped, returns closeable handle |
| PATH manipulation for DLL search | `os.add_dll_directory()` | Python 3.8 (2019) | No global side effects |
| CFFI `ffi.dlopen("name")` using system search | CFFI `ffi.dlopen("/full/path")` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` | CFFI 1.17 (2024) | Automatically finds dependent DLLs in same directory when path contains slashes |
| `auditwheel repair` for all Linux wheels | Manual RPATH + pre-bundled libs for CFFI ABI-mode | Always been this way | `auditwheel` cannot detect `ffi.dlopen()` dependencies statically; manual bundling required |

**Deprecated/outdated:**
- **Walking up directories to find `build/`**: The current `_loader.py` approach. Only useful for dev mode. The rewrite replaces this with `_libs/`-first discovery.
- **macOS/darwin code paths**: Stripped per user decision. Only Windows + Linux supported.
- **`os.environ['PATH']` manipulation on Windows**: Replaced by `os.add_dll_directory()` since Python 3.8.

## Open Questions

1. **auditwheel interaction with pre-bundled CFFI libraries**
   - What we know: `auditwheel` cannot detect `ffi.dlopen()` dependencies statically (documented limitation). The project manually bundles libs via CMake install targets.
   - What's unclear: When running `auditwheel repair` in CI (Phase 3), will it interfere with pre-bundled libs? Will it try to re-bundle or mangle them?
   - Recommendation: This is a Phase 3 concern. For Phase 2, the RPATH `$ORIGIN` approach is correct. In Phase 3, `auditwheel repair --no-update-tags` or `auditwheel show` can be tested. The STATE.md already flags this: "auditwheel behavior with pre-bundled CFFI libraries needs testing during Phase 2 local validation."
   - **Confidence:** MEDIUM -- the RPATH approach is well-documented, but auditwheel interaction needs empirical testing.

2. **dev-mode walk-up vs PATH-only fallback**
   - What we know: User runs tests via `test.bat` which prepends `build/bin/` to PATH. The walk-up approach in the current `_loader.py` finds `build/bin/` or `build/lib/` by walking up from `__file__`.
   - What's unclear: Is walk-up ever used outside of `test.bat`? Is there a workflow where PATH is not set but walk-up would work?
   - Recommendation: **PATH-only for dev mode.** Walk-up is fragile (breaks when installed to site-packages), and the user confirmed dev workflow convenience through the loader is not critical. PATH-only is simpler, more predictable, and matches the test.bat workflow. If PATH isn't set, the error message tells the user what's missing.
   - **Confidence:** HIGH -- user explicitly stated "dev workflow convenience through the loader is not critical."

## Sources

### Primary (HIGH confidence)
- [CFFI cdef/dlopen documentation](https://cffi.readthedocs.io/en/latest/cdef.html) - `ffi.dlopen()` behavior, Windows `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` in CFFI >= 1.17, absolute path support
- [scikit-build-core dynamic linking guide](https://scikit-build-core.readthedocs.io/en/latest/guide/dynamic_link.html) - RPATH `$ORIGIN` pattern for Linux, `os.add_dll_directory()` for Windows
- [Linux dlopen(3) man page](https://man7.org/linux/man-pages/man3/dlopen.3.html) - DT_NEEDED dependency resolution, RPATH search order
- Project codebase: `_loader.py`, `_c_api.py`, `CMakeLists.txt`, `cmake/Platform.cmake` - existing patterns and dependencies

### Secondary (MEDIUM confidence)
- [delvewheel GitHub](https://github.com/adang1345/delvewheel) - Windows DLL bundling patterns, `os.add_dll_directory()` usage
- [auditwheel GitHub](https://github.com/pypa/auditwheel) - Linux wheel repair, RPATH patching, CFFI dlopen limitation documented

### Tertiary (LOW confidence)
- [scikit-build-core RPATH issue #673](https://github.com/scikit-build/scikit-build-core/issues/673) - Practical challenges with RPATH in temporary wheel directories (may or may not apply to this project's simpler case)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - stdlib only (pathlib, os, sys) plus existing cffi dependency. No new dependencies.
- Architecture: HIGH - Two-file change (one Python rewrite + one CMake addition). Patterns verified against CFFI docs and scikit-build-core docs.
- Pitfalls: HIGH - All pitfalls verified against official documentation (CFFI dlopen behavior, Linux dlopen(3) RPATH resolution, os.add_dll_directory lifecycle).

**Research date:** 2026-02-25
**Valid until:** 2026-06-25 (stable domain -- Python native loading patterns change infrequently)
