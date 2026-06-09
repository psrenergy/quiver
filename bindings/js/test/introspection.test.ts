import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database } from "../src/index.ts";

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
});
