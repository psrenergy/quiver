import { describe, expect, test } from "bun:test";
import { join } from "node:path";
import { Database, QuiverError } from "../src/index";

const SCHEMA_PATH = join(
  import.meta.dir,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "all_types.sql",
);

describe("readScalarIntegers / readScalarFloats / readScalarStrings", () => {
  test("reads integer scalars from collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.createElement("AllTypes", { label: "Item2", some_integer: 99 });
      const values = db.readScalarIntegers("AllTypes", "some_integer");
      expect(values).toEqual([42, 99]);
    } finally {
      db.close();
    }
  });

  test("reads float scalars from collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      db.createElement("AllTypes", { label: "Item2", some_float: 2.71 });
      const values = db.readScalarFloats("AllTypes", "some_float");
      expect(values).toEqual([3.14, 2.71]);
    } finally {
      db.close();
    }
  });

  test("reads string scalars from collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      db.createElement("AllTypes", { label: "Item2", some_text: "world" });
      const values = db.readScalarStrings("AllTypes", "some_text");
      expect(values).toEqual(["hello", "world"]);
    } finally {
      db.close();
    }
  });

  test("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readScalarIntegers("AllTypes", "some_integer");
      expect(values).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => {
      db.readScalarIntegers("AllTypes", "some_integer");
    }).toThrow(QuiverError);

    try {
      db.readScalarIntegers("AllTypes", "some_integer");
    } catch (e) {
      expect((e as QuiverError).message).toBe("Database is closed");
    }
  });
});

describe("readScalarIntegerById / readScalarFloatById / readScalarStringById", () => {
  test("reads integer by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toBe(42);
    } finally {
      db.close();
    }
  });

  test("returns null for non-existent id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const value = db.readScalarIntegerById("AllTypes", "some_integer", 9999);
      expect(value).toBeNull();
    } finally {
      db.close();
    }
  });

  test("returns null for null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toBeNull();
    } finally {
      db.close();
    }
  });

  test("reads float by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const value = db.readScalarFloatById("AllTypes", "some_float", id);
      expect(value).toBe(3.14);
    } finally {
      db.close();
    }
  });

  test("reads string by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      const value = db.readScalarStringById("AllTypes", "some_text", id);
      expect(value).toBe("hello");
    } finally {
      db.close();
    }
  });

  test("returns null for non-existent string id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const value = db.readScalarStringById("AllTypes", "some_text", 9999);
      expect(value).toBeNull();
    } finally {
      db.close();
    }
  });
});

describe("readElementIds", () => {
  test("reads all element ids", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id1 = db.createElement("AllTypes", { label: "Item1" });
      const id2 = db.createElement("AllTypes", { label: "Item2" });
      const ids = db.readElementIds("AllTypes");
      expect(ids).toHaveLength(2);
      expect(ids).toContain(id1);
      expect(ids).toContain(id2);
    } finally {
      db.close();
    }
  });

  test("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const ids = db.readElementIds("AllTypes");
      expect(ids).toEqual([]);
    } finally {
      db.close();
    }
  });
});
