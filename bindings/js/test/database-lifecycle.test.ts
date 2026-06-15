import { CString, type Pointer } from "bun:ffi";
import { afterAll, describe, expect, test } from "bun:test";
import { existsSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database, QuiverError } from "../src/index.ts";
import { getSymbols } from "../src/loader.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "basic.sql");
const MIGRATIONS_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "migrations");
const DATABASE_DIR = join(__dirname, "..", "..", "..", "tests", "schemas", "from_database");

describe("Database lifecycle", () => {
  const tempDirs: string[] = [];

  // Cleanup after all steps
  afterAll(() => {
    for (const dir of tempDirs) {
      try {
        rmSync(dir, { recursive: true, force: true });
      } catch {
        // Ignore cleanup errors
      }
    }
  });

  function makeTempDir(): string {
    const dir = mkdtempSync(join(tmpdir(), "quiver_js_test_"));
    tempDirs.push(dir);
    return dir;
  }

  test("fromSchema opens database and returns Database instance", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db !== undefined).toBeTruthy();
      expect(db instanceof Database).toBeTruthy();
    } finally {
      db.close();
    }
  });

  test("fromSchema creates database file on disk", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromSchema(dbPath, SCHEMA_PATH);
    try {
      expect(existsSync(dbPath)).toEqual(true);
    } finally {
      db.close();
    }
  });

  test("fromSchema with invalid schema throws QuiverError", () => {
    expect(() => {
      Database.fromSchema(":memory:", "nonexistent/path/schema.sql");
    }).toThrow(QuiverError);
  });

  test("fromMigrations opens database and returns Database instance", () => {
    const db = Database.fromMigrations(":memory:", MIGRATIONS_PATH);
    try {
      expect(db !== undefined).toBeTruthy();
      expect(db instanceof Database).toBeTruthy();
    } finally {
      db.close();
    }
  });

  test("fromMigrations creates database file on disk", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromMigrations(dbPath, MIGRATIONS_PATH);
    try {
      expect(existsSync(dbPath)).toEqual(true);
    } finally {
      db.close();
    }
  });

  test("fromMigrations with invalid path throws QuiverError", () => {
    expect(() => {
      Database.fromMigrations(":memory:", "nonexistent/path");
    }).toThrow(QuiverError);
  });

  test("fromDatabase applies migrations and loads UI metadata", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromDatabase(dbPath, DATABASE_DIR);
    try {
      expect(db.currentVersion()).toEqual(1);
      expect(db.getModelName()).toEqual("demo_model");
      expect(db.getAttributeUnit("Material", "demand")).toEqual("unit/year");
      expect(db.getAttributeUnit("Material", "label")).toEqual("");
      expect(db.getAttributeUnit("Nonexistent", "x")).toEqual("");
    } finally {
      db.close();
    }
  });

  test("fromDatabase with invalid path throws QuiverError", () => {
    expect(() => {
      Database.fromDatabase(":memory:", "nonexistent/path");
    }).toThrow(QuiverError);
  });

  test("UI metadata is empty without fromDatabase", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.getModelName()).toEqual("");
      expect(db.getAttributeUnit("Material", "demand")).toEqual("");
    } finally {
      db.close();
    }
  });

  test("open reopens an existing database file", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    Database.fromSchema(dbPath, SCHEMA_PATH).close();

    const reopened = Database.open(dbPath);
    try {
      expect(reopened instanceof Database).toBeTruthy();
    } finally {
      reopened.close();
    }
  });

  test("open with readOnly rejects writes", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    Database.fromSchema(dbPath, SCHEMA_PATH).close();

    const reopened = Database.open(dbPath, { readOnly: true });
    try {
      expect(() => reopened.createElement("Configuration", { label: "nope" })).toThrow();
    } finally {
      reopened.close();
    }
  });

  test("close is idempotent", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    // Second close should not throw
    db.close();
  });

  test("ensureOpen throws QuiverError after close", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    // Access the internal _handle to trigger ensureOpen
    // Since _handle calls ensureOpen, this tests the guard
    expect(() => {
      void db._handle;
    }).toThrow(QuiverError);
  });

  test("ensureOpen error message is 'Database is closed'", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    try {
      void db._handle;
      throw new Error("Should have thrown");
    } catch (e) {
      expect(e).toBeInstanceOf(QuiverError);
      expect((e as QuiverError).message).toEqual("Database is closed");
    }
  });
});

describe("Error handling", () => {
  test("QuiverError has correct name property", () => {
    const err = new QuiverError("test message");
    expect(err.name).toEqual("QuiverError");
  });

  test("QuiverError is an instance of Error", () => {
    const err = new QuiverError("test message");
    expect(err).toBeInstanceOf(Error);
  });

  test("QuiverError contains C-layer message", () => {
    try {
      Database.fromSchema(":memory:", "nonexistent/schema.sql");
      throw new Error("Should have thrown");
    } catch (e) {
      expect(e).toBeInstanceOf(QuiverError);
      // C layer returns "Schema file not found: ..." or similar
      expect((e as QuiverError).message).toBeTruthy();
      expect((e as QuiverError).message.length).toBeGreaterThan(0);
    }
  });
});

describe("Library loading", () => {
  test("quiver_version returns non-empty string", () => {
    const lib = getSymbols();
    const ptr = lib.quiver_version();
    expect(ptr).toBeTruthy();
    const version = new CString(ptr as Pointer).toString();
    expect(typeof version).toEqual("string");
    expect(version.length).toBeGreaterThan(0);
  });
});
