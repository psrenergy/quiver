import type { NativePointer } from "./loader.js";
import { check, QuiverError } from "./errors.js";
import { allocPtrOut, makeDefaultOptions, readPtrOut } from "./ffi-helpers.js";
import { getSymbols } from "./loader.js";

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

    check(lib.quiver_database_from_schema(dbPath, schemaPath, options, outDb));

    return new Database(readPtrOut(outDb));
  }

  static fromMigrations(dbPath: string, migrationsPath: string): Database {
    const lib = getSymbols();
    const options = makeDefaultOptions();
    const outDb = allocPtrOut();

    check(lib.quiver_database_from_migrations(dbPath, migrationsPath, options, outDb));

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
