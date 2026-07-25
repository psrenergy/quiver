import type { Database } from "./database.ts";
import { check, QuiverError } from "./errors.ts";
import { allocPtrOut, decodeStringFromBuf, readPtrOut, toCString } from "./ffi-helpers.ts";
import type { NativePointer } from "./loader.ts";
import { getSymbols } from "./loader.ts";

export class LuaRunner {
  private _ptr: NativePointer;
  private _closed = false;

  constructor(db: Database) {
    const lib = getSymbols();
    const outRunner = allocPtrOut();
    check(lib.quiver_lua_runner_new(db._handle, outRunner.buf));
    this._ptr = readPtrOut(outRunner);
  }

  /**
   * Runs a Lua script and returns its return value encoded as JSON, or "" if it returned nothing.
   *
   * To execute a script without keeping its writes, wrap the call in
   * `db.beginDryRun()` / `db.endDryRun()`.
   */
  run(script: string): string {
    this.ensureOpen();
    const lib = getSymbols();
    const scriptBuf = toCString(script);
    const outResult = allocPtrOut();
    check(lib.quiver_lua_runner_run(this._ptr, scriptBuf.buf, outResult.buf));
    const result = decodeStringFromBuf(outResult);
    lib.quiver_lua_runner_free_string(readPtrOut(outResult));
    return result;
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
