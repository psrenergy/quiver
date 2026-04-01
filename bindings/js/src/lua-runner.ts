import { CString, type Pointer, ptr } from "bun:ffi";
import { Database } from "./database";
import { QuiverError, check } from "./errors";
import { allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

export class LuaRunner {
  private _ptr: Pointer;
  private _db: Database; // prevent GC of database
  private _closed = false;

  constructor(db: Database) {
    this._db = db;
    const lib = getSymbols();
    const outRunner = allocPointerOut();
    check(lib.quiver_lua_runner_new(db._handle, ptr(outRunner)));
    this._ptr = readPointerOut(outRunner);
  }

  run(script: string): void {
    this.ensureOpen();
    const lib = getSymbols();
    const err = lib.quiver_lua_runner_run(this._ptr, toCString(script));
    if (err !== 0) {
      // Try runner-specific error first
      const outError = allocPointerOut();
      const getErr = lib.quiver_lua_runner_get_error(this._ptr, ptr(outError));
      if (getErr === 0) {
        const errPtr = readPointerOut(outError);
        if (errPtr) {
          const msg = new CString(errPtr).toString();
          if (msg) {
            throw new QuiverError(msg);
          }
        }
      }
      // Fall back to global error
      check(err);
    }
  }

  close(): void {
    if (this._closed) return;
    const lib = getSymbols();
    check(lib.quiver_lua_runner_free(this._ptr));
    this._closed = true;
  }

  private ensureOpen(): void {
    if (this._closed) {
      throw new QuiverError("LuaRunner is closed");
    }
  }
}
