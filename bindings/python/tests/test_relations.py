"""Tests for relation operations."""
import pytest

from psr_database import Database


class TestScalarRelations:
    """Tests for scalar relation (foreign key) operations."""

    def test_set_and_read_scalar_relation(self, relations_db: Database) -> None:
        """Set and read a scalar relation."""
        # Create parent elements
        relations_db.create_element("Parent", {"label": "parent1"})
        relations_db.create_element("Parent", {"label": "parent2"})

        # Create child elements
        relations_db.create_element("Child", {"label": "child1"})
        relations_db.create_element("Child", {"label": "child2"})

        # Set relations
        relations_db.set_scalar_relation("Child", "parent_id", "child1", "parent1")
        relations_db.set_scalar_relation("Child", "parent_id", "child2", "parent2")

        # Read relations
        relations = relations_db.read_scalar_relation("Child", "parent_id")

        assert len(relations) == 2
        assert "parent1" in relations
        assert "parent2" in relations

    def test_read_null_relations(self, relations_db: Database) -> None:
        """Read relations where some elements have no relation set."""
        relations_db.create_element("Parent", {"label": "parent1"})
        relations_db.create_element("Child", {"label": "child1"})
        relations_db.create_element("Child", {"label": "child2"})

        # Only set relation for child1
        relations_db.set_scalar_relation("Child", "parent_id", "child1", "parent1")

        relations = relations_db.read_scalar_relation("Child", "parent_id")

        assert len(relations) == 2
        assert "parent1" in relations
        # child2 has no relation, so it should be None
        assert None in relations

    def test_self_referencing_relation(self, relations_db: Database) -> None:
        """Test self-referencing relation (sibling_id)."""
        relations_db.create_element("Child", {"label": "child1"})
        relations_db.create_element("Child", {"label": "child2"})

        relations_db.set_scalar_relation("Child", "sibling_id", "child1", "child2")

        relations = relations_db.read_scalar_relation("Child", "sibling_id")

        assert "child2" in relations
