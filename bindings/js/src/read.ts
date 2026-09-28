import { CString, type Pointer, toArrayBuffer } from "bun:ffi";
import { integerToBoolean } from "./boolean.ts";
import { Database } from "./database.ts";
import { check } from "./errors.ts";
import {
  allocPtrOut,
  allocUint64Out,
  decodeFloat64Array,
  decodeInt64Array,
  decodePtrArray,
  decodeStringFromBuf,
  decodeUint64Array,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols, type NativePointer } from "./loader.ts";

// --- Scalar array reads ---

Database.prototype.readScalarIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): (number | null)[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outMask = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    lib.quiver_database_read_scalar_integers(
      this._handle,
      collBuf.buf,
      attrBuf.buf,
      outValues.buf,
      outMask.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outValues);
  const maskPtr = readPtrOut(outMask);
  const vals = decodeInt64Array(arrPtr, count);
  // mask[i] === 0 means SQL NULL; gate to null (never Number(placeholder)).
  const mask = new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, count));
  const result = vals.map((v, i) => (mask[i] ? v : null));
  lib.quiver_database_free_integer_array(arrPtr);
  lib.quiver_database_free_mask(maskPtr);
  return result;
};

Database.prototype.readScalarBooleans = function (
  this: Database,
  collection: string,
  attribute: string,
): (boolean | null)[] {
  return this.readScalarIntegers(collection, attribute).map((value) =>
    integerToBoolean(value, collection, attribute),
  );
};

Database.prototype.readScalarFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): (number | null)[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outMask = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    lib.quiver_database_read_scalar_floats(
      this._handle,
      collBuf.buf,
      attrBuf.buf,
      outValues.buf,
      outMask.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outValues);
  const maskPtr = readPtrOut(outMask);
  const vals = decodeFloat64Array(arrPtr, count);
  const mask = new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, count));
  const result = vals.map((v, i) => (mask[i] ? v : null));
  lib.quiver_database_free_float_array(arrPtr);
  lib.quiver_database_free_mask(maskPtr);
  return result;
};

Database.prototype.readScalarStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): (string | null)[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    lib.quiver_database_read_scalar_strings(
      this._handle,
      collBuf.buf,
      attrBuf.buf,
      outValues.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outValues);
  // A NULL string is a NULL char* entry: decode the (null-guarded) pointer array and map
  // NULL -> null (decodeStringArray would construct "" from a NULL pointer instead).
  const result = decodePtrArray(arrPtr, count).map((p) =>
    p === null ? null : new CString(p).toString(),
  );
  lib.quiver_database_free_string_array(arrPtr, BigInt(count));
  return result;
};

// --- Scalar by-ID reads ---

Database.prototype.readScalarIntegerById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number | null {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValBuf = new Uint8Array(8);
  const outHasBuf = new Uint8Array(4);
  check(
    lib.quiver_database_read_scalar_integer_by_id(
      this._handle,
      collBuf.buf,
      attrBuf.buf,
      BigInt(id),
      outValBuf,
      outHasBuf,
    ),
  );
  if (new DataView(outHasBuf.buffer).getInt32(0, true) === 0) return null;
  return Number(new DataView(outValBuf.buffer).getBigInt64(0, true));
};

Database.prototype.readScalarBooleanById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): boolean | null {
  return integerToBoolean(
    this.readScalarIntegerById(collection, attribute, id),
    collection,
    attribute,
  );
};

Database.prototype.readScalarFloatById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number | null {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValBuf = new Uint8Array(8);
  const outHasBuf = new Uint8Array(4);
  check(
    lib.quiver_database_read_scalar_float_by_id(
      this._handle,
      collBuf.buf,
      attrBuf.buf,
      BigInt(id),
      outValBuf,
      outHasBuf,
    ),
  );
  if (new DataView(outHasBuf.buffer).getInt32(0, true) === 0) return null;
  return new DataView(outValBuf.buffer).getFloat64(0, true);
};

Database.prototype.readScalarStringById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): string | null {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValue = allocPtrOut();
  const outHasBuf = new Uint8Array(4);
  check(
    lib.quiver_database_read_scalar_string_by_id(
      this._handle,
      collBuf.buf,
      attrBuf.buf,
      BigInt(id),
      outValue.buf,
      outHasBuf,
    ),
  );
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
  check(lib.quiver_database_read_element_ids(this._handle, collBuf.buf, outIds.buf, outCount.buf));
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outIds);
  const result = decodeInt64Array(arrPtr, count);
  lib.quiver_database_free_integer_array(arrPtr);
  return result;
};

Database.prototype.numberOfElements = function (this: Database, collection: string): number {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const outBuf = new Uint8Array(8);
  check(lib.quiver_database_number_of_elements(this._handle, collBuf.buf, outBuf));
  return Number(new DataView(outBuf.buffer).getBigInt64(0, true));
};

