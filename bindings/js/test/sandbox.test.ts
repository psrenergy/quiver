import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Database, QuiverError, Sandbox } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("Sandbox", () => {
  test("exports a Parquet snapshot through Lua", () => {
    const directory = mkdtempSync(join(tmpdir(), "quiver-parquet-"));
    const db = Database.fromSchema(join(directory, "test.db"), SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      sandbox.run(`
        local md = quiver.metadata{initial_datetime='2024-01-01T00:00:00', unit='MW',
          dimensions={'row'}, dimension_sizes={2}, labels={'value'}}
        local f = db:open_file('snapshot', 'w', md)
        f:write({1.5}, {row=2})
        f:close()
        db:bin_to_parquet('snapshot')
      `);
      expect(readFileSync(join(directory, "snapshot.parquet")).subarray(0, 4).toString()).toBe(
        "PAR1",
      );
    } finally {
      sandbox.close();
      db.close();
      rmSync(directory, { recursive: true, force: true });
    }
  });
  test("element REAL arrays preserve each Lua cell type", () => {
    const schema = join(
      __dirname,
      "..",
      "..",
      "..",
      "tests",
      "schemas",
      "valid",
      "collections.sql",
    );
    const db = Database.fromSchema(":memory:", schema);
    const sandbox = new Sandbox(db);
    try {
      sandbox.run(
        'db:create_element("Collection", { label = "Mixed", value_float = {1, 2.5, true} })',
      );
      expect(db.readVectorFloats("Collection", "value_float")).toEqual([[1, 2.5, 1]]);

      sandbox.run(
        'db:update_element_by_label("Collection", "Mixed", { value_float = {false, 3.5, 2} })',
      );
      expect(db.readVectorFloats("Collection", "value_float")).toEqual([[0, 3.5, 2]]);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("create element from sandbox and verify via JS", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      sandbox.run('db:create_element("AllTypes", { label = "FromSandbox" })');
      const labels = db.readScalarStrings("AllTypes", "label");
      expect(labels.includes("FromSandbox")).toBeTruthy();
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("script syntax error throws QuiverError", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const sandbox = new Sandbox(db);
    try {
      expect(() => sandbox.run("if then")).toThrow(QuiverError);
    } finally {
      sandbox.close();
      db.close();
    }
  });

  test("script runtime error throws QuiverError", () => {
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
