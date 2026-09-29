import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMAS_DIR = join(__dirname, "..", "..", "..", "tests", "schemas", "valid");
const SCHEMA_PATH = join(SCHEMAS_DIR, "all_types.sql");
const MULTI_COLUMN_SCHEMA_PATH = join(SCHEMAS_DIR, "multi_column_groups.sql");

describe("readSetIntegers / readSetFloats / readSetStrings", () => {
  test("reads integer sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", code: [30, 40] });
      const values = db.readSetIntegers("AllTypes", "code");
      expect(values.length).toEqual(2);
      expect(values[0].sort()).toEqual([10, 20]);
      expect(values[1].sort()).toEqual([30, 40]);
    } finally {
      db.close();
    }
  });

  test("reads float sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", weight: [1.5, 2.5] });
      db.createElement("AllTypes", { label: "Item2", weight: [3.5] });
      const values = db.readSetFloats("AllTypes", "weight");
      expect(values.length).toEqual(2);
      expect(values[0].sort()).toEqual([1.5, 2.5]);
      expect(values[1]).toEqual([3.5]);
    } finally {
      db.close();
    }
  });

  test("reads string sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", tag: ["a", "b"] });
      db.createElement("AllTypes", { label: "Item2", tag: ["c"] });
      const values = db.readSetStrings("AllTypes", "tag");
      expect(values.length).toEqual(2);
      expect(values[0].sort()).toEqual(["a", "b"]);
      expect(values[1]).toEqual(["c"]);
    } finally {
      db.close();
    }
  });

  test("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readSetIntegers("AllTypes", "code");
      expect(values).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("returns one entry per element, aligned with readElementIds across an empty element", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", tag: ["a", "b"] });
      db.createElement("AllTypes", { label: "Item2" }); // no set rows
      db.createElement("AllTypes", { label: "Item3", tag: ["c"] });

      const ids = db.readElementIds("AllTypes");
      const values = db.readSetStrings("AllTypes", "tag");

      expect(values.length).toEqual(ids.length);
      // One entry per element: the element with no rows is an empty list, not a gap
      expect(values).toEqual([["a", "b"], [], ["c"]]);
    } finally {
      db.close();
    }
  });
});

describe("readSetIntegersById / readSetFloatsById / readSetStringsById", () => {
  test("reads integer set by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20, 30] });
      const values = db.readSetIntegersById("AllTypes", "code", id);
      expect(values.sort()).toEqual([10, 20, 30]);
    } finally {
      db.close();
    }
  });

  test("reads float set by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", weight: [1.5, 2.5] });
      const values = db.readSetFloatsById("AllTypes", "weight", id);
      expect(values.sort()).toEqual([1.5, 2.5]);
    } finally {
      db.close();
    }
  });

  test("reads string set by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", tag: ["x", "y"] });
      const values = db.readSetStringsById("AllTypes", "tag", id);
      expect(values.sort()).toEqual(["x", "y"]);
    } finally {
      db.close();
    }
  });

  test("returns empty array for element with no set data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const values = db.readSetIntegersById("AllTypes", "code", id);
      expect(values).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("pairs two per-column reads of one set group by row", () => {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item1" });
      // Unsorted in both columns on purpose: a value-ordered reader would pair the wrong rows
      db.updateSetGroup("Items", "codes", id, {
        code: ["zeta", "alpha", "mu"],
        weight: [2.5, 3.5, 1.5],
      });

      const codes = db.readSetStringsById("Items", "code", id);
      const weights = db.readSetFloatsById("Items", "weight", id);

      expect(codes.length).toEqual(3);
      expect(weights.length).toEqual(codes.length);
      const pairs = codes.map((code, i) => [code, weights[i]]).toSorted();
      expect(pairs).toEqual([
        ["alpha", 3.5],
        ["mu", 1.5],
        ["zeta", 2.5],
      ]);
    } finally {
      db.close();
    }
  });
});

describe("set NULL cells", () => {
  test("keeps NULL cells positionally, and no rows is not the same as a NULL cell", () => {
    const db = Database.fromSchema(":memory:", join(SCHEMAS_DIR, "collections.sql"));
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Collection", { label: "Item 1" });
      db.createElement("Collection", { label: "Item 2" }); // no set rows
      // createElement keeps a non-null array write surface, so the NULL cell goes in
      // through the group writer.
      db.updateSetGroup("Collection", "tags", id, { tag: ["a", null, "c"] });

      // Set order is unspecified: pin the agreement between the readers and the content.
      const byId = db.readSetStringsById("Collection", "tag", id);
      expect(db.readSetStrings("Collection", "tag")).toEqual([byId, []]);
      expect(byId).toHaveLength(3);
      expect(byId).toEqual(expect.arrayContaining(["a", null, "c"]));
    } finally {
      db.close();
    }
  });

  test("keeps NULL cells through the bulk float reader", () => {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item1" });
      db.updateSetGroup("Items", "codes", id, { code: ["a", "b"], weight: [1.5, null] });

      const [weights] = db.readSetFloats("Items", "weight");
      expect(weights).toHaveLength(2);
      expect(weights).toEqual(expect.arrayContaining([1.5, null]));
    } finally {
      db.close();
    }
  });
});

describe("readSetGroupById", () => {
  test("returns every row with NULL cells in place", () => {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item1" });
      db.updateSetGroup("Items", "codes", id, {
        code: ["alpha", null, "mu"],
        weight: [1.5, 2.5, null],
      });

      const rows = db.readSetGroupById("Items", "codes", id);
      // A set's row order is unspecified: check membership, not positions.
      expect(rows).toHaveLength(3);
      expect(rows).toContainEqual({ code: "alpha", weight: 1.5 });
      expect(rows).toContainEqual({ code: null, weight: 2.5 });
      expect(rows).toContainEqual({ code: "mu", weight: null });
    } finally {
      db.close();
    }
  });

  test("returns [] for an element with no rows", () => {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item1" });
      expect(db.readSetGroupById("Items", "codes", id)).toEqual([]);
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
      // mentors and sponsors share parent_ref, and a per-column read of that name resolves to mentors.
      db.updateSetGroup("Child", "mentors", child, { parent_ref: [parentA] });
      db.updateSetGroup("Child", "sponsors", child, {
        parent_ref: [parentB, parentB],
        tier: [1, 2],
      });

      const rows = db.readSetGroupById("Child", "sponsors", child);
      expect(rows).toHaveLength(2);
      expect(rows).toContainEqual({ parent_ref: parentB, tier: 1 });
      expect(rows).toContainEqual({ parent_ref: parentB, tier: 2 });
    } finally {
      db.close();
    }
  });
});
