import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database } from "../src/index.ts";

// The binding only verifies each method returns a string; the report content is
// covered by the C++ core tests (tests/test_database_describe.cpp).
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
      expect(typeof db.describe()).toBe("string");
    } finally {
      db.close();
    }
  });
});

describe("describeCollection", () => {
  test("returns a string", () => {
    const db = openDb();
    try {
      expect(typeof db.describeCollection("Collection")).toBe("string");
    } finally {
      db.close();
    }
  });
});

describe("summarizeCollection", () => {
  test("returns a string", () => {
    const db = openDb();
    try {
      expect(typeof db.summarizeCollection("Collection")).toBe("string");
    } finally {
      db.close();
    }
  });
});
