import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("queryString", () => {
  test("returns string from plain SQL", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE id = 1");
      expect(result).toEqual("Item1");
    } finally {
      db.close();
    }
  });

  test("returns null when no rows match", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const result = db.queryString("SELECT label FROM AllTypes WHERE id = 9999");
      expect(result).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("returns string with parameterized SQL (string parameter)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE label = ?", ["Item1"]);
      expect(result).toEqual("Item1");
    } finally {
      db.close();
    }
  });

  test("returns null with null parameter", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      const result = db.queryString("SELECT label FROM AllTypes WHERE label = ?", [null]);
      expect(result).toEqual(null);
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
      expect(result).toEqual(42);
    } finally {
      db.close();
    }
  });

  test("returns null when no rows match", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const result = db.queryInteger("SELECT some_integer FROM AllTypes WHERE id = 9999");
      expect(result).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("returns integer with parameterized SQL (integer parameter)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const result = db.queryInteger(
        "SELECT some_integer FROM AllTypes WHERE some_integer > ?",
        [10],
      );
      expect(result).toEqual(42);
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
      expect(result).toEqual(42);
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
      expect(result).toEqual(3.14);
    } finally {
      db.close();
    }
  });

  test("returns float with parameterized SQL (float parameter)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const result = db.queryFloat("SELECT some_float FROM AllTypes WHERE some_float > ?", [1.0]);
      expect(result).toEqual(3.14);
    } finally {
      db.close();
    }
  });
});

describe("query parameter count", () => {
  test("throws when parameter count does not match placeholders", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 1 });
      // Too few parameters for one placeholder
      expect(() => db.queryString("SELECT label FROM AllTypes WHERE some_integer = ?", [])).toThrow();
      // Too many parameters for one placeholder
      expect(() => db.queryString("SELECT label FROM AllTypes WHERE some_integer = ?", [1, 2])).toThrow();
      // Exactly one parameter succeeds
      expect(db.queryString("SELECT label FROM AllTypes WHERE some_integer = ?", [1])).toEqual("Item1");
    } finally {
      db.close();
    }
  });
});
