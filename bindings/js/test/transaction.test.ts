import { assertEquals, assertThrows } from "jsr:@std/assert";
import { join } from "jsr:@std/path";
const __dirname = import.meta.dirname!;
import { Database, QuiverError } from "../src/index.ts";

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

Deno.test({ name: "transaction control", sanitizeResources: false }, async (t) => {
  await t.step("commit persists data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.createElement("AllTypes", { label: "TxnItem" });
      db.commit();
      const ids = db.readElementIds("AllTypes");
      assertEquals(ids.length, 1);
    } finally {
      db.close();
    }
  });

  await t.step("rollback discards data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.createElement("AllTypes", { label: "TxnItem" });
      db.rollback();
      const ids = db.readElementIds("AllTypes");
      assertEquals(ids.length, 0);
    } finally {
      db.close();
    }
  });

  await t.step("commit without begin throws", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertThrows(() => db.commit(), QuiverError);
    } finally {
      db.close();
    }
  });

  await t.step("rollback without begin throws", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertThrows(() => db.rollback(), QuiverError);
    } finally {
      db.close();
    }
  });

  await t.step("inTransaction returns false initially", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertEquals(db.inTransaction(), false);
    } finally {
      db.close();
    }
  });

  await t.step("inTransaction returns true after begin", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      assertEquals(db.inTransaction(), true);
      db.rollback(); // cleanup
    } finally {
      db.close();
    }
  });

  await t.step("inTransaction returns false after commit", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.commit();
      assertEquals(db.inTransaction(), false);
    } finally {
      db.close();
    }
  });
});
