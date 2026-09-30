import { integerToBoolean } from "./boolean.ts";
import { Database } from "./database.ts";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativeString,
  allocPtrOut,
  decodeStringFromBuf,
  nativeAddress,
  readPtrOut,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";
import {
  type Allocation,
  DATA_TYPE_FLOAT,
  DATA_TYPE_INTEGER,
  DATA_TYPE_NULL,
  DATA_TYPE_STRING,
  type QueryParam,
} from "./types.ts";

// Marshals query parameters into the C API's parallel type/value arrays. No parameters (omitted or
// empty) marshal to NULL pointers with a count of 0: the C API reads neither array then, and a
// zero-length TypedArray has no pointer in Bun (ptr() of one returns a TypeError).
function marshalParams(parameters?: QueryParam[]): {
  types: Uint8Array | null;
  values: Uint8Array | null;
  count: bigint;
  _keepalive: Allocation[];
} {
  if (!parameters || parameters.length === 0)
    return { types: null, values: null, count: 0n, _keepalive: [] };

  const n = parameters.length;
  const typesBuf = new Uint8Array(n * 4);
  const typesDv = new DataView(typesBuf.buffer);
  const valuesBuf = new Uint8Array(n * 8);
  const valuesDv = new DataView(valuesBuf.buffer);
  const keepalive: Allocation[] = [];

  for (let i = 0; i < n; i++) {
    const p = parameters[i];
    if (p === null) {
      typesDv.setInt32(i * 4, DATA_TYPE_NULL, true);
      valuesDv.setBigInt64(i * 8, 0n, true);
    } else if (typeof p === "boolean") {
      typesDv.setInt32(i * 4, DATA_TYPE_INTEGER, true);
      const native = allocNativeInt64([p ? 1 : 0]);
      keepalive.push(native);
      valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
    } else if (typeof p === "bigint") {
      typesDv.setInt32(i * 4, DATA_TYPE_INTEGER, true);
      const native = allocNativeInt64([p]);
      keepalive.push(native);
      valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
    } else if (typeof p === "number") {
      if (Number.isInteger(p)) {
        typesDv.setInt32(i * 4, DATA_TYPE_INTEGER, true);
        const native = allocNativeInt64([p]);
        keepalive.push(native);
        valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
      } else {
        typesDv.setInt32(i * 4, DATA_TYPE_FLOAT, true);
        const native = allocNativeFloat64([p]);
        keepalive.push(native);
        valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
      }
    } else if (typeof p === "string") {
      typesDv.setInt32(i * 4, DATA_TYPE_STRING, true);
      const native = allocNativeString(p);
      keepalive.push(native);
      valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
    } else {
      throw new QuiverError(`Unsupported query parameter type at index ${i}: ${typeof p}`);
    }
  }

  return { types: typesBuf, values: valuesBuf, count: BigInt(n), _keepalive: keepalive };
}

Database.prototype.queryString = function (
  this: Database,
  sql: string,
  parameters?: QueryParam[],
): string | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = allocPtrOut();
  const outHasValue = new Uint8Array(4);
  const params = marshalParams(parameters);

  check(
    lib.quiver_database_query_string(
      this._handle,
      sqlBuf.buf,
      params.types,
      params.values,
      params.count,
      outValue.buf,
      outHasValue,
    ),
  );

  if (new DataView(outHasValue.buffer).getInt32(0, true) === 0) return null;
  const result = decodeStringFromBuf(outValue);
  lib.quiver_database_free_string(readPtrOut(outValue));
  return result;
};

Database.prototype.queryInteger = function (
  this: Database,
  sql: string,
  parameters?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = new Uint8Array(8);
  const outHasValue = new Uint8Array(4);
  const params = marshalParams(parameters);

  check(
    lib.quiver_database_query_integer(
      this._handle,
      sqlBuf.buf,
      params.types,
      params.values,
      params.count,
      outValue,
      outHasValue,
    ),
  );

  if (new DataView(outHasValue.buffer).getInt32(0, true) === 0) return null;
  return Number(new DataView(outValue.buffer).getBigInt64(0, true));
};

Database.prototype.queryBoolean = function (
  this: Database,
  sql: string,
  parameters?: QueryParam[],
): boolean | null {
  return integerToBoolean(this.queryInteger(sql, parameters));
};

Database.prototype.queryFloat = function (
  this: Database,
  sql: string,
  parameters?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = new Uint8Array(8);
  const outHasValue = new Uint8Array(4);
  const params = marshalParams(parameters);

  check(
    lib.quiver_database_query_float(
      this._handle,
      sqlBuf.buf,
      params.types,
      params.values,
      params.count,
      outValue,
      outHasValue,
    ),
  );

  if (new DataView(outHasValue.buffer).getInt32(0, true) === 0) return null;
  return new DataView(outValue.buffer).getFloat64(0, true);
};
