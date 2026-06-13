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
