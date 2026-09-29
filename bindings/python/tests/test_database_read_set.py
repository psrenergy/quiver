"""Tests for set read operations (bulk and by-id)."""

from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone

import pytest

from quiverdb import Database

# -- Set reads by ID ----------------------------------------------------------


class TestReadSetStringsById:
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
        # One entry per element: an element with no set data reads back as an empty list
        collections_db.create_element("Collection", label="item1", some_integer=10, tag=["alpha"])
        collections_db.create_element("Collection", label="item2", some_integer=20)
        result = collections_db.read_set_strings("Collection", "tag")
        assert len(result) == 2
        assert result[0] == ["alpha"]
        assert result[1] == []


class TestReadSetDateTimesBulk:
    def test_read_set_date_times(self, all_types_db: Database) -> None:
        assert all_types_db.read_set_date_times("AllTypes", "tag") == []

        all_types_db.create_element(
            "AllTypes",
            label="item1",
            tag=["2024-01-15T10:30:00", "2024-01-16"],
        )
        all_types_db.create_element(
            "AllTypes",
            label="item2",
            tag=["2024-06-20 14:45:30"],
        )
        all_types_db.create_element("AllTypes", label="no set")

        result = all_types_db.read_set_date_times("AllTypes", "tag")
        assert len(result) == 3
        assert sorted(result[0]) == [
            datetime(2024, 1, 15, 10, 30, tzinfo=timezone.utc),
            datetime(2024, 1, 16, tzinfo=timezone.utc),
        ]
        assert result[1] == [datetime(2024, 6, 20, 14, 45, 30, tzinfo=timezone.utc)]
        assert result[2] == []

    def test_rejects_a_malformed_cell_naming_the_column(self, all_types_db: Database) -> None:
        all_types_db.create_element("AllTypes", label="item1", tag=["2024-01-15", "20240115"])

        with pytest.raises(ValueError, match=r"AllTypes\.tag.*expected a valid YYYY-MM-DD"):
            all_types_db.read_set_date_times("AllTypes", "tag")

        with pytest.raises(ValueError, match=r"AllTypes\.tag"):
            all_types_db.read_set_date_time_by_id("AllTypes", "tag", 1)


# -- Convenience set reads ---------------------------------------------------


class TestReadSetsById:
    def test_read_sets_by_id_no_groups(self, db: Database) -> None:
        """read_sets_by_id returns empty dict for collections with no set groups."""
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_sets_by_id("Configuration", id1)
        assert result == {}


