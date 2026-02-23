from quiver.database import Database
from quiver.element import Element
from quiver.exceptions import QuiverError
from quiver.metadata import GroupMetadata, ScalarMetadata

__all__ = [
    "Database",
    "Element",
    "GroupMetadata",
    "QuiverError",
    "ScalarMetadata",
    "version",
]


def version() -> str:
    """Return the Quiver C library version string."""
    from quiver._c_api import ffi, get_lib

    lib = get_lib()
    return ffi.string(lib.quiver_version()).decode("utf-8")
