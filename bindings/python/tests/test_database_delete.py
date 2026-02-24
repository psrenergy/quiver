"""Tests for delete_element operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, Element


class TestDeleteElement:
    def test_delete_element(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.delete_element("Collection", elem_id)
        ids = collections_db.read_element_ids("Collection")
        assert elem_id not in ids

    def test_delete_one_of_many(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        id1 = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        id2 = collections_db.create_element(
            "Collection", Element().set("label", "Item2"),
        )
        collections_db.delete_element("Collection", id1)
        ids = collections_db.read_element_ids("Collection")
        assert id1 not in ids
        assert id2 in ids

    def test_delete_cascades_vectors(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [10, 20, 30])
        # Verify vector data exists
        assert collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id) == [10, 20, 30]
        # Delete element -- cascade should clean up vectors
        collections_db.delete_element("Collection", elem_id)
        ids = collections_db.read_element_ids("Collection")
        assert elem_id not in ids
