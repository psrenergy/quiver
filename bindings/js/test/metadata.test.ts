import { describe, expect, test } from "vitest";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
const __dirname = dirname(fileURLToPath(import.meta.url));
import { join } from "node:path";
import { Database } from "../src/index";
import type { GroupMetadata, ScalarMetadata } from "../src/index";

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
      expect(meta.name).toBe("some_integer");
      expect(typeof meta.dataType).toBe("number");
      expect(meta.notNull).toBe(false);
      expect(meta.primaryKey).toBe(false);
      expect(meta.defaultValue).toBeNull();
      expect(meta.isForeignKey).toBe(false);
      expect(meta.referencesCollection).toBeNull();
      expect(meta.referencesColumn).toBeNull();
    } finally {
      db.close();
    }
  });

  test("returns correct metadata for the primary key", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection", "id");
      expect(meta.name).toBe("id");
      expect(meta.primaryKey).toBe(true);
      // SQLite INTEGER PRIMARY KEY does not set the not_null PRAGMA flag
      // even though the column is implicitly non-nullable
      expect(typeof meta.notNull).toBe("boolean");
    } finally {
      db.close();
    }
  });

  test("returns correct metadata for a foreign key", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection_vector_values", "id");
      expect(meta.isForeignKey).toBe(true);
      expect(meta.referencesCollection).toBe("Collection");
      expect(meta.referencesColumn).toBe("id");
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
      expect(meta.groupName).toBe("values");
      expect(meta.dimensionColumn).toBeNull();
      expect(meta.valueColumns.length).toBeGreaterThanOrEqual(1);
      for (const col of meta.valueColumns) {
        expect(typeof col.name).toBe("string");
        expect(typeof col.dataType).toBe("number");
      }
    } finally {
      db.close();
    }
  });

  test("getSetMetadata returns correct group", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getSetMetadata("Collection", "tags");
      expect(meta.groupName).toBe("tags");
      expect(meta.dimensionColumn).toBeNull();
      expect(meta.valueColumns.length).toBeGreaterThanOrEqual(1);
    } finally {
      db.close();
    }
  });

  test("getTimeSeriesMetadata returns non-null dimensionColumn", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getTimeSeriesMetadata("Collection", "data");
      expect(meta.groupName).toBe("data");
      expect(meta.dimensionColumn).toBe("date_time");
      expect(meta.valueColumns.length).toBeGreaterThanOrEqual(1);
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
      expect(Array.isArray(attrs)).toBe(true);
      // Collection has: id, label, some_integer, some_float
      expect(attrs.length).toBeGreaterThanOrEqual(3);

      const someInt = attrs.find((a: ScalarMetadata) => a.name === "some_integer");
      expect(someInt).toBeDefined();
      expect(typeof someInt!.dataType).toBe("number");
      expect(someInt!.primaryKey).toBe(false);
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
      expect(groups.length).toBeGreaterThanOrEqual(1);

      const values = groups.find((g: GroupMetadata) => g.groupName === "values");
      expect(values).toBeDefined();
      expect(values!.dimensionColumn).toBeNull();
    } finally {
      db.close();
    }
  });

  test("listSetGroups returns set groups", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listSetGroups("Collection");
      expect(groups.length).toBeGreaterThanOrEqual(1);

      const tags = groups.find((g: GroupMetadata) => g.groupName === "tags");
      expect(tags).toBeDefined();
      expect(tags!.dimensionColumn).toBeNull();
    } finally {
      db.close();
    }
  });

  test("listTimeSeriesGroups returns time series groups with dimensionColumn", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listTimeSeriesGroups("Collection");
      expect(groups.length).toBeGreaterThanOrEqual(1);

      const data = groups.find((g: GroupMetadata) => g.groupName === "data");
      expect(data).toBeDefined();
      expect(data!.dimensionColumn).toBe("date_time");
    } finally {
      db.close();
    }
  });
});
