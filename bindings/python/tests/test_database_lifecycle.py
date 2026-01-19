"""Database lifecycle tests."""

import tempfile
from pathlib import Path

import pytest

from psr_database import Database, DatabaseError

SCHEMAS_PATH = Path(__file__).parent.parent.parent.parent / "tests" / "schemas"


class TestFromSchema:
    def test_creates_in_memory_database(self):
        db = Database.from_schema(":memory:", str(SCHEMAS_PATH / "valid" / "basic.sql"))
        assert db.is_healthy()
        db.close()

    def test_creates_file_database(self):
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
            db_path = f.name
        db = Database.from_schema(db_path, str(SCHEMAS_PATH / "valid" / "basic.sql"))
        assert db.is_healthy()
        assert Path(db_path).exists()
        db.close()
        Path(db_path).unlink()

    def test_fails_with_invalid_schema(self):
        with pytest.raises(DatabaseError):
            Database.from_schema(":memory:", "nonexistent.sql")


class TestClose:
    def test_close_is_idempotent(self):
        db = Database.from_schema(":memory:", str(SCHEMAS_PATH / "valid" / "basic.sql"))
        db.close()
        db.close()  # Should not raise

    def test_is_healthy_after_close(self):
        db = Database.from_schema(":memory:", str(SCHEMAS_PATH / "valid" / "basic.sql"))
        db.close()
        assert not db.is_healthy()


class TestContextManager:
    def test_context_manager_closes(self):
        with Database.from_schema(":memory:", str(SCHEMAS_PATH / "valid" / "basic.sql")) as db:
            assert db.is_healthy()
        assert not db.is_healthy()


class TestPath:
    def test_memory_path(self):
        db = Database.from_schema(":memory:", str(SCHEMAS_PATH / "valid" / "basic.sql"))
        assert db.path() == ":memory:"
        db.close()


class TestVersion:
    def test_new_database_version_zero(self):
        db = Database.from_schema(":memory:", str(SCHEMAS_PATH / "valid" / "basic.sql"))
        assert db.current_version() == 0
        db.close()
