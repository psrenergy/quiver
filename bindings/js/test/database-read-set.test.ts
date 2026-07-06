import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("readSetIntegers / readSetFloats / readSetStrings", () => {
  test("reads integer sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", code: [30, 40] });
      const values = db.readSetIntegers("AllTypes", "code");
      expect(values.length).toEqual(2);
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
      expect(values.length).toEqual(2);
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
      expect(values.length).toEqual(2);
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
