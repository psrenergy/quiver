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
// upsertTimeSeriesRow
// ============================================================================

describe("upsertTimeSeriesRow", () => {
  test("appends rows one at a time and upserts on the same date", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.upsertTimeSeriesRow("Collection", "data", id, {
        date_time: "2024-01-01T00:00:00",
        value: 1.5,
      });
      db.upsertTimeSeriesRow("Collection", "data", id, {
        date_time: "2024-01-02T00:00:00",
        value: 2.5,
      });
      // Upsert: same date overwrites the value
      db.upsertTimeSeriesRow("Collection", "data", id, {
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

  test("writes a boolean as INTEGER, not FLOAT", () => {
    const db = Database.fromSchema(":memory:", MIXED_TS_SCHEMA);
    try {
      const id = db.createElement("Sensor", { label: "S1" });

      // humidity is INTEGER: Number.isInteger(true) is false, so a boolean used to fall through to
      // the FLOAT branch and the core rejected a double for an INTEGER column.
      db.upsertTimeSeriesRow("Sensor", "readings", id, {
        date_time: "2024-01-01T00:00:00",
        temperature: 21.5,
        humidity: true,
        status: "ok",
      });

      const result = db.readTimeSeriesGroup("Sensor", "readings", id);
      expect(result.humidity).toEqual([1]);
    } finally {
      db.close();
    }
  });

  test("upsertTimeSeriesRowByLabel writes only the labelled element", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const item = db.createElement("Collection", { label: "Item1" });
      const other = db.createElement("Collection", { label: "Item2" });

      db.upsertTimeSeriesRowByLabel("Collection", "data", "Item2", {
        date_time: "2024-01-01T00:00:00",
        value: 99.0,
      });
      db.upsertTimeSeriesRowByLabel("Collection", "data", "Item1", {
        date_time: "2024-01-01T00:00:00",
        value: 1.5,
      });
      // Same dimension PK upserts rather than appending
      db.upsertTimeSeriesRowByLabel("Collection", "data", "Item1", {
        date_time: "2024-01-01T00:00:00",
        value: 9.5,
      });

      const result = db.readTimeSeriesGroup("Collection", "data", item);
      expect(result.date_time).toEqual(["2024-01-01T00:00:00"]);
      expect(result.value).toEqual([9.5]);
      expect(db.readTimeSeriesGroup("Collection", "data", other).value).toEqual([99.0]);
    } finally {
      db.close();
    }
  });

  test("upsertTimeSeriesRowByLabel throws on an unresolvable label", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      expect(() =>
        db.upsertTimeSeriesRowByLabel("Collection", "data", "Nope", {
          date_time: "2024-01-01T00:00:00",
          value: 1.5,
        }),
      ).toThrow(/Element not found: label 'Nope' in collection 'Collection'/);
      expect(db.readTimeSeriesGroup("Collection", "data", id)).toEqual({});
    } finally {
      db.close();
    }
  });
});
