from quiverdb.database import Database
from quiverdb.exceptions import QuiverError
from quiverdb.lua_runner import LuaRunner
from quiverdb.metadata import CSVOptions, DataType, GroupMetadata, ScalarMetadata

__all__ = [
    "CSVOptions",
    "DataType",
    "Database",
    "GroupMetadata",
    "LuaRunner",
    "QuiverError",
    "ScalarMetadata",
    "version",
]


def version() -> str:
    """Return the Quiver C library version string."""
    from quiverdb._c_api import ffi, get_lib

    lib = get_lib()
    return ffi.string(lib.quiver_version()).decode("utf-8")
