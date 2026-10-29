from quiverdb.database import Database
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import (
    CSVOptions,
    DataType,
    GroupMetadata,
    LogLevel,
    ScalarMetadata,
)
from quiverdb.sandbox import Sandbox

__all__ = [
    "CSVOptions",
    "DataType",
    "Database",
    "GroupMetadata",
    "LogLevel",
    "Sandbox",
    "QuiverError",
    "ScalarMetadata",
    "version",
]


def version() -> str:
    """Return the Quiver C library version string."""
    from quiverdb._c_api import get_lib
    from quiverdb._helpers import decode_string

    lib = get_lib()
    return decode_string(lib.quiver_version())
