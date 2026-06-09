import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("updateElement", () => {
  test("updates integer scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.updateElement("AllTypes", id, { some_integer: 99 });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toEqual(99);
    } finally {
      db.close();
    }
  });

  test("updates float scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      db.updateElement("AllTypes", id, { some_float: 2.71 });
      const value = db.readScalarFloatById("AllTypes", "some_float", id);
      expect(value).toEqual(2.71);
    } finally {
      db.close();
    }
  });

  test("updates string scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      db.updateElement("AllTypes", id, { some_text: "world" });
      const value = db.readScalarStringById("AllTypes", "some_text", id);
      expect(value).toEqual("world");
    } finally {
      db.close();
    }
  });

  test("updates with null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.updateElement("AllTypes", id, { some_integer: null });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("updates array field", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.updateElement("AllTypes", id, { code: [30, 40, 50] });
      const count = db.queryInteger("SELECT COUNT(*) FROM AllTypes_set_codes WHERE id = ?", [id]);
      expect(count).toEqual(3);
      const minVal = db.queryInteger("SELECT MIN(code) FROM AllTypes_set_codes WHERE id = ?", [id]);
      expect(minVal).toEqual(30);
      const maxVal = db.queryInteger("SELECT MAX(code) FROM AllTypes_set_codes WHERE id = ?", [id]);
      expect(maxVal).toEqual(50);
    } finally {
      db.close();
    }
  });

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => {
      db.updateElement("AllTypes", 1, { some_integer: 42 });
    }).toThrow(QuiverError);

    try {
      db.updateElement("AllTypes", 1, { some_integer: 42 });
    } catch (e) {
      expect((e as QuiverError).message).toContain("Database is closed");
    }
  });
});
