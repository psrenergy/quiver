import { Database } from "./database.js";
import { check } from "./errors.js";
import { getSymbols } from "./loader.js";

declare module "./database.js" {
  interface Database {
    beginTransaction(): void;
    commit(): void;
    rollback(): void;
    inTransaction(): boolean;
  }
}

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
  const outActive = new Int32Array(1);
  check(lib.quiver_database_in_transaction(this._handle, outActive));
  return outActive[0] !== 0;
};
