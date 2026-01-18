"""pytest fixtures for PSR Database tests."""
import os
from pathlib import Path
from typing import Generator

import pytest

from psr_database import Database


def get_schemas_dir() -> Path:
    """Get the path to the shared test schemas directory."""
    # Navigate from bindings/python/tests to tests/schemas/valid
    current = Path(__file__).parent
    return current.parent.parent.parent / "tests" / "schemas" / "valid"


@pytest.fixture
def basic_schema_path() -> Path:
    """Path to the basic test schema."""
    return get_schemas_dir() / "basic.sql"


@pytest.fixture
def collections_schema_path() -> Path:
    """Path to the collections test schema (with vectors and sets)."""
    return get_schemas_dir() / "collections.sql"


@pytest.fixture
def relations_schema_path() -> Path:
    """Path to the relations test schema (with foreign keys)."""
    return get_schemas_dir() / "relations.sql"


@pytest.fixture
def basic_db(basic_schema_path: Path) -> Generator[Database, None, None]:
    """Create an in-memory database with the basic schema."""
    db = Database.from_schema(":memory:", str(basic_schema_path))
    yield db
    db.close()


@pytest.fixture
def collections_db(collections_schema_path: Path) -> Generator[Database, None, None]:
    """Create an in-memory database with the collections schema."""
    db = Database.from_schema(":memory:", str(collections_schema_path))
    yield db
    db.close()


@pytest.fixture
def relations_db(relations_schema_path: Path) -> Generator[Database, None, None]:
    """Create an in-memory database with the relations schema."""
    db = Database.from_schema(":memory:", str(relations_schema_path))
    yield db
    db.close()
