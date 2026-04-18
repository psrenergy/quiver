import { assertGreater, assertStringIncludes, assertThrows } from "jsr:@std/assert";
import { join } from "jsr:@std/path";
const __dirname = import.meta.dirname!;
import { Database, QuiverError } from "../src/index.ts";
import type { ElementData, Value } from "../src/types.ts";

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

Deno.test({ name: "createElement", sanitizeResources: false }, async (t) => {
  await t.step("creates element with integer scalar and returns numeric ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const data: ElementData = { label: "Item1", some_integer: 42 };
      const id = db.createElement("AllTypes", data);
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with float scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_float: 3.14 });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with string scalar", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_text: "hello" });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with null value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: null });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("skips undefined values", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: undefined });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with bigint value", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42n });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("returns incrementing IDs", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id1 = db.createElement("AllTypes", { label: "Item1" });
      const id2 = db.createElement("AllTypes", { label: "Item2" });
      assertGreater(id2, id1);
    } finally {
      db.close();
    }
  });

  await t.step("throws on unsupported type (boolean)", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      assertThrows(
        () => {
          db.createElement("AllTypes", { label: "Item1", some_integer: true as unknown as Value });
        },
        QuiverError,
      );

      try {
        db.createElement("AllTypes", { label: "Item1", some_integer: true as unknown as Value });
      } catch (e) {
        assertStringIncludes(
          (e as QuiverError).message,
          "Unsupported value type for 'some_integer': boolean",
        );
      }
    } finally {
      db.close();
    }
  });

  await t.step("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    assertThrows(
      () => {
        db.createElement("AllTypes", { label: "Item1" });
      },
      QuiverError,
    );

    try {
      db.createElement("AllTypes", { label: "Item1" });
    } catch (e) {
      assertStringIncludes((e as QuiverError).message, "Database is closed");
    }
  });
});

Deno.test({ name: "createElement with arrays", sanitizeResources: false }, async (t) => {
  await t.step("creates element with integer array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10, 20, 30] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with float array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", weight: [1.5, 2.5] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with string array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", tag: ["a", "b", "c"] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with empty array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", tag: [] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("treats number[] with all integers as integer array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [1, 2, 3] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("treats number[] with any decimal as float array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", weight: [1, 2.5, 3] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });

  await t.step("creates element with bigint array", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", code: [10n, 20n, 30n] });
      assertGreater(id, 0);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "deleteElement", sanitizeResources: false }, async (t) => {
  await t.step("deletes element by ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1" });
      // Should not throw
      db.deleteElement("AllTypes", id);
    } finally {
      db.close();
    }
  });

  await t.step("throws on closed database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    db.close();
    assertThrows(
      () => {
        db.deleteElement("AllTypes", 1);
      },
      QuiverError,
    );

    try {
      db.deleteElement("AllTypes", 1);
    } catch (e) {
      assertStringIncludes((e as QuiverError).message, "Database is closed");
    }
  });
});
