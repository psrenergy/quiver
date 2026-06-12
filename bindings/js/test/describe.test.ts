import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database } from "../src/index.ts";

const SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "collections.sql",
);

function seeded(): Database {
  const db = Database.fromSchema(":memory:", SCHEMA_PATH);
  db.createElement("Collection", { label: "a", some_integer: 1, value_int: [10, 20] });
  db.createElement("Collection", { label: "b", some_integer: 1 });
  db.createElement("Collection", { label: "c", some_integer: 5 });
  return db;
}

describe("describe", () => {
  test("returns a whole-database text report", () => {
    const db = seeded();
    try {
      const report = db.describe();
      expect(report).toContain("Database: :memory:");
      expect(report).toContain("Collection: Configuration");
      expect(report).toContain("Collection: Collection (3 elements)");
      expect(report).toContain("some_integer");
    } finally {
      db.close();
    }
  });
});

describe("describeCollection", () => {
  test("returns one collection's text report", () => {
    const db = seeded();
    try {
      const report = db.describeCollection("Collection");
      expect(report).toContain("Collection: Collection (3 elements)");
      expect(report).toContain("[date_time]"); // time-series dimension column
      expect(report).not.toContain("Collection: Configuration");
    } finally {
      db.close();
    }
  });
});

describe("summarizeCollection", () => {
  test("returns a value-statistics text report", () => {
    const db = seeded();
    try {
      const report = db.summarizeCollection("Collection");
      expect(report).toContain("some_integer: 3 non-null, 0 null; values {1: 2, 5: 1}");
      expect(report).toContain("values: 1/3 non-empty"); // the vector group named "values"
    } finally {
      db.close();
    }
  });
});
