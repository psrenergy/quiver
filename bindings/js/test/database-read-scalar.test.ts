import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

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

  test("preserves a slot for NULL float values", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      // some_float has no default, so an unset value is SQL NULL.
      db.createElement("AllTypes", { label: "Item1", some_float: 10 });
      db.createElement("AllTypes", { label: "Item2" }); // some_float is NULL
      db.createElement("AllTypes", { label: "Item3", some_float: 30 });
      db.createElement("AllTypes", { label: "Item4", some_float: 40 });
      const values = db.readScalarFloats("AllTypes", "some_float");
      // One entry per element; the NULL must occupy a slot (null), not be dropped.
      expect(values).toEqual([10, null, 30, 40]);
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
    let threw = false;
    try {
      db.readScalarIntegers("AllTypes", "some_integer");
    } catch (e) {
      threw = true;
      expect(e instanceof QuiverError).toBeTruthy();
      expect((e as QuiverError).message).toEqual("Database is closed");
    }
    expect(threw).toBeTruthy();
  });
});

describe("readScalarIntegerById / readScalarFloatById / readScalarStringById", () => {
  test("reads integer by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toEqual(42);
    } finally {
      db.close();
    }
  });

  test("returns null for non-existent id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const value = db.readScalarIntegerById("AllTypes", "some_integer", 9999);
      expect(value).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("returns null for null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("reads float by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const value = db.readScalarFloatById("AllTypes", "some_float", id);
      expect(value).toEqual(3.14);
    } finally {
      db.close();
    }
  });

  test("reads string by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      const value = db.readScalarStringById("AllTypes", "some_text", id);
      expect(value).toEqual("hello");
    } finally {
      db.close();
    }
  });

  test("returns null for non-existent string id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const value = db.readScalarStringById("AllTypes", "some_text", 9999);
      expect(value).toEqual(null);
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
      expect(ids.length).toEqual(2);
      expect(ids.includes(id1)).toBeTruthy();
      expect(ids.includes(id2)).toBeTruthy();
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
