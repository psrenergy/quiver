"""Tests for create_element operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, Element, QuiverError


class TestCreateElement:
    def test_create_element_returns_id(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem = Element().set("label", "Item1").set("some_integer", 42)
        result = collections_db.create_element("Collection", elem)
        assert isinstance(result, int)
        assert result > 0

    def test_create_multiple_elements(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        id1 = collections_db.create_element(
            "Collection",
            Element().set("label", "Item1").set("some_integer", 10),
        )
        id2 = collections_db.create_element(
            "Collection",
            Element().set("label", "Item2").set("some_integer", 20),
        )
        assert id1 != id2
        assert id1 > 0
        assert id2 > 0

    def test_create_with_float(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection",
            Element().set("label", "Item1").set("some_float", 3.14),
        )
        value = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert value is not None
        assert abs(value - 3.14) < 1e-9

    def test_create_with_set_array(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection",
            Element().set("label", "Item1"),
        )
        collections_db.update_element("Collection", elem_id, Element().set("tag", ["a", "b"]))
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert sorted(result) == ["a", "b"]

    def test_create_with_fk_label(self, relations_db: Database) -> None:
        """FK label resolution: string value for FK column resolves to parent ID."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        child_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", "Parent 1"),
        )
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
        assert result == 1


class TestFKResolutionCreate:
    """FK label resolution tests for create_element -- ported from Julia/Dart."""

    def test_scalar_fk_label(self, relations_db: Database) -> None:
        """Scalar FK: string label resolves to parent ID."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        child_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", "Parent 1"),
        )
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
        assert result == 1

    def test_scalar_fk_integer(self, relations_db: Database) -> None:
        """Scalar FK: integer value passed through as-is."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        child_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", 1),
        )
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
        assert result == 1

    def test_vector_fk_labels(self, relations_db: Database) -> None:
        """Vector FK: string labels resolve to parent IDs."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        child_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_ref", ["Parent 1", "Parent 2"]),
        )
        result = relations_db.read_vector_integers_by_id("Child", "parent_ref", child_id)
        assert result == [1, 2]

    def test_set_fk_labels(self, relations_db: Database) -> None:
        """Set FK: string labels resolve to parent IDs."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        child_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("mentor_id", ["Parent 1", "Parent 2"]),
        )
        result = relations_db.read_set_integers_by_id("Child", "mentor_id", child_id)
        assert sorted(result) == [1, 2]

    def test_time_series_fk_labels(self, relations_db: Database) -> None:
        """Time series FK: string labels resolve to parent IDs."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        child_id = relations_db.create_element(
            "Child",
            Element()
            .set("label", "Child 1")
            .set("date_time", ["2024-01-01", "2024-01-02"])
            .set("sponsor_id", ["Parent 1", "Parent 2"]),
        )
        rows = relations_db.read_time_series_group("Child", "events", child_id)
        sponsor_ids = [row["sponsor_id"] for row in rows]
        assert sponsor_ids == [1, 2]

    def test_all_fk_types_in_one_call(self, relations_db: Database) -> None:
        """All FK types resolved in a single create_element call."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        relations_db.create_element("Parent", Element().set("label", "Parent 2"))
        child_id = relations_db.create_element(
            "Child",
            Element()
            .set("label", "Child 1")
            .set("parent_id", "Parent 1")
            .set("mentor_id", ["Parent 2"])
            .set("parent_ref", ["Parent 1"])
            .set("date_time", ["2024-01-01"])
            .set("sponsor_id", ["Parent 2"]),
        )
        # Verify scalar FK
        assert relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id) == 1
        # Verify set FK
        assert sorted(relations_db.read_set_integers_by_id("Child", "mentor_id", child_id)) == [2]
        # Verify vector FK
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child_id) == [1]
        # Verify time series FK
        rows = relations_db.read_time_series_group("Child", "events", child_id)
        assert [row["sponsor_id"] for row in rows] == [2]

    def test_no_fk_columns_unchanged(self, db: Database) -> None:
        """No FK columns: pre-resolve pass is a no-op for non-FK schemas."""
        db.create_element(
            "Configuration",
            Element().set("label", "Config 1").set("integer_attribute", 42).set("float_attribute", 3.14),
        )
        assert db.read_scalar_strings("Configuration", "label") == ["Config 1"]
        assert db.read_scalar_integers("Configuration", "integer_attribute") == [42]
        floats = db.read_scalar_floats("Configuration", "float_attribute")
        assert len(floats) == 1
        assert abs(floats[0] - 3.14) < 1e-9

    def test_missing_target_label_error(self, relations_db: Database) -> None:
        """Missing FK target label raises QuiverError."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        # No Parent created
        with pytest.raises(QuiverError):
            relations_db.create_element(
                "Child",
                Element().set("label", "Child 1").set("mentor_id", ["Nonexistent Parent"]),
            )

    def test_non_fk_string_rejection_error(self, relations_db: Database) -> None:
        """String value for non-FK integer column raises QuiverError."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        with pytest.raises(QuiverError):
            relations_db.create_element(
                "Child",
                Element().set("label", "Child 1").set("parent_id", 1).set("score", ["not_a_label"]),
            )

    def test_resolution_failure_no_partial_writes(self, relations_db: Database) -> None:
        """Failed FK resolution causes no partial writes (atomicity)."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        # No Parent created
        with pytest.raises(QuiverError):
            relations_db.create_element(
                "Child",
                Element().set("label", "Orphan Child").set("parent_id", "Nonexistent Parent"),
            )
        # Verify no child was created
        labels = relations_db.read_scalar_strings("Child", "label")
        assert labels == []

    def test_self_reference_fk(self, relations_db: Database) -> None:
        """Self-reference FK: sibling_id references same collection."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        relations_db.create_element("Parent", Element().set("label", "Parent 1"))
        child_1_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 1").set("parent_id", 1),
        )
        child_2_id = relations_db.create_element(
            "Child",
            Element().set("label", "Child 2").set("sibling_id", "Child 1"),
        )
        result = relations_db.read_scalar_integer_by_id("Child", "sibling_id", child_2_id)
        assert result == child_1_id

    def test_empty_collection_no_fk_data(self, relations_db: Database) -> None:
        """Empty collection with no FK data returns empty list."""
        relations_db.create_element("Configuration", Element().set("label", "cfg"))
        # No Parent, no Child created
        result = relations_db.read_scalar_integers("Child", "parent_id")
        assert result == []
