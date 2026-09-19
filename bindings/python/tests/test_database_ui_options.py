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


def _malformed_ui_dir() -> Path:
    return Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas" / "ui" / "malformed" / "ui"


def _open(tmp_path: Path, stem: str, *, ui_config_dir: str | None = None, ui_locale: str | None = None) -> Database:
    return Database.from_schema(
        str(tmp_path / f"{stem}.sqlite"),
        str(_foresight_schema_path()),
        ui_config_dir=ui_config_dir,
        ui_locale=ui_locale,
    )


def _write_migrations_dir(tmp_path: Path) -> Path:
    """Gap 7 (02-VERIFICATION.md): from_migrations() needs a migrations directory that actually
    creates the EconomicDriver schema the foresight_like sidecar labels -- no such fixture is
    checked in (the plan's files_modified list is the six test files only), so this writes one at
    runtime under pytest's tmp_path, mirroring foresight_like/schema.sql's own DDL. Nothing here
    is committed."""
    migrations_dir = tmp_path / "migrations"
    up_dir = migrations_dir / "1"
    up_dir.mkdir(parents=True)
    (up_dir / "up.sql").write_text(_foresight_schema_path().read_text(encoding="utf-8"), encoding="utf-8")
    (up_dir / "down.sql").write_text("DROP TABLE EconomicDriver;\nDROP TABLE Configuration;\n", encoding="utf-8")
    return migrations_dir


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


def test_malformed_sidecar_reports_false_without_raising(tmp_path: Path) -> None:
    """Gap 5 (02-VERIFICATION.md): malformed polarity, previously C++-only."""
    db = _open(tmp_path, "malformed", ui_config_dir=str(_malformed_ui_dir()))
    try:
        assert db.has_ui_config() is False
    finally:
        db.close()


def test_memory_database_distinction(tmp_path: Path) -> None:
    """Gap 5 (02-VERIFICATION.md): the D-01 :memory: distinction, previously C++-only -- an
    explicit ui_config_dir loads even for :memory:, while the <db_dir>/ui/ convention never
    fires for :memory: (there is no directory to resolve against)."""
    with_dir = Database.from_schema(":memory:", str(_foresight_schema_path()), ui_config_dir=str(_foresight_ui_dir()))
    try:
        assert with_dir.has_ui_config() is True
    finally:
        with_dir.close()

    without_dir = Database.from_schema(":memory:", str(_foresight_schema_path()))
    try:
        assert without_dir.has_ui_config() is False
    finally:
        without_dir.close()


def test_open_threads_ui_config_dir_and_locale(tmp_path: Path) -> None:
    """Gap 7 (02-VERIFICATION.md): open() with ui_config_dir + ui_locale, previously proven only
    through from_schema() by a committed test."""
    db_path = tmp_path / "reopened.sqlite"
    Database.from_schema(str(db_path), str(_foresight_schema_path())).close()

    db = Database.open(str(db_path), ui_config_dir=str(_foresight_ui_dir()), ui_locale="es")
    try:
        report = db.describe_collection("EconomicDriver")
        assert "Ingenuo Estacional" in report
        assert "Tendencia Lineal Local" in report
        assert "Seasonal Naïve" not in report
    finally:
        db.close()


def test_from_migrations_threads_ui_config_dir_and_locale(tmp_path: Path) -> None:
    """Gap 7 (02-VERIFICATION.md): from_migrations() with ui_config_dir + ui_locale, previously
    proven only through from_schema() by a committed test."""
    migrations_dir = _write_migrations_dir(tmp_path)
    db = Database.from_migrations(
        str(tmp_path / "migrated.sqlite"),
        str(migrations_dir),
        ui_config_dir=str(_foresight_ui_dir()),
        ui_locale="es",
    )
    try:
        report = db.describe_collection("EconomicDriver")
        assert "Ingenuo Estacional" in report
        assert "Tendencia Lineal Local" in report
        assert "Seasonal Naïve" not in report
    finally:
        db.close()


def test_describe_carries_locale_specific_label(tmp_path: Path) -> None:
    """Gap 7 (02-VERIFICATION.md): a locale label through whole-database describe(), not only
    describe_collection()."""
    db = _open(tmp_path, "describe_es", ui_config_dir=str(_foresight_ui_dir()), ui_locale="es")
    try:
        report = db.describe()
        assert "Ingenuo Estacional" in report
        assert "Seasonal Naïve" not in report
    finally:
        db.close()


def test_empty_string_ui_locale_matches_unset_output(tmp_path: Path) -> None:
    """OPT-03/empty (02-09 edge lift): an empty-string ui_locale is unset (D-03), not a locale
    named ""."""
    db = _open(tmp_path, "empty_locale", ui_config_dir=str(_foresight_ui_dir()), ui_locale="")
    try:
        report = db.describe_collection("EconomicDriver")
        assert "Seasonal Naïve" in report
        assert "Ingenuo Estacional" not in report
    finally:
        db.close()
