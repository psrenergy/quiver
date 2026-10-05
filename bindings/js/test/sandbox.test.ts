import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database, Sandbox, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("Sandbox", () => {
  test("create element from Lua and verify via JS", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      sandbox.run('db:create_element("AllTypes", { label = "FromLua" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      expect(labels.includes("FromLua")).toBeTruthy();
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("Lua syntax error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      expect(() => sandbox.run("if then")).toThrow(QuiverError);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("Lua runtime error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      expect(() => sandbox.run("local x = nil; x.field = 1")).toThrow(QuiverError);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("multiple run calls on same sandbox succeed", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      sandbox.run('db:create_element("AllTypes", { label = "First" })');
      sandbox.run('db:create_element("AllTypes", { label = "Second" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      expect(labels.length).toEqual(2);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("empty script succeeds", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      // If this throws, the test fails automatically
      sandbox.run("");
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("close is idempotent", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      sandbox.close();
      // Second close should not throw
      sandbox.close();
    } finally {
      db.close();
    }
  });

  test("run after close throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      sandbox.close();
      expect(() => sandbox.run("print('hello')")).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });
});

describe("Sandbox return values", () => {
  test("returns the script's value as JSON", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      expect(sandbox.run("return { a = 1, b = { 2, 3 } }")).toBe('{"a":1,"b":[2,3]}');
      expect(JSON.parse(sandbox.run("return db:read_element_ids('AllTypes')"))).toEqual([]);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("returns an empty string when the script returns nothing", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      expect(sandbox.run("local x = 1")).toBe("");
    } finally {
      sandbox.close();
      db.close();
    }
  });
});

describe("Database dry run", () => {
  test("rolls back a script's writes", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      expect(db.inDryRun()).toBe(false);
      db.beginDryRun();
      expect(db.inDryRun()).toBe(true);

      // db:transaction composes: the dry run absorbs the nested BEGIN/COMMIT.
      const result = sandbox.run(`
        db:transaction(function(db)
          db:create_element("AllTypes", { label = "Preview" })
        end)
        return db:read_scalar_strings("AllTypes", "label")
      `);
      expect(JSON.parse(result)).toEqual(["Preview"]);

      db.endDryRun();
      expect(db.inDryRun()).toBe(false);
      expect(db.readScalarStrings("AllTypes", "label")).toEqual([]);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("endDryRun without a dry run throws", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.endDryRun()).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });
});
