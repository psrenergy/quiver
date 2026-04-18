import { assert, assertEquals } from "jsr:@std/assert";
import { join } from "jsr:@std/path";
const __dirname = import.meta.dirname!;
import { Database, QuiverError } from "../src/index.ts";

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

Deno.test({ name: "readScalarIntegers / readScalarFloats / readScalarStrings", sanitizeResources: false }, async (t) => {
  await t.step("reads integer scalars from collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      db.createElement("AllTypes", { label: "Item2", some_integer: 99 });
      const values = db.readScalarIntegers("AllTypes", "some_integer");
      assertEquals(values, [42, 99]);
    } finally {
      db.close();
    }
  });

  await t.step("reads float scalars from collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      db.createElement("AllTypes", { label: "Item2", some_float: 2.71 });
      const values = db.readScalarFloats("AllTypes", "some_float");
      assertEquals(values, [3.14, 2.71]);
    } finally {
      db.close();
    }
  });

  await t.step("reads string scalars from collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      db.createElement("AllTypes", { label: "Item2", some_text: "world" });
      const values = db.readScalarStrings("AllTypes", "some_text");
      assertEquals(values, ["hello", "world"]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readScalarIntegers("AllTypes", "some_integer");
      assertEquals(values, []);
    } finally {
      db.close();
    }
  });

  await t.step("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    let threw = false;
    try {
      db.readScalarIntegers("AllTypes", "some_integer");
    } catch (e) {
      threw = true;
      assert(e instanceof QuiverError);
      assertEquals((e as QuiverError).message, "Database is closed");
    }
    assert(threw, "Expected QuiverError to be thrown");
  });
});

Deno.test({ name: "readScalarIntegerById / readScalarFloatById / readScalarStringById", sanitizeResources: false }, async (t) => {
  await t.step("reads integer by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      assertEquals(value, 42);
    } finally {
      db.close();
    }
  });

  await t.step("returns null for non-existent id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const value = db.readScalarIntegerById("AllTypes", "some_integer", 9999);
      assertEquals(value, null);
    } finally {
      db.close();
    }
  });

  await t.step("returns null for null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const value = db.readScalarIntegerById("AllTypes", "some_integer", id);
      assertEquals(value, null);
    } finally {
      db.close();
    }
  });

  await t.step("reads float by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      const value = db.readScalarFloatById("AllTypes", "some_float", id);
      assertEquals(value, 3.14);
    } finally {
      db.close();
    }
  });

  await t.step("reads string by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      const value = db.readScalarStringById("AllTypes", "some_text", id);
      assertEquals(value, "hello");
    } finally {
      db.close();
    }
  });

  await t.step("returns null for non-existent string id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const value = db.readScalarStringById("AllTypes", "some_text", 9999);
      assertEquals(value, null);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readElementIds", sanitizeResources: false }, async (t) => {
  await t.step("reads all element ids", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id1 = db.createElement("AllTypes", { label: "Item1" });
      const id2 = db.createElement("AllTypes", { label: "Item2" });
      const ids = db.readElementIds("AllTypes");
      assertEquals(ids.length, 2);
      assert(ids.includes(id1));
      assert(ids.includes(id2));
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const ids = db.readElementIds("AllTypes");
      assertEquals(ids, []);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readVectorIntegers / readVectorFloats / readVectorStrings", sanitizeResources: false }, async (t) => {
  await t.step("reads integer vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", count_value: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", count_value: [30, 40, 50] });
      const values = db.readVectorIntegers("AllTypes", "count_value");
      assertEquals(values, [[10, 20], [30, 40, 50]]);
    } finally {
      db.close();
    }
  });

  await t.step("reads float vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", score: [1.5, 2.5] });
      db.createElement("AllTypes", { label: "Item2", score: [3.5] });
      const values = db.readVectorFloats("AllTypes", "score");
      assertEquals(values, [[1.5, 2.5], [3.5]]);
    } finally {
      db.close();
    }
  });

  await t.step("reads string vectors bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", label_value: ["a", "b"] });
      db.createElement("AllTypes", { label: "Item2", label_value: ["c"] });
      const values = db.readVectorStrings("AllTypes", "label_value");
      assertEquals(values, [["a", "b"], ["c"]]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readVectorIntegers("AllTypes", "count_value");
      assertEquals(values, []);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readVectorIntegersById / readVectorFloatsById / readVectorStringsById", sanitizeResources: false }, async (t) => {
  await t.step("reads integer vector by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", count_value: [10, 20, 30] });
      const values = db.readVectorIntegersById("AllTypes", "count_value", id);
      assertEquals(values, [10, 20, 30]);
    } finally {
      db.close();
    }
  });

  await t.step("reads float vector by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", score: [1.5, 2.5] });
      const values = db.readVectorFloatsById("AllTypes", "score", id);
      assertEquals(values, [1.5, 2.5]);
    } finally {
      db.close();
    }
  });

  await t.step("reads string vector by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", label_value: ["x", "y"] });
      const values = db.readVectorStringsById("AllTypes", "label_value", id);
      assertEquals(values, ["x", "y"]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for element with no vector data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const values = db.readVectorIntegersById("AllTypes", "count_value", id);
      assertEquals(values, []);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for non-existent id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readVectorIntegersById("AllTypes", "count_value", 9999);
      assertEquals(values, []);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readSetIntegers / readSetFloats / readSetStrings", sanitizeResources: false }, async (t) => {
  await t.step("reads integer sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", code: [10, 20] });
      db.createElement("AllTypes", { label: "Item2", code: [30, 40] });
      const values = db.readSetIntegers("AllTypes", "code");
      assertEquals(values.length, 2);
      assertEquals(values[0].sort(), [10, 20]);
      assertEquals(values[1].sort(), [30, 40]);
    } finally {
      db.close();
    }
  });

  await t.step("reads float sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", weight: [1.5, 2.5] });
      db.createElement("AllTypes", { label: "Item2", weight: [3.5] });
      const values = db.readSetFloats("AllTypes", "weight");
      assertEquals(values.length, 2);
      assertEquals(values[0].sort(), [1.5, 2.5]);
      assertEquals(values[1], [3.5]);
    } finally {
      db.close();
    }
  });

  await t.step("reads string sets bulk", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      db.createElement("AllTypes", { label: "Item1", tag: ["a", "b"] });
      db.createElement("AllTypes", { label: "Item2", tag: ["c"] });
      const values = db.readSetStrings("AllTypes", "tag");
      assertEquals(values.length, 2);
      assertEquals(values[0].sort(), ["a", "b"]);
      assertEquals(values[1], ["c"]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for empty collection", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const values = db.readSetIntegers("AllTypes", "code");
      assertEquals(values, []);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readSetIntegersById / readSetFloatsById / readSetStringsById", sanitizeResources: false }, async (t) => {
  await t.step("reads integer set by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20, 30] });
      const values = db.readSetIntegersById("AllTypes", "code", id);
      assertEquals(values.sort(), [10, 20, 30]);
    } finally {
      db.close();
    }
  });

  await t.step("reads float set by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", weight: [1.5, 2.5] });
      const values = db.readSetFloatsById("AllTypes", "weight", id);
      assertEquals(values.sort(), [1.5, 2.5]);
    } finally {
      db.close();
    }
  });

  await t.step("reads string set by id", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", tag: ["x", "y"] });
      const values = db.readSetStringsById("AllTypes", "tag", id);
      assertEquals(values.sort(), ["x", "y"]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty array for element with no set data", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      const values = db.readSetIntegersById("AllTypes", "code", id);
      assertEquals(values, []);
    } finally {
      db.close();
    }
  });
});
