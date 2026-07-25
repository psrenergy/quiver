import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { getSymbols } from "./loader.ts";

Database.prototype.beginTransaction = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_begin_transaction(this._handle));
};

Database.prototype.commit = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_commit(this._handle));
};

Database.prototype.rollback = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_rollback(this._handle));
};

Database.prototype.inTransaction = function (this: Database): boolean {
  const lib = getSymbols();
  const outBuf = new Uint8Array(4);
  check(lib.quiver_database_in_transaction(this._handle, outBuf));
  return new DataView(outBuf.buffer).getInt32(0, true) !== 0;
};

/**
 * Begin a dry run: a transaction that endDryRun always rolls back.
 *
 * While it is active, beginTransaction/commit/rollback are absorbed (no-ops), so code that
 * manages its own transactions composes instead of erroring on a nested BEGIN. A nested rollback
 * is therefore not partial -- everything is undone when the dry run ends.
 */
Database.prototype.beginDryRun = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_begin_dry_run(this._handle));
};

Database.prototype.endDryRun = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_end_dry_run(this._handle));
};

Database.prototype.inDryRun = function (this: Database): boolean {
  const lib = getSymbols();
  const outBuf = new Uint8Array(4);
  check(lib.quiver_database_in_dry_run(this._handle, outBuf));
  return new DataView(outBuf.buffer).getInt32(0, true) !== 0;
};
