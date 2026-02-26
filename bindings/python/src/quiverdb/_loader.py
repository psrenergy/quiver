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
