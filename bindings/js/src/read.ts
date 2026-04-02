import { Database } from "./database.js";
import { check } from "./errors.js";
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
} from "./ffi-helpers.js";
import { getSymbols } from "./loader.js";

declare module "./database.js" {
  interface Database {
    readScalarIntegers(collection: string, attribute: string): number[];
    readScalarFloats(collection: string, attribute: string): number[];
    readScalarStrings(collection: string, attribute: string): string[];
    readScalarIntegerById(collection: string, attribute: string, id: number): number | null;
    readScalarFloatById(collection: string, attribute: string, id: number): number | null;
    readScalarStringById(collection: string, attribute: string, id: number): string | null;
    readElementIds(collection: string): number[];
    readVectorIntegers(collection: string, attribute: string): number[][];
    readVectorFloats(collection: string, attribute: string): number[][];
    readVectorStrings(collection: string, attribute: string): string[][];
    readVectorIntegersById(collection: string, attribute: string, id: number): number[];
    readVectorFloatsById(collection: string, attribute: string, id: number): number[];
    readVectorStringsById(collection: string, attribute: string, id: number): string[];
    readSetIntegers(collection: string, attribute: string): number[][];
    readSetFloats(collection: string, attribute: string): number[][];
    readSetStrings(collection: string, attribute: string): string[][];
    readSetIntegersById(collection: string, attribute: string, id: number): number[];
    readSetFloatsById(collection: string, attribute: string, id: number): number[];
    readSetStringsById(collection: string, attribute: string, id: number): string[];
  }
}

// --- Scalar array reads ---

Database.prototype.readScalarIntegers = function (this: Database, collection: string, attribute: string): number[] {
  const lib = getSymbols();
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_scalar_integers(this._handle, collection, attribute, outValues, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeInt64Array(ptr, count);
  lib.quiver_database_free_integer_array(ptr);
  return result;
};

Database.prototype.readScalarFloats = function (this: Database, collection: string, attribute: string): number[] {
  const lib = getSymbols();
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_scalar_floats(this._handle, collection, attribute, outValues, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeFloat64Array(ptr, count);
  lib.quiver_database_free_float_array(ptr);
  return result;
};

Database.prototype.readScalarStrings = function (this: Database, collection: string, attribute: string): string[] {
  const lib = getSymbols();
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_scalar_strings(this._handle, collection, attribute, outValues, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeStringArray(ptr, count);
  lib.quiver_database_free_string_array(ptr, count);
  return result;
};

// --- Scalar by-ID reads ---

Database.prototype.readScalarIntegerById = function (this: Database, collection: string, attribute: string, id: number): number | null {
  const lib = getSymbols();
  const outValue = new BigInt64Array(1);
  const outHasValue = new Int32Array(1);
  check(lib.quiver_database_read_scalar_integer_by_id(this._handle, collection, attribute, id, outValue, outHasValue));
  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
};

Database.prototype.readScalarFloatById = function (this: Database, collection: string, attribute: string, id: number): number | null {
  const lib = getSymbols();
  const outValue = new Float64Array(1);
  const outHasValue = new Int32Array(1);
  check(lib.quiver_database_read_scalar_float_by_id(this._handle, collection, attribute, id, outValue, outHasValue));
  if (outHasValue[0] === 0) return null;
  return outValue[0];
};

Database.prototype.readScalarStringById = function (this: Database, collection: string, attribute: string, id: number): string | null {
  const lib = getSymbols();
  const outValue = allocPtrOut();
  const outHasValue = new Int32Array(1);
  check(lib.quiver_database_read_scalar_string_by_id(this._handle, collection, attribute, id, outValue, outHasValue));
  if (outHasValue[0] === 0) return null;
  const result = decodeStringFromBuf(outValue);
  lib.quiver_database_free_string(readPtrOut(outValue));
  return result;
};

// --- Read element IDs ---

Database.prototype.readElementIds = function (this: Database, collection: string): number[] {
  const lib = getSymbols();
  const outIds = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib.quiver_database_read_element_ids(this._handle, collection, outIds, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outIds);
  const result = decodeInt64Array(ptr, count);
  lib.quiver_database_free_integer_array(ptr);
  return result;
};

// --- Vector bulk reads ---

function readBulkIntegers(lib: ReturnType<typeof getSymbols>, handle: unknown, fn: string, collection: string, attribute: string): number[][] {
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, attribute, outVectors, outSizes, outCount));
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
  lib.quiver_database_free_integer_vectors(vectorsPtr, sizesPtr, count);
  return result;
}

function readBulkFloats(lib: ReturnType<typeof getSymbols>, handle: unknown, fn: string, collection: string, attribute: string): number[][] {
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, attribute, outVectors, outSizes, outCount));
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
  lib.quiver_database_free_float_vectors(vectorsPtr, sizesPtr, count);
  return result;
}

function readBulkStrings(lib: ReturnType<typeof getSymbols>, handle: unknown, fn: string, collection: string, attribute: string): string[][] {
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, attribute, outVectors, outSizes, outCount));
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
  lib.quiver_database_free_string_vectors(vectorsPtr, sizesPtr, count);
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

function readByIdIntegers(lib: ReturnType<typeof getSymbols>, handle: unknown, fn: string, collection: string, attribute: string, id: number): number[] {
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, attribute, id, outValues, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeInt64Array(ptr, count);
  lib.quiver_database_free_integer_array(ptr);
  return result;
}

function readByIdFloats(lib: ReturnType<typeof getSymbols>, handle: unknown, fn: string, collection: string, attribute: string, id: number): number[] {
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, attribute, id, outValues, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeFloat64Array(ptr, count);
  lib.quiver_database_free_float_array(ptr);
  return result;
}

function readByIdStrings(lib: ReturnType<typeof getSymbols>, handle: unknown, fn: string, collection: string, attribute: string, id: number): string[] {
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(lib[fn](handle, collection, attribute, id, outValues, outCount));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const ptr = readPtrOut(outValues);
  const result = decodeStringArray(ptr, count);
  lib.quiver_database_free_string_array(ptr, count);
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
