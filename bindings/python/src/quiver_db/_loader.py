from __future__ import annotations

import sys
from pathlib import Path

from cffi import FFI


def _find_library_dir() -> Path | None:
    """Walk up from this file to find a build/bin/ directory containing the DLLs."""
    current = Path(__file__).resolve().parent
    while current != current.parent:
        candidate = current / "build" / "bin"
        if candidate.is_dir():
            return candidate
        current = current.parent
    return None


def load_library(ffi: FFI):
    """Load the Quiver C API shared library via CFFI ABI mode.

    On Windows, pre-loads libquiver.dll so that libquiver_c.dll can resolve
    its dependency chain. Returns the CFFI library handle for libquiver_c.
    """
    lib_dir = _find_library_dir()

    if lib_dir is not None:
        if sys.platform == "win32":
            # Pre-load core library first (dependency of libquiver_c.dll).
            # The handle is not used further -- CFFI keeps it loaded in-process.
            ffi.dlopen(str(lib_dir / "libquiver.dll"))
            return ffi.dlopen(str(lib_dir / "libquiver_c.dll"))
        elif sys.platform == "darwin":
            return ffi.dlopen(str(lib_dir / "libquiver_c.dylib"))
        else:
            return ffi.dlopen(str(lib_dir / "libquiver_c.so"))

    # Fallback: load by name from system PATH
    return ffi.dlopen("libquiver_c")
