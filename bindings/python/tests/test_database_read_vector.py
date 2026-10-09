"""Tests for vector read operations (bulk and by-id)."""

from __future__ import annotations

from datetime import datetime, timezone

import pytest

from quiverdb import Database

# -- Vector reads by ID -------------------------------------------------------


class TestReadVectorIntegersById:
    def test_read_vector_integers_by_id(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, value_int=[1, 2, 3])
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert result == [1, 2, 3]

    def test_read_vector_integers_by_id_empty(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10)
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert result == []

    def test_read_vector_integers_by_id_single(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, value_int=[42])
        result = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        assert result == [42]


class TestReadVectorFloatsById:
    def test_read_vector_floats_by_id(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10, value_float=[1.5, 2.5, 3.5])
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", id1)
        assert len(result) == 3
        assert abs(result[0] - 1.5) < 1e-9
        assert abs(result[1] - 2.5) < 1e-9
        assert abs(result[2] - 3.5) < 1e-9

    def test_read_vector_floats_by_id_empty(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10)
        result = collections_db.read_vector_floats_by_id("Collection", "value_float", id1)
        assert result == []


# -- Bulk vector reads ---------------------------------------------------------


class TestReadVectorIntegersBulk:
    def test_read_vector_integers(self, collections_db: Database) -> None:
        collections_db.create_element("Collection", label="item1", some_integer=10, value_int=[1, 2])
        collections_db.create_element("Collection", label="item2", some_integer=20, value_int=[3, 4, 5])
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert len(result) == 2
        assert result[0] == [1, 2]
        assert result[1] == [3, 4, 5]

    def test_read_vector_integers_empty_collection(self, collections_db: Database) -> None:
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert result == []

    def test_read_vector_integers_with_empty_vector(self, collections_db: Database) -> None:
        # One entry per element: an element with no vector data reads back as an empty list
        collections_db.create_element("Collection", label="item1", some_integer=10, value_int=[1, 2])
        collections_db.create_element("Collection", label="item2", some_integer=20)
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert len(result) == 2
        assert result[0] == [1, 2]
        assert result[1] == []


class TestReadVectorFloatsBulk:
    def test_read_vector_floats(self, collections_db: Database) -> None:
        collections_db.create_element("Collection", label="item1", some_integer=10, value_float=[1.1, 2.2])
        collections_db.create_element("Collection", label="item2", some_integer=20, value_float=[3.3])
        result = collections_db.read_vector_floats("Collection", "value_float")
        assert len(result) == 2
        assert len(result[0]) == 2
        assert abs(result[0][0] - 1.1) < 1e-9
        assert abs(result[0][1] - 2.2) < 1e-9
        assert len(result[1]) == 1
        assert abs(result[1][0] - 3.3) < 1e-9


# -- Convenience vector reads ------------------------------------------------


class TestReadVectorsById:
    def test_read_vectors_by_id_no_groups(self, db: Database) -> None:
        """read_vectors_by_id returns empty dict for collections with no vector groups."""
        id1 = db.create_element("Configuration", label="item1")
        result = db.read_vectors_by_id("Configuration", id1)
        assert result == {}


