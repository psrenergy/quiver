"""Tests for time series group read/update operations."""

from __future__ import annotations

import pytest

from quiverdb import Database, Element


# -- Helpers ------------------------------------------------------------------


def _create_sensor(db: Database, label: str) -> int:
    """Create a Sensor element and return its ID."""
    elem = Element()
    elem.set("label", label)
    return db.create_element("Sensor", elem)


def _create_collection_element(db: Database, label: str) -> int:
    """Create a Collection element and return its ID."""
    elem = Element()
    elem.set("label", label)
    return db.create_element("Collection", elem)


SAMPLE_ROWS = [
    {"date_time": "2024-01-01T00:00:00", "temperature": 20.5, "humidity": 65, "status": "normal"},
    {"date_time": "2024-01-02T00:00:00", "temperature": 21.3, "humidity": 70, "status": "normal"},
    {"date_time": "2024-01-03T00:00:00", "temperature": 19.8, "humidity": 55, "status": "low"},
]


# -- Read tests (mixed_time_series_db: Sensor schema) -------------------------


class TestReadTimeSeriesGroup:
    def test_read_time_series_group_empty(self, mixed_time_series_db: Database) -> None:
        """Read from element with no rows returns empty list."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == []

    def test_read_time_series_group_single_row(self, mixed_time_series_db: Database) -> None:
        """Write one row, read back, verify all column types correct."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        row = {"date_time": "2024-01-01T00:00:00", "temperature": 20.5, "humidity": 65, "status": "normal"}
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, [row])

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert len(result) == 1
        assert result[0] == row

    def test_read_time_series_group_multiple_rows(self, mixed_time_series_db: Database) -> None:
        """Write multiple rows, read back, verify ordering and all values."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_ROWS)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert len(result) == 3
        assert result == SAMPLE_ROWS

    def test_read_time_series_group_column_order(self, mixed_time_series_db: Database) -> None:
        """Verify dimension column (date_time) comes first in dict key order."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_ROWS[:1])

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        keys = list(result[0].keys())
        assert keys[0] == "date_time"

    def test_read_time_series_group_types(self, mixed_time_series_db: Database) -> None:
        """Verify int for INTEGER, float for FLOAT, str for STRING/DATE_TIME columns."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_ROWS[:1])

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        row = result[0]
        assert type(row["date_time"]) is str  # DATE_TIME -> str
        assert type(row["temperature"]) is float  # FLOAT -> float
        assert type(row["humidity"]) is int  # INTEGER -> int
        assert type(row["status"]) is str  # STRING -> str


# -- Write tests (mixed_time_series_db: Sensor schema) ------------------------


class TestUpdateTimeSeriesGroup:
    def test_update_time_series_group_round_trip(self, mixed_time_series_db: Database) -> None:
        """Write rows, read back, assert exact match."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_ROWS)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == SAMPLE_ROWS

    def test_update_time_series_group_clear(self, mixed_time_series_db: Database) -> None:
        """Write rows, then update with empty list, read back returns empty."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_ROWS)
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, [])

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result == []

    def test_update_time_series_group_overwrite(self, mixed_time_series_db: Database) -> None:
        """Write rows, write different rows, read back returns only new rows."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, SAMPLE_ROWS)

        new_rows = [
            {"date_time": "2024-06-01T00:00:00", "temperature": 30.0, "humidity": 80, "status": "hot"},
        ]
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, new_rows)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert len(result) == 1
        assert result == new_rows


# -- Validation tests ----------------------------------------------------------


class TestTimeSeriesValidation:
    def test_update_time_series_group_missing_column(self, mixed_time_series_db: Database) -> None:
        """Omit a column from row dict, expect ValueError."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_row = {"date_time": "2024-01-01T00:00:00", "temperature": 20.5, "humidity": 65}
        # Missing "status" column
        with pytest.raises(ValueError, match="missing column 'status'"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, [bad_row])

    def test_update_time_series_group_wrong_type_int_for_float(self, mixed_time_series_db: Database) -> None:
        """Pass int where float expected, expect TypeError."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_row = {"date_time": "2024-01-01T00:00:00", "temperature": 20, "humidity": 65, "status": "normal"}
        with pytest.raises(TypeError, match="Column 'temperature' expects float, got int"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, [bad_row])

    def test_update_time_series_group_wrong_type_str_for_int(self, mixed_time_series_db: Database) -> None:
        """Pass str where int expected, expect TypeError."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_row = {"date_time": "2024-01-01T00:00:00", "temperature": 20.5, "humidity": "65", "status": "normal"}
        with pytest.raises(TypeError, match="Column 'humidity' expects int, got str"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, [bad_row])

    def test_update_time_series_group_wrong_type_bool_for_int(self, mixed_time_series_db: Database) -> None:
        """Pass bool where int expected, expect TypeError (strict type check)."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_row = {"date_time": "2024-01-01T00:00:00", "temperature": 20.5, "humidity": True, "status": "normal"}
        with pytest.raises(TypeError, match="Column 'humidity' expects int, got bool"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, [bad_row])


# -- Single-column tests (collections_db) -------------------------------------


class TestTimeSeriesSingleColumn:
    def test_read_time_series_group_single_column(self, collections_db: Database) -> None:
        """Write and read a simple date_time+value time series."""
        eid = _create_collection_element(collections_db, "Item1")
        rows = [
            {"date_time": "2024-01-01T00:00:00", "value": 3.14},
            {"date_time": "2024-01-02T00:00:00", "value": 2.72},
        ]
        collections_db.update_time_series_group("Collection", "data", eid, rows)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert len(result) == 2
        assert result == rows


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
