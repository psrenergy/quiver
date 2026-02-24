"""Tests for create_element operations."""

from __future__ import annotations

import pytest

from quiver_db import Database, Element


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
            "Collection", Element().set("label", "Item1").set("some_integer", 10),
        )
        id2 = collections_db.create_element(
            "Collection", Element().set("label", "Item2").set("some_integer", 20),
        )
        assert id1 != id2
        assert id1 > 0
        assert id2 > 0

    def test_create_with_float(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_float", 3.14),
        )
        value = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert value is not None
        assert abs(value - 3.14) < 1e-9

    def test_create_with_set_array(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_set_strings("Collection", "tag", elem_id, ["a", "b"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert sorted(result) == ["a", "b"]
