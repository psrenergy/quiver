import { Database } from "./database.js";
import { check } from "./errors.js";
import { allocPtrOut, decodeStringFromBuf } from "./ffi-helpers.js";
import { getSymbols } from "./loader.js";

declare module "./database.js" {
  interface Database {
    isHealthy(): boolean;
    currentVersion(): number;
    path(): string;
    describe(): void;
  }
}

Database.prototype.isHealthy = function (this: Database): boolean {
  const lib = getSymbols();
  const outHealthy = new Int32Array(1);
  check(lib.quiver_database_is_healthy(this._handle, outHealthy));
  return outHealthy[0] !== 0;
};

Database.prototype.currentVersion = function (this: Database): number {
  const lib = getSymbols();
  const outVersion = new BigInt64Array(1);
  check(lib.quiver_database_current_version(this._handle, outVersion));
  return Number(outVersion[0]);
};

Database.prototype.path = function (this: Database): string {
  const lib = getSymbols();
  const outPath = allocPtrOut();
  check(lib.quiver_database_path(this._handle, outPath));
  return decodeStringFromBuf(outPath);
};

Database.prototype.describe = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_describe(this._handle));
};
