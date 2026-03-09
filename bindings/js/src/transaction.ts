import { ptr } from "bun:ffi";
import { Database } from "./database";
import { getSymbols } from "./loader";
import { check } from "./errors";

declare module "./database" {
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
  const outActive = new Uint8Array(1);
  check(lib.quiver_database_in_transaction(this._handle, ptr(outActive)));
  return outActive[0] !== 0;
};
