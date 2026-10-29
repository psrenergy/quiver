from __future__ import annotations

import sys
from pathlib import Path

from cffi import FFI

_PACKAGE_DIR = Path(__file__).resolve().parent
_LIBS_DIR = _PACKAGE_DIR / "_libs"
_EXT = {"win32": ".dll", "darwin": ".dylib"}.get(sys.platform, ".so")
# Match the C API's install name on macOS to avoid loading a second core image.
_LIB_CORE = "libquiver.0.dylib" if sys.platform == "darwin" else f"libquiver{_EXT}"
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
    dev_core = "libquiver" if sys.platform == "win32" else _LIB_CORE
    dev_c_api = "libquiver_c" if sys.platform == "win32" else _LIB_C_API
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
