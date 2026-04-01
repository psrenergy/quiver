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

describe("readVectorIntegers / readVectorFloats / readVectorStrings", () => {
  test("reads integer vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", count_value: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", count_value: [30, 40, 50] });
      const values = db.readVectorIntegers("AllTypes", "count_value");
      expect(values).toEqual([[10, 20], [30, 40, 50]]);
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

describe("readSetIntegers / readSetFloats / readSetStrings", () => {
  test("reads integer sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", code: [30, 40] });
      const values = db.readSetIntegers("AllTypes", "code");
      expect(values).toHaveLength(2);
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
      expect(values).toHaveLength(2);
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
      expect(values).toHaveLength(2);
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
});
