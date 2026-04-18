import { assert, assertEquals, assertThrows } from "jsr:@std/assert";
const __dirname = import.meta.dirname!;
import { join } from "jsr:@std/path";
import { Database, LuaRunner, QuiverError } from "../src/index.ts";

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

Deno.test({ name: "LuaRunner", sanitizeResources: false }, async (t) => {
  await t.step("create element from Lua and verify via JS", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.run('db:create_element("AllTypes", { label = "FromLua" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      assert(labels.includes("FromLua"));
    } finally {
      runner.close();
      db.close();
    }
  });

  await t.step("Lua syntax error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      assertThrows(() => runner.run("if then"), QuiverError);
    } finally {
      runner.close();
      db.close();
    }
  });

  await t.step("Lua runtime error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      assertThrows(() => runner.run("local x = nil; x.field = 1"), QuiverError);
    } finally {
      runner.close();
      db.close();
    }
  });

  await t.step("multiple run calls on same runner succeed", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.run('db:create_element("AllTypes", { label = "First" })');
      runner.run('db:create_element("AllTypes", { label = "Second" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      assertEquals(labels.length, 2);
    } finally {
      runner.close();
      db.close();
    }
  });

  await t.step("empty script succeeds", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      // If this throws, the test fails automatically
      runner.run("");
    } finally {
      runner.close();
      db.close();
    }
  });

  await t.step("close is idempotent", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.close();
      // Second close should not throw
      runner.close();
    } finally {
      db.close();
    }
  });

  await t.step("run after close throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.close();
      assertThrows(() => runner.run("print('hello')"), QuiverError);
    } finally {
      db.close();
    }
  });
});
