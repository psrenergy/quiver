import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

describe("updateElement", () => {
  test("updates integer scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.updateElement("AllTypes", id, { some_integer: 99 });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toEqual(99);
    } finally {
      db.close();
    }
  });

  test("throws on non-existent ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1" });
      expect(() => db.updateElement("AllTypes", 999, { some_integer: 5 })).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });

  test("updates float scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      db.updateElement("AllTypes", id, { some_float: 2.71 });
      const value = db.readScalarFloatById("AllTypes", "some_float", id);
      expect(value).toEqual(2.71);
    } finally {
      db.close();
    }
  });

  test("updates string scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      db.updateElement("AllTypes", id, { some_text: "world" });
      const value = db.readScalarStringById("AllTypes", "some_text", id);
      expect(value).toEqual("world");
    } finally {
      db.close();
    }
  });

  test("updates with null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.updateElement("AllTypes", id, { some_integer: null });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      expect(value).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("updates array field", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.updateElement("AllTypes", id, { code: [30, 40, 50] });
      const count = db.queryInteger("SELECT COUNT(*) FROM AllTypes_set_codes WHERE id = ?", [id]);
      expect(count).toEqual(3);
      const minVal = db.queryInteger("SELECT MIN(code) FROM AllTypes_set_codes WHERE id = ?", [id]);
      expect(minVal).toEqual(30);
      const maxVal = db.queryInteger("SELECT MAX(code) FROM AllTypes_set_codes WHERE id = ?", [id]);
      expect(maxVal).toEqual(50);
    } finally {
      db.close();
    }
  });

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => {
      db.updateElement("AllTypes", 1, { some_integer: 42 });
    }).toThrow(QuiverError);

    try {
      db.updateElement("AllTypes", 1, { some_integer: 42 });
    } catch (e) {
      expect((e as QuiverError).message).toContain("Database is closed");
    }
  });
});

describe("updateElementByLabel", () => {
  test("updates by label and leaves siblings alone", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id1 = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const id2 = db.createElement("AllTypes", { label: "Item2", some_integer: 7 });
      db.updateElementByLabel("AllTypes", "Item1", { some_integer: 99 });
      expect(db.readScalarIntegerById("AllTypes", "some_integer", id1)).toEqual(99);
      expect(db.readScalarIntegerById("AllTypes", "some_integer", id2)).toEqual(7);
    } finally {
      db.close();
    }
  });

  test("throws on non-existent label", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      expect(() => db.updateElementByLabel("AllTypes", "Nope", { some_integer: 5 })).toThrow(
        /Element not found/,
      );
    } finally {
      db.close();
    }
  });

  test("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    expect(() => db.updateElementByLabel("AllTypes", "Item1", { some_integer: 42 })).toThrow(
      QuiverError,
    );
  });
});

const RELATIONS_SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "relations.sql",
);