// --- Vector bulk reads ---

function readBulkIntegers(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  attribute: string,
): (number | null)[][] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outVectors = allocPtrOut();
  const outMasks = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    (lib as Record<string, Function>)[fn](
      handle,
      collBuf.buf,
      attrBuf.buf,
      outVectors.buf,
      outMasks.buf,
      outSizes.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const vectorsPtr = readPtrOut(outVectors);
  const masksPtr = readPtrOut(outMasks);
  const sizesPtr = readPtrOut(outSizes);
  const vectorPtrs = decodePtrArray(vectorsPtr, count);
  const maskPtrs = decodePtrArray(masksPtr, count);
  const sizes = decodeUint64Array(sizesPtr, count);
  const result: (number | null)[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    if (sizes[i] === 0) {
      result[i] = [];
      continue;
    }
    const values = decodeInt64Array(vectorPtrs[i], sizes[i]);
    const mask = new Uint8Array(toArrayBuffer(maskPtrs[i] as Pointer, 0, sizes[i]));
    result[i] = values.map((v, j) => (mask[j] ? v : null));
  }
  lib.quiver_database_free_integer_vectors(vectorsPtr, sizesPtr, BigInt(count));
  lib.quiver_database_free_masks(masksPtr, BigInt(count));
  return result;
}

function readBulkFloats(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  attribute: string,
): (number | null)[][] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outVectors = allocPtrOut();
  const outMasks = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    (lib as Record<string, Function>)[fn](
      handle,
      collBuf.buf,
      attrBuf.buf,
      outVectors.buf,
      outMasks.buf,
      outSizes.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const vectorsPtr = readPtrOut(outVectors);
  const masksPtr = readPtrOut(outMasks);
  const sizesPtr = readPtrOut(outSizes);
  const vectorPtrs = decodePtrArray(vectorsPtr, count);
  const maskPtrs = decodePtrArray(masksPtr, count);
  const sizes = decodeUint64Array(sizesPtr, count);
  const result: (number | null)[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    if (sizes[i] === 0) {
      result[i] = [];
      continue;
    }
    const values = decodeFloat64Array(vectorPtrs[i], sizes[i]);
    const mask = new Uint8Array(toArrayBuffer(maskPtrs[i] as Pointer, 0, sizes[i]));
    result[i] = values.map((v, j) => (mask[j] ? v : null));
  }
  lib.quiver_database_free_float_vectors(vectorsPtr, sizesPtr, BigInt(count));
  lib.quiver_database_free_masks(masksPtr, BigInt(count));
  return result;
}

function readBulkStrings(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  attribute: string,
): (string | null)[][] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outVectors = allocPtrOut();
  const outSizes = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    (lib as Record<string, Function>)[fn](
      handle,
      collBuf.buf,
      attrBuf.buf,
      outVectors.buf,
      outSizes.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const vectorsPtr = readPtrOut(outVectors);
  const sizesPtr = readPtrOut(outSizes);
  const vectorPtrs = decodePtrArray(vectorsPtr, count);
  const sizes = decodeUint64Array(sizesPtr, count);
  const result: (string | null)[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    // A NULL cell is a NULL char* entry, so decode the pointer array and map NULL -> null
    // (decodeStringArray would construct "" from a NULL pointer instead).
    result[i] =
      sizes[i] === 0
        ? []
        : decodePtrArray(vectorPtrs[i], sizes[i]).map((cell) =>
            cell === null ? null : new CString(cell).toString(),
          );
  }
  lib.quiver_database_free_string_vectors(vectorsPtr, sizesPtr, BigInt(count));
  return result;
}

Database.prototype.readVectorIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): (number | null)[][] {
  return readBulkIntegers(
    getSymbols(),
    this._handle,
    "quiver_database_read_vector_integers",
    collection,
    attribute,
  );
};
/** One entry per element; within an entry a SQL NULL cell is `null`. */
Database.prototype.readVectorBooleans = function (
  this: Database,
  collection: string,
  attribute: string,
): (boolean | null)[][] {
  return this.readVectorIntegers(collection, attribute).map((values) =>
    values.map((value) => integerToBoolean(value, collection, attribute)),
  );
};
Database.prototype.readVectorFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): (number | null)[][] {
  return readBulkFloats(
    getSymbols(),
    this._handle,
    "quiver_database_read_vector_floats",
    collection,
    attribute,
  );
};
Database.prototype.readVectorStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): (string | null)[][] {
  return readBulkStrings(
    getSymbols(),
    this._handle,
    "quiver_database_read_vector_strings",
    collection,
    attribute,
  );
};
Database.prototype.readSetIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): (number | null)[][] {
  return readBulkIntegers(
    getSymbols(),
    this._handle,
    "quiver_database_read_set_integers",
    collection,
    attribute,
  );
};
/** Same contract as `readVectorBooleans`: one entry per element, and a NULL cell is `null`. */
Database.prototype.readSetBooleans = function (
  this: Database,
  collection: string,
  attribute: string,
): (boolean | null)[][] {
  return this.readSetIntegers(collection, attribute).map((values) =>
    values.map((value) => integerToBoolean(value, collection, attribute)),
  );
};
Database.prototype.readSetFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): (number | null)[][] {
  return readBulkFloats(
    getSymbols(),
    this._handle,
    "quiver_database_read_set_floats",
    collection,
    attribute,
  );
};
Database.prototype.readSetStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): (string | null)[][] {
  return readBulkStrings(
    getSymbols(),
    this._handle,
    "quiver_database_read_set_strings",
    collection,
    attribute,
  );
};

