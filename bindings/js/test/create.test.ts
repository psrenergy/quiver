import { describe, expect, test } from "bun:test";
import { join } from "node:path";
import { Database, QuiverError } from "../src/index";
import type { ElementData, Value } from "../src/types";

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

describe("createElement", () => {
  test("creates element with integer scalar and returns numeric ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const data: ElementData = { label: "Item1", some_integer: 42 };
      const id = db.createElement("AllTypes", data);
      expect(typeof id).toBe("number");
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
      expect((e as QuiverError).message).toBe("Database is closed");
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

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => {
      db.deleteElement("AllTypes", 1);
    }).toThrow(QuiverError);

    try {
      db.deleteElement("AllTypes", 1);
    } catch (e) {
      expect((e as QuiverError).message).toBe("Database is closed");
    }
  });
});
