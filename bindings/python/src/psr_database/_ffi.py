"""CFFI bindings for PSR Database C API."""

import os
import sys
from pathlib import Path

from cffi import FFI

from psr_database._bindings import CDEF

ffi = FFI()
ffi.cdef(CDEF)


def _find_library() -> str:
    """Find the PSR Database C library."""
    if sys.platform == "win32":
        lib_name = "libpsr_database_c.dll"
    elif sys.platform == "darwin":
        lib_name = "libpsr_database_c.dylib"
    else:
        lib_name = "libpsr_database_c.so"

    # 1. Environment variable
    if "PSR_DATABASE_LIB_PATH" in os.environ:
        path = Path(os.environ["PSR_DATABASE_LIB_PATH"]) / lib_name
        if path.exists():
            return str(path)

    # 2. Development path (bindings/python/src/psr_database -> build/bin)
    project_root = Path(__file__).parent.parent.parent.parent.parent
    path = project_root / "build" / "bin" / lib_name
    if path.exists():
        return str(path)

    # 3. System PATH
    return lib_name


lib = ffi.dlopen(_find_library())
