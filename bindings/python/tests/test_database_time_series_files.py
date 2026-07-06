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


# -- Time series files tests ---------------------------------------------------


class TestHasTimeSeriesFiles:
    def test_has_time_series_files_true(self, collections_db: Database) -> None:
        """Collection with a files table returns True."""
        assert collections_db.has_time_series_files("Collection") is True

    def test_has_time_series_files_false(self, mixed_time_series_db: Database) -> None:
        """Sensor schema has no files table, returns False."""
        assert mixed_time_series_db.has_time_series_files("Sensor") is False


class TestListTimeSeriesFilesColumns:
    def test_list_time_series_files_columns(self, collections_db: Database) -> None:
        """Returns column names from the files table."""
        columns = collections_db.list_time_series_files_columns("Collection")
        assert sorted(columns) == ["data_file", "metadata_file"]


class TestReadTimeSeriesFiles:
    def test_read_time_series_files_initial(self, collections_db: Database) -> None:
        """Before any update, all columns map to None."""
        result = collections_db.read_time_series_files("Collection")
        assert isinstance(result, dict)
        for col in ["data_file", "metadata_file"]:
            assert col in result
            assert result[col] is None


class TestUpdateTimeSeriesFiles:
    def test_update_and_read_time_series_files(self, collections_db: Database) -> None:
        """Update with paths, read back, verify exact match."""
        data = {"data_file": "/path/to/data.csv", "metadata_file": "/path/to/meta.json"}
        collections_db.update_time_series_files("Collection", data)

        result = collections_db.read_time_series_files("Collection")
        assert result == data

    def test_update_time_series_files_partial_none(self, collections_db: Database) -> None:
        """Update with one path set and one None, verify None preserved."""
        data = {"data_file": "/path/to/data.csv", "metadata_file": None}
        collections_db.update_time_series_files("Collection", data)

        result = collections_db.read_time_series_files("Collection")
        assert result["data_file"] == "/path/to/data.csv"
        assert result["metadata_file"] is None

    def test_update_time_series_files_overwrite(self, collections_db: Database) -> None:
        """Update with values, then update again, verify second values persisted."""
        data1 = {"data_file": "/first/data.csv", "metadata_file": "/first/meta.json"}
        collections_db.update_time_series_files("Collection", data1)

        data2 = {"data_file": "/second/data.csv", "metadata_file": "/second/meta.json"}
        collections_db.update_time_series_files("Collection", data2)

        result = collections_db.read_time_series_files("Collection")
        assert result == data2
