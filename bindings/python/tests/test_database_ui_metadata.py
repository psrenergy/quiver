from __future__ import annotations

from pathlib import Path

import pytest

from quiverdb import Database, QuiverError, UiEnumEntry

# Phase 3 tracer slice (META-01/META-02/META-05, D-36, D-41): get_attribute_ui_metadata proves the
# whole path -- TOML sidecar -> C++ getter -> quiver_ui_metadata_t -> this hand-written decoder --
# end to end for one attribute. Fixture referenced from tests/schemas/, never copied (CORPUS-03).


def _enum_basic_dir() -> Path:
    return Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas" / "ui" / "enum_basic"


def _enum_basic_schema_path() -> Path:
    return _enum_basic_dir() / "schema.sql"


def _open(tmp_path: Path, stem: str) -> Database:
    # The database file lives inside the fixture directory itself (not tmp_path) so its parent's
    # `ui/` sibling resolves via the <db_dir>/ui/ convention -- matching every other binding's own
    # enum_basic fixture files (python_declared.sqlite etc.) already checked in beside it.
    return Database.from_schema(str(_enum_basic_dir() / f"{stem}.sqlite"), str(_enum_basic_schema_path()))


def test_configured_attribute_carries_label_and_vocabulary(tmp_path: Path) -> None:
    db = _open(tmp_path, "python_ui_metadata_configured")
    try:
        meta = db.get_attribute_ui_metadata("Storage", "has_commitment")
        assert meta.configured is True
        assert meta.label == "Has Commitment"
        assert meta.vocabulary == "bool"
        assert meta.display_order == -1
    finally:
        db.close()


def test_unit_distinguishes_offset_16_from_neighbours(tmp_path: Path) -> None:
    """max_generation is the one attribute carrying a non-empty `unit` -- distinguishes offset 16
    from the empty tooltip@8/format@24/icon@32 around it. A record asserted only on label/vocabulary
    would pass with those four transposed."""
    db = _open(tmp_path, "python_ui_metadata_unit")
    try:
        meta = db.get_attribute_ui_metadata("Storage", "max_generation")
        assert meta.unit == "MW"
    finally:
        db.close()


def test_hidden_distinguishes_offset_60_from_configured(tmp_path: Path) -> None:
    """internal_code is the only hide=true attribute -- the only thing that tells offset 60
    (hidden) apart from offset 56 (configured), since both are `int` and configured is true on
    every configured record."""
    db = _open(tmp_path, "python_ui_metadata_hidden")
    try:
        meta = db.get_attribute_ui_metadata("Storage", "internal_code")
        assert meta.hidden is True
        assert meta.label == "Internal Code"
    finally:
        db.close()


def test_declared_blank_label_is_configured_not_absent(tmp_path: Path) -> None:
    """D-12: notes declares label = "" -- declared-blank is not unconfigured."""
    db = _open(tmp_path, "python_ui_metadata_blank")
    try:
        meta = db.get_attribute_ui_metadata("Storage", "notes")
        assert meta.configured is True
        assert meta.label == ""
    finally:
        db.close()


def test_real_unconfigured_column_returns_default_without_raising(tmp_path: Path) -> None:
    """META-02: `label` is a real Storage column the sidecar never mentions -- configured=False,
    every string field empty, no raise."""
    db = _open(tmp_path, "python_ui_metadata_unconfigured")
    try:
        meta = db.get_attribute_ui_metadata("Storage", "label")
        assert meta.configured is False
        assert meta.label == ""
        assert meta.tooltip == ""
        assert meta.unit == ""
        assert meta.format == ""
        assert meta.icon == ""
        assert meta.vocabulary == ""
        assert meta.display_order == -1
    finally:
        db.close()


def test_nonexistent_collection_raises_precondition_message(tmp_path: Path) -> None:
    """require_collection runs before the column check -- a bad collection name reports Pattern 1
    ("Cannot get_attribute_ui_metadata: ..."), distinct from the column-level Pattern 2 message."""
    db = _open(tmp_path, "python_ui_metadata_no_such_collection")
    try:
        with pytest.raises(QuiverError) as exc_info:
            db.get_attribute_ui_metadata("NoSuchCollection", "has_commitment")
        assert "Cannot get_attribute_ui_metadata" in str(exc_info.value)
    finally:
        db.close()


def test_nonexistent_column_raises_exact_pattern_2_message(tmp_path: Path) -> None:
    """D-36: an absent SQL column throws, distinguishing this design from returning a default
    record for every miss regardless of source."""
    db = _open(tmp_path, "python_ui_metadata_no_such_column")
    try:
        with pytest.raises(QuiverError) as exc_info:
            db.get_attribute_ui_metadata("Storage", "no_such_column")
        assert str(exc_info.value) == "Scalar attribute not found: 'no_such_column' in collection 'Storage'"
    finally:
        db.close()


# -- Plan 03-02: quiver_database_list_ui_vocabularies / quiver_database_get_ui_vocabulary --------
# closing the C API's UI-metadata surface (META-05) with Python as the reference FFI decoder.


def test_list_ui_vocabularies_returns_fixture_names(tmp_path: Path) -> None:
    """enum_basic declares exactly one vocabulary -- a shape assertion, not an ordering one. The
    non-vacuous ordering proof is 03-01 Task 3's three-name C++ scratch sidecar."""
    db = _open(tmp_path, "python_ui_metadata_list_vocab")
    try:
        assert db.list_ui_vocabularies() == ["bool"]
    finally:
        db.close()


def test_get_ui_vocabulary_returns_ordered_entries(tmp_path: Path) -> None:
    db = _open(tmp_path, "python_ui_metadata_get_vocab")
    try:
        entries = db.get_ui_vocabulary("bool")
        assert entries == [
            UiEnumEntry(code=0, label="Disabled"),
            UiEnumEntry(code=1, label="Enabled"),
        ]
    finally:
        db.close()


def test_get_ui_vocabulary_unknown_name_raises_exact_message(tmp_path: Path) -> None:
    db = _open(tmp_path, "python_ui_metadata_vocab_missing")
    try:
        with pytest.raises(QuiverError) as exc_info:
            db.get_ui_vocabulary("no_such_vocabulary")
        assert str(exc_info.value) == "Vocabulary not found: 'no_such_vocabulary'"
    finally:
        db.close()


def test_get_ui_vocabulary_declared_but_empty_returns_empty_list(tmp_path: Path) -> None:
    """A vocabulary declared with a name but zero entries is a present key holding an empty
    vector -- structurally different from an undeclared name, which raises. No tracked fixture
    exercises this (the one confirmed corpus gap 03-01 closed at the C++ layer with a scratch
    sidecar); this mirrors that sidecar under pytest's tmp_path, following the tmp_path idiom
    already used in test_database_ui_options.py. The database file lives in tmp_path itself so the
    <db_dir>/ui/ convention resolves the sidecar with no explicit ui_config_dir needed."""
    (tmp_path / "ui").mkdir()
    (tmp_path / "schema.sql").write_text(
        "CREATE TABLE Configuration (\n    id INTEGER PRIMARY KEY,\n    label TEXT UNIQUE NOT NULL\n) STRICT;\n",
        encoding="utf-8",
    )
    (tmp_path / "ui" / "main.toml").write_text("collections = []\n", encoding="utf-8")
    (tmp_path / "ui" / "enum.toml").write_text("alpha = []\n", encoding="utf-8")

    db = Database.from_schema(str(tmp_path / "declared_but_empty.sqlite"), str(tmp_path / "schema.sql"))
    try:
        assert db.get_ui_vocabulary("alpha") == []
    finally:
        db.close()
