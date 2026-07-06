/**
 * @module
 * Quiver - SQLite wrapper with typed FFI bindings for Bun.
 *
 * @example
 * ```ts
 * import { Database } from "quiverdb";
 *
 * const db = Database.fromSchema("my.db", "schema.sql");
 * db.close();
 * ```
 */

export type {
  ArrayValue,
  CsvOptions,
  ElementData,
  GroupMetadata,
  QueryParam,
  ScalarMetadata,
  ScalarValue,
  TimeSeriesData,
  Value,
} from "./src/index.ts";
export { Database, LUA_DB_API_REFERENCE, LuaRunner, QuiverError } from "./src/index.ts";
