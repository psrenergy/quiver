import { describe, expect, test } from "bun:test";
import { join } from "node:path";
import * as quiver from "../mod.ts";

const SCHEMA_PATH = join(
  import.meta.dir,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "basic.sql",
);

describe("package entry (mod.ts)", () => {
  test("exports the public classes", () => {
    expect(typeof quiver.Database).toEqual("function");
    expect(typeof quiver.LuaRunner).toEqual("function");
    expect(typeof quiver.QuiverError).toEqual("function");
    expect(typeof quiver.LUA_DB_API_REFERENCE).toEqual("string");
  });

  test("exports the log-level constants", () => {
    expect(quiver.LOG_LEVEL_DEBUG).toEqual(0);
    expect(quiver.LOG_LEVEL_INFO).toEqual(1);
    expect(quiver.LOG_LEVEL_WARN).toEqual(2);
    expect(quiver.LOG_LEVEL_ERROR).toEqual(3);
    expect(quiver.LOG_LEVEL_OFF).toEqual(4);
  });

  test("exports the data-type constants matching quiver_data_type_t", () => {
    expect(quiver.DATA_TYPE_INTEGER).toEqual(0);
    expect(quiver.DATA_TYPE_FLOAT).toEqual(1);
    expect(quiver.DATA_TYPE_STRING).toEqual(2);
    expect(quiver.DATA_TYPE_DATE_TIME).toEqual(3);
    expect(quiver.DATA_TYPE_NULL).toEqual(4);
  });

  test("a DatabaseOptions value built from exported constants opens a database", () => {
    const options: quiver.DatabaseOptions = { consoleLevel: quiver.LOG_LEVEL_OFF };
    const db = quiver.Database.fromSchema(":memory:", SCHEMA_PATH, options);
    try {
      const dataType = (attribute: string) =>
        db.getScalarMetadata("Configuration", attribute).dataType;
      expect(dataType("integer_attribute")).toEqual(quiver.DATA_TYPE_INTEGER);
      expect(dataType("float_attribute")).toEqual(quiver.DATA_TYPE_FLOAT);
      expect(dataType("string_attribute")).toEqual(quiver.DATA_TYPE_STRING);
      expect(dataType("date_attribute")).toEqual(quiver.DATA_TYPE_DATE_TIME);
    } finally {
      db.close();
    }
  });
});
