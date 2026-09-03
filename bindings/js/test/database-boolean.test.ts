import { describe, expect, test } from "bun:test";
import { join } from "node:path";

import { Database } from "../src/index.ts";

const __dirname = import.meta.dir;

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

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

  test("writes booleans as integers", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", {
        label: "Written",
        some_integer: true,
        count_value: [true, false],
        code: [true],
      });

      expect(db.readScalarBooleanById("AllTypes", "some_integer", id)).toBeTrue();
      expect(db.readVectorBooleansById("AllTypes", "count_value", id)).toEqual([true, false]);
      expect(db.readSetBooleansById("AllTypes", "code", id)).toEqual([true]);

      db.updateElement("AllTypes", id, { some_integer: false });
      expect(db.readScalarBooleanById("AllTypes", "some_integer", id)).toBeFalse();

      expect(
        db.queryBoolean("SELECT some_integer FROM AllTypes WHERE some_integer = ?", [false]),
      ).toBeFalse();
    } finally {
      db.close();
    }
  });

  test("writes booleans as integers through the group writers", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Grouped" });

      // updateGroupColumns is shared by updateVectorGroup / updateSetGroup /
      // updateTimeSeriesGroup and their ByLabel forms.
      db.updateVectorGroup("AllTypes", "counts", id, { count_value: [true, false, true] });
      db.updateSetGroup("AllTypes", "codes", id, { code: [true, false] });

      expect(db.readVectorBooleansById("AllTypes", "count_value", id)).toEqual([true, false, true]);
      // A set has no insertion order, so compare unordered.
      expect(db.readSetBooleansById("AllTypes", "code", id).toSorted()).toEqual([false, true]);
    } finally {
      db.close();
    }
  });

  test("rejects non-binary integers", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", {
        label: "Invalid",
        some_integer: 2,
        count_value: [0, 2],
        code: [2],
      });

      expect(() => db.readScalarBooleans("AllTypes", "some_integer")).toThrow(
        /AllTypes\.some_integer.*expected 0 or 1/,
      );
      expect(() => db.readScalarBooleanById("AllTypes", "some_integer", id)).toThrow(RangeError);
      expect(() => db.readVectorBooleans("AllTypes", "count_value")).toThrow(RangeError);
      expect(() => db.readVectorBooleansById("AllTypes", "count_value", id)).toThrow(RangeError);
      expect(() => db.readSetBooleans("AllTypes", "code")).toThrow(RangeError);
      expect(() => db.readSetBooleansById("AllTypes", "code", id)).toThrow(RangeError);
      expect(() => db.queryBoolean("SELECT 2")).toThrow(RangeError);
    } finally {
      db.close();
    }
  });
});
