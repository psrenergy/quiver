import { describe, test, expect } from "bun:test";
import { join } from "node:path";
import { Database, QuiverError } from "../src/index";

const SCHEMA_PATH = join(import.meta.dir, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("transaction control", () => {
  test("commit persists data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.createElement("AllTypes", { label: "TxnItem" });
      db.commit();
      const ids = db.readElementIds("AllTypes");
      expect(ids).toHaveLength(1);
    } finally {
      db.close();
    }
  });

  test("rollback discards data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.createElement("AllTypes", { label: "TxnItem" });
      db.rollback();
      const ids = db.readElementIds("AllTypes");
      expect(ids).toHaveLength(0);
    } finally {
      db.close();
    }
  });

  test("commit without begin throws", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.commit()).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });

  test("rollback without begin throws", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.rollback()).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });

  test("inTransaction returns false initially", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(db.inTransaction()).toBe(false);
    } finally {
      db.close();
    }
  });

  test("inTransaction returns true after begin", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      expect(db.inTransaction()).toBe(true);
      db.rollback(); // cleanup
    } finally {
      db.close();
    }
  });

  test("inTransaction returns false after commit", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.commit();
      expect(db.inTransaction()).toBe(false);
    } finally {
      db.close();
    }
  });
});