// --- By-ID reads ---

function readByIdIntegers(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  attribute: string,
  id: number,
): (number | null)[] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outMask = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    (lib as Record<string, Function>)[fn](
      handle,
      collBuf.buf,
      attrBuf.buf,
      BigInt(id),
      outValues.buf,
      outMask.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outValues);
  const maskPtr = readPtrOut(outMask);
  const values = decodeInt64Array(arrPtr, count);
  const mask = new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, count));
  const result = values.map((v, i) => (mask[i] ? v : null));
  lib.quiver_database_free_integer_array(arrPtr);
  lib.quiver_database_free_mask(maskPtr);
  return result;
}

function readByIdFloats(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  attribute: string,
  id: number,
): (number | null)[] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outMask = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    (lib as Record<string, Function>)[fn](
      handle,
      collBuf.buf,
      attrBuf.buf,
      BigInt(id),
      outValues.buf,
      outMask.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outValues);
  const maskPtr = readPtrOut(outMask);
  const values = decodeFloat64Array(arrPtr, count);
  const mask = new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, count));
  const result = values.map((v, i) => (mask[i] ? v : null));
  lib.quiver_database_free_float_array(arrPtr);
  lib.quiver_database_free_mask(maskPtr);
  return result;
}

function readByIdStrings(
  lib: ReturnType<typeof getSymbols>,
  handle: NativePointer,
  fn: string,
  collection: string,
  attribute: string,
  id: number,
): (string | null)[] {
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outValues = allocPtrOut();
  const outCount = allocUint64Out();
  check(
    (lib as Record<string, Function>)[fn](
      handle,
      collBuf.buf,
      attrBuf.buf,
      BigInt(id),
      outValues.buf,
      outCount.buf,
    ),
  );
  const count = readUint64Out(outCount);
  if (count === 0) return [];
  const arrPtr = readPtrOut(outValues);
  // A NULL cell is a NULL char* entry, so decode the pointer array and map NULL -> null
  // (decodeStringArray would construct "" from a NULL pointer instead).
  const result = decodePtrArray(arrPtr, count).map((cell) =>
    cell === null ? null : new CString(cell).toString(),
  );
  lib.quiver_database_free_string_array(arrPtr, BigInt(count));
  return result;
}

Database.prototype.readVectorIntegersById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (number | null)[] {
  return readByIdIntegers(
    getSymbols(),
    this._handle,
    "quiver_database_read_vector_integers_by_id",
    collection,
    attribute,
    id,
  );
};
Database.prototype.readVectorBooleansById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (boolean | null)[] {
  return this.readVectorIntegersById(collection, attribute, id).map((value) =>
    integerToBoolean(value, collection, attribute),
  );
};
Database.prototype.readVectorFloatsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (number | null)[] {
  return readByIdFloats(
    getSymbols(),
    this._handle,
    "quiver_database_read_vector_floats_by_id",
    collection,
    attribute,
    id,
  );
};
Database.prototype.readVectorStringsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (string | null)[] {
  return readByIdStrings(
    getSymbols(),
    this._handle,
    "quiver_database_read_vector_strings_by_id",
    collection,
    attribute,
    id,
  );
};
Database.prototype.readSetIntegersById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (number | null)[] {
  return readByIdIntegers(
    getSymbols(),
    this._handle,
    "quiver_database_read_set_integers_by_id",
    collection,
    attribute,
    id,
  );
};
Database.prototype.readSetBooleansById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (boolean | null)[] {
  return this.readSetIntegersById(collection, attribute, id).map((value) =>
    integerToBoolean(value, collection, attribute),
  );
};
Database.prototype.readSetFloatsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (number | null)[] {
  return readByIdFloats(
    getSymbols(),
    this._handle,
    "quiver_database_read_set_floats_by_id",
    collection,
    attribute,
    id,
  );
};
Database.prototype.readSetStringsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): (string | null)[] {
  return readByIdStrings(
    getSymbols(),
    this._handle,
    "quiver_database_read_set_strings_by_id",
    collection,
    attribute,
    id,
  );
};
