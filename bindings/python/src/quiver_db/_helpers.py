from __future__ import annotations

from quiver_db._c_api import ffi, get_lib
from quiver_db.exceptions import QuiverError


def check(err: int) -> None:
    """Raise QuiverError if err indicates a C API failure.

    Reads the thread-local error message via quiver_get_last_error().
    Does NOT call quiver_clear_last_error() (matches Julia/Dart behavior).
    """
    if err != 0:
        lib = get_lib()
        ptr = lib.quiver_get_last_error()
        detail = ffi.string(ptr).decode("utf-8") if ptr != ffi.NULL else ""
        raise QuiverError(detail or "Unknown error")


def encode_string(s: str) -> bytes:
    """Encode a Python string to UTF-8 bytes for passing to the C API."""
    return s.encode("utf-8")


def decode_string(ptr) -> str:
    """Decode a non-null char* pointer from the C API to a Python string."""
    return ffi.string(ptr).decode("utf-8")


def decode_string_or_none(ptr) -> str | None:
    """Decode a nullable char* pointer. Returns None if ptr is NULL."""
    if ptr == ffi.NULL:
        return None
    return ffi.string(ptr).decode("utf-8")
