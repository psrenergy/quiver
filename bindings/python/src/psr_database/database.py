"""Database class wrapper."""

from psr_database._ffi import ffi, lib
from psr_database.exceptions import DatabaseError


class Database:
    """PSR Database handle."""

    def __init__(self, ptr):
        self._ptr = ptr
        self._closed = False

    @staticmethod
    def from_schema(db_path: str, schema_path: str, *, read_only: bool = False) -> "Database":
        """Create database from SQL schema file."""
        options = lib.psr_database_options_default()
        options.read_only = int(read_only)
        ptr = lib.psr_database_from_schema(
            db_path.encode("utf-8"),
            schema_path.encode("utf-8"),
            ffi.addressof(options),
        )
        if ptr == ffi.NULL:
            raise DatabaseError(f"Failed to create database from schema: {schema_path}")
        return Database(ptr)

    @staticmethod
    def from_migrations(db_path: str, migrations_path: str, *, read_only: bool = False) -> "Database":
        """Create database from migrations directory."""
        options = lib.psr_database_options_default()
        options.read_only = int(read_only)
        ptr = lib.psr_database_from_migrations(
            db_path.encode("utf-8"),
            migrations_path.encode("utf-8"),
            ffi.addressof(options),
        )
        if ptr == ffi.NULL:
            raise DatabaseError(f"Failed to create database from migrations: {migrations_path}")
        return Database(ptr)

    def close(self) -> None:
        """Close the database."""
        if self._closed:
            return
        lib.psr_database_close(self._ptr)
        self._closed = True

    def is_healthy(self) -> bool:
        """Check if database is healthy."""
        if self._closed:
            return False
        return lib.psr_database_is_healthy(self._ptr) == 1

    def path(self) -> str:
        """Get database path."""
        if self._closed:
            raise DatabaseError("Database is closed")
        result = lib.psr_database_path(self._ptr)
        return ffi.string(result).decode("utf-8")

    def current_version(self) -> int:
        """Get current migration version."""
        if self._closed:
            raise DatabaseError("Database is closed")
        return lib.psr_database_current_version(self._ptr)

    def __enter__(self) -> "Database":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def __del__(self) -> None:
        self.close()
