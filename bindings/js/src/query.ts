import koffi from "koffi";
import { Database } from "./database.js";
import { check } from "./errors.js";
import { allocNativeFloat64, allocNativeInt64, allocNativeString, allocPtrOut, decodeStringFromBuf, nativeAddress, readPtrOut } from "./ffi-helpers.js";
import type { NativePointer } from "./loader.js";
import { getSymbols } from "./loader.js";
import type { QueryParam } from "./types.js";

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

function marshalParams(params: QueryParam[]): { types: Int32Array; values: BigInt64Array; _keepalive: NativePointer[] } {
  const n = params.length;
  const types = new Int32Array(n);
  const values = new BigInt64Array(n);
  const keepalive: NativePointer[] = [];

  for (let i = 0; i < n; i++) {
    const p = params[i];
    if (p === null) {
      types[i] = DATA_TYPE_NULL;
      values[i] = 0n;
    } else if (typeof p === "number") {
      if (Number.isInteger(p)) {
        types[i] = DATA_TYPE_INTEGER;
        const native = allocNativeInt64([p]);
        keepalive.push(native);
        values[i] = nativeAddress(native);
      } else {
        types[i] = DATA_TYPE_FLOAT;
        const native = allocNativeFloat64([p]);
        keepalive.push(native);
        values[i] = nativeAddress(native);
      }
    } else if (typeof p === "string") {
      types[i] = DATA_TYPE_STRING;
      const native = allocNativeString(p);
      keepalive.push(native);
      values[i] = nativeAddress(native);
    }
  }

  return { types, values, _keepalive: keepalive };
}

Database.prototype.queryString = function (this: Database, sql: string, params?: QueryParam[]): string | null {
  const lib = getSymbols();
  const outValue = allocPtrOut();
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const m = marshalParams(params);
    check(lib.quiver_database_query_string_params(this._handle, sql, m.types, m.values, params.length, outValue, outHasValue));
  } else {
    check(lib.quiver_database_query_string(this._handle, sql, outValue, outHasValue));
  }

  if (outHasValue[0] === 0) return null;
  const result = decodeStringFromBuf(outValue);
  lib.quiver_database_free_string(readPtrOut(outValue));
  return result;
};

Database.prototype.queryInteger = function (this: Database, sql: string, params?: QueryParam[]): number | null {
  const lib = getSymbols();
  const outValue = new BigInt64Array(1);
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const m = marshalParams(params);
    check(lib.quiver_database_query_integer_params(this._handle, sql, m.types, m.values, params.length, outValue, outHasValue));
  } else {
    check(lib.quiver_database_query_integer(this._handle, sql, outValue, outHasValue));
  }

  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
};

Database.prototype.queryFloat = function (this: Database, sql: string, params?: QueryParam[]): number | null {
  const lib = getSymbols();
  const outValue = new Float64Array(1);
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const m = marshalParams(params);
    check(lib.quiver_database_query_float_params(this._handle, sql, m.types, m.values, params.length, outValue, outHasValue));
  } else {
    check(lib.quiver_database_query_float(this._handle, sql, outValue, outHasValue));
  }

  if (outHasValue[0] === 0) return null;
  return outValue[0];
};
