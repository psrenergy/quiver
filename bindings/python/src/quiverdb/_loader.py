from __future__ import annotations

import sys
from pathlib import Path

from cffi import FFI


def _find_library_dir() -> Path | None:
    """Walk up from this file to find a build directory containing the shared libraries."""
    current = Path(__file__).resolve().parent

    # Platform-specific library name to look for
    if sys.platform == "win32":
        lib_name = "libquiver_c.dll"
        subdirs = ["bin", "lib"]
    elif sys.platform == "darwin":
        lib_name = "libquiver_c.dylib"
        subdirs = ["lib", "bin"]
    else:
        lib_name = "libquiver_c.so"
        subdirs = ["lib", "bin"]

    while current != current.parent:
        build_dir = current / "build"
        if build_dir.is_dir():
            for subdir in subdirs:
                candidate = build_dir / subdir
                if candidate.is_dir() and (candidate / lib_name).exists():
                    return candidate
        current = current.parent
    return None


def load_library(ffi: FFI):
    """Load the Quiver C API shared library via CFFI ABI mode.

    Pre-loads the core library first so that the C API library can resolve
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
            ffi.dlopen(str(lib_dir / "libquiver.dylib"))
            return ffi.dlopen(str(lib_dir / "libquiver_c.dylib"))
        else:
            ffi.dlopen(str(lib_dir / "libquiver.so"))
            return ffi.dlopen(str(lib_dir / "libquiver_c.so"))

    # Fallback: load by name from system PATH.
    # We try to pre-load the core library here too, but ignore errors if not found
    # as it might be statically linked or already in the search path.
    try:
        if sys.platform == "win32":
            ffi.dlopen("libquiver")
        else:
            ffi.dlopen("libquiver.so" if sys.platform != "darwin" else "libquiver.dylib")
    except Exception:
        pass

    return ffi.dlopen("libquiver_c" if sys.platform == "win32" else "libquiver_c.so" if sys.platform != "darwin" else "libquiver_c.dylib")
