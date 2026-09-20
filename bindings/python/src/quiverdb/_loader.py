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

# Struct name -> native *_sizeof() accessor name. Fixed order (options, scalar, group, csv
# options, ui metadata) matches every other binding's load-time gate (SAFE-02/SAFE-03,
# D-08/D-09; ui metadata appended last per Phase 3 D-40 -- no reordering of the first four).
_STRUCT_SIZEOF_ACCESSORS = (
    ("quiver_database_options_t", "quiver_database_options_sizeof"),
    ("quiver_scalar_metadata_t", "quiver_scalar_metadata_sizeof"),
    ("quiver_group_metadata_t", "quiver_group_metadata_sizeof"),
    ("quiver_csv_options_t", "quiver_csv_options_sizeof"),
    ("quiver_ui_metadata_t", "quiver_ui_metadata_sizeof"),
)

# Wiring evidence: names of structs whose check has actually passed, in the order they were
# checked. Empty unless _assert_struct_sizes actually ran; partial if it short-circuited on a
# mismatch. Exists because a call to _assert_struct_sizes was once silently replaced with `pass`
# in this file (both call sites below) and no test noticed -- a test asserting this list is exact
# fails when the gate is unwired, which a test driving only _check_struct_size cannot do.
_CHECKED_STRUCTS: list[str] = []


def _check_struct_size(name: str, expected: int, native: int) -> None:
    """Raise RuntimeError naming the struct and both numbers when they disagree.

    D-09: this is one of the binding's few locally crafted error messages -- the C API cannot
    diagnose a disagreement about its own layout. Kept pure and parameterized so the failure
    path can be tested without a second, deliberately-mismatched native build.
    """
    if expected != native:
        raise RuntimeError(
            f"Struct size mismatch for {name}: this quiverdb binding expects {expected} bytes, "
            f"the loaded native library reports {native} bytes. Reinstall a matching quiverdb "
            f"native library."
        )


def _assert_struct_sizes(ffi: FFI, lib) -> None:
    """Call the five native *_sizeof() accessors and compare against this cdef's own sizes.

    CFFI ABI mode resolves each `lib.<name>` attribute via dlsym-on-demand -- a native library
    that predates this phase has no such symbol, and that only raises AttributeError at the
    CALL, never at ffi.dlopen (RESEARCH Pitfall 4). So this must actively call each accessor,
    not merely declare it in the cdef. Runs in the fixed order above, short-circuiting on the
    first mismatch or the first missing symbol.
    """
    _CHECKED_STRUCTS.clear()
    for struct_name, accessor_name in _STRUCT_SIZEOF_ACCESSORS:
        expected = ffi.sizeof(struct_name)
        try:
            accessor = getattr(lib, accessor_name)
        except AttributeError:
            raise RuntimeError(
                f"Native library is missing {accessor_name}() -- it predates this version of "
                f"quiverdb. Reinstall a matching quiverdb native library."
            ) from None
        _check_struct_size(struct_name, expected, accessor())
        _CHECKED_STRUCTS.append(struct_name)


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
            _assert_struct_sizes(ffi, lib)
            _load_source = "bundled"
            return lib
        except OSError as e:
            raise RuntimeError(f"Cannot load bundled native libraries: {e}. Searched: {_LIBS_DIR}") from None

    # --- Dev mode ---
    dev_core = "libquiver" if sys.platform == "win32" else f"libquiver{_EXT}"
    dev_c_api = "libquiver_c" if sys.platform == "win32" else f"libquiver_c{_EXT}"
    try:
        ffi.dlopen(dev_core)
    except Exception:
        pass

    try:
        lib = ffi.dlopen(dev_c_api)
        _assert_struct_sizes(ffi, lib)
        _load_source = "development"
        return lib
    except OSError:
        raise RuntimeError(
            f"Cannot load native libraries. Searched: {_LIBS_DIR} (not found), system PATH. Missing: {_LIB_C_API}"
        ) from None


if __name__ == "__main__":
    from quiverdb._c_api import ffi

    try:
        load_library(ffi)
        print(f"OK: loaded via '{_load_source}'")
        print(f"  _libs dir: {_LIBS_DIR}")
        print(f"  _libs exists: {_LIBS_DIR.is_dir()}")
    except RuntimeError as e:
        print(f"FAIL: {e}")
        raise SystemExit(1)
