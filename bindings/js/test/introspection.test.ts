import { describe, expect, test } from "vitest";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
const __dirname = dirname(fileURLToPath(import.meta.url));
import { join } from "node:path";
import { Database } from "../src/index";

const SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "all_types.sql",
);

describe("introspection", () => {
  test("isHealthy returns true for valid database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.isHealthy()).toBe(true);
    } finally {
      db.close();
    }
  });

  test("currentVersion returns a number >= 0", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const version = db.currentVersion();
      expect(typeof version).toBe("number");
      expect(version).toBeGreaterThanOrEqual(0);
    } finally {
      db.close();
    }
  });

  test("path returns ':memory:' for in-memory databases", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.path()).toBe(":memory:");
    } finally {
      db.close();
    }
  });

  test("describe runs without error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.describe()).not.toThrow();
    } finally {
      db.close();
    }
  });
});
