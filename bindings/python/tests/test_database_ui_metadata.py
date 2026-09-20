from __future__ import annotations

from pathlib import Path

import pytest

from quiverdb import Database, QuiverError

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
