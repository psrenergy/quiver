import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database } from "../src/index.ts";

// Verify report forwarding; detailed formatting is covered by the C++ core tests.
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

function openDb(): Database {
  return Database.fromSchema(":memory:", SCHEMA_PATH);
}

describe("describe", () => {
  test("returns a string", () => {
    const db = openDb();
    try {
      expect(db.describe()).toBe(
        "Database: :memory:\nVersion: 0\n\n" +
          db.describeCollection("Collection") +
          "\n" +
          db.describeCollection("Configuration"),
      );
    } finally {
      db.close();
    }
  });
});

describe("describeCollection", () => {
  test("returns structure and statistics", () => {
    const db = openDb();
    try {
      db.createElement("Collection", { label: "a", some_integer: 1 });
      const report = db.describeCollection("Collection");
      expect(report).toContain("some_integer (INTEGER); 1 non-null, 0 null; values {1: 1}\n");
      expect(report).toContain("some_float (REAL); 0 non-null, 1 null\n");
      expect(report).toContain("data: [date_time], value(REAL); 0/1 non-empty\n");
    } finally {
      db.close();
    }
  });
});
