"""Tests for CSV export operations (export_csv, import_csv stub)."""

from __future__ import annotations

import csv
from io import StringIO

import pytest

from quiver_db import CSVExportOptions, Database, Element


class TestExportCSVScalarsDefault:
    """Test scalar CSV export with default options."""

    def test_export_csv_scalars_default(self, csv_db: Database, tmp_path):
        """Export scalars with default options: verify header and data rows."""
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        e1.set("status", 1)
        e1.set("price", 9.99)
        e1.set("date_created", "2024-01-15T10:30:00")
        e1.set("notes", "first")
        csv_db.create_element("Items", e1)

        e2 = Element()
        e2.set("label", "Item2")
        e2.set("name", "Beta")
        e2.set("status", 2)
        e2.set("price", 19.5)
        e2.set("date_created", "2024-02-20T08:00:00")
        e2.set("notes", "second")
        csv_db.create_element("Items", e2)

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
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        e1.set("status", 1)
        csv_db.create_element("Items", e1)

        e2 = Element()
        e2.set("label", "Item2")
        e2.set("name", "Beta")
        e2.set("status", 2)
        csv_db.create_element("Items", e2)

        out = str(tmp_path / "enums.csv")
        opts = CSVExportOptions(
            enum_labels={"status": {1: "Active", 2: "Inactive"}},
        )
        csv_db.export_csv("Items", "", out, opts)

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
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        e1.set("status", 1)
        e1.set("date_created", "2024-01-15T10:30:00")
        csv_db.create_element("Items", e1)

        out = str(tmp_path / "dates.csv")
        opts = CSVExportOptions(date_time_format="%Y/%m/%d")
        csv_db.export_csv("Items", "", out, opts)

        content = _read_file(out)
        # date_created formatted
        assert "2024/01/15" in content
        # Raw ISO format should not appear
        assert "2024-01-15T10:30:00" not in content


class TestExportCSVWithEnumAndDate:
    """Test CSV export with both enum labels and date formatting."""

    def test_export_csv_with_enum_and_date(self, csv_db: Database, tmp_path):
        """Combined options: both enum and date formatting applied."""
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        e1.set("status", 1)
        e1.set("date_created", "2024-01-15T10:30:00")
        csv_db.create_element("Items", e1)

        out = str(tmp_path / "combined.csv")
        opts = CSVExportOptions(
            date_time_format="%Y-%m-%d",
            enum_labels={"status": {1: "Active"}},
        )
        csv_db.export_csv("Items", "", out, opts)

        content = _read_file(out)
        assert "Active" in content
        assert "2024-01-15" in content
        # Raw ISO datetime should not appear
        assert "2024-01-15T10:30:00" not in content


class TestExportCSVGroup:
    """Test CSV export for vector groups."""

    def test_export_csv_group(self, csv_db: Database, tmp_path):
        """Export vector group: verify header and data rows."""
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        csv_db.create_element("Items", e1)

        e2 = Element()
        e2.set("label", "Item2")
        e2.set("name", "Beta")
        csv_db.create_element("Items", e2)

        csv_db.update_vector_floats("Items", "measurement", 1, [1.1, 2.2, 3.3])
        csv_db.update_vector_floats("Items", "measurement", 2, [4.4, 5.5])

        out = str(tmp_path / "group.csv")
        csv_db.export_csv("Items", "measurements", out)

        content = _read_file(out)
        # Header: label + value column(s)
        assert "label,measurement\n" in content
        # Data rows: one per vector element
        assert "Item1,1.1\n" in content
        assert "Item1,2.2\n" in content
        assert "Item1,3.3\n" in content
        assert "Item2,4.4\n" in content
        assert "Item2,5.5\n" in content


class TestExportCSVNullValues:
    """Test CSV export with NULL values."""

    def test_export_csv_null_values(self, csv_db: Database, tmp_path):
        """NULL values appear as empty fields in CSV."""
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        # status, price, date_created, notes all NULL
        csv_db.create_element("Items", e1)

        out = str(tmp_path / "nulls.csv")
        csv_db.export_csv("Items", "", out)

        content = _read_file(out)
        # NULL fields as empty: Item1,Alpha,,,,
        assert "Item1,Alpha,,,,\n" in content


class TestImportCSVStub:
    """Test import_csv raises NotImplementedError."""

    def test_import_csv_raises_not_implemented(self, csv_db: Database):
        """import_csv raises NotImplementedError with descriptive message."""
        with pytest.raises(NotImplementedError, match="not yet implemented"):
            csv_db.import_csv("Items", "any.csv")


class TestCSVExportOptionsDefaults:
    """Test CSVExportOptions dataclass defaults."""

    def test_default_options(self):
        """Default options have empty date_time_format and no enum labels."""
        opts = CSVExportOptions()
        assert opts.date_time_format == ""
        assert opts.enum_labels == {}

    def test_custom_options(self):
        """Options can be constructed with custom values."""
        opts = CSVExportOptions(
            date_time_format="%Y",
            enum_labels={"status": {1: "Active"}},
        )
        assert opts.date_time_format == "%Y"
        assert opts.enum_labels == {"status": {1: "Active"}}


# -- Helper -------------------------------------------------------------------


def _read_file(path: str) -> str:
    """Read file as string with binary mode to preserve LF."""
    with open(path, "rb") as f:
        return f.read().decode("utf-8")
