"""Tests for set read operations (bulk and by-id)."""

from __future__ import annotations

from quiver_db import Database, Element


# -- Set reads by ID ----------------------------------------------------------


class TestReadSetStringsByID:
    def test_read_set_strings_by_id(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10).set("tag", ["alpha", "beta"])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert sorted(result) == ["alpha", "beta"]

    def test_read_set_strings_by_id_empty(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10)
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert result == []

    def test_read_set_strings_by_id_single(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10).set("tag", ["only"])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert result == ["only"]

    def test_read_set_returns_list_not_set(self, collections_db: Database) -> None:
        """Verify that set reads return list type, not Python set."""
        elem = Element().set("label", "item1").set("some_integer", 10).set("tag", ["a", "b"])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert isinstance(result, list)


# -- Bulk set reads ------------------------------------------------------------


class TestReadSetStringsBulk:
    def test_read_set_strings(self, collections_db: Database) -> None:
        collections_db.create_element(
            "Collection",
            Element().set("label", "item1").set("some_integer", 10).set("tag", ["alpha", "beta"]),
        )
        collections_db.create_element(
            "Collection",
            Element().set("label", "item2").set("some_integer", 20).set("tag", ["gamma"]),
        )
        result = collections_db.read_set_strings("Collection", "tag")
        assert len(result) == 2
        assert sorted(result[0]) == ["alpha", "beta"]
        assert result[1] == ["gamma"]

    def test_read_set_strings_empty_collection(self, collections_db: Database) -> None:
        result = collections_db.read_set_strings("Collection", "tag")
        assert result == []

    def test_read_set_strings_with_empty_set(self, collections_db: Database) -> None:
        # C++ bulk read skips elements with no set data (same as scalar NULL skipping)
        collections_db.create_element(
            "Collection",
            Element().set("label", "item1").set("some_integer", 10).set("tag", ["alpha"]),
        )
        collections_db.create_element(
            "Collection",
            Element().set("label", "item2").set("some_integer", 20),
        )
        result = collections_db.read_set_strings("Collection", "tag")
        assert len(result) == 1
        assert result[0] == ["alpha"]


# -- Convenience set reads ---------------------------------------------------


class TestReadAllSetsByID:
    def test_read_all_sets_by_id_no_groups(self, db: Database) -> None:
        """read_all_sets_by_id returns empty dict for collections with no set groups."""
        id1 = db.create_element("Configuration", Element().set("label", "item1"))
        result = db.read_all_sets_by_id("Configuration", id1)
        assert result == {}


class TestReadSetGroupByID:
    def test_read_set_group_by_id(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10).set("tag", ["alpha", "beta"])
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_set_group_by_id("Collection", "tags", id1)
        assert isinstance(result, list)
        # Each row is a dict with column name "tag"
        tags = [row["tag"] for row in result]
        assert sorted(tags) == ["alpha", "beta"]

    def test_read_set_group_by_id_empty(self, collections_db: Database) -> None:
        elem = Element().set("label", "item1").set("some_integer", 10)
        id1 = collections_db.create_element("Collection", elem)
        result = collections_db.read_set_group_by_id("Collection", "tags", id1)
        assert result == []
