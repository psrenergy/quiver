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
});
