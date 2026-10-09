import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");
const SCHEMAS_DIR = join(__dirname, "..", "..", "..", "tests", "schemas", "valid");

describe("readVectorIntegers / readVectorFloats / readVectorStrings", () => {
  test("reads integer vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", count_value: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", count_value: [30, 40, 50] });
      const values = db.readVectorIntegers("AllTypes", "count_value");
      expect(values).toEqual([
        [10, 20],
        [30, 40, 50],
      ]);
    } finally {
      db.close();
    }
  });

  test("reads float vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", score: [1.5, 2.5] });
      db.createElement("AllTypes", { label: "Item2", score: [3.5] });
      const values = db.readVectorFloats("AllTypes", "score");
      expect(values).toEqual([[1.5, 2.5], [3.5]]);
    } finally {
      db.close();
    }
  });

  test("reads string vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", label_value: ["a", "b"] });
      db.createElement("AllTypes", { label: "Item2", label_value: ["c"] });
      const values = db.readVectorStrings("AllTypes", "label_value");
      expect(values).toEqual([["a", "b"], ["c"]]);
    } finally {
      db.close();
    }
  });

  test("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readVectorIntegers("AllTypes", "count_value");
      expect(values).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("returns one entry per element, aligned with readElementIds across an empty element", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", count_value: [10, 20] });
      db.createElement("AllTypes", { label: "Item2" }); // no vector rows
      db.createElement("AllTypes", { label: "Item3", count_value: [30] });

      const ids = db.readElementIds("AllTypes");
      const values = db.readVectorIntegers("AllTypes", "count_value");

      expect(values.length).toEqual(ids.length);
      expect(values).toEqual([[10, 20], [], [30]]);
    } finally {
      db.close();
    }
  });
});

describe("readVectorIntegersById / readVectorFloatsById / readVectorStringsById", () => {
  test("reads integer vector by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", count_value: [10, 20, 30] });
      const values = db.readVectorIntegersById("AllTypes", "count_value", id);
      expect(values).toEqual([10, 20, 30]);
    } finally {
      db.close();
    }
  });

  test("reads float vector by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", score: [1.5, 2.5] });
      const values = db.readVectorFloatsById("AllTypes", "score", id);
      expect(values).toEqual([1.5, 2.5]);
    } finally {
      db.close();
    }
  });

  test("reads string vector by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", label_value: ["x", "y"] });
      const values = db.readVectorStringsById("AllTypes", "label_value", id);
      expect(values).toEqual(["x", "y"]);
    } finally {
      db.close();
    }
  });

  test("returns empty array for element with no vector data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const values = db.readVectorIntegersById("AllTypes", "count_value", id);
      expect(values).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("returns empty array for non-existent id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readVectorIntegersById("AllTypes", "count_value", 9999);
      expect(values).toEqual([]);
    } finally {
      db.close();
    }
  });
});

