from __future__ import annotations

from datetime import datetime

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, column_data_type, decode_string, format_datetime
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import DataType


class Element:
    """Builder for creating database elements with a fluent set() API."""

    def __init__(self) -> None:
        lib = get_lib()
        out = ffi.new("quiver_element_t**")
        check(lib.quiver_element_create(out))
        self._ptr = out[0]
        self._destroyed = False

    def set(self, name: str, value: object) -> Element:
        """Set an attribute value. Returns self for fluent chaining.

        Supported types: int, float, str, None, bool (stored as int), datetime (stored as a
        DATE_TIME string, see format_datetime), and lists of int/bool, float, str or datetime --
        a float anywhere in a numeric list makes it a float array.
        """
        self._ensure_valid()
        if value is None:
            self._set_null(name)
        elif isinstance(value, int):  # bool is an int subclass: True/False marshal as 1/0
            self._set_integer(name, value)
        elif isinstance(value, float):
            self._set_float(name, value)
        elif isinstance(value, str):
            self._set_string(name, value)
        elif isinstance(value, datetime):
            self._set_string(name, format_datetime(value))
        elif isinstance(value, list):
            self._set_array(name, value)
        else:
            raise TypeError(f"Unsupported type {type(value).__name__} for Element.set('{name}')")
        return self

    def _set_integer(self, name: str, value: int) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_integer(self._ptr, name.encode("utf-8"), value))

    def _set_float(self, name: str, value: float) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_float(self._ptr, name.encode("utf-8"), value))

    def _set_string(self, name: str, value: str) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_string(self._ptr, name.encode("utf-8"), value.encode("utf-8")))

    def _set_null(self, name: str) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_null(self._ptr, name.encode("utf-8")))

    def _set_array(self, name: str, values: list) -> None:
        lib = get_lib()
        if len(values) == 0:
            # Empty array -- type doesn't matter
            check(lib.quiver_element_set_array_integer(self._ptr, name.encode("utf-8"), ffi.NULL, 0, ffi.NULL))
            return

        # A vector/set/time-series read returns a NULL cell as None. The element surface is non-null
        # (NULL cells go through the group writers), so name the column here instead of failing
        # inside cffi or on str.encode.
        if any(v is None for v in values):
            raise TypeError(
                f"Unsupported array element type NoneType for Element.set('{name}'): "
                "write NULL cells with update_vector_group, update_set_group or update_time_series_group"
            )
        # Typed from every cell like a group-writer column (column_data_type).
        array_type = column_data_type(name, values)
        if array_type == DataType.INTEGER:
            self._set_array_integer(name, values)
        elif array_type == DataType.FLOAT:
            self._set_array_float(name, values)
        elif array_type == DataType.STRING:
            self._set_array_string(name, values)
        else:
            # DataType.DATE_TIME: None was refused above, so every cell is a datetime.
            self._set_array_string(name, [format_datetime(v) for v in values])

    def _set_array_integer(self, name: str, values: list[int]) -> None:
        lib = get_lib()
        c_arr = ffi.new("int64_t[]", values)
        check(lib.quiver_element_set_array_integer(self._ptr, name.encode("utf-8"), c_arr, len(values), ffi.NULL))

    def _set_array_float(self, name: str, values: list[float]) -> None:
        lib = get_lib()
        c_arr = ffi.new("double[]", values)
        check(lib.quiver_element_set_array_float(self._ptr, name.encode("utf-8"), c_arr, len(values), ffi.NULL))

    def _set_array_string(self, name: str, values: list[str]) -> None:
        lib = get_lib()
        encoded = [v.encode("utf-8") for v in values]
        c_strings = [ffi.new("char[]", e) for e in encoded]
        c_arr = ffi.new("const char*[]", c_strings)
        check(lib.quiver_element_set_array_string(self._ptr, name.encode("utf-8"), c_arr, len(values), ffi.NULL))

    def _ensure_valid(self) -> None:
        if self._destroyed:
            raise QuiverError("Element has been destroyed")

    def destroy(self) -> None:
        """Free the underlying C element. Idempotent."""
        if self._destroyed:
            return
        lib = get_lib()
        lib.quiver_element_destroy(self._ptr)
        self._ptr = ffi.NULL
        self._destroyed = True

    def clear(self) -> None:
        """Clear all set attributes from this element."""
        self._ensure_valid()
        lib = get_lib()
        check(lib.quiver_element_clear(self._ptr))

    def __repr__(self) -> str:
        if self._destroyed:
            return "Element(destroyed)"
        lib = get_lib()
        out = ffi.new("char**")
        check(lib.quiver_element_to_string(self._ptr, out))
        result = decode_string(out[0])
        lib.quiver_database_free_string(out[0])
        return result

    def __del__(self) -> None:
        if not self._destroyed:
            self.destroy()
