from __future__ import annotations

from datetime import datetime, timezone

from quiverdb._c_api import ffi, get_lib
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import DataType


def check(err: int) -> None:
    """Raise QuiverError if err indicates a C API failure.

    Reads the thread-local error message via quiver_get_last_error().
    """
    if err != 0:
        lib = get_lib()
        ptr = lib.quiver_get_last_error()
        detail = ffi.string(ptr).decode("utf-8") if ptr != ffi.NULL else ""
        raise QuiverError(detail or "Unknown error")


def decode_string(ptr) -> str:
    """Decode a non-null char* pointer from the C API to a Python string."""
    return ffi.string(ptr).decode("utf-8")


def decode_string_or_none(ptr) -> str | None:
    """Decode a nullable char* pointer. Returns None if ptr is NULL."""
    if ptr == ffi.NULL:
        return None
    return ffi.string(ptr).decode("utf-8")


_NUMERIC = (DataType.INTEGER, DataType.FLOAT)


def column_data_type(name: str, values: list) -> DataType | None:
    """Type one column of cells -- a group-writer column or an element array -- from every cell.

    bool/int cells are INTEGER (a bool is 1/0), and a float anywhere among them widens the column
    to FLOAT: an int loses nothing by it, since the core's int-for-REAL rule stores it as REAL
    anyway and still rejects a float in an INTEGER column. str cells are STRING and datetime cells
    DATE_TIME. None cells are skipped; a column with no other cell is typed None.

    A cell of any other type, or one that does not fit the column (a str among numbers, a number
    among strs, a str among datetimes), raises TypeError naming the cell and the column. No cell
    is run through int()/float() to make it fit: that turned 2.5 into 2 and "7" into 7 with no
    error. cffi converts the bool/int/float cells of a typed column itself.
    """
    column_type = None
    for i, v in enumerate(values):
        if v is None:
            continue
        if isinstance(v, datetime):
            cell_type = DataType.DATE_TIME
        elif isinstance(v, str):
            cell_type = DataType.STRING
        elif isinstance(v, float):
            cell_type = DataType.FLOAT
        elif isinstance(v, int):  # bool is an int subclass
            cell_type = DataType.INTEGER
        else:
            cell_type = None
        if cell_type is not None and column_type in (None, cell_type):
            column_type = cell_type
        elif column_type in _NUMERIC and cell_type in _NUMERIC:
            column_type = DataType.FLOAT
        else:
            raise TypeError(f"Unsupported value type {type(v).__name__} in cell {i} of column '{name}'")
    return column_type


def format_datetime(value: datetime) -> str:
    """Format a datetime as the core's DATE_TIME string, YYYY-MM-DDTHH:MM:SS.

    An aware value is converted to UTC first, the zone every datetime reader returns, so
    10:00+03:00 is written as 07:00. A naive value is written as given. Sub-second precision
    is truncated; the core's grammar has none.
    """
    if value.utcoffset() is not None:
        value = value.astimezone(timezone.utc).replace(tzinfo=None)
    return value.isoformat(timespec="seconds")
