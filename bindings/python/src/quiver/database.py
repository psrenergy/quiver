from __future__ import annotations

import warnings
from contextlib import contextmanager

from quiver._c_api import ffi, get_lib
from quiver._helpers import check, decode_string, decode_string_or_none
from quiver.exceptions import QuiverError
from quiver.metadata import GroupMetadata, ScalarMetadata


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

    # -- Write operations -------------------------------------------------------

    def update_element(self, collection: str, id: int, element) -> None:
        """Update an existing element's scalar attributes."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_update_element(
            self._ptr, collection.encode("utf-8"), id, element._ptr))

    def delete_element(self, collection: str, id: int) -> None:
        """Delete an element by ID."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_delete_element(
            self._ptr, collection.encode("utf-8"), id))

    # -- Scalar updates ---------------------------------------------------------

    def update_scalar_integer(self, collection, attribute, id, value):
        """Update an integer scalar attribute for a single element."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_update_scalar_integer(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, value))

    def update_scalar_float(self, collection, attribute, id, value):
        """Update a float scalar attribute for a single element."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_update_scalar_float(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, value))

    def update_scalar_string(self, collection, attribute, id, value):
        """Update a string scalar attribute. Pass None to set NULL."""
        self._ensure_open()
        lib = get_lib()
        c_value = value.encode("utf-8") if value is not None else ffi.NULL
        check(lib.quiver_database_update_scalar_string(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, c_value))

    # -- Vector updates ---------------------------------------------------------

    def update_vector_integers(self, collection, attribute, id, values):
        """Replace an integer vector for a single element. Empty list clears."""
        self._ensure_open()
        lib = get_lib()
        if not values:
            check(lib.quiver_database_update_vector_integers(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, ffi.NULL, 0))
        else:
            c_arr = ffi.new("int64_t[]", values)
            check(lib.quiver_database_update_vector_integers(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, c_arr, len(values)))

    def update_vector_floats(self, collection, attribute, id, values):
        """Replace a float vector for a single element. Empty list clears."""
        self._ensure_open()
        lib = get_lib()
        if not values:
            check(lib.quiver_database_update_vector_floats(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, ffi.NULL, 0))
        else:
            c_arr = ffi.new("double[]", values)
            check(lib.quiver_database_update_vector_floats(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, c_arr, len(values)))

    def update_vector_strings(self, collection, attribute, id, values):
        """Replace a string vector for a single element. Empty list clears."""
        self._ensure_open()
        lib = get_lib()
        if not values:
            check(lib.quiver_database_update_vector_strings(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, ffi.NULL, 0))
        else:
            encoded = [v.encode("utf-8") for v in values]
            c_strings = [ffi.new("char[]", e) for e in encoded]
            c_arr = ffi.new("const char*[]", c_strings)
            check(lib.quiver_database_update_vector_strings(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, c_arr, len(values)))

    # -- Set updates ------------------------------------------------------------

    def update_set_integers(self, collection, attribute, id, values):
        """Replace an integer set for a single element. Empty list clears."""
        self._ensure_open()
        lib = get_lib()
        if not values:
            check(lib.quiver_database_update_set_integers(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, ffi.NULL, 0))
        else:
            c_arr = ffi.new("int64_t[]", values)
            check(lib.quiver_database_update_set_integers(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, c_arr, len(values)))

    def update_set_floats(self, collection, attribute, id, values):
        """Replace a float set for a single element. Empty list clears."""
        self._ensure_open()
        lib = get_lib()
        if not values:
            check(lib.quiver_database_update_set_floats(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, ffi.NULL, 0))
        else:
            c_arr = ffi.new("double[]", values)
            check(lib.quiver_database_update_set_floats(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, c_arr, len(values)))

    def update_set_strings(self, collection, attribute, id, values):
        """Replace a string set for a single element. Empty list clears."""
        self._ensure_open()
        lib = get_lib()
        if not values:
            check(lib.quiver_database_update_set_strings(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, ffi.NULL, 0))
        else:
            encoded = [v.encode("utf-8") for v in values]
            c_strings = [ffi.new("char[]", e) for e in encoded]
            c_arr = ffi.new("const char*[]", c_strings)
            check(lib.quiver_database_update_set_strings(
                self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
                id, c_arr, len(values)))

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

    # -- Scalar reads (bulk) --------------------------------------------------

    def read_scalar_integers(self, collection: str, attribute: str) -> list[int]:
        """Read all integer values for a scalar attribute across all elements."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_scalar_integers(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_values, out_count,
        ))
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
        check(lib.quiver_database_read_scalar_floats(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_values, out_count,
        ))
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
        check(lib.quiver_database_read_scalar_strings(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_values, out_count,
        ))
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
        self, collection: str, attribute: str, id: int,
    ) -> int | None:
        """Read an integer scalar attribute for a single element. Returns None if NULL."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("int64_t*")
        out_has = ffi.new("int*")
        check(lib.quiver_database_read_scalar_integer_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_value, out_has,
        ))
        if out_has[0] == 0:
            return None
        return out_value[0]

    def read_scalar_float_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> float | None:
        """Read a float scalar attribute for a single element. Returns None if NULL."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("double*")
        out_has = ffi.new("int*")
        check(lib.quiver_database_read_scalar_float_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_value, out_has,
        ))
        if out_has[0] == 0:
            return None
        return out_value[0]

    def read_scalar_string_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> str | None:
        """Read a string scalar attribute for a single element. Returns None if NULL."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")
        check(lib.quiver_database_read_scalar_string_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_value, out_has,
        ))
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
        check(lib.quiver_database_read_element_ids(
            self._ptr, collection.encode("utf-8"), out_ids, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_ids[0] == ffi.NULL:
            return []
        try:
            return [out_ids[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_ids[0])

    # -- Relation operations ----------------------------------------------------

    def update_scalar_relation(
        self, collection: str, attribute: str, from_label: str, to_label: str,
    ) -> None:
        """Set a scalar relation by element labels."""
        self._ensure_open()
        lib = get_lib()
        check(lib.quiver_database_update_scalar_relation(
            self._ptr,
            collection.encode("utf-8"),
            attribute.encode("utf-8"),
            from_label.encode("utf-8"),
            to_label.encode("utf-8"),
        ))

    def read_scalar_relation(
        self, collection: str, attribute: str,
    ) -> list[str | None]:
        """Read scalar relation labels. Returns labels of related elements, None for unset."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_scalar_relation(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_values, out_count,
        ))
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
                    s = ffi.string(ptr).decode("utf-8")
                    result.append(None if s == "" else s)
            return result
        finally:
            lib.quiver_database_free_string_array(out_values[0], count)

    # -- Vector reads (bulk) -----------------------------------------------------

    def read_vector_integers(self, collection: str, attribute: str) -> list[list[int]]:
        """Read integer vectors for all elements in a collection."""
        self._ensure_open()
        lib = get_lib()
        out_vectors = ffi.new("int64_t***")
        out_sizes = ffi.new("size_t**")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_vector_integers(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_vectors, out_sizes, out_count,
        ))
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
        check(lib.quiver_database_read_vector_floats(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_vectors, out_sizes, out_count,
        ))
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
        check(lib.quiver_database_read_vector_strings(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_vectors, out_sizes, out_count,
        ))
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
                    result.append([
                        ffi.string(out_vectors[0][i][j]).decode("utf-8")
                        for j in range(size)
                    ])
            return result
        finally:
            lib.quiver_database_free_string_vectors(out_vectors[0], out_sizes[0], count)

    # -- Vector reads (by ID) ----------------------------------------------------

    def read_vector_integers_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> list[int]:
        """Read an integer vector for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_vector_integers_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_values, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])

    def read_vector_floats_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> list[float]:
        """Read a float vector for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_vector_floats_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_values, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])

    def read_vector_strings_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> list[str]:
        """Read a string vector for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_vector_strings_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_values, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [
                ffi.string(out_values[0][i]).decode("utf-8")
                for i in range(count)
            ]
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
        check(lib.quiver_database_read_set_integers(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_sets, out_sizes, out_count,
        ))
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
        check(lib.quiver_database_read_set_floats(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_sets, out_sizes, out_count,
        ))
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
        check(lib.quiver_database_read_set_strings(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            out_sets, out_sizes, out_count,
        ))
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
                    result.append([
                        ffi.string(out_sets[0][i][j]).decode("utf-8")
                        for j in range(size)
                    ])
            return result
        finally:
            lib.quiver_database_free_string_vectors(out_sets[0], out_sizes[0], count)

    # -- Set reads (by ID) -------------------------------------------------------

    def read_set_integers_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> list[int]:
        """Read an integer set for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("int64_t**")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_set_integers_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_values, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_integer_array(out_values[0])

    def read_set_floats_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> list[float]:
        """Read a float set for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("double**")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_set_floats_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_values, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [out_values[0][i] for i in range(count)]
        finally:
            lib.quiver_database_free_float_array(out_values[0])

    def read_set_strings_by_id(
        self, collection: str, attribute: str, id: int,
    ) -> list[str]:
        """Read a string set for a single element."""
        self._ensure_open()
        lib = get_lib()
        out_values = ffi.new("char***")
        out_count = ffi.new("size_t*")
        check(lib.quiver_database_read_set_strings_by_id(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, out_values, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        try:
            return [
                ffi.string(out_values[0][i]).decode("utf-8")
                for i in range(count)
            ]
        finally:
            lib.quiver_database_free_string_array(out_values[0], count)

    # -- Metadata get ------------------------------------------------------------

    def get_scalar_metadata(
        self, collection: str, attribute: str,
    ) -> ScalarMetadata:
        """Get metadata for a scalar attribute."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_scalar_metadata_t*")
        check(lib.quiver_database_get_scalar_metadata(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"), out,
        ))
        try:
            return _parse_scalar_metadata(out)
        finally:
            lib.quiver_database_free_scalar_metadata(out)

    def get_vector_metadata(
        self, collection: str, group_name: str,
    ) -> GroupMetadata:
        """Get metadata for a vector group."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_group_metadata_t*")
        check(lib.quiver_database_get_vector_metadata(
            self._ptr, collection.encode("utf-8"), group_name.encode("utf-8"), out,
        ))
        try:
            return _parse_group_metadata(out)
        finally:
            lib.quiver_database_free_group_metadata(out)

    def get_set_metadata(
        self, collection: str, group_name: str,
    ) -> GroupMetadata:
        """Get metadata for a set group."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_group_metadata_t*")
        check(lib.quiver_database_get_set_metadata(
            self._ptr, collection.encode("utf-8"), group_name.encode("utf-8"), out,
        ))
        try:
            return _parse_group_metadata(out)
        finally:
            lib.quiver_database_free_group_metadata(out)

    def get_time_series_metadata(
        self, collection: str, group_name: str,
    ) -> GroupMetadata:
        """Get metadata for a time series group."""
        self._ensure_open()
        lib = get_lib()
        out = ffi.new("quiver_group_metadata_t*")
        check(lib.quiver_database_get_time_series_metadata(
            self._ptr, collection.encode("utf-8"), group_name.encode("utf-8"), out,
        ))
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
        check(lib.quiver_database_list_scalar_attributes(
            self._ptr, collection.encode("utf-8"), out_metadata, out_count,
        ))
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
        check(lib.quiver_database_list_vector_groups(
            self._ptr, collection.encode("utf-8"), out_metadata, out_count,
        ))
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
        check(lib.quiver_database_list_set_groups(
            self._ptr, collection.encode("utf-8"), out_metadata, out_count,
        ))
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
        check(lib.quiver_database_list_time_series_groups(
            self._ptr, collection.encode("utf-8"), out_metadata, out_count,
        ))
        count = out_count[0]
        if count == 0 or out_metadata[0] == ffi.NULL:
            return []
        try:
            return [_parse_group_metadata(out_metadata[0] + i) for i in range(count)]
        finally:
            lib.quiver_database_free_group_metadata_array(out_metadata[0], count)

    def __repr__(self) -> str:
        if self._closed:
            return "Database(closed)"
        return f"Database(path={self.path()!r})"


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
