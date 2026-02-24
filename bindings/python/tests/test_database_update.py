"""Tests for update_element, scalar/vector/set update operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, Element


class TestUpdateElement:
    def test_update_single_scalar(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_integer", 10),
        )
        collections_db.update_element(
            "Collection", elem_id, Element().set("some_integer", 99),
        )
        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
        assert value == 99

    def test_update_preserves_other_attributes(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection",
            Element().set("label", "Item1").set("some_integer", 10).set("some_float", 2.5),
        )
        collections_db.update_element(
            "Collection", elem_id, Element().set("some_integer", 99),
        )
        # some_float should be unchanged
        float_val = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert float_val is not None
        assert abs(float_val - 2.5) < 1e-9


class TestUpdateScalar:
    def test_update_scalar_integer(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_integer", 10),
        )
        collections_db.update_scalar_integer("Collection", "some_integer", elem_id, 100)
        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
        assert value == 100

    def test_update_scalar_float(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1").set("some_float", 1.0),
        )
        collections_db.update_scalar_float("Collection", "some_float", elem_id, 9.99)
        value = collections_db.read_scalar_float_by_id("Collection", "some_float", elem_id)
        assert value is not None
        assert abs(value - 9.99) < 1e-9

    def test_update_scalar_string(self, db: Database) -> None:
        elem_id = db.create_element(
            "Configuration",
            Element().set("label", "item1").set("string_attribute", "hello"),
        )
        db.update_scalar_string("Configuration", "string_attribute", elem_id, "world")
        value = db.read_scalar_string_by_id("Configuration", "string_attribute", elem_id)
        assert value == "world"

    def test_update_scalar_string_to_none(self, db: Database) -> None:
        elem_id = db.create_element(
            "Configuration",
            Element().set("label", "item1").set("string_attribute", "hello"),
        )
        db.update_scalar_string("Configuration", "string_attribute", elem_id, None)
        value = db.read_scalar_string_by_id("Configuration", "string_attribute", elem_id)
        assert value is None


class TestUpdateVector:
    def test_update_vector_integers(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [10, 20, 30])
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id)
        assert result == [10, 20, 30]

    def test_update_vector_floats(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_floats("Collection", "value_float", elem_id, [1.1, 2.2, 3.3])
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", elem_id)
        assert len(result) == 3
        assert abs(result[0] - 1.1) < 1e-9
        assert abs(result[1] - 2.2) < 1e-9
        assert abs(result[2] - 3.3) < 1e-9

    def test_update_vector_empty_clears(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [10, 20])
        # Verify data exists
        assert collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id) == [10, 20]
        # Clear with empty list
        collections_db.update_vector_integers("Collection", "value_int", elem_id, [])
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", elem_id)
        assert result == []


class TestUpdateSet:
    def test_update_set_strings(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_set_strings("Collection", "tag", elem_id, ["tag1", "tag2"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert sorted(result) == ["tag1", "tag2"]

    def test_update_set_empty_clears(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem_id = collections_db.create_element(
            "Collection", Element().set("label", "Item1"),
        )
        collections_db.update_set_strings("Collection", "tag", elem_id, ["tag1", "tag2"])
        # Verify data exists
        assert sorted(collections_db.read_set_strings_by_id("Collection", "tag", elem_id)) == ["tag1", "tag2"]
        # Clear with empty list
        collections_db.update_set_strings("Collection", "tag", elem_id, [])
        result = collections_db.read_set_strings_by_id("Collection", "tag", elem_id)
        assert result == []


# -- String vector update (gap-fill) -----------------------------------------


class TestUpdateVectorStrings:
    def test_update_vector_strings(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes", Element().set("label", "Item1"),
        )
        all_types_db.update_vector_strings("AllTypes", "label_value", elem_id, ["alpha", "beta"])
        result = all_types_db.read_vector_strings_by_id("AllTypes", "label_value", elem_id)
        assert result == ["alpha", "beta"]


# -- Integer set update (gap-fill) ------------------------------------------


class TestUpdateSetIntegers:
    def test_update_set_integers(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes", Element().set("label", "Item1"),
        )
        all_types_db.update_set_integers("AllTypes", "code", elem_id, [10, 20, 30])
        result = all_types_db.read_set_integers_by_id("AllTypes", "code", elem_id)
        assert sorted(result) == [10, 20, 30]


# -- Float set update (gap-fill) -------------------------------------------


class TestUpdateSetFloats:
    def test_update_set_floats(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes", Element().set("label", "Item1"),
        )
        all_types_db.update_set_floats("AllTypes", "weight", elem_id, [1.1, 2.2])
        result = all_types_db.read_set_floats_by_id("AllTypes", "weight", elem_id)
        assert len(result) == 2
        assert any(abs(v - 1.1) < 1e-9 for v in result)
        assert any(abs(v - 2.2) < 1e-9 for v in result)
