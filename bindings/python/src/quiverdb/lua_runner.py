from __future__ import annotations

import warnings
from typing import TYPE_CHECKING

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check
from quiverdb.exceptions import QuiverError

if TYPE_CHECKING:
    from quiverdb.database import Database


class LuaRunner:
    """Execute Lua scripts against a Quiver database.

    Wraps the C API quiver_lua_runner_new/run/get_error/free functions.
    Holds a reference to the Database to prevent GC while the runner is alive.
    """

    def __init__(self, db: Database) -> None:
        self._db = db
        self._closed = False
        lib = get_lib()
        out_runner = ffi.new("quiver_lua_runner_t**")
        check(lib.quiver_lua_runner_new(db._ptr, out_runner))
        self._ptr = out_runner[0]

    def close(self) -> None:
        """Free the LuaRunner handle. Idempotent."""
        if self._closed:
            return
        lib = get_lib()
        lib.quiver_lua_runner_free(self._ptr)
        self._ptr = ffi.NULL
        self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("LuaRunner is closed")

    def run(self, script: str) -> None:
        """Execute a Lua script against the database.

        Raises QuiverError if the script fails (syntax or runtime error).
        """
        self._ensure_open()
        lib = get_lib()
        err = lib.quiver_lua_runner_run(self._ptr, script.encode("utf-8"))
        if err != 0:
            # Two-source error resolution:
            # 1. Try runner-specific error (Lua errors)
            out_error = ffi.new("const char**")
            get_err = lib.quiver_lua_runner_get_error(self._ptr, out_error)
            if get_err == 0 and out_error[0] != ffi.NULL:
                msg = ffi.string(out_error[0]).decode("utf-8")
                if msg:
                    raise QuiverError(msg)

            # 2. Fall back to global error
            ptr = lib.quiver_get_last_error()
            if ptr != ffi.NULL:
                detail = ffi.string(ptr).decode("utf-8")
                if detail:
                    raise QuiverError(detail)

            # 3. Generic fallback
            raise QuiverError("Lua script execution failed")

    def __enter__(self) -> LuaRunner:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def __del__(self) -> None:
        if not self._closed:
            warnings.warn(
                "LuaRunner was not closed explicitly", ResourceWarning, stacklevel=2
            )
            self.close()
