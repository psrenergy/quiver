import { CString, ptr, toArrayBuffer } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { allocPointerOut, readPointerOut, readPtrAt, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

declare module "./database" {
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

Database.prototype.readScalarIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_scalar_integers(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  lib.quiver_database_free_integer_array(valuesPtr);
  return result;
};

Database.prototype.readScalarFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_scalar_floats(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const result = Array.from(new Float64Array(buffer));

  lib.quiver_database_free_float_array(valuesPtr);
  return result;
};

Database.prototype.readScalarStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): string[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_scalar_strings(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outValues);
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = readPtrAt(arrPtr, i * 8);
    result[i] = new CString(strPtr).toString();
  }

  lib.quiver_database_free_string_array(arrPtr, count);
  return result;
};

Database.prototype.readScalarIntegerById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = new BigInt64Array(1);
  const outHasValue = new Int32Array(1);

  check(
    lib.quiver_database_read_scalar_integer_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValue),
      ptr(outHasValue),
    ),
  );

  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
};

Database.prototype.readScalarFloatById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = new Float64Array(1);
  const outHasValue = new Int32Array(1);

  check(
    lib.quiver_database_read_scalar_float_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValue),
      ptr(outHasValue),
    ),
  );

  if (outHasValue[0] === 0) return null;
  return outValue[0];
};

Database.prototype.readScalarStringById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): string | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = allocPointerOut();
  const outHasValue = new Int32Array(1);

  check(
    lib.quiver_database_read_scalar_string_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValue),
      ptr(outHasValue),
    ),
  );

  if (outHasValue[0] === 0) return null;

  const strPtr = readPointerOut(outValue);
  const result = new CString(strPtr).toString();
  lib.quiver_database_free_string(strPtr);
  return result;
};

Database.prototype.readElementIds = function (this: Database, collection: string): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outIds = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_element_ids(handle, toCString(collection), ptr(outIds), ptr(outCount)),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const idsPtr = readPointerOut(outIds);
  const buffer = toArrayBuffer(idsPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  lib.quiver_database_free_integer_array(idsPtr);
  return result;
};

// --- Vector bulk reads ---

Database.prototype.readVectorIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): number[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outVectors = allocPointerOut();
  const outSizes = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_vector_integers(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outVectors),
      ptr(outSizes),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const vectorsPtr = readPointerOut(outVectors);
  const sizesPtr = readPointerOut(outSizes);
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const vecPtr = readPtrAt(vectorsPtr, i * 8);
      const buf = toArrayBuffer(vecPtr, 0, size * 8);
      const bigInts = new BigInt64Array(buf);
      result[i] = new Array(size);
      for (let j = 0; j < size; j++) {
        result[i][j] = Number(bigInts[j]);
      }
    }
  }

  lib.quiver_database_free_integer_vectors(vectorsPtr, sizesPtr, count);
  return result;
};

Database.prototype.readVectorFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): number[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outVectors = allocPointerOut();
  const outSizes = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_vector_floats(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outVectors),
      ptr(outSizes),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const vectorsPtr = readPointerOut(outVectors);
  const sizesPtr = readPointerOut(outSizes);
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const vecPtr = readPtrAt(vectorsPtr, i * 8);
      const buf = toArrayBuffer(vecPtr, 0, size * 8);
      result[i] = Array.from(new Float64Array(buf));
    }
  }

  lib.quiver_database_free_float_vectors(vectorsPtr, sizesPtr, count);
  return result;
};

Database.prototype.readVectorStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): string[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outVectors = allocPointerOut();
  const outSizes = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_vector_strings(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outVectors),
      ptr(outSizes),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const vectorsPtr = readPointerOut(outVectors);
  const sizesPtr = readPointerOut(outSizes);
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: string[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const strArrPtr = readPtrAt(vectorsPtr, i * 8);
      result[i] = new Array(size);
      for (let j = 0; j < size; j++) {
        const strPtr = readPtrAt(strArrPtr, j * 8);
        result[i][j] = new CString(strPtr).toString();
      }
    }
  }

  lib.quiver_database_free_string_vectors(vectorsPtr, sizesPtr, count);
  return result;
};

// --- Set bulk reads ---

Database.prototype.readSetIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): number[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outSets = allocPointerOut();
  const outSizes = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_set_integers(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outSets),
      ptr(outSizes),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const setsPtr = readPointerOut(outSets);
  const sizesPtr = readPointerOut(outSizes);
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const setPtr = readPtrAt(setsPtr, i * 8);
      const buf = toArrayBuffer(setPtr, 0, size * 8);
      const bigInts = new BigInt64Array(buf);
      result[i] = new Array(size);
      for (let j = 0; j < size; j++) {
        result[i][j] = Number(bigInts[j]);
      }
    }
  }

  lib.quiver_database_free_integer_vectors(setsPtr, sizesPtr, count);
  return result;
};

Database.prototype.readSetFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): number[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outSets = allocPointerOut();
  const outSizes = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_set_floats(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outSets),
      ptr(outSizes),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const setsPtr = readPointerOut(outSets);
  const sizesPtr = readPointerOut(outSizes);
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const setPtr = readPtrAt(setsPtr, i * 8);
      const buf = toArrayBuffer(setPtr, 0, size * 8);
      result[i] = Array.from(new Float64Array(buf));
    }
  }

  lib.quiver_database_free_float_vectors(setsPtr, sizesPtr, count);
  return result;
};

Database.prototype.readSetStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): string[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outSets = allocPointerOut();
  const outSizes = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_set_strings(
      handle,
      toCString(collection),
      toCString(attribute),
      ptr(outSets),
      ptr(outSizes),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const setsPtr = readPointerOut(outSets);
  const sizesPtr = readPointerOut(outSizes);
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: string[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const strArrPtr = readPtrAt(setsPtr, i * 8);
      result[i] = new Array(size);
      for (let j = 0; j < size; j++) {
        const strPtr = readPtrAt(strArrPtr, j * 8);
        result[i][j] = new CString(strPtr).toString();
      }
    }
  }

  lib.quiver_database_free_string_vectors(setsPtr, sizesPtr, count);
  return result;
};

// --- Vector by-ID reads ---

Database.prototype.readVectorIntegersById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_vector_integers_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  lib.quiver_database_free_integer_array(valuesPtr);
  return result;
};

Database.prototype.readVectorFloatsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_vector_floats_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const result = Array.from(new Float64Array(buffer));

  lib.quiver_database_free_float_array(valuesPtr);
  return result;
};

Database.prototype.readVectorStringsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): string[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_vector_strings_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outValues);
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = readPtrAt(arrPtr, i * 8);
    result[i] = new CString(strPtr).toString();
  }

  lib.quiver_database_free_string_array(arrPtr, count);
  return result;
};

// --- Set by-ID reads ---

Database.prototype.readSetIntegersById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_set_integers_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  lib.quiver_database_free_integer_array(valuesPtr);
  return result;
};

Database.prototype.readSetFloatsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_set_floats_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const result = Array.from(new Float64Array(buffer));

  lib.quiver_database_free_float_array(valuesPtr);
  return result;
};

Database.prototype.readSetStringsById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): string[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_set_strings_by_id(
      handle,
      toCString(collection),
      toCString(attribute),
      id,
      ptr(outValues),
      ptr(outCount),
    ),
  );

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outValues);
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = readPtrAt(arrPtr, i * 8);
    result[i] = new CString(strPtr).toString();
  }

  lib.quiver_database_free_string_array(arrPtr, count);
  return result;
};
