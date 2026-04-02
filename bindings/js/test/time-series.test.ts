import { describe, expect, test } from "vitest";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
const __dirname = dirname(fileURLToPath(import.meta.url));
import { join } from "node:path";
import { Database } from "../src/index";

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
      expect(db.hasTimeSeriesFiles("Collection")).toBe(true);
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
      expect(result.data_file).toBeNull();
      expect(result.metadata_file).toBeNull();
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
      expect(result.data_file).toBe("/path/to/data.qvr");
      expect(result.metadata_file).toBe("/path/to/metadata.toml");
    } finally {
      db.close();
    }
  });
});
