import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";
import type { ElementData, Value } from "../src/types.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("createElement", () => {
  test("creates element with integer scalar and returns numeric ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const data: ElementData = { label: "Item1", some_integer: 42 };
      const id = db.createElement("AllTypes", data);
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with float scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with string scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: null });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("skips undefined values", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: undefined });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with bigint value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42n });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("returns incrementing IDs", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id1 = db.createElement("AllTypes", { label: "Item1" });
      const id2 = db.createElement("AllTypes", { label: "Item2" });
      expect(id2).toBeGreaterThan(id1);
    } finally {
      db.close();
    }
  });

  test("throws on unsupported type (boolean)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => {
        db.createElement("AllTypes", { label: "Item1", some_integer: true as unknown as Value });
      }).toThrow(QuiverError);

      try {
        db.createElement("AllTypes", { label: "Item1", some_integer: true as unknown as Value });
      } catch (e) {
        expect((e as QuiverError).message).toContain(
          "Unsupported value type for 'some_integer': boolean",
        );
      }
    } finally {
      db.close();
    }
  });

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => {
      db.createElement("AllTypes", { label: "Item1" });
    }).toThrow(QuiverError);

    try {
      db.createElement("AllTypes", { label: "Item1" });
    } catch (e) {
      expect((e as QuiverError).message).toContain("Database is closed");
    }
  });
});

describe("createElement with arrays", () => {
  test("creates element with integer array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20, 30] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with float array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", weight: [1.5, 2.5] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with string array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", tag: ["a", "b", "c"] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with empty array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", tag: [] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("treats number[] with all integers as integer array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [1, 2, 3] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("treats number[] with any decimal as float array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", weight: [1, 2.5, 3] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("creates element with bigint array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10n, 20n, 30n] });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });

  test("bigint array stores values beyond Number.MAX_SAFE_INTEGER without precision loss", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const big = 9007199254740993n; // 2^53 + 1 — not representable as a JS number
      db.createElement("AllTypes", { label: "Item1", count_value: [big] });
      const stored = db.queryString("SELECT CAST(count_value AS TEXT) FROM AllTypes_vector_counts");
      expect(stored).toBe("9007199254740993");
    } finally {
      db.close();
    }
  });
});

describe("deleteElement", () => {
  test("deletes element by ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      // Should not throw
      db.deleteElement("AllTypes", id);
    } finally {
      db.close();
    }
  });

  test("throws on non-existent ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      expect(() => db.deleteElement("AllTypes", 999)).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => {
      db.deleteElement("AllTypes", 1);
    }).toThrow(QuiverError);

    try {
      db.deleteElement("AllTypes", 1);
    } catch (e) {
      expect((e as QuiverError).message).toContain("Database is closed");
    }
  });
});

describe("scalar type coercion policy", () => {
  test("accepts a whole number for a REAL column", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      // JS sends whole numbers as int64; an integer is accepted for a REAL column.
      db.createElement("AllTypes", { label: "Item1", some_float: 7 });
      expect(db.queryFloat("SELECT some_float FROM AllTypes WHERE id = 1")).toEqual(7);
    } finally {
      db.close();
    }
  });

  test("rejects a fractional number for an INTEGER column", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.createElement("AllTypes", { label: "Item1", some_integer: 1.5 })).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });
});
