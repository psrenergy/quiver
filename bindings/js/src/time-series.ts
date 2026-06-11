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
  decodePtrArray,
  decodeStringArray,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";
import {
  type Allocation,
  DATA_TYPE_DATE_TIME,
  DATA_TYPE_FLOAT,
  DATA_TYPE_INTEGER,
  DATA_TYPE_STRING,
} from "./types.ts";

export type TimeSeriesData = Record<string, (number | string)[]>;

Database.prototype.readTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
): TimeSeriesData {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const outNames = allocPtrOut();
  const outTypes = allocPtrOut();
  const outData = allocPtrOut();
  const outColCount = allocUint64Out();
  const outRowCount = allocUint64Out();

  check(
    lib.quiver_database_read_time_series_group(
      this._handle,
      collBuf.buf,
      grpBuf.buf,
      BigInt(id),
      outNames.buf,
      outTypes.buf,
      outData.buf,
      outColCount.buf,
      outRowCount.buf,
    ),
  );

  const colCount = readUint64Out(outColCount);
  const rowCount = readUint64Out(outRowCount);
  if (colCount === 0) return {};

  const namesPtr = readPtrOut(outNames);
  const typesPtr = readPtrOut(outTypes);
  const dataPtr = readPtrOut(outData);

  const colNames = decodeStringArray(namesPtr, colCount);
  const typesAb = toArrayBuffer(typesPtr as Pointer, 0, colCount * 4);
  const types = Array.from(new Int32Array(typesAb));
  const dataPtrs = decodePtrArray(dataPtr, colCount);

  const result: TimeSeriesData = {};
  for (let c = 0; c < colCount; c++) {
    const colName = colNames[c];
    switch (types[c]) {
      case DATA_TYPE_INTEGER:
        result[colName] = decodeInt64Array(dataPtrs[c], rowCount);
        break;
      case DATA_TYPE_FLOAT:
        result[colName] = decodeFloat64Array(dataPtrs[c], rowCount);
        break;
      case DATA_TYPE_STRING:
      case DATA_TYPE_DATE_TIME:
        result[colName] = decodeStringArray(dataPtrs[c], rowCount);
        break;
    }
  }

  lib.quiver_database_free_time_series_data(
    namesPtr,
    typesPtr,
    dataPtr,
    BigInt(colCount),
    BigInt(rowCount),
  );
  return result;
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
      outCount.buf,
    ),
  );

  const count = readUint64Out(outCount);
  const valuesPtr = readPtrOut(outValues);
  if (count === 0 || !valuesPtr) return [];

  const dataType = new DataView(outDataType.buffer).getInt32(0, true);
  switch (dataType) {
    case DATA_TYPE_INTEGER: {
      const result = decodeInt64Array(valuesPtr, count);
      lib.quiver_database_free_integer_array(valuesPtr);
      return result;
    }
    case DATA_TYPE_FLOAT: {
      const result = decodeFloat64Array(valuesPtr, count);
      lib.quiver_database_free_float_array(valuesPtr);
      return result;
    }
    default: {
      // STRING or DATE_TIME; NULL entries mark elements with no data
      const result: (string | null)[] = new Array(count);
      for (let i = 0; i < count; i++) {
        const strPtr = read.ptr(valuesPtr as Pointer, i * 8);
        result[i] = strPtr === 0 ? null : new CString(strPtr as Pointer).toString();
      }
      lib.quiver_database_free_string_array(valuesPtr, BigInt(count));
      return result;
    }
  }
};

Database.prototype.updateTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  data: TimeSeriesData,
): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const entries = Object.entries(data);

  if (entries.length === 0) {
    check(
      lib.quiver_database_update_time_series_group(
        this._handle,
        collBuf.buf,
        grpBuf.buf,
        BigInt(id),
        null,
        null,
        null,
        0n,
        0n,
      ),
    );
    return;
  }

  const columnCount = entries.length;
  const rowCount = entries[0][1].length;

  // Validate before marshalling: a jagged column would desync the parallel
  // arrays the C API reads against row_count, and a zero-length column (with
  // columns present) would otherwise marshal a null data pointer that the C API
  // dereferences. Named-but-empty columns are a caller mistake -- pass {} to
  // clear the group instead.
  for (const [name, values] of entries) {
    if (values.length !== rowCount) {
      throw new QuiverError(
        `Cannot updateTimeSeriesGroup: column '${name}' has length ${values.length} but expected ${rowCount}`,
      );
    }
  }
  if (rowCount === 0) {
    const names = entries.map(([name]) => name).join(", ");
    throw new QuiverError(
      `Cannot updateTimeSeriesGroup: columns [${names}] contain no rows; pass {} to clear the group`,
    );
  }

  const keepalive: Allocation[] = [];

  // Build column names as native string array
  const colNames = entries.map(([name]) => name);
  const { table: namesTable, keepalive: namesPtrs } = allocNativeStringArray(colNames);
  keepalive.push(namesTable, ...namesPtrs);

  // Build column types and data
  const typesBuf = new Uint8Array(columnCount * 4);
  const typesDv = new DataView(typesBuf.buffer);
  const dataPtrs: (Pointer | null)[] = [];

  for (let c = 0; c < columnCount; c++) {
    const values = entries[c][1];

    if (typeof values[0] === "string") {
      typesDv.setInt32(c * 4, DATA_TYPE_STRING, true);
      const { table, keepalive: strPtrs } = allocNativeStringArray(values as string[]);
      keepalive.push(table, ...strPtrs);
      dataPtrs.push(table.ptr);
    } else if (typeof values[0] === "number") {
      const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
      if (allIntegers) {
        typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
        const p = allocNativeInt64(values as number[]);
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      } else {
        typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
        const p = allocNativeFloat64(values as number[]);
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      }
    }
  }

  const typesAlloc: Allocation = { ptr: ptr(typesBuf), buf: typesBuf };
  keepalive.push(typesAlloc);
  const dataTable = allocNativePtrTable(dataPtrs);
  keepalive.push(dataTable);

  check(
    lib.quiver_database_update_time_series_group(
      this._handle,
      collBuf.buf,
      grpBuf.buf,
      BigInt(id),
      namesTable.buf,
      typesAlloc.buf,
      dataTable.buf,
      BigInt(columnCount),
      BigInt(rowCount),
    ),
  );
};

Database.prototype.addTimeSeriesRow = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  row: Record<string, number | bigint | string>,
): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
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
    const value = entries[c][1];
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
    } else {
      typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
      const p = allocNativeFloat64([value as number]);
      keepalive.push(p);
      dataPtrs.push(p.ptr);
    }
  }

  const typesAlloc: Allocation = { ptr: ptr(typesBuf), buf: typesBuf };
  keepalive.push(typesAlloc);
  const dataTable = allocNativePtrTable(dataPtrs);
  keepalive.push(dataTable);

  check(
    lib.quiver_database_add_time_series_row(
      this._handle,
      collBuf.buf,
      grpBuf.buf,
      BigInt(id),
      namesTable.buf,
      typesAlloc.buf,
      dataTable.buf,
      BigInt(columnCount),
    ),
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
  if (entries.length === 0) return;
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
