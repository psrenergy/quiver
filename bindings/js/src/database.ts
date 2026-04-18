import { check, QuiverError } from "./errors.ts";
import { allocPtrOut, makeDefaultOptions, readPtrOut, toCString } from "./ffi-helpers.ts";
import type { NativePointer } from "./loader.ts";
import { getSymbols } from "./loader.ts";

export class Database {
  private _ptr: NativePointer;
  private _closed = false;

  private constructor(nativePtr: NativePointer) {
    this._ptr = nativePtr;
  }

  static fromSchema(dbPath: string, schemaPath: string): Database {
    const lib = getSymbols();
    const options = makeDefaultOptions();
    const outDb = allocPtrOut();
    const dbPathBuf = toCString(dbPath);
    const schemaPathBuf = toCString(schemaPath);

    check(lib.quiver_database_from_schema(dbPathBuf.buf, schemaPathBuf.buf, options.buf, outDb.ptr));

    return new Database(readPtrOut(outDb));
  }

  static fromMigrations(dbPath: string, migrationsPath: string): Database {
    const lib = getSymbols();
    const options = makeDefaultOptions();
    const outDb = allocPtrOut();
    const dbPathBuf = toCString(dbPath);
    const migrPathBuf = toCString(migrationsPath);

    check(lib.quiver_database_from_migrations(dbPathBuf.buf, migrPathBuf.buf, options.buf, outDb.ptr));

    return new Database(readPtrOut(outDb));
  }

  close(): void {
    if (this._closed) return;
    const lib = getSymbols();
    check(lib.quiver_database_close(this._ptr));
    this._closed = true;
  }

  private ensureOpen(): void {
    if (this._closed) {
      throw new QuiverError("Database is closed");
    }
  }

  /** @internal */
  get _handle(): NativePointer {
    this.ensureOpen();
    return this._ptr;
  }
}
