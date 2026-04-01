import { CString, ptr, toArrayBuffer } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { allocPointerOut, readPointerOut, readPtrAt, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

export type TimeSeriesData = Record<string, (number | string)[]>;

declare module "./database" {
  interface Database {
    readTimeSeriesGroup(collection: string, group: string, id: number): TimeSeriesData;
    updateTimeSeriesGroup(
      collection: string,
      group: string,
      id: number,
      data: TimeSeriesData,
    ): void;
    hasTimeSeriesFiles(collection: string): boolean;
    listTimeSeriesFilesColumns(collection: string): string[];
    readTimeSeriesFiles(collection: string): Record<string, string | null>;
    updateTimeSeriesFiles(collection: string, data: Record<string, string | null>): void;
  }
}

// C API data type enum values
const DATA_TYPE_INTEGER = 0;
const DATA_TYPE_FLOAT = 1;
const DATA_TYPE_STRING = 2;
const DATA_TYPE_DATE_TIME = 3;

Database.prototype.readTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
): TimeSeriesData {
  const lib = getSymbols();
  const handle = this._handle;

  const outColumnNames = allocPointerOut();
  const outColumnTypes = allocPointerOut();
  const outColumnData = allocPointerOut();
  const outColumnCount = new BigUint64Array(1);
  const outRowCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_time_series_group(
      handle,
      toCString(collection),
      toCString(group),
      id,
      ptr(outColumnNames),
      ptr(outColumnTypes),
      ptr(outColumnData),
      ptr(outColumnCount),
      ptr(outRowCount),
    ),
  );

  const colCount = Number(outColumnCount[0]);
  const rowCount = Number(outRowCount[0]);

  if (colCount === 0) return {};

  const namesPtr = readPointerOut(outColumnNames);
  const typesPtr = readPointerOut(outColumnTypes);
  const dataPtr = readPointerOut(outColumnData);

  const typesBuf = toArrayBuffer(typesPtr, 0, colCount * 4);
  const types = new Int32Array(typesBuf);

  const result: TimeSeriesData = {};

  for (let c = 0; c < colCount; c++) {
    const namePtr = readPtrAt(namesPtr, c * 8);
    const colName = new CString(namePtr).toString();
    const colDataPtr = readPtrAt(dataPtr, c * 8);

    switch (types[c]) {
      case DATA_TYPE_INTEGER: {
        const buf = toArrayBuffer(colDataPtr, 0, rowCount * 8);
        const bigInts = new BigInt64Array(buf);
        const arr: number[] = new Array(rowCount);
        for (let r = 0; r < rowCount; r++) {
          arr[r] = Number(bigInts[r]);
        }
        result[colName] = arr;
        break;
      }
      case DATA_TYPE_FLOAT: {
        const buf = toArrayBuffer(colDataPtr, 0, rowCount * 8);
        result[colName] = Array.from(new Float64Array(buf));
        break;
      }
      case DATA_TYPE_STRING:
      case DATA_TYPE_DATE_TIME: {
        const arr: string[] = new Array(rowCount);
        for (let r = 0; r < rowCount; r++) {
          const strPtr = readPtrAt(colDataPtr, r * 8);
          arr[r] = new CString(strPtr).toString();
        }
        result[colName] = arr;
        break;
      }
    }
  }

  lib.quiver_database_free_time_series_data(namesPtr, typesPtr, dataPtr, colCount, rowCount);
  return result;
};

