"""Tests for query operations: simple, parameterized, and date_time."""

from __future__ import annotations

from datetime import datetime

import pytest

from quiverdb import Database


# -- Simple queries (QUERY-01) ------------------------------------------------


class TestQueryString:
    def test_query_string_returns_value(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", string_attribute="hello")
        result = db.query_string("SELECT string_attribute FROM Configuration WHERE label = 'item1'")
        assert result == "hello"

    def test_query_string_no_rows_returns_none(self, db: Database) -> None:
        result = db.query_string("SELECT string_attribute FROM Configuration WHERE 1 = 0")
        assert result is None


class TestQueryInteger:
    def test_query_integer_returns_value(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", integer_attribute=42)
        result = db.query_integer("SELECT integer_attribute FROM Configuration WHERE label = 'item1'")
        assert result == 42
        assert isinstance(result, int)

    def test_query_integer_no_rows_returns_none(self, db: Database) -> None:
        result = db.query_integer("SELECT integer_attribute FROM Configuration WHERE 1 = 0")
        assert result is None

    def test_query_integer_count(self, db: Database) -> None:
        db.create_element("Configuration", label="a")
        db.create_element("Configuration", label="b")
        result = db.query_integer("SELECT COUNT(*) FROM Configuration")
        assert result == 2


class TestQueryFloat:
    def test_query_float_returns_value(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", float_attribute=3.14)
        result = db.query_float("SELECT float_attribute FROM Configuration WHERE label = 'item1'")
        assert result is not None
        assert isinstance(result, float)
        assert abs(result - 3.14) < 1e-9

    def test_query_float_no_rows_returns_none(self, db: Database) -> None:
        result = db.query_float("SELECT float_attribute FROM Configuration WHERE 1 = 0")
        assert result is None


# -- Parameterized queries (QUERY-02) -----------------------------------------


class TestQueryStringParameterized:
    def test_query_string_with_string_param(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", string_attribute="hello")
        result = db.query_string(
            "SELECT string_attribute FROM Configuration WHERE label = ?",
            params=["item1"],
        )
        assert result == "hello"

    def test_query_string_with_none_param(self, db: Database) -> None:
        db.create_element("Configuration", label="item1")
        result = db.query_integer(
            "SELECT COUNT(*) FROM Configuration WHERE ? IS NULL",
            params=[None],
        )
        assert result == 1


class TestQueryIntegerParameterized:
    def test_query_integer_with_integer_param(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", integer_attribute=99)
        result = db.query_integer(
            "SELECT integer_attribute FROM Configuration WHERE id = ?",
            params=[1],
        )
        assert result == 99

    def test_query_with_bool_param(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", boolean_attribute=1)
        result = db.query_integer(
            "SELECT boolean_attribute FROM Configuration WHERE boolean_attribute = ?",
            params=[True],
        )
        assert result == 1

    def test_query_with_mixed_params(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", integer_attribute=42)
        result = db.query_integer(
            "SELECT integer_attribute FROM Configuration WHERE label = ? AND id = ?",
            params=["item1", 1],
        )
        assert result == 42


class TestQueryFloatParameterized:
    def test_query_with_float_param(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", float_attribute=2.718)
        result = db.query_float(
            "SELECT float_attribute FROM Configuration WHERE float_attribute > ?",
            params=[2.0],
        )
        assert result is not None
        assert abs(result - 2.718) < 1e-9


class TestQueryEmptyParams:
    def test_query_with_empty_params_routes_to_simple(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", integer_attribute=7)
        result = db.query_integer(
            "SELECT integer_attribute FROM Configuration WHERE label = 'item1'",
            params=[],
        )
        assert result == 7


# -- Edge cases ----------------------------------------------------------------


class TestMarshalParamsErrors:
    def test_marshal_params_unsupported_type_raises_typeerror(self, db: Database) -> None:
        with pytest.raises(TypeError, match="Unsupported parameter type: dict"):
            db.query_string("SELECT 1", params=[{"bad": "type"}])

    def test_marshal_params_list_type_raises_typeerror(self, db: Database) -> None:
        with pytest.raises(TypeError, match="Unsupported parameter type: list"):
            db.query_string("SELECT 1", params=[[1, 2, 3]])


# -- query_date_time -----------------------------------------------------------


class TestQueryDateTime:
    def test_query_date_time_returns_datetime(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", date_attribute="2025-06-15T10:30:00")
        result = db.query_date_time("SELECT date_attribute FROM Configuration WHERE label = 'item1'")
        assert result is not None
        assert isinstance(result, datetime)
        assert result.year == 2025
        assert result.month == 6
        assert result.day == 15
        assert result.hour == 10
        assert result.minute == 30

    def test_query_date_time_no_rows_returns_none(self, db: Database) -> None:
        result = db.query_date_time("SELECT date_attribute FROM Configuration WHERE 1 = 0")
        assert result is None

    def test_query_date_time_with_params(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", date_attribute="2025-01-01T00:00:00")
        result = db.query_date_time(
            "SELECT date_attribute FROM Configuration WHERE label = ?",
            params=["item1"],
        )
        assert result is not None
        assert isinstance(result, datetime)
        assert result.year == 2025
        assert result.month == 1

    def test_query_date_time_invalid_format_raises_valueerror(self, db: Database) -> None:
        db.create_element("Configuration", label="item1", date_attribute="not-a-date")
        with pytest.raises(ValueError):
            db.query_date_time("SELECT date_attribute FROM Configuration WHERE label = 'item1'")
