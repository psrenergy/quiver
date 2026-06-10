import { CString, type Pointer, ptr, read } from "bun:ffi";
import { Database } from "./database.ts";
import { check } from "./errors.ts";
import {
  allocPtrOut,
  allocUint64Out,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols, type NativePointer } from "./loader.ts";

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

function readNullableString(base: Pointer, offset: number): string | null {
  const p = read.ptr(base, offset);
  if (p === 0) return null;
  return new CString(p as Pointer).toString();
}

function readScalarMetadataAt(base: Pointer, byteOffset: number): ScalarMetadata {
  const namePtr = read.ptr(base, byteOffset + 0);
  return {
    name: namePtr === 0 ? "" : new CString(namePtr as Pointer).toString(),
    dataType: read.i32(base, byteOffset + 8),
    notNull: read.i32(base, byteOffset + 12) !== 0,
    primaryKey: read.i32(base, byteOffset + 16) !== 0,
    defaultValue: readNullableString(base, byteOffset + 24),
    isForeignKey: read.i32(base, byteOffset + 32) !== 0,
    referencesCollection: readNullableString(base, byteOffset + 40),
    referencesColumn: readNullableString(base, byteOffset + 48),
  };
}

function readGroupMetadataAt(base: Pointer, byteOffset: number): GroupMetadata {
  const groupNamePtr = read.ptr(base, byteOffset + 0);
  const dimColPtr = read.ptr(base, byteOffset + 8);
  const valueColumnsPtr = read.ptr(base, byteOffset + 16);
  const valueColumnCount = Number(read.u64(base, byteOffset + 24));

  const valueColumns: ScalarMetadata[] = new Array(valueColumnCount);
  if (valueColumnsPtr !== 0 && valueColumnCount > 0) {
    for (let i = 0; i < valueColumnCount; i++) {
      valueColumns[i] = readScalarMetadataAt(valueColumnsPtr as Pointer, i * SCALAR_METADATA_SIZE);
    }
  }

  return {
    groupName: groupNamePtr === 0 ? "" : new CString(groupNamePtr as Pointer).toString(),
    dimensionColumn: dimColPtr === 0 ? null : new CString(dimColPtr as Pointer).toString(),
    valueColumns,
  };
}

Database.prototype.getScalarMetadata = function (
  this: Database,
  collection: string,
  attribute: string,
): ScalarMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outBuf = new Uint8Array(SCALAR_METADATA_SIZE);
  const outPtr = ptr(outBuf);
  check(lib.quiver_database_get_scalar_metadata(this._handle, collBuf.buf, attrBuf.buf, outPtr));
  const result = readScalarMetadataAt(outPtr, 0);
  lib.quiver_database_free_scalar_metadata(outPtr);
  return result;
};

Database.prototype.getVectorMetadata = function (
  this: Database,
  collection: string,
  groupName: string,
): GroupMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const groupBuf = toCString(groupName);
  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);
  const outPtr = ptr(outBuf);
  check(lib.quiver_database_get_vector_metadata(this._handle, collBuf.buf, groupBuf.buf, outPtr));
  const result = readGroupMetadataAt(outPtr, 0);
  lib.quiver_database_free_group_metadata(outPtr);
  return result;
};

Database.prototype.getSetMetadata = function (
  this: Database,
  collection: string,
  groupName: string,
): GroupMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const groupBuf = toCString(groupName);
  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);
  const outPtr = ptr(outBuf);
  check(lib.quiver_database_get_set_metadata(this._handle, collBuf.buf, groupBuf.buf, outPtr));
  const result = readGroupMetadataAt(outPtr, 0);
  lib.quiver_database_free_group_metadata(outPtr);
  return result;
};

Database.prototype.getTimeSeriesMetadata = function (
  this: Database,
  collection: string,
  groupName: string,
): GroupMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const groupBuf = toCString(groupName);
  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);
  const outPtr = ptr(outBuf);
  check(
    lib.quiver_database_get_time_series_metadata(this._handle, collBuf.buf, groupBuf.buf, outPtr),
  );
  const result = readGroupMetadataAt(outPtr, 0);
  lib.quiver_database_free_group_metadata(outPtr);
  return result;
};

function listMetadata<T>(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  structSize: number,
  reader: (base: Pointer, offset: number) => T,
  freeFn: string,
): T[] {
  const collBuf = toCString(collection);
  const outArray = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, outArray.buf, outCount.buf));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outArray);
  const result: T[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = reader(arrPtr as Pointer, i * structSize);
  }
  (lib as Record<string, Function>)[freeFn](arrPtr, BigInt(count));
  return result;
}

Database.prototype.listScalarAttributes = function (
  this: Database,
  collection: string,
): ScalarMetadata[] {
  return listMetadata(
    getSymbols(),
    this._handle,
    "quiver_database_list_scalar_attributes",
    collection,
    SCALAR_METADATA_SIZE,
    readScalarMetadataAt,
    "quiver_database_free_scalar_metadata_array",
  );
};

Database.prototype.listVectorGroups = function (
  this: Database,
  collection: string,
): GroupMetadata[] {
  return listMetadata(
    getSymbols(),
    this._handle,
    "quiver_database_list_vector_groups",
    collection,
    GROUP_METADATA_SIZE,
    readGroupMetadataAt,
    "quiver_database_free_group_metadata_array",
  );
};

Database.prototype.listSetGroups = function (this: Database, collection: string): GroupMetadata[] {
  return listMetadata(
    getSymbols(),
    this._handle,
    "quiver_database_list_set_groups",
    collection,
    GROUP_METADATA_SIZE,
    readGroupMetadataAt,
    "quiver_database_free_group_metadata_array",
  );
};

Database.prototype.listTimeSeriesGroups = function (
  this: Database,
  collection: string,
): GroupMetadata[] {
  return listMetadata(
    getSymbols(),
    this._handle,
    "quiver_database_list_time_series_groups",
    collection,
    GROUP_METADATA_SIZE,
    readGroupMetadataAt,
    "quiver_database_free_group_metadata_array",
  );
};
