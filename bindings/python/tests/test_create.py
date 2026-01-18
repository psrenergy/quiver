"""Tests for element creation operations."""
import pytest

from psr_database import Database, Element


class TestCreateElement:
    """Tests for create_element and create_element_from_builder."""

    def test_create_element_with_dict(self, basic_db: Database) -> None:
        """Create element using dictionary."""
        element_id = basic_db.create_element("Configuration", {
            "label": "test_config",
            "integer_attribute": 42,
            "float_attribute": 3.14,
            "string_attribute": "hello",
        })

        assert element_id > 0

    def test_create_element_with_builder(self, basic_db: Database) -> None:
        """Create element using Element builder."""
        with Element() as element:
            element.set("label", "test_config")
            element.set("integer_attribute", 100)
            element_id = basic_db.create_element_from_builder("Configuration", element)

        assert element_id > 0

    def test_create_multiple_elements(self, basic_db: Database) -> None:
        """Create multiple elements."""
        id1 = basic_db.create_element("Configuration", {"label": "config1"})
        id2 = basic_db.create_element("Configuration", {"label": "config2"})
        id3 = basic_db.create_element("Configuration", {"label": "config3"})

        assert id1 > 0
        assert id2 > 0
        assert id3 > 0
        assert id1 != id2 != id3

    def test_create_element_with_null(self, basic_db: Database) -> None:
        """Create element with null values."""
        element_id = basic_db.create_element("Configuration", {
            "label": "null_test",
            "integer_attribute": None,
            "float_attribute": None,
            "string_attribute": None,
        })

        assert element_id > 0


class TestCreateElementWithCollections:
    """Tests for creating elements with vectors and sets."""

    def test_create_element_with_integer_vector(self, collections_db: Database) -> None:
        """Create element with integer vector attribute."""
        # Configuration must be created first (schema requirement)
        collections_db.create_element("Configuration", {"label": "test_config"})
        element_id = collections_db.create_element("Collection", {
            "label": "with_vector",
            "value_int": [1, 2, 3, 4, 5],
        })

        assert element_id > 0

    def test_create_element_with_float_vector(self, collections_db: Database) -> None:
        """Create element with float vector attribute."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        element_id = collections_db.create_element("Collection", {
            "label": "with_floats",
            "value_float": [1.1, 2.2, 3.3],
        })

        assert element_id > 0

    def test_create_element_with_string_set(self, collections_db: Database) -> None:
        """Create element with string set attribute."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        element_id = collections_db.create_element("Collection", {
            "label": "with_tags",
            "tag": ["tag1", "tag2", "tag3"],
        })

        assert element_id > 0


class TestElementBuilder:
    """Tests for the Element builder class."""

    def test_element_fluent_api(self) -> None:
        """Test fluent API (method chaining)."""
        with Element() as element:
            result = element.set("a", 1).set("b", 2).set("c", 3)
            assert result is element

    def test_element_type_dispatch(self) -> None:
        """Test automatic type dispatch in set()."""
        with Element() as element:
            element.set("int_val", 42)
            element.set("float_val", 3.14)
            element.set("str_val", "hello")
            element.set("null_val", None)
            element.set("bool_val", True)
            element.set("int_array", [1, 2, 3])
            element.set("float_array", [1.1, 2.2])
            element.set("str_array", ["a", "b"])

    def test_element_context_manager(self) -> None:
        """Test element context manager."""
        with Element() as element:
            element.set("label", "test")
        # Element should be disposed after exiting context

    def test_element_dispose(self) -> None:
        """Test manual dispose."""
        element = Element()
        element.set("label", "test")
        element.dispose()
        # Should not raise on double dispose
        element.dispose()
