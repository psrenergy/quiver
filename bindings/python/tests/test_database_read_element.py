"""Tests for read_element_by_id convenience method."""
from __future__ import annotations

import pytest

from quiverdb import Database


class TestReadElementById:
    """Tests for Database.read_element_by_id."""

    def test_read_element_with_all_categories(self, collections_db: Database) -> None:
        """Element with scalars, multi-column vectors, and sets."""
        # Create prerequisite Configuration
        collections_db.create_element("Configuration", label="cfg")
        # Create Collection element with all data types
        elem_id = collections_db.create_element(
            "Collection",
            label="Item1",
            some_integer=42,
            some_float=3.14,
            value_int=[10, 20, 30],
            value_float=[1.1, 2.2, 3.3],
            tag=["alpha", "beta"],
        )

        result = collections_db.read_element_by_id("Collection", elem_id)

        # Scalars
        assert result["label"] == "Item1"
        assert result["some_integer"] == 42
        assert result["some_float"] == pytest.approx(3.14)

        # Vector columns (flat, not nested under group name)
        assert result["value_int"] == [10, 20, 30]
        assert result["value_float"] == pytest.approx([1.1, 2.2, 3.3])

        # Set column
        assert sorted(result["tag"]) == ["alpha", "beta"]

        # id is excluded
        assert "id" not in result

    def test_nonexistent_element_returns_empty_dict(self, collections_db: Database) -> None:
        """Nonexistent ID returns empty dict, no error."""
        collections_db.create_element("Configuration", label="cfg")
        result = collections_db.read_element_by_id("Collection", 9999)
        assert result == {}

    def test_scalars_only_no_vectors_or_sets(self, db: Database) -> None:
        """Schema with only scalar attributes (basic.sql Configuration)."""
        db.create_element("Configuration", label="Config1", integer_attribute=7)
        result = db.read_element_by_id("Configuration", 1)

        assert result["label"] == "Config1"
        assert result["integer_attribute"] == 7
        assert "id" not in result
