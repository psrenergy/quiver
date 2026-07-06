import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import type { GroupMetadata, ScalarMetadata } from "../src/index.ts";
import { Database } from "../src/index.ts";

const SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "collections.sql",
);

describe("getScalarMetadata", () => {
  test("returns correct metadata for an integer scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection", "some_integer");
      expect(meta.name).toEqual("some_integer");
      expect(typeof meta.dataType).toEqual("number");
      expect(meta.notNull).toEqual(false);
      expect(meta.primaryKey).toEqual(false);
      expect(meta.defaultValue).toEqual(null);
      expect(meta.isForeignKey).toEqual(false);
      expect(meta.referencesCollection).toEqual(null);
      expect(meta.referencesColumn).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("returns correct metadata for the primary key", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection", "id");
      expect(meta.name).toEqual("id");
      expect(meta.primaryKey).toEqual(true);
      // An INTEGER PRIMARY KEY is a rowid alias and is never NULL; the C++ core reports it
      // as not_null even though SQLite's PRAGMA table_info leaves the notnull flag unset.
      expect(meta.notNull).toEqual(true);
    } finally {
      db.close();
    }
  });

  test("returns correct metadata for a foreign key", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection_vector_values", "id");
      expect(meta.isForeignKey).toEqual(true);
      expect(meta.referencesCollection).toEqual("Collection");
      expect(meta.referencesColumn).toEqual("id");
    } finally {
      db.close();
    }
  });

  test("throws on nonexistent attribute", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.getScalarMetadata("Collection", "nonexistent")).toThrow();
    } finally {
      db.close();
    }
  });
});

describe("getVectorMetadata / getSetMetadata / getTimeSeriesMetadata", () => {
  test("getVectorMetadata returns correct group", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getVectorMetadata("Collection", "values");
      expect(meta.groupName).toEqual("values");
      expect(meta.dimensionColumn).toEqual(null);
      expect(meta.valueColumns.length >= 1).toBeTruthy();
      for (const col of meta.valueColumns) {
        expect(typeof col.name).toEqual("string");
        expect(typeof col.dataType).toEqual("number");
      }
    } finally {
      db.close();
    }
  });

  test("getSetMetadata returns correct group", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getSetMetadata("Collection", "tags");
      expect(meta.groupName).toEqual("tags");
      expect(meta.dimensionColumn).toEqual(null);
      expect(meta.valueColumns.length >= 1).toBeTruthy();
    } finally {
      db.close();
    }
  });

  test("getTimeSeriesMetadata returns non-null dimensionColumn", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getTimeSeriesMetadata("Collection", "data");
      expect(meta.groupName).toEqual("data");
      expect(meta.dimensionColumn).toEqual("date_time");
      expect(meta.valueColumns.length >= 1).toBeTruthy();
    } finally {
      db.close();
    }
  });
});

describe("listScalarAttributes", () => {
  test("lists all scalar attributes", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const attrs = db.listScalarAttributes("Collection");
      expect(Array.isArray(attrs)).toEqual(true);
      // Collection has: id, label, some_integer, some_float
      expect(attrs.length >= 3).toBeTruthy();

      const someInt = attrs.find((a: ScalarMetadata) => a.name === "some_integer");
      expect(someInt).not.toEqual(undefined);
      expect(typeof someInt!.dataType).toEqual("number");
      expect(someInt!.primaryKey).toEqual(false);
    } finally {
      db.close();
    }
  });
});

describe("listVectorGroups / listSetGroups / listTimeSeriesGroups", () => {
  test("listVectorGroups returns vector groups", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listVectorGroups("Collection");
      expect(groups.length >= 1).toBeTruthy();

      const values = groups.find((g: GroupMetadata) => g.groupName === "values");
      expect(values).not.toEqual(undefined);
      expect(values!.dimensionColumn).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("listSetGroups returns set groups", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listSetGroups("Collection");
      expect(groups.length >= 1).toBeTruthy();

      const tags = groups.find((g: GroupMetadata) => g.groupName === "tags");
      expect(tags).not.toEqual(undefined);
      expect(tags!.dimensionColumn).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("listTimeSeriesGroups returns time series groups with dimensionColumn", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listTimeSeriesGroups("Collection");
      expect(groups.length >= 1).toBeTruthy();

      const data = groups.find((g: GroupMetadata) => g.groupName === "data");
      expect(data).not.toEqual(undefined);
      expect(data!.dimensionColumn).toEqual("date_time");
    } finally {
      db.close();
    }
  });
});