class TestReadVectorGroupById:
    def test_read_vector_group_by_id(self, collections_db: Database) -> None:
        id1 = collections_db.create_element(
            "Collection",
            label="item1",
            some_integer=10,
            value_int=[1, 2, 3],
            value_float=[1.1, 2.2, 3.3],
        )

        result = collections_db.read_vector_group_by_id("Collection", "values", id1)
        assert len(result) == 3

        # Check first row
        assert result[0]["vector_index"] == 0
        assert result[0]["value_int"] == 1
        assert abs(result[0]["value_float"] - 1.1) < 1e-9

        # Check last row
        assert result[2]["vector_index"] == 2
        assert result[2]["value_int"] == 3
        assert abs(result[2]["value_float"] - 3.3) < 1e-9

        # Every row has vector_index as int
        for row in result:
            assert "vector_index" in row
            assert isinstance(row["vector_index"], int)

    def test_read_vector_group_by_id_empty(self, collections_db: Database) -> None:
        id1 = collections_db.create_element("Collection", label="item1", some_integer=10)
        result = collections_db.read_vector_group_by_id("Collection", "values", id1)
        assert result == []

    def test_parses_date_time_columns(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        db.update_vector_group(
            "Items",
            "events",
            item,
            {"date_event": ["2024-01-15T10:30:00", None, "2024-03-01"], "note": [None, "second", "third"]},
        )

        assert db.read_vector_group_by_id("Items", "events", item) == [
            {"vector_index": 0, "date_event": datetime(2024, 1, 15, 10, 30, tzinfo=timezone.utc), "note": None},
            {"vector_index": 1, "date_event": None, "note": "second"},
            {"vector_index": 2, "date_event": datetime(2024, 3, 1, tzinfo=timezone.utc), "note": "third"},
        ]

    def test_reads_its_own_table_when_groups_share_a_column(self, shared_group_columns_db: Database) -> None:
        db = shared_group_columns_db
        db.create_element("Configuration", label="Config")
        parent_a = db.create_element("Parent", label="Parent A")
        parent_b = db.create_element("Parent", label="Parent B")
        child = db.create_element("Child", label="Child 1")
        # links and routes share parent_ref, and a per-column read of that name resolves to links.
        db.update_vector_group("Child", "links", child, {"parent_ref": [parent_a]})
        db.update_vector_group("Child", "routes", child, {"parent_ref": [parent_b, parent_b], "cost": [1.5, 2.5]})

        assert db.read_vector_group_by_id("Child", "routes", child) == [
            {"vector_index": 0, "parent_ref": parent_b, "cost": 1.5},
            {"vector_index": 1, "parent_ref": parent_b, "cost": 2.5},
        ]


# -- String vector reads (gap-fill) ------------------------------------------


class TestReadVectorStringsBulk:
    def test_read_vector_strings(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        id2 = all_types_db.create_element("AllTypes", label="item2")
        all_types_db.update_element("AllTypes", id1, label_value=["alpha", "beta"])
        all_types_db.update_element("AllTypes", id2, label_value=["gamma", "delta", "epsilon"])
        result = all_types_db.read_vector_strings("AllTypes", "label_value")
        assert len(result) == 2
        assert result[0] == ["alpha", "beta"]
        assert result[1] == ["gamma", "delta", "epsilon"]


class TestReadVectorDateTimesBulk:
    def test_read_vector_date_times(self, all_types_db: Database) -> None:
        assert all_types_db.read_vector_date_times("AllTypes", "label_value") == []

        all_types_db.create_element(
            "AllTypes",
            label="item1",
            label_value=["2024-01-15T10:30:00", "2024-01-16"],
        )
        all_types_db.create_element(
            "AllTypes",
            label="item2",
            label_value=["2024-06-20 14:45:30"],
        )
        all_types_db.create_element("AllTypes", label="no vector")

        assert all_types_db.read_vector_date_times("AllTypes", "label_value") == [
            [
                datetime(2024, 1, 15, 10, 30, tzinfo=timezone.utc),
                datetime(2024, 1, 16, tzinfo=timezone.utc),
            ],
            [datetime(2024, 6, 20, 14, 45, 30, tzinfo=timezone.utc)],
            [],
        ]

    def test_rejects_a_malformed_cell_naming_the_column(self, all_types_db: Database) -> None:
        all_types_db.create_element("AllTypes", label="item1", label_value=["2024-01-15", "2024-01"])

        with pytest.raises(ValueError, match=r"AllTypes\.label_value.*expected a valid YYYY-MM-DD"):
            all_types_db.read_vector_date_times("AllTypes", "label_value")

        with pytest.raises(ValueError, match=r"AllTypes\.label_value"):
            all_types_db.read_vector_date_times_by_id("AllTypes", "label_value", 1)


class TestReadVectorStringsById:
    def test_read_vector_strings_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element("AllTypes", id1, label_value=["hello", "world"])
        result = all_types_db.read_vector_strings_by_id("AllTypes", "label_value", id1)
        assert result == ["hello", "world"]


class TestReadVectorDateTimesById:
    def test_read_vector_date_times_by_id(self, all_types_db: Database) -> None:
        """read_vector_date_times_by_id wraps read_vector_strings_by_id + datetime parsing."""
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element(
            "AllTypes",
            id1,
            label_value=["2024-01-15T10:30:00", "2024-06-20T08:00:00"],
        )
        result = all_types_db.read_vector_date_times_by_id("AllTypes", "label_value", id1)
        assert len(result) == 2
        assert isinstance(result[0], datetime)
        assert result[0].year == 2024
        assert result[0].month == 1
        assert result[0].day == 15
        assert result[1].month == 6

    def test_singular_date_time_by_id_names_are_gone(self, all_types_db: Database) -> None:
        assert not hasattr(all_types_db, "read_vector_date_time_by_id")
        assert not hasattr(all_types_db, "read_set_date_time_by_id")


# -- Convenience vector reads with data (gap-fill) --------------------------


class TestReadVectorsByIdWithData:
    def test_read_vectors_by_id_returns_all_groups(self, composite_helpers_db: Database) -> None:
        """read_vectors_by_id returns dict with integer, float, and string vector groups."""
        id1 = composite_helpers_db.create_element(
            "Items", label="item1", amount=[10, 20, 30], score=[1.1, 2.2], note=["hello", "world"]
        )
        result = composite_helpers_db.read_vectors_by_id("Items", id1)

        assert len(result) == 3
        assert result["amount"] == [10, 20, 30]
        assert len(result["score"]) == 2
        assert abs(result["score"][0] - 1.1) < 1e-9
        assert abs(result["score"][1] - 2.2) < 1e-9
        assert result["note"] == ["hello", "world"]

    def test_read_vectors_by_id_correct_types(self, composite_helpers_db: Database) -> None:
        """Each vector group returns the correct Python type."""
        id1 = composite_helpers_db.create_element("Items", label="item1", amount=[5], score=[9.9], note=["text"])
        result = composite_helpers_db.read_vectors_by_id("Items", id1)

        assert all(isinstance(v, int) for v in result["amount"])
        assert all(isinstance(v, float) for v in result["score"])
        assert all(isinstance(v, str) for v in result["note"])


class TestVectorNullCells:
    """NULL cells round-trip positionally."""

    def test_bulk_and_by_id_keep_null_cells(self, collections_db: Database) -> None:
        """A NULL cell keeps its slot, and an element with no rows is an empty list."""
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.create_element("Collection", label="Item 2")  # no vector rows
        # create_element keeps a non-null array write surface, so the NULL cell goes in
        # through the group writer.
        collections_db.update_vector_group("Collection", "values", id1, {"value_int": [10, None, 30]})

        assert collections_db.read_vector_integers("Collection", "value_int") == [[10, None, 30], []]
        assert collections_db.read_vector_integers_by_id("Collection", "value_int", id1) == [10, None, 30]

    def test_null_cell_is_refused_on_element_write(self, collections_db: Database) -> None:
        """A read with a NULL cell written back through create_element names the column."""
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.update_vector_group("Collection", "values", id1, {"value_int": [10, None, 30]})

        values = collections_db.read_vector_integers_by_id("Collection", "value_int", id1)
        with pytest.raises(TypeError, match="Element.set\\('value_int'\\)"):
            collections_db.create_element("Collection", label="Item 2", value_int=values)

    def test_boolean_wrapper_keeps_null_cells(self, collections_db: Database) -> None:
        """The boolean wrapper maps a NULL cell to None rather than raising."""
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.update_vector_group("Collection", "values", id1, {"value_int": [1, None, 0]})

        assert collections_db.read_vector_booleans("Collection", "value_int") == [[True, None, False]]
        assert collections_db.read_vector_booleans_by_id("Collection", "value_int", id1) == [True, None, False]

    def test_group_reader_keeps_null_cells_in_place(self, collections_db: Database) -> None:
        """read_vector_group_by_id returns a NULL cell as None in its own row."""
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.update_vector_group(
            "Collection", "values", id1, {"value_int": [10, None, 30], "value_float": [1.5, 2.5, None]}
        )

        rows = collections_db.read_vector_group_by_id("Collection", "values", id1)
        assert len(rows) == 3
        assert [row["value_int"] for row in rows] == [10, None, 30]
        assert [row["value_float"] for row in rows] == [1.5, 2.5, None]

    def test_float_reader_keeps_null_cells(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="Config")
        id1 = collections_db.create_element("Collection", label="Item 1")
        collections_db.update_vector_group(
            "Collection", "values", id1, {"value_int": [1, 2], "value_float": [None, 2.5]}
        )

        assert collections_db.read_vector_floats("Collection", "value_float") == [[None, 2.5]]
        assert collections_db.read_vector_floats_by_id("Collection", "value_float", id1) == [None, 2.5]

    def test_string_and_date_time_readers_keep_null_cells(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        db.update_vector_group("Items", "events", item, {"date_event": ["2024-01-01", None], "note": [None, "b"]})

        assert db.read_vector_strings("Items", "note") == [[None, "b"]]
        assert db.read_vector_strings_by_id("Items", "note", item) == [None, "b"]
        jan_first = datetime(2024, 1, 1, tzinfo=timezone.utc)
        assert db.read_vector_date_times("Items", "date_event") == [[jan_first, None]]
        assert db.read_vector_date_times_by_id("Items", "date_event", item) == [jan_first, None]
        assert db.read_vectors_by_id("Items", item)["date_event"] == [jan_first, None]
