import { ptr } from "bun:ffi";
import { getSymbols } from "./loader";
import { QuiverError, check } from "./errors";
import { makeDefaultOptions, allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";

export class Database {
  private _ptr: number;
  private _closed = false;

  private constructor(nativePtr: number) {
    this._ptr = nativePtr;
  }

  static fromSchema(dbPath: string, schemaPath: string): Database {
    const lib = getSymbols();
    const options = makeDefaultOptions();
    const outDb = allocPointerOut();

    check(lib.quiver_database_from_schema(
      toCString(dbPath),
      toCString(schemaPath),
      ptr(options),
      ptr(outDb),
    ));

    const dbPtr = readPointerOut(outDb);
    return new Database(dbPtr);
  }

  static fromMigrations(dbPath: string, migrationsPath: string): Database {
    const lib = getSymbols();
    const options = makeDefaultOptions();
    const outDb = allocPointerOut();

    check(lib.quiver_database_from_migrations(
      toCString(dbPath),
      toCString(migrationsPath),
      ptr(options),
      ptr(outDb),
    ));

    const dbPtr = readPointerOut(outDb);
    return new Database(dbPtr);
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
  get _handle(): number {
    this.ensureOpen();
    return this._ptr;
  }
}
