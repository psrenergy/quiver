"""Tests for vector read operations (bulk and by-id)."""

from __future__ import annotations

from datetime import datetime, timezone

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
        # C++ bulk read skips elements with no vector data (same as scalar NULL skipping)
        collections_db.create_element("Collection", label="item1", some_integer=10, value_int=[1, 2])
        collections_db.create_element("Collection", label="item2", some_integer=20)
        result = collections_db.read_vector_integers("Collection", "value_int")
        assert len(result) == 1
        assert result[0] == [1, 2]


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


class TestReadVectorStringsById:
    def test_read_vector_strings_by_id(self, all_types_db: Database) -> None:
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element("AllTypes", id1, label_value=["hello", "world"])
        result = all_types_db.read_vector_strings_by_id("AllTypes", "label_value", id1)
        assert result == ["hello", "world"]


class TestReadVectorDateTimeById:
    def test_read_vector_date_time_by_id(self, all_types_db: Database) -> None:
        """read_vector_date_time_by_id wraps read_vector_strings_by_id + datetime parsing."""
        id1 = all_types_db.create_element("AllTypes", label="item1")
        all_types_db.update_element(
            "AllTypes",
            id1,
            label_value=["2024-01-15T10:30:00", "2024-06-20T08:00:00"],
        )
        result = all_types_db.read_vector_date_time_by_id("AllTypes", "label_value", id1)
        assert len(result) == 2
        assert isinstance(result[0], datetime)
        assert result[0].year == 2024
        assert result[0].month == 1
        assert result[0].day == 15
        assert result[1].month == 6


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
