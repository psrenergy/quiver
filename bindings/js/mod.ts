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

export * from "./src/index.ts";
