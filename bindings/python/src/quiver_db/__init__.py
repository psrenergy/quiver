from quiver_db.database import Database
from quiver_db.element import Element
from quiver_db.exceptions import QuiverError
from quiver_db.metadata import CSVExportOptions, GroupMetadata, ScalarMetadata

__all__ = [
    "CSVExportOptions",
    "Database",
    "Element",
    "GroupMetadata",
    "QuiverError",
    "ScalarMetadata",
    "version",
]


def version() -> str:
    """Return the Quiver C library version string."""
    from quiver_db._c_api import ffi, get_lib

    lib = get_lib()
    return ffi.string(lib.quiver_version()).decode("utf-8")
