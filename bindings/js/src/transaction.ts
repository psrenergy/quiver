import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { getSymbols } from "./loader.ts";

declare module "./database.ts" {
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
  const outBuf = new Uint8Array(4);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_in_transaction(this._handle, outPtr));
  return new DataView(outBuf.buffer).getInt32(0, true) !== 0;
};
