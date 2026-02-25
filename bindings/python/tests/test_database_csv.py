"""Tests for CSV export and import operations."""

from __future__ import annotations

import csv
from io import StringIO

import pytest

from quiverdb import CSVOptions, Database, Element


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
        opts = CSVOptions(
            enum_labels={"status": {"en": {"Active": 1, "Inactive": 2}}},
        )
        csv_db.export_csv("Items", "", out, options=opts)

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
        opts = CSVOptions(date_time_format="%Y/%m/%d")
        csv_db.export_csv("Items", "", out, options=opts)

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
        opts = CSVOptions(
            date_time_format="%Y-%m-%d",
            enum_labels={"status": {"en": {"Active": 1}}},
        )
        csv_db.export_csv("Items", "", out, options=opts)

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


class TestImportCSVScalarRoundTrip:
    """Test scalar CSV import round-trip: export -> import -> verify."""

    def test_scalar_round_trip(self, csv_db: Database, tmp_path, csv_export_schema_path):
        """Export scalars, import into fresh DB, read back matches original."""
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

        csv_path = str(tmp_path / "scalars_rt.csv")
        csv_db.export_csv("Items", "", csv_path)

        db2 = Database.from_schema(str(tmp_path / "csv2.db"), str(csv_export_schema_path))
        try:
            db2.import_csv("Items", "", csv_path)

            names = db2.read_scalar_strings("Items", "name")
            assert sorted(names) == ["Alpha", "Beta"]

            statuses = db2.read_scalar_integers("Items", "status")
            assert sorted(statuses) == [1, 2]

            prices = db2.read_scalar_floats("Items", "price")
            assert sorted(prices) == pytest.approx([9.99, 19.5])

            notes = db2.read_scalar_strings("Items", "notes")
            assert sorted(notes) == ["first", "second"]
        finally:
            db2.close()


class TestImportCSVGroupRoundTrip:
    """Test group (vector) CSV import round-trip: export -> import -> verify."""

    def test_group_round_trip(self, csv_db: Database, tmp_path, csv_export_schema_path):
        """Export vector group, import into fresh DB, read back matches original."""
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

        csv_path = str(tmp_path / "group_rt.csv")
        csv_db.export_csv("Items", "measurements", csv_path)

        db2 = Database.from_schema(str(tmp_path / "csv2.db"), str(csv_export_schema_path))
        try:
            # Parent elements must exist before importing vector group data
            e1b = Element()
            e1b.set("label", "Item1")
            e1b.set("name", "Alpha")
            db2.create_element("Items", e1b)

            e2b = Element()
            e2b.set("label", "Item2")
            e2b.set("name", "Beta")
            db2.create_element("Items", e2b)

            db2.import_csv("Items", "measurements", csv_path)

            v1 = db2.read_vector_floats_by_id("Items", "measurement", 1)
            assert v1 == pytest.approx([1.1, 2.2, 3.3])

            v2 = db2.read_vector_floats_by_id("Items", "measurement", 2)
            assert v2 == pytest.approx([4.4, 5.5])
        finally:
            db2.close()


class TestImportCSVEnumResolution:
    """Test CSV import with enum label resolution."""

    def test_enum_labels_resolve_on_import(self, csv_db: Database, tmp_path):
        """CSV with string enum labels imports as integer values."""
        csv_path = str(tmp_path / "enum_import.csv")
        with open(csv_path, "w", newline="") as f:
            f.write("sep=,\n")
            f.write("label,name,status,price,date_created,notes\n")
            f.write("EnumItem,Alpha,Active,,,\n")

        opts = CSVOptions(
            enum_labels={"status": {"en": {"Active": 1, "Inactive": 2}}},
        )
        csv_db.import_csv("Items", "", csv_path, options=opts)

        status = csv_db.read_scalar_integer_by_id("Items", "status", 1)
        assert status == 1


class TestCSVOptionsDefaults:
    """Test CSVOptions dataclass defaults."""

    def test_default_options(self):
        """Default options have empty date_time_format and no enum labels."""
        opts = CSVOptions()
        assert opts.date_time_format == ""
        assert opts.enum_labels == {}

    def test_custom_options(self):
        """Options can be constructed with custom values."""
        opts = CSVOptions(
            date_time_format="%Y",
            enum_labels={"status": {"en": {"Active": 1}}},
        )
        assert opts.date_time_format == "%Y"
        assert opts.enum_labels == {"status": {"en": {"Active": 1}}}


# -- Helper -------------------------------------------------------------------


def _read_file(path: str) -> str:
    """Read file as string with binary mode to preserve LF."""
    with open(path, "rb") as f:
        return f.read().decode("utf-8")
