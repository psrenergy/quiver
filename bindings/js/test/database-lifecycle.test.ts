import { CString, type Pointer } from "bun:ffi";
import { afterAll, describe, expect, test } from "bun:test";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";

const __dirname = import.meta.dir;

import { join, relative } from "node:path";
import { Database, LOG_LEVEL_OFF, QuiverError, Sandbox } from "../src/index.ts";
import { getSymbols } from "../src/loader.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "basic.sql");
const MIGRATIONS_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "migrations");
const INVALID_SCHEMAS_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "invalid");

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

  for (const factory of ["open", "fromSchema", "fromMigrations"] as const) {
    test(`logPath selects the file for ${factory} and receives Lua prints`, () => {
      const dir = makeTempDir();
      const dbPath = join(dir, "test.db");
      const logPath = join(dir, "logs", `${factory}.log`);
      const options = {
        consoleLevel: LOG_LEVEL_OFF,
        logPath: factory === "fromSchema" ? logPath : join("logs", `${factory}.log`),
      };
      const db =
        factory === "open"
          ? Database.open(dbPath, options)
          : factory === "fromSchema"
            ? Database.fromSchema(dbPath, SCHEMA_PATH, options)
            : Database.fromMigrations(dbPath, MIGRATIONS_PATH, options);
      try {
        const sandbox = new Sandbox(db);
        try {
          sandbox.run(`print('custom ${factory}')`);
          expect(readFileSync(logPath, "utf8")).toContain(`[info] custom ${factory}`);
        } finally {
          sandbox.close();
        }
      } finally {
        db.close();
      }
      expect(readFileSync(logPath, "utf8")).toContain("Opening database:");
      expect(existsSync(join(dir, "quiver_database.log"))).toBe(false);
    });
  }

  test("explicit logPath enables read-only and in-memory file logging", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    Database.fromMigrations(dbPath, MIGRATIONS_PATH, { consoleLevel: LOG_LEVEL_OFF }).close();
    const readonlyLog = join(dir, "readonly.log");
    for (const open of [
      () =>
        Database.open(dbPath, {
          readOnly: true,
          consoleLevel: LOG_LEVEL_OFF,
          logPath: readonlyLog,
        }),
      () =>
        Database.fromMigrations(dbPath, MIGRATIONS_PATH, {
          readOnly: true,
          consoleLevel: LOG_LEVEL_OFF,
          logPath: join(dir, "migrations-readonly.log"),
        }),
    ]) {
      open().close();
    }
    expect(readFileSync(readonlyLog, "utf8")).toContain("Database opened successfully:");
    expect(readFileSync(join(dir, "migrations-readonly.log"), "utf8")).toContain(
      "Database opened successfully:",
    );

    const memoryLog = join(dir, "memory.log");
    Database.fromSchema(":memory:", SCHEMA_PATH, {
      consoleLevel: LOG_LEVEL_OFF,
      logPath: relative(process.cwd(), memoryLog),
    }).close();
    expect(readFileSync(memoryLog, "utf8")).toContain("Database opened successfully: :memory:");
  });

  test("omitted and empty logPath keep defaults, and explicit logs truncate on open", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const logPath = join(dir, "quiver_database.log");
    Database.open(dbPath, { consoleLevel: LOG_LEVEL_OFF }).close();
    const original = readFileSync(logPath, "utf8");
    Database.open(dbPath, { readOnly: true, consoleLevel: LOG_LEVEL_OFF, logPath: "" }).close();
    expect(readFileSync(logPath, "utf8")).toBe(original);
    rmSync(logPath);
    Database.open(dbPath, { consoleLevel: LOG_LEVEL_OFF, logPath: "" }).close();
    expect(existsSync(logPath)).toBe(true);
    writeFileSync(logPath, "old log history");
    Database.open(dbPath, { consoleLevel: LOG_LEVEL_OFF, logPath }).close();
    expect(readFileSync(logPath, "utf8")).not.toContain("old log history");
  });

  test("explicit log failures throw through every factory and preserve the database", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromSchema(dbPath, SCHEMA_PATH, { consoleLevel: LOG_LEVEL_OFF });
    db.createElement("Configuration", { label: "preserved" });
    db.close();
    const options = { consoleLevel: LOG_LEVEL_OFF, logPath: dir };
    for (const open of [
      () => Database.open(dbPath, options),
      () => Database.fromSchema(dbPath, SCHEMA_PATH, options),
      () => Database.fromMigrations(dbPath, MIGRATIONS_PATH, options),
    ]) {
      expect(open).toThrow(QuiverError);
      const reopened = Database.open(dbPath, { readOnly: true, consoleLevel: LOG_LEVEL_OFF });
      try {
        expect(reopened.readScalarStrings("Configuration", "label")).toEqual(["preserved"]);
      } finally {
        reopened.close();
      }
    }
    expect(() => Database.open(dbPath, { ...options, logPath: dbPath })).toThrow(QuiverError);
  });

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

  test("fromSchema rejects a set table without a parent foreign key", () => {
    expect(() =>
      Database.fromSchema(":memory:", join(INVALID_SCHEMAS_PATH, "set_no_parent_fk.sql")),
    ).toThrow(
      "Set table 'Collection_set_tags' must have foreign key to parent collection 'Collection'",
    );
  });

  test("fromMigrations opens database and returns Database instance", () => {
    const db = Database.fromMigrations(":memory:", MIGRATIONS_PATH);
    try {
      expect(db instanceof Database).toBeTruthy();
      expect(db.currentVersion()).toBe(3);
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

  test("fromMigrations readOnly rejects a database behind its migrations", () => {
    expect(() => {
      Database.fromMigrations(":memory:", MIGRATIONS_PATH, { readOnly: true });
    }).toThrow("a read-only open cannot apply them");
  });

  test("fromMigrations readOnly opens an up-to-date database file", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    Database.fromMigrations(dbPath, MIGRATIONS_PATH).close();

    const db = Database.fromMigrations(dbPath, MIGRATIONS_PATH, { readOnly: true });
    try {
      expect(db.currentVersion()).toBe(3);
    } finally {
      db.close();
    }
  });

  test("validateMigrations succeeds for a valid migrations directory", () => {
    // Should not throw
    Database.validateMigrations(MIGRATIONS_PATH);
  });

  test("validateMigrations with invalid path throws QuiverError", () => {
    const badPath = join("nonexistent", "path");
    try {
      Database.validateMigrations(badPath);
      throw new Error("Should have thrown");
    } catch (e) {
      expect(e).toBeInstanceOf(QuiverError);
      expect((e as QuiverError).message).toEqual(
        `Cannot validate_migrations: migrations path not found: ${badPath}`,
      );
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
