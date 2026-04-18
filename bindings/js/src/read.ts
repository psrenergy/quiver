import { Database } from "./database.ts";
import { check } from "./errors.ts";
import {
  allocPtrOut,
  allocUint64Out,
  decodeFloat64Array,
  decodeInt64Array,
  decodePtrArray,
  decodeStringArray,
  decodeStringFromBuf,
  decodeUint64Array,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";

// --- Scalar array reads ---

Database.prototype.readScalarIntegers = function (this: Database, collection: string, attribute: string): number[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_scalar_integers(this._handle, collBuf.buf, attrBuf.buf, outValues.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeInt64Array(ptr, count);
  lib.quiver_database_free_integer_array(ptr);
  return result;
};

Database.prototype.readScalarFloats = function (this: Database, collection: string, attribute: string): number[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_scalar_floats(this._handle, collBuf.buf, attrBuf.buf, outValues.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeFloat64Array(ptr, count);
  lib.quiver_database_free_float_array(ptr);
  return result;
};

Database.prototype.readScalarStrings = function (this: Database, collection: string, attribute: string): string[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_scalar_strings(this._handle, collBuf.buf, attrBuf.buf, outValues.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeStringArray(ptr, count);
  lib.quiver_database_free_string_array(ptr, BigInt(count));
  return result;
};

// --- Scalar by-ID reads ---

Database.prototype.readScalarIntegerById = function (this: Database, collection: string, attribute: string, id: number): number | null {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValBuf = new Uint8Array(8);
  const outValPtr = Deno.UnsafePointer.of(outValBuf)!;
  const outHasBuf = new Uint8Array(4);
  const outHasPtr = Deno.UnsafePointer.of(outHasBuf)!;
  check(lib.quiver_database_read_scalar_integer_by_id(this._handle, collBuf.buf, attrBuf.buf, BigInt(id), outValPtr, outHasPtr));
  if (new DataView(outHasBuf.buffer).getInt32(0, true) === 0) return null;
  return Number(new DataView(outValBuf.buffer).getBigInt64(0, true));
};

Database.prototype.readScalarFloatById = function (this: Database, collection: string, attribute: string, id: number): number | null {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValBuf = new Uint8Array(8);
  const outValPtr = Deno.UnsafePointer.of(outValBuf)!;
  const outHasBuf = new Uint8Array(4);
  const outHasPtr = Deno.UnsafePointer.of(outHasBuf)!;
  check(lib.quiver_database_read_scalar_float_by_id(this._handle, collBuf.buf, attrBuf.buf, BigInt(id), outValPtr, outHasPtr));
  if (new DataView(outHasBuf.buffer).getInt32(0, true) === 0) return null;
  return new DataView(outValBuf.buffer).getFloat64(0, true);
};

Database.prototype.readScalarStringById = function (this: Database, collection: string, attribute: string, id: number): string | null {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValue = allocPtrOut();
  const outHasBuf = new Uint8Array(4);
  const outHasPtr = Deno.UnsafePointer.of(outHasBuf)!;
  check(lib.quiver_database_read_scalar_string_by_id(this._handle, collBuf.buf, attrBuf.buf, BigInt(id), outValue.ptr, outHasPtr));
  if (new DataView(outHasBuf.buffer).getInt32(0, true) === 0) return null;
  const result = decodeStringFromBuf(outValue);
  lib.quiver_database_free_string(readPtrOut(outValue));
  return result;
};

// --- Read element IDs ---

Database.prototype.readElementIds = function (this: Database, collection: string): number[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const outIds = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_element_ids(this._handle, collBuf.buf, outIds.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outIds);
  const result = decodeInt64Array(ptr, count);
  lib.quiver_database_free_integer_array(ptr);
  return result;
};

// --- Vector bulk reads ---

function readBulkIntegers(lib: ReturnType<typeof getSymbols>, handle: Deno.PointerValue, fn: string, collection: string, attribute: string): number[][] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, outVectors.ptr, outSizes.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const vectorsPtr = readPtrOut(outVectors);
  const sizesPtr = readPtrOut(outSizes);
  const vectorPtrs = decodePtrArray(vectorsPtr, count);
  const sizes = decodeUint64Array(sizesPtr, count);
  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = sizes[i] === 0 ? [] : decodeInt64Array(vectorPtrs[i], sizes[i]);
  }
  lib.quiver_database_free_integer_vectors(vectorsPtr, sizesPtr, BigInt(count));
  return result;
}

function readBulkFloats(lib: ReturnType<typeof getSymbols>, handle: Deno.PointerValue, fn: string, collection: string, attribute: string): number[][] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, outVectors.ptr, outSizes.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const vectorsPtr = readPtrOut(outVectors);
  const sizesPtr = readPtrOut(outSizes);
  const vectorPtrs = decodePtrArray(vectorsPtr, count);
  const sizes = decodeUint64Array(sizesPtr, count);
  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = sizes[i] === 0 ? [] : decodeFloat64Array(vectorPtrs[i], sizes[i]);
  }
  lib.quiver_database_free_float_vectors(vectorsPtr, sizesPtr, BigInt(count));
  return result;
}

