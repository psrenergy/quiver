import { ptr } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

/**
 * CSV export/import options.
 * - dateTimeFormat: strftime format string for date/time columns (default: empty = raw)
 * - enumLabels: nested map of attribute -> locale -> { label: intValue }
 */
export interface CsvOptions {
  dateTimeFormat?: string;
  enumLabels?: Record<string, Record<string, Record<string, number>>>;
}

declare module "./database" {
  interface Database {
    exportCsv(collection: string, group: string, filePath: string, options?: CsvOptions): void;
    importCsv(collection: string, group: string, filePath: string, options?: CsvOptions): void;
  }
}

/**
 * Build the 56-byte quiver_csv_options_t struct as a Uint8Array.
 *
 * Layout (x86-64, all fields 8 bytes):
 *   offset  0: const char* date_time_format
 *   offset  8: const char* const* enum_attribute_names
 *   offset 16: const char* const* enum_locale_names
 *   offset 24: const size_t* enum_entry_counts
 *   offset 32: const char* const* enum_labels
 *   offset 40: const int64_t* enum_values
 *   offset 48: size_t enum_group_count
 *
 * Returns [structBuffer, keepalive] where keepalive holds references to prevent GC.
 */
function buildCsvOptionsBuffer(options?: CsvOptions): [Uint8Array, unknown[]] {
  const buf = new ArrayBuffer(56);
  const view = new DataView(buf);
  const keepalive: unknown[] = [];

  // date_time_format: always set (empty string if not provided)
  const dtfStr = toCString(options?.dateTimeFormat ?? "");
  keepalive.push(dtfStr);
  view.setBigInt64(0, BigInt(ptr(dtfStr)), true);

  if (!options?.enumLabels || Object.keys(options.enumLabels).length === 0) {
    // No enum labels: set all enum pointers to NULL, group_count = 0
    view.setBigInt64(8, 0n, true);  // enum_attribute_names
    view.setBigInt64(16, 0n, true); // enum_locale_names
    view.setBigInt64(24, 0n, true); // enum_entry_counts
    view.setBigInt64(32, 0n, true); // enum_labels
    view.setBigInt64(40, 0n, true); // enum_values
    view.setBigInt64(48, 0n, true); // enum_group_count
    return [new Uint8Array(buf), keepalive];
  }

  // Flatten attribute -> locale -> entries into grouped parallel arrays
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
  const totalEntries = allLabels.length;

  // Build pointer arrays for attribute names
  const attrNamePtrs = new BigInt64Array(groupCount);
  for (let i = 0; i < groupCount; i++) {
    const b = toCString(groupAttrNames[i]);
    keepalive.push(b);
    attrNamePtrs[i] = BigInt(ptr(b));
  }
  keepalive.push(attrNamePtrs);

  // Build pointer arrays for locale names
  const localeNamePtrs = new BigInt64Array(groupCount);
  for (let i = 0; i < groupCount; i++) {
    const b = toCString(groupLocaleNames[i]);
    keepalive.push(b);
    localeNamePtrs[i] = BigInt(ptr(b));
  }
  keepalive.push(localeNamePtrs);

  // Entry counts (size_t = 8 bytes on 64-bit)
  const entryCounts = new BigUint64Array(groupCount);
  for (let i = 0; i < groupCount; i++) {
    entryCounts[i] = BigInt(groupEntryCounts[i]);
  }
  keepalive.push(entryCounts);

  // Build pointer arrays for all labels
  const labelPtrs = new BigInt64Array(totalEntries);
  for (let i = 0; i < totalEntries; i++) {
    const b = toCString(allLabels[i]);
    keepalive.push(b);
    labelPtrs[i] = BigInt(ptr(b));
  }
  keepalive.push(labelPtrs);

  // All values (int64_t)
  const valueArr = new BigInt64Array(totalEntries);
  for (let i = 0; i < totalEntries; i++) {
    valueArr[i] = BigInt(allValues[i]);
  }
  keepalive.push(valueArr);

  // Write pointers into struct
  view.setBigInt64(8, BigInt(ptr(attrNamePtrs)), true);
  view.setBigInt64(16, BigInt(ptr(localeNamePtrs)), true);
  view.setBigInt64(24, BigInt(ptr(entryCounts)), true);
  view.setBigInt64(32, BigInt(ptr(labelPtrs)), true);
  view.setBigInt64(40, BigInt(ptr(valueArr)), true);
  view.setBigInt64(48, BigInt(groupCount), true);

  return [new Uint8Array(buf), keepalive];
}

Database.prototype.exportCsv = function (
  this: Database,
  collection: string,
  group: string,
  filePath: string,
  options?: CsvOptions,
): void {
  const lib = getSymbols();
  const handle = this._handle;
  const [optsBuf, _keepalive] = buildCsvOptionsBuffer(options);

  check(
    lib.quiver_database_export_csv(
      handle,
      toCString(collection),
      toCString(group),
      toCString(filePath),
      ptr(optsBuf),
    ),
  );
};

Database.prototype.importCsv = function (
  this: Database,
  collection: string,
  group: string,
  filePath: string,
  options?: CsvOptions,
): void {
  const lib = getSymbols();
  const handle = this._handle;
  const [optsBuf, _keepalive] = buildCsvOptionsBuffer(options);

  check(
    lib.quiver_database_import_csv(
      handle,
      toCString(collection),
      toCString(group),
      toCString(filePath),
      ptr(optsBuf),
    ),
  );
};
