"""Tests for read operations."""
import pytest

from psr_database import Database


class TestReadScalars:
    """Tests for reading scalar attributes."""

    def test_read_scalar_integers(self, basic_db: Database) -> None:
        """Read integer scalar values."""
        basic_db.create_element("Configuration", {"label": "a", "integer_attribute": 10})
        basic_db.create_element("Configuration", {"label": "b", "integer_attribute": 20})
        basic_db.create_element("Configuration", {"label": "c", "integer_attribute": 30})

        values = basic_db.read_scalar_integers("Configuration", "integer_attribute")

        assert values == [10, 20, 30]

    def test_read_scalar_floats(self, basic_db: Database) -> None:
        """Read float scalar values."""
        basic_db.create_element("Configuration", {"label": "a", "float_attribute": 1.1})
        basic_db.create_element("Configuration", {"label": "b", "float_attribute": 2.2})

        values = basic_db.read_scalar_floats("Configuration", "float_attribute")

        assert len(values) == 2
        assert abs(values[0] - 1.1) < 0.001
        assert abs(values[1] - 2.2) < 0.001

    def test_read_scalar_strings(self, basic_db: Database) -> None:
        """Read string scalar values."""
        basic_db.create_element("Configuration", {"label": "x", "string_attribute": "hello"})
        basic_db.create_element("Configuration", {"label": "y", "string_attribute": "world"})

        values = basic_db.read_scalar_strings("Configuration", "string_attribute")

        assert values == ["hello", "world"]

    def test_read_empty_collection(self, basic_db: Database) -> None:
        """Read from empty collection."""
        values = basic_db.read_scalar_integers("Configuration", "integer_attribute")
        assert values == []


class TestReadScalarById:
    """Tests for reading scalar attributes by element ID."""

    def test_read_scalar_integer_by_id(self, basic_db: Database) -> None:
        """Read integer by element ID."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "integer_attribute": 42})

        value = basic_db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1)

        assert value == 42

    def test_read_scalar_float_by_id(self, basic_db: Database) -> None:
        """Read float by element ID."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "float_attribute": 3.14})

        value = basic_db.read_scalar_float_by_id("Configuration", "float_attribute", id1)

        assert value is not None
        assert abs(value - 3.14) < 0.001

    def test_read_scalar_string_by_id(self, basic_db: Database) -> None:
        """Read string by element ID."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "string_attribute": "test"})

        value = basic_db.read_scalar_string_by_id("Configuration", "string_attribute", id1)

        assert value == "test"

    def test_read_null_value_by_id(self, basic_db: Database) -> None:
        """Read null value by element ID."""
        id1 = basic_db.create_element("Configuration", {"label": "a", "integer_attribute": None})

        value = basic_db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1)

        assert value is None


class TestReadVectors:
    """Tests for reading vector attributes."""

    def test_read_vector_integers(self, collections_db: Database) -> None:
        """Read integer vectors."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        collections_db.create_element("Collection", {"label": "a", "value_int": [1, 2, 3]})
        collections_db.create_element("Collection", {"label": "b", "value_int": [4, 5]})

        vectors = collections_db.read_vector_integers("Collection", "value_int")

        assert vectors == [[1, 2, 3], [4, 5]]

    def test_read_vector_floats(self, collections_db: Database) -> None:
        """Read float vectors."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        collections_db.create_element("Collection", {"label": "a", "value_float": [1.1, 2.2]})

        vectors = collections_db.read_vector_floats("Collection", "value_float")

        assert len(vectors) == 1
        assert len(vectors[0]) == 2
        assert abs(vectors[0][0] - 1.1) < 0.001

    def test_read_element_with_no_vector_data(self, collections_db: Database) -> None:
        """Read from collection where element has no vector data."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        collections_db.create_element("Collection", {"label": "a"})

        vectors = collections_db.read_vector_integers("Collection", "value_int")

        # Element exists but has no vector data - returns empty list
        # (vector data not stored means no rows in vector table)
        assert vectors == []


class TestReadVectorById:
    """Tests for reading vectors by element ID."""

    def test_read_vector_integers_by_id(self, collections_db: Database) -> None:
        """Read integer vector by element ID."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {"label": "a", "value_int": [10, 20, 30]})

        values = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)

        assert values == [10, 20, 30]


class TestReadSets:
    """Tests for reading set attributes."""

    def test_read_set_strings(self, collections_db: Database) -> None:
        """Read string sets."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        collections_db.create_element("Collection", {"label": "a", "tag": ["x", "y"]})
        collections_db.create_element("Collection", {"label": "b", "tag": ["z"]})

        sets = collections_db.read_set_strings("Collection", "tag")

        assert len(sets) == 2
        assert set(sets[0]) == {"x", "y"}
        assert set(sets[1]) == {"z"}


class TestReadSetById:
    """Tests for reading sets by element ID."""

    def test_read_set_strings_by_id(self, collections_db: Database) -> None:
        """Read string set by element ID."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        id1 = collections_db.create_element("Collection", {"label": "a", "tag": ["a", "b", "c"]})

        values = collections_db.read_set_strings_by_id("Collection", "tag", id1)

        assert set(values) == {"a", "b", "c"}


class TestReadElementIds:
    """Tests for reading element IDs."""

    def test_read_element_ids(self, basic_db: Database) -> None:
        """Read all element IDs from collection."""
        id1 = basic_db.create_element("Configuration", {"label": "a"})
        id2 = basic_db.create_element("Configuration", {"label": "b"})
        id3 = basic_db.create_element("Configuration", {"label": "c"})

        ids = basic_db.read_element_ids("Configuration")

        assert set(ids) == {id1, id2, id3}

    def test_read_element_ids_empty(self, basic_db: Database) -> None:
        """Read element IDs from empty collection."""
        ids = basic_db.read_element_ids("Configuration")
        assert ids == []


class TestGetAttributeType:
    """Tests for getting attribute type information."""

    def test_get_scalar_integer_type(self, basic_db: Database) -> None:
        """Get type of scalar integer attribute."""
        structure, dtype = basic_db.get_attribute_type("Configuration", "integer_attribute")

        assert structure == "scalar"
        assert dtype == "integer"

    def test_get_scalar_float_type(self, basic_db: Database) -> None:
        """Get type of scalar float attribute."""
        structure, dtype = basic_db.get_attribute_type("Configuration", "float_attribute")

        assert structure == "scalar"
        assert dtype == "real"

    def test_get_scalar_string_type(self, basic_db: Database) -> None:
        """Get type of scalar string attribute."""
        structure, dtype = basic_db.get_attribute_type("Configuration", "string_attribute")

        assert structure == "scalar"
        assert dtype == "text"

    def test_get_vector_integer_type(self, collections_db: Database) -> None:
        """Get type of vector integer attribute."""
        structure, dtype = collections_db.get_attribute_type("Collection", "value_int")

        assert structure == "vector"
        assert dtype == "integer"

    def test_get_set_string_type(self, collections_db: Database) -> None:
        """Get type of set string attribute."""
        structure, dtype = collections_db.get_attribute_type("Collection", "tag")

        assert structure == "set"
        assert dtype == "text"
