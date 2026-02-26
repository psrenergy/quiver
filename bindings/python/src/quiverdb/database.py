from __future__ import annotations

import warnings
from contextlib import contextmanager
from datetime import datetime, timezone

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, decode_string, decode_string_or_none
from quiverdb.database_csv_export import DatabaseCSVExport
from quiverdb.database_csv_import import DatabaseCSVImport
from quiverdb.element import Element
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import GroupMetadata, ScalarMetadata


class Database(DatabaseCSVExport, DatabaseCSVImport):
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

    def create_element(self, collection: str, **kwargs: object) -> int:
        """Create a new element. Returns the new element ID."""
        self._ensure_open()
        elem = Element()
        try:
            for name, value in kwargs.items():
                elem.set(name, value)
            lib = get_lib()
            out_id = ffi.new("int64_t*")
            check(
                lib.quiver_database_create_element(
                    self._ptr,
                    collection.encode("utf-8"),
                    elem._ptr,
                    out_id,
                )
            )
            return out_id[0]
        finally:
            elem.destroy()

    # -- Write operations -------------------------------------------------------

    def update_element(self, collection: str, id: int, **kwargs: object) -> None:
        """Update an existing element's attributes."""
        self._ensure_open()
        elem = Element()
        try:
            for name, value in kwargs.items():
                elem.set(name, value)
            lib = get_lib()
            check(
                lib.quiver_database_update_element(
                    self._ptr,
                    collection.encode("utf-8"),
                    id,
                    elem._ptr,
                )
            )
        finally:
            elem.destroy()

    def delete_element(self, collection: str, id: int) -> None:
        """Delete an element by ID."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_delete_element(self._ptr, collection.encode("utf-8"), id))

    # -- Transaction control ----------------------------------------------------

    def begin_transaction(self) -> None:
        """Begin an explicit transaction."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_begin_transaction(self._ptr))

    def commit(self) -> None:
        """Commit the current transaction."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_commit(self._ptr))

    def rollback(self) -> None:
        """Roll back the current transaction."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_rollback(self._ptr))

    def in_transaction(self) -> bool:
        """Return True if a transaction is currently active."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("_Bool*")
        check(lib.quiver_database_in_transaction(self._ptr, out))
        return bool(out[0])

    @contextmanager
    def transaction(self):
        """Execute operations within a transaction. Auto-commits on success, rolls back on exception."""
        self._ensure_open()
        self.begin_transaction()
        try:
            yield self
            self.commit()
        except BaseException:
            try:
                self.rollback()
            except Exception:
                pass  # Best-effort rollback; swallow rollback errors
            raise

    # -- Query operations -------------------------------------------------------

    def query_string(self, sql: str, *, params: list | None = None) -> str | None:
        """Execute SQL and return the first column of the first row as str, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")

        if params is not None and len(params) > 0:
            keepalive, c_types, c_values = _marshal_params(params)
            check(
                lib.quiver_database_query_string_params(
                    self._ptr,
                    sql.encode("utf-8"),
                    c_types,
                    c_values,
                    len(params),
                    out_value,
                    out_has,
                )
            )
        else:
            check(
                lib.quiver_database_query_string(
                    self._ptr,
                    sql.encode("utf-8"),
                    out_value,
                    out_has,
                )
            )

        if out_has[0] == 0 or out_value[0] == ffi.NULL:
            return None
        try:
            return ffi.string(out_value[0]).decode("utf-8")
        finally:
            lib.quiver_element_free_string(out_value[0])

    def query_integer(self, sql: str, *, params: list | None = None) -> int | None:
        """Execute SQL and return the first column of the first row as int, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("int64_t*")
        out_has = ffi.new("int*")

        if params is not None and len(params) > 0:
            keepalive, c_types, c_values = _marshal_params(params)
            check(
                lib.quiver_database_query_integer_params(
                    self._ptr,
                    sql.encode("utf-8"),
                    c_types,
                    c_values,
                    len(params),
                    out_value,
                    out_has,
                )
            )
        else:
            check(
                lib.quiver_database_query_integer(
                    self._ptr,
                    sql.encode("utf-8"),
                    out_value,
                    out_has,
                )
            )

        if out_has[0] == 0:
            return None
        return out_value[0]

    def query_float(self, sql: str, *, params: list | None = None) -> float | None:
        """Execute SQL and return the first column of the first row as float, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("double*")
        out_has = ffi.new("int*")

        if params is not None and len(params) > 0:
            keepalive, c_types, c_values = _marshal_params(params)
            check(
                lib.quiver_database_query_float_params(
                    self._ptr,
                    sql.encode("utf-8"),
                    c_types,
                    c_values,
                    len(params),
                    out_value,
                    out_has,
                )
            )
        else:
            check(
                lib.quiver_database_query_float(
                    self._ptr,
                    sql.encode("utf-8"),
                    out_value,
                    out_has,
                )
            )

        if out_has[0] == 0:
            return None
        return out_value[0]

    def query_date_time(self, sql: str, *, params: list | None = None) -> datetime | None:
        """Execute SQL and return the first column of the first row as datetime, or None."""
        result = self.query_string(sql, params=params)
        if result is None:
            return None
        return _parse_datetime(result)

    def read_scalar_date_time_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> datetime | None:
        """Read a datetime scalar attribute. Returns timezone-aware UTC datetime or None."""
        return _parse_datetime(self.read_scalar_string_by_id(collection, attribute, id))

    def read_vector_date_time_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[datetime]:
        """Read datetime values from a vector. Returns list of timezone-aware UTC datetimes."""
        return [_parse_datetime(s) for s in self.read_vector_strings_by_id(collection, attribute, id)]

    def read_set_date_time_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[datetime]:
        """Read datetime values from a set. Returns list of timezone-aware UTC datetimes."""
        return [_parse_datetime(s) for s in self.read_set_strings_by_id(collection, attribute, id)]

    # -- Scalar reads (bulk) --------------------------------------------------

    def read_scalar_integers(self, collection: str, attribute: str) -> list[int]:
        """Read all integer values for a scalar attribute across all elements."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_scalar_integers(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])

    def read_scalar_floats(self, collection: str, attribute: str) -> list[float]:
        """Read all float values for a scalar attribute across all elements."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_scalar_floats(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])

    def read_scalar_strings(self, collection: str, attribute: str) -> list[str | None]:
        """Read all string values for a scalar attribute. NULL values become None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_scalar_strings(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            result: list[str | None] = []
            for i in range(count):
                ptr = out_values[0][i]
                if ptr == ffi.NULL:
                    result.append(None)
                else:
                    result.append(ffi.string(ptr).decode("utf-8"))
            return result
        finally:
            lib.quiver_database_free_string_array(out_values[0], count)

    # -- Scalar reads (by ID) -------------------------------------------------

    def read_scalar_integer_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> int | None:
        """Read an integer scalar attribute for a single element. Returns None if NULL."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("int64_t*")
        out_has = ffi.new("int*")
        check(
            lib.quiver_database_read_scalar_integer_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_value,
                out_has,
            )
        )
        if out_has[0] == 0:
            return None
        return out_value[0]

    def read_scalar_float_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> float | None:
        """Read a float scalar attribute for a single element. Returns None if NULL."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("double*")
        out_has = ffi.new("int*")
        check(
            lib.quiver_database_read_scalar_float_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_value,
                out_has,
            )
        )
        if out_has[0] == 0:
            return None
        return out_value[0]

    def read_scalar_string_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> str | None:
        """Read a string scalar attribute for a single element. Returns None if NULL."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")
        check(
            lib.quiver_database_read_scalar_string_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_value,
                out_has,
            )
        )
        if out_has[0] == 0 or out_value[0] == ffi.NULL:
            return None
        try:
            return ffi.string(out_value[0]).decode("utf-8")
        finally:
            lib.quiver_element_free_string(out_value[0])

    # -- Element IDs -----------------------------------------------------------

    def read_element_ids(self, collection: str) -> list[int]:
        """Read all element IDs in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_ids = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_element_ids(
                self._ptr,
                collection.encode("utf-8"),
                out_ids,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_ids[0] == ffi.NULL:
            return []
        try:
            return [out_ids[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_ids[0])

    # -- Vector reads (bulk) -----------------------------------------------------

    def read_vector_integers(self, collection: str, attribute: str) -> list[list[int]]:
        """Read integer vectors for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_vectors = ffi.new("int64_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_integers(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_vectors,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_vectors[0] == ffi.NULL:
            return []
        try:
            result: list[list[int]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_vectors[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    result.append([out_vectors[0][i][j] for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_integer_vectors(out_vectors[0], out_sizes[0], count)

    def read_vector_floats(self, collection: str, attribute: str) -> list[list[float]]:
        """Read float vectors for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_vectors = ffi.new("double***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_floats(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_vectors,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_vectors[0] == ffi.NULL:
            return []
        try:
            result: list[list[float]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_vectors[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    result.append([out_vectors[0][i][j] for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_float_vectors(out_vectors[0], out_sizes[0], count)

    def read_vector_strings(self, collection: str, attribute: str) -> list[list[str]]:
        """Read string vectors for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_vectors = ffi.new("char****")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_strings(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_vectors,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_vectors[0] == ffi.NULL:
            return []
        try:
            result: list[list[str]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_vectors[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    result.append([ffi.string(out_vectors[0][i][j]).decode("utf-8") for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_string_vectors(out_vectors[0], out_sizes[0], count)

    # -- Vector reads (by ID) ----------------------------------------------------

    def read_vector_integers_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[int]:
        """Read an integer vector for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_integers_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])

    def read_vector_floats_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[float]:
        """Read a float vector for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_floats_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])

    def read_vector_strings_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[str]:
        """Read a string vector for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_strings_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [ffi.string(out_values[0][i]).decode("utf-8") for i in range(count)]
        finally:
            lib.quiver_database_free_string_array(out_values[0], count)

    # -- Set reads (bulk) --------------------------------------------------------

    def read_set_integers(self, collection: str, attribute: str) -> list[list[int]]:
        """Read integer sets for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_sets = ffi.new("int64_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_integers(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_sets,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_sets[0] == ffi.NULL:
            return []
        try:
            result: list[list[int]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_sets[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    result.append([out_sets[0][i][j] for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_integer_vectors(out_sets[0], out_sizes[0], count)

    def read_set_floats(self, collection: str, attribute: str) -> list[list[float]]:
        """Read float sets for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_sets = ffi.new("double***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_floats(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_sets,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_sets[0] == ffi.NULL:
            return []
        try:
            result: list[list[float]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_sets[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    result.append([out_sets[0][i][j] for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_float_vectors(out_sets[0], out_sizes[0], count)

    def read_set_strings(self, collection: str, attribute: str) -> list[list[str]]:
        """Read string sets for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_sets = ffi.new("char****")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_strings(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_sets,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_sets[0] == ffi.NULL:
            return []
        try:
            result: list[list[str]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_sets[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    result.append([ffi.string(out_sets[0][i][j]).decode("utf-8") for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_string_vectors(out_sets[0], out_sizes[0], count)

    # -- Set reads (by ID) -------------------------------------------------------

    def read_set_integers_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[int]:
        """Read an integer set for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_integers_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])

    def read_set_floats_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[float]:
        """Read a float set for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_floats_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])

    def read_set_strings_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[str]:
        """Read a string set for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_strings_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [ffi.string(out_values[0][i]).decode("utf-8") for i in range(count)]
        finally:
            lib.quiver_database_free_string_array(out_values[0], count)

    # -- Metadata get ------------------------------------------------------------

    def get_scalar_metadata(
        self,
        collection: str,
        attribute: str,
    ) -> ScalarMetadata:
        """Get metadata for a scalar attribute."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_scalar_metadata_t*")
        check(
            lib.quiver_database_get_scalar_metadata(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out,
            )
        )
        try:
            return _parse_scalar_metadata(out)
        finally:
            lib.quiver_database_free_scalar_metadata(out)

    def get_vector_metadata(
        self,
        collection: str,
        group_name: str,
    ) -> GroupMetadata:
        """Get metadata for a vector group."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_group_metadata_t*")
        check(
            lib.quiver_database_get_vector_metadata(
                self._ptr,
                collection.encode("utf-8"),
                group_name.encode("utf-8"),
                out,
            )
        )
        try:
            return _parse_group_metadata(out)
        finally:
            lib.quiver_database_free_group_metadata(out)

    def get_set_metadata(
        self,
        collection: str,
        group_name: str,
    ) -> GroupMetadata:
        """Get metadata for a set group."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_group_metadata_t*")
        check(
            lib.quiver_database_get_set_metadata(
                self._ptr,
                collection.encode("utf-8"),
                group_name.encode("utf-8"),
                out,
            )
        )
        try:
            return _parse_group_metadata(out)
        finally:
            lib.quiver_database_free_group_metadata(out)

    def get_time_series_metadata(
        self,
        collection: str,
        group_name: str,
    ) -> GroupMetadata:
        """Get metadata for a time series group."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_group_metadata_t*")
        check(
            lib.quiver_database_get_time_series_metadata(
                self._ptr,
                collection.encode("utf-8"),
                group_name.encode("utf-8"),
                out,
            )
        )
        try:
            return _parse_group_metadata(out)
        finally:
            lib.quiver_database_free_group_metadata(out)

    # -- Metadata list -----------------------------------------------------------

    def list_scalar_attributes(self, collection: str) -> list[ScalarMetadata]:
        """List all scalar attributes for a collection. Returns full metadata objects."""
        self._ensure_open()
        lib = get_lib()
        out_metadata = ffi.new("quiver_scalar_metadata_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_list_scalar_attributes(
                self._ptr,
                collection.encode("utf-8"),
                out_metadata,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_metadata[0] == ffi.NULL:
            return []
        try:
            return [_parse_scalar_metadata(out_metadata[0] + i) for i in range(count)]
        finally:
            lib.quiver_database_free_scalar_metadata_array(out_metadata[0], count)

    def list_vector_groups(self, collection: str) -> list[GroupMetadata]:
        """List all vector groups for a collection. Returns full metadata objects."""
        self._ensure_open()
        lib = get_lib()
        out_metadata = ffi.new("quiver_group_metadata_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_list_vector_groups(
                self._ptr,
                collection.encode("utf-8"),
                out_metadata,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_metadata[0] == ffi.NULL:
            return []
        try:
            return [_parse_group_metadata(out_metadata[0] + i) for i in range(count)]
        finally:
            lib.quiver_database_free_group_metadata_array(out_metadata[0], count)

    def list_set_groups(self, collection: str) -> list[GroupMetadata]:
        """List all set groups for a collection. Returns full metadata objects."""
        self._ensure_open()
        lib = get_lib()
        out_metadata = ffi.new("quiver_group_metadata_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_list_set_groups(
                self._ptr,
                collection.encode("utf-8"),
                out_metadata,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_metadata[0] == ffi.NULL:
            return []
        try:
            return [_parse_group_metadata(out_metadata[0] + i) for i in range(count)]
        finally:
            lib.quiver_database_free_group_metadata_array(out_metadata[0], count)

    def list_time_series_groups(self, collection: str) -> list[GroupMetadata]:
        """List all time series groups for a collection. Returns full metadata objects."""
        self._ensure_open()
        lib = get_lib()
        out_metadata = ffi.new("quiver_group_metadata_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_list_time_series_groups(
                self._ptr,
                collection.encode("utf-8"),
                out_metadata,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_metadata[0] == ffi.NULL:
            return []
        try:
            return [_parse_group_metadata(out_metadata[0] + i) for i in range(count)]
        finally:
            lib.quiver_database_free_group_metadata_array(out_metadata[0], count)

    # -- Time series group operations -------------------------------------------

    def read_time_series_group(
        self,
        collection: str,
        group: str,
        id: int,
    ) -> list[dict]:
        """Read time series data for an element. Returns list of row dicts with typed values."""
        self._ensure_open()
        lib = get_lib()
        out_names = ffi.new("char***")
        out_types = ffi.new("int**")
        out_data = ffi.new("void***")
        out_col_count = ffi.new("size_t*")
        out_row_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_time_series_group(
                self._ptr,
                collection.encode("utf-8"),
                group.encode("utf-8"),
                id,
                out_names,
                out_types,
                out_data,
                out_col_count,
                out_row_count,
            )
        )
        col_count = out_col_count[0]
        row_count = out_row_count[0]
        if col_count == 0 or row_count == 0:
            return []
        try:
            # Build column info
            columns = []
            for c in range(col_count):
                name = ffi.string(out_names[0][c]).decode("utf-8")
                ctype = out_types[0][c]
                columns.append((name, ctype))

            # Transpose columnar to row dicts
            rows: list[dict] = []
            for r in range(row_count):
                row: dict = {}
                for c, (name, ctype) in enumerate(columns):
                    if ctype == 0:  # INTEGER
                        row[name] = ffi.cast("int64_t*", out_data[0][c])[r]
                    elif ctype == 1:  # FLOAT
                        row[name] = ffi.cast("double*", out_data[0][c])[r]
                    else:  # STRING or DATE_TIME
                        row[name] = ffi.string(ffi.cast("char**", out_data[0][c])[r]).decode("utf-8")
                rows.append(row)
            return rows
        finally:
            lib.quiver_database_free_time_series_data(out_names[0], out_types[0], out_data[0], col_count, row_count)

    def update_time_series_group(
        self,
        collection: str,
        group: str,
        id: int,
        rows: list[dict],
    ) -> None:
        """Update time series data for an element. Pass empty list to clear all rows."""
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")

        if not rows:
            # Clear operation
            check(
                lib.quiver_database_update_time_series_group(
                    self._ptr, c_collection, c_group, id, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        # Get metadata for column schema
        metadata = self.get_time_series_metadata(collection, group)
        keepalive, c_col_names, c_col_types, c_col_data, col_count, row_count = _marshal_time_series_columns(
            rows, metadata
        )
        check(
            lib.quiver_database_update_time_series_group(
                self._ptr, c_collection, c_group, id, c_col_names, c_col_types, c_col_data, col_count, row_count
            )
        )

    # -- Time series files ------------------------------------------------------

    def has_time_series_files(self, collection: str) -> bool:
        """Return True if the collection has a time series files table."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("int*")
        check(lib.quiver_database_has_time_series_files(self._ptr, collection.encode("utf-8"), out))
        return bool(out[0])

    def list_time_series_files_columns(self, collection: str) -> list[str]:
        """List column names in the time series files table."""
        self._ensure_open()
        lib = get_lib()
        out_columns = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_list_time_series_files_columns(
                self._ptr, collection.encode("utf-8"), out_columns, out_count
            )
        )
        count = out_count[0]
        if count == 0:
            return []
        try:
            return [ffi.string(out_columns[0][i]).decode("utf-8") for i in range(count)]
        finally:
            lib.quiver_database_free_string_array(out_columns[0], count)

    def read_time_series_files(self, collection: str) -> dict[str, str | None]:
        """Read time series file paths. Returns dict mapping column names to paths (or None)."""
        self._ensure_open()
        lib = get_lib()
        out_columns = ffi.new("char***")
        out_paths = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_time_series_files(
                self._ptr, collection.encode("utf-8"), out_columns, out_paths, out_count
            )
        )
        count = out_count[0]
        if count == 0:
            return {}
        try:
            result: dict[str, str | None] = {}
            for i in range(count):
                col_name = ffi.string(out_columns[0][i]).decode("utf-8")
                if out_paths[0][i] == ffi.NULL:
                    result[col_name] = None
                else:
                    result[col_name] = ffi.string(out_paths[0][i]).decode("utf-8")
            return result
        finally:
            lib.quiver_database_free_time_series_files(out_columns[0], out_paths[0], count)

    def update_time_series_files(
        self,
        collection: str,
        data: dict[str, str | None],
    ) -> None:
        """Update time series file paths. Pass None for a path to clear it."""
        self._ensure_open()
        lib = get_lib()
        count = len(data)
        if count == 0:
            return

        keepalive: list = []
        c_columns = ffi.new("const char*[]", count)
        c_paths = ffi.new("const char*[]", count)
        for i, (col, path) in enumerate(data.items()):
            c_col = ffi.new("char[]", col.encode("utf-8"))
            keepalive.append(c_col)
            c_columns[i] = c_col
            if path is None:
                c_paths[i] = ffi.NULL
            else:
                c_path = ffi.new("char[]", path.encode("utf-8"))
                keepalive.append(c_path)
                c_paths[i] = c_path

        check(
            lib.quiver_database_update_time_series_files(
                self._ptr, collection.encode("utf-8"), c_columns, c_paths, count
            )
        )

    # -- Convenience helpers ---------------------------------------------------

    def read_all_scalars_by_id(self, collection: str, id: int) -> dict:
        """Read all scalar attribute values for an element.

        Returns dict mapping attribute names to typed values.
        Includes id and label. DATE_TIME attributes are parsed to datetime objects.
        """
        self._ensure_open()
        result = {}
        for attr in self.list_scalar_attributes(collection):
            name = attr.name
            if attr.data_type == 0:  # INTEGER
                result[name] = self.read_scalar_integer_by_id(collection, name, id)
            elif attr.data_type == 1:  # FLOAT
                result[name] = self.read_scalar_float_by_id(collection, name, id)
            elif attr.data_type == 3:  # DATE_TIME
                result[name] = self.read_scalar_date_time_by_id(collection, name, id)
            else:  # STRING (2)
                result[name] = self.read_scalar_string_by_id(collection, name, id)
        return result

    def read_all_vectors_by_id(self, collection: str, id: int) -> dict:
        """Read all vector group values for an element (single-column groups).

        Returns dict mapping group names to typed lists.
        DATE_TIME groups are parsed to datetime objects.
        For multi-column groups, use read_vector_group_by_id.
        """
        self._ensure_open()
        result = {}
        for group in self.list_vector_groups(collection):
            name = group.group_name
            dt = group.value_columns[0].data_type if group.value_columns else 2
            if dt == 0:  # INTEGER
                result[name] = self.read_vector_integers_by_id(collection, name, id)
            elif dt == 1:  # FLOAT
                result[name] = self.read_vector_floats_by_id(collection, name, id)
            elif dt == 3:  # DATE_TIME
                result[name] = self.read_vector_date_time_by_id(collection, name, id)
            else:  # STRING (2)
                result[name] = self.read_vector_strings_by_id(collection, name, id)
        return result

    def read_all_sets_by_id(self, collection: str, id: int) -> dict:
        """Read all set group values for an element (single-column groups).

        Returns dict mapping group names to typed lists.
        DATE_TIME groups are parsed to datetime objects.
        For multi-column groups, use read_set_group_by_id.
        """
        self._ensure_open()
        result = {}
        for group in self.list_set_groups(collection):
            name = group.group_name
            dt = group.value_columns[0].data_type if group.value_columns else 2
            if dt == 0:  # INTEGER
                result[name] = self.read_set_integers_by_id(collection, name, id)
            elif dt == 1:  # FLOAT
                result[name] = self.read_set_floats_by_id(collection, name, id)
            elif dt == 3:  # DATE_TIME
                result[name] = self.read_set_date_time_by_id(collection, name, id)
            else:  # STRING (2)
                result[name] = self.read_set_strings_by_id(collection, name, id)
        return result

    def read_element_by_id(self, collection: str, id: int) -> dict:
        """Read all scalar, vector, and set values for an element by ID.

        Returns a flat dict with attribute/column names as keys.
        Multi-column vector/set groups have each column as a separate top-level key.
        DATE_TIME attributes are parsed to datetime objects.
        Returns an empty dict if the element does not exist.
        The 'id' field is excluded (caller already knows it).
        """
        self._ensure_open()
        result = {}

        # 1. Scalars (skip "id" -- caller already knows it)
        for attr in self.list_scalar_attributes(collection):
            name = attr.name
            if name == "id":
                continue
            if attr.data_type == 0:  # INTEGER
                result[name] = self.read_scalar_integer_by_id(collection, name, id)
            elif attr.data_type == 1:  # FLOAT
                result[name] = self.read_scalar_float_by_id(collection, name, id)
            elif attr.data_type == 3:  # DATE_TIME
                result[name] = self.read_scalar_date_time_by_id(collection, name, id)
            else:  # STRING (2)
                result[name] = self.read_scalar_string_by_id(collection, name, id)

        # Early exit for nonexistent element (label is NOT NULL, None means no row)
        if "label" in result and result["label"] is None:
            return {}

        # 2. Vectors -- each column is a separate top-level key
        for group in self.list_vector_groups(collection):
            for col in group.value_columns:
                name = col.name
                dt = col.data_type
                if dt == 0:  # INTEGER
                    result[name] = self.read_vector_integers_by_id(collection, name, id)
                elif dt == 1:  # FLOAT
                    result[name] = self.read_vector_floats_by_id(collection, name, id)
                elif dt == 3:  # DATE_TIME
                    result[name] = self.read_vector_date_time_by_id(collection, name, id)
                else:  # STRING (2)
                    result[name] = self.read_vector_strings_by_id(collection, name, id)

        # 3. Sets -- each column is a separate top-level key
        for group in self.list_set_groups(collection):
            for col in group.value_columns:
                name = col.name
                dt = col.data_type
                if dt == 0:  # INTEGER
                    result[name] = self.read_set_integers_by_id(collection, name, id)
                elif dt == 1:  # FLOAT
                    result[name] = self.read_set_floats_by_id(collection, name, id)
                elif dt == 3:  # DATE_TIME
                    result[name] = self.read_set_date_time_by_id(collection, name, id)
                else:  # STRING (2)
                    result[name] = self.read_set_strings_by_id(collection, name, id)

        return result

    def read_vector_group_by_id(
        self,
        collection: str,
        group: str,
        id: int,
    ) -> list[dict]:
        """Read a multi-column vector group as row dicts.

        Each row is a dict mapping column names to typed values.
        Includes 'vector_index' (0-based) for ordering info.
        Rows are returned in vector_index order.
        DATE_TIME columns are parsed to datetime objects.
        """
        self._ensure_open()
        metadata = self.get_vector_metadata(collection, group)
        columns = metadata.value_columns
        if not columns:
            return []

        column_data = {}
        row_count = 0
        for col in columns:
            name = col.name
            if col.data_type == 0:  # INTEGER
                values = self.read_vector_integers_by_id(collection, name, id)
            elif col.data_type == 1:  # FLOAT
                values = self.read_vector_floats_by_id(collection, name, id)
            elif col.data_type == 3:  # DATE_TIME
                values = self.read_vector_date_time_by_id(collection, name, id)
            else:  # STRING (2)
                values = self.read_vector_strings_by_id(collection, name, id)
            column_data[name] = values
            row_count = len(values)

        return [{"vector_index": i, **{name: vals[i] for name, vals in column_data.items()}} for i in range(row_count)]

    def read_set_group_by_id(
        self,
        collection: str,
        group: str,
        id: int,
    ) -> list[dict]:
        """Read a multi-column set group as row dicts.

        Each row is a dict mapping column names to typed values.
        DATE_TIME columns are parsed to datetime objects.
        """
        self._ensure_open()
        metadata = self.get_set_metadata(collection, group)
        columns = metadata.value_columns
        if not columns:
            return []

        column_data = {}
        row_count = 0
        for col in columns:
            name = col.name
            if col.data_type == 0:  # INTEGER
                values = self.read_set_integers_by_id(collection, name, id)
            elif col.data_type == 1:  # FLOAT
                values = self.read_set_floats_by_id(collection, name, id)
            elif col.data_type == 3:  # DATE_TIME
                values = self.read_set_date_time_by_id(collection, name, id)
            else:  # STRING (2)
                values = self.read_set_strings_by_id(collection, name, id)
            column_data[name] = values
            row_count = len(values)

        return [{name: vals[i] for name, vals in column_data.items()} for i in range(row_count)]

    def __repr__(self) -> str:
        if self._closed:
            return "Database(closed)"
        return f"Database(path={self.path()!r})"


# -- DateTime parsing helper (module-level) ----------------------------------


def _parse_datetime(s: str | None) -> datetime | None:
    """Parse an ISO 8601 datetime string to timezone-aware UTC datetime.

    Returns None if input is None. Raises ValueError on malformed input.
    Both 'YYYY-MM-DDTHH:MM:SS' and 'YYYY-MM-DD HH:MM:SS' formats are supported.
    """
    if s is None:
        return None
    return datetime.fromisoformat(s).replace(tzinfo=timezone.utc)


# -- Parameter marshaling helpers (module-level) -----------------------------


def _marshal_params(params: list) -> tuple:
    """Marshal Python params into C API parallel arrays.

    Returns (keepalive, c_types, c_values) where keepalive must remain
    referenced until the C API call completes.
    """
    n = len(params)
    c_types = ffi.new("int[]", n)
    c_values = ffi.new("void*[]", n)
    keepalive = []  # prevent GC of typed buffers

    for i, p in enumerate(params):
        if p is None:
            c_types[i] = 4  # QUIVER_DATA_TYPE_NULL
            c_values[i] = ffi.NULL
        elif isinstance(p, int):  # bool is subclass of int, handled here
            c_types[i] = 0  # QUIVER_DATA_TYPE_INTEGER
            buf = ffi.new("int64_t*", p)
            keepalive.append(buf)
            c_values[i] = ffi.cast("void*", buf)
        elif isinstance(p, float):
            c_types[i] = 1  # QUIVER_DATA_TYPE_FLOAT
            buf = ffi.new("double*", p)
            keepalive.append(buf)
            c_values[i] = ffi.cast("void*", buf)
        elif isinstance(p, str):
            c_types[i] = 2  # QUIVER_DATA_TYPE_STRING
            c_buf = ffi.new("char[]", p.encode("utf-8"))
            keepalive.append(c_buf)
            c_values[i] = ffi.cast("void*", c_buf)
        else:
            raise TypeError(f"Unsupported parameter type: {type(p).__name__}. Expected int, float, str, or None.")

    return keepalive, c_types, c_values


def _marshal_time_series_columns(
    rows: list[dict],
    metadata: GroupMetadata,
) -> tuple:
    """Marshal Python row dicts into columnar C API arrays for time series update.

    Returns (keepalive, c_col_names, c_col_types, c_col_data, col_count, row_count)
    where keepalive must remain referenced until the C API call completes.
    """
    # Build column schema from metadata: dimension column first, then value columns
    # Dimension column type is always STRING (2) since it's TEXT in SQL
    columns: list[tuple[str, int]] = []
    columns.append((metadata.dimension_column, 2))  # STRING for dimension
    for vc in metadata.value_columns:
        columns.append((vc.name, vc.data_type))

    col_count = len(columns)
    row_count = len(rows)
    keepalive: list = []

    # Validate rows
    for r, row in enumerate(rows):
        for name, col_type in columns:
            if name not in row:
                raise ValueError(f"Row {r} is missing column '{name}'")
            v = row[name]
            if col_type == 0:  # INTEGER
                if type(v) is not int:
                    raise TypeError(f"Column '{name}' expects int, got {type(v).__name__}")
            elif col_type == 1:  # FLOAT
                if type(v) is not float:
                    raise TypeError(f"Column '{name}' expects float, got {type(v).__name__}")
            else:  # STRING or DATE_TIME
                if type(v) is not str:
                    raise TypeError(f"Column '{name}' expects str, got {type(v).__name__}")

    # Build column name array
    c_col_names = ffi.new("const char*[]", col_count)
    for c, (name, _) in enumerate(columns):
        buf = ffi.new("char[]", name.encode("utf-8"))
        keepalive.append(buf)
        c_col_names[c] = buf

    # Build column type array
    c_col_types = ffi.new("int[]", [ct for _, ct in columns])

    # Build column data arrays (transpose rows to columnar)
    c_col_data = ffi.new("void*[]", col_count)
    for c, (name, col_type) in enumerate(columns):
        if col_type == 0:  # INTEGER
            arr = ffi.new("int64_t[]", [row[name] for row in rows])
            keepalive.append(arr)
            c_col_data[c] = ffi.cast("void*", arr)
        elif col_type == 1:  # FLOAT
            arr = ffi.new("double[]", [row[name] for row in rows])
            keepalive.append(arr)
            c_col_data[c] = ffi.cast("void*", arr)
        else:  # STRING or DATE_TIME
            encoded = [row[name].encode("utf-8") for row in rows]
            c_strs = [ffi.new("char[]", e) for e in encoded]
            keepalive.extend(c_strs)
            c_arr = ffi.new("char*[]", c_strs)
            keepalive.append(c_arr)
            c_col_data[c] = ffi.cast("void*", c_arr)

    return keepalive, c_col_names, c_col_types, c_col_data, col_count, row_count


# -- Metadata parsing helpers (module-level) ---------------------------------


def _parse_scalar_metadata(meta) -> ScalarMetadata:
    """Parse a quiver_scalar_metadata_t struct into a ScalarMetadata dataclass."""
    return ScalarMetadata(
        name=decode_string(meta.name),
        data_type=int(meta.data_type),
        not_null=bool(meta.not_null),
        primary_key=bool(meta.primary_key),
        default_value=decode_string_or_none(meta.default_value),
        is_foreign_key=bool(meta.is_foreign_key),
        references_collection=decode_string_or_none(meta.references_collection),
        references_column=decode_string_or_none(meta.references_column),
    )


def _parse_group_metadata(meta) -> GroupMetadata:
    """Parse a quiver_group_metadata_t struct into a GroupMetadata dataclass."""
    value_columns: list[ScalarMetadata] = []
    for i in range(meta.value_column_count):
        value_columns.append(_parse_scalar_metadata(meta.value_columns[i]))
    return GroupMetadata(
        group_name=decode_string(meta.group_name),
        dimension_column="" if meta.dimension_column == ffi.NULL else decode_string(meta.dimension_column),
        value_columns=value_columns,
    )
