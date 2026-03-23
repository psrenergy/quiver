import { describe, expect, test } from "bun:test";
import { join } from "node:path";
import { Database } from "../src/index";

const BASIC_SCHEMA = join(
  import.meta.dir,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "basic.sql",
);

const COMPOSITE_SCHEMA = join(
  import.meta.dir,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "composite_helpers.sql",
);

describe("readScalarsById", () => {
  test("returns all scalar attributes for an element", () => {
    const db = Database.fromSchema(":memory:", BASIC_SCHEMA);
    try {
      db.createElement("Configuration", {
        label: "cfg1",
        integer_attribute: 42,
        float_attribute: 3.14,
        string_attribute: "hello",
        date_attribute: "2024-01-15",
      });

      const scalars = db.readScalarsById("Configuration", 1);
      expect(scalars.integer_attribute).toBe(42);
      expect(scalars.float_attribute).toBeCloseTo(3.14);
      expect(scalars.string_attribute).toBe("hello");
      expect(scalars.date_attribute).toBe("2024-01-15");
    } finally {
      db.close();
    }
  });

  test("returns null for nullable unset attributes", () => {
    const db = Database.fromSchema(":memory:", BASIC_SCHEMA);
    try {
      db.createElement("Configuration", { label: "cfg1" });

      const scalars = db.readScalarsById("Configuration", 1);
      expect(scalars.float_attribute).toBeNull();
      expect(scalars.string_attribute).toBeNull();
      expect(scalars.date_attribute).toBeNull();
    } finally {
      db.close();
    }
  });

  test("includes label and id in result", () => {
    const db = Database.fromSchema(":memory:", BASIC_SCHEMA);
    try {
      db.createElement("Configuration", { label: "cfg1", integer_attribute: 10 });

      const scalars = db.readScalarsById("Configuration", 1);
      expect(scalars.label).toBe("cfg1");
      expect(scalars.id).toBe(1);
    } finally {
      db.close();
    }
  });
});

describe("readVectorsById", () => {
  test("returns all vector columns for an element", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", {
        label: "item1",
        amount: [10, 20, 30],
        score: [1.1, 2.2],
        note: ["hello", "world"],
      });

      const vectors = db.readVectorsById("Items", 1);
      expect(vectors.amount).toEqual([10, 20, 30]);
      expect(vectors.score).toEqual([1.1, 2.2]);
      expect(vectors.note).toEqual(["hello", "world"]);
    } finally {
      db.close();
    }
  });

  test("returns empty arrays for element with no vector data", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", { label: "empty_item" });

      const vectors = db.readVectorsById("Items", 1);
      expect(vectors.amount).toEqual([]);
      expect(vectors.score).toEqual([]);
      expect(vectors.note).toEqual([]);
    } finally {
      db.close();
    }
  });
});

describe("readSetsById", () => {
  test("returns all set columns for an element", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", {
        label: "item1",
        code: [1, 2, 3],
        weight: [1.1, 2.2],
        tag: ["a", "b"],
      });

      const sets = db.readSetsById("Items", 1);
      expect(sets.code).toEqual([1, 2, 3]);
      expect(sets.weight).toEqual([1.1, 2.2]);
      expect(sets.tag).toEqual(["a", "b"]);
    } finally {
      db.close();
    }
  });

  test("returns empty arrays for element with no set data", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", { label: "empty_item" });

      const sets = db.readSetsById("Items", 1);
      expect(sets.code).toEqual([]);
      expect(sets.weight).toEqual([]);
      expect(sets.tag).toEqual([]);
    } finally {
      db.close();
    }
  });
});
