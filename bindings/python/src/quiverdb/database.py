from __future__ import annotations

import re
import warnings
from contextlib import contextmanager
from datetime import datetime, timezone
from typing import overload

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, column_data_type, decode_string, decode_string_or_none, format_datetime
from quiverdb.database_csv_export import DatabaseCSVExport
from quiverdb.database_csv_import import DatabaseCSVImport
from quiverdb.element import Element
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import (
    DataType,
    GroupMetadata,
    ScalarMetadata,
)


class Database(DatabaseCSVExport, DatabaseCSVImport):
    """Thin Python wrapper around the Quiver C API database handle."""

    def __init__(self, ptr) -> None:
        self._ptr = ptr
        self._closed = False

    @staticmethod
    def _make_options(read_only: bool, console_level: int | None):
        lib = get_lib()
        options = ffi.new("quiver_database_options_t*")
        options[0] = lib.quiver_database_options_default()
        options.read_only = 1 if read_only else 0
        if console_level is not None:
            options.console_level = console_level
        return options

    @staticmethod
    def from_schema(
        db_path: str,
        schema_path: str,
        *,
        read_only: bool = False,
        console_level: int | None = None,
    ) -> Database:
        """Create a database from a SQL schema file.

        console_level takes a LogLevel constant (e.g. LogLevel.OFF).
        """
        lib = get_lib()
        options = Database._make_options(read_only, console_level)
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
    def from_migrations(
        db_path: str,
        migrations_path: str,
        *,
        read_only: bool = False,
        console_level: int | None = None,
    ) -> Database:
        """Create a database using a migrations directory."""
        lib = get_lib()
        options = Database._make_options(read_only, console_level)
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

    @staticmethod
    def validate_migrations(migrations_path: str) -> None:
        """Apply every up.sql then every down.sql in a migrations directory, in-memory.

        Raises if the round trip leaves any table behind.
        """
        lib = get_lib()
        check(lib.quiver_database_validate_migrations(migrations_path.encode("utf-8")))

    @staticmethod
    def open(
        db_path: str,
        *,
        read_only: bool = False,
        console_level: int | None = None,
    ) -> Database:
        """Open an existing database file."""
        lib = get_lib()
        options = Database._make_options(read_only, console_level)
        out_db = ffi.new("quiver_database_t**")
        check(
            lib.quiver_database_open(
                db_path.encode("utf-8"),
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

    # -- Schema inspection ------------------------------------------------------

    def describe(self) -> str:
        """Return a human-readable report of every collection in the database."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("char**")
        check(lib.quiver_database_describe(self._ptr, out))
        try:
            return ffi.string(out[0]).decode("utf-8")
        finally:
            lib.quiver_database_free_string(out[0])

    def describe_collection(self, collection: str) -> str:
        """Return a human-readable report of a single collection's structure."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("char**")
        check(lib.quiver_database_describe_collection(self._ptr, collection.encode("utf-8"), out))
        try:
            return ffi.string(out[0]).decode("utf-8")
        finally:
            lib.quiver_database_free_string(out[0])

    def summarize_collection(self, collection: str) -> str:
        """Return a human-readable value-statistics report for a single collection.

        Reports per-scalar null/non-null counts and integer value distributions,
        and per-group empty/non-empty element counts.
        """
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("char**")
        check(lib.quiver_database_summarize_collection(self._ptr, collection.encode("utf-8"), out))
        try:
            return ffi.string(out[0]).decode("utf-8")
        finally:
            lib.quiver_database_free_string(out[0])

    def create_element(self, collection: str, /, **kwargs: object) -> int:
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

    def update_element(self, collection: str, id: int, /, **kwargs: object) -> None:
        """Update an existing element's attributes.

        `collection` and `id` are positional-only so that an `id` in kwargs (e.g. the dict
        from `read_scalars_by_id`) is written as an attribute instead of colliding with this parameter.
        """
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

    def update_element_by_label(self, collection: str, label: str, /, **kwargs: object) -> None:
        """Update an existing element's attributes, addressed by label.

        `collection` and `label` are positional-only so that `label="New"` in kwargs
        renames the element instead of colliding with this parameter.
        """
        self._ensure_open()
        elem = Element()
        try:
            for name, value in kwargs.items():
                elem.set(name, value)
            lib = get_lib()
            check(
                lib.quiver_database_update_element_by_label(
                    self._ptr,
                    collection.encode("utf-8"),
                    label.encode("utf-8"),
                    elem._ptr,
                )
            )
        finally:
            elem.destroy()

    def update_relation(
        self,
        collection_from: str,
        collection_to: str,
        relation_type: str,
        id: int,
        target_label: str | None,
    ) -> None:
        """Point one scalar foreign-key relation at the element labeled `target_label`.

        The column is derived as `collection_to.lower() + "_" + relation_type`.
        `None` clears the relation.
        """
        self._ensure_open()
        lib = get_lib()
        check(
            lib.quiver_database_update_relation(
                self._ptr,
                collection_from.encode("utf-8"),
                collection_to.encode("utf-8"),
                relation_type.encode("utf-8"),
                id,
                ffi.NULL if target_label is None else target_label.encode("utf-8"),
            )
        )

    def update_relation_by_label(
        self,
        collection_from: str,
        collection_to: str,
        relation_type: str,
        label: str,
        target_label: str | None,
    ) -> None:
        """Label-addressed counterpart of update_relation."""
        self._ensure_open()
        lib = get_lib()
        check(
            lib.quiver_database_update_relation_by_label(
                self._ptr,
                collection_from.encode("utf-8"),
                collection_to.encode("utf-8"),
                relation_type.encode("utf-8"),
                label.encode("utf-8"),
                ffi.NULL if target_label is None else target_label.encode("utf-8"),
            )
        )

    def delete_element(self, collection: str, id: int) -> None:
        """Delete an element by ID."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_delete_element(self._ptr, collection.encode("utf-8"), id))

    def delete_element_by_label(self, collection: str, label: str) -> None:
        """Delete an element by label."""
        self._ensure_open()
        lib = get_lib()
        check(
            lib.quiver_database_delete_element_by_label(
                self._ptr,
                collection.encode("utf-8"),
                label.encode("utf-8"),
            )
        )

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
        out = ffi.new("int*")
        check(lib.quiver_database_in_transaction(self._ptr, out))
        return out[0] != 0

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

    def begin_dry_run(self) -> None:
        """Begin a dry run: a transaction that end_dry_run always rolls back."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_begin_dry_run(self._ptr))

    def end_dry_run(self) -> None:
        """End the active dry run, rolling back everything it covered."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_end_dry_run(self._ptr))

    def in_dry_run(self) -> bool:
        """Return True if a dry run is currently active."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("int*")
        check(lib.quiver_database_in_dry_run(self._ptr, out))
        return out[0] != 0

    @contextmanager
    def dry_run(self):
        """Execute operations inside a transaction that is always rolled back.

        While the dry run is active, begin_transaction/commit/rollback are absorbed (no-ops), so
        code that manages its own transactions composes instead of erroring on a nested BEGIN. A
        nested rollback is therefore not partial -- everything is undone when the dry run ends.
        """
        self.begin_dry_run()
        try:
            yield self
        except BaseException:
            try:
                self.end_dry_run()
            except Exception:
                pass  # Best-effort only while an exception is already in flight
            raise
        # On the success path the rollback is this wrapper's whole promise -- let a failure surface.
        self.end_dry_run()

    # -- Query operations -------------------------------------------------------

    def query_string(self, sql: str, *, parameters: list | None = None) -> str | None:
        """Execute SQL and return the first column of the first row as str, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")
        parameters = parameters or []
        keepalive, c_types, c_values = _marshal_params(parameters)
        check(
            lib.quiver_database_query_string(
                self._ptr,
                sql.encode("utf-8"),
                c_types,
                c_values,
                len(parameters),
                out_value,
                out_has,
            )
        )

        if out_has[0] == 0 or out_value[0] == ffi.NULL:
            return None
        try:
            return ffi.string(out_value[0]).decode("utf-8")
        finally:
            lib.quiver_database_free_string(out_value[0])

    def query_integer(self, sql: str, *, parameters: list | None = None) -> int | None:
        """Execute SQL and return the first column of the first row as int, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("int64_t*")
        out_has = ffi.new("int*")
        parameters = parameters or []
        keepalive, c_types, c_values = _marshal_params(parameters)
        check(
            lib.quiver_database_query_integer(
                self._ptr,
                sql.encode("utf-8"),
                c_types,
                c_values,
                len(parameters),
                out_value,
                out_has,
            )
        )

        if out_has[0] == 0:
            return None
        return out_value[0]

    def query_boolean(self, sql: str, *, parameters: list | None = None) -> bool | None:
        """Execute SQL and convert the first integer result to bool, or return None."""
        return _integer_to_boolean(self.query_integer(sql, parameters=parameters))

    def query_float(self, sql: str, *, parameters: list | None = None) -> float | None:
        """Execute SQL and return the first column of the first row as float, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("double*")
        out_has = ffi.new("int*")
        parameters = parameters or []
        keepalive, c_types, c_values = _marshal_params(parameters)
        check(
            lib.quiver_database_query_float(
                self._ptr,
                sql.encode("utf-8"),
                c_types,
                c_values,
                len(parameters),
                out_value,
                out_has,
            )
        )

        if out_has[0] == 0:
            return None
        return out_value[0]

    def query_date_time(self, sql: str, *, parameters: list | None = None) -> datetime | None:
        """Execute SQL and return the first column of the first row as datetime, or None."""
        result = self.query_string(sql, parameters=parameters)
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
        return _parse_datetime(self.read_scalar_string_by_id(collection, attribute, id), collection, attribute)

    def read_vector_date_times_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[datetime | None]:
        """Read datetime values from a vector. Timezone-aware UTC datetimes; a NULL cell is None."""
        return [
            _parse_datetime(s, collection, attribute) for s in self.read_vector_strings_by_id(collection, attribute, id)
        ]

    def read_set_date_times_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[datetime | None]:
        """Read datetime values from a set. Timezone-aware UTC datetimes; a NULL cell is None."""
        return [
            _parse_datetime(s, collection, attribute) for s in self.read_set_strings_by_id(collection, attribute, id)
        ]

    # -- Scalar reads (bulk) --------------------------------------------------

    def read_scalar_integers(self, collection: str, attribute: str) -> list[int | None]:
        """Read all integer values for a scalar attribute. One entry per element; NULL is None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_scalar_integers(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            mask = out_mask[0]
            return [out_values[0][i] if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])
            lib.quiver_database_free_mask(out_mask[0])

    def read_scalar_booleans(self, collection: str, attribute: str) -> list[bool | None]:
        """Read boolean values stored as integer scalars. SQL NULL values become None."""
        return [
            _integer_to_boolean(value, collection, attribute)
            for value in self.read_scalar_integers(collection, attribute)
        ]

    def read_scalar_floats(self, collection: str, attribute: str) -> list[float | None]:
        """Read all float values for a scalar attribute. One entry per element; NULL is None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_scalar_floats(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            mask = out_mask[0]
            return [out_values[0][i] if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])
            lib.quiver_database_free_mask(out_mask[0])

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

    def read_scalar_date_times(self, collection: str, attribute: str) -> list[datetime | None]:
        """Read all datetime scalar values. SQL NULL values become None."""
        return [
            _parse_datetime(value, collection, attribute) for value in self.read_scalar_strings(collection, attribute)
        ]

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

    def read_scalar_boolean_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> bool | None:
        """Read a boolean stored as an integer scalar. Returns None if absent."""
        return _integer_to_boolean(self.read_scalar_integer_by_id(collection, attribute, id), collection, attribute)

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
            lib.quiver_database_free_string(out_value[0])

    # -- Element Ids -----------------------------------------------------------

    def read_element_ids(self, collection: str) -> list[int]:
        """Read all element Ids in a collection."""
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

    def number_of_elements(self, collection: str) -> int:
        """Return the current number of elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("int64_t*")
        check(lib.quiver_database_number_of_elements(self._ptr, collection.encode("utf-8"), out))
        return out[0]

    # -- Vector reads (bulk) -----------------------------------------------------

    def read_vector_integers(self, collection: str, attribute: str) -> list[list[int | None]]:
        """Read integer vectors for all elements in a collection. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_vectors = ffi.new("int64_t***")
        out_masks = ffi.new("uint8_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_integers(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_vectors,
                out_masks,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_vectors[0] == ffi.NULL:
            return []
        try:
            result: list[list[int | None]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_vectors[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    values, mask = out_vectors[0][i], out_masks[0][i]
                    result.append([values[j] if mask[j] else None for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_integer_vectors(out_vectors[0], out_sizes[0], count)
            lib.quiver_database_free_masks(out_masks[0], count)

    def read_vector_booleans(self, collection: str, attribute: str) -> list[list[bool | None]]:
        """Read boolean vectors stored as integer vectors. A NULL cell is None."""
        return [
            [_integer_to_boolean(value, collection, attribute) for value in values]
            for values in self.read_vector_integers(collection, attribute)
        ]

    def read_vector_floats(self, collection: str, attribute: str) -> list[list[float | None]]:
        """Read float vectors for all elements in a collection. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_vectors = ffi.new("double***")
        out_masks = ffi.new("uint8_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_floats(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_vectors,
                out_masks,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_vectors[0] == ffi.NULL:
            return []
        try:
            result: list[list[float | None]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_vectors[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    values, mask = out_vectors[0][i], out_masks[0][i]
                    result.append([values[j] if mask[j] else None for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_float_vectors(out_vectors[0], out_sizes[0], count)
            lib.quiver_database_free_masks(out_masks[0], count)

    def read_vector_strings(self, collection: str, attribute: str) -> list[list[str | None]]:
        """Read string vectors for all elements in a collection. A NULL cell is None."""
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
            result: list[list[str | None]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_vectors[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    cells = out_vectors[0][i]
                    result.append(
                        [None if cells[j] == ffi.NULL else ffi.string(cells[j]).decode("utf-8") for j in range(size)]
                    )
            return result
        finally:
            lib.quiver_database_free_string_vectors(out_vectors[0], out_sizes[0], count)

    def read_vector_date_times(self, collection: str, attribute: str) -> list[list[datetime | None]]:
        """Read datetime vectors stored as string vectors. A NULL cell is None."""
        return [
            [_parse_datetime(value, collection, attribute) for value in values]
            for values in self.read_vector_strings(collection, attribute)
        ]

    # -- Vector reads (by ID) ----------------------------------------------------

    def read_vector_integers_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[int | None]:
        """Read an integer vector for a single element. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_integers_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            mask = out_mask[0]
            return [out_values[0][i] if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])
            lib.quiver_database_free_mask(out_mask[0])

    def read_vector_booleans_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[bool | None]:
        """Read a boolean vector stored as integers for one element. A NULL cell is None."""
        return [
            _integer_to_boolean(value, collection, attribute)
            for value in self.read_vector_integers_by_id(collection, attribute, id)
        ]

    def read_vector_floats_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[float | None]:
        """Read a float vector for a single element. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_floats_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            mask = out_mask[0]
            return [out_values[0][i] if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])
            lib.quiver_database_free_mask(out_mask[0])

    def read_vector_strings_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[str | None]:
        """Read a string vector for a single element. A NULL cell is None."""
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
            cells = out_values[0]
            return [None if cells[i] == ffi.NULL else ffi.string(cells[i]).decode("utf-8") for i in range(count)]
        finally:
            lib.quiver_database_free_string_array(out_values[0], count)

    # -- Set reads (bulk) --------------------------------------------------------

    def read_set_integers(self, collection: str, attribute: str) -> list[list[int | None]]:
        """Read integer sets for all elements in a collection. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_sets = ffi.new("int64_t***")
        out_masks = ffi.new("uint8_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_integers(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_sets,
                out_masks,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_sets[0] == ffi.NULL:
            return []
        try:
            result: list[list[int | None]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_sets[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    values, mask = out_sets[0][i], out_masks[0][i]
                    result.append([values[j] if mask[j] else None for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_integer_vectors(out_sets[0], out_sizes[0], count)
            lib.quiver_database_free_masks(out_masks[0], count)

    def read_set_booleans(self, collection: str, attribute: str) -> list[list[bool | None]]:
        """Read boolean sets stored as integer sets. A NULL cell is None."""
        return [
            [_integer_to_boolean(value, collection, attribute) for value in values]
            for values in self.read_set_integers(collection, attribute)
        ]

    def read_set_floats(self, collection: str, attribute: str) -> list[list[float | None]]:
        """Read float sets for all elements in a collection. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_sets = ffi.new("double***")
        out_masks = ffi.new("uint8_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_floats(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                out_sets,
                out_masks,
                out_sizes,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_sets[0] == ffi.NULL:
            return []
        try:
            result: list[list[float | None]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_sets[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    values, mask = out_sets[0][i], out_masks[0][i]
                    result.append([values[j] if mask[j] else None for j in range(size)])
            return result
        finally:
            lib.quiver_database_free_float_vectors(out_sets[0], out_sizes[0], count)
            lib.quiver_database_free_masks(out_masks[0], count)

    def read_set_strings(self, collection: str, attribute: str) -> list[list[str | None]]:
        """Read string sets for all elements in a collection. A NULL cell is None."""
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
            result: list[list[str | None]] = []
            for i in range(count):
                size = out_sizes[0][i]
                if out_sets[0][i] == ffi.NULL or size == 0:
                    result.append([])
                else:
                    cells = out_sets[0][i]
                    result.append(
                        [None if cells[j] == ffi.NULL else ffi.string(cells[j]).decode("utf-8") for j in range(size)]
                    )
            return result
        finally:
            lib.quiver_database_free_string_vectors(out_sets[0], out_sizes[0], count)

    def read_set_date_times(self, collection: str, attribute: str) -> list[list[datetime | None]]:
        """Read datetime sets stored as string sets. A NULL cell is None."""
        return [
            [_parse_datetime(value, collection, attribute) for value in values]
            for values in self.read_set_strings(collection, attribute)
        ]

    # -- Set reads (by ID) -------------------------------------------------------

    def read_set_integers_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[int | None]:
        """Read an integer set for a single element. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_integers_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            mask = out_mask[0]
            return [out_values[0][i] if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])
            lib.quiver_database_free_mask(out_mask[0])

    def read_set_booleans_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[bool | None]:
        """Read a boolean set stored as integers for one element. A NULL cell is None."""
        return [
            _integer_to_boolean(value, collection, attribute)
            for value in self.read_set_integers_by_id(collection, attribute, id)
        ]

    def read_set_floats_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[float | None]:
        """Read a float set for a single element. A NULL cell is None."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_floats_by_id(
                self._ptr,
                collection.encode("utf-8"),
                attribute.encode("utf-8"),
                id,
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            mask = out_mask[0]
            return [out_values[0][i] if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])
            lib.quiver_database_free_mask(out_mask[0])

    def read_set_strings_by_id(
        self,
        collection: str,
        attribute: str,
        id: int,
    ) -> list[str | None]:
        """Read a string set for a single element. A NULL cell is None."""
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
            cells = out_values[0]
            return [None if cells[i] == ffi.NULL else ffi.string(cells[i]).decode("utf-8") for i in range(count)]
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
    ) -> dict[str, list]:
        """Read time series data for an element as column lists keyed by column name.

        The dimension column (e.g. date_time) is parsed into timezone-aware UTC
        datetimes; value columns keep their schema types.
        """
        self._ensure_open()
        lib = get_lib()
        out_names = ffi.new("char***")
        out_types = ffi.new("int**")
        out_data = ffi.new("void***")
        out_has_value = ffi.new("uint8_t***")
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
                out_has_value,
                out_col_count,
                out_row_count,
            )
        )
        col_count = out_col_count[0]
        row_count = out_row_count[0]
        if col_count == 0 or row_count == 0:
            return {}
        try:
            dim_col = self.get_time_series_metadata(collection, group).dimension_column

            # Per-cell NULL mask: mask[c][r] == 0 means SQL NULL, surfaced as None. The
            # dimension column's mask is always all 1, so it stays dense.
            result: dict[str, list] = {}
            for c in range(col_count):
                name = ffi.string(out_names[0][c]).decode("utf-8")
                ctype = out_types[0][c]
                mask = out_has_value[0][c]
                if ctype == DataType.INTEGER:
                    int_ptr = ffi.cast("int64_t*", out_data[0][c])
                    result[name] = [int_ptr[r] if mask[r] else None for r in range(row_count)]
                elif ctype == DataType.FLOAT:
                    float_ptr = ffi.cast("double*", out_data[0][c])
                    result[name] = [float_ptr[r] if mask[r] else None for r in range(row_count)]
                else:  # STRING or DATE_TIME
                    str_ptr = ffi.cast("char**", out_data[0][c])
                    if name == dim_col:
                        result[name] = [
                            _parse_datetime(ffi.string(str_ptr[r]).decode("utf-8"), collection, name)
                            for r in range(row_count)
                        ]
                    else:
                        result[name] = [
                            ffi.string(str_ptr[r]).decode("utf-8") if mask[r] else None for r in range(row_count)
                        ]
            return result
        finally:
            lib.quiver_database_free_time_series_data(
                out_names[0], out_types[0], out_data[0], out_has_value[0], col_count, row_count
            )

    def read_time_series_row(
        self,
        collection: str,
        group: str,
        attribute: str,
        date_time: datetime,
    ) -> list:
        """Read one value per element for a time series attribute at a given date.

        Uses "last non-null value at or before date_time" lookup semantics. An aware
        date_time is converted to UTC first; a naive one is taken as UTC.
        Entries are typed by the column (int, float, or str); elements with no
        matching data yield None. Raises QuiverError for a group with more than
        one dimension column; use read_time_series_group for those.
        """
        self._ensure_open()
        lib = get_lib()
        out_data_type = ffi.new("int*")
        out_values = ffi.new("void**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_time_series_row(
                self._ptr,
                collection.encode("utf-8"),
                group.encode("utf-8"),
                attribute.encode("utf-8"),
                format_datetime(date_time).encode("utf-8"),
                out_data_type,
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        data_type = out_data_type[0]
        # mask[i] falsy: no data at or before date_time; the data slot is a placeholder
        mask = out_mask[0]
        if data_type == DataType.INTEGER:
            int_ptr = ffi.cast("int64_t*", out_values[0])
            try:
                return [int_ptr[i] if mask[i] else None for i in range(count)]
            finally:
                lib.quiver_database_free_integer_array(int_ptr)
                lib.quiver_database_free_mask(mask)
        if data_type == DataType.FLOAT:
            float_ptr = ffi.cast("double*", out_values[0])
            try:
                return [float_ptr[i] if mask[i] else None for i in range(count)]
            finally:
                lib.quiver_database_free_float_array(float_ptr)
                lib.quiver_database_free_mask(mask)
        # STRING or DATE_TIME; never ffi.string a masked-out (NULL) pointer
        str_ptr = ffi.cast("char**", out_values[0])
        try:
            return [ffi.string(str_ptr[i]).decode("utf-8") if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_string_array(str_ptr, count)
            lib.quiver_database_free_mask(mask)

    def update_time_series_group(
        self,
        collection: str,
        group: str,
        id: int,
        data: dict[str, list],
    ) -> None:
        """Update time series data for an element from column lists keyed by name.

        Pass an empty dict to clear all rows. datetime values are formatted to
        ISO strings (an aware value is converted to UTC); integers are accepted
        for REAL columns.
        """
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")

        if not data:
            # Clear operation
            check(
                lib.quiver_database_update_time_series_group(
                    self._ptr, c_collection, c_group, id, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count = _marshal_group_columns(
            data
        )
        check(
            lib.quiver_database_update_time_series_group(
                self._ptr,
                c_collection,
                c_group,
                id,
                c_col_names,
                c_col_types,
                c_col_data,
                c_col_has_value,
                col_count,
                row_count,
            )
        )

    def update_time_series_group_by_label(
        self,
        collection: str,
        group: str,
        label: str,
        data: dict[str, list],
    ) -> None:
        """Label-addressed counterpart of update_time_series_group."""
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")
        c_label = label.encode("utf-8")

        if not data:
            check(
                lib.quiver_database_update_time_series_group_by_label(
                    self._ptr, c_collection, c_group, c_label, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count = _marshal_group_columns(
            data
        )
        check(
            lib.quiver_database_update_time_series_group_by_label(
                self._ptr,
                c_collection,
                c_group,
                c_label,
                c_col_names,
                c_col_types,
                c_col_data,
                c_col_has_value,
                col_count,
                row_count,
            )
        )

    def update_vector_group(
        self,
        collection: str,
        group: str,
        id: int,
        data: dict[str, list],
    ) -> None:
        """Replace all of an element's rows in one vector group, from column lists keyed by name.

        Pass an empty dict to clear the group. Prefer this over passing the group's columns
        through update_element when a column name is shared by two groups of the collection
        (legal for foreign keys): (collection, group) names exactly one table, a column name
        alone does not, and update_element writes an ambiguous column to every match.
        """
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")

        if not data:
            check(
                lib.quiver_database_update_vector_group(
                    self._ptr, c_collection, c_group, id, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count = _marshal_group_columns(
            data
        )
        check(
            lib.quiver_database_update_vector_group(
                self._ptr,
                c_collection,
                c_group,
                id,
                c_col_names,
                c_col_types,
                c_col_data,
                c_col_has_value,
                col_count,
                row_count,
            )
        )

    def update_vector_group_by_label(
        self,
        collection: str,
        group: str,
        label: str,
        data: dict[str, list],
    ) -> None:
        """Label-addressed counterpart of update_vector_group."""
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")
        c_label = label.encode("utf-8")

        if not data:
            check(
                lib.quiver_database_update_vector_group_by_label(
                    self._ptr, c_collection, c_group, c_label, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count = _marshal_group_columns(
            data
        )
        check(
            lib.quiver_database_update_vector_group_by_label(
                self._ptr,
                c_collection,
                c_group,
                c_label,
                c_col_names,
                c_col_types,
                c_col_data,
                c_col_has_value,
                col_count,
                row_count,
            )
        )

    def update_set_group(
        self,
        collection: str,
        group: str,
        id: int,
        data: dict[str, list],
    ) -> None:
        """Replace all of an element's rows in one set group, from column lists keyed by name.

        Pass an empty dict to clear the group. See update_vector_group for why this is
        preferred over update_element for groups whose column names are shared.
        """
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")

        if not data:
            check(
                lib.quiver_database_update_set_group(
                    self._ptr, c_collection, c_group, id, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count = _marshal_group_columns(
            data
        )
        check(
            lib.quiver_database_update_set_group(
                self._ptr,
                c_collection,
                c_group,
                id,
                c_col_names,
                c_col_types,
                c_col_data,
                c_col_has_value,
                col_count,
                row_count,
            )
        )

    def update_set_group_by_label(
        self,
        collection: str,
        group: str,
        label: str,
        data: dict[str, list],
    ) -> None:
        """Label-addressed counterpart of update_set_group."""
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")
        c_label = label.encode("utf-8")

        if not data:
            check(
                lib.quiver_database_update_set_group_by_label(
                    self._ptr, c_collection, c_group, c_label, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL, 0, 0
                )
            )
            return

        keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count = _marshal_group_columns(
            data
        )
        check(
            lib.quiver_database_update_set_group_by_label(
                self._ptr,
                c_collection,
                c_group,
                c_label,
                c_col_names,
                c_col_types,
                c_col_data,
                c_col_has_value,
                col_count,
                row_count,
            )
        )

    def upsert_time_series_row(self, collection: str, group: str, id: int, /, **kwargs: object) -> None:
        """Insert or upsert a single time series row for an element.

        Keyword arguments map column names to values. Every dimension column (e.g.
        date_time) must be provided; a value column left out gets its column default
        (NULL if it declares none; a NOT NULL column with no default must be provided).
        Type dispatch uses isinstance: bool -> INTEGER (0/1), int -> INTEGER,
        float -> FLOAT, str -> STRING, datetime -> STRING (an aware value is converted
        to UTC); integers are accepted for REAL columns.
        Dict unpacking is supported: db.upsert_time_series_row("Col", "grp", 1, **row_dict).
        """
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")

        keepalive, c_col_names, c_col_types, c_col_data, col_count = _marshal_row_columns(kwargs)
        check(
            lib.quiver_database_upsert_time_series_row(
                self._ptr, c_collection, c_group, id, c_col_names, c_col_types, c_col_data, col_count
            )
        )

    def upsert_time_series_row_by_label(self, collection: str, group: str, label: str, /, **kwargs: object) -> None:
        """Label-addressed counterpart of upsert_time_series_row."""
        self._ensure_open()
        lib = get_lib()
        c_collection = collection.encode("utf-8")
        c_group = group.encode("utf-8")
        c_label = label.encode("utf-8")

        keepalive, c_col_names, c_col_types, c_col_data, col_count = _marshal_row_columns(kwargs)
        check(
            lib.quiver_database_upsert_time_series_row_by_label(
                self._ptr, c_collection, c_group, c_label, c_col_names, c_col_types, c_col_data, col_count
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
        """Update time series file paths.

        Only the named columns are written: pass None for a path to clear that column, and leave a
        column out of `data` to keep its current value.
        """
        self._ensure_open()
        lib = get_lib()
        count = len(data)

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

    def read_scalars_by_id(self, collection: str, id: int) -> dict:
        """Read all scalar attribute values for an element.

        Returns dict mapping attribute names to typed values.
        Includes id and label. DATE_TIME attributes are parsed to datetime objects.
        """
        self._ensure_open()
        result = {}
        for attr in self.list_scalar_attributes(collection):
            name = attr.name
            if attr.data_type == DataType.INTEGER:
                result[name] = self.read_scalar_integer_by_id(collection, name, id)
            elif attr.data_type == DataType.FLOAT:
                result[name] = self.read_scalar_float_by_id(collection, name, id)
            elif attr.data_type == DataType.DATE_TIME:
                result[name] = self.read_scalar_date_time_by_id(collection, name, id)
            else:  # STRING
                result[name] = self.read_scalar_string_by_id(collection, name, id)
        return result

    def read_vectors_by_id(self, collection: str, id: int) -> dict:
        """Read all vector column values for an element.

        Returns dict mapping column names to typed lists.
        DATE_TIME columns are parsed to datetime objects.
        """
        self._ensure_open()
        result = {}
        for group in self.list_vector_groups(collection):
            for col in group.value_columns:
                name = col.name
                dt = col.data_type
                if dt == DataType.INTEGER:
                    result[name] = self.read_vector_integers_by_id(collection, name, id)
                elif dt == DataType.FLOAT:
                    result[name] = self.read_vector_floats_by_id(collection, name, id)
                elif dt == DataType.DATE_TIME:
                    result[name] = self.read_vector_date_times_by_id(collection, name, id)
                else:  # STRING
                    result[name] = self.read_vector_strings_by_id(collection, name, id)
        return result

    def read_sets_by_id(self, collection: str, id: int) -> dict:
        """Read all set column values for an element.

        Returns dict mapping column names to typed lists.
        DATE_TIME columns are parsed to datetime objects.
        """
        self._ensure_open()
        result = {}
        for group in self.list_set_groups(collection):
            for col in group.value_columns:
                name = col.name
                dt = col.data_type
                if dt == DataType.INTEGER:
                    result[name] = self.read_set_integers_by_id(collection, name, id)
                elif dt == DataType.FLOAT:
                    result[name] = self.read_set_floats_by_id(collection, name, id)
                elif dt == DataType.DATE_TIME:
                    result[name] = self.read_set_date_times_by_id(collection, name, id)
                else:  # STRING
                    result[name] = self.read_set_strings_by_id(collection, name, id)
        return result

    def read_element_by_id(self, collection: str, id: int) -> dict:
        """Read all scalar, vector, and set attribute values for an element.

        Returns a single dict merging all attribute types.
        """
        result = self.read_scalars_by_id(collection, id)
        result.update(self.read_vectors_by_id(collection, id))
        result.update(self.read_sets_by_id(collection, id))
        return result

    def read_vector_group_by_id(
        self,
        collection: str,
        group: str,
        id: int,
    ) -> list[dict]:
        """Read a multi-column vector group as row dicts, in vector_index order.

        Each row maps column names to typed values and adds a synthetic 0-based 'vector_index'.
        One native read of the named group's own table: a SQL NULL cell is None in its row, and a
        column name another group shares still reads this group. DATE_TIME columns are parsed to
        datetime objects.
        """
        self._ensure_open()
        lib = get_lib()
        out_names = ffi.new("char***")
        out_types = ffi.new("int**")
        out_data = ffi.new("void***")
        out_has_value = ffi.new("uint8_t***")
        out_col_count = ffi.new("size_t*")
        out_row_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_group_by_id(
                self._ptr,
                collection.encode("utf-8"),
                group.encode("utf-8"),
                id,
                out_names,
                out_types,
                out_data,
                out_has_value,
                out_col_count,
                out_row_count,
            )
        )
        rows = _decode_group_rows(
            collection, out_names, out_types, out_data, out_has_value, out_col_count[0], out_row_count[0]
        )
        return [{"vector_index": i, **row} for i, row in enumerate(rows)]

    def read_set_group_by_id(
        self,
        collection: str,
        group: str,
        id: int,
    ) -> list[dict]:
        """Read a multi-column set group as row dicts.

        One native read of the named group's own table: a SQL NULL cell is None in its row, and a
        column name another group shares still reads this group. DATE_TIME columns are parsed to
        datetime objects. Row order is consistent across every reader of the group, otherwise
        unspecified.
        """
        self._ensure_open()
        lib = get_lib()
        out_names = ffi.new("char***")
        out_types = ffi.new("int**")
        out_data = ffi.new("void***")
        out_has_value = ffi.new("uint8_t***")
        out_col_count = ffi.new("size_t*")
        out_row_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_set_group_by_id(
                self._ptr,
                collection.encode("utf-8"),
                group.encode("utf-8"),
                id,
                out_names,
                out_types,
                out_data,
                out_has_value,
                out_col_count,
                out_row_count,
            )
        )
        return _decode_group_rows(
            collection, out_names, out_types, out_data, out_has_value, out_col_count[0], out_row_count[0]
        )

    def __repr__(self) -> str:
        if self._closed:
            return "Database(closed)"
        return f"Database(path={self.path()!r})"


# -- DateTime parsing helper (module-level) ----------------------------------


# The core's DATE_TIME grammar (datetime::is_valid_iso8601, src/utils/datetime.h): YYYY-MM-DD
# optionally followed by THH:MM:SS or ' HH:MM:SS', every field fixed-width and zero-padded, year
# 0001-9999. fromisoformat is wider than that -- it accepts 'YYYYMMDD', a 'Z' suffix and a UTC
# offset, none of which Julia's parser reads -- so gate the shape before parsing. Rejecting the
# offset forms is also what makes the replace(tzinfo=utc) below correct: everything reaching it is
# naive, so there is no offset to convert away.
_DATE_TIME_PATTERN = re.compile(r"(?!0000)\d{4}-\d{2}-\d{2}(?:[T ]\d{2}:\d{2}:\d{2})?")


@overload
def _parse_datetime(s: str, collection: str = ..., attribute: str = ...) -> datetime: ...


@overload
def _parse_datetime(s: None, collection: str = ..., attribute: str = ...) -> None: ...


@overload
def _parse_datetime(s: str | None, collection: str = ..., attribute: str = ...) -> datetime | None: ...


def _parse_datetime(s: str | None, collection: str = "", attribute: str = "") -> datetime | None:
    """Parse an ISO 8601 datetime string to timezone-aware UTC datetime.

    Returns None if input is None. Raises ValueError on malformed input, naming the offending
    column when one is given. Both 'YYYY-MM-DDTHH:MM:SS' and 'YYYY-MM-DD HH:MM:SS' are supported.
    """
    if s is None:
        return None
    if _DATE_TIME_PATTERN.fullmatch(s):
        try:
            return datetime.fromisoformat(s).replace(tzinfo=timezone.utc)
        except ValueError:
            # An out-of-range field ("2024-02-31", hour 25) has the right shape but no valid date;
            # fall through so the rejection still names the column.
            pass
    source = f" in '{collection}.{attribute}'" if collection else ""
    raise ValueError(f'Cannot convert "{s}" to a date time{source}: expected a valid YYYY-MM-DD[THH:MM:SS]')


@overload
def _integer_to_boolean(value: int, collection: str = ..., attribute: str = ...) -> bool: ...


@overload
def _integer_to_boolean(value: None, collection: str = ..., attribute: str = ...) -> None: ...


@overload
def _integer_to_boolean(value: int | None, collection: str = ..., attribute: str = ...) -> bool | None: ...


def _integer_to_boolean(value: int | None, collection: str = "", attribute: str = "") -> bool | None:
    if value is None:
        return None
    if value == 0:
        return False
    if value == 1:
        return True
    source = f" in '{collection}.{attribute}'" if collection else ""
    raise ValueError(f"Cannot convert integer {value} to boolean{source}: expected 0 or 1")


# -- Parameter marshaling helpers (module-level) -----------------------------


def _marshal_params(parameters: list) -> tuple:
    """Marshal Python parameters into C API parallel arrays.

    Returns (keepalive, c_types, c_values) where keepalive must remain
    referenced until the C API call completes.
    """
    n = len(parameters)
    c_types = ffi.new("int[]", n)
    c_values = ffi.new("void*[]", n)
    keepalive = []  # prevent GC of typed buffers

    for i, p in enumerate(parameters):
        if p is None:
            c_types[i] = DataType.NULL
            c_values[i] = ffi.NULL
        elif isinstance(p, int):  # bool is subclass of int, handled here
            c_types[i] = DataType.INTEGER
            buf = ffi.new("int64_t*", p)
            keepalive.append(buf)
            c_values[i] = ffi.cast("void*", buf)
        elif isinstance(p, float):
            c_types[i] = DataType.FLOAT
            buf = ffi.new("double*", p)
            keepalive.append(buf)
            c_values[i] = ffi.cast("void*", buf)
        elif isinstance(p, str):
            c_types[i] = DataType.STRING
            c_buf = ffi.new("char[]", p.encode("utf-8"))
            keepalive.append(c_buf)
            c_values[i] = ffi.cast("void*", c_buf)
        else:
            raise TypeError(f"Unsupported parameter type: {type(p).__name__}. Expected int, float, str, or None.")

    return keepalive, c_types, c_values


def _decode_group_rows(
    collection: str, out_names, out_types, out_data, out_has_value, col_count: int, row_count: int
) -> list[dict]:
    """Decode a whole-group read's columnar typed arrays + per-cell mask into row dicts, then free them.

    Shared by read_vector_group_by_id and read_set_group_by_id (Dart's _decodeGroupRows). A cell
    whose mask is 0 is None and its data slot is never read (a NULL cell's char* is NULL). DATE_TIME
    columns go through _parse_datetime. read_time_series_group keeps its own loop: it returns
    columns and parses only the dimension column.
    """
    if col_count == 0 or row_count == 0:
        return []
    try:
        columns: dict[str, list] = {}
        for c in range(col_count):
            name = ffi.string(out_names[0][c]).decode("utf-8")
            ctype = out_types[0][c]
            mask = out_has_value[0][c]
            if ctype == DataType.INTEGER:
                ints = ffi.cast("int64_t*", out_data[0][c])
                columns[name] = [ints[r] if mask[r] else None for r in range(row_count)]
            elif ctype == DataType.FLOAT:
                floats = ffi.cast("double*", out_data[0][c])
                columns[name] = [floats[r] if mask[r] else None for r in range(row_count)]
            else:  # STRING or DATE_TIME
                strs = ffi.cast("char**", out_data[0][c])
                texts = [ffi.string(strs[r]).decode("utf-8") if mask[r] else None for r in range(row_count)]
                columns[name] = (
                    [_parse_datetime(t, collection, name) for t in texts] if ctype == DataType.DATE_TIME else texts
                )
        return [{name: cells[r] for name, cells in columns.items()} for r in range(row_count)]
    finally:
        get_lib().quiver_database_free_time_series_data(
            out_names[0], out_types[0], out_data[0], out_has_value[0], col_count, row_count
        )


def _marshal_group_columns(data: dict[str, list]) -> tuple:
    """Marshal column lists into parallel C arrays for the columnar group writers (time series, vector, set).

    Each column is typed from all of its non-None cells by `column_data_type`: bool/int ->
    INTEGER, and a float anywhere widens the column to FLOAT; str -> STRING; datetime ->
    STRING in the core's ISO format (via format_datetime, aware values converted to UTC). A
    cell that does not fit its column raises TypeError naming the cell and the column. The
    C++ layer validates against the schema and accepts integers for REAL columns. A None entry
    becomes a per-cell NULL via the mask (with a placeholder in the data array); an all-None
    column is tagged FLOAT with a zero-filled placeholder.

    Returns (keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value,
    col_count, row_count) where keepalive must remain referenced until the C API
    call completes.
    """
    col_count = len(data)
    row_count = len(next(iter(data.values())))
    for name, values in data.items():
        if len(values) != row_count:
            raise ValueError(f"All column lists must have the same length, got {len(values)} for '{name}'")

    keepalive: list = []
    c_col_names = ffi.new("const char*[]", col_count)
    c_col_types = ffi.new("int[]", col_count)
    c_col_data = ffi.new("void*[]", col_count)
    c_col_has_value = ffi.new("uint8_t*[]", col_count)

    for c, (name, values) in enumerate(data.items()):
        name_buf = ffi.new("char[]", name.encode("utf-8"))
        keepalive.append(name_buf)
        c_col_names[c] = name_buf

        mask = ffi.new("uint8_t[]", [0 if v is None else 1 for v in values])
        keepalive.append(mask)
        c_col_has_value[c] = mask

        column_type = column_data_type(name, values)
        if column_type is None:
            # All-null column: tag FLOAT with zeroed placeholder data; the mask is all zero.
            arr = ffi.new("double[]", [0.0] * row_count)
            keepalive.append(arr)
            c_col_types[c] = DataType.FLOAT
            c_col_data[c] = ffi.cast("void*", arr)
        elif column_type == DataType.DATE_TIME:
            encoded = [(format_datetime(v).encode("utf-8") if v is not None else b"") for v in values]
            c_strs = [ffi.new("char[]", e) for e in encoded]
            keepalive.extend(c_strs)
            c_arr = ffi.new("char*[]", [(s if v is not None else ffi.NULL) for s, v in zip(c_strs, values)])
            keepalive.append(c_arr)
            c_col_types[c] = DataType.STRING
            c_col_data[c] = ffi.cast("void*", c_arr)
        elif column_type == DataType.STRING:
            encoded = [(v.encode("utf-8") if v is not None else b"") for v in values]
            c_strs = [ffi.new("char[]", e) for e in encoded]
            keepalive.extend(c_strs)
            c_arr = ffi.new("char*[]", [(s if v is not None else ffi.NULL) for s, v in zip(c_strs, values)])
            keepalive.append(c_arr)
            c_col_types[c] = DataType.STRING
            c_col_data[c] = ffi.cast("void*", c_arr)
        elif column_type == DataType.INTEGER:
            # Every cell is a bool or an int; cffi stores True/False as 1/0.
            arr = ffi.new("int64_t[]", [v if v is not None else 0 for v in values])
            keepalive.append(arr)
            c_col_types[c] = DataType.INTEGER
            c_col_data[c] = ffi.cast("void*", arr)
        else:
            # DataType.FLOAT: float cells, plus any bool/int cells, which cffi widens to double.
            arr = ffi.new("double[]", [v if v is not None else 0.0 for v in values])
            keepalive.append(arr)
            c_col_types[c] = DataType.FLOAT
            c_col_data[c] = ffi.cast("void*", arr)

    return keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count


def _marshal_row_columns(kwargs: dict) -> tuple:
    """Marshal one row of scalars into parallel C arrays for the row-oriented upsert API.

    Each value is wrapped in a 1-element typed array; a datetime is formatted by
    format_datetime and sent as a string. Not `_marshal_group_columns`: the
    row-upsert C signature carries no per-cell mask, so a None cell would be written as that
    helper's zeroed placeholder instead of NULL.

    Returns (keepalive, c_col_names, c_col_types, c_col_data, col_count) where keepalive must
    remain referenced until the C API call completes.
    """
    col_count = len(kwargs)
    keepalive: list = []

    c_col_names = ffi.new("const char*[]", col_count)
    c_col_types = ffi.new("int[]", col_count)
    c_col_data = ffi.new("void*[]", col_count)

    for i, (name, v) in enumerate(kwargs.items()):
        name_buf = ffi.new("char[]", name.encode("utf-8"))
        keepalive.append(name_buf)
        c_col_names[i] = name_buf

        if isinstance(v, datetime):
            v = format_datetime(v)  # marshalled by the str branch below

        if isinstance(v, int):  # bool is an int subclass: True/False marshal as 1/0
            arr = ffi.new("int64_t[]", [v])
            keepalive.append(arr)
            c_col_types[i] = DataType.INTEGER
            c_col_data[i] = ffi.cast("void*", arr)
        elif isinstance(v, float):
            arr = ffi.new("double[]", [v])
            keepalive.append(arr)
            c_col_types[i] = DataType.FLOAT
            c_col_data[i] = ffi.cast("void*", arr)
        elif isinstance(v, str):
            str_buf = ffi.new("char[]", v.encode("utf-8"))
            keepalive.append(str_buf)
            c_str_arr = ffi.new("char*[]", [str_buf])
            keepalive.append(c_str_arr)
            c_col_types[i] = DataType.STRING
            c_col_data[i] = ffi.cast("void*", c_str_arr)
        else:
            raise TypeError(
                f"Column '{name}' value has unsupported type {type(v).__name__}; expected int, float, str, or datetime"
            )

    return keepalive, c_col_names, c_col_types, c_col_data, col_count


# -- Metadata parsing helpers (module-level) ---------------------------------


def _parse_scalar_metadata(meta) -> ScalarMetadata:
    """Parse a quiver_scalar_metadata_t struct into a ScalarMetadata dataclass."""
    return ScalarMetadata(
        name=decode_string(meta.name),
        data_type=DataType(int(meta.data_type)),
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
