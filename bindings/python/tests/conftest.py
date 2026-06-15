from __future__ import annotations

from collections.abc import Generator
from pathlib import Path

import pytest

from quiverdb import Database


@pytest.fixture
def tests_path() -> Path:
    """Return the tests/ directory path."""
    return Path(__file__).resolve().parent


@pytest.fixture
def schemas_path() -> Path:
    """Return the shared test schemas directory."""
    return Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas"


@pytest.fixture
def valid_schema_path(schemas_path: Path) -> Path:
    """Return the path to the basic test schema."""
    return schemas_path / "valid" / "basic.sql"


@pytest.fixture
def hub_dir(schemas_path: Path) -> Path:
    """Return the from_hub fixture directory (contains migrations/ and ui/)."""
    return schemas_path / "from_hub"


@pytest.fixture
def collections_schema_path(schemas_path: Path) -> Path:
    """Return the path to the collections test schema."""
    return schemas_path / "valid" / "collections.sql"


@pytest.fixture
def relations_schema_path(schemas_path: Path) -> Path:
    """Return the path to the relations test schema."""
    return schemas_path / "valid" / "relations.sql"


@pytest.fixture
def db(valid_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database and close it after the test."""
    database = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    yield database
    database.close()


@pytest.fixture
def collections_db(collections_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the collections schema."""
    database = Database.from_schema(str(tmp_path / "collections.db"), str(collections_schema_path))
    yield database
    database.close()


@pytest.fixture
def relations_db(relations_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the relations schema."""
    database = Database.from_schema(str(tmp_path / "relations.db"), str(relations_schema_path))
    yield database
    database.close()


@pytest.fixture
def csv_export_schema_path(schemas_path: Path) -> Path:
    """Return the path to the CSV export test schema."""
    return schemas_path / "valid" / "csv_export.sql"


@pytest.fixture
def csv_db(csv_export_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the CSV export schema."""
    database = Database.from_schema(str(tmp_path / "csv.db"), str(csv_export_schema_path))
    yield database
    database.close()


@pytest.fixture
def csv_db_export(csv_db: Database) -> Database:
    """Return csv_db typed as DatabaseCSVExport (Database inherits it)."""
    return csv_db


@pytest.fixture
def csv_db_import(csv_db: Database) -> Database:
    """Return csv_db typed as DatabaseCSVImport (Database inherits it)."""
    return csv_db


@pytest.fixture
def all_types_schema_path(schemas_path: Path) -> Path:
    """Return the path to the all_types test schema."""
    return schemas_path / "valid" / "all_types.sql"


@pytest.fixture
def all_types_db(all_types_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the all_types schema."""
    database = Database.from_schema(str(tmp_path / "all_types.db"), str(all_types_schema_path))
    yield database
    database.close()


@pytest.fixture
def composite_helpers_schema_path(schemas_path: Path) -> Path:
    """Return the path to the composite helpers test schema."""
    return schemas_path / "valid" / "composite_helpers.sql"


@pytest.fixture
def composite_helpers_db(composite_helpers_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the composite helpers schema."""
    database = Database.from_schema(str(tmp_path / "composite.db"), str(composite_helpers_schema_path))
    yield database
    database.close()


@pytest.fixture
def mixed_time_series_schema_path(schemas_path: Path) -> Path:
    """Return the path to the mixed time series test schema."""
    return schemas_path / "valid" / "mixed_time_series.sql"


@pytest.fixture
def mixed_time_series_db(mixed_time_series_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the mixed time series schema (multi-type columns)."""
    database = Database.from_schema(str(tmp_path / "mixed_ts.db"), str(mixed_time_series_schema_path))
    yield database
    database.close()


@pytest.fixture
def nullable_time_series_schema_path(schemas_path: Path) -> Path:
    """Return the path to the nullable time series test schema (nullable value columns)."""
    return schemas_path / "valid" / "nullable_time_series.sql"


@pytest.fixture
def nullable_time_series_db(nullable_time_series_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the nullable time series schema (REAL/INTEGER/TEXT nullable columns)."""
    database = Database.from_schema(str(tmp_path / "nullable_ts.db"), str(nullable_time_series_schema_path))
    yield database
    database.close()


@pytest.fixture
def multi_dim_ts_schema_path(schemas_path: Path) -> Path:
    """Return the path to the multi-dim time series test schema."""
    return schemas_path / "valid" / "multi_dim_time_series.sql"


@pytest.fixture
def multi_dim_ts_db(multi_dim_ts_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the multi-dim time series schema (composite PK: date_time + block)."""
    database = Database.from_schema(str(tmp_path / "multi_dim_ts.db"), str(multi_dim_ts_schema_path))
    yield database
    database.close()
