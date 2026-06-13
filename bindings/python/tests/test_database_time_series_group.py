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


# -- Read tests (mixed_time_series_db: Sensor schema) -------------------------


class TestReadTimeSeriesGroup:
    def test_read_time_series_group_empty(self, mixed_time_series_db: Database) -> None:
        """Read from element with no rows returns empty dict."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == {}

    def test_read_time_series_group_single_row(self, mixed_time_series_db: Database) -> None:
        """Write one row, read back, verify all column values."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        data = {
            "date_time": ["2024-01-01T00:00:00"],
            "temperature": [20.5],
            "humidity": [65],
            "status": ["normal"],
        }
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, data)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == {
            "date_time": [_utc(2024, 1, 1)],
            "temperature": [20.5],
            "humidity": [65],
            "status": ["normal"],
        }

    def test_read_time_series_group_multiple_rows(self, mixed_time_series_db: Database) -> None:
        """Write multiple rows, read back, verify ordering and all values."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_DATA)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == SAMPLE_READBACK

    def test_read_time_series_group_column_order(self, mixed_time_series_db: Database) -> None:
        """Verify dimension column (date_time) comes first in dict key order."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_DATA)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert next(iter(result)) == "date_time"

    def test_read_time_series_group_types(self, mixed_time_series_db: Database) -> None:
        """Verify datetime for the dimension column and schema types for values."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_DATA)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert type(result["date_time"][0]) is datetime  # dimension -> datetime
        assert type(result["temperature"][0]) is float  # FLOAT -> float
        assert type(result["humidity"][0]) is int  # INTEGER -> int
        assert type(result["status"][0]) is str  # STRING -> str

    def test_update_time_series_group_accepts_datetime_values(self, mixed_time_series_db: Database) -> None:
        """datetime objects in the dimension column are formatted automatically."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        data = {
            "date_time": [datetime(2024, 1, 1)],
            "temperature": [20.5],
            "humidity": [65],
            "status": ["normal"],
        }
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, data)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["date_time"] == [_utc(2024, 1, 1)]


# -- Write tests (mixed_time_series_db: Sensor schema) ------------------------


class TestUpdateTimeSeriesGroup:
    def test_update_time_series_group_round_trip(self, mixed_time_series_db: Database) -> None:
        """Write columns, read back, assert exact match."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_DATA)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == SAMPLE_READBACK

    def test_update_time_series_group_clear(self, mixed_time_series_db: Database) -> None:
        """Write rows, then update with empty dict, read back returns empty."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_DATA)
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, {})

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == {}

    def test_update_time_series_group_overwrite(self, mixed_time_series_db: Database) -> None:
        """Write rows, write different rows, read back returns only new rows."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_DATA)

        new_data = {
            "date_time": ["2024-06-01T00:00:00"],
            "temperature": [30.0],
            "humidity": [80],
            "status": ["hot"],
        }
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, new_data)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["temperature"] == [30.0]
        assert result["status"] == ["hot"]


# -- Validation tests ----------------------------------------------------------


class TestTimeSeriesValidation:
    def test_update_time_series_group_mismatched_lengths(self, mixed_time_series_db: Database) -> None:
        """Column lists of differing lengths raise ValueError before the FFI call."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_data = {
            "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
            "temperature": [20.5],
            "humidity": [65, 70],
            "status": ["normal", "normal"],
        }
        with pytest.raises(ValueError, match="same length"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, bad_data)

    def test_update_time_series_group_int_for_float_column(self, mixed_time_series_db: Database) -> None:
        """Integers are accepted for REAL columns (converted on insert)."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        data = {
            "date_time": ["2024-01-01T00:00:00"],
            "temperature": [20],
            "humidity": [65],
            "status": ["normal"],
        }
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, data)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["temperature"] == [20.0]

    def test_update_time_series_group_wrong_type_str_for_int(self, mixed_time_series_db: Database) -> None:
        """Strings for an INTEGER column are rejected by the C++ layer."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_data = {
            "date_time": ["2024-01-01T00:00:00"],
            "temperature": [20.5],
            "humidity": ["65"],
            "status": ["normal"],
        }
        with pytest.raises(QuiverError, match="column 'humidity' has type INTEGER but received TEXT"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, bad_data)

    def test_update_time_series_group_unknown_column(self, mixed_time_series_db: Database) -> None:
        """Columns that do not exist in the schema are rejected by the C++ layer."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_data = {
            "date_time": ["2024-01-01T00:00:00"],
            "temperature": [20.5],
            "humidity": [65],
            "status": ["normal"],
            "nonexistent": [1],
        }
        with pytest.raises(QuiverError, match="column 'nonexistent' not found"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, bad_data)


# -- Single-column tests (collections_db) -------------------------------------


class TestTimeSeriesSingleColumn:
    def test_read_time_series_group_single_column(self, collections_db: Database) -> None:
        """Write and read a simple date_time+value time series."""
        eid = _create_collection_element(collections_db, "Item1")
        data = {
            "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
            "value": [3.14, 2.72],
        }
        collections_db.update_time_series_group("Collection", "data", eid, data)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result == {
            "date_time": [_utc(2024, 1, 1), _utc(2024, 1, 2)],
            "value": [3.14, 2.72],
        }
