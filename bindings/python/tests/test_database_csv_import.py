"""Tests for CSV import operations."""

from __future__ import annotations

import pytest

from quiverdb import CSVOptions, Database


class TestImportCSVScalarRoundTrip:
    """Test scalar CSV import round-trip: export -> import -> verify."""

    def test_scalar_round_trip(self, csv_db: Database, tmp_path, csv_export_schema_path):
        """Export scalars, import into fresh DB, read back matches original."""
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
        csv_db.create_element("Items", label="Item1", name="Alpha")
        csv_db.create_element("Items", label="Item2", name="Beta")

        csv_db.update_element("Items", 1, measurement=[1.1, 2.2, 3.3])
        csv_db.update_element("Items", 2, measurement=[4.4, 5.5])

        csv_path = str(tmp_path / "group_rt.csv")
        csv_db.export_csv("Items", "measurements", csv_path)

        db2 = Database.from_schema(str(tmp_path / "csv2.db"), str(csv_export_schema_path))
        try:
            # Parent elements must exist before importing vector group data
            db2.create_element("Items", label="Item1", name="Alpha")
            db2.create_element("Items", label="Item2", name="Beta")

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

        options = CSVOptions(
            enum_labels={"status": {"en": {"Active": 1, "Inactive": 2}}},
        )
        csv_db.import_csv("Items", "", csv_path, options=options)

        status = csv_db.read_scalar_integer_by_id("Items", "status", 1)
        assert status == 1
