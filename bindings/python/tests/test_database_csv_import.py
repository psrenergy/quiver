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


class TestImportCSVLabelOnThirdColumn:
    """Test that label column works when not in the first position."""

    def test_label_on_third_column(self, csv_db: Database, tmp_path):
        """CSV with label as 3rd column imports correctly."""
        csv_path = str(tmp_path / "label_col3.csv")
        with open(csv_path, "w", newline="") as f:
            f.write("sep=,\n")
            f.write("name,status,label,price,date_created,notes\n")
            f.write("Alpha,1,Item1,10.5,,\n")
            f.write("Beta,2,Item2,20.0,,\n")

        csv_db.import_csv("Items", "", csv_path)

        labels = csv_db.read_scalar_strings("Items", "label")
        assert labels == ["Item1", "Item2"]

        names = csv_db.read_scalar_strings("Items", "name")
        assert names == ["Alpha", "Beta"]

        statuses = csv_db.read_scalar_integers("Items", "status")
        assert statuses == [1, 2]

        prices = csv_db.read_scalar_floats("Items", "price")
        assert prices == pytest.approx([10.5, 20.0])


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


class TestImportCSVSelfReferenceFKReImport:
    """Test CSV import with self-referencing FK re-import."""

    def test_self_reference_fk_reimport(self, relations_db: Database, tmp_path):
        """Import children with self-FK, then re-import with self-referencing rows."""
        relations_db.create_element("Parent", label="Parent1")

        # First import: 2 children, one with self-FK
        csv_path = str(tmp_path / "self_fk_reimport.csv")
        with open(csv_path, "w", newline="") as f:
            f.write("sep=,\n")
            f.write("label,parent_id,sibling_id\n")
            f.write("Child1,Parent1,\n")
            f.write("Child2,Parent1,Child1\n")

        relations_db.import_csv("Child", "", csv_path)

        sib1 = relations_db.query_integer("SELECT sibling_id FROM Child WHERE label = ?", params=["Child1"])
        assert sib1 is None

        sib2 = relations_db.query_integer("SELECT sibling_id FROM Child WHERE label = ?", params=["Child2"])
        child1_id = relations_db.query_integer("SELECT id FROM Child WHERE label = ?", params=["Child1"])
        assert sib2 == child1_id

        # Second import (re-import): 4 children, includes self-referencing row
        with open(csv_path, "w", newline="") as f:
            f.write("sep=,\n")
            f.write("label,parent_id,sibling_id\n")
            f.write("Child1,Parent1,\n")
            f.write("Child2,Parent1,Child1\n")
            f.write("Child3,Parent1,Child3\n")
            f.write("Child4,Parent1,Child3\n")

        relations_db.import_csv("Child", "", csv_path)

        labels = relations_db.read_scalar_strings("Child", "label")
        assert len(labels) == 4

        child3_id = relations_db.query_integer("SELECT id FROM Child WHERE label = ?", params=["Child3"])
        sib3 = relations_db.query_integer("SELECT sibling_id FROM Child WHERE label = ?", params=["Child3"])
        assert sib3 == child3_id

        sib4 = relations_db.query_integer("SELECT sibling_id FROM Child WHERE label = ?", params=["Child4"])
        assert sib4 == child3_id


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
