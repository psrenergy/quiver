import { Database } from "./database.js";
import { check, QuiverError } from "./errors.js";
import { allocNativeFloat64, allocNativeInt64, allocNativeString, allocPtrOut, decodeStringFromBuf, nativeAddress, readPtrOut, toCString } from "./ffi-helpers.js";
import { getSymbols } from "./loader.js";
import type { Allocation, QueryParam } from "./types.js";

declare module "./database.js" {
  interface Database {
    queryString(sql: string, params?: QueryParam[]): string | null;
    queryInteger(sql: string, params?: QueryParam[]): number | null;
    queryFloat(sql: string, params?: QueryParam[]): number | null;
  }
}

const DATA_TYPE_INTEGER = 0;
const DATA_TYPE_FLOAT = 1;
const DATA_TYPE_STRING = 2;
const DATA_TYPE_NULL = 4;

function marshalParams(params: QueryParam[]): { types: Allocation; values: Allocation; _keepalive: Allocation[] } {
  const n = params.length;
  const typesBuf = new Uint8Array(n * 4);
  const typesDv = new DataView(typesBuf.buffer);
  const valuesBuf = new Uint8Array(n * 8);
  const valuesDv = new DataView(valuesBuf.buffer);
  const keepalive: Allocation[] = [];

  for (let i = 0; i < n; i++) {
    const p = params[i];
    if (p === null) {
      typesDv.setInt32(i * 4, DATA_TYPE_NULL, true);
      valuesDv.setBigInt64(i * 8, 0n, true);
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

  const types: Allocation = { ptr: Deno.UnsafePointer.of(typesBuf)!, buf: typesBuf };
  const values: Allocation = { ptr: Deno.UnsafePointer.of(valuesBuf)!, buf: valuesBuf };
  keepalive.push(types, values);
  return { types, values, _keepalive: keepalive };
}

Database.prototype.queryString = function (this: Database, sql: string, params?: QueryParam[]): string | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = allocPtrOut();
  const outHasValue = new Uint8Array(4);
  const outHasValuePtr = Deno.UnsafePointer.of(outHasValue)!;
  const outHasValueDv = new DataView(outHasValue.buffer);

  if (params && params.length > 0) {
    const m = marshalParams(params);
    check(lib.quiver_database_query_string_params(this._handle, sqlBuf.buf, m.types.ptr, m.values.ptr, BigInt(params.length), outValue.ptr, outHasValuePtr));
  } else {
    check(lib.quiver_database_query_string(this._handle, sqlBuf.buf, outValue.ptr, outHasValuePtr));
  }

  if (outHasValueDv.getInt32(0, true) === 0) return null;
  const result = decodeStringFromBuf(outValue);
  lib.quiver_database_free_string(readPtrOut(outValue));
  return result;
};

Database.prototype.queryInteger = function (this: Database, sql: string, params?: QueryParam[]): number | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = new Uint8Array(8);
  const outValuePtr = Deno.UnsafePointer.of(outValue)!;
  const outValueDv = new DataView(outValue.buffer);
  const outHasValue = new Uint8Array(4);
  const outHasValuePtr = Deno.UnsafePointer.of(outHasValue)!;
  const outHasValueDv = new DataView(outHasValue.buffer);

  if (params && params.length > 0) {
    const m = marshalParams(params);
    check(lib.quiver_database_query_integer_params(this._handle, sqlBuf.buf, m.types.ptr, m.values.ptr, BigInt(params.length), outValuePtr, outHasValuePtr));
  } else {
    check(lib.quiver_database_query_integer(this._handle, sqlBuf.buf, outValuePtr, outHasValuePtr));
  }

  if (outHasValueDv.getInt32(0, true) === 0) return null;
  return Number(outValueDv.getBigInt64(0, true));
};

Database.prototype.queryFloat = function (this: Database, sql: string, params?: QueryParam[]): number | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = new Uint8Array(8);
  const outValuePtr = Deno.UnsafePointer.of(outValue)!;
  const outValueDv = new DataView(outValue.buffer);
  const outHasValue = new Uint8Array(4);
  const outHasValuePtr = Deno.UnsafePointer.of(outHasValue)!;
  const outHasValueDv = new DataView(outHasValue.buffer);

  if (params && params.length > 0) {
    const m = marshalParams(params);
    check(lib.quiver_database_query_float_params(this._handle, sqlBuf.buf, m.types.ptr, m.values.ptr, BigInt(params.length), outValuePtr, outHasValuePtr));
  } else {
    check(lib.quiver_database_query_float(this._handle, sqlBuf.buf, outValuePtr, outHasValuePtr));
  }

  if (outHasValueDv.getInt32(0, true) === 0) return null;
  return outValueDv.getFloat64(0, true);
};
