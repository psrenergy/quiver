from __future__ import annotations

from pathlib import Path

from quiverdb import Database
from quiverdb._c_api import ffi

# OPT-01/OPT-02/OPT-03/OPT-04/OPT-06 (Phase 2, plan 02-03): the grown `quiver_database_options_t`
# (24 bytes: read_only@0, console_level@4, ui_config_dir@8, ui_locale@16), the new `ui_config_dir`/
# `ui_locale` kwargs on Database.open/from_schema/from_migrations, and `has_ui_config()`.
#
# Deliberately built on pytest's `tmp_path` for the database file rather than the repo's shared
# `foresight_like` fixture directory: `tmp_path` has no `ui/` sibling of its own, so a config that
# loads here can only have come from the *explicit* `ui_config_dir` argument, never the
# `<db_dir>/ui/` convention -- that is the whole point of the proof (D-17).
#
# The fixture itself is referenced by path, never copied into `bindings/` (root rule; enforced from
# the C++ suite by DatabaseUiCorpus.FixturesAreNeverCopiedIntoABinding).


def _foresight_fixture_dir() -> Path:
    return Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas" / "ui" / "foresight_like"


def _foresight_schema_path() -> Path:
    return _foresight_fixture_dir() / "schema.sql"


def _foresight_ui_dir() -> Path:
    return _foresight_fixture_dir() / "ui"


def _open(tmp_path: Path, stem: str, *, ui_config_dir: str | None = None, ui_locale: str | None = None) -> Database:
    return Database.from_schema(
        str(tmp_path / f"{stem}.sqlite"),
        str(_foresight_schema_path()),
        ui_config_dir=ui_config_dir,
        ui_locale=ui_locale,
    )


def test_options_struct_layout_matches_native() -> None:
    """The hand-edited cdef matches the frozen native header exactly (OPT-06)."""
    assert ffi.sizeof("quiver_database_options_t") == 24
    assert ffi.offsetof("quiver_database_options_t", "read_only") == 0
    assert ffi.offsetof("quiver_database_options_t", "console_level") == 4
    assert ffi.offsetof("quiver_database_options_t", "ui_config_dir") == 8
    assert ffi.offsetof("quiver_database_options_t", "ui_locale") == 16


def test_explicit_ui_config_dir_loads_config_not_beside_database(tmp_path: Path) -> None:
    """A database file with no `ui/` sibling still loads the sidecar via an explicit directory."""
    db = _open(tmp_path, "explicit_dir", ui_config_dir=str(_foresight_ui_dir()))
    try:
        assert db.has_ui_config() is True
        report = db.describe_collection("EconomicDriver")
        assert "Seasonal Naïve" in report
    finally:
        db.close()


def test_spanish_locale_renders_spanish_labels(tmp_path: Path) -> None:
    """ui_locale='es' renders the Spanish enum labels, not their English counterparts (L18/L19)."""
    db = _open(tmp_path, "locale_es", ui_config_dir=str(_foresight_ui_dir()), ui_locale="es")
    try:
        report = db.describe_collection("EconomicDriver")
        assert "Ingenuo Estacional" in report
        assert "Tendencia Lineal Local" in report
        assert "Seasonal Naïve" not in report
    finally:
        db.close()


def test_default_locale_renders_english_labels(tmp_path: Path) -> None:
    """With no ui_locale given, the Phase 1 default ('en') is preserved."""
    db = _open(tmp_path, "locale_default", ui_config_dir=str(_foresight_ui_dir()))
    try:
        report = db.describe_collection("EconomicDriver")
        assert "Seasonal Naïve" in report
        assert "Ingenuo Estacional" not in report
    finally:
        db.close()


def test_missing_ui_config_dir_degrades_without_raising(tmp_path: Path) -> None:
    """An explicit directory that does not exist degrades to no config -- it never raises."""
    missing_dir = tmp_path / "does_not_exist"
    db = _open(tmp_path, "missing_dir", ui_config_dir=str(missing_dir))
    try:
        assert db.has_ui_config() is False
    finally:
        db.close()
