"""Tests for update operations."""
import pytest

from psr_database import Database


class TestUpdateElement:
    """Tests for update_element with dictionary."""

    def test_update_scalar_attributes(self, basic_db: Database) -> None:
        """Update scalar attributes using dictionary."""
        id1 = basic_db.create_element("Configuration", {
            "label": "test",
            "integer_attribute": 10,
            "string_attribute": "old",
        })

        basic_db.update_element("Configuration", id1, {
            "integer_attribute": 99,
            "string_attribute": "new",
        })

        assert basic_db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1) == 99
        assert basic_db.read_scalar_string_by_id("Configuration", "string_attribute", id1) == "new"


class TestUpdateScalars:
    """Tests for updating individual scalar attributes."""

    def test_update_scalar_integer(self, basic_db: Database) -> None:
        """Update a single integer attribute."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "integer_attribute": 1})

        basic_db.update_scalar_integer("Configuration", "integer_attribute", id1, 100)

        assert basic_db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1) == 100

    def test_update_scalar_float(self, basic_db: Database) -> None:
        """Update a single float attribute."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "float_attribute": 1.0})

        basic_db.update_scalar_float("Configuration", "float_attribute", id1, 9.99)

        value = basic_db.read_scalar_float_by_id("Configuration", "float_attribute", id1)
        assert value is not None
        assert abs(value - 9.99) < 0.001

    def test_update_scalar_string(self, basic_db: Database) -> None:
        """Update a single string attribute."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "string_attribute": "old"})

        basic_db.update_scalar_string("Configuration", "string_attribute", id1, "updated")

        assert basic_db.read_scalar_string_by_id("Configuration", "string_attribute", id1) == "updated"


class TestUpdateVectors:
    """Tests for updating vector attributes."""

    def test_update_vector_integers(self, collections_db: Database) -> None:
        """Update integer vector (replaces entire vector)."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {"label": "a", "value_int": [1, 2, 3]})

        collections_db.update_vector_integers("Collection", "value_int", id1, [10, 20, 30, 40])

        values = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert values == [10, 20, 30, 40]

    def test_update_vector_floats(self, collections_db: Database) -> None:
        """Update float vector (replaces entire vector)."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {"label": "a", "value_float": [1.0]})

        collections_db.update_vector_floats("Collection", "value_float", id1, [1.1, 2.2, 3.3])

        values = collections_db.read_vector_floats_by_id("Collection", "value_float", id1)
        assert len(values) == 3
        assert abs(values[0] - 1.1) < 0.001


class TestUpdateSets:
    """Tests for updating set attributes."""

    def test_update_set_strings(self, collections_db: Database) -> None:
        """Update string set (replaces entire set)."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {"label": "a", "tag": ["old1", "old2"]})

        collections_db.update_set_strings("Collection", "tag", id1, ["new1", "new2", "new3"])

        values = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert set(values) == {"new1", "new2", "new3"}

    def test_update_set_to_empty(self, collections_db: Database) -> None:
        """Update set to empty."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {"label": "a", "tag": ["x", "y"]})

        collections_db.update_set_strings("Collection", "tag", id1, [])

        values = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert values == []
