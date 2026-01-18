"""Tests for delete operations."""
import pytest

from psr_database import Database


class TestDeleteElement:
    """Tests for delete_element_by_id."""

    def test_delete_element(self, basic_db: Database) -> None:
        """Delete an element by ID."""
        id1 = basic_db.create_element("Configuration", {"label": "to_delete"})

        basic_db.delete_element_by_id("Configuration", id1)

        ids = basic_db.read_element_ids("Configuration")
        assert id1 not in ids

    def test_delete_preserves_other_elements(self, basic_db: Database) -> None:
        """Deleting one element preserves others."""
        id1 = basic_db.create_element("Configuration", {"label": "keep1"})
        id2 = basic_db.create_element("Configuration", {"label": "delete"})
        id3 = basic_db.create_element("Configuration", {"label": "keep2"})

        basic_db.delete_element_by_id("Configuration", id2)

        ids = basic_db.read_element_ids("Configuration")
        assert id1 in ids
        assert id2 not in ids
        assert id3 in ids

    def test_delete_cascades_vectors(self, collections_db: Database) -> None:
        """Deleting element cascades to vector tables."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {
            "label": "with_vectors",
            "value_int": [1, 2, 3],
        })

        collections_db.delete_element_by_id("Collection", id1)

        ids = collections_db.read_element_ids("Collection")
        assert id1 not in ids

    def test_delete_cascades_sets(self, collections_db: Database) -> None:
        """Deleting element cascades to set tables."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {
            "label": "with_tags",
            "tag": ["a", "b", "c"],
        })

        collections_db.delete_element_by_id("Collection", id1)

        ids = collections_db.read_element_ids("Collection")
        assert id1 not in ids
