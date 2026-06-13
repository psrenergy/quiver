import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database } from "../src/index.ts";

const COLLECTIONS_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "collections.sql",
);

const MIXED_TS_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "mixed_time_series.sql",
);

const NULLABLE_TS_SCHEMA = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "nullable_time_series.sql",
);

// ============================================================================
// readTimeSeriesRow
// ============================================================================

describe("readTimeSeriesRow", () => {
  test("returns the last value at or before the date, one entry per element", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id1 = db.createElement("Collection", { label: "Item1" });
      const id2 = db.createElement("Collection", { label: "Item2" });

      db.updateTimeSeriesGroup("Collection", "data", id1, {
        date_time: ["2024-01-01T00:00:00", "2024-02-01T00:00:00"],
        value: [10.5, 20.5],
      });
      db.updateTimeSeriesGroup("Collection", "data", id2, {
        date_time: ["2024-01-01T00:00:00"],
        value: [30.5],
      });

      const row = db.readTimeSeriesRow("Collection", "data", "value", "2024-01-15T00:00:00");
      expect(row).toEqual([10.5, 30.5]);
    } finally {
      db.close();
    }
  });

  test("returns empty array when the collection has no elements", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const row = db.readTimeSeriesRow("Collection", "data", "value", "2024-01-15T00:00:00");
      expect(row).toEqual([]);
    } finally {
      db.close();
    }
  });
});

// ============================================================================
// addTimeSeriesRow
// ============================================================================

describe("addTimeSeriesRow", () => {
  test("appends rows one at a time and upserts on the same date", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.addTimeSeriesRow("Collection", "data", id, {
        date_time: "2024-01-01T00:00:00",
        value: 1.5,
      });
      db.addTimeSeriesRow("Collection", "data", id, {
        date_time: "2024-01-02T00:00:00",
        value: 2.5,
      });
      // Upsert: same date overwrites the value
      db.addTimeSeriesRow("Collection", "data", id, {
        date_time: "2024-01-02T00:00:00",
        value: 9.5,
      });

      const result = db.readTimeSeriesGroup("Collection", "data", id);
      expect(result.date_time).toEqual(["2024-01-01T00:00:00", "2024-01-02T00:00:00"]);
      expect(result.value).toEqual([1.5, 9.5]);
    } finally {
      db.close();
    }
  });
});
