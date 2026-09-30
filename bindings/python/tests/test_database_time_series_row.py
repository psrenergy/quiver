"""Tests for time series group read/update operations."""

from __future__ import annotations

from datetime import datetime, timedelta, timezone

import pytest

from quiverdb import Database, QuiverError

# -- Helpers ------------------------------------------------------------------


def _create_sensor(db: Database, label: str) -> int:
    """Create a Sensor element and return its ID."""
    return db.create_element("Sensor", label=label)


def _create_collection_element(db: Database, label: str) -> int:
    """Create a Collection element and return its ID."""
    return db.create_element("Collection", label=label)


def _utc(year: int, month: int, day: int) -> datetime:
    return datetime(year, month, day, tzinfo=timezone.utc)


SAMPLE_DATA = {
    "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00", "2024-01-03T00:00:00"],
    "temperature": [20.5, 21.3, 19.8],
    "humidity": [65, 70, 55],
    "status": ["normal", "normal", "low"],
}

SAMPLE_READBACK = {
    "date_time": [_utc(2024, 1, 1), _utc(2024, 1, 2), _utc(2024, 1, 3)],
    "temperature": [20.5, 21.3, 19.8],
    "humidity": [65, 70, 55],
    "status": ["normal", "normal", "low"],
}


# -- upsert_time_series_row tests -------------------------------------------------


class TestUpsertTimeSeriesRow:
    def test_upsert_time_series_row_insert(self, collections_db: Database) -> None:
        """Insert one row via kwargs and read back to assert presence."""
        eid = _create_collection_element(collections_db, "Item1")
        collections_db.upsert_time_series_row("Collection", "data", eid, date_time="2024-01-01", value=10.0)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["date_time"] == [_utc(2024, 1, 1)]
        assert result["value"] == [10.0]

    def test_upsert_time_series_row_upsert(self, collections_db: Database) -> None:
        """Insert then call again with same dimension PK; value column is overwritten."""
        eid = _create_collection_element(collections_db, "Item1")
        collections_db.upsert_time_series_row("Collection", "data", eid, date_time="2024-01-01", value=10.0)
        collections_db.upsert_time_series_row("Collection", "data", eid, date_time="2024-01-01", value=99.0)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["value"] == [99.0]

    def test_upsert_time_series_row_dict_unpacking(self, collections_db: Database) -> None:
        """Dict unpacking pattern works: db.upsert_time_series_row(..., **row_dict)."""
        eid = _create_collection_element(collections_db, "Item1")
        row_dict = {"date_time": "2024-02-01", "value": 7.5}
        collections_db.upsert_time_series_row("Collection", "data", eid, **row_dict)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["date_time"] == [_utc(2024, 2, 1)]
        assert result["value"] == [7.5]

    def test_upsert_time_series_row_accepts_datetime(self, collections_db: Database) -> None:
        """A datetime dimension value is accepted by both upserts; an aware one is stored as UTC."""
        eid = _create_collection_element(collections_db, "Item1")
        aware = datetime(2024, 1, 1, 10, tzinfo=timezone(timedelta(hours=3)))
        collections_db.upsert_time_series_row("Collection", "data", eid, date_time=aware, value=10.0)
        collections_db.upsert_time_series_row_by_label(
            "Collection", "data", "Item1", date_time=datetime(2024, 1, 2), value=20.0
        )

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["date_time"] == [datetime(2024, 1, 1, 7, tzinfo=timezone.utc), _utc(2024, 1, 2)]
        assert result["value"] == [10.0, 20.0]

    def test_upsert_time_series_row_multi_dim(self, multi_dim_ts_db: Database) -> None:
        """Multi-dimension PK (date_time + block) round-trips through the Python wrapper."""
        eid = multi_dim_ts_db.create_element("Resource", label="R1")
        multi_dim_ts_db.upsert_time_series_row(
            "Resource", "load", eid, date_time="2024-01-01", block=1, load=500.0, flag=0
        )

        result = multi_dim_ts_db.read_time_series_group("Resource", "load", eid)
        assert result["date_time"] == [_utc(2024, 1, 1)]
        assert result["block"] == [1]
        assert result["load"] == [500.0]
        assert result["flag"] == [0]

    def test_upsert_time_series_row_int_for_real_and_omitted_columns(self, multi_dim_ts_db: Database) -> None:
        """An int is accepted for a REAL column; a value column left out of kwargs is stored as NULL."""
        eid = multi_dim_ts_db.create_element("Resource", label="R1")
        multi_dim_ts_db.upsert_time_series_row("Resource", "load", eid, date_time="2024-01-01", block=1, load=42)
        multi_dim_ts_db.upsert_time_series_row("Resource", "load", eid, date_time="2024-01-02", block=1, flag=5)

        result = multi_dim_ts_db.read_time_series_group("Resource", "load", eid)
        assert result["date_time"] == [_utc(2024, 1, 1), _utc(2024, 1, 2)]
        assert result["load"] == [42.0, None]
        assert result["flag"] == [None, 5]

    def test_upsert_time_series_row_by_label(self, collections_db: Database) -> None:
        """Label-addressed upsert writes only the labelled element; same PK overwrites."""
        item = _create_collection_element(collections_db, "Item1")
        other = _create_collection_element(collections_db, "Item2")

        collections_db.upsert_time_series_row_by_label(
            "Collection", "data", "Item2", date_time="2024-01-01", value=99.0
        )
        collections_db.upsert_time_series_row_by_label(
            "Collection", "data", "Item1", date_time="2024-01-01", value=10.0
        )
        collections_db.upsert_time_series_row_by_label(
            "Collection", "data", "Item1", date_time="2024-01-01", value=20.0
        )

        result = collections_db.read_time_series_group("Collection", "data", item)
        assert result["date_time"] == [_utc(2024, 1, 1)]
        assert result["value"] == [20.0]
        assert collections_db.read_time_series_group("Collection", "data", other)["value"] == [99.0]

    def test_upsert_time_series_row_by_label_not_found(self, collections_db: Database) -> None:
        """An unresolvable label raises and writes nothing."""
        eid = _create_collection_element(collections_db, "Item1")

        with pytest.raises(QuiverError, match="Element not found: label 'Nope' in collection 'Collection'"):
            collections_db.upsert_time_series_row_by_label(
                "Collection", "data", "Nope", date_time="2024-01-01", value=10.0
            )

        assert collections_db.read_time_series_group("Collection", "data", eid) == {}

    def test_upsert_passes_an_id_attribute_to_the_core(self, collections_db: Database) -> None:
        """`id` is positional-only: an `id=` kwarg reaches the core's column check instead of colliding."""
        eid = _create_collection_element(collections_db, "Item1")
        with pytest.raises(QuiverError, match="column 'id' not found in group 'data'"):
            collections_db.upsert_time_series_row("Collection", "data", eid, id=eid, date_time="2024-01-01", value=1.0)
        assert collections_db.read_time_series_group("Collection", "data", eid) == {}

    def test_upsert_by_label_passes_a_label_attribute_to_the_core(self, collections_db: Database) -> None:
        """`label` is positional-only: a `label=` kwarg reaches the core's column check instead of colliding."""
        eid = _create_collection_element(collections_db, "Item1")
        with pytest.raises(QuiverError, match="column 'label' not found in group 'data'"):
            collections_db.upsert_time_series_row_by_label(
                "Collection", "data", "Item1", label="Item1", date_time="2024-01-01", value=1.0
            )
        assert collections_db.read_time_series_group("Collection", "data", eid) == {}


