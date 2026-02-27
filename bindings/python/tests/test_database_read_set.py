"""Tests for set read operations (bulk and by-id)."""

from __future__ import annotations

from datetime import datetime, timezone

from quiverdb import Database


# -- Set reads by ID ----------------------------------------------------------


class TestReadSetStringsByID:
    def test_read_set_strings_by_id(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, tag=["alpha", "beta"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert sorted(result) == ["alpha", "beta"]

    def test_read_set_strings_by_id_empty(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10)
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert result == []

    def test_read_set_strings_by_id_single(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, tag=["only"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert result == ["only"]

    def test_read_set_returns_list_not_set(self, collections_db: Database) -> None:
        """Verify that set reads return list type, not Python set."""
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, tag=["a", "b"])
        result = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert isinstance(result, list)


# -- Bulk set reads ------------------------------------------------------------


class TestReadSetStringsBulk:
    def test_read_set_strings(self, collections_db: Database) -> None:
        collections_db.create_element("Collection", label="item1", some_integer=10, tag=["alpha", "beta"])
        collections_db.create_element("Collection", label="item2", some_integer=20, tag=["gamma"])
        result = collections_db.read_set_strings("Collection", "tag")
        assert len(result) == 2
        assert sorted(result[0]) == ["alpha", "beta"]
        assert result[1] == ["gamma"]

    def test_read_set_strings_empty_collection(self, collections_db: Database) -> None:
        result = collections_db.read_set_strings("Collection", "tag")
        assert result == []

    def test_read_set_strings_with_empty_set(self, collections_db: Database) -> None:
        # C++ bulk read skips elements with no set data (same as scalar NULL skipping)
        collections_db.create_element("Collection", label="item1", some_integer=10, tag=["alpha"])
        collections_db.create_element("Collection", label="item2", some_integer=20)
        result = collections_db.read_set_strings("Collection", "tag")
        assert len(result) == 1
        assert result[0] == ["alpha"]


# -- Convenience set reads ---------------------------------------------------


class TestReadAllSetsByID:
    def test_read_all_sets_by_id_no_groups(self, db: Database) -> None:
        """read_all_sets_by_id returns empty dict for collections with no set groups."""
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_all_sets_by_id("Configuration", id1)
        assert result == {}


class TestReadSetGroupByID:
    def test_read_set_group_by_id(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, tag=["alpha", "beta"])
        result = collections_db.read_set_group_by_id("Collection", "tags", id1)
        assert isinstance(result, list)
        # Each row is a dict with column name "tag"
        tags = [row["tag"] for row in result]
        assert sorted(tags) == ["alpha", "beta"]

    def test_read_set_group_by_id_empty(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10)
        result = collections_db.read_set_group_by_id("Collection", "tags", id1)
        assert result == []


# -- Integer set reads (gap-fill) --------------------------------------------


class TestReadSetIntegersBulk:
    def test_read_set_integers(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        id2 = all_types_db.create_element("AllTypes", label="item2")
        all_types_db.update_element("AllTypes", id1, code=[10, 20, 30])
        all_types_db.update_element("AllTypes", id2, code=[40, 50])
        result = all_types_db.read_set_integers("AllTypes", "code")
        assert len(result) == 2
        assert sorted(result[0]) == [10, 20, 30]
        assert sorted(result[1]) == [40, 50]


class TestReadSetIntegersByID:
    def test_read_set_integers_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element("AllTypes", id1, code=[100, 200, 300])
        result = all_types_db.read_set_integers_by_id("AllTypes", "code", id1)
        assert sorted(result) == [100, 200, 300]


# -- Float set reads (gap-fill) ---------------------------------------------


class TestReadSetFloatsBulk:
    def test_read_set_floats(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        id2 = all_types_db.create_element("AllTypes", label="item2")
        all_types_db.update_element("AllTypes", id1, weight=[1.1, 2.2])
        all_types_db.update_element("AllTypes", id2, weight=[3.3, 4.4, 5.5])
        result = all_types_db.read_set_floats("AllTypes", "weight")
        assert len(result) == 2
        assert len(result[0]) == 2
        assert len(result[1]) == 3


class TestReadSetFloatsByID:
    def test_read_set_floats_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element("AllTypes", id1, weight=[9.9, 8.8])
        result = all_types_db.read_set_floats_by_id("AllTypes", "weight", id1)
        assert len(result) == 2
        assert any(abs(v - 9.9) < 1e-9 for v in result)
        assert any(abs(v - 8.8) < 1e-9 for v in result)


# -- DateTime set convenience (gap-fill) ------------------------------------


class TestReadSetDateTimeByID:
    def test_read_set_date_time_by_id(self, all_types_db: Database) -> None:
        """read_set_date_time_by_id wraps read_set_strings_by_id + datetime parsing."""
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element(
            "AllTypes",
            id1,
            tag=["2024-01-15T10:30:00", "2024-06-20T08:00:00"],
        )
        result = all_types_db.read_set_date_time_by_id("AllTypes", "tag", id1)
        assert len(result) == 2
        assert all(isinstance(dt, datetime) for dt in result)
        years = sorted(dt.month for dt in result)
        assert years == [1, 6]


# -- Convenience set reads with data (gap-fill) ------------------------------


class TestReadAllSetsByIDWithData:
    def test_read_all_sets_by_id_returns_all_groups(self, composite_helpers_db: Database) -> None:
        """read_all_sets_by_id returns dict with integer, float, and string set groups."""
        id1 = composite_helpers_db.create_element(
            "Items", label="item1", code=[10, 20, 30], weight=[1.1, 2.2], tag=["alpha", "beta"]
        )
        result = composite_helpers_db.read_all_sets_by_id("Items", id1)

        assert len(result) == 3
        assert sorted(result["code"]) == [10, 20, 30]
        assert len(result["weight"]) == 2
        assert any(abs(v - 1.1) < 1e-9 for v in result["weight"])
        assert any(abs(v - 2.2) < 1e-9 for v in result["weight"])
        assert sorted(result["tag"]) == ["alpha", "beta"]

    def test_read_all_sets_by_id_correct_types(self, composite_helpers_db: Database) -> None:
        """Each set group returns the correct Python type."""
        id1 = composite_helpers_db.create_element("Items", label="item1", code=[5], weight=[9.9], tag=["text"])
        result = composite_helpers_db.read_all_sets_by_id("Items", id1)

        assert all(isinstance(v, int) for v in result["code"])
        assert all(isinstance(v, float) for v in result["weight"])
        assert all(isinstance(v, str) for v in result["tag"])
