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

  test("a rejected updateElement inside a transaction writes nothing", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item 1", some_integer: 1 });
      db.beginTransaction();
      expect(() => db.updateElement("AllTypes", id, { some_integer: 2, tag: [1.5] })).toThrow(
        /type mismatch for array 'tag'/,
      );
      db.commit();
      expect(db.readScalarIntegerById("AllTypes", "some_integer", id)).toEqual(1);
    } finally {
      db.close();
    }
  });
});

describe("transaction and dryRun wrappers", () => {
  test("transaction commits on success and returns fn's value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.transaction((tx) => tx.createElement("AllTypes", { label: "TxnItem" }));
      expect(db.inTransaction()).toEqual(false);
      expect(db.readElementIds("AllTypes")).toEqual([id]);
    } finally {
      db.close();
    }
  });

  test("transaction rolls back and rethrows fn's original error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const boom = new Error("boom");
    try {
      expect(() =>
        db.transaction((tx) => {
          tx.createElement("AllTypes", { label: "TxnItem" });
          throw boom;
        }),
      ).toThrow(boom);
      expect(db.inTransaction()).toEqual(false);
      expect(db.readElementIds("AllTypes").length).toEqual(0);
    } finally {
      db.close();
    }
  });

  test("dryRun discards writes and returns fn's value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const labels = db.dryRun((tx) => {
        tx.createElement("AllTypes", { label: "Preview" });
        return tx.readScalarStrings("AllTypes", "label");
      });
      expect(labels).toEqual(["Preview"]);
      expect(db.inDryRun()).toEqual(false);
      expect(db.readElementIds("AllTypes").length).toEqual(0);
    } finally {
      db.close();
    }
  });

  test("dryRun rethrows fn's original error and leaves no dry run active", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const boom = new Error("boom");
    try {
      expect(() =>
        db.dryRun((tx) => {
          tx.createElement("AllTypes", { label: "Preview" });
          throw boom;
        }),
      ).toThrow(boom);
      expect(db.inDryRun()).toEqual(false);
      expect(db.readElementIds("AllTypes").length).toEqual(0);
    } finally {
      db.close();
    }
  });

  test("a failing endDryRun on the error path does not mask fn's error", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    const boom = new Error("boom");
    try {
      expect(() =>
        db.dryRun((tx) => {
          tx.endDryRun(); // the wrapper's own endDryRun now fails
          throw boom;
        }),
      ).toThrow(boom);
      expect(db.inDryRun()).toEqual(false);
    } finally {
      db.close();
    }
  });
});
