"""Tests for create_element operations."""

from __future__ import annotations

from datetime import datetime, timedelta, timezone

import pytest

from quiverdb import Database, QuiverError


class TestCreateElement:
    def test_create_element_returns_id(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        result = collections_db.create_element("Collection", label="Item1", some_integer=42)
        assert isinstance(result, int)
        assert result > 0

    def test_create_element_passes_a_collection_attribute_to_the_core(self, collections_db: Database) -> None:
        """`collection` is positional-only: an attribute of that name reaches the core, not a TypeError."""
        collections_db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError, match="'collection'"):
            collections_db.create_element("Collection", label="Item1", collection="x")

    def test_create_multiple_elements(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        id1 = collections_db.create_element(
            "Collection",
            label="Item1",
            some_integer=10,
        )
        id2 = collections_db.create_element(
            "Collection",
            label="Item2",
            some_integer=20,
        )
        assert id1 != id2
        assert id1 > 0
        assert id2 > 0

    def test_create_with_float(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
            some_float=3.14,
        )
        value = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert value is not None
        assert abs(value - 3.14) < 1e-9

    def test_create_with_set_array(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element("Collection", elem_id, tag=["a", "b"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert sorted(result) == ["a", "b"]

    def test_create_with_fk_label(self, relations_db: Database) -> None:
        """FK label resolution: string value for FK column resolves to parent ID."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id="Parent 1",
        )
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
        assert result == 1

    def test_create_element_with_dict_unpacking(self, collections_db: Database) -> None:
        """Verify **dict unpacking works with create_element."""
        collections_db.create_element("Configuration", label="cfg")
        attrs = {"label": "Item1", "some_integer": 42}
        elem_id = collections_db.create_element("Collection", **attrs)
        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
        assert value == 42


class TestFKResolutionCreate:
    """FK label resolution tests for create_element -- ported from Julia/Dart."""

    def test_scalar_fk_label(self, relations_db: Database) -> None:
        """Scalar FK: string label resolves to parent ID."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id="Parent 1",
        )
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
        assert result == 1

    def test_scalar_fk_integer(self, relations_db: Database) -> None:
        """Scalar FK: integer value passed through as-is."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id=1,
        )
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
        assert result == 1

    def test_vector_fk_labels(self, relations_db: Database) -> None:
        """Vector FK: string labels resolve to parent Ids."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            parent_ref=["Parent 1", "Parent 2"],
        )
        result = relations_db.read_vector_integers_by_id("Child", "parent_ref", child_id)
        assert result == [1, 2]

    def test_set_fk_labels(self, relations_db: Database) -> None:
        """Set FK: string labels resolve to parent Ids."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            mentor_id=["Parent 1", "Parent 2"],
        )
        result = relations_db.read_set_integers_by_id("Child", "mentor_id", child_id)
        assert sorted(result) == [1, 2]

    def test_time_series_fk_labels(self, relations_db: Database) -> None:
        """Time series FK: string labels resolve to parent Ids."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            date_time=["2024-01-01", "2024-01-02"],
            sponsor_id=["Parent 1", "Parent 2"],
        )
        data = relations_db.read_time_series_group("Child", "events", child_id)
        assert data["sponsor_id"] == [1, 2]

    def test_all_fk_types_in_one_call(self, relations_db: Database) -> None:
        """All FK types resolved in a single create_element call."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        child_id = relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id="Parent 1",
            mentor_id=["Parent 2"],
            parent_ref=["Parent 1"],
            date_time=["2024-01-01"],
            sponsor_id=["Parent 2"],
        )
        # Verify scalar FK
        assert relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id) == 1
        # Verify set FK
        assert sorted(relations_db.read_set_integers_by_id("Child", "mentor_id", child_id)) == [2]
        # Verify vector FK
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child_id) == [1]
        # Verify time series FK
        data = relations_db.read_time_series_group("Child", "events", child_id)
        assert data["sponsor_id"] == [2]

    def test_no_fk_columns_unchanged(self, db: Database) -> None:
        """No FK columns: pre-resolve pass is a no-op for non-FK schemas."""
        db.create_element(
            "Configuration",
            label="Config 1",
            integer_attribute=42,
            float_attribute=3.14,
        )
        assert db.read_scalar_strings("Configuration", "label") == ["Config 1"]
        assert db.read_scalar_integers("Configuration", "integer_attribute") == [42]
        floats = db.read_scalar_floats("Configuration", "float_attribute")
        assert len(floats) == 1
        assert abs(floats[0] - 3.14) < 1e-9

    def test_missing_target_label_error(self, relations_db: Database) -> None:
        """Missing FK target label raises QuiverError."""
        relations_db.create_element("Configuration", label="cfg")
        # No Parent created
        with pytest.raises(QuiverError):
            relations_db.create_element(
                "Child",
                label="Child 1",
                mentor_id=["Nonexistent Parent"],
            )

    def test_non_fk_string_rejection_error(self, relations_db: Database) -> None:
        """String value for non-FK integer column raises QuiverError."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        with pytest.raises(QuiverError):
            relations_db.create_element(
                "Child",
                label="Child 1",
                parent_id=1,
                score=["not_a_label"],
            )

    def test_resolution_failure_no_partial_writes(self, relations_db: Database) -> None:
        """Failed FK resolution causes no partial writes (atomicity)."""
        relations_db.create_element("Configuration", label="cfg")
        # No Parent created
        with pytest.raises(QuiverError):
            relations_db.create_element(
                "Child",
                label="Orphan Child",
                parent_id="Nonexistent Parent",
            )
        # Verify no child was created
        labels = relations_db.read_scalar_strings("Child", "label")
        assert labels == []

    def test_self_reference_fk(self, relations_db: Database) -> None:
        """Self-reference FK: sibling_id references same collection."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        child_1_id = relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id=1,
        )
        child_2_id = relations_db.create_element(
            "Child",
            label="Child 2",
            sibling_id="Child 1",
        )
        result = relations_db.read_scalar_integer_by_id("Child", "sibling_id", child_2_id)
        assert result == child_1_id

    def test_empty_collection_no_fk_data(self, relations_db: Database) -> None:
        """Empty collection with no FK data returns empty list."""
        relations_db.create_element("Configuration", label="cfg")
        # No Parent, no Child created
        result = relations_db.read_scalar_integers("Child", "parent_id")
        assert result == []


class TestCreateScalarTypeCoercion:
    def test_float_rejected_for_integer_column(self, db: Database) -> None:
        with pytest.raises(QuiverError):
            db.create_element("Configuration", label="cfg", integer_attribute=42.0)

    def test_integer_accepted_for_real_column(self, db: Database) -> None:
        elem_id = db.create_element("Configuration", label="cfg", float_attribute=7)
        assert db.read_scalar_float_by_id("Configuration", "float_attribute", elem_id) == 7.0


class TestCreateScalarDateTime:
    def test_partial_date_rejected_on_write(self, db: Database) -> None:
        # A year-month string used to be stored happily and then blew up on the read, inside
        # datetime.fromisoformat. The core rejects it at the write now. Full grammar lives in the
        # C++ suite (tests/test_database_create.cpp).
        with pytest.raises(QuiverError, match="invalid DATE_TIME value for column 'date_attribute'"):
            db.create_element("Configuration", label="cfg", date_attribute="2005-01")

    def test_date_only_accepted_and_reads_back(self, db: Database) -> None:
        elem_id = db.create_element("Configuration", label="cfg", date_attribute="2005-01-01")
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", elem_id) == "2005-01-01"
        assert db.read_scalar_date_time_by_id("Configuration", "date_attribute", elem_id) == datetime(
            2005, 1, 1, tzinfo=timezone.utc
        )


class TestCreateWithDatetime:
    def test_aware_datetime_stored_as_utc_instant(self, db: Database) -> None:
        # Every reader returns UTC, so an aware value is converted to UTC on the way in. Writing its
        # wall clock would read back three hours off.
        written = datetime(2024, 1, 1, 10, tzinfo=timezone(timedelta(hours=3)))
        elem_id = db.create_element("Configuration", label="cfg", date_attribute=written)

        assert db.read_scalar_string_by_id("Configuration", "date_attribute", elem_id) == "2024-01-01T07:00:00"
        assert db.read_scalar_date_time_by_id("Configuration", "date_attribute", elem_id) == datetime(
            2024, 1, 1, 7, tzinfo=timezone.utc
        )

    def test_naive_datetime_stored_as_written(self, db: Database) -> None:
        elem_id = db.create_element("Configuration", label="cfg", date_attribute=datetime(2024, 1, 15, 10, 30))
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", elem_id) == "2024-01-15T10:30:00"

    def test_read_back_datetime_writes_back(self, db: Database) -> None:
        # A read-modify-write round trip: the reader's UTC-aware datetime is a valid write value.
        source = db.create_element("Configuration", label="a", date_attribute="2024-01-15T10:30:00")
        value = db.read_scalar_date_time_by_id("Configuration", "date_attribute", source)

        copy = db.create_element("Configuration", label="b", date_attribute=value)
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", copy) == "2024-01-15T10:30:00"

        db.update_element("Configuration", copy, date_attribute=datetime(2025, 6, 1, tzinfo=timezone.utc))
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", copy) == "2025-06-01T00:00:00"

    def test_datetime_list_stored_as_strings(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes",
            label="item1",
            label_value=[datetime(2024, 1, 15, 10, 30), datetime(2024, 1, 16, tzinfo=timezone(timedelta(hours=3)))],
        )
        assert all_types_db.read_vector_strings_by_id("AllTypes", "label_value", elem_id) == [
            "2024-01-15T10:30:00",
            "2024-01-15T21:00:00",
        ]
