import {
  assertEquals,
  assertFalse,
  assertStringIncludes,
  assertThrows,
} from "jsr:@std/assert";
const __dirname = import.meta.dirname!;
import { join } from "jsr:@std/path";
import { Database, QuiverError } from "../src/index.ts";
import type { CsvOptions } from "../src/index.ts";

const SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "csv_export.sql",
);

function tempCsv(name: string): string {
  return join(Deno.makeTempDirSync(), `${name}.csv`);
}

function cleanup(...paths: string[]): void {
  for (const p of paths) {
    try {
      Deno.removeSync(p);
    } catch {
      // Ignore cleanup errors
    }
  }
}

// ============================================================================
// Export tests (JSCSV-01)
// ============================================================================

Deno.test({ name: "CSV export", sanitizeResources: false }, async (t) => {
  await t.step("exports scalar collection to CSV with header and data rows", () => {
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

      const content = Deno.readTextFileSync(csvPath);

      // Header row
      assertStringIncludes(content, "label,name,status,price,date_created,notes\n");

      // Data rows
      assertStringIncludes(content, "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n");
      assertStringIncludes(content, "Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  await t.step("exports vector group to CSV", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("vector_export");
    try {
      const id = db.createElement("Items", { label: "Item1", name: "Alpha" });
      db.updateElement("Items", id, { measurement: [1.1, 2.2, 3.3] });

      db.exportCsv("Items", "measurements", csvPath);

      const content = Deno.readTextFileSync(csvPath);

      // Header with sep= prefix
      assertStringIncludes(content, "sep=,\nid,vector_index,measurement\n");

      // Data rows
      assertStringIncludes(content, "Item1,1,1.1\n");
      assertStringIncludes(content, "Item1,2,2.2\n");
      assertStringIncludes(content, "Item1,3,3.3\n");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  await t.step("exports empty collection as header-only CSV", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("empty_export");
    try {
      db.exportCsv("Items", "", csvPath);

      const content = Deno.readTextFileSync(csvPath);

      // Header row with sep= prefix and columns
      assertEquals(content, "sep=,\nlabel,name,status,price,date_created,notes\n");
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  await t.step("export with invalid group returns error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const csvPath = tempCsv("invalid_group");
    try {
      assertThrows(() => db.exportCsv("Items", "nonexistent", csvPath), QuiverError);
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });
});

// ============================================================================
// Import tests (JSCSV-02)
// ============================================================================

Deno.test({ name: "CSV import", sanitizeResources: false }, async (t) => {
  await t.step("import scalar CSV round-trip", () => {
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
      assertEquals(labels.sort(), ["Item1", "Item2"]);

      const names = db2.readScalarStrings("Items", "name");
      assertEquals(names.sort(), ["Alpha", "Beta"]);

      const statuses = db2.readScalarIntegers("Items", "status");
      assertEquals(statuses.sort(), [1, 2]);

      db2.close();
    } finally {
      cleanup(csvPath, dbPath1, dbPath2);
    }
  });

  await t.step("import vector CSV round-trip", () => {
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
      assertEquals(values, [1.1, 2.2, 3.3]);

      db2.close();
    } finally {
      cleanup(csvPath, dbPath1, dbPath2);
    }
  });

  await t.step("import set CSV round-trip", () => {
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
      assertEquals(tags.sort(), ["blue", "green", "red"]);

      db2.close();
    } finally {
      cleanup(csvPath, dbPath1, dbPath2);
    }
  });
});

// ============================================================================
// Options tests
// ============================================================================

Deno.test({ name: "CSV options", sanitizeResources: false }, async (t) => {
  await t.step("export with enum labels replaces integers with labels", () => {
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

      const content = Deno.readTextFileSync(csvPath);

      // status column should have labels instead of integers
      assertStringIncludes(content, "Item1,Alpha,Active,");
      assertStringIncludes(content, "Item2,Beta,Inactive,");

      // Raw integers should NOT be present as status values
      assertFalse(content.includes("Item1,Alpha,1,"));
      assertFalse(content.includes("Item2,Beta,2,"));
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });

  await t.step("export with dateTimeFormat formats date columns", () => {
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

      const content = Deno.readTextFileSync(csvPath);

      // Formatted date should appear
      assertStringIncludes(content, "2024/01/15");
      // Raw ISO format should NOT appear
      assertFalse(content.includes("2024-01-15T10:30:00"));
    } finally {
      db.close();
      cleanup(csvPath);
    }
  });
});
