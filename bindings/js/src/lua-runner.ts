import { Database } from "./database.ts";
import { QuiverError, check } from "./errors.ts";
import { allocPtrOut, decodeStringFromBuf, readPtrOut, toCString } from "./ffi-helpers.ts";
import type { NativePointer } from "./loader.ts";
import { getSymbols } from "./loader.ts";

export class LuaRunner {
  private _ptr: NativePointer;
  private _db: Database;
  private _closed = false;

  constructor(db: Database) {
    this._db = db;
    const lib = getSymbols();
    const outRunner = allocPtrOut();
    check(lib.quiver_lua_runner_new(db._handle, outRunner.ptr));
    this._ptr = readPtrOut(outRunner);
  }

  run(script: string): void {
    this.ensureOpen();
    const lib = getSymbols();
    const scriptBuf = toCString(script);
    const err = lib.quiver_lua_runner_run(this._ptr, scriptBuf.buf);
    if (err !== 0) {
      const outError = allocPtrOut();
      const getErr = lib.quiver_lua_runner_get_error(this._ptr, outError.ptr);
      if (getErr === 0 && readPtrOut(outError)) {
        const msg = decodeStringFromBuf(outError);
        if (msg) throw new QuiverError(msg);
      }
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
