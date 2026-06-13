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


# -- NULL cell tests (nullable_time_series_db: Sensor schema, nullable value columns) ---


class TestTimeSeriesGroupNulls:
    def test_null_cells_round_trip(self, nullable_time_series_db: Database) -> None:
        """None entries become SQL NULL and round-trip back as None per cell."""
        eid = _create_sensor(nullable_time_series_db, "S1")
        nullable_time_series_db.update_time_series_group(
            "Sensor",
            "readings",
            eid,
            {
                "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
                "temperature": [10.5, None],
                "counter": [None, 7],
                "status": ["ok", None],
            },
        )
        result = nullable_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["date_time"] == [_utc(2024, 1, 1), _utc(2024, 1, 2)]
        assert result["temperature"] == [10.5, None]
        assert result["counter"] == [None, 7]
        assert result["status"] == ["ok", None]

    def test_all_null_column(self, nullable_time_series_db: Database) -> None:
        """An all-None value column round-trips as all None."""
        eid = _create_sensor(nullable_time_series_db, "S1")
        nullable_time_series_db.update_time_series_group(
            "Sensor",
            "readings",
            eid,
            {
                "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
                "status": [None, None],
            },
        )
        result = nullable_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["status"] == [None, None]

    def test_null_read_modify_write(self, nullable_time_series_db: Database) -> None:
        """A NULL cell survives a read-modify-write round-trip."""
        eid = _create_sensor(nullable_time_series_db, "S1")
        nullable_time_series_db.update_time_series_group(
            "Sensor",
            "readings",
            eid,
            {
                "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
                "temperature": [10.5, None],
            },
        )
        ts = nullable_time_series_db.read_time_series_group("Sensor", "readings", eid)
        ts["temperature"][0] = 99.0
        nullable_time_series_db.update_time_series_group("Sensor", "readings", eid, ts)
        result = nullable_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["temperature"] == [99.0, None]
