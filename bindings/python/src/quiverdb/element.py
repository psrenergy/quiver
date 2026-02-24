from __future__ import annotations

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, decode_string
from quiverdb.exceptions import QuiverError


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

        Supported types: int, float, str, None, bool (stored as int),
        list[int], list[float], list[str].
        """
        self._ensure_valid()
        if value is None:
            self._set_null(name)
        elif isinstance(value, bool):
            # Must check bool before int (bool is subclass of int)
            self._set_integer(name, int(value))
        elif isinstance(value, int):
            self._set_integer(name, value)
        elif isinstance(value, float):
            self._set_float(name, value)
        elif isinstance(value, str):
            self._set_string(name, value)
        elif isinstance(value, list):
            self._set_array(name, value)
        else:
            raise TypeError(
                f"Unsupported type {type(value).__name__} for Element.set('{name}')"
            )
        return self

    def _set_integer(self, name: str, value: int) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_integer(self._ptr, name.encode("utf-8"), value))

    def _set_float(self, name: str, value: float) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_float(self._ptr, name.encode("utf-8"), value))

    def _set_string(self, name: str, value: str) -> None:
        lib = get_lib()
        check(
            lib.quiver_element_set_string(
                self._ptr, name.encode("utf-8"), value.encode("utf-8")
            )
        )

    def _set_null(self, name: str) -> None:
        lib = get_lib()
        check(lib.quiver_element_set_null(self._ptr, name.encode("utf-8")))

    def _set_array(self, name: str, values: list) -> None:
        lib = get_lib()
        if len(values) == 0:
            # Empty array -- type doesn't matter
            check(
                lib.quiver_element_set_array_integer(
                    self._ptr, name.encode("utf-8"), ffi.NULL, 0
                )
            )
            return

        first = values[0]
        if isinstance(first, bool):
            self._set_array_integer(name, [int(v) for v in values])
        elif isinstance(first, int):
            self._set_array_integer(name, values)
        elif isinstance(first, float):
            self._set_array_float(name, values)
        elif isinstance(first, str):
            self._set_array_string(name, values)
        else:
            raise TypeError(
                f"Unsupported array element type {type(first).__name__} for Element.set('{name}')"
            )

    def _set_array_integer(self, name: str, values: list[int]) -> None:
        lib = get_lib()
        c_arr = ffi.new("int64_t[]", values)
        check(
            lib.quiver_element_set_array_integer(
                self._ptr, name.encode("utf-8"), c_arr, len(values)
            )
        )

    def _set_array_float(self, name: str, values: list[float]) -> None:
        lib = get_lib()
        c_arr = ffi.new("double[]", values)
        check(
            lib.quiver_element_set_array_float(
                self._ptr, name.encode("utf-8"), c_arr, len(values)
            )
        )

    def _set_array_string(self, name: str, values: list[str]) -> None:
        lib = get_lib()
        encoded = [v.encode("utf-8") for v in values]
        c_strings = [ffi.new("char[]", e) for e in encoded]
        c_arr = ffi.new("const char*[]", c_strings)
        check(
            lib.quiver_element_set_array_string(
                self._ptr, name.encode("utf-8"), c_arr, len(values)
            )
        )

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
        lib.quiver_element_free_string(out[0])
        return result

    def __del__(self) -> None:
        if not self._destroyed:
            self.destroy()
