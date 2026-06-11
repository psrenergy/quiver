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

describe("readTimeSeriesGroup / updateTimeSeriesGroup (single-column)", () => {
  test("write and read back single-column time series data", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-01-01", "2024-01-02"],
        value: [1.5, 2.5],
      });

      const result = db.readTimeSeriesGroup("Collection", "data", id);

      expect(result.date_time).toEqual(["2024-01-01", "2024-01-02"]);
      expect(result.value).toEqual([1.5, 2.5]);
    } finally {
      db.close();
    }
  });

  test("returns empty object when element has no time series data", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      const result = db.readTimeSeriesGroup("Collection", "data", id);
      expect(result).toEqual({});
    } finally {
      db.close();
    }
  });

  test("overwrite replaces existing rows", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-01-01"],
        value: [1.5],
      });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-06-01", "2024-06-02"],
        value: [10.5, 20.5],
      });

      const result = db.readTimeSeriesGroup("Collection", "data", id);
      expect(result.date_time).toEqual(["2024-06-01", "2024-06-02"]);
      expect(result.value).toEqual([10.5, 20.5]);
    } finally {
      db.close();
    }
  });

  test("clear rows with empty object", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-01-01"],
        value: [1.5],
      });

      db.updateTimeSeriesGroup("Collection", "data", id, {});

      const result = db.readTimeSeriesGroup("Collection", "data", id);
      expect(result).toEqual({});
    } finally {
      db.close();
    }
  });

  test("throws on jagged columns instead of corrupting the C API call", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      expect(() =>
        db.updateTimeSeriesGroup("Collection", "data", id, {
          date_time: ["2024-01-01", "2024-01-02"],
          value: [1.5],
        }),
      ).toThrow();
    } finally {
      db.close();
    }
  });

  test("throws on named-but-empty columns instead of silently clearing", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-01-01"],
        value: [1.5],
      });

      expect(() =>
        db.updateTimeSeriesGroup("Collection", "data", id, {
          date_time: [],
          value: [],
        }),
      ).toThrow();

      // The existing rows must still be intact (no silent delete).
      const result = db.readTimeSeriesGroup("Collection", "data", id);
      expect(result.date_time).toEqual(["2024-01-01"]);
      expect(result.value).toEqual([1.5]);
    } finally {
      db.close();
    }
  });
});

describe("readTimeSeriesGroup / updateTimeSeriesGroup (multi-column)", () => {
  test("write and read back multi-column time series with mixed types", () => {
    const db = Database.fromSchema(":memory:", MIXED_TS_SCHEMA);
    try {
      const id = db.createElement("Sensor", { label: "Sensor1" });

      db.updateTimeSeriesGroup("Sensor", "readings", id, {
        date_time: ["2024-01-01T00:00:00", "2024-01-01T01:00:00"],
        temperature: [22.5, 23.1],
        humidity: [60, 65],
        status: ["ok", "warning"],
      });

      const result = db.readTimeSeriesGroup("Sensor", "readings", id);

      expect(result.date_time).toEqual(["2024-01-01T00:00:00", "2024-01-01T01:00:00"]);
      expect(result.temperature).toEqual([22.5, 23.1]);
      expect(result.humidity).toEqual([60, 65]);
      expect(result.status).toEqual(["ok", "warning"]);
    } finally {
      db.close();
    }
  });
});

describe("time series files", () => {
  test("hasTimeSeriesFiles returns true for collection with files table", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      expect(db.hasTimeSeriesFiles("Collection")).toEqual(true);
    } finally {
      db.close();
    }
  });

  test("listTimeSeriesFilesColumns returns column names", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const columns = db.listTimeSeriesFilesColumns("Collection");
      expect(columns).toEqual(["data_file", "metadata_file"]);
    } finally {
      db.close();
    }
  });

  test("readTimeSeriesFiles returns null values when no paths set", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const result = db.readTimeSeriesFiles("Collection");
      expect(result.data_file).toEqual(null);
      expect(result.metadata_file).toEqual(null);
    } finally {
      db.close();
    }
  });

  test("updateTimeSeriesFiles and readTimeSeriesFiles round-trip", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      db.updateTimeSeriesFiles("Collection", {
        data_file: "/path/to/data.qvr",
        metadata_file: "/path/to/metadata.toml",
      });

      const result = db.readTimeSeriesFiles("Collection");
      expect(result.data_file).toEqual("/path/to/data.qvr");
      expect(result.metadata_file).toEqual("/path/to/metadata.toml");
    } finally {
      db.close();
    }
  });
});

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
