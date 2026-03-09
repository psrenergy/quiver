import { CString, ptr } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";
import type { QueryParam } from "./types";

declare module "./database" {
  interface Database {
    queryString(sql: string, params?: QueryParam[]): string | null;
    queryInteger(sql: string, params?: QueryParam[]): number | null;
    queryFloat(sql: string, params?: QueryParam[]): number | null;
  }
}

// C API data type enum values (from include/quiver/c/database.h)
const DATA_TYPE_INTEGER = 0;
const DATA_TYPE_FLOAT = 1;
const DATA_TYPE_STRING = 2;
const DATA_TYPE_NULL = 4;

interface MarshaledParams {
  types: Int32Array;
  values: BigInt64Array;
  _keepalive: unknown[];
}

function marshalParams(params: QueryParam[]): MarshaledParams {
  const n = params.length;
  const types = new Int32Array(n);
  const values = new BigInt64Array(n);
  const keepalive: unknown[] = [];

  for (let i = 0; i < n; i++) {
    const p = params[i];
    if (p === null) {
      types[i] = DATA_TYPE_NULL;
      values[i] = 0n;
    } else if (typeof p === "number") {
      if (Number.isInteger(p)) {
        types[i] = DATA_TYPE_INTEGER;
        const buf = new BigInt64Array([BigInt(p)]);
        keepalive.push(buf);
        values[i] = BigInt(ptr(buf));
      } else {
        types[i] = DATA_TYPE_FLOAT;
        const buf = new Float64Array([p]);
        keepalive.push(buf);
        values[i] = BigInt(ptr(buf));
      }
    } else if (typeof p === "string") {
      types[i] = DATA_TYPE_STRING;
      const buf = toCString(p);
      keepalive.push(buf);
      values[i] = BigInt(ptr(buf));
    }
  }

  return { types, values, _keepalive: keepalive };
}

Database.prototype.queryString = function (
  this: Database,
  sql: string,
  params?: QueryParam[],
): string | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = allocPointerOut();
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const marshaled = marshalParams(params);
    check(
      lib.quiver_database_query_string_params(
        handle,
        toCString(sql),
        ptr(marshaled.types),
        ptr(marshaled.values),
        params.length,
        ptr(outValue),
        ptr(outHasValue),
      ),
    );
  } else {
    check(
      lib.quiver_database_query_string(handle, toCString(sql), ptr(outValue), ptr(outHasValue)),
    );
  }

  if (outHasValue[0] === 0) return null;

  const strPtr = readPointerOut(outValue);
  const result = new CString(strPtr).toString();
  lib.quiver_database_free_string(strPtr);
  return result;
};

Database.prototype.queryInteger = function (
  this: Database,
  sql: string,
  params?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = new BigInt64Array(1);
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const marshaled = marshalParams(params);
    check(
      lib.quiver_database_query_integer_params(
        handle,
        toCString(sql),
        ptr(marshaled.types),
        ptr(marshaled.values),
        params.length,
        ptr(outValue),
        ptr(outHasValue),
      ),
    );
  } else {
    check(
      lib.quiver_database_query_integer(handle, toCString(sql), ptr(outValue), ptr(outHasValue)),
    );
  }

  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
};

Database.prototype.queryFloat = function (
  this: Database,
  sql: string,
  params?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = new Float64Array(1);
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const marshaled = marshalParams(params);
    check(
      lib.quiver_database_query_float_params(
        handle,
        toCString(sql),
        ptr(marshaled.types),
        ptr(marshaled.values),
        params.length,
        ptr(outValue),
        ptr(outHasValue),
      ),
    );
  } else {
    check(lib.quiver_database_query_float(handle, toCString(sql), ptr(outValue), ptr(outHasValue)));
  }

  if (outHasValue[0] === 0) return null;
  return outValue[0];
};
