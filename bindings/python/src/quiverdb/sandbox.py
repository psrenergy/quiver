from __future__ import annotations

import warnings
from typing import TYPE_CHECKING

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, decode_string
from quiverdb.exceptions import QuiverError

if TYPE_CHECKING:
    from quiverdb.database import Database


class Sandbox:
    """Execute scripts against a Quiver database.

    Wraps the C API quiver_sandbox_new/run/free functions.
    Holds a reference to the Database to prevent GC while the runner is alive.
    """

    def __init__(self, db: Database) -> None:
        # Closed until the native runner exists: Python still runs __del__ when __init__ raises,
        # and a failed construction must be a no-op there, not a warning plus a missing _ptr.
        self._closed = True
        self._db = db
        lib = get_lib()
        out_runner = ffi.new("quiver_sandbox_t**")
        check(lib.quiver_sandbox_new(db._ptr, out_runner))
        self._ptr = out_runner[0]
        self._closed = False

    def close(self) -> None:
        """Free the Sandbox handle. Idempotent."""
        if self._closed:
            return
        lib = get_lib()
        lib.quiver_sandbox_free(self._ptr)
        self._ptr = ffi.NULL
        self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("Sandbox is closed")

    def run(self, script: str) -> str:
        """Execute a script against the database.

        Returns the script's return value encoded as JSON, or "" if it returned nothing.

        To execute a script without keeping its writes, wrap the call in Database.dry_run().

        Raises QuiverError if the script fails (syntax or runtime error).
        """
        self._ensure_open()
        lib = get_lib()
        out_result = ffi.new("char**")
        check(lib.quiver_sandbox_run(self._ptr, script.encode("utf-8"), out_result))
        try:
            return decode_string(out_result[0])
        finally:
            # In a finally so a decode failure cannot leak the native JSON buffer.
            lib.quiver_sandbox_free_string(out_result[0])

    def __enter__(self) -> Sandbox:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def __del__(self) -> None:
        if not self._closed:
            warnings.warn("Sandbox was not closed explicitly", ResourceWarning, stacklevel=2)
            self.close()
