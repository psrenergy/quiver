import { describe, test, expect, afterEach } from "bun:test";
import { join } from "node:path";
import { mkdtempSync, rmSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { Database, QuiverError } from "../src/index";
import { getSymbols } from "../src/loader";

const SCHEMA_PATH = join(import.meta.dir, "..", "..", "..", "tests", "schemas", "valid", "basic.sql");
const MIGRATIONS_PATH = join(import.meta.dir, "..", "..", "..", "tests", "schemas", "migrations");

describe("Database lifecycle", () => {
  const tempDirs: string[] = [];

  function makeTempDir(): string {
    const dir = mkdtempSync(join(tmpdir(), "quiver_js_test_"));
    tempDirs.push(dir);
    return dir;
  }

  afterEach(() => {
    for (const dir of tempDirs) {
      try {
        rmSync(dir, { recursive: true });
      } catch {
        // Ignore cleanup errors
      }
    }
    tempDirs.length = 0;
  });

  test("fromSchema opens database and returns Database instance", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db).toBeDefined();
      expect(db).toBeInstanceOf(Database);
    } finally {
      db.close();
    }
  });

  test("fromSchema creates database file on disk", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromSchema(dbPath, SCHEMA_PATH);
    try {
      expect(existsSync(dbPath)).toBe(true);
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
      expect(db).toBeDefined();
      expect(db).toBeInstanceOf(Database);
    } finally {
      db.close();
    }
  });

  test("fromMigrations creates database file on disk", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromMigrations(dbPath, MIGRATIONS_PATH);
    try {
      expect(existsSync(dbPath)).toBe(true);
    } finally {
      db.close();
    }
  });

  test("fromMigrations with invalid path throws QuiverError", () => {
    expect(() => {
      Database.fromMigrations(":memory:", "nonexistent/path");
    }).toThrow(QuiverError);
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
    expect(() => db._handle).toThrow(QuiverError);
  });

  test("ensureOpen error message is 'Database is closed'", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    try {
      void db._handle;
      throw new Error("Should have thrown");
    } catch (e) {
      expect(e).toBeInstanceOf(QuiverError);
      expect((e as QuiverError).message).toBe("Database is closed");
    }
  });
});

describe("Error handling", () => {
  test("QuiverError has correct name property", () => {
    const err = new QuiverError("test message");
    expect(err.name).toBe("QuiverError");
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
    const version = lib.quiver_version();
    expect(version).toBeTruthy();
    expect(typeof version).toBe("string");
    expect((version as string).length).toBeGreaterThan(0);
  });
});
