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


# -- add_time_series_row tests -------------------------------------------------


class TestAddTimeSeriesRow:
    def test_add_time_series_row_insert(self, collections_db: Database) -> None:
        """Insert one row via kwargs and read back to assert presence."""
        eid = _create_collection_element(collections_db, "Item1")
        collections_db.add_time_series_row("Collection", "data", eid, date_time="2024-01-01", value=10.0)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["date_time"] == [_utc(2024, 1, 1)]
        assert result["value"] == [10.0]

    def test_add_time_series_row_upsert(self, collections_db: Database) -> None:
        """Insert then call again with same dimension PK; value column is overwritten."""
        eid = _create_collection_element(collections_db, "Item1")
        collections_db.add_time_series_row("Collection", "data", eid, date_time="2024-01-01", value=10.0)
        collections_db.add_time_series_row("Collection", "data", eid, date_time="2024-01-01", value=99.0)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["value"] == [99.0]

    def test_add_time_series_row_dict_unpacking(self, collections_db: Database) -> None:
        """Dict unpacking pattern works: db.add_time_series_row(..., **row_dict)."""
        eid = _create_collection_element(collections_db, "Item1")
        row_dict = {"date_time": "2024-02-01", "value": 7.5}
        collections_db.add_time_series_row("Collection", "data", eid, **row_dict)

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["date_time"] == [_utc(2024, 2, 1)]
        assert result["value"] == [7.5]

    def test_add_time_series_row_multi_dim(self, multi_dim_ts_db: Database) -> None:
        """Multi-dimension PK (date_time + block) round-trips through the Python wrapper."""
        eid = multi_dim_ts_db.create_element("Resource", label="R1")
        multi_dim_ts_db.add_time_series_row(
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
