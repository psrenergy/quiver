"""Tests for update_element, scalar/vector/set update operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, Element, QuiverError


class TestUpdateElement:
    def test_update_single_scalar(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_integer", 10),
        )
        collections_db.update_element(
            "Collection", elem_id, Element().set("some_integer", 99),
        )
        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
        assert value == 99

    def test_update_preserves_other_attributes(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection",
            Element().set("label", "Item1").set("some_integer", 10).set("some_float", 2.5),
        )
        collections_db.update_element(
            "Collection", elem_id, Element().set("some_integer", 99),
        )
        # some_float should be unchanged
        float_val = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert float_val is not None
        assert abs(float_val - 2.5) < 1e-9


class TestUpdateScalar:
    def test_update_scalar_integer(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_integer", 10),
        )
        collections_db.update_scalar_integer("Collection", "some_integer", elem_id, 100)
        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
        assert value == 100

    def test_update_scalar_float(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_float", 1.0),
        )
        collections_db.update_scalar_float("Collection", "some_float", elem_id, 9.99)
        value = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert value is not None
        assert abs(value - 9.99) < 1e-9

    def test_update_scalar_string(self, db: Database) -> None:
        elem_id = db.create_element(
            "Configuration",
            Element().set("label", "item1").set("string_attribute", "hello"),
        )
        db.update_scalar_string("Configuration", "string_attribute", elem_id, "world")
        value = db.read_scalar_string_by_id("Configuration", "string_attribute", elem_id)
        assert value == "world"

    def test_update_scalar_string_to_none(self, db: Database) -> None:
        elem_id = db.create_element(
            "Configuration",
            Element().set("label", "item1").set("string_attribute", "hello"),
        )
        db.update_scalar_string("Configuration", "string_attribute", elem_id, None)
        value = db.read_scalar_string_by_id("Configuration", "string_attribute", elem_id)
        assert value is None


class TestUpdateVector:
    def test_update_vector_integers(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [10, 20, 30])
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id)
        assert result == [10, 20, 30]

    def test_update_vector_floats(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_floats("Collection", "value_float", elem_id, [1.1, 2.2, 3.3])
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", elem_id)
        assert len(result) == 3
        assert abs(result[0] - 1.1) < 1e-9
        assert abs(result[1] - 2.2) < 1e-9
        assert abs(result[2] - 3.3) < 1e-9

    def test_update_vector_empty_clears(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [10, 20])
        # Verify data exists
        assert collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id) == [10, 20]
        # Clear with empty list
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [])
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id)
        assert result == []


class TestUpdateSet:
    def test_update_set_strings(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_set_strings("Collection", "tag", elem_id, ["tag1", "tag2"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert sorted(result) == ["tag1", "tag2"]

    def test_update_set_empty_clears(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_set_strings("Collection", "tag", elem_id, ["tag1", "tag2"])
        # Verify data exists
        assert sorted(collections_db.read_set_strings_by_id("Collection", "tag", elem_id)) == ["tag1", "tag2"]
        # Clear with empty list
        collections_db.update_set_strings("Collection", "tag", elem_id, [])
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert result == []


# -- String vector update (gap-fill) -----------------------------------------


class TestUpdateVectorStrings:
    def test_update_vector_strings(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes", Element().set("label", "Item1"),
        )
        all_types_db.update_vector_strings("AllTypes", "label_value", elem_id, ["alpha", "beta"])
        result = all_types_db.read_vector_strings_by_id("AllTypes", "label_value", elem_id)
        assert result == ["alpha", "beta"]


# -- Integer set update (gap-fill) ------------------------------------------


class TestUpdateSetIntegers:
    def test_update_set_integers(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes", Element().set("label", "Item1"),
        )
        all_types_db.update_set_integers("AllTypes", "code", elem_id, [10, 20, 30])
        result = all_types_db.read_set_integers_by_id("AllTypes", "code", elem_id)
        assert sorted(result) == [10, 20, 30]


# -- Float set update (gap-fill) -------------------------------------------


class TestUpdateSetFloats:
    def test_update_set_floats(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes", Element().set("label", "Item1"),
        )
        all_types_db.update_set_floats("AllTypes", "weight", elem_id, [1.1, 2.2])
        result = all_types_db.read_set_floats_by_id("AllTypes", "weight", elem_id)
        assert len(result) == 2
        assert any(abs(v - 1.1) < 1e-9 for v in result)
        assert any(abs(v - 2.2) < 1e-9 for v in result)


class TestFKResolutionUpdate:
    """FK label resolution tests for update_element -- ported from Julia/Dart."""

    def test_scalar_fk_label(self, relations_db: Database) -> None:
        """Update scalar FK with string label resolves to new parent ID."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", "Parent 1"),
        )
        relations_db.update_element("Child", 1, Element().set("parent_id", "Parent 2"))
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
        assert result == 2

    def test_scalar_fk_integer(self, relations_db: Database) -> None:
        """Update scalar FK with integer value passed through as-is."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", 1),
        )
        relations_db.update_element("Child", 1, Element().set("parent_id", 2))
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
        assert result == 2

    def test_vector_fk_labels(self, relations_db: Database) -> None:
        """Update vector FK with string labels resolves to parent IDs."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_ref", ["Parent 1"]),
        )
        relations_db.update_element(
            "Child", 1, Element().set("parent_ref", ["Parent 2", "Parent 1"]),
        )
        result = relations_db.read_vector_integers_by_id("Child", "parent_ref", 1)
        assert result == [2, 1]

    def test_set_fk_labels(self, relations_db: Database) -> None:
        """Update set FK with string labels resolves to parent IDs."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("mentor_id", ["Parent 1"]),
        )
        relations_db.update_element(
            "Child", 1, Element().set("mentor_id", ["Parent 2"]),
        )
        result = relations_db.read_set_integers_by_id("Child", "mentor_id", 1)
        assert result == [2]

    def test_time_series_fk_labels(self, relations_db: Database) -> None:
        """Update time series FK with string labels resolves to parent IDs."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        relations_db.create_element(
            "Child",
            Element()
            .set("label", "Child 1")
            .set("date_time", ["2024-01-01"])
            .set("sponsor_id", ["Parent 1"]),
        )
        relations_db.update_element(
            "Child",
            1,
            Element()
            .set("date_time", ["2024-06-01", "2024-06-02"])
            .set("sponsor_id", ["Parent 2", "Parent 1"]),
        )
        rows = relations_db.read_time_series_group("Child", "events", 1)
        sponsor_ids = [row["sponsor_id"] for row in rows]
        assert sponsor_ids == [2, 1]
        assert len(rows) == 2

    def test_all_fk_types_in_one_call(self, relations_db: Database) -> None:
        """Update all FK types in a single update_element call."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        # Create with all FK types pointing to Parent 1
        relations_db.create_element(
            "Child",
            Element()
            .set("label", "Child 1")
            .set("parent_id", "Parent 1")
            .set("mentor_id", ["Parent 1"])
            .set("parent_ref", ["Parent 1"])
            .set("date_time", ["2024-01-01"])
            .set("sponsor_id", ["Parent 1"]),
        )
        # Update all FK types to Parent 2
        relations_db.update_element(
            "Child",
            1,
            Element()
            .set("parent_id", "Parent 2")
            .set("mentor_id", ["Parent 2"])
            .set("parent_ref", ["Parent 2"])
            .set("date_time", ["2025-01-01"])
            .set("sponsor_id", ["Parent 2"]),
        )
        # Verify scalar FK
        assert relations_db.read_scalar_integer_by_id("Child", "parent_id", 1) == 2
        # Verify set FK
        assert relations_db.read_set_integers_by_id("Child", "mentor_id", 1) == [2]
        # Verify vector FK
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", 1) == [2]
        # Verify time series FK
        rows = relations_db.read_time_series_group("Child", "events", 1)
        assert [row["sponsor_id"] for row in rows] == [2]

    def test_no_fk_columns_unchanged(self, db: Database) -> None:
        """No FK columns: update_element works normally for non-FK schemas."""
        db.create_element(
            "Configuration",
            Element()
            .set("label", "Config 1")
            .set("integer_attribute", 42)
            .set("float_attribute", 3.14)
            .set("string_attribute", "hello"),
        )
        db.update_element(
            "Configuration",
            1,
            Element()
            .set("integer_attribute", 100)
            .set("float_attribute", 2.71)
            .set("string_attribute", "world"),
        )
        assert db.read_scalar_integer_by_id("Configuration", "integer_attribute", 1) == 100
        float_val = db.read_scalar_float_by_id("Configuration", "float_attribute", 1)
        assert float_val is not None
        assert abs(float_val - 2.71) < 1e-9
        assert db.read_scalar_string_by_id("Configuration", "string_attribute", 1) == "world"

    def test_resolution_failure_preserves_existing(self, relations_db: Database) -> None:
        """Failed FK resolution preserves existing values."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", "Parent 1"),
        )
        with pytest.raises(QuiverError):
            relations_db.update_element(
                "Child", 1, Element().set("parent_id", "Nonexistent Parent"),
            )
        # Verify original value preserved
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
        assert result == 1
