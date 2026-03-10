import { CString, ptr, read } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

export interface ScalarMetadata {
  name: string;
  dataType: number;
  notNull: boolean;
  primaryKey: boolean;
  defaultValue: string | null;
  isForeignKey: boolean;
  referencesCollection: string | null;
  referencesColumn: string | null;
}

export interface GroupMetadata {
  groupName: string;
  dimensionColumn: string | null;
  valueColumns: ScalarMetadata[];
}

const SCALAR_METADATA_SIZE = 56;
const GROUP_METADATA_SIZE = 32;

function readNullableString(base: number, offset: number): string | null {
  const p = read.ptr(base, offset);
  if (!p) return null;
  return new CString(p).toString();
}

function readScalarMetadata(base: number, byteOffset: number): ScalarMetadata {
  const addr = base + byteOffset;
  const name = new CString(read.ptr(addr, 0)).toString();
  const dataType = read.i32(addr, 8);
  const notNull = read.i32(addr, 12) !== 0;
  const primaryKey = read.i32(addr, 16) !== 0;
  const defaultValue = readNullableString(addr, 24);
  const isForeignKey = read.i32(addr, 32) !== 0;
  const referencesCollection = readNullableString(addr, 40);
  const referencesColumn = readNullableString(addr, 48);

  return {
    name,
    dataType,
    notNull,
    primaryKey,
    defaultValue,
    isForeignKey,
    referencesCollection,
    referencesColumn,
  };
}

function readGroupMetadata(base: number, byteOffset: number): GroupMetadata {
  const addr = base + byteOffset;
  const groupName = new CString(read.ptr(addr, 0)).toString();
  const dimensionColumn = readNullableString(addr, 8);
  const valueColumnsPtr = read.ptr(addr, 16);
  const valueColumnCount = Number(read.u64(addr, 24));

  const valueColumns: ScalarMetadata[] = new Array(valueColumnCount);
  for (let i = 0; i < valueColumnCount; i++) {
    valueColumns[i] = readScalarMetadata(valueColumnsPtr, i * SCALAR_METADATA_SIZE);
  }

  return { groupName, dimensionColumn, valueColumns };
}

declare module "./database" {
  interface Database {
    getScalarMetadata(collection: string, attribute: string): ScalarMetadata;
    getVectorMetadata(collection: string, groupName: string): GroupMetadata;
    getSetMetadata(collection: string, groupName: string): GroupMetadata;
    getTimeSeriesMetadata(collection: string, groupName: string): GroupMetadata;
    listScalarAttributes(collection: string): ScalarMetadata[];
    listVectorGroups(collection: string): GroupMetadata[];
    listSetGroups(collection: string): GroupMetadata[];
    listTimeSeriesGroups(collection: string): GroupMetadata[];
  }
}

// --- Get methods ---

Database.prototype.getScalarMetadata = function (
  this: Database,
  collection: string,
  attribute: string,
): ScalarMetadata {
  const lib = getSymbols();
  const handle = this._handle;

  const outBuf = new Uint8Array(SCALAR_METADATA_SIZE);

  check(
    lib.quiver_database_get_scalar_metadata(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outBuf),
    ),
  );

  const result = readScalarMetadata(ptr(outBuf) as unknown as number, 0);
  lib.quiver_database_free_scalar_metadata(ptr(outBuf));
  return result;
};

Database.prototype.getVectorMetadata = function (
  this: Database,
  collection: string,
  groupName: string,
): GroupMetadata {
  const lib = getSymbols();
  const handle = this._handle;

  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);

  check(
    lib.quiver_database_get_vector_metadata(
      handle,
      toCString(collection),
      toCString(groupName),
      ptr(outBuf),
    ),
  );

  const result = readGroupMetadata(ptr(outBuf) as unknown as number, 0);
  lib.quiver_database_free_group_metadata(ptr(outBuf));
  return result;
};

Database.prototype.getSetMetadata = function (
  this: Database,
  collection: string,
  groupName: string,
): GroupMetadata {
  const lib = getSymbols();
  const handle = this._handle;

  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);

  check(
    lib.quiver_database_get_set_metadata(
      handle,
      toCString(collection),
      toCString(groupName),
      ptr(outBuf),
    ),
  );

  const result = readGroupMetadata(ptr(outBuf) as unknown as number, 0);
  lib.quiver_database_free_group_metadata(ptr(outBuf));
  return result;
};

Database.prototype.getTimeSeriesMetadata = function (
  this: Database,
  collection: string,
  groupName: string,
): GroupMetadata {
  const lib = getSymbols();
  const handle = this._handle;

  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);

  check(
    lib.quiver_database_get_time_series_metadata(
      handle,
      toCString(collection),
      toCString(groupName),
      ptr(outBuf),
    ),
  );

  const result = readGroupMetadata(ptr(outBuf) as unknown as number, 0);
  lib.quiver_database_free_group_metadata(ptr(outBuf));
  return result;
};

// --- List methods ---

Database.prototype.listScalarAttributes = function (
  this: Database,
  collection: string,
): ScalarMetadata[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outArray = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_list_scalar_attributes(
      handle,
      toCString(collection),
      ptr(outArray),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outArray);
  const arrAddr = arrPtr as unknown as number;
  const result: ScalarMetadata[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = readScalarMetadata(arrAddr, i * SCALAR_METADATA_SIZE);
  }

  lib.quiver_database_free_scalar_metadata_array(arrPtr, count);
  return result;
};

Database.prototype.listVectorGroups = function (
  this: Database,
  collection: string,
): GroupMetadata[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outArray = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_list_vector_groups(
      handle,
      toCString(collection),
      ptr(outArray),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outArray);
  const arrAddr = arrPtr as unknown as number;
  const result: GroupMetadata[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = readGroupMetadata(arrAddr, i * GROUP_METADATA_SIZE);
  }

  lib.quiver_database_free_group_metadata_array(arrPtr, count);
  return result;
};

Database.prototype.listSetGroups = function (
  this: Database,
  collection: string,
): GroupMetadata[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outArray = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_list_set_groups(
      handle,
      toCString(collection),
      ptr(outArray),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outArray);
  const arrAddr = arrPtr as unknown as number;
  const result: GroupMetadata[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = readGroupMetadata(arrAddr, i * GROUP_METADATA_SIZE);
  }

  lib.quiver_database_free_group_metadata_array(arrPtr, count);
  return result;
};

Database.prototype.listTimeSeriesGroups = function (
  this: Database,
  collection: string,
): GroupMetadata[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outArray = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_list_time_series_groups(
      handle,
      toCString(collection),
      ptr(outArray),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outArray);
  const arrAddr = arrPtr as unknown as number;
  const result: GroupMetadata[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = readGroupMetadata(arrAddr, i * GROUP_METADATA_SIZE);
  }

  lib.quiver_database_free_group_metadata_array(arrPtr, count);
  return result;
};
