import { describe, expect, test } from "vitest";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
const __dirname = dirname(fileURLToPath(import.meta.url));
import { join } from "node:path";
import { Database } from "../src/index";

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

describe("queryString", () => {
  test("returns string from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE id = 1");
      expect(result).toBe("Item1");
    } finally {
      db.close();
    }
  });

  test("returns null when no rows match", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const result = db.queryString("SELECT label FROM AllTypes WHERE id = 9999");
      expect(result).toBeNull();
    } finally {
      db.close();
    }
  });

  test("returns string with parameterized SQL (string param)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE label = ?", ["Item1"]);
      expect(result).toBe("Item1");
    } finally {
      db.close();
    }
  });

  test("returns null with null parameter", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE label = ?", [null]);
      expect(result).toBeNull();
    } finally {
      db.close();
    }
  });
});

describe("queryInteger", () => {
  test("returns integer from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger("SELECT some_integer FROM AllTypes WHERE id = 1");
      expect(result).toBe(42);
    } finally {
      db.close();
    }
  });

  test("returns null when no rows match", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const result = db.queryInteger("SELECT some_integer FROM AllTypes WHERE id = 9999");
      expect(result).toBeNull();
    } finally {
      db.close();
    }
  });

  test("returns integer with parameterized SQL (integer param)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger(
        "SELECT some_integer FROM AllTypes WHERE some_integer > ?",
        [10],
      );
      expect(result).toBe(42);
    } finally {
      db.close();
    }
  });

  test("returns integer with multiple mixed-type parameters", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger(
        "SELECT some_integer FROM AllTypes WHERE id = ? AND some_integer > ?",
        [1, 0],
      );
      expect(result).toBe(42);
    } finally {
      db.close();
    }
  });
});

describe("queryFloat", () => {
  test("returns float from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const result = db.queryFloat("SELECT some_float FROM AllTypes WHERE id = 1");
      expect(result).toBe(3.14);
    } finally {
      db.close();
    }
  });

  test("returns float with parameterized SQL (float param)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const result = db.queryFloat("SELECT some_float FROM AllTypes WHERE some_float > ?", [1.0]);
      expect(result).toBe(3.14);
    } finally {
      db.close();
    }
  });
});