// relations.sql gives Child a vector group and a set group that legally share the FK column name
// "parent_ref" -- the case that makes routing an array by column name ambiguous.
describe("updateVectorGroup / updateSetGroup", () => {
  function openRelations(): { db: Database; parentA: number; parentB: number; child: number } {
    const db = Database.fromSchema(":memory:", RELATIONS_SCHEMA_PATH);
    db.createElement("Configuration", { label: "Config" });
    const parentA = db.createElement("Parent", { label: "Parent A" });
    const parentB = db.createElement("Parent", { label: "Parent B" });
    const child = db.createElement("Child", { label: "Child 1" });
    return { db, parentA, parentB, child };
  }

  test("replaces rows and clears on an empty object", () => {
    const { db, parentA, parentB, child } = openRelations();
    try {
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA, parentB] });
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentA, parentB]);

      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentB] });
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentB]);

      db.updateVectorGroup("Child", "refs", child, {});
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("leaves a sibling group sharing a column name untouched", () => {
    const { db, parentA, parentB, child } = openRelations();
    try {
      db.updateSetGroup("Child", "parents", child, { parent_ref: [parentA] });
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentB] });

      expect(db.readSetIntegersById("Child", "parent_ref", child)).toEqual([parentA]);
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentB]);

      db.updateVectorGroup("Child", "refs", child, {});
      expect(db.readSetIntegersById("Child", "parent_ref", child)).toEqual([parentA]);
    } finally {
      db.close();
    }
  });

  test("resolves foreign key labels", () => {
    const { db, parentB, child } = openRelations();
    try {
      db.updateVectorGroup("Child", "refs", child, { parent_ref: ["Parent B"] });
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentB]);
    } finally {
      db.close();
    }
  });

  test("writes null cells as SQL NULL", () => {
    const { db, parentA, parentB, child } = openRelations();
    try {
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA, null, parentB] });
      // Asserted in SQL: the per-column reader drops NULL cells.
      expect(
        db.queryInteger("SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", [child]),
      ).toEqual(3);
      expect(
        db.queryInteger(
          "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ? AND parent_ref IS NULL",
          [child],
        ),
      ).toEqual(1);
    } finally {
      db.close();
    }
  });

  test("throws on an unknown group or column", () => {
    const { db, parentA, child } = openRelations();
    try {
      expect(() => db.updateVectorGroup("Child", "nope", child, { parent_ref: [parentA] })).toThrow(
        /Vector group not found/,
      );
      expect(() => db.updateSetGroup("Child", "nope", child, { parent_ref: [parentA] })).toThrow(
        /Set group not found/,
      );
      expect(() => db.updateVectorGroup("Child", "refs", child, { not_a_column: [1] })).toThrow(
        /not found in group/,
      );
    } finally {
      db.close();
    }
  });

  test("rejects the columns the group manages itself", () => {
    const { db, parentA, child } = openRelations();
    try {
      // Accepting these silently dropped the caller's value: SQLite keeps the first of a
      // duplicated INSERT column.
      expect(() =>
        db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA], vector_index: [7] }),
      ).toThrow(/managed by the group table/);
      expect(() =>
        db.updateSetGroup("Child", "parents", child, { parent_ref: [parentA], id: [2] }),
      ).toThrow(/managed by the group table/);
    } finally {
      db.close();
    }
  });

  test("rejects a named-but-empty column instead of clearing", () => {
    const { db, parentA, child } = openRelations();
    try {
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA] });
      expect(() => db.updateVectorGroup("Child", "refs", child, { parent_ref: [] })).toThrow(
        /contain no rows/,
      );
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentA]);
    } finally {
      db.close();
    }
  });

  test("throws Element not found for a missing id", () => {
    const { db, parentA } = openRelations();
    try {
      expect(() => db.updateVectorGroup("Child", "refs", 999, { parent_ref: [parentA] })).toThrow(
        /Element not found/,
      );
      // The clear path used to succeed silently: the DELETE simply matched nothing.
      expect(() => db.updateSetGroup("Child", "parents", 999, {})).toThrow(/Element not found/);
    } finally {
      db.close();
    }
  });

  test("rejects jagged columns", () => {
    const { db, parentA, child } = openRelations();
    try {
      expect(() =>
        db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA, parentA], id: [1] }),
      ).toThrow(/has length 1 but expected 2/);
    } finally {
      db.close();
    }
  });

  test("updateVectorGroupByLabel writes and clears only the labelled element", () => {
    const { db, parentA, parentB, child } = openRelations();
    try {
      const other = db.createElement("Child", { label: "Child 2" });

      db.updateVectorGroupByLabel("Child", "refs", "Child 2", { parent_ref: [parentA] });
      db.updateVectorGroupByLabel("Child", "refs", "Child 1", { parent_ref: [parentA, parentB] });
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentA, parentB]);

      db.updateVectorGroupByLabel("Child", "refs", "Child 1", {});
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([]);
      expect(db.readVectorIntegersById("Child", "parent_ref", other)).toEqual([parentA]);
    } finally {
      db.close();
    }
  });

  test("updateVectorGroupByLabel throws on an unresolvable label", () => {
    const { db, parentA, child } = openRelations();
    try {
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA] });

      expect(() =>
        db.updateVectorGroupByLabel("Child", "refs", "Nope", { parent_ref: [parentA] }),
      ).toThrow(/Element not found: label 'Nope' in collection 'Child'/);
      expect(db.readVectorIntegersById("Child", "parent_ref", child)).toEqual([parentA]);
    } finally {
      db.close();
    }
  });

  test("updateSetGroupByLabel writes and clears only the labelled element", () => {
    const { db, parentA, parentB, child } = openRelations();
    try {
      const other = db.createElement("Child", { label: "Child 2" });

      db.updateSetGroupByLabel("Child", "parents", "Child 2", { parent_ref: [parentA] });
      db.updateSetGroupByLabel("Child", "parents", "Child 1", { parent_ref: [parentA, parentB] });
      expect(db.readSetIntegersById("Child", "parent_ref", child)).toEqual([parentA, parentB]);

      db.updateSetGroupByLabel("Child", "parents", "Child 1", {});
      expect(db.readSetIntegersById("Child", "parent_ref", child)).toEqual([]);
      expect(db.readSetIntegersById("Child", "parent_ref", other)).toEqual([parentA]);
    } finally {
      db.close();
    }
  });

  test("updateSetGroupByLabel throws on an unresolvable label", () => {
    const { db, parentA, child } = openRelations();
    try {
      db.updateSetGroup("Child", "parents", child, { parent_ref: [parentA] });

      expect(() =>
        db.updateSetGroupByLabel("Child", "parents", "Nope", { parent_ref: [parentA] }),
      ).toThrow(/Element not found: label 'Nope' in collection 'Child'/);
      expect(db.readSetIntegersById("Child", "parent_ref", child)).toEqual([parentA]);
    } finally {
      db.close();
    }
  });
});

