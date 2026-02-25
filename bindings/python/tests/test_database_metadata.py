"""Tests for metadata get and list operations."""

from __future__ import annotations

import dataclasses

import pytest

from quiverdb import Database, Element, GroupMetadata, ScalarMetadata


# -- get_scalar_metadata -------------------------------------------------------


class TestGetScalarMetadata:
    def test_get_scalar_metadata_integer(self, db: Database) -> None:
        meta = db.get_scalar_metadata("Configuration", "integer_attribute")
        assert meta.name == "integer_attribute"
        assert meta.data_type == 0  # QUIVER_DATA_TYPE_INTEGER
        assert meta.not_null is False
        assert meta.primary_key is False
        assert meta.is_foreign_key is False
        assert meta.references_collection is None
        assert meta.references_column is None

    def test_get_scalar_metadata_string(self, db: Database) -> None:
        meta = db.get_scalar_metadata("Configuration", "string_attribute")
        assert meta.name == "string_attribute"
        assert meta.data_type == 2  # QUIVER_DATA_TYPE_STRING

    def test_get_scalar_metadata_label(self, db: Database) -> None:
        meta = db.get_scalar_metadata("Configuration", "label")
        assert meta.name == "label"
        assert meta.data_type == 2  # TEXT -> STRING
        assert meta.not_null is True

    def test_get_scalar_metadata_primary_key(self, db: Database) -> None:
        meta = db.get_scalar_metadata("Configuration", "id")
        assert meta.name == "id"
        assert meta.data_type == 0  # INTEGER
        assert meta.primary_key is True

    def test_get_scalar_metadata_with_default(self, db: Database) -> None:
        # integer_attribute has DEFAULT 6 in basic.sql
        meta = db.get_scalar_metadata("Configuration", "integer_attribute")
        assert meta.default_value is not None
        assert "6" in meta.default_value

    def test_get_scalar_metadata_date_attribute(self, db: Database) -> None:
        meta = db.get_scalar_metadata("Configuration", "date_attribute")
        assert meta.name == "date_attribute"
        assert meta.data_type == 3  # QUIVER_DATA_TYPE_DATE_TIME

    def test_get_scalar_metadata_foreign_key(self, relations_db: Database) -> None:
        meta = relations_db.get_scalar_metadata("Child", "parent_id")
        assert meta.name == "parent_id"
        assert meta.is_foreign_key is True
        assert meta.references_collection == "Parent"
        assert meta.references_column == "id"

    def test_get_scalar_metadata_self_reference_fk(self, relations_db: Database) -> None:
        meta = relations_db.get_scalar_metadata("Child", "sibling_id")
        assert meta.is_foreign_key is True
        assert meta.references_collection == "Child"
        assert meta.references_column == "id"


# -- get_vector_metadata / get_set_metadata -----------------------------------


class TestGetGroupMetadata:
    def test_get_vector_metadata(self, collections_db: Database) -> None:
        meta = collections_db.get_vector_metadata("Collection", "values")
        assert isinstance(meta, GroupMetadata)
        assert meta.group_name == "values"
        assert meta.dimension_column == ""  # vectors have no dimension
        assert len(meta.value_columns) >= 1
        col_names = [c.name for c in meta.value_columns]
        assert "value_int" in col_names
        assert "value_float" in col_names

    def test_get_vector_metadata_value_column_types(self, collections_db: Database) -> None:
        meta = collections_db.get_vector_metadata("Collection", "values")
        by_name = {c.name: c for c in meta.value_columns}
        assert by_name["value_int"].data_type == 0  # INTEGER
        assert by_name["value_float"].data_type == 1  # FLOAT

    def test_get_set_metadata(self, collections_db: Database) -> None:
        meta = collections_db.get_set_metadata("Collection", "tags")
        assert isinstance(meta, GroupMetadata)
        assert meta.group_name == "tags"
        assert meta.dimension_column == ""
        assert len(meta.value_columns) >= 1
        col_names = [c.name for c in meta.value_columns]
        assert "tag" in col_names

    def test_get_time_series_metadata(self, collections_db: Database) -> None:
        meta = collections_db.get_time_series_metadata("Collection", "data")
        assert isinstance(meta, GroupMetadata)
        assert meta.group_name == "data"
        assert meta.dimension_column != ""  # time series has dimension
        assert "date" in meta.dimension_column.lower()


# -- list_scalar_attributes ----------------------------------------------------


class TestListScalarAttributes:
    def test_list_scalar_attributes(self, db: Database) -> None:
        attrs = db.list_scalar_attributes("Configuration")
        assert isinstance(attrs, list)
        assert len(attrs) > 0
        assert all(isinstance(a, ScalarMetadata) for a in attrs)
        names = [a.name for a in attrs]
        assert "label" in names
        assert "integer_attribute" in names
        assert "float_attribute" in names
        assert "string_attribute" in names

    def test_list_scalar_attributes_metadata_fields(self, db: Database) -> None:
        attrs = db.list_scalar_attributes("Configuration")
        by_name = {a.name: a for a in attrs}
        label = by_name["label"]
        assert label.not_null is True
        assert label.data_type == 2  # TEXT -> STRING


# -- list_vector_groups / list_set_groups / list_time_series_groups -----------


class TestListGroups:
    def test_list_vector_groups(self, collections_db: Database) -> None:
        groups = collections_db.list_vector_groups("Collection")
        assert isinstance(groups, list)
        assert len(groups) >= 1
        assert all(isinstance(g, GroupMetadata) for g in groups)
        names = [g.group_name for g in groups]
        assert "values" in names

    def test_list_set_groups(self, collections_db: Database) -> None:
        groups = collections_db.list_set_groups("Collection")
        assert isinstance(groups, list)
        assert len(groups) >= 1
        names = [g.group_name for g in groups]
        assert "tags" in names

    def test_list_time_series_groups(self, collections_db: Database) -> None:
        groups = collections_db.list_time_series_groups("Collection")
        assert isinstance(groups, list)
        assert len(groups) >= 1
        names = [g.group_name for g in groups]
        assert "data" in names
        # Time series groups should have non-empty dimension_column
        for g in groups:
            assert g.dimension_column != ""


# -- Frozen dataclass behavior ------------------------------------------------


class TestMetadataFrozen:
    def test_scalar_metadata_frozen(self, db: Database) -> None:
        meta = db.get_scalar_metadata("Configuration", "label")
        with pytest.raises(dataclasses.FrozenInstanceError):
            meta.name = "changed"  # type: ignore[misc]

    def test_group_metadata_frozen(self, collections_db: Database) -> None:
        meta = collections_db.get_vector_metadata("Collection", "values")
        with pytest.raises(dataclasses.FrozenInstanceError):
            meta.group_name = "changed"  # type: ignore[misc]
