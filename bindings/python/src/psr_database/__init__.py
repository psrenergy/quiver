"""Python bindings for PSR Database."""

from enum import IntEnum

from psr_database._ffi import ffi, lib
from psr_database.database import Database
from psr_database.exceptions import DatabaseError

__all__ = ["Database", "DatabaseError", "ErrorCode", "version", "error_string"]


class ErrorCode(IntEnum):
    """Error codes from PSR Database C API."""

    OK = 0
    INVALID_ARGUMENT = -1
    DATABASE = -2
    MIGRATION = -3
    SCHEMA = -4
    CREATE_ELEMENT = -5
    NOT_FOUND = -6


def version() -> str:
    """Get the PSR Database library version."""
    return ffi.string(lib.psr_version()).decode("utf-8")


def error_string(error_code: int) -> str:
    """Get a human-readable error message for an error code."""
    return ffi.string(lib.psr_error_string(error_code)).decode("utf-8")
