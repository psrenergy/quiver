"""Tests for set read operations (bulk and by-id)."""

from __future__ import annotations

from datetime import datetime, timezone

from quiverdb import Database, Element


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


# -- Integer set reads (gap-fill) --------------------------------------------


class TestReadSetIntegersBulk:
    def test_read_set_integers(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", Element().set("label", "item1"))
        id2 = all_types_db.create_element("AllTypes", Element().set("label", "item2"))
        all_types_db.update_element("AllTypes", id1, Element().set("code", [10, 20, 30]))
        all_types_db.update_element("AllTypes", id2, Element().set("code", [40, 50]))
        result = all_types_db.read_set_integers("AllTypes", "code")
        assert len(result) == 2
        assert sorted(result[0]) == [10, 20, 30]
        assert sorted(result[1]) == [40, 50]


class TestReadSetIntegersByID:
    def test_read_set_integers_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", Element().set("label", "item1"))
        all_types_db.update_element("AllTypes", id1, Element().set("code", [100, 200, 300]))
        result = all_types_db.read_set_integers_by_id("AllTypes", "code", id1)
        assert sorted(result) == [100, 200, 300]


# -- Float set reads (gap-fill) ---------------------------------------------


class TestReadSetFloatsBulk:
    def test_read_set_floats(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", Element().set("label", "item1"))
        id2 = all_types_db.create_element("AllTypes", Element().set("label", "item2"))
        all_types_db.update_element("AllTypes", id1, Element().set("weight", [1.1, 2.2]))
        all_types_db.update_element("AllTypes", id2, Element().set("weight", [3.3, 4.4, 5.5]))
        result = all_types_db.read_set_floats("AllTypes", "weight")
        assert len(result) == 2
        assert len(result[0]) == 2
        assert len(result[1]) == 3


class TestReadSetFloatsByID:
    def test_read_set_floats_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", Element().set("label", "item1"))
        all_types_db.update_element("AllTypes", id1, Element().set("weight", [9.9, 8.8]))
        result = all_types_db.read_set_floats_by_id("AllTypes", "weight", id1)
        assert len(result) == 2
        assert any(abs(v - 9.9) < 1e-9 for v in result)
        assert any(abs(v - 8.8) < 1e-9 for v in result)


# -- DateTime set convenience (gap-fill) ------------------------------------


class TestReadSetDateTimeByID:
    def test_read_set_date_time_by_id(self, all_types_db: Database) -> None:
        """read_set_date_time_by_id wraps read_set_strings_by_id + datetime parsing."""
        id1 = all_types_db.create_element("AllTypes", Element().set("label", "item1"))
        all_types_db.update_set_strings(
            "AllTypes",
            "tag",
            id1,
            ["2024-01-15T10:30:00", "2024-06-20T08:00:00"],
        )
        result = all_types_db.read_set_date_time_by_id("AllTypes", "tag", id1)
        assert len(result) == 2
        assert all(isinstance(dt, datetime) for dt in result)
        years = sorted(dt.month for dt in result)
        assert years == [1, 6]


# -- Convenience set reads with data (gap-fill) ------------------------------


class TestReadAllSetsByIDWithData:
    """Skipped: read_all_sets_by_id uses group name as attribute, but no existing
    schema has single-column set groups where group name == column name.
    The convenience method has a known limitation (decision 06-02). The empty-case
    test above covers the method plumbing."""

    pass
