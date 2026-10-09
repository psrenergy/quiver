import { CString, type Pointer, ptr, read, toArrayBuffer } from "bun:ffi";
import { Database } from "./database.ts";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativePtrTable,
  allocNativeString,
  allocNativeStringArray,
  allocPtrOut,
  allocUint64Out,
  decodeFloat64Array,
  decodeInt64Array,
  decodeStringArray,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import {
  type GroupColumns,
  readGroupColumns,
  type TimeSeriesData,
  updateGroupColumns,
} from "./group-columns.ts";
import { getSymbols, type NativePointer } from "./loader.ts";
import { type Allocation, DATA_TYPE_FLOAT, DATA_TYPE_INTEGER, DATA_TYPE_STRING } from "./types.ts";

Database.prototype.readTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
): TimeSeriesData {
  return readGroupColumns(
    this._handle,
    getSymbols().quiver_database_read_time_series_group,
    collection,
    group,
    id,
  );
};

Database.prototype.readTimeSeriesRow = function (
  this: Database,
  collection: string,
  group: string,
  attribute: string,
  dateTime: string,
): (number | string | null)[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const attrBuf = toCString(attribute);
  const dtBuf = toCString(dateTime);
  const outDataType = new Uint8Array(4);
  const outValues = allocPtrOut();
  const outMask = allocPtrOut();
  const outCount = allocUint64Out();

  check(
    lib.quiver_database_read_time_series_row(
      this._handle,
      collBuf.buf,
      grpBuf.buf,
      attrBuf.buf,
      dtBuf.buf,
      outDataType,
      outValues.buf,
      outMask.buf,
      outCount.buf,
    ),
  );

  const count = readUint64Out(outCount);
  const valuesPtr = readPtrOut(outValues);
  if (count === 0 || !valuesPtr) return [];

  // mask[i] === 0: no data at or before dateTime; the data slot is a placeholder, never read.
  const maskPtr = readPtrOut(outMask);
  const mask = new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, count));
  const dataType = new DataView(outDataType.buffer).getInt32(0, true);
  switch (dataType) {
    case DATA_TYPE_INTEGER: {
      const result = decodeInt64Array(valuesPtr, count).map((v, i) => (mask[i] ? v : null));
      lib.quiver_database_free_integer_array(valuesPtr);
      lib.quiver_database_free_mask(maskPtr);
      return result;
    }
    case DATA_TYPE_FLOAT: {
      const result = decodeFloat64Array(valuesPtr, count).map((v, i) => (mask[i] ? v : null));
      lib.quiver_database_free_float_array(valuesPtr);
      lib.quiver_database_free_mask(maskPtr);
      return result;
    }
    default: {
      // STRING or DATE_TIME; never build a CString from a masked-out (NULL) pointer
      const result: (string | null)[] = new Array(count);
      for (let i = 0; i < count; i++) {
        result[i] = mask[i]
          ? new CString(read.ptr(valuesPtr as Pointer, i * 8) as Pointer).toString()
          : null;
      }
      lib.quiver_database_free_string_array(valuesPtr, BigInt(count));
      lib.quiver_database_free_mask(maskPtr);
      return result;
    }
  }
};

Database.prototype.updateTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  data: GroupColumns,
): void {
  updateGroupColumns(
    this._handle,
    "updateTimeSeriesGroup",
    getSymbols().quiver_database_update_time_series_group,
    collection,
    group,
    id,
    data,
  );
};

/** Label-addressed counterpart of updateTimeSeriesGroup. */
Database.prototype.updateTimeSeriesGroupByLabel = function (
  this: Database,
  collection: string,
  group: string,
  label: string,
  data: GroupColumns,
): void {
  updateGroupColumns(
    this._handle,
    "updateTimeSeriesGroupByLabel",
    getSymbols().quiver_database_update_time_series_group_by_label,
    collection,
    group,
    label,
    data,
  );
};

/**
 * The parallel-array signature both row-oriented time-series upsert C functions share
 * (quiver_database_upsert_time_series_row and its _by_label form). `key` is an id for the by-id
 * form, a NUL-terminated label otherwise.
 */
type UpsertRowFn<Key extends number | string> = (
  db: NativePointer,
  collection: Uint8Array,
  group: Uint8Array,
  key: Key extends string ? Uint8Array : bigint,
  names: Uint8Array | null,
  types: Uint8Array | null,
  data: Uint8Array | null,
  columnCount: bigint,
) => number;

/**
 * Marshal a single row of scalars and forward it to one of the row-oriented time-series upsert
 * C functions. Shared by upsertTimeSeriesRow / upsertTimeSeriesRowByLabel: they differ only in
 * which C entry point they call and whether `key` is an id or a label.
 */
