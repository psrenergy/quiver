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

describe("readTimeSeriesGroup / updateTimeSeriesGroup (NULL cells)", () => {
  // nullable_time_series.sql: Sensor.readings with date_time TEXT NOT NULL,
  // temperature REAL, counter INTEGER, status TEXT (all value columns nullable).
  test("null cells round-trip through update and read", () => {
    const db = Database.fromSchema(":memory:", NULLABLE_TS_SCHEMA);
    try {
      const id = db.createElement("Sensor", { label: "Sensor1" });

      db.updateTimeSeriesGroup("Sensor", "readings", id, {
        date_time: ["2024-01-01", "2024-01-02"],
        temperature: [10.5, null],
        counter: [null, 7],
        status: ["ok", null],
      });

      const result = db.readTimeSeriesGroup("Sensor", "readings", id);
      expect(result.date_time).toEqual(["2024-01-01", "2024-01-02"]);
      expect(result.temperature).toEqual([10.5, null]);
      expect(result.counter).toEqual([null, 7]);
      expect(result.status).toEqual(["ok", null]);
    } finally {
      db.close();
    }
  });

  test("all-null value column reads back as all nulls", () => {
    const db = Database.fromSchema(":memory:", NULLABLE_TS_SCHEMA);
    try {
      const id = db.createElement("Sensor", { label: "Sensor1" });

      db.updateTimeSeriesGroup("Sensor", "readings", id, {
        date_time: ["2024-01-01", "2024-01-02"],
        status: [null, null],
      });

      const result = db.readTimeSeriesGroup("Sensor", "readings", id);
      expect(result.date_time).toEqual(["2024-01-01", "2024-01-02"]);
      expect(result.status).toEqual([null, null]);
    } finally {
      db.close();
    }
  });

  test("null cells survive a read-modify-write round-trip", () => {
    const db = Database.fromSchema(":memory:", NULLABLE_TS_SCHEMA);
    try {
      const id = db.createElement("Sensor", { label: "Sensor1" });

      db.updateTimeSeriesGroup("Sensor", "readings", id, {
        date_time: ["2024-01-01", "2024-01-02"],
        temperature: [10.5, null],
      });

      const ts = db.readTimeSeriesGroup("Sensor", "readings", id);
      (ts.temperature as (number | null)[])[0] = 99.0;
      db.updateTimeSeriesGroup("Sensor", "readings", id, ts);

      const result = db.readTimeSeriesGroup("Sensor", "readings", id);
      expect(result.temperature).toEqual([99.0, null]);
    } finally {
      db.close();
    }
  });
});
