import koffi from "koffi";
import { Database } from "./database.js";
import { check } from "./errors.js";
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
} from "./ffi-helpers.js";
import type { NativePointer } from "./loader.js";
import { getSymbols } from "./loader.js";

export type TimeSeriesData = Record<string, (number | string)[]>;

declare module "./database.js" {
  interface Database {
    readTimeSeriesGroup(collection: string, group: string, id: number): TimeSeriesData;
    updateTimeSeriesGroup(collection: string, group: string, id: number, data: TimeSeriesData): void;
    hasTimeSeriesFiles(collection: string): boolean;
    listTimeSeriesFilesColumns(collection: string): string[];
    readTimeSeriesFiles(collection: string): Record<string, string | null>;
    updateTimeSeriesFiles(collection: string, data: Record<string, string | null>): void;
  }
}

const DATA_TYPE_INTEGER = 0;
const DATA_TYPE_FLOAT = 1;
const DATA_TYPE_STRING = 2;
const DATA_TYPE_DATE_TIME = 3;

Database.prototype.readTimeSeriesGroup = function (this: Database, collection: string, group: string, id: number): TimeSeriesData {
  const lib = getSymbols();
  const outNames = allocPtrOut();
  const outTypes = allocPtrOut();
  const outData = allocPtrOut();
  const outColCount = allocUint64Out();
  const outRowCount = allocUint64Out();

  check(lib.quiver_database_read_time_series_group(
    this._handle, collection, group, id, outNames, outTypes, outData, outColCount, outRowCount,
  ));

  const colCount = readUint64Out(outColCount);
  const rowCount = readUint64Out(outRowCount);
  if (colCount === 0) return {};

  const namesPtr = readPtrOut(outNames);
  const typesPtr = readPtrOut(outTypes);
  const dataPtr = readPtrOut(outData);

  const colNames = decodeStringArray(namesPtr, colCount);
  const types = koffi.decode(typesPtr, koffi.array("int", colCount)) as number[];
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

  lib.quiver_database_free_time_series_data(namesPtr, typesPtr, dataPtr, colCount, rowCount);
  return result;
};

Database.prototype.updateTimeSeriesGroup = function (this: Database, collection: string, group: string, id: number, data: TimeSeriesData): void {
  const lib = getSymbols();
  const entries = Object.entries(data);

  if (entries.length === 0) {
    check(lib.quiver_database_update_time_series_group(this._handle, collection, group, id, null, null, null, 0, 0));
    return;
  }

  const columnCount = entries.length;
  const rowCount = entries[0][1].length;
  const keepalive: NativePointer[] = [];

  // Build column names as native string array
  const colNames = entries.map(([name]) => name);
  const { table: namesTable, keepalive: namesPtrs } = allocNativeStringArray(colNames);
  keepalive.push(namesTable, ...namesPtrs);

  // Build column types and data
  const typesArr = new Int32Array(columnCount);
  const dataPtrs: NativePointer[] = [];

  for (let c = 0; c < columnCount; c++) {
    const values = entries[c][1];

    if (values.length === 0) {
      typesArr[c] = DATA_TYPE_STRING;
      dataPtrs.push(null);
    } else if (typeof values[0] === "string") {
      typesArr[c] = DATA_TYPE_STRING;
      const { table, keepalive: strPtrs } = allocNativeStringArray(values as string[]);
      keepalive.push(table, ...strPtrs);
      dataPtrs.push(table);
    } else if (typeof values[0] === "number") {
      const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
      if (allIntegers) {
        typesArr[c] = DATA_TYPE_INTEGER;
        const p = allocNativeInt64(values as number[]);
        keepalive.push(p);
        dataPtrs.push(p);
      } else {
        typesArr[c] = DATA_TYPE_FLOAT;
        const p = allocNativeFloat64(values as number[]);
        keepalive.push(p);
        dataPtrs.push(p);
      }
    }
  }

  const dataTable = allocNativePtrTable(dataPtrs);
  keepalive.push(dataTable);

  check(lib.quiver_database_update_time_series_group(
    this._handle, collection, group, id, namesTable, typesArr, dataTable, columnCount, rowCount,
  ));
};

Database.prototype.hasTimeSeriesFiles = function (this: Database, collection: string): boolean {
  const lib = getSymbols();
  const outResult = new Int32Array(1);
  check(lib.quiver_database_has_time_series_files(this._handle, collection, outResult));
  return outResult[0] !== 0;
};

Database.prototype.listTimeSeriesFilesColumns = function (this: Database, collection: string): string[] {
  const lib = getSymbols();
  const outColumns = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_list_time_series_files_columns(this._handle, collection, outColumns, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outColumns);
  const result = decodeStringArray(ptr, count);
  lib.quiver_database_free_string_array(ptr, count);
  return result;
};

Database.prototype.readTimeSeriesFiles = function (this: Database, collection: string): Record<string, string | null> {
  const lib = getSymbols();
  const outColumns = allocPtrOut();
  const outPaths = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_time_series_files(this._handle, collection, outColumns, outPaths, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return {};
  const colsPtr = readPtrOut(outColumns);
  const pathsPtr = readPtrOut(outPaths);
  const colNames = decodeStringArray(colsPtr, count);
  const paths = koffi.decode(pathsPtr, koffi.array("str", count)) as (string | null)[];
  const result: Record<string, string | null> = {};
  for (let i = 0; i < count; i++) {
    result[colNames[i]] = paths[i];
  }
  lib.quiver_database_free_time_series_files(colsPtr, pathsPtr, count);
  return result;
};

Database.prototype.updateTimeSeriesFiles = function (this: Database, collection: string, data: Record<string, string | null>): void {
  const lib = getSymbols();
  const entries = Object.entries(data);
  if (entries.length === 0) return;
  const keepalive: NativePointer[] = [];

  const colNames = entries.map(([name]) => name);
  const { table: colsTable, keepalive: colPtrs } = allocNativeStringArray(colNames);
  keepalive.push(colsTable, ...colPtrs);

  const pathPtrs: NativePointer[] = entries.map(([, value]) => {
    if (value === null) return null;
    const p = allocNativeString(value);
    keepalive.push(p);
    return p;
  });

  const pathsTable = allocNativePtrTable(pathPtrs);
  keepalive.push(pathsTable);

  check(lib.quiver_database_update_time_series_files(this._handle, collection, colsTable, pathsTable, entries.length));
};
