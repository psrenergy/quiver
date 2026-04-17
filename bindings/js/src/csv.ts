import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { allocNativeInt64, allocNativeString, allocNativeStringArray, nativeAddress, toCString } from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";
import type { Allocation } from "./types.ts";

export interface CsvOptions {
  dateTimeFormat?: string;
  enumLabels?: Record<string, Record<string, Record<string, number>>>;
}

declare module "./database.ts" {
  interface Database {
    exportCsv(collection: string, group: string, filePath: string, options?: CsvOptions): void;
    importCsv(collection: string, group: string, filePath: string, options?: CsvOptions): void;
  }
}

/**
 * Build the 56-byte quiver_csv_options_t struct as an Allocation.
 * Returns [structAllocation, keepalive] where keepalive prevents GC of native allocations.
 */
function buildCsvOptionsBuffer(options?: CsvOptions): [Allocation, Allocation[]] {
  const buf = new Uint8Array(56);
  const dv = new DataView(buf.buffer);
  const keepalive: Allocation[] = [];

  const dtfStr = allocNativeString(options?.dateTimeFormat ?? "");
  keepalive.push(dtfStr);
  dv.setBigUint64(0, nativeAddress(dtfStr.ptr), true);

  if (!options?.enumLabels || Object.keys(options.enumLabels).length === 0) {
    return [{ ptr: Deno.UnsafePointer.of(buf)!, buf }, keepalive];
  }

  const groupAttrNames: string[] = [];
  const groupLocaleNames: string[] = [];
  const groupEntryCounts: number[] = [];
  const allLabels: string[] = [];
  const allValues: number[] = [];

  for (const [attrName, locales] of Object.entries(options.enumLabels)) {
    for (const [localeName, entries] of Object.entries(locales)) {
      groupAttrNames.push(attrName);
      groupLocaleNames.push(localeName);
      groupEntryCounts.push(Object.keys(entries).length);
      for (const [label, value] of Object.entries(entries)) {
        allLabels.push(label);
        allValues.push(value);
      }
    }
  }

  const groupCount = groupAttrNames.length;

  const { table: attrTable, keepalive: attrPtrs } = allocNativeStringArray(groupAttrNames);
  keepalive.push(attrTable, ...attrPtrs);

  const { table: localeTable, keepalive: localePtrs } = allocNativeStringArray(groupLocaleNames);
  keepalive.push(localeTable, ...localePtrs);

  const entryCountsBuf = new Uint8Array(groupCount * 8);
  const entryCountsDv = new DataView(entryCountsBuf.buffer);
  for (let i = 0; i < groupCount; i++) {
    entryCountsDv.setBigUint64(i * 8, BigInt(groupEntryCounts[i]), true);
  }
  const entryCounts: Allocation = { ptr: Deno.UnsafePointer.of(entryCountsBuf)!, buf: entryCountsBuf };
  keepalive.push(entryCounts);

  const { table: labelsTable, keepalive: labelPtrs } = allocNativeStringArray(allLabels);
  keepalive.push(labelsTable, ...labelPtrs);

  const valuesArr = allocNativeInt64(allValues);
  keepalive.push(valuesArr);

  dv.setBigUint64(8, nativeAddress(attrTable.ptr), true);
  dv.setBigUint64(16, nativeAddress(localeTable.ptr), true);
  dv.setBigUint64(24, nativeAddress(entryCounts.ptr), true);
  dv.setBigUint64(32, nativeAddress(labelsTable.ptr), true);
  dv.setBigUint64(40, nativeAddress(valuesArr.ptr), true);
  dv.setBigUint64(48, BigInt(groupCount), true);

  return [{ ptr: Deno.UnsafePointer.of(buf)!, buf }, keepalive];
}

Database.prototype.exportCsv = function (
  this: Database,
  collection: string,
  group: string,
  filePath: string,
  options?: CsvOptions,
): void {
  const lib = getSymbols();
  const [optsBuf, _keepalive] = buildCsvOptionsBuffer(options);
  check(lib.quiver_database_export_csv(this._handle, toCString(collection).buf, toCString(group).buf, toCString(filePath).buf, optsBuf.ptr));
};

Database.prototype.importCsv = function (
  this: Database,
  collection: string,
  group: string,
  filePath: string,
  options?: CsvOptions,
): void {
  const lib = getSymbols();
  const [optsBuf, _keepalive] = buildCsvOptionsBuffer(options);
  check(lib.quiver_database_import_csv(this._handle, toCString(collection).buf, toCString(group).buf, toCString(filePath).buf, optsBuf.ptr));
};
