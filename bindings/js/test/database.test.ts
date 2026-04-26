import {
  assert,
  assertEquals,
  assertGreater,
  assertInstanceOf,
  assertThrows,
} from "jsr:@std/assert";
const __dirname = import.meta.dirname!;
import { join } from "jsr:@std/path";
import { Database, QuiverError } from "../src/index.ts";
import { getSymbols } from "../src/loader.ts";

function existsSync(path: string): boolean {
  try {
    Deno.statSync(path);
    return true;
  } catch {
    return false;
  }
}

const SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "basic.sql",
);
const MIGRATIONS_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "migrations");

Deno.test({ name: "Database lifecycle", sanitizeResources: false }, async (t) => {
  const tempDirs: string[] = [];

  function makeTempDir(): string {
    const dir = Deno.makeTempDirSync({ prefix: "quiver_js_test_" });
    tempDirs.push(dir);
    return dir;
  }

  await t.step("fromSchema opens database and returns Database instance", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assert(db !== undefined);
      assert(db instanceof Database);
    } finally {
      db.close();
    }
  });

  await t.step("fromSchema creates database file on disk", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromSchema(dbPath, SCHEMA_PATH);
    try {
      assertEquals(existsSync(dbPath), true);
    } finally {
      db.close();
    }
  });

  await t.step("fromSchema with invalid schema throws QuiverError", () => {
    assertThrows(() => {
      Database.fromSchema(":memory:", "nonexistent/path/schema.sql");
    }, QuiverError);
  });

  await t.step("fromMigrations opens database and returns Database instance", () => {
    const db = Database.fromMigrations(":memory:", MIGRATIONS_PATH);
    try {
      assert(db !== undefined);
      assert(db instanceof Database);
    } finally {
      db.close();
    }
  });

  await t.step("fromMigrations creates database file on disk", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    const db = Database.fromMigrations(dbPath, MIGRATIONS_PATH);
    try {
      assertEquals(existsSync(dbPath), true);
    } finally {
      db.close();
    }
  });

  await t.step("fromMigrations with invalid path throws QuiverError", () => {
    assertThrows(() => {
      Database.fromMigrations(":memory:", "nonexistent/path");
    }, QuiverError);
  });

  await t.step("open reopens an existing database file", () => {
    const dir = makeTempDir();
    const dbPath = join(dir, "test.db");
    Database.fromSchema(dbPath, SCHEMA_PATH).close();

    const reopened = Database.open(dbPath);
    try {
      assert(reopened instanceof Database);
    } finally {
      reopened.close();
    }
  });

  await t.step("close is idempotent", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    // Second close should not throw
    db.close();
  });

  await t.step("ensureOpen throws QuiverError after close", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    // Access the internal _handle to trigger ensureOpen
    // Since _handle calls ensureOpen, this tests the guard
    assertThrows(() => {
      void db._handle;
    }, QuiverError);
  });

  await t.step("ensureOpen error message is 'Database is closed'", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    try {
      void db._handle;
      throw new Error("Should have thrown");
    } catch (e) {
      assertInstanceOf(e, QuiverError);
      assertEquals((e as QuiverError).message, "Database is closed");
    }
  });

  // Cleanup after all steps
  for (const dir of tempDirs) {
    try {
      Deno.removeSync(dir, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
});

Deno.test({ name: "Error handling", sanitizeResources: false }, async (t) => {
  await t.step("QuiverError has correct name property", () => {
    const err = new QuiverError("test message");
    assertEquals(err.name, "QuiverError");
  });

  await t.step("QuiverError is an instance of Error", () => {
    const err = new QuiverError("test message");
    assertInstanceOf(err, Error);
  });

  await t.step("QuiverError contains C-layer message", () => {
    try {
      Database.fromSchema(":memory:", "nonexistent/schema.sql");
      throw new Error("Should have thrown");
    } catch (e) {
      assertInstanceOf(e, QuiverError);
      // C layer returns "Schema file not found: ..." or similar
      assert((e as QuiverError).message);
      assertGreater((e as QuiverError).message.length, 0);
    }
  });
});

Deno.test({ name: "Library loading", sanitizeResources: false }, async (t) => {
  await t.step("quiver_version returns non-empty string", () => {
    const lib = getSymbols();
    const ptr = lib.quiver_version();
    assert(ptr, "quiver_version returned null pointer");
    const version = new Deno.UnsafePointerView(ptr as NonNullable<Deno.PointerValue>).getCString();
    assertEquals(typeof version, "string");
    assertGreater(version.length, 0);
  });
});
