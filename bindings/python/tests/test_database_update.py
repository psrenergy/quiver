"""Tests for update_element, scalar/vector/set update operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, QuiverError


class TestUpdateElement:
    def test_update_single_scalar(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
            some_integer=10,
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            some_integer=99,
        )
        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
        assert value == 99

    def test_update_preserves_other_attributes(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
            some_integer=10,
            some_float=2.5,
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            some_integer=99,
        )
        # some_float should be unchanged
        float_val = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert float_val is not None
        assert abs(float_val - 2.5) < 1e-9


class TestUpdateElementByLabel:
    def test_update_element_by_label(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        id1 = collections_db.create_element("Collection", label="Item1", some_integer=10)
        id2 = collections_db.create_element("Collection", label="Item2", some_integer=20)
        collections_db.update_element_by_label("Collection", "Item1", some_integer=99)
        assert collections_db.read_scalar_integer_by_id("Collection", "some_integer", id1) == 99
        # The sibling element is untouched
        assert collections_db.read_scalar_integer_by_id("Collection", "some_integer", id2) == 20

    def test_rename_through_the_label_form(self, collections_db: Database) -> None:
        """`label=` in kwargs renames; the positional-only marker keeps it from colliding."""
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element("Collection", label="Item1")
        collections_db.update_element_by_label("Collection", "Item1", label="Renamed")
        assert collections_db.read_scalar_string_by_id("Collection", "label", elem_id) == "Renamed"
        # The old label no longer resolves; the new one does
        with pytest.raises(QuiverError, match="Element not found"):
            collections_db.update_element_by_label("Collection", "Item1", some_integer=5)
        collections_db.update_element_by_label("Collection", "Renamed", some_integer=5)

    def test_update_nonexistent_label_raises(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError, match="Element not found"):
            collections_db.update_element_by_label("Collection", "nope", some_integer=5)


class TestUpdateVector:
    def test_update_vector_integers(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            value_int=[10, 20, 30],
        )
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id)
        assert result == [10, 20, 30]

    def test_update_vector_floats(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            value_float=[1.1, 2.2, 3.3],
        )
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", elem_id)
        assert len(result) == 3
        assert abs(result[0] - 1.1) < 1e-9
        assert abs(result[1] - 2.2) < 1e-9
        assert abs(result[2] - 3.3) < 1e-9

    def test_update_vector_empty_clears(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            value_int=[10, 20],
        )
        # Verify data exists
        assert collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id) == [10, 20]
        # Clear with empty list
        collections_db.update_element(
            "Collection",
            elem_id,
            value_int=[],
        )
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id)
        assert result == []


class TestUpdateSet:
    def test_update_set_strings(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            tag=["tag1", "tag2"],
        )
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert sorted(result) == ["tag1", "tag2"]

    def test_update_set_empty_clears(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element(
            "Collection",
            elem_id,
            tag=["tag1", "tag2"],
        )
        # Verify data exists
        assert sorted(collections_db.read_set_strings_by_id("Collection", "tag", elem_id)) == ["tag1", "tag2"]
        # Clear with empty list
        collections_db.update_element(
            "Collection",
            elem_id,
            tag=[],
        )
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert result == []


class TestFKResolutionUpdate:
    """FK label resolution tests for update_element -- ported from Julia/Dart."""

    def test_scalar_fk_label(self, relations_db: Database) -> None:
        """Update scalar FK with string label resolves to new parent ID."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id="Parent 1",
        )
        relations_db.update_element("Child", 1, parent_id="Parent 2")
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
        assert result == 2

    def test_scalar_fk_integer(self, relations_db: Database) -> None:
        """Update scalar FK with integer value passed through as-is."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id=1,
        )
        relations_db.update_element("Child", 1, parent_id=2)
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
        assert result == 2

    def test_vector_fk_labels(self, relations_db: Database) -> None:
        """Update vector FK with string labels resolves to parent Ids."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        relations_db.create_element(
            "Child",
            label="Child 1",
            parent_ref=["Parent 1"],
        )
        relations_db.update_element(
            "Child",
            1,
            parent_ref=["Parent 2", "Parent 1"],
        )
        result = relations_db.read_vector_integers_by_id("Child", "parent_ref", 1)
        assert result == [2, 1]

    def test_set_fk_labels(self, relations_db: Database) -> None:
        """Update set FK with string labels resolves to parent Ids."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        relations_db.create_element(
            "Child",
            label="Child 1",
            mentor_id=["Parent 1"],
        )
        relations_db.update_element(
            "Child",
            1,
            mentor_id=["Parent 2"],
        )
        result = relations_db.read_set_integers_by_id("Child", "mentor_id", 1)
        assert result == [2]

    def test_time_series_fk_labels(self, relations_db: Database) -> None:
        """Update time series FK with string labels resolves to parent Ids."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        relations_db.create_element(
            "Child",
            label="Child 1",
            date_time=["2024-01-01"],
            sponsor_id=["Parent 1"],
        )
        relations_db.update_element(
            "Child",
            1,
            date_time=["2024-06-01", "2024-06-02"],
            sponsor_id=["Parent 2", "Parent 1"],
        )
        data = relations_db.read_time_series_group("Child", "events", 1)
        assert data["sponsor_id"] == [2, 1]

    def test_all_fk_types_in_one_call(self, relations_db: Database) -> None:
        """Update all FK types in a single update_element call."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element("Parent", label="Parent 2")
        # Create with all FK types pointing to Parent 1
        relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id="Parent 1",
            mentor_id=["Parent 1"],
            parent_ref=["Parent 1"],
            date_time=["2024-01-01"],
            sponsor_id=["Parent 1"],
        )
        # Update all FK types to Parent 2
        relations_db.update_element(
            "Child",
            1,
            parent_id="Parent 2",
            mentor_id=["Parent 2"],
            parent_ref=["Parent 2"],
            date_time=["2025-01-01"],
            sponsor_id=["Parent 2"],
        )
        # Verify scalar FK
        assert relations_db.read_scalar_integer_by_id("Child", "parent_id", 1) == 2
        # Verify set FK
        assert relations_db.read_set_integers_by_id("Child", "mentor_id", 1) == [2]
        # Verify vector FK
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", 1) == [2]
        # Verify time series FK
        data = relations_db.read_time_series_group("Child", "events", 1)
        assert data["sponsor_id"] == [2]

    def test_no_fk_columns_unchanged(self, db: Database) -> None:
        """No FK columns: update_element works normally for non-FK schemas."""
        db.create_element(
            "Configuration",
            label="Config 1",
            integer_attribute=42,
            float_attribute=3.14,
            string_attribute="hello",
        )
        db.update_element(
            "Configuration",
            1,
            integer_attribute=100,
            float_attribute=2.71,
            string_attribute="world",
        )
        assert db.read_scalar_integer_by_id("Configuration", "integer_attribute", 1) == 100
        float_val = db.read_scalar_float_by_id("Configuration", "float_attribute", 1)
        assert float_val is not None
        assert abs(float_val - 2.71) < 1e-9
        assert db.read_scalar_string_by_id("Configuration", "string_attribute", 1) == "world"

    def test_resolution_failure_preserves_existing(self, relations_db: Database) -> None:
        """Failed FK resolution preserves existing values."""
        relations_db.create_element("Configuration", label="cfg")
        relations_db.create_element("Parent", label="Parent 1")
        relations_db.create_element(
            "Child",
            label="Child 1",
            parent_id="Parent 1",
        )
        with pytest.raises(QuiverError):
            relations_db.update_element(
                "Child",
                1,
                parent_id="Nonexistent Parent",
            )
        # Verify original value preserved
        result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
        assert result == 1


class TestUpdateNotFoundAndTyping:
    def test_update_nonexistent_raises(self, db: Database) -> None:
        db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError, match="Element not found"):
            db.update_element("Configuration", 999, integer_attribute=5)

    def test_float_rejected_for_integer_column(self, db: Database) -> None:
        elem_id = db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError):
            db.update_element("Configuration", elem_id, integer_attribute=42.0)

    def test_integer_accepted_for_real_column(self, db: Database) -> None:
        elem_id = db.create_element("Configuration", label="cfg")
        db.update_element("Configuration", elem_id, float_attribute=7)
        assert db.read_scalar_float_by_id("Configuration", "float_attribute", elem_id) == 7.0


class TestUpdateVectorSetGroup:
    """relations.sql gives Child a vector group and a set group that legally share the FK column
    name "parent_ref" - the case that makes routing an array by column name ambiguous."""

    @staticmethod
    def _seed(db: Database) -> int:
        db.create_element("Configuration", label="Config")
        db.create_element("Parent", label="Parent A")
        db.create_element("Parent", label="Parent B")
        return db.create_element("Child", label="Child 1")

    def test_replaces_rows_and_clears_on_empty_dict(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [1, 2]})
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child) == [1, 2]

        relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [2]})
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child) == [2]

        relations_db.update_vector_group("Child", "refs", child, {})
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child) == []

    def test_leaves_sibling_group_sharing_a_column_name_untouched(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        relations_db.update_set_group("Child", "parents", child, {"parent_ref": [1]})
        relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [2]})

        assert relations_db.read_set_integers_by_id("Child", "parent_ref", child) == [1]
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child) == [2]

        relations_db.update_vector_group("Child", "refs", child, {})
        assert relations_db.read_set_integers_by_id("Child", "parent_ref", child) == [1]

    def test_resolves_foreign_key_labels(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        relations_db.update_vector_group("Child", "refs", child, {"parent_ref": ["Parent B"]})
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child) == [2]

    def test_accepts_null_cells(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [1, None, 2]})

        # Asserted in SQL, not through read_vector_group_by_id: Python composes that from
        # per-column reads, which drop NULL cells (the documented null-dropping caveat - only
        # Dart binds the NULL-preserving native reader).
        assert (
            relations_db.query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", parameters=[child]) == 3
        )
        assert (
            relations_db.query_integer(
                "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ? AND parent_ref IS NULL", parameters=[child]
            )
            == 1
        )

    def test_unknown_group_or_column_raises(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        with pytest.raises(QuiverError, match="Vector group not found"):
            relations_db.update_vector_group("Child", "nope", child, {"parent_ref": [1]})
        with pytest.raises(QuiverError, match="Set group not found"):
            relations_db.update_set_group("Child", "nope", child, {"parent_ref": [1]})
        with pytest.raises(QuiverError, match="not found in group"):
            relations_db.update_vector_group("Child", "refs", child, {"not_a_column": [1]})

    def test_structural_columns_rejected(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        # Both are derived (the element and the row's position); accepting them silently dropped
        # the caller's value, because SQLite keeps the first of a duplicated INSERT column.
        with pytest.raises(QuiverError, match="managed by the group table"):
            relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [1], "vector_index": [7]})
        with pytest.raises(QuiverError, match="managed by the group table"):
            relations_db.update_set_group("Child", "parents", child, {"parent_ref": [1], "id": [2]})

    def test_named_but_empty_column_raises_instead_of_clearing(self, relations_db: Database) -> None:
        child = self._seed(relations_db)
        relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [1]})

        # A typo'd column name must not destroy data: clearing is spelled {}.
        with pytest.raises(QuiverError, match="contain no rows"):
            relations_db.update_vector_group("Child", "refs", child, {"parent_ref": []})
        assert relations_db.read_vector_integers_by_id("Child", "parent_ref", child) == [1]

    def test_missing_element_raises_not_found(self, relations_db: Database) -> None:
        self._seed(relations_db)

        with pytest.raises(QuiverError, match="Element not found"):
            relations_db.update_vector_group("Child", "refs", 999, {"parent_ref": [1]})
        # The clear path used to succeed silently: the DELETE simply matched nothing.
        with pytest.raises(QuiverError, match="Element not found"):
            relations_db.update_set_group("Child", "parents", 999, {})

    def test_jagged_columns_rejected(self, relations_db: Database) -> None:
        child = self._seed(relations_db)

        with pytest.raises(ValueError, match="same length"):
            relations_db.update_vector_group("Child", "refs", child, {"parent_ref": [1, 2], "id": [1]})
