import koffi from "koffi";
import { Database } from "./database.js";
import { check } from "./errors.js";
import { allocPtrOut, allocUint64Out, readPtrOut, readUint64Out } from "./ffi-helpers.js";
import type { NativePointer } from "./loader.js";
import { getSymbols } from "./loader.js";

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

/** Read a string at offset in a struct. Handles the koffi bug where decode(externalPtr, "str") segfaults. */
function readStrAt(basePtr: NativePointer, offset: number): string {
  return (koffi.decode(basePtr, offset, koffi.array("str", 1)) as string[])[0];
}

function readNullableStrAt(basePtr: NativePointer, offset: number): string | null {
  const ptr = koffi.decode(basePtr, offset, "void *") as NativePointer;
  if (!ptr) return null;
  return readStrAt(basePtr, offset);
}

function readScalarMetadataAt(basePtr: NativePointer, byteOffset: number): ScalarMetadata {
  return {
    name: readStrAt(basePtr, byteOffset),
    dataType: koffi.decode(basePtr, byteOffset + 8, "int") as number,
    notNull: (koffi.decode(basePtr, byteOffset + 12, "int") as number) !== 0,
    primaryKey: (koffi.decode(basePtr, byteOffset + 16, "int") as number) !== 0,
    defaultValue: readNullableStrAt(basePtr, byteOffset + 24),
    isForeignKey: (koffi.decode(basePtr, byteOffset + 32, "int") as number) !== 0,
    referencesCollection: readNullableStrAt(basePtr, byteOffset + 40),
    referencesColumn: readNullableStrAt(basePtr, byteOffset + 48),
  };
}

function readGroupMetadataAt(basePtr: NativePointer, byteOffset: number): GroupMetadata {
  const groupName = readStrAt(basePtr, byteOffset);
  const dimensionColumn = readNullableStrAt(basePtr, byteOffset + 8);
  const valueColumnsPtr = koffi.decode(basePtr, byteOffset + 16, "void *") as NativePointer;
  const valueColumnCount = Number(koffi.decode(basePtr, byteOffset + 24, "uint64_t") as bigint);

  const valueColumns: ScalarMetadata[] = new Array(valueColumnCount);
  for (let i = 0; i < valueColumnCount; i++) {
    valueColumns[i] = readScalarMetadataAt(valueColumnsPtr, i * SCALAR_METADATA_SIZE);
  }

  return { groupName, dimensionColumn, valueColumns };
}

declare module "./database.js" {
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

Database.prototype.getScalarMetadata = function (this: Database, collection: string, attribute: string): ScalarMetadata {
  const lib = getSymbols();
  const outBuf = Buffer.alloc(SCALAR_METADATA_SIZE);
  check(lib.quiver_database_get_scalar_metadata(this._handle, collection, attribute, outBuf));
  const result = readScalarMetadataAt(outBuf, 0);
  lib.quiver_database_free_scalar_metadata(outBuf);
  return result;
};

Database.prototype.getVectorMetadata = function (this: Database, collection: string, groupName: string): GroupMetadata {
  const lib = getSymbols();
  const outBuf = Buffer.alloc(GROUP_METADATA_SIZE);
  check(lib.quiver_database_get_vector_metadata(this._handle, collection, groupName, outBuf));
  const result = readGroupMetadataAt(outBuf, 0);
  lib.quiver_database_free_group_metadata(outBuf);
  return result;
};

Database.prototype.getSetMetadata = function (this: Database, collection: string, groupName: string): GroupMetadata {
  const lib = getSymbols();
  const outBuf = Buffer.alloc(GROUP_METADATA_SIZE);
  check(lib.quiver_database_get_set_metadata(this._handle, collection, groupName, outBuf));
  const result = readGroupMetadataAt(outBuf, 0);
  lib.quiver_database_free_group_metadata(outBuf);
  return result;
};

Database.prototype.getTimeSeriesMetadata = function (this: Database, collection: string, groupName: string): GroupMetadata {
  const lib = getSymbols();
  const outBuf = Buffer.alloc(GROUP_METADATA_SIZE);
  check(lib.quiver_database_get_time_series_metadata(this._handle, collection, groupName, outBuf));
  const result = readGroupMetadataAt(outBuf, 0);
  lib.quiver_database_free_group_metadata(outBuf);
  return result;
};

function listMetadata<T>(
  lib: ReturnType<typeof getSymbols>,
  handle: unknown,
  fn: string,
  collection: string,
  structSize: number,
  reader: (ptr: NativePointer, offset: number) => T,
  freeFn: string,
): T[] {
  const outArray = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, outArray, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outArray);
  const result: T[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = reader(arrPtr, i * structSize);
  }
  lib[freeFn](arrPtr, count);
  return result;
}

Database.prototype.listScalarAttributes = function (this: Database, collection: string): ScalarMetadata[] {
  return listMetadata(getSymbols(), this._handle, "quiver_database_list_scalar_attributes", collection, SCALAR_METADATA_SIZE, readScalarMetadataAt, "quiver_database_free_scalar_metadata_array");
};

Database.prototype.listVectorGroups = function (this: Database, collection: string): GroupMetadata[] {
  return listMetadata(getSymbols(), this._handle, "quiver_database_list_vector_groups", collection, GROUP_METADATA_SIZE, readGroupMetadataAt, "quiver_database_free_group_metadata_array");
};

Database.prototype.listSetGroups = function (this: Database, collection: string): GroupMetadata[] {
  return listMetadata(getSymbols(), this._handle, "quiver_database_list_set_groups", collection, GROUP_METADATA_SIZE, readGroupMetadataAt, "quiver_database_free_group_metadata_array");
};

Database.prototype.listTimeSeriesGroups = function (this: Database, collection: string): GroupMetadata[] {
  return listMetadata(getSymbols(), this._handle, "quiver_database_list_time_series_groups", collection, GROUP_METADATA_SIZE, readGroupMetadataAt, "quiver_database_free_group_metadata_array");
};
