"""Tests for scalar read operations, element IDs, and convenience helpers."""

from __future__ import annotations

from datetime import datetime, timezone

import pytest

from quiverdb import Database


# -- Bulk scalar reads ---------------------------------------------------------


class TestReadScalarIntegers:
    def test_read_scalar_integers(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", integer_attribute=10)
        db.create_element("Configuration", label="item2", integer_attribute=20)
        result = db.read_scalar_integers("Configuration", "integer_attribute")
        assert result == [10, 20]

    def test_read_scalar_integers_with_default(self, db: Database) -> None:
        # integer_attribute has DEFAULT 6 in basic.sql
        db.create_element("Configuration", label="item1")
        result = db.read_scalar_integers("Configuration", "integer_attribute")
        assert result == [6]

    def test_read_scalar_integers_empty_collection(self, db: Database) -> None:
        result = db.read_scalar_integers("Configuration", "integer_attribute")
        assert result == []


class TestReadScalarFloats:
    def test_read_scalar_floats(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", float_attribute=3.14)
        db.create_element("Configuration", label="item2", float_attribute=2.71)
        result = db.read_scalar_floats("Configuration", "float_attribute")
        assert len(result) == 2
        assert abs(result[0] - 3.14) < 1e-9
        assert abs(result[1] - 2.71) < 1e-9

    def test_read_scalar_floats_empty_collection(self, db: Database) -> None:
        result = db.read_scalar_floats("Configuration", "float_attribute")
        assert result == []


class TestReadScalarStrings:
    def test_read_scalar_strings(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", string_attribute="hello")
        db.create_element("Configuration", label="item2", string_attribute="world")
        result = db.read_scalar_strings("Configuration", "string_attribute")
        assert result == ["hello", "world"]

    def test_read_scalar_strings_skips_nulls(self, db: Database) -> None:
        # C++ bulk read filters out NULL values (only returns non-NULL strings)
        db.create_element("Configuration", label="item1", string_attribute="hello")
        db.create_element("Configuration", label="item2")  # string_attribute is NULL
        result = db.read_scalar_strings("Configuration", "string_attribute")
        assert result == ["hello"]

    def test_read_scalar_strings_empty_string_preserved(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", string_attribute="")
        db.create_element("Configuration", label="item2", string_attribute="hi")
        result = db.read_scalar_strings("Configuration", "string_attribute")
        assert result == ["", "hi"]

    def test_read_scalar_strings_empty_collection(self, db: Database) -> None:
        result = db.read_scalar_strings("Configuration", "string_attribute")
        assert result == []


# -- Scalar reads by ID -------------------------------------------------------


class TestReadScalarByID:
    def test_read_scalar_integer_by_id(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1", integer_attribute=42)
        result = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1)
        assert result == 42

    def test_read_scalar_integer_by_id_default(self, db: Database) -> None:
        # integer_attribute has DEFAULT 6
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1)
        assert result == 6

    def test_read_scalar_integer_by_id_null(self, db: Database) -> None:
        # float_attribute has no default and is nullable
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_scalar_float_by_id("Configuration", "float_attribute", id1)
        assert result is None

    def test_read_scalar_float_by_id(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1", float_attribute=9.81)
        result = db.read_scalar_float_by_id("Configuration", "float_attribute", id1)
        assert result is not None
        assert abs(result - 9.81) < 1e-9

    def test_read_scalar_float_by_id_null(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_scalar_float_by_id("Configuration", "float_attribute", id1)
        assert result is None

    def test_read_scalar_string_by_id(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1", string_attribute="hello")
        result = db.read_scalar_string_by_id("Configuration", "string_attribute", id1)
        assert result == "hello"

    def test_read_scalar_string_by_id_null(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_scalar_string_by_id("Configuration", "string_attribute", id1)
        assert result is None

    def test_read_scalar_string_by_id_empty_string(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1", string_attribute="")
        result = db.read_scalar_string_by_id("Configuration", "string_attribute", id1)
        assert result == ""


# -- Element IDs ---------------------------------------------------------------


class TestReadElementIDs:
    def test_read_element_ids(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1")
        id2 = db.create_element("Configuration", label="item2")
        ids = db.read_element_ids("Configuration")
        assert id1 in ids
        assert id2 in ids
        assert len(ids) == 2

    def test_read_element_ids_empty(self, db: Database) -> None:
        ids = db.read_element_ids("Configuration")
        assert ids == []


# -- DateTime scalar reads ---------------------------------------------------


class TestReadScalarDateTimeByID:
    def test_read_scalar_date_time_by_id(self, db: Database) -> None:
        id1 = db.create_element(
            "Configuration", label="item1", date_attribute="2024-01-15T10:30:00",
        )
        result = db.read_scalar_date_time_by_id("Configuration", "date_attribute", id1)
        assert result == datetime(2024, 1, 15, 10, 30, 0, tzinfo=timezone.utc)
        assert result.tzinfo is not None

    def test_read_scalar_date_time_by_id_space_format(self, db: Database) -> None:
        id1 = db.create_element(
            "Configuration", label="item1", date_attribute="2024-01-15 10:30:00",
        )
        result = db.read_scalar_date_time_by_id("Configuration", "date_attribute", id1)
        assert result == datetime(2024, 1, 15, 10, 30, 0, tzinfo=timezone.utc)

    def test_read_scalar_date_time_by_id_none(self, db: Database) -> None:
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_scalar_date_time_by_id("Configuration", "date_attribute", id1)
        assert result is None


# -- Composite scalar reads --------------------------------------------------


class TestReadAllScalarsByID:
    def test_read_all_scalars_by_id(self, db: Database) -> None:
        id1 = db.create_element(
            "Configuration",
            label="item1",
            integer_attribute=42,
            float_attribute=3.14,
            string_attribute="hello",
            date_attribute="2024-01-15T10:30:00",
        )
        result = db.read_all_scalars_by_id("Configuration", id1)

        # Check all expected keys present
        assert "id" in result
        assert "label" in result
        assert "integer_attribute" in result
        assert "float_attribute" in result
        assert "string_attribute" in result
        assert "date_attribute" in result

        # Check types
        assert isinstance(result["id"], int)
        assert isinstance(result["label"], str)
        assert isinstance(result["integer_attribute"], int)
        assert isinstance(result["float_attribute"], float)
        assert isinstance(result["string_attribute"], str)
        assert isinstance(result["date_attribute"], datetime)

        # Check values
        assert result["label"] == "item1"
        assert result["integer_attribute"] == 42
        assert abs(result["float_attribute"] - 3.14) < 1e-9
        assert result["string_attribute"] == "hello"
        assert result["date_attribute"] == datetime(2024, 1, 15, 10, 30, 0, tzinfo=timezone.utc)
        assert result["date_attribute"].tzinfo is not None