describe("updateRelation", () => {
  function openRelations(): { db: Database; parentA: number; parentB: number; child: number } {
    const db = Database.fromSchema(":memory:", RELATIONS_SCHEMA_PATH);
    db.createElement("Configuration", { label: "Config" });
    const parentA = db.createElement("Parent", { label: "Parent A" });
    const parentB = db.createElement("Parent", { label: "Parent B" });
    const child = db.createElement("Child", { label: "Child 1" });
    return { db, parentA, parentB, child };
  }

  test("sets the derived foreign key by id", () => {
    const { db, parentA, child } = openRelations();
    try {
      db.updateRelation("Child", "Parent", "id", child, "Parent A");
      expect(db.readScalarIntegerById("Child", "parent_id", child)).toEqual(parentA);
    } finally {
      db.close();
    }
  });

  test("updateRelationByLabel resolves the source element", () => {
    const { db, parentB, child } = openRelations();
    try {
      db.updateRelationByLabel("Child", "Parent", "id", "Child 1", "Parent B");
      expect(db.readScalarIntegerById("Child", "parent_id", child)).toEqual(parentB);
    } finally {
      db.close();
    }
  });

  test("a null target label clears the relation, through either form", () => {
    const { db, child } = openRelations();
    try {
      db.updateRelation("Child", "Parent", "id", child, "Parent A");
      db.updateRelation("Child", "Parent", "id", child, null);
      expect(db.readScalarIntegerById("Child", "parent_id", child)).toBeNull();

      db.updateRelationByLabel("Child", "Parent", "id", "Child 1", "Parent A");
      db.updateRelationByLabel("Child", "Parent", "id", "Child 1", null);
      expect(db.readScalarIntegerById("Child", "parent_id", child)).toBeNull();
    } finally {
      db.close();
    }
  });

  test("throws when the relation type derives no such column", () => {
    const { db, child } = openRelations();
    try {
      expect(() => db.updateRelation("Child", "Parent", "owner", child, "Parent A")).toThrow(
        /relation column 'parent_owner' not found in collection 'Child'/,
      );
    } finally {
      db.close();
    }
  });
});
