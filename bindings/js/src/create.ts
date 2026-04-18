import { Database } from "./database.ts";
import { check, QuiverError } from "./errors.ts";
import { allocNativeFloat64, allocNativeInt64, allocNativeStringArray, allocPtrOut, readPtrOut, toCString } from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";
import type { ElementData, Value } from "./types.ts";

declare module "./database.ts" {
  interface Database {
    createElement(collection: string, data: ElementData): number;
    updateElement(collection: string, id: number, data: ElementData): void;
    deleteElement(collection: string, id: number): void;
  }
}

type Symbols = ReturnType<typeof getSymbols>;

function setElementArray(lib: Symbols, elemPtr: Deno.PointerValue, name: string, values: unknown[]): void {
  const nameBuf = toCString(name);

  if (values.length === 0) {
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, null, 0));
    return;
  }

  const first = values[0];

  if (typeof first === "bigint") {
    const arr = allocNativeInt64((values as bigint[]).map(Number));
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.ptr, values.length));
    return;
  }

  if (typeof first === "number") {
    const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
    if (allIntegers) {
      const arr = allocNativeInt64(values as number[]);
      check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.ptr, values.length));
    } else {
      const arr = allocNativeFloat64(values as number[]);
      check(lib.quiver_element_set_array_float(elemPtr, nameBuf.buf, arr.ptr, values.length));
    }
    return;
  }

  if (typeof first === "string") {
    const { table, keepalive: _keepalive } = allocNativeStringArray(values as string[]);
    check(lib.quiver_element_set_array_string(elemPtr, nameBuf.buf, table.ptr, values.length));
    return;
  }

  throw new QuiverError(`Unsupported array element type for '${name}': ${typeof first}`);
}

function setElementField(lib: Symbols, elemPtr: Deno.PointerValue, name: string, value: Value): void {
  const nameBuf = toCString(name);

  if (value === null) {
    check(lib.quiver_element_set_null(elemPtr, nameBuf.buf));
    return;
  }

  if (typeof value === "bigint") {
    check(lib.quiver_element_set_integer(elemPtr, nameBuf.buf, value));
    return;
  }

  if (typeof value === "number") {
    if (Number.isInteger(value)) {
      check(lib.quiver_element_set_integer(elemPtr, nameBuf.buf, BigInt(value)));
    } else {
      check(lib.quiver_element_set_float(elemPtr, nameBuf.buf, value));
    }
    return;
  }

  if (typeof value === "string") {
    const valBuf = toCString(value);
    check(lib.quiver_element_set_string(elemPtr, nameBuf.buf, valBuf.buf));
    return;
  }

  if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, name, value);
    return;
  }

  throw new QuiverError(`Unsupported value type for '${name}': ${typeof value}`);
}

Database.prototype.createElement = function (this: Database, collection: string, data: ElementData): number {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem.ptr));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }

    const outIdBuf = new Uint8Array(8);
    const outIdPtr = Deno.UnsafePointer.of(outIdBuf)!;
    const collBuf = toCString(collection);
    check(lib.quiver_database_create_element(handle, collBuf.buf, elemPtr, outIdPtr));
    return Number(new DataView(outIdBuf.buffer).getBigInt64(0, true));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.updateElement = function (this: Database, collection: string, id: number, data: ElementData): void {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem.ptr));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }
    const collBuf = toCString(collection);
    check(lib.quiver_database_update_element(handle, collBuf.buf, BigInt(id), elemPtr));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.deleteElement = function (this: Database, collection: string, id: number): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  check(lib.quiver_database_delete_element(this._handle, collBuf.buf, BigInt(id)));
};