class TestReadTimeSeriesRow:
    def test_read_time_series_row_returns_one_value_per_element(self, collections_db: Database) -> None:
        """Last non-null value at or before the given date, one entry per element."""
        id1 = _create_collection_element(collections_db, "Item 1")
        id2 = _create_collection_element(collections_db, "Item 2")

        collections_db.update_time_series_group(
            "Collection",
            "data",
            id1,
            {"date_time": ["2024-01-01T00:00:00", "2024-02-01T00:00:00"], "value": [10.5, 20.5]},
        )
        collections_db.update_time_series_group(
            "Collection",
            "data",
            id2,
            {"date_time": ["2024-01-01T00:00:00"], "value": [30.5]},
        )

        row = collections_db.read_time_series_row("Collection", "data", "value", datetime(2024, 1, 15))
        assert row == [10.5, 30.5]

    def test_read_time_series_row_converts_aware_datetime_to_utc(self, collections_db: Database) -> None:
        """An aware date_time is looked up at its UTC instant."""
        eid = _create_collection_element(collections_db, "Item 1")
        collections_db.update_time_series_group(
            "Collection",
            "data",
            eid,
            {"date_time": ["2024-01-01T07:00:00", "2024-01-01T09:00:00"], "value": [10.5, 20.5]},
        )

        # 11:00+03:00 is 08:00 UTC, so the 07:00 row is the last at or before it. Formatting the
        # wall clock would look up 11:00 and return the 09:00 row.
        at = datetime(2024, 1, 1, 11, tzinfo=timezone(timedelta(hours=3)))
        assert collections_db.read_time_series_row("Collection", "data", "value", at) == [10.5]

    def test_read_time_series_row_no_elements(self, collections_db: Database) -> None:
        row = collections_db.read_time_series_row("Collection", "data", "value", datetime(2024, 1, 15))
        assert row == []

    def test_read_time_series_row_unknown_attribute_raises(self, collections_db: Database) -> None:
        _create_collection_element(collections_db, "Item 1")
        with pytest.raises(QuiverError, match="Time series attribute not found"):
            collections_db.read_time_series_row("Collection", "data", "nonexistent", datetime(2024, 1, 15))

    def test_read_time_series_row_multi_dimension_group_raises(self, multi_dim_ts_db: Database) -> None:
        """A second dimension (block) leaves no single value per element at a date."""
        with pytest.raises(
            QuiverError,
            match="Cannot read_time_series_row: group 'load' of collection 'Resource' has more than one dimension column",
        ):
            multi_dim_ts_db.read_time_series_row("Resource", "load", "load", datetime(2024, 1, 1))

    def test_read_time_series_row_no_data_is_none(self, mixed_time_series_db: Database) -> None:
        """An element with no row at or before the date reads None in every column type, never 0 or nan."""
        id1 = _create_sensor(mixed_time_series_db, "Sensor 1")
        _create_sensor(mixed_time_series_db, "Sensor 2")  # no rows
        mixed_time_series_db.update_time_series_group(
            "Sensor",
            "readings",
            id1,
            {"date_time": ["2024-01-02T00:00:00"], "temperature": [20.5], "humidity": [0], "status": ["ok"]},
        )

        # A stored 0 and "no data" are distinguishable.
        at = datetime(2024, 1, 2)
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "humidity", at) == [0, None]
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "temperature", at) == [20.5, None]
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "status", at) == ["ok", None]

        # Before the first row even Sensor 1 has no data.
        before = datetime(2024, 1, 1)
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "humidity", before) == [None, None]
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "temperature", before) == [None, None]
