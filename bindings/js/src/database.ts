import { check, QuiverError } from "./errors.ts";
import { allocPtrOut, makeDefaultOptions, readPtrOut, toCString } from "./ffi-helpers.ts";
import type { NativePointer } from "./loader.ts";
import { getSymbols } from "./loader.ts";
import type { CsvOptions } from "./csv.ts";
import type { GroupMetadata, ScalarMetadata } from "./metadata.ts";
import type { TimeSeriesData } from "./time-series.ts";
import type { ElementData, QueryParam } from "./types.ts";

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

  // --- Element CRUD (implemented in create.ts) ---
  declare createElement: (collection: string, data: ElementData) => number;
  declare updateElement: (collection: string, id: number, data: ElementData) => void;
  declare deleteElement: (collection: string, id: number) => void;

  // --- Reads (implemented in read.ts) ---
  declare readScalarIntegers: (collection: string, attribute: string) => number[];
  declare readScalarFloats: (collection: string, attribute: string) => number[];
  declare readScalarStrings: (collection: string, attribute: string) => string[];
  declare readScalarIntegerById: (collection: string, attribute: string, id: number) => number | null;
  declare readScalarFloatById: (collection: string, attribute: string, id: number) => number | null;
  declare readScalarStringById: (collection: string, attribute: string, id: number) => string | null;
  declare readElementIds: (collection: string) => number[];
  declare readVectorIntegers: (collection: string, attribute: string) => number[][];
  declare readVectorFloats: (collection: string, attribute: string) => number[][];
  declare readVectorStrings: (collection: string, attribute: string) => string[][];
  declare readVectorIntegersById: (collection: string, attribute: string, id: number) => number[];
  declare readVectorFloatsById: (collection: string, attribute: string, id: number) => number[];
  declare readVectorStringsById: (collection: string, attribute: string, id: number) => string[];
  declare readSetIntegers: (collection: string, attribute: string) => number[][];
  declare readSetFloats: (collection: string, attribute: string) => number[][];
  declare readSetStrings: (collection: string, attribute: string) => string[][];
  declare readSetIntegersById: (collection: string, attribute: string, id: number) => number[];
  declare readSetFloatsById: (collection: string, attribute: string, id: number) => number[];
  declare readSetStringsById: (collection: string, attribute: string, id: number) => string[];

  // --- Queries (implemented in query.ts) ---
  declare queryString: (sql: string, params?: QueryParam[]) => string | null;
  declare queryInteger: (sql: string, params?: QueryParam[]) => number | null;
  declare queryFloat: (sql: string, params?: QueryParam[]) => number | null;

  // --- Transactions (implemented in transaction.ts) ---
  declare beginTransaction: () => void;
  declare commit: () => void;
  declare rollback: () => void;
  declare inTransaction: () => boolean;

  // --- Metadata (implemented in metadata.ts) ---
  declare getScalarMetadata: (collection: string, attribute: string) => ScalarMetadata;
  declare getVectorMetadata: (collection: string, groupName: string) => GroupMetadata;
  declare getSetMetadata: (collection: string, groupName: string) => GroupMetadata;
  declare getTimeSeriesMetadata: (collection: string, groupName: string) => GroupMetadata;
  declare listScalarAttributes: (collection: string) => ScalarMetadata[];
  declare listVectorGroups: (collection: string) => GroupMetadata[];
  declare listSetGroups: (collection: string) => GroupMetadata[];
  declare listTimeSeriesGroups: (collection: string) => GroupMetadata[];

  // --- Time series (implemented in time-series.ts) ---
  declare readTimeSeriesGroup: (collection: string, group: string, id: number) => TimeSeriesData;
  declare updateTimeSeriesGroup: (collection: string, group: string, id: number, data: TimeSeriesData) => void;
  declare hasTimeSeriesFiles: (collection: string) => boolean;
  declare listTimeSeriesFilesColumns: (collection: string) => string[];
  declare readTimeSeriesFiles: (collection: string) => Record<string, string | null>;
  declare updateTimeSeriesFiles: (collection: string, data: Record<string, string | null>) => void;

  // --- CSV (implemented in csv.ts) ---
  declare exportCsv: (collection: string, group: string, filePath: string, options?: CsvOptions) => void;
  declare importCsv: (collection: string, group: string, filePath: string, options?: CsvOptions) => void;

  // --- Introspection (implemented in introspection.ts) ---
  declare isHealthy: () => boolean;
  declare currentVersion: () => number;
  declare path: () => string;
  declare describe: () => void;

  // --- Composite helpers (implemented in composites.ts) ---
  declare readScalarsById: (collection: string, id: number) => Record<string, number | string | null>;
  declare readVectorsById: (collection: string, id: number) => Record<string, number[] | string[]>;
  declare readSetsById: (collection: string, id: number) => Record<string, number[] | string[]>;
}
