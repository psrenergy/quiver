import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("transaction control", () => {
  test("commit persists data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      db.createElement("AllTypes", { label: "TxnItem" });
      db.commit();
      const ids = db.readElementIds("AllTypes");
      expect(ids.length).toEqual(1);
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
      expect(ids.length).toEqual(0);
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
      expect(db.inTransaction()).toEqual(false);
    } finally {
      db.close();
    }
  });

  test("inTransaction returns true after begin", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.beginTransaction();
      expect(db.inTransaction()).toEqual(true);
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
      expect(db.inTransaction()).toEqual(false);
    } finally {
      db.close();
    }
  });
});