class TestReadSetGroupById:
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

    def test_null_cells_keep_rows_aligned(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        db.update_set_group("Items", "codes", item, {"code": ["alpha", None, "mu"], "weight": [1.5, 2.5, None]})

        rows = db.read_set_group_by_id("Items", "codes", item)
        assert len(rows) == 3
        # A set's row order is unspecified: compare the (code, weight) pairs, not positions.
        assert {(row["code"], row["weight"]) for row in rows} == {("alpha", 1.5), (None, 2.5), ("mu", None)}

    def test_reads_its_own_table_when_groups_share_a_column(self, shared_group_columns_db: Database) -> None:
        db = shared_group_columns_db
        db.create_element("Configuration", label="Config")
        parent_a = db.create_element("Parent", label="Parent A")
        parent_b = db.create_element("Parent", label="Parent B")
        child = db.create_element("Child", label="Child 1")
        # mentors and sponsors share parent_ref, and a per-column read of that name resolves to mentors.
        db.update_set_group("Child", "mentors", child, {"parent_ref": [parent_a]})
        db.update_set_group("Child", "sponsors", child, {"parent_ref": [parent_b, parent_b], "tier": [1, 2]})

        rows = db.read_set_group_by_id("Child", "sponsors", child)
        assert len(rows) == 2
        assert {(row["parent_ref"], row["tier"]) for row in rows} == {(parent_b, 1), (parent_b, 2)}


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


class TestReadSetIntegersById:
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


class TestReadSetFloatsById:
    def test_read_set_floats_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element("AllTypes", id1, weight=[9.9, 8.8])
        result = all_types_db.read_set_floats_by_id("AllTypes", "weight", id1)
        assert len(result) == 2
        assert any(abs(v - 9.9) < 1e-9 for v in result)
        assert any(abs(v - 8.8) < 1e-9 for v in result)


# -- DateTime set convenience (gap-fill) ------------------------------------


class TestReadSetDateTimeById:
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


class TestReadSetsByIdWithData:
    def test_read_sets_by_id_returns_all_groups(self, composite_helpers_db: Database) -> None:
        """read_sets_by_id returns dict with integer, float, and string set groups."""
        id1 = composite_helpers_db.create_element(
            "Items", label="item1", code=[10, 20, 30], weight=[1.1, 2.2], tag=["alpha", "beta"]
        )
        result = composite_helpers_db.read_sets_by_id("Items", id1)

        assert len(result) == 3
        assert sorted(result["code"]) == [10, 20, 30]
        assert len(result["weight"]) == 2
        assert any(abs(v - 1.1) < 1e-9 for v in result["weight"])
        assert any(abs(v - 2.2) < 1e-9 for v in result["weight"])
        assert sorted(result["tag"]) == ["alpha", "beta"]

    def test_read_sets_by_id_correct_types(self, composite_helpers_db: Database) -> None:
        """Each set group returns the correct Python type."""
        id1 = composite_helpers_db.create_element("Items", label="item1", code=[5], weight=[9.9], tag=["text"])
        result = composite_helpers_db.read_sets_by_id("Items", id1)

        assert all(isinstance(v, int) for v in result["code"])
        assert all(isinstance(v, float) for v in result["weight"])
        assert all(isinstance(v, str) for v in result["tag"])


# -- read_element_by_id -------------------------------------------------------


class TestReadElementById:
    def test_read_element_by_id_merges_all(self, composite_helpers_db: Database) -> None:
        """read_element_by_id returns scalars, vectors, and sets merged."""
        id1 = composite_helpers_db.create_element(
            "Items",
            label="item1",
            amount=[10, 20, 30],
            score=[1.1, 2.2],
            note=["hello", "world"],
            code=[5, 6],
            weight=[9.9],
            tag=["alpha", "beta"],
        )
        result = composite_helpers_db.read_element_by_id("Items", id1)

        # Scalars
        assert result["label"] == "item1"

        # Vectors
        assert result["amount"] == [10, 20, 30]
        assert result["note"] == ["hello", "world"]

        # Sets (unordered)
        assert sorted(result["code"]) == [5, 6]
        assert sorted(result["tag"]) == ["alpha", "beta"]

    def test_read_element_by_id_correct_types(self, composite_helpers_db: Database) -> None:
        """Merged dict preserves correct Python types for each attribute."""
        id1 = composite_helpers_db.create_element(
            "Items", label="item1", amount=[1], score=[2.0], note=["x"], code=[3], weight=[4.0], tag=["y"]
        )
        result = composite_helpers_db.read_element_by_id("Items", id1)

        assert isinstance(result["label"], str)
        assert all(isinstance(v, int) for v in result["amount"])
        assert all(isinstance(v, float) for v in result["score"])
        assert all(isinstance(v, str) for v in result["note"])
        assert all(isinstance(v, int) for v in result["code"])
        assert all(isinstance(v, float) for v in result["weight"])
        assert all(isinstance(v, str) for v in result["tag"])


# -- Set group column pairing -------------------------------------------------


class TestReadSetGroupColumnsPairByRow:
    def test_two_per_column_reads_pair_by_row(self, multi_column_groups_db: Database) -> None:
        multi_column_groups_db.create_element("Configuration", label="Config")
        element_id = multi_column_groups_db.create_element("Items", label="item1")
        # Unsorted in both columns on purpose: a value-ordered reader would pair the wrong rows
        multi_column_groups_db.update_set_group(
            "Items",
            "codes",
            element_id,
            {"code": ["zeta", "alpha", "mu"], "weight": [2.5, 3.5, 1.5]},
        )

        codes = multi_column_groups_db.read_set_strings_by_id("Items", "code", element_id)
        weights = multi_column_groups_db.read_set_floats_by_id("Items", "weight", element_id)

        assert len(codes) == 3
        assert len(weights) == len(codes)
        assert sorted(zip(codes, weights)) == [("alpha", 3.5), ("mu", 1.5), ("zeta", 2.5)]


class TestSetNullCells:
    """NULL cells round-trip positionally."""

    def test_bulk_and_by_id_keep_null_cells(self, collections_db: Database) -> None:
        """A NULL cell keeps its slot, and an element with no rows is an empty list."""
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.create_element("Collection", label="Item 2")  # no set rows
        collections_db.update_set_group("Collection", "tags", id1, {"tag": ["a", None, "c"]})

        # Set order is unspecified: pin the agreement between the readers and the content, not the order.
        by_id = collections_db.read_set_strings_by_id("Collection", "tag", id1)
        assert collections_db.read_set_strings("Collection", "tag") == [by_id, []]
        assert Counter(by_id) == Counter(["a", None, "c"])

    def test_numeric_and_boolean_readers_keep_null_cells(self, relations_db: Database) -> None:
        relations_db.create_element("Configuration", label="Config")
        child = relations_db.create_element("Child", label="Child 1")
        relations_db.update_set_group("Child", "scores", child, {"score": [1, None, 0]})

        assert Counter(relations_db.read_set_integers("Child", "score")[0]) == Counter([1, None, 0])
        assert Counter(relations_db.read_set_integers_by_id("Child", "score", child)) == Counter([1, None, 0])
        assert Counter(relations_db.read_set_booleans("Child", "score")[0]) == Counter([True, None, False])
        assert Counter(relations_db.read_set_booleans_by_id("Child", "score", child)) == Counter([True, None, False])

    def test_float_reader_keeps_null_cells(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        db.update_set_group("Items", "codes", item, {"code": ["a", "b"], "weight": [1.5, None]})

        assert Counter(db.read_set_floats("Items", "weight")[0]) == Counter([1.5, None])
        assert Counter(db.read_set_floats_by_id("Items", "weight", item)) == Counter([1.5, None])

    def test_date_time_reader_keeps_null_cells(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.update_set_group("Collection", "tags", id1, {"tag": ["2024-01-01", None]})

        expected = Counter([datetime(2024, 1, 1, tzinfo=timezone.utc), None])
        assert Counter(collections_db.read_set_date_times("Collection", "tag")[0]) == expected
        assert Counter(collections_db.read_set_date_time_by_id("Collection", "tag", id1)) == expected
