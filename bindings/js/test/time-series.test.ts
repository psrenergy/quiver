import { assertEquals } from "jsr:@std/assert";
const __dirname = import.meta.dirname!;
import { join } from "jsr:@std/path";
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

Deno.test({ name: "readTimeSeriesGroup / updateTimeSeriesGroup (single-column)", sanitizeResources: false }, async (t) => {
  await t.step("write and read back single-column time series data", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-01-01", "2024-01-02"],
        value: [1.5, 2.5],
      });

      const result = db.readTimeSeriesGroup("Collection", "data", id);

      assertEquals(result.date_time, ["2024-01-01", "2024-01-02"]);
      assertEquals(result.value, [1.5, 2.5]);
    } finally {
      db.close();
    }
  });

  await t.step("returns empty object when element has no time series data", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      const result = db.readTimeSeriesGroup("Collection", "data", id);
      assertEquals(result, {});
    } finally {
      db.close();
    }
  });

  await t.step("overwrite replaces existing rows", () => {
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
      assertEquals(result.date_time, ["2024-06-01", "2024-06-02"]);
      assertEquals(result.value, [10.5, 20.5]);
    } finally {
      db.close();
    }
  });

  await t.step("clear rows with empty object", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const id = db.createElement("Collection", { label: "Item1" });

      db.updateTimeSeriesGroup("Collection", "data", id, {
        date_time: ["2024-01-01"],
        value: [1.5],
      });

      db.updateTimeSeriesGroup("Collection", "data", id, {});

      const result = db.readTimeSeriesGroup("Collection", "data", id);
      assertEquals(result, {});
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "readTimeSeriesGroup / updateTimeSeriesGroup (multi-column)", sanitizeResources: false }, async (t) => {
  await t.step("write and read back multi-column time series with mixed types", () => {
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

      assertEquals(result.date_time, ["2024-01-01T00:00:00", "2024-01-01T01:00:00"]);
      assertEquals(result.temperature, [22.5, 23.1]);
      assertEquals(result.humidity, [60, 65]);
      assertEquals(result.status, ["ok", "warning"]);
    } finally {
      db.close();
    }
  });
});

Deno.test({ name: "time series files", sanitizeResources: false }, async (t) => {
  await t.step("hasTimeSeriesFiles returns true for collection with files table", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      assertEquals(db.hasTimeSeriesFiles("Collection"), true);
    } finally {
      db.close();
    }
  });

  await t.step("listTimeSeriesFilesColumns returns column names", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const columns = db.listTimeSeriesFilesColumns("Collection");
      assertEquals(columns, ["data_file", "metadata_file"]);
    } finally {
      db.close();
    }
  });

  await t.step("readTimeSeriesFiles returns null values when no paths set", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      const result = db.readTimeSeriesFiles("Collection");
      assertEquals(result.data_file, null);
      assertEquals(result.metadata_file, null);
    } finally {
      db.close();
    }
  });

  await t.step("updateTimeSeriesFiles and readTimeSeriesFiles round-trip", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      db.updateTimeSeriesFiles("Collection", {
        data_file: "/path/to/data.qvr",
        metadata_file: "/path/to/metadata.toml",
      });

      const result = db.readTimeSeriesFiles("Collection");
      assertEquals(result.data_file, "/path/to/data.qvr");
      assertEquals(result.metadata_file, "/path/to/metadata.toml");
    } finally {
      db.close();
    }
  });
});
