import { assert, assertEquals, assertNotEquals, assertThrows } from "jsr:@std/assert";
const __dirname = import.meta.dirname!;
import { join } from "jsr:@std/path";
import { Database } from "../src/index.ts";
import type { GroupMetadata, ScalarMetadata } from "../src/index.ts";

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

Deno.test({ name: "getScalarMetadata", sanitizeResources: false }, async (t) => {
  await t.step("returns correct metadata for an integer scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection", "some_integer");
      assertEquals(meta.name, "some_integer");
      assertEquals(typeof meta.dataType, "number");
      assertEquals(meta.notNull, false);
      assertEquals(meta.primaryKey, false);
      assertEquals(meta.defaultValue, null);
      assertEquals(meta.isForeignKey, false);
      assertEquals(meta.referencesCollection, null);
      assertEquals(meta.referencesColumn, null);
    } finally {
      db.close();
    }
  });

  await t.step("returns correct metadata for the primary key", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection", "id");
      assertEquals(meta.name, "id");
      assertEquals(meta.primaryKey, true);
      // SQLite INTEGER PRIMARY KEY does not set the not_null PRAGMA flag
      // even though the column is implicitly non-nullable
      assertEquals(typeof meta.notNull, "boolean");
    } finally {
      db.close();
    }
  });

  await t.step("returns correct metadata for a foreign key", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getScalarMetadata("Collection_vector_values", "id");
      assertEquals(meta.isForeignKey, true);
      assertEquals(meta.referencesCollection, "Collection");
      assertEquals(meta.referencesColumn, "id");
    } finally {
      db.close();
    }
  });

  await t.step("throws on nonexistent attribute", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertThrows(() => db.getScalarMetadata("Collection", "nonexistent"));
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "getVectorMetadata / getSetMetadata / getTimeSeriesMetadata", sanitizeResources: false }, async (t) => {
  await t.step("getVectorMetadata returns correct group", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getVectorMetadata("Collection", "values");
      assertEquals(meta.groupName, "values");
      assertEquals(meta.dimensionColumn, null);
      assert(meta.valueColumns.length >= 1);
      for (const col of meta.valueColumns) {
        assertEquals(typeof col.name, "string");
        assertEquals(typeof col.dataType, "number");
      }
    } finally {
      db.close();
    }
  });

  await t.step("getSetMetadata returns correct group", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getSetMetadata("Collection", "tags");
      assertEquals(meta.groupName, "tags");
      assertEquals(meta.dimensionColumn, null);
      assert(meta.valueColumns.length >= 1);
    } finally {
      db.close();
    }
  });

  await t.step("getTimeSeriesMetadata returns non-null dimensionColumn", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const meta = db.getTimeSeriesMetadata("Collection", "data");
      assertEquals(meta.groupName, "data");
      assertEquals(meta.dimensionColumn, "date_time");
      assert(meta.valueColumns.length >= 1);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "listScalarAttributes", sanitizeResources: false }, async (t) => {
  await t.step("lists all scalar attributes", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const attrs = db.listScalarAttributes("Collection");
      assertEquals(Array.isArray(attrs), true);
      // Collection has: id, label, some_integer, some_float
      assert(attrs.length >= 3);

      const someInt = attrs.find((a: ScalarMetadata) => a.name === "some_integer");
      assertNotEquals(someInt, undefined);
      assertEquals(typeof someInt!.dataType, "number");
      assertEquals(someInt!.primaryKey, false);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "listVectorGroups / listSetGroups / listTimeSeriesGroups", sanitizeResources: false }, async (t) => {
  await t.step("listVectorGroups returns vector groups", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listVectorGroups("Collection");
      assert(groups.length >= 1);

      const values = groups.find((g: GroupMetadata) => g.groupName === "values");
      assertNotEquals(values, undefined);
      assertEquals(values!.dimensionColumn, null);
    } finally {
      db.close();
    }
  });

  await t.step("listSetGroups returns set groups", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listSetGroups("Collection");
      assert(groups.length >= 1);

      const tags = groups.find((g: GroupMetadata) => g.groupName === "tags");
      assertNotEquals(tags, undefined);
      assertEquals(tags!.dimensionColumn, null);
    } finally {
      db.close();
    }
  });

  await t.step("listTimeSeriesGroups returns time series groups with dimensionColumn", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const groups = db.listTimeSeriesGroups("Collection");
      assert(groups.length >= 1);

      const data = groups.find((g: GroupMetadata) => g.groupName === "data");
      assertNotEquals(data, undefined);
      assertEquals(data!.dimensionColumn, "date_time");
    } finally {
      db.close();
    }
  });
});