function readBulkStrings(lib: ReturnType<typeof getSymbols>, handle: Deno.PointerValue, fn: string, collection: string, attribute: string): string[][] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, outVectors.ptr, outSizes.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const vectorsPtr = readPtrOut(outVectors);
  const sizesPtr = readPtrOut(outSizes);
  const vectorPtrs = decodePtrArray(vectorsPtr, count);
  const sizes = decodeUint64Array(sizesPtr, count);
  const result: string[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = sizes[i] === 0 ? [] : decodeStringArray(vectorPtrs[i], sizes[i]);
  }
  lib.quiver_database_free_string_vectors(vectorsPtr, sizesPtr, BigInt(count));
  return result;
}

Database.prototype.readVectorIntegers = function (this: Database, collection: string, attribute: string): number[][] {
  return readBulkIntegers(getSymbols(), this._handle, "quiver_database_read_vector_integers", collection, attribute);
};
Database.prototype.readVectorFloats = function (this: Database, collection: string, attribute: string): number[][] {
  return readBulkFloats(getSymbols(), this._handle, "quiver_database_read_vector_floats", collection, attribute);
};
Database.prototype.readVectorStrings = function (this: Database, collection: string, attribute: string): string[][] {
  return readBulkStrings(getSymbols(), this._handle, "quiver_database_read_vector_strings", collection, attribute);
};
Database.prototype.readSetIntegers = function (this: Database, collection: string, attribute: string): number[][] {
  return readBulkIntegers(getSymbols(), this._handle, "quiver_database_read_set_integers", collection, attribute);
};
Database.prototype.readSetFloats = function (this: Database, collection: string, attribute: string): number[][] {
  return readBulkFloats(getSymbols(), this._handle, "quiver_database_read_set_floats", collection, attribute);
};
Database.prototype.readSetStrings = function (this: Database, collection: string, attribute: string): string[][] {
  return readBulkStrings(getSymbols(), this._handle, "quiver_database_read_set_strings", collection, attribute);
};

// --- By-ID reads ---

function readByIdIntegers(lib: ReturnType<typeof getSymbols>, handle: Deno.PointerValue, fn: string, collection: string, attribute: string, id: number): number[] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, BigInt(id), outValues.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeInt64Array(ptr, count);
  lib.quiver_database_free_integer_array(ptr);
  return result;
}

function readByIdFloats(lib: ReturnType<typeof getSymbols>, handle: Deno.PointerValue, fn: string, collection: string, attribute: string, id: number): number[] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, BigInt(id), outValues.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeFloat64Array(ptr, count);
  lib.quiver_database_free_float_array(ptr);
  return result;
}

function readByIdStrings(lib: ReturnType<typeof getSymbols>, handle: Deno.PointerValue, fn: string, collection: string, attribute: string, id: number): string[] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, BigInt(id), outValues.ptr, outCount.ptr));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeStringArray(ptr, count);
  lib.quiver_database_free_string_array(ptr, BigInt(count));
  return result;
}

Database.prototype.readVectorIntegersById = function (this: Database, collection: string, attribute: string, id: number): number[] {
  return readByIdIntegers(getSymbols(), this._handle, "quiver_database_read_vector_integers_by_id", collection, attribute, id);
};
Database.prototype.readVectorFloatsById = function (this: Database, collection: string, attribute: string, id: number): number[] {
  return readByIdFloats(getSymbols(), this._handle, "quiver_database_read_vector_floats_by_id", collection, attribute, id);
};
Database.prototype.readVectorStringsById = function (this: Database, collection: string, attribute: string, id: number): string[] {
  return readByIdStrings(getSymbols(), this._handle, "quiver_database_read_vector_strings_by_id", collection, attribute, id);
};
Database.prototype.readSetIntegersById = function (this: Database, collection: string, attribute: string, id: number): number[] {
  return readByIdIntegers(getSymbols(), this._handle, "quiver_database_read_set_integers_by_id", collection, attribute, id);
};
Database.prototype.readSetFloatsById = function (this: Database, collection: string, attribute: string, id: number): number[] {
  return readByIdFloats(getSymbols(), this._handle, "quiver_database_read_set_floats_by_id", collection, attribute, id);
};
Database.prototype.readSetStringsById = function (this: Database, collection: string, attribute: string, id: number): string[] {
  return readByIdStrings(getSymbols(), this._handle, "quiver_database_read_set_strings_by_id", collection, attribute, id);
};
