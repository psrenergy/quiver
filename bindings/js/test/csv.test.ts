import { describe, expect, test } from "bun:test";
import { existsSync, readFileSync, unlinkSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Database, QuiverError } from "../src/index";
import type { CsvOptions } from "../src/index";

const SCHEMA_PATH = join(
  import.meta.dir,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "csv_export.sql",
);

function tempCsv(name: string): string {
  return join(tmpdir(), `quiver_js_csv_test_${name}_${Date.now()}.csv`);
}

function cleanup(...paths: string[]): void {
  for (const p of paths) {
    try {
      if (existsSync(p)) unlinkSync(p);
    } catch {
      // Ignore cleanup errors
    }
  }
}

// ============================================================================
// Export tests (JSCSV-01)
// ============================================================================

describe("CSV export", () => {
  test("exports scalar collection to CSV with header and data rows", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("scalar_export");
    try {
      db.createElement("Items", {
        label: "Item1",
        name: "Alpha",
        status: 1,
        price: 9.99,
        date_created: "2024-01-15T10:30:00",
        notes: "first",
      });
      db.createElement("Items", {
        label: "Item2",
        name: "Beta",
        status: 2,
        price: 19.5,
        date_created: "2024-02-20T08:00:00",
        notes: "second",
      });

      db.exportCsv("Items", "", csvPath);

      expect(existsSync(csvPath)).toBe(true);
      const content = readFileSync(csvPath, "utf-8");

      // Header row
      expect(content).toContain("label,name,status,price,date_created,notes\n");

      // Data rows
      expect(content).toContain("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n");
      expect(content).toContain("Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  test("exports vector group to CSV", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("vector_export");
    try {
      const id = db.createElement("Items", { label: "Item1", name: "Alpha" });
      db.updateElement("Items", id, { measurement: [1.1, 2.2, 3.3] });

      db.exportCsv("Items", "measurements", csvPath);

      const content = readFileSync(csvPath, "utf-8");

      // Header with sep= prefix
      expect(content).toContain("sep=,\nid,vector_index,measurement\n");

      // Data rows
      expect(content).toContain("Item1,1,1.1\n");
      expect(content).toContain("Item1,2,2.2\n");
      expect(content).toContain("Item1,3,3.3\n");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  test("exports empty collection as header-only CSV", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("empty_export");
    try {
      db.exportCsv("Items", "", csvPath);

      const content = readFileSync(csvPath, "utf-8");

      // Header row with sep= prefix and columns
      expect(content).toBe("sep=,\nlabel,name,status,price,date_created,notes\n");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  test("export with invalid group returns error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("invalid_group");
    try {
      expect(() => db.exportCsv("Items", "nonexistent", csvPath)).toThrow(QuiverError);
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });
});

// ============================================================================
// Import tests (JSCSV-02)
// ============================================================================

describe("CSV import", () => {
  test("import scalar CSV round-trip", () => {
    const csvPath = tempCsv("scalar_roundtrip");
    const dbPath1 = tempCsv("scalar_rt_db1") + ".db";
    const dbPath2 = tempCsv("scalar_rt_db2") + ".db";
    try {
      // Create DB1, populate, export
      const db1 = Database.fromSchema(dbPath1, SCHEMA_PATH);
      db1.createElement("Items", {
        label: "Item1",
        name: "Alpha",
        status: 1,
        price: 9.99,
        date_created: "2024-01-15T10:30:00",
        notes: "first",
      });
      db1.createElement("Items", {
        label: "Item2",
        name: "Beta",
        status: 2,
        price: 19.5,
        date_created: "2024-02-20T08:00:00",
        notes: "second",
      });
      db1.exportCsv("Items", "", csvPath);
      db1.close();

      // Create DB2, import, verify
      const db2 = Database.fromSchema(dbPath2, SCHEMA_PATH);
      db2.importCsv("Items", "", csvPath);

      const labels = db2.readScalarStrings("Items", "label");
      expect(labels.sort()).toEqual(["Item1", "Item2"]);

      const names = db2.readScalarStrings("Items", "name");
      expect(names.sort()).toEqual(["Alpha", "Beta"]);

      const statuses = db2.readScalarIntegers("Items", "status");
      expect(statuses.sort()).toEqual([1, 2]);

      db2.close();
    } finally {
      cleanup(csvPath, dbPath1, dbPath2);
    }
  });

  test("import vector CSV round-trip", () => {
    const csvPath = tempCsv("vector_roundtrip");
    const dbPath1 = tempCsv("vector_rt_db1") + ".db";
    const dbPath2 = tempCsv("vector_rt_db2") + ".db";
    try {
      // Create DB1, populate vector, export
      const db1 = Database.fromSchema(dbPath1, SCHEMA_PATH);
      const id1 = db1.createElement("Items", { label: "Item1", name: "Alpha" });
      db1.updateElement("Items", id1, { measurement: [1.1, 2.2, 3.3] });
      db1.exportCsv("Items", "measurements", csvPath);
      db1.close();

      // Create DB2 with same element (label needed for FK), import
      const db2 = Database.fromSchema(dbPath2, SCHEMA_PATH);
      db2.createElement("Items", { label: "Item1", name: "Placeholder" });
      db2.importCsv("Items", "measurements", csvPath);

      // Verify vector data was imported
      const values = db2.readVectorFloatsById("Items", "measurement", 1);
      expect(values).toEqual([1.1, 2.2, 3.3]);

      db2.close();
    } finally {
      cleanup(csvPath, dbPath1, dbPath2);
    }
  });

  test("import set CSV round-trip", () => {
    const csvPath = tempCsv("set_roundtrip");
    const dbPath1 = tempCsv("set_rt_db1") + ".db";
    const dbPath2 = tempCsv("set_rt_db2") + ".db";
    try {
      // Create DB1, populate set, export
      const db1 = Database.fromSchema(dbPath1, SCHEMA_PATH);
      const id1 = db1.createElement("Items", { label: "Item1", name: "Alpha" });
      db1.updateElement("Items", id1, { tag: ["red", "green", "blue"] });
      db1.exportCsv("Items", "tags", csvPath);
      db1.close();

      // Create DB2, import
      const db2 = Database.fromSchema(dbPath2, SCHEMA_PATH);
      db2.createElement("Items", { label: "Item1", name: "Placeholder" });
      db2.importCsv("Items", "tags", csvPath);

      // Verify set data was imported
      const tags = db2.readSetStringsById("Items", "tag", 1);
      expect(tags.sort()).toEqual(["blue", "green", "red"]);

      db2.close();
    } finally {
      cleanup(csvPath, dbPath1, dbPath2);
    }
  });
});

// ============================================================================
// Options tests
// ============================================================================

describe("CSV options", () => {
  test("export with enum labels replaces integers with labels", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("enum_labels");
    try {
      db.createElement("Items", { label: "Item1", name: "Alpha", status: 1 });
      db.createElement("Items", { label: "Item2", name: "Beta", status: 2 });

      const options: CsvOptions = {
        enumLabels: {
          status: {
            en: {
              Active: 1,
              Inactive: 2,
            },
          },
        },
      };

      db.exportCsv("Items", "", csvPath, options);

      const content = readFileSync(csvPath, "utf-8");

      // status column should have labels instead of integers
      expect(content).toContain("Item1,Alpha,Active,");
      expect(content).toContain("Item2,Beta,Inactive,");

      // Raw integers should NOT be present as status values
      expect(content).not.toContain("Item1,Alpha,1,");
      expect(content).not.toContain("Item2,Beta,2,");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  test("export with dateTimeFormat formats date columns", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("date_format");
    try {
      db.createElement("Items", {
        label: "Item1",
        name: "Alpha",
        status: 1,
        date_created: "2024-01-15T10:30:00",
      });

      const options: CsvOptions = {
        dateTimeFormat: "%Y/%m/%d",
      };

      db.exportCsv("Items", "", csvPath, options);

      const content = readFileSync(csvPath, "utf-8");

      // Formatted date should appear
      expect(content).toContain("2024/01/15");
      // Raw ISO format should NOT appear
      expect(content).not.toContain("2024-01-15T10:30:00");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });
});
