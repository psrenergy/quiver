from __future__ import annotations

import warnings

from quiver._c_api import ffi, get_lib
from quiver._helpers import check, decode_string
from quiver.exceptions import QuiverError


class Database:
    """Thin Python wrapper around the Quiver C API database handle."""

    def __init__(self, ptr) -> None:
        self._ptr = ptr
        self._closed = False

    @staticmethod
    def from_schema(db_path: str, schema_path: str) -> Database:
        """Create a database from a SQL schema file."""
        lib = get_lib()
        options = ffi.new("quiver_database_options_t*")
        options[0] = lib.quiver_database_options_default()
        out_db = ffi.new("quiver_database_t**")
        check(
            lib.quiver_database_from_schema(
                db_path.encode("utf-8"),
                schema_path.encode("utf-8"),
                options,
                out_db,
            )
        )
        return Database(out_db[0])

    @staticmethod
    def from_migrations(db_path: str, migrations_path: str) -> Database:
        """Create a database using a migrations directory."""
        lib = get_lib()
        options = ffi.new("quiver_database_options_t*")
        options[0] = lib.quiver_database_options_default()
        out_db = ffi.new("quiver_database_t**")
        check(
            lib.quiver_database_from_migrations(
                db_path.encode("utf-8"),
                migrations_path.encode("utf-8"),
                options,
                out_db,
            )
        )
        return Database(out_db[0])

    def close(self) -> None:
        """Close the database. Idempotent -- safe to call multiple times."""
        if self._closed:
            return
        lib = get_lib()
        check(lib.quiver_database_close(self._ptr))
        self._ptr = ffi.NULL
        self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("Database has been closed")

    def __enter__(self) -> Database:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def __del__(self) -> None:
        if not self._closed:
            warnings.warn(
                "Database was not closed explicitly",
                ResourceWarning,
                stacklevel=2,
            )
            self.close()

    def path(self) -> str:
        """Return the database file path."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("const char**")
        check(lib.quiver_database_path(self._ptr, out))
        return decode_string(out[0])

    def current_version(self) -> int:
        """Return the current schema version number."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("int64_t*")
        check(lib.quiver_database_current_version(self._ptr, out))
        return out[0]

    def is_healthy(self) -> bool:
        """Return True if the database passes integrity checks."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("int*")
        check(lib.quiver_database_is_healthy(self._ptr, out))
        return bool(out[0])

    def describe(self) -> None:
        """Print schema inspection output to stdout."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_describe(self._ptr))

    def create_element(self, collection: str, element) -> int:
        """Create a new element. Returns the new element ID."""
        self._ensure_open()
        lib = get_lib()
        out_id = ffi.new("int64_t*")
        check(
            lib.quiver_database_create_element(
                self._ptr, collection.encode("utf-8"), element._ptr, out_id,
            )
        )
        return out_id[0]

    def __repr__(self) -> str:
        if self._closed:
            return "Database(closed)"
        return f"Database(path={self.path()!r})"
