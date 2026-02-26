"""Tests for CSV export operations."""

from __future__ import annotations

import pytest

from quiverdb import CSVOptions, Database


class TestExportCSVScalarsDefault:
    """Test scalar CSV export with default options."""

    def test_export_csv_scalars_default(self, csv_db: Database, tmp_path):
        """Export scalars with default options: verify header and data rows."""
        csv_db.create_element(
            "Items",
            label="Item1",
            name="Alpha",
            status=1,
            price=9.99,
            date_created="2024-01-15T10:30:00",
            notes="first",
        )
        csv_db.create_element(
            "Items",
            label="Item2",
            name="Beta",
            status=2,
            price=19.5,
            date_created="2024-02-20T08:00:00",
            notes="second",
        )

        out = str(tmp_path / "scalars.csv")
        csv_db.export_csv("Items", "", out)

        content = _read_file(out)
        # Header row (after sep=, line)
        assert "label,name,status,price,date_created,notes\n" in content
        # Data rows
        assert "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n" in content
        assert "Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n" in content


class TestExportCSVWithEnumLabels:
    """Test CSV export with enum label resolution."""

    def test_export_csv_with_enum_labels(self, csv_db: Database, tmp_path):
        """Enum labels replace integer values in CSV output."""
        csv_db.create_element("Items", label="Item1", name="Alpha", status=1)
        csv_db.create_element("Items", label="Item2", name="Beta", status=2)

        out = str(tmp_path / "enums.csv")
        options = CSVOptions(
            enum_labels={"status": {"en": {"Active": 1, "Inactive": 2}}},
        )
        csv_db.export_csv("Items", "", out, options=options)

        content = _read_file(out)
        assert "Item1,Alpha,Active," in content
        assert "Item2,Beta,Inactive," in content
        # Raw integers should not appear as status
        assert "Item1,Alpha,1," not in content
        assert "Item2,Beta,2," not in content


class TestExportCSVWithDateFormat:
    """Test CSV export with date formatting."""

    def test_export_csv_with_date_format(self, csv_db: Database, tmp_path):
        """DateTime formatting applies strftime to date columns."""
        csv_db.create_element("Items", label="Item1", name="Alpha", status=1, date_created="2024-01-15T10:30:00")

        out = str(tmp_path / "dates.csv")
        options = CSVOptions(date_time_format="%Y/%m/%d")
        csv_db.export_csv("Items", "", out, options=options)

        content = _read_file(out)
        # date_created formatted
        assert "2024/01/15" in content
        # Raw ISO format should not appear
        assert "2024-01-15T10:30:00" not in content


class TestExportCSVWithEnumAndDate:
    """Test CSV export with both enum labels and date formatting."""

    def test_export_csv_with_enum_and_date(self, csv_db: Database, tmp_path):
        """Combined options: both enum and date formatting applied."""
        csv_db.create_element("Items", label="Item1", name="Alpha", status=1, date_created="2024-01-15T10:30:00")

        out = str(tmp_path / "combined.csv")
        options = CSVOptions(
            date_time_format="%Y-%m-%d",
            enum_labels={"status": {"en": {"Active": 1}}},
        )
        csv_db.export_csv("Items", "", out, options=options)

        content = _read_file(out)
        assert "Active" in content
        assert "2024-01-15" in content
        # Raw ISO datetime should not appear
        assert "2024-01-15T10:30:00" not in content


class TestExportCSVGroup:
    """Test CSV export for vector groups."""

    def test_export_csv_group(self, csv_db: Database, tmp_path):
        """Export vector group: verify header and data rows."""
        csv_db.create_element("Items", label="Item1", name="Alpha")
        csv_db.create_element("Items", label="Item2", name="Beta")

        csv_db.update_element("Items", 1, measurement=[1.1, 2.2, 3.3])
        csv_db.update_element("Items", 2, measurement=[4.4, 5.5])

        out = str(tmp_path / "group.csv")
        csv_db.export_csv("Items", "measurements", out)

        content = _read_file(out)
        # Header: sep line + id,vector_index,value column(s)
        assert "sep=,\nid,vector_index,measurement\n" in content
        # Data rows: label, vector_index, value per vector element
        assert "Item1,1,1.1\n" in content
        assert "Item1,2,2.2\n" in content
        assert "Item1,3,3.3\n" in content
        assert "Item2,1,4.4\n" in content
        assert "Item2,2,5.5\n" in content


class TestExportCSVNullValues:
    """Test CSV export with NULL values."""

    def test_export_csv_null_values(self, csv_db: Database, tmp_path):
        """NULL values appear as empty fields in CSV."""
        csv_db.create_element("Items", label="Item1", name="Alpha")

        out = str(tmp_path / "nulls.csv")
        csv_db.export_csv("Items", "", out)

        content = _read_file(out)
        # NULL fields as empty: Item1,Alpha,,,,
        assert "Item1,Alpha,,,,\n" in content


class TestCSVOptionsDefaults:
    """Test CSVOptions dataclass defaults."""

    def test_default_options(self):
        """Default options have empty date_time_format and no enum labels."""
        options = CSVOptions()
        assert options.date_time_format == ""
        assert options.enum_labels == {}

    def test_custom_options(self):
        """Options can be constructed with custom values."""
        options = CSVOptions(
            date_time_format="%Y",
            enum_labels={"status": {"en": {"Active": 1}}},
        )
        assert options.date_time_format == "%Y"
        assert options.enum_labels == {"status": {"en": {"Active": 1}}}


# -- Helper -------------------------------------------------------------------


def _read_file(path: str) -> str:
    """Read file as string with binary mode to preserve LF."""
    with open(path, "rb") as f:
        return f.read().decode("utf-8")
