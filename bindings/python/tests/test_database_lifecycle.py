from __future__ import annotations

from pathlib import Path

import pytest

import quiverdb
from quiverdb import Database, QuiverError


def test_from_schema_creates_database(valid_schema_path: Path, tmp_path: Path) -> None:
    db = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    assert db is not None
    db.close()


def test_from_schema_context_manager(valid_schema_path: Path, tmp_path: Path) -> None:
    with Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path)) as db:
        assert db.is_healthy()
    # After exiting, operations should raise
    with pytest.raises(QuiverError, match="closed"):
        db.path()


def test_from_migrations_creates_database(migrations_path: Path, tmp_path: Path) -> None:
    db = Database.from_migrations(str(tmp_path / "test.db"), str(migrations_path))
    assert db is not None
    db.close()


def test_close_is_idempotent(db: Database) -> None:
    db.close()
    db.close()  # Should not raise


def test_operations_after_close_raise(valid_schema_path: Path, tmp_path: Path) -> None:
    db = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    db.close()
    with pytest.raises(QuiverError, match="closed"):
        db.path()


def test_path_returns_string(db: Database) -> None:
    result = db.path()
    assert isinstance(result, str)
    assert "test.db" in result


def test_current_version_returns_int(db: Database) -> None:
    result = db.current_version()
    assert isinstance(result, int)
    assert result >= 0


def test_is_healthy_returns_true(db: Database) -> None:
    assert db.is_healthy() is True


def test_describe_runs_without_error(db: Database) -> None:
    db.describe()  # Should not raise; output goes to stdout


def test_version_returns_string() -> None:
    result = quiverdb.version()
    assert isinstance(result, str)
    assert len(result) > 0


def test_error_on_nonexistent_schema(tmp_path: Path) -> None:
    with pytest.raises(QuiverError):
        Database.from_schema(str(tmp_path / "test.db"), "nonexistent_schema.sql")


def test_repr_open_database(db: Database) -> None:
    r = repr(db)
    assert "Database(path=" in r


def test_repr_closed_database(valid_schema_path: Path, tmp_path: Path) -> None:
    db = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    db.close()
    assert repr(db) == "Database(closed)"
