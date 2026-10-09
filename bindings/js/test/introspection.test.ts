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

  test("currentVersion returns 0 for a schema database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.currentVersion()).toBe(0);
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
});
