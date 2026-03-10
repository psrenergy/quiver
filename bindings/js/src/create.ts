import { type Pointer, ptr } from "bun:ffi";
import { Database } from "./database";
import { check, QuiverError } from "./errors";
import { allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";
import type { ElementData, Value } from "./types";

declare module "./database" {
  interface Database {
    createElement(collection: string, data: ElementData): number;
    updateElement(collection: string, id: number, data: ElementData): void;
    deleteElement(collection: string, id: number): void;
  }
}

type Symbols = ReturnType<typeof getSymbols>;

function setElementArray(lib: Symbols, elemPtr: Pointer, name: string, values: unknown[]): void {
  const cName = toCString(name);

  if (values.length === 0) {
    // Empty array: pass null pointer with count 0
    check(lib.quiver_element_set_array_integer(elemPtr, cName, null, 0));
    return;
  }

  const first = values[0];

  if (typeof first === "bigint") {
    const arr = BigInt64Array.from(values as bigint[], (v) => BigInt(v));
    check(lib.quiver_element_set_array_integer(elemPtr, cName, ptr(arr), values.length));
    return;
  }

  if (typeof first === "number") {
    const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
    if (allIntegers) {
      const arr = BigInt64Array.from(values as number[], (v) => BigInt(v));
      check(lib.quiver_element_set_array_integer(elemPtr, cName, ptr(arr), values.length));
    } else {
      const arr = Float64Array.from(values as number[]);
      check(lib.quiver_element_set_array_float(elemPtr, cName, ptr(arr), values.length));
    }
    return;
  }

  if (typeof first === "string") {
    // Build pointer table: BigInt64Array of pointers to each string buffer
    const buffers = (values as string[]).map((s) => toCString(s));
    const ptrTable = new BigInt64Array(buffers.length);
    for (let i = 0; i < buffers.length; i++) {
      ptrTable[i] = BigInt(ptr(buffers[i]));
    }
    check(lib.quiver_element_set_array_string(elemPtr, cName, ptr(ptrTable), values.length));
    return;
  }

  throw new QuiverError(`Unsupported array element type for '${name}': ${typeof first}`);
}

function setElementField(lib: Symbols, elemPtr: Pointer, name: string, value: Value): void {
  const cName = toCString(name);

  if (value === null) {
    check(lib.quiver_element_set_null(elemPtr, cName));
    return;
  }

  if (typeof value === "bigint") {
    check(lib.quiver_element_set_integer(elemPtr, cName, value));
    return;
  }

  if (typeof value === "number") {
    if (Number.isInteger(value)) {
      check(lib.quiver_element_set_integer(elemPtr, cName, value));
    } else {
      check(lib.quiver_element_set_float(elemPtr, cName, value));
    }
    return;
  }

  if (typeof value === "string") {
    check(lib.quiver_element_set_string(elemPtr, cName, toCString(value)));
    return;
  }

  if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, name, value);
    return;
  }

  throw new QuiverError(`Unsupported value type for '${name}': ${typeof value}`);
}

Database.prototype.createElement = function (
  this: Database,
  collection: string,
  data: ElementData,
): number {
  const lib = getSymbols();
  const handle = this._handle; // triggers ensureOpen

  // Create element handle
  const outElem = allocPointerOut();
  check(lib.quiver_element_create(ptr(outElem)));
  const elemPtr = readPointerOut(outElem);

  try {
    // Set fields from data object
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }

    // Create element in database
    const outId = new BigInt64Array(1);
    check(lib.quiver_database_create_element(handle, toCString(collection), elemPtr, ptr(outId)));

    return Number(outId[0]);
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.updateElement = function (
  this: Database,
  collection: string,
  id: number,
  data: ElementData,
): void {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPointerOut();
  check(lib.quiver_element_create(ptr(outElem)));
  const elemPtr = readPointerOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }

    check(lib.quiver_database_update_element(handle, toCString(collection), id, elemPtr));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.deleteElement = function (this: Database, collection: string, id: number): void {
  const lib = getSymbols();
  check(lib.quiver_database_delete_element(this._handle, toCString(collection), id));
};
