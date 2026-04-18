import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { allocPtrOut, allocUint64Out, readPtrOut, readUint64Out, toCString } from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";

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

function readNullableString(view: Deno.UnsafePointerView, offset: number): string | null {
  const ptr = view.getPointer(offset);
  if (!ptr) return null;
  return new Deno.UnsafePointerView(ptr).getCString();
}

function readScalarMetadataAt(view: Deno.UnsafePointerView, byteOffset: number): ScalarMetadata {
  const namePtr = view.getPointer(byteOffset + 0);
  return {
    name: namePtr ? new Deno.UnsafePointerView(namePtr).getCString() : "",
    dataType: view.getInt32(byteOffset + 8),
    notNull: view.getInt32(byteOffset + 12) !== 0,
    primaryKey: view.getInt32(byteOffset + 16) !== 0,
    defaultValue: readNullableString(view, byteOffset + 24),
    isForeignKey: view.getInt32(byteOffset + 32) !== 0,
    referencesCollection: readNullableString(view, byteOffset + 40),
    referencesColumn: readNullableString(view, byteOffset + 48),
  };
}

function readGroupMetadataAt(view: Deno.UnsafePointerView, byteOffset: number): GroupMetadata {
  const groupNamePtr = view.getPointer(byteOffset + 0);
  const dimColPtr = view.getPointer(byteOffset + 8);
  const valueColumnsPtr = view.getPointer(byteOffset + 16);
  const valueColumnCount = Number(view.getBigUint64(byteOffset + 24));

  const valueColumns: ScalarMetadata[] = new Array(valueColumnCount);
  if (valueColumnsPtr && valueColumnCount > 0) {
    const colView = new Deno.UnsafePointerView(valueColumnsPtr);
    for (let i = 0; i < valueColumnCount; i++) {
      valueColumns[i] = readScalarMetadataAt(colView, i * SCALAR_METADATA_SIZE);
    }
  }

  return {
    groupName: groupNamePtr ? new Deno.UnsafePointerView(groupNamePtr).getCString() : "",
    dimensionColumn: dimColPtr ? new Deno.UnsafePointerView(dimColPtr).getCString() : null,
    valueColumns,
  };
}

declare module "./database.ts" {
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
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outBuf = new Uint8Array(SCALAR_METADATA_SIZE);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_get_scalar_metadata(this._handle, collBuf.buf, attrBuf.buf, outPtr));
  const view = new Deno.UnsafePointerView(outPtr);
  const result = readScalarMetadataAt(view, 0);
  lib.quiver_database_free_scalar_metadata(outPtr);
  return result;
};

Database.prototype.getVectorMetadata = function (this: Database, collection: string, groupName: string): GroupMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const groupBuf = toCString(groupName);
  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_get_vector_metadata(this._handle, collBuf.buf, groupBuf.buf, outPtr));
  const view = new Deno.UnsafePointerView(outPtr);
  const result = readGroupMetadataAt(view, 0);
  lib.quiver_database_free_group_metadata(outPtr);
  return result;
};

Database.prototype.getSetMetadata = function (this: Database, collection: string, groupName: string): GroupMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const groupBuf = toCString(groupName);
  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_get_set_metadata(this._handle, collBuf.buf, groupBuf.buf, outPtr));
  const view = new Deno.UnsafePointerView(outPtr);
  const result = readGroupMetadataAt(view, 0);
  lib.quiver_database_free_group_metadata(outPtr);
  return result;
};

Database.prototype.getTimeSeriesMetadata = function (this: Database, collection: string, groupName: string): GroupMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const groupBuf = toCString(groupName);
  const outBuf = new Uint8Array(GROUP_METADATA_SIZE);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_get_time_series_metadata(this._handle, collBuf.buf, groupBuf.buf, outPtr));
  const view = new Deno.UnsafePointerView(outPtr);
  const result = readGroupMetadataAt(view, 0);
  lib.quiver_database_free_group_metadata(outPtr);
  return result;
};

function listMetadata<T>(
  lib: ReturnType<typeof getSymbols>,
  handle: Deno.PointerValue,
  fn: string,
  collection: string,
  structSize: number,
  reader: (view: Deno.UnsafePointerView, offset: number) => T,
  freeFn: string,
): T[] {
  const collBuf = toCString(collection);
  const outArray = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, outArray.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outArray);
  const view = new Deno.UnsafePointerView(arrPtr!);
  const result: T[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = reader(view, i * structSize);
  }
  (lib as Record<string, Function>)[freeFn](arrPtr, BigInt(count));
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
