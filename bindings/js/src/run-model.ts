import { ptr } from "bun:ffi";
import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { getSymbols } from "./loader.ts";

Database.prototype.runModel = function (this: Database): number {
  const lib = getSymbols();
  const outBuf = new Uint8Array(4);
  const outPtr = ptr(outBuf);
  check(lib.quiver_database_run_model(this._handle, outPtr));
  return new DataView(outBuf.buffer).getInt32(0, true);
};
