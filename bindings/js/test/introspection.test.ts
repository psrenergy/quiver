import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("introspection", () => {
  test("isHealthy returns true for valid database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.isHealthy()).toEqual(true);
    } finally {
      db.close();
    }
  });

  test("currentVersion returns a number >= 0", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const version = db.currentVersion();
      expect(typeof version).toEqual("number");
      expect(version >= 0).toBeTruthy();
    } finally {
      db.close();
    }
  });

  test("path returns ':memory:' for in-memory databases", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.path()).toEqual(":memory:");
    } finally {
      db.close();
    }
  });

  test("describe runs without error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.describe();
    } finally {
      db.close();
    }
  });

  test("numberOfElements counts current rows, not maximum id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const empty = db.numberOfElements("AllTypes");
      expect(typeof empty).toEqual("number");
      expect(empty).toEqual(0);

      db.createElement("AllTypes", { label: "Item 1" });
      const middleId = db.createElement("AllTypes", { label: "Item 2" });
      db.createElement("AllTypes", { label: "Item 3" });
      expect(db.numberOfElements("AllTypes")).toEqual(3);

      // Deleting the middle id leaves a gap in the ids; the count still drops.
      db.deleteElement("AllTypes", middleId);
      expect(db.numberOfElements("AllTypes")).toEqual(2);
    } finally {
      db.close();
    }
  });

  test("numberOfElements throws on unknown collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.numberOfElements("DoesNotExist")).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });
});
