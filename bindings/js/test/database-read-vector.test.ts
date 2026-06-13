import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("readVectorIntegers / readVectorFloats / readVectorStrings", () => {
  test("reads integer vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", count_value: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", count_value: [30, 40, 50] });
      const values = db.readVectorIntegers("AllTypes", "count_value");
      expect(values).toEqual([
        [10, 20],
        [30, 40, 50],
      ]);
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
