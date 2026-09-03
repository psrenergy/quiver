import { describe, expect, test } from "bun:test";
import { join } from "node:path";

import { Database } from "../src/index.ts";

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

describe("boolean convenience methods", () => {
  test("reads scalar, vector, set, and query booleans", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.readScalarBooleans("AllTypes", "some_integer")).toEqual([]);
      expect(db.readVectorBooleans("AllTypes", "count_value")).toEqual([]);
      expect(db.readSetBooleans("AllTypes", "code")).toEqual([]);

      const idFalse = db.createElement("AllTypes", {
        label: "False",
        some_integer: 0,
        count_value: [0, 1],
        code: [0, 1],
      });
      const idTrue = db.createElement("AllTypes", {
        label: "True",
        some_integer: 1,
        count_value: [1, 0],
        code: [1],
      });
      const idNull = db.createElement("AllTypes", { label: "Null" });

      expect(db.readScalarBooleans("AllTypes", "some_integer")).toEqual([false, true, null]);
      expect(db.readScalarBooleanById("AllTypes", "some_integer", idFalse)).toBeFalse();
      expect(db.readScalarBooleanById("AllTypes", "some_integer", idTrue)).toBeTrue();
      expect(db.readScalarBooleanById("AllTypes", "some_integer", idNull)).toBeNull();

      expect(db.readVectorBooleans("AllTypes", "count_value")).toEqual([
        [false, true],
        [true, false],
      ]);
      expect(db.readVectorBooleansById("AllTypes", "count_value", idFalse)).toEqual([false, true]);
      expect(db.readVectorBooleansById("AllTypes", "count_value", idNull)).toEqual([]);

      expect(db.readSetBooleans("AllTypes", "code")).toEqual([[false, true], [true]]);
      expect(db.readSetBooleansById("AllTypes", "code", idFalse)).toEqual([false, true]);
      expect(db.readSetBooleansById("AllTypes", "code", idNull)).toEqual([]);

      expect(db.queryBoolean("SELECT 0")).toBeFalse();
      expect(
        db.queryBoolean("SELECT some_integer FROM AllTypes WHERE id = ?", [idTrue]),
      ).toBeTrue();
      expect(db.queryBoolean("SELECT some_integer FROM AllTypes WHERE id = -1")).toBeNull();
    } finally {
      db.close();
    }
  });

  test("rejects non-binary integers", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Invalid", some_integer: 2 });

      expect(() => db.readScalarBooleans("AllTypes", "some_integer")).toThrow(RangeError);
      expect(() => db.queryBoolean("SELECT 2")).toThrow(RangeError);
    } finally {
      db.close();
    }
  });
});
