import koffi from "koffi";
import { Database } from "./database.js";
import { check } from "./errors.js";
import { allocNativeInt64, allocNativeString, allocNativeStringArray, allocNativePtrTable, nativeAddress } from "./ffi-helpers.js";
import type { NativePointer } from "./loader.js";
import { getSymbols } from "./loader.js";

export interface CsvOptions {
  dateTimeFormat?: string;
  enumLabels?: Record<string, Record<string, Record<string, number>>>;
}

declare module "./database.js" {
  interface Database {
    exportCsv(collection: string, group: string, filePath: string, options?: CsvOptions): void;
    importCsv(collection: string, group: string, filePath: string, options?: CsvOptions): void;
  }
}

/**
 * Build the 56-byte quiver_csv_options_t struct as a Buffer.
 * Returns [structBuffer, keepalive] where keepalive prevents GC of native allocations.
 */
function buildCsvOptionsBuffer(options?: CsvOptions): [Buffer, NativePointer[]] {
  const buf = Buffer.alloc(56);
  const keepalive: NativePointer[] = [];

  const dtfStr = allocNativeString(options?.dateTimeFormat ?? "");
  keepalive.push(dtfStr);
  buf.writeBigUInt64LE(nativeAddress(dtfStr), 0);

  if (!options?.enumLabels || Object.keys(options.enumLabels).length === 0) {
    return [buf, keepalive];
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

  const entryCounts = koffi.alloc("uint64_t", groupCount);
  for (let i = 0; i < groupCount; i++) {
    koffi.encode(entryCounts, i * 8, "uint64_t", BigInt(groupEntryCounts[i]));
  }
  keepalive.push(entryCounts);

  const { table: labelsTable, keepalive: labelPtrs } = allocNativeStringArray(allLabels);
  keepalive.push(labelsTable, ...labelPtrs);

  const valuesArr = allocNativeInt64(allValues);
  keepalive.push(valuesArr);

  buf.writeBigUInt64LE(nativeAddress(attrTable), 8);
  buf.writeBigUInt64LE(nativeAddress(localeTable), 16);
  buf.writeBigUInt64LE(nativeAddress(entryCounts), 24);
  buf.writeBigUInt64LE(nativeAddress(labelsTable), 32);
  buf.writeBigUInt64LE(nativeAddress(valuesArr), 40);
  buf.writeBigUInt64LE(BigInt(groupCount), 48);

  return [buf, keepalive];
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
  check(lib.quiver_database_export_csv(this._handle, collection, group, filePath, optsBuf));
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
  check(lib.quiver_database_import_csv(this._handle, collection, group, filePath, optsBuf));
};
