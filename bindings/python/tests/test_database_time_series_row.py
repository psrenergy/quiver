"""Tests for time series group read/update operations."""

from __future__ import annotations

from datetime import datetime, timezone

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

    def test_read_time_series_row_no_elements(self, collections_db: Database) -> None:
        row = collections_db.read_time_series_row("Collection", "data", "value", datetime(2024, 1, 15))
        assert row == []

    def test_read_time_series_row_unknown_attribute_raises(self, collections_db: Database) -> None:
        _create_collection_element(collections_db, "Item 1")
        with pytest.raises(QuiverError, match="Time series attribute not found"):
            collections_db.read_time_series_row("Collection", "data", "nonexistent", datetime(2024, 1, 15))
