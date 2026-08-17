"""Tests for delete_element operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, QuiverError


class TestDeleteElement:
    def test_delete_element(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.delete_element("Collection", elem_id)
        ids = collections_db.read_element_ids("Collection")
        assert elem_id not in ids

    def test_delete_one_of_many(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        id1 = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        id2 = collections_db.create_element(
            "Collection",
            label="Item2",
        )
        collections_db.delete_element("Collection", id1)
        ids = collections_db.read_element_ids("Collection")
        assert id1 not in ids
        assert id2 in ids

    def test_delete_cascades_vectors(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
        )
        collections_db.update_element("Collection", elem_id, value_int=[10, 20, 30])
        # Verify vector data exists
        assert collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id) == [10, 20, 30]
        # Delete element -- cascade should clean up vectors
        collections_db.delete_element("Collection", elem_id)
        ids = collections_db.read_element_ids("Collection")
        assert elem_id not in ids

    def test_delete_nonexistent_raises(self, db: Database) -> None:
        db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError, match="Element not found"):
            db.delete_element("Configuration", 999)


class TestDeleteElementByLabel:
    def test_delete_element_by_label(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        id1 = collections_db.create_element("Collection", label="Item1")
        id2 = collections_db.create_element("Collection", label="Item2")
        collections_db.delete_element_by_label("Collection", "Item1")
        ids = collections_db.read_element_ids("Collection")
        assert id1 not in ids
        assert id2 in ids

    def test_delete_by_label_cascades_vectors(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element("Collection", label="Item1")
        collections_db.update_element("Collection", elem_id, value_int=[10, 20, 30])
        collections_db.delete_element_by_label("Collection", "Item1")
        assert collections_db.read_element_ids("Collection") == []
        # Cascade fired: no vector rows survive the element
        assert collections_db.query_integer("SELECT COUNT(*) FROM Collection_vector_values") == 0

    def test_delete_nonexistent_label_raises(self, db: Database) -> None:
        db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError, match="Element not found"):
            db.delete_element_by_label("Configuration", "nope")
        # Nothing was deleted
        assert len(db.read_element_ids("Configuration")) == 1

    def test_label_does_not_resolve_across_collections(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        collections_db.create_element("Collection", label="Item1")
        with pytest.raises(QuiverError, match="Element not found"):
            collections_db.delete_element_by_label("Collection", "cfg")
        assert len(collections_db.read_element_ids("Configuration")) == 1
        assert len(collections_db.read_element_ids("Collection")) == 1