Database.prototype.updateTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  data: TimeSeriesData,
): void {
  const lib = getSymbols();
  const handle = this._handle;

  const entries = Object.entries(data);

  if (entries.length === 0) {
    check(
      lib.quiver_database_update_time_series_group(
        handle,
        toCString(collection),
        toCString(group),
        id,
        null,
        null,
        null,
        0,
        0,
      ),
    );
    return;
  }

  const columnCount = entries.length;
  const rowCount = entries[0][1].length;
  const keepalive: unknown[] = [];

  // Build column names pointer table
  const namesPtrTable = new BigInt64Array(columnCount);
  const typesArr = new Int32Array(columnCount);
  const dataPtrTable = new BigInt64Array(columnCount);

  for (let c = 0; c < columnCount; c++) {
    const [colName, values] = entries[c];

    // Column name
    const nameBuf = toCString(colName);
    keepalive.push(nameBuf);
    namesPtrTable[c] = BigInt(ptr(nameBuf));

    // Determine type and build data array
    if (values.length === 0) {
      // Empty column: treat as string type with null data
      typesArr[c] = DATA_TYPE_STRING;
      dataPtrTable[c] = 0n;
    } else if (typeof values[0] === "string") {
      typesArr[c] = DATA_TYPE_STRING;
      const strBufs = (values as string[]).map((s) => toCString(s));
      keepalive.push(strBufs);
      const strPtrTable = new BigInt64Array(values.length);
      for (let r = 0; r < values.length; r++) {
        strPtrTable[r] = BigInt(ptr(strBufs[r]));
      }
      keepalive.push(strPtrTable);
      dataPtrTable[c] = BigInt(ptr(strPtrTable));
    } else if (typeof values[0] === "number") {
      const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
      if (allIntegers) {
        typesArr[c] = DATA_TYPE_INTEGER;
        const intArr = BigInt64Array.from(values as number[], (v) => BigInt(v));
        keepalive.push(intArr);
        dataPtrTable[c] = BigInt(ptr(intArr));
      } else {
        typesArr[c] = DATA_TYPE_FLOAT;
        const floatArr = Float64Array.from(values as number[]);
        keepalive.push(floatArr);
        dataPtrTable[c] = BigInt(ptr(floatArr));
      }
    }
  }

  keepalive.push(namesPtrTable, typesArr, dataPtrTable);

  check(
    lib.quiver_database_update_time_series_group(
      handle,
      toCString(collection),
      toCString(group),
      id,
      ptr(namesPtrTable),
      ptr(typesArr),
      ptr(dataPtrTable),
      columnCount,
      rowCount,
    ),
  );
};

Database.prototype.hasTimeSeriesFiles = function (
  this: Database,
  collection: string,
): boolean {
  const lib = getSymbols();
  const handle = this._handle;

  const outResult = new Int32Array(1);
  check(
    lib.quiver_database_has_time_series_files(handle, toCString(collection), ptr(outResult)),
  );
  return outResult[0] !== 0;
};

Database.prototype.listTimeSeriesFilesColumns = function (
  this: Database,
  collection: string,
): string[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outColumns = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_list_time_series_files_columns(
      handle,
      toCString(collection),
      ptr(outColumns),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const columnsPtr = readPointerOut(outColumns);
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = readPtrAt(columnsPtr, i * 8);
    result[i] = new CString(strPtr).toString();
  }

  lib.quiver_database_free_string_array(columnsPtr, count);
  return result;
};

Database.prototype.readTimeSeriesFiles = function (
  this: Database,
  collection: string,
): Record<string, string | null> {
  const lib = getSymbols();
  const handle = this._handle;

  const outColumns = allocPointerOut();
  const outPaths = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_time_series_files(
      handle,
      toCString(collection),
      ptr(outColumns),
      ptr(outPaths),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return {};

  const columnsPtr = readPointerOut(outColumns);
  const pathsPtr = readPointerOut(outPaths);

  const result: Record<string, string | null> = {};
  for (let i = 0; i < count; i++) {
    const colNamePtr = readPtrAt(columnsPtr, i * 8);
    const colName = new CString(colNamePtr).toString();
    const pathPtr = readPtrAt(pathsPtr, i * 8);
    result[colName] = pathPtr ? new CString(pathPtr).toString() : null;
  }

  lib.quiver_database_free_time_series_files(columnsPtr, pathsPtr, count);
  return result;
};

Database.prototype.updateTimeSeriesFiles = function (
  this: Database,
  collection: string,
  data: Record<string, string | null>,
): void {
  const lib = getSymbols();
  const handle = this._handle;

  const entries = Object.entries(data);
  if (entries.length === 0) return;

  const keepalive: unknown[] = [];
  const columnsPtrTable = new BigInt64Array(entries.length);
  const pathsPtrTable = new BigInt64Array(entries.length);

  for (let i = 0; i < entries.length; i++) {
    const [colName, pathValue] = entries[i];
    const colBuf = toCString(colName);
    keepalive.push(colBuf);
    columnsPtrTable[i] = BigInt(ptr(colBuf));

    if (pathValue !== null) {
      const pathBuf = toCString(pathValue);
      keepalive.push(pathBuf);
      pathsPtrTable[i] = BigInt(ptr(pathBuf));
    } else {
      pathsPtrTable[i] = 0n;
    }
  }

  keepalive.push(columnsPtrTable, pathsPtrTable);

  check(
    lib.quiver_database_update_time_series_files(
      handle,
      toCString(collection),
      ptr(columnsPtrTable),
      ptr(pathsPtrTable),
      entries.length,
    ),
  );
};
