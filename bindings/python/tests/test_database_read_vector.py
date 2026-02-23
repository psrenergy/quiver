"""Tests for vector read operations (bulk and by-id)."""

from __future__ import annotations

from quiver import Database, Element


# -- Vector reads by ID -------------------------------------------------------


class TestReadVectorIntegersByID:
    def test_read_vector_integers_by_id(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10).set("value_int", [1, 2, 3])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert result == [1, 2, 3]

    def test_read_vector_integers_by_id_empty(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10)
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert result == []

    def test_read_vector_integers_by_id_single(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10).set("value_int", [42])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert result == [42]


class TestReadVectorFloatsByID:
    def test_read_vector_floats_by_id(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10).set("value_float", [1.5, 2.5, 3.5])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", id1)
        assert len(result) == 3
        assert abs(result[0] - 1.5) < 1e-9
        assert abs(result[1] - 2.5) < 1e-9
        assert abs(result[2] - 3.5) < 1e-9

    def test_read_vector_floats_by_id_empty(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10)
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", id1)
        assert result == []


# -- Bulk vector reads ---------------------------------------------------------


class TestReadVectorIntegersBulk:
    def test_read_vector_integers(self, collections_db: Database) -> None:
        collections_db.create_element(
            "Collection",
            Element().set("label", "item1").set("some_integer", 10).set("value_int", [1, 2]),
        )
        collections_db.create_element(
            "Collection",
            Element().set("label", "item2").set("some_integer", 20).set("value_int", [3, 4, 5]),
        )
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert len(result) == 2
        assert result[0] == [1, 2]
        assert result[1] == [3, 4, 5]

    def test_read_vector_integers_empty_collection(self, collections_db: Database) -> None:
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert result == []

    def test_read_vector_integers_with_empty_vector(self, collections_db: Database) -> None:
        # C++ bulk read skips elements with no vector data (same as scalar NULL skipping)
        collections_db.create_element(
            "Collection",
            Element().set("label", "item1").set("some_integer", 10).set("value_int", [1, 2]),
        )
        collections_db.create_element(
            "Collection",
            Element().set("label", "item2").set("some_integer", 20),
        )
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert len(result) == 1
        assert result[0] == [1, 2]


class TestReadVectorFloatsBulk:
    def test_read_vector_floats(self, collections_db: Database) -> None:
        collections_db.create_element(
            "Collection",
            Element().set("label", "item1").set("some_integer", 10).set("value_float", [1.1, 2.2]),
        )
        collections_db.create_element(
            "Collection",
            Element().set("label", "item2").set("some_integer", 20).set("value_float", [3.3]),
        )
        result = collections_db.read_vector_floats("Collection", "value_float")
        assert len(result) == 2
        assert len(result[0]) == 2
        assert abs(result[0][0] - 1.1) < 1e-9
        assert abs(result[0][1] - 2.2) < 1e-9
        assert len(result[1]) == 1
        assert abs(result[1][0] - 3.3) < 1e-9