function upsertRowColumns<Key extends number | string>(
  handle: NativePointer,
  caller: string,
  upsert: UpsertRowFn<Key>,
  collection: string,
  group: string,
  key: Key,
  row: Record<string, number | bigint | string | boolean>,
): void {
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const keyArg = (typeof key === "string" ? toCString(key).buf : BigInt(key)) as Key extends string
    ? Uint8Array
    : bigint;
  const entries = Object.entries(row);
  const columnCount = entries.length;
  const keepalive: Allocation[] = [];

  const colNames = entries.map(([name]) => name);
  const { table: namesTable, keepalive: namesPtrs } = allocNativeStringArray(colNames);
  keepalive.push(namesTable, ...namesPtrs);

  const typesBuf = new Uint8Array(columnCount * 4);
  const typesDv = new DataView(typesBuf.buffer);
  const dataPtrs: (Pointer | null)[] = [];

  for (let c = 0; c < columnCount; c++) {
    const [colName, raw] = entries[c];
    // SQLite has no boolean type: a boolean is INTEGER 1/0. Normalized here rather than tested in
    // the INTEGER condition below, because Number.isInteger(true) is false — a boolean otherwise
    // reaches allocNativeFloat64 and lands in the column as FLOAT 1.0, silently the wrong type.
    const value = typeof raw === "boolean" ? (raw ? 1 : 0) : raw;
    if (typeof value === "string") {
      typesDv.setInt32(c * 4, DATA_TYPE_STRING, true);
      const { table, keepalive: strPtrs } = allocNativeStringArray([value]);
      keepalive.push(table, ...strPtrs);
      dataPtrs.push(table.ptr);
    } else if (typeof value === "bigint" || Number.isInteger(value)) {
      typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
      const p = allocNativeInt64([value]);
      keepalive.push(p);
      dataPtrs.push(p.ptr);
    } else if (typeof value === "number") {
      typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
      const p = allocNativeFloat64([value]);
      keepalive.push(p);
      dataPtrs.push(p.ptr);
    } else {
      // Mirrors updateGroupColumns: never let null/undefined/an object fall through to the FLOAT
      // branch, where Number(null) is 0 and anything else is NaN — both written with no error.
      throw new QuiverError(
        `Cannot ${caller}: column '${colName}' has unsupported value type ${typeof value}`,
      );
    }
  }

  const typesAlloc: Allocation = { ptr: ptr(typesBuf), buf: typesBuf };
  keepalive.push(typesAlloc);
  const dataTable = allocNativePtrTable(dataPtrs);
  keepalive.push(dataTable);

  check(
    upsert(
      handle,
      collBuf.buf,
      grpBuf.buf,
      keyArg,
      namesTable.buf,
      typesAlloc.buf,
      dataTable.buf,
      BigInt(columnCount),
    ),
  );
}

Database.prototype.upsertTimeSeriesRow = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  row: Record<string, number | bigint | string | boolean>,
): void {
  upsertRowColumns(
    this._handle,
    "upsertTimeSeriesRow",
    getSymbols().quiver_database_upsert_time_series_row,
    collection,
    group,
    id,
    row,
  );
};

/** Label-addressed counterpart of upsertTimeSeriesRow. */
Database.prototype.upsertTimeSeriesRowByLabel = function (
  this: Database,
  collection: string,
  group: string,
  label: string,
  row: Record<string, number | bigint | string | boolean>,
): void {
  upsertRowColumns(
    this._handle,
    "upsertTimeSeriesRowByLabel",
    getSymbols().quiver_database_upsert_time_series_row_by_label,
    collection,
    group,
    label,
    row,
  );
};

Database.prototype.hasTimeSeriesFiles = function (this: Database, collection: string): boolean {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const outResult = new Uint8Array(4);
  check(lib.quiver_database_has_time_series_files(this._handle, collBuf.buf, outResult));
  return new DataView(outResult.buffer).getInt32(0, true) !== 0;
};

Database.prototype.listTimeSeriesFilesColumns = function (
  this: Database,
  collection: string,
): string[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const outColumns = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    lib.quiver_database_list_time_series_files_columns(
      this._handle,
      collBuf.buf,
      outColumns.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outColumns);
  const result = decodeStringArray(arrPtr, count);
  lib.quiver_database_free_string_array(arrPtr, BigInt(count));
  return result;
};

Database.prototype.readTimeSeriesFiles = function (
  this: Database,
  collection: string,
): Record<string, string | null> {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const outColumns = allocPtrOut();
  const outPaths = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    lib.quiver_database_read_time_series_files(
      this._handle,
      collBuf.buf,
      outColumns.buf,
      outPaths.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return {};
  const colsPtr = readPtrOut(outColumns);
  const pathsPtr = readPtrOut(outPaths);
  const colNames = decodeStringArray(colsPtr, count);
  const paths: (string | null)[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = read.ptr(pathsPtr as Pointer, i * 8);
    paths[i] = strPtr === 0 ? null : new CString(strPtr as Pointer).toString();
  }
  const result: Record<string, string | null> = {};
  for (let i = 0; i < count; i++) {
    result[colNames[i]] = paths[i];
  }
  lib.quiver_database_free_time_series_files(colsPtr, pathsPtr, BigInt(count));
  return result;
};

Database.prototype.updateTimeSeriesFiles = function (
  this: Database,
  collection: string,
  data: Record<string, string | null>,
): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const entries = Object.entries(data);
  // An empty map still reaches the core, which validates the collection and its files table
  // before the no-op. null tables, never zero-length buffers (same as updateGroupColumns).
  if (entries.length === 0) {
    check(lib.quiver_database_update_time_series_files(this._handle, collBuf.buf, null, null, 0n));
    return;
  }
  const keepalive: Allocation[] = [];

  const colNames = entries.map(([name]) => name);
  const { table: colsTable, keepalive: colPtrs } = allocNativeStringArray(colNames);
  keepalive.push(colsTable, ...colPtrs);

  const pathPtrs: (Pointer | null)[] = entries.map(([, value]) => {
    if (value === null) return null;
    const p = allocNativeString(value);
    keepalive.push(p);
    return p.ptr;
  });

  const pathsTable = allocNativePtrTable(pathPtrs);
  keepalive.push(pathsTable);

  check(
    lib.quiver_database_update_time_series_files(
      this._handle,
      collBuf.buf,
      colsTable.buf,
      pathsTable.buf,
      BigInt(entries.length),
    ),
  );
};
