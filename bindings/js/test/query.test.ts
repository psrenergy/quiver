import { assertEquals } from "jsr:@std/assert";
import { join } from "jsr:@std/path";
const __dirname = import.meta.dirname!;
import { Database } from "../src/index.ts";

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

Deno.test({ name: "queryString", sanitizeResources: false }, async (t) => {
  await t.step("returns string from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE id = 1");
      assertEquals(result, "Item1");
    } finally {
      db.close();
    }
  });

  await t.step("returns null when no rows match", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const result = db.queryString("SELECT label FROM AllTypes WHERE id = 9999");
      assertEquals(result, null);
    } finally {
      db.close();
    }
  });

  await t.step("returns string with parameterized SQL (string param)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE label = ?", ["Item1"]);
      assertEquals(result, "Item1");
    } finally {
      db.close();
    }
  });

  await t.step("returns null with null parameter", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE label = ?", [null]);
      assertEquals(result, null);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "queryInteger", sanitizeResources: false }, async (t) => {
  await t.step("returns integer from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger("SELECT some_integer FROM AllTypes WHERE id = 1");
      assertEquals(result, 42);
    } finally {
      db.close();
    }
  });

  await t.step("returns null when no rows match", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const result = db.queryInteger("SELECT some_integer FROM AllTypes WHERE id = 9999");
      assertEquals(result, null);
    } finally {
      db.close();
    }
  });

  await t.step("returns integer with parameterized SQL (integer param)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger(
        "SELECT some_integer FROM AllTypes WHERE some_integer > ?",
        [10],
      );
      assertEquals(result, 42);
    } finally {
      db.close();
    }
  });

  await t.step("returns integer with multiple mixed-type parameters", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger(
        "SELECT some_integer FROM AllTypes WHERE id = ? AND some_integer > ?",
        [1, 0],
      );
      assertEquals(result, 42);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "queryFloat", sanitizeResources: false }, async (t) => {
  await t.step("returns float from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const result = db.queryFloat("SELECT some_float FROM AllTypes WHERE id = 1");
      assertEquals(result, 3.14);
    } finally {
      db.close();
    }
  });

  await t.step("returns float with parameterized SQL (float param)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const result = db.queryFloat("SELECT some_float FROM AllTypes WHERE some_float > ?", [1.0]);
      assertEquals(result, 3.14);
    } finally {
      db.close();
    }
  });
});
