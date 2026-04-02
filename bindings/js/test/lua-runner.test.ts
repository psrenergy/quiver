import { describe, expect, test } from "vitest";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
const __dirname = dirname(fileURLToPath(import.meta.url));
import { join } from "node:path";
import { Database, LuaRunner, QuiverError } from "../src/index";

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

describe("LuaRunner", () => {
  test("create element from Lua and verify via JS", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.run('db:create_element("AllTypes", { label = "FromLua" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      expect(labels).toContain("FromLua");
    } finally {
      runner.close();
      db.close();
    }
  });

  test("Lua syntax error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      expect(() => runner.run("if then")).toThrow(QuiverError);
    } finally {
      runner.close();
      db.close();
    }
  });

  test("Lua runtime error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      expect(() => runner.run("local x = nil; x.field = 1")).toThrow(QuiverError);
    } finally {
      runner.close();
      db.close();
    }
  });

  test("multiple run calls on same runner succeed", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.run('db:create_element("AllTypes", { label = "First" })');
      runner.run('db:create_element("AllTypes", { label = "Second" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      expect(labels).toHaveLength(2);
    } finally {
      runner.close();
      db.close();
    }
  });

  test("empty script succeeds", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      expect(() => runner.run("")).not.toThrow();
    } finally {
      runner.close();
      db.close();
    }
  });

  test("close is idempotent", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.close();
      expect(() => runner.close()).not.toThrow();
    } finally {
      db.close();
    }
  });

  test("run after close throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const runner = new LuaRunner(db);
    try {
      runner.close();
      expect(() => runner.run("print('hello')")).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });
});
