import { assert, assertEquals } from "jsr:@std/assert";
import { join } from "jsr:@std/path";
const __dirname = import.meta.dirname!;
import { Database } from "../src/index.ts";

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

Deno.test({ name: "introspection", sanitizeResources: false }, async (t) => {
  await t.step("isHealthy returns true for valid database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertEquals(db.isHealthy(), true);
    } finally {
      db.close();
    }
  });

  await t.step("currentVersion returns a number >= 0", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const version = db.currentVersion();
      assertEquals(typeof version, "number");
      assert(version >= 0);
    } finally {
      db.close();
    }
  });

  await t.step("path returns ':memory:' for in-memory databases", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertEquals(db.path(), ":memory:");
    } finally {
      db.close();
    }
  });

  await t.step("describe runs without error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.describe();
    } finally {
      db.close();
    }
  });
});
