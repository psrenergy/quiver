from quiverdb.database import Database
from quiverdb.database_csv_export import DatabaseCSVExport
from quiverdb.database_csv_import import DatabaseCSVImport
from quiverdb.element import Element
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import CSVOptions, GroupMetadata, ScalarMetadata

__all__ = [
    "CSVOptions",
    "Database",
    "DatabaseCSVExport",
    "DatabaseCSVImport",
    "Element",
    "GroupMetadata",
    "QuiverError",
    "ScalarMetadata",
    "version",
]


def version() -> str:
    """Return the Quiver C library version string."""
    from quiverdb._c_api import ffi, get_lib

    lib = get_lib()
    return ffi.string(lib.quiver_version()).decode("utf-8")
