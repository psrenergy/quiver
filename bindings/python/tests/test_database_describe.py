from __future__ import annotations

from collections.abc import Generator
from pathlib import Path

import pytest

from quiverdb import Database


# DESC-07: exact-string enum rendering against the shared tests/schemas/ui/ fixtures -- a
# "returns a str" assertion alone (kept in test_database_metadata.py's TestDescribe classes)
# cannot catch a per-binding decoding bug (D-31).
#
# Deliberately NOT built on the existing pytest temp-directory fixtures: a database created in
# a bare temp directory has no `ui/` sibling directory, so the test would pass by silently
# reading no sidecar at all and asserting the old, unchanged output. Each database here is
# built inside the fixture directory itself, under a filename unique to this binding and this
# test, so it cannot collide with the other bindings' or the C++/C API suites' own fixture
# databases.


@pytest.fixture
def ui_fixture(schemas_path: Path):
    def _ui_fixture(name: str) -> Path:
        return schemas_path / "ui" / name

    return _ui_fixture


def _open_ui_fixture(ui_fixture, name: str, stem: str) -> Database:
    directory = ui_fixture(name)
    return Database.from_schema(str(directory / f"python_{stem}.sqlite"), str(directory / "schema.sql"))


@pytest.fixture
def enum_declared_db(ui_fixture) -> Generator[Database, None, None]:
    database = _open_ui_fixture(ui_fixture, "enum_basic", "declared")
    yield database
    database.close()


@pytest.fixture
def enum_histogram_db(ui_fixture) -> Generator[Database, None, None]:
    database = _open_ui_fixture(ui_fixture, "enum_basic", "histogram")
    yield database
    database.close()


@pytest.fixture
def enum_header_db(ui_fixture) -> Generator[Database, None, None]:
    database = _open_ui_fixture(ui_fixture, "enum_basic", "header")
    yield database
    database.close()


@pytest.fixture
def enum_unit_hidden_db(ui_fixture) -> Generator[Database, None, None]:
    database = _open_ui_fixture(ui_fixture, "enum_basic", "unit_hidden")
    yield database
    database.close()


@pytest.fixture
def foresight_accented_db(ui_fixture) -> Generator[Database, None, None]:
    database = _open_ui_fixture(ui_fixture, "foresight_like", "accented")
    yield database
    database.close()


@pytest.fixture
def no_sidecar_db(ui_fixture) -> Generator[Database, None, None]:
    database = _open_ui_fixture(ui_fixture, "no_ui_dir", "no_sidecar")
    yield database
    database.close()


class TestEnumRendering:
    def test_declared_vocabulary_with_zero_elements(self, enum_declared_db: Database) -> None:
        report = enum_declared_db.describe_collection("Storage")
        assert "enum bool {0: Disabled, 1: Enabled}" in report

    def test_histogram_over_twelve_elements(self, enum_histogram_db: Database) -> None:
        for i in range(8):
            enum_histogram_db.create_element("Storage", label=f"Disabled {i}", has_commitment=0)
        for i in range(4):
            enum_histogram_db.create_element("Storage", label=f"Enabled {i}", has_commitment=1)

        report = enum_histogram_db.summarize_collection("Storage")
        assert "values {0: 8 (Disabled), 1: 4 (Enabled)}" in report

    def test_ui_config_header_line(self, enum_header_db: Database) -> None:
        report = enum_header_db.describe()
        assert "UI config: " in report
        assert " (locale: en)" in report

    def test_unit_and_hidden_decoration(self, enum_unit_hidden_db: Database) -> None:
        report = enum_unit_hidden_db.describe_collection("Storage")
        assert "[MW]" in report
        assert "[hidden]" in report

    def test_accented_enum_label_renders_byte_for_byte(self, foresight_accented_db: Database) -> None:
        report = foresight_accented_db.describe_collection("EconomicDriver")
        assert "Seasonal Naïve" in report

    def test_no_ui_directory_leaves_header_off(self, no_sidecar_db: Database) -> None:
        report = no_sidecar_db.describe()
        assert "UI config: " not in report