describe("vector NULL cells", () => {
  test("keeps NULL cells positionally, and no rows is not the same as a NULL cell", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "collections.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Collection", { label: "Item 1" });
      db.createElement("Collection", { label: "Item 2" }); // no vector rows
      // createElement keeps a non-null array write surface, so the NULL cell goes in
      // through the group writer.
      db.updateVectorGroup("Collection", "values", id, { value_int: [10, null, 30] });

      expect(db.readVectorIntegers("Collection", "value_int")).toEqual([[10, null, 30], []]);
      expect(db.readVectorIntegersById("Collection", "value_int", id)).toEqual([10, null, 30]);
    } finally {
      db.close();
    }
  });

  test("surfaces NULL cells through the boolean wrapper", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "collections.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Collection", { label: "Item 1" });
      db.updateVectorGroup("Collection", "values", id, { value_int: [1, null, 0] });

      expect(db.readVectorBooleans("Collection", "value_int")).toEqual([[true, null, false]]);
      expect(db.readVectorBooleansById("Collection", "value_int", id)).toEqual([true, null, false]);
    } finally {
      db.close();
    }
  });

  test("refuses a null cell written back through updateElement instead of storing 0", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "collections.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const a = db.createElement("Collection", { label: "Item 1" });
      const b = db.createElement("Collection", { label: "Item 2", value_float: [7.5] });
      db.updateVectorGroup("Collection", "values", a, { value_float: [1.5, null, 2.5] });

      const read = db.readVectorFloatsById("Collection", "value_float", a) as number[];
      expect(() => db.updateElement("Collection", b, { value_float: read })).toThrow(
        "Unsupported null cell in array 'value_float'",
      );
      expect(db.readVectorFloatsById("Collection", "value_float", b)).toEqual([7.5]);
    } finally {
      db.close();
    }
  });

  test("keeps NULL cells through the bulk float reader", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "collections.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Collection", { label: "Item 1" });
      db.updateVectorGroup("Collection", "values", id, {
        value_int: [1, 2],
        value_float: [null, 2.5],
      });

      expect(db.readVectorFloats("Collection", "value_float")).toEqual([[null, 2.5]]);
      expect(db.readVectorFloatsById("Collection", "value_float", id)).toEqual([null, 2.5]);
    } finally {
      db.close();
    }
  });

  test("refuses a null cell in a string array whatever its position", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "multi_column_groups.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item 1", note: ["kept"] });
      const late = ["a", null] as unknown as string[];
      const early = [null, "a"] as unknown as string[];
      const sparse: string[] = [];
      sparse[0] = "a";
      sparse[2] = "c";
      expect(() => db.updateElement("Items", id, { note: late })).toThrow(
        "Unsupported null cell in array 'note'",
      );
      expect(() => db.updateElement("Items", id, { note: early })).toThrow(
        "Unsupported null cell in array 'note'",
      );
      expect(() => db.updateElement("Items", id, { note: sparse })).toThrow(
        "Unsupported null cell in array 'note'",
      );
      expect(db.readVectorStringsById("Items", "note", id)).toEqual(["kept"]);
    } finally {
      db.close();
    }
  });
});

describe("readVectorGroupById", () => {
  function openItem(): { db: Database; id: number } {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "multi_column_groups.sql"));
    db.createElement("Configuration", { label: "Config" });
    const id = db.createElement("Items", { label: "Item1" });
    return { db, id };
  }

  test("returns one row per vector_index with NULL cells in place", () => {
    const { db, id } = openItem();
    try {
      db.updateVectorGroup("Items", "readings", id, { amount: [1.5, 2.5], score: [null, 20.5] });
      expect(db.readVectorGroupById("Items", "readings", id)).toEqual([
        { amount: 1.5, score: null },
        { amount: 2.5, score: 20.5 },
      ]);

      db.updateVectorGroup("Items", "readings", id, { amount: [null, 2.5], score: [10.5, 20.5] });
      expect(db.readVectorGroupById("Items", "readings", id)).toEqual([
        { amount: null, score: 10.5 },
        { amount: 2.5, score: 20.5 },
      ]);
    } finally {
      db.close();
    }
  });

  test("keeps DATE_TIME cells as strings and NULL strings as null", () => {
    const { db, id } = openItem();
    try {
      db.updateVectorGroup("Items", "events", id, {
        date_event: ["2024-01-15T10:30:00", null, "2024-03-01"],
        note: [null, "second", "third"],
      });
      expect(db.readVectorGroupById("Items", "events", id)).toEqual([
        { date_event: "2024-01-15T10:30:00", note: null },
        { date_event: null, note: "second" },
        { date_event: "2024-03-01", note: "third" },
      ]);
    } finally {
      db.close();
    }
  });

  test("returns [] for an element with no rows", () => {
    const { db, id } = openItem();
    try {
      expect(db.readVectorGroupById("Items", "readings", id)).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("throws on an unknown group", () => {
    const { db, id } = openItem();
    try {
      expect(() => db.readVectorGroupById("Items", "nope", id)).toThrow(/Vector group not found/);
    } finally {
      db.close();
    }
  });

  test("reads its own table when another group shares a column name", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "shared_group_columns.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const parentA = db.createElement("Parent", { label: "Parent A" });
      const parentB = db.createElement("Parent", { label: "Parent B" });
      const child = db.createElement("Child", { label: "Child 1" });
      // links and routes share parent_ref, and a per-column read of that name resolves to links.
      db.updateVectorGroup("Child", "links", child, { parent_ref: [parentA] });
      db.updateVectorGroup("Child", "routes", child, {
        parent_ref: [parentB, parentB],
        cost: [1.5, 2.5],
      });

      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentA]);
      expect(db.readVectorGroupById("Child", "routes", child)).toEqual([
        { parent_ref: parentB, cost: 1.5 },
        { parent_ref: parentB, cost: 2.5 },
      ]);
    } finally {
      db.close();
    }
  });
});
