import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { allocPtrOut, decodeStringFromBuf, toCString } from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";

Database.prototype.isHealthy = function (this: Database): boolean {
  const lib = getSymbols();
  const outBuf = new Uint8Array(4);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_is_healthy(this._handle, outPtr));
  return new DataView(outBuf.buffer).getInt32(0, true) !== 0;
};

Database.prototype.currentVersion = function (this: Database): number {
  const lib = getSymbols();
  const outBuf = new Uint8Array(8);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_current_version(this._handle, outPtr));
  return Number(new DataView(outBuf.buffer).getBigInt64(0, true));
};

Database.prototype.path = function (this: Database): string {
  const lib = getSymbols();
  const outPath = allocPtrOut();
  check(lib.quiver_database_path(this._handle, outPath.ptr));
  return decodeStringFromBuf(outPath);
};

Database.prototype.describe = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_describe(this._handle));
};
