import { assertAlmostEquals, assertEquals } from "jsr:@std/assert";
const __dirname = import.meta.dirname!;
import { join } from "jsr:@std/path";
import { Database } from "../src/index.ts";

const BASIC_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "basic.sql",
);

const COMPOSITE_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "composite_helpers.sql",
);

Deno.test({ name: "readScalarsById", sanitizeResources: false }, async (t) => {
  await t.step("returns all scalar attributes for an element", () => {
    const db = Database.fromSchema(":memory:", BASIC_SCHEMA);
    try {
      db.createElement("Configuration", {
        label: "cfg1",
        integer_attribute: 42,
        float_attribute: 3.14,
        string_attribute: "hello",
        date_attribute: "2024-01-15",
      });

      const scalars = db.readScalarsById("Configuration", 1);
      assertEquals(scalars.integer_attribute, 42);
      assertAlmostEquals(scalars.float_attribute as number, 3.14);
      assertEquals(scalars.string_attribute, "hello");
      assertEquals(scalars.date_attribute, "2024-01-15");
    } finally {
      db.close();
    }
  });

  await t.step("returns null for nullable unset attributes", () => {
    const db = Database.fromSchema(":memory:", BASIC_SCHEMA);
    try {
      db.createElement("Configuration", { label: "cfg1" });

      const scalars = db.readScalarsById("Configuration", 1);
      assertEquals(scalars.float_attribute, null);
      assertEquals(scalars.string_attribute, null);
      assertEquals(scalars.date_attribute, null);
    } finally {
      db.close();
    }
  });

  await t.step("includes label and id in result", () => {
    const db = Database.fromSchema(":memory:", BASIC_SCHEMA);
    try {
      db.createElement("Configuration", { label: "cfg1", integer_attribute: 10 });

      const scalars = db.readScalarsById("Configuration", 1);
      assertEquals(scalars.label, "cfg1");
      assertEquals(scalars.id, 1);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readVectorsById", sanitizeResources: false }, async (t) => {
  await t.step("returns all vector columns for an element", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", {
        label: "item1",
        amount: [10, 20, 30],
        score: [1.1, 2.2],
        note: ["hello", "world"],
      });

      const vectors = db.readVectorsById("Items", 1);
      assertEquals(vectors.amount, [10, 20, 30]);
      assertEquals(vectors.score, [1.1, 2.2]);
      assertEquals(vectors.note, ["hello", "world"]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty arrays for element with no vector data", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", { label: "empty_item" });

      const vectors = db.readVectorsById("Items", 1);
      assertEquals(vectors.amount, []);
      assertEquals(vectors.score, []);
      assertEquals(vectors.note, []);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readSetsById", sanitizeResources: false }, async (t) => {
  await t.step("returns all set columns for an element", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", {
        label: "item1",
        code: [1, 2, 3],
        weight: [1.1, 2.2],
        tag: ["a", "b"],
      });

      const sets = db.readSetsById("Items", 1);
      assertEquals(sets.code, [1, 2, 3]);
      assertEquals(sets.weight, [1.1, 2.2]);
      assertEquals(sets.tag, ["a", "b"]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty arrays for element with no set data", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA);
    try {
      db.createElement("Items", { label: "empty_item" });

      const sets = db.readSetsById("Items", 1);
      assertEquals(sets.code, []);
      assertEquals(sets.weight, []);
      assertEquals(sets.tag, []);
    } finally {
      db.close();
    }
  });
});
