import { assertEquals, assertStringIncludes, assertThrows } from "jsr:@std/assert";
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

Deno.test({ name: "updateElement", sanitizeResources: false }, async (t) => {
  await t.step("updates integer scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.updateElement("AllTypes", id, { some_integer: 99 });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      assertEquals(value, 99);
    } finally {
      db.close();
    }
  });

  await t.step("updates float scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      db.updateElement("AllTypes", id, { some_float: 2.71 });
      const value = db.readScalarFloatById("AllTypes", "some_float", id);
      assertEquals(value, 2.71);
    } finally {
      db.close();
    }
  });

  await t.step("updates string scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      db.updateElement("AllTypes", id, { some_text: "world" });
      const value = db.readScalarStringById("AllTypes", "some_text", id);
      assertEquals(value, "world");
    } finally {
      db.close();
    }
  });

  await t.step("updates with null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.updateElement("AllTypes", id, { some_integer: null });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      assertEquals(value, null);
    } finally {
      db.close();
    }
  });

  await t.step("updates array field", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.updateElement("AllTypes", id, { code: [30, 40, 50] });
      const count = db.queryInteger(
        "SELECT COUNT(*) FROM AllTypes_set_codes WHERE id = ?",
        [id],
      );
      assertEquals(count, 3);
      const minVal = db.queryInteger(
        "SELECT MIN(code) FROM AllTypes_set_codes WHERE id = ?",
        [id],
      );
      assertEquals(minVal, 30);
      const maxVal = db.queryInteger(
        "SELECT MAX(code) FROM AllTypes_set_codes WHERE id = ?",
        [id],
      );
      assertEquals(maxVal, 50);
    } finally {
      db.close();
    }
  });

  await t.step("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    assertThrows(
      () => {
        db.updateElement("AllTypes", 1, { some_integer: 42 });
      },
      QuiverError,
    );

    try {
      db.updateElement("AllTypes", 1, { some_integer: 42 });
    } catch (e) {
      assertStringIncludes((e as QuiverError).message, "Database is closed");
    }
  });
});
