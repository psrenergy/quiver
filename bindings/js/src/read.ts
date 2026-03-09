import { ptr, read, toArrayBuffer, CString } from "bun:ffi";
import { Database } from "./database";
import { getSymbols } from "./loader";
import { check } from "./errors";
import { toCString, allocPointerOut, readPointerOut } from "./ffi-helpers";

declare module "./database" {
  interface Database {
    readScalarIntegers(collection: string, attribute: string): number[];
    readScalarFloats(collection: string, attribute: string): number[];
    readScalarStrings(collection: string, attribute: string): string[];
    readScalarIntegerById(collection: string, attribute: string, id: number): number | null;
    readScalarFloatById(collection: string, attribute: string, id: number): number | null;
    readScalarStringById(collection: string, attribute: string, id: number): string | null;
    readElementIds(collection: string): number[];
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
    const strPtr = read.ptr(arrPtr, i * 8);
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

Database.prototype.readElementIds = function (
  this: Database,
  collection: string,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outIds = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(
    lib.quiver_database_read_element_ids(
      handle,
      toCString(collection),
      ptr(outIds),
      ptr(outCount),
    ),
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
