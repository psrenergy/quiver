from quiver.exceptions import QuiverError

__all__ = ["QuiverError", "version"]


def version() -> str:
    """Return the Quiver C library version string."""
    from quiver._c_api import ffi, get_lib

    lib = get_lib()
    return ffi.string(lib.quiver_version()).decode("utf-8")
